// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Подключает ввод и камеру и передаёт сессии клики и команду продолжения этапа.
 */

#include "Combat/CombatPlayerController.h"
#include "Combat/CombatArena.h"
#include "Combat/CombatEnemy.h"
#include "Combat/CombatGameMode.h"
#include "Combat/CombatSession.h"
#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "InputMappingContext.h"



DEFINE_LOG_CATEGORY_STATIC(LogCombatPlayerController, Log, All);



void ACombatPlayerController::UseArenaCamera(ACombatArena* Arena) {
    // Переключаем вид только на действительную арену с активной камерой. Ошибку можно увидеть в Output Log.
    if (!IsValid(Arena) || !Arena->HasActiveCameraComponent())
    {
        UE_LOG( LogCombatPlayerController, Error, TEXT("Cannot select arena camera: %s"), *GetNameSafe(Arena) );
        return;
    }

    // Целью вида служит актор арены. Нулевое время перехода переключает ракурс сразу.
    SetViewTargetWithBlend(Arena, 0.0f);

    // Записываем текущую цель камеры, чтобы сверить её с зарегистрированной ареной.
    UE_LOG( LogCombatPlayerController, Display, TEXT("Combat view target: %s"), *GetNameSafe(GetViewTarget()) );
}

bool ACombatPlayerController::RequestNextStage(FName SaveSlot)
{
#if WITH_EDITOR
	const UWorld* World = GetWorld();
	ACombatGameMode* GameMode = World ? World->GetAuthGameMode<ACombatGameMode>() : nullptr;
	UCombatSession* Session = GameMode ? GameMode->GetCombatSession() : nullptr;
	return Session && Session->RequestNextStage(SaveSlot);
#else
	return false;
#endif
}

void ACombatPlayerController::BeginPlay() {
	Super::BeginPlay();

	// Показываем курсор даже при захвате мыши. GameAndUI даёт интерфейсу приоритет обработки ввода.
	SetShowMouseCursor(true);
	FInputModeGameAndUI InputMode {};
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);

	UE_LOG(LogCombatPlayerController, Log, TEXT("Begin: %s"), *GetName() );
}

void ACombatPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// После базовой настройки берём подсистему локального игрока и созданный движком компонент ввода.
	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);

	// Для рабочего маршрута нужны подсистема, компонент Enhanced Input и обе настройки Blueprint.
	if (!Subsystem || !EnhancedInput || !CombatMappingContext || !CombatClickAction)
	{
		UE_LOG(LogCombatPlayerController, Warning,
			TEXT("Combat input requires a local Enhanced Input subsystem, component, mapping context and click action."));
		return;
	}

	// Started вызывает обработчик в начале нажатия. Удержание кнопки не повторяет запрос.
	EnhancedInput->BindAction(CombatClickAction.Get(), ETriggerEvent::Started, this,
		&ACombatPlayerController::HandleCombatClick);
	// Активируем назначения клавиш для локального игрока и запоминаем, откуда снять контекст.
	Subsystem->AddMappingContext(CombatMappingContext.Get(), 0);
	CombatInputSubsystem = Subsystem;
	UE_LOG(LogCombatPlayerController, Log, TEXT("Combat input context added: %s"), *CombatMappingContext->GetName());
}

void ACombatPlayerController::HandleCombatClick()
{
	// Берём сессию из GameMode текущего мира. Без неё клик некуда передать.
	UWorld* World = GetWorld();
	ACombatGameMode* GameMode = World ? World->GetAuthGameMode<ACombatGameMode>() : nullptr;
	UCombatSession* Session = GameMode ? GameMode->GetCombatSession() : nullptr;
	if (!Session)
	{
		return;
	}

	// Проверяем конкретный экземпляр цели. При Ignore оставляем результат в логе без команды сессии.
	const ACombatEnemy* Target = Session->GetActiveEnemy();
	const ECombatClickResult Result = ClassifyCombatClick(Target);
	if (Result == ECombatClickResult::Ignore)
	{
		UE_LOG(LogCombatPlayerController, Display, TEXT("Combat click: Ignore"));
		return;
	}

	// Передаём ту же цель, по которой трассировали. Сессия проверит, что противник всё ещё актуален.
	Session->TrySubmitClickAttack(Result, Target);
}

ECombatClickResult ACombatPlayerController::ClassifyCombatClick(const ACombatEnemy* Target) const
{
	// При действительной цели переводим координаты курсора на экране в луч из камеры.
	FVector RayOrigin;
	FVector RayDirection;
	if (!IsValid(Target) || !DeprojectMousePositionToWorld(RayOrigin, RayDirection))
	{
		return ECombatClickResult::Ignore;
	}

	// Проверяем каждый компонент отдельно. Геометрия окружения и визуальный меш в этот запрос не входят.
	const FVector RayEnd = RayOrigin + RayDirection * HitResultTraceDistance;
	FHitResult Hit;
	const auto TraceArea = [&](UBoxComponent& Area)
	{
		return Area.IsQueryCollisionEnabled() &&
			Area.LineTraceComponent(Hit, RayOrigin, RayEnd, FCollisionQueryParams::DefaultQueryParam);
	};

	// Hit имеет приоритет при пересечении областей. К Miss переходим только после проверки HitArea.
	if (TraceArea(Target->GetHitArea()))
	{
		return ECombatClickResult::Hit;
	}

	return TraceArea(Target->GetMissArea()) ? ECombatClickResult::Miss : ECombatClickResult::Ignore;
}

void ACombatPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	// Снимаем контекст только из той подсистемы, куда его добавили. Запись в логе подтверждает снятие.
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = CombatInputSubsystem.Get())
	{
		Subsystem->RemoveMappingContext(CombatMappingContext.Get());
		CombatInputSubsystem.Reset();
		UE_LOG(LogCombatPlayerController, Log, TEXT("Combat input context removed: %s"), *GetNameSafe(CombatMappingContext.Get()));
	}

	UE_LOG(LogCombatPlayerController, Log, TEXT("End: %s"), *GetName());

	Super::EndPlay(EndPlayReason);
}