// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Подключает ввод, камеру и интерфейс, передаёт сессии команды и публикует готовность к выходу из боя.
 */

#include "Combat/CombatPlayerController.h"
#include "Combat/CombatArena.h"
#include "Combat/CombatEnemy.h"
#include "Combat/CombatGameMode.h"
#include "Combat/CombatSession.h"
#include "Combat/CombatRootWidget.h"
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
	UCombatSession* Session = GetActiveCombatSession();
	return Session && Session->RequestNextStage(SaveSlot);
#else
	return false;
#endif
}

bool ACombatPlayerController::RequestExitCombat(FName SaveSlot)
{
#if WITH_EDITOR
	UCombatSession* Session = GetActiveCombatSession();
	return Session && Session->RequestExitCombat(SaveSlot);
#else
	return false;
#endif
}

void ACombatPlayerController::NotifyCombatExitReady()
{
	RemoveCombatWidget();
	UE_LOG(LogCombatPlayerController, Display, TEXT("Combat exit ready: %s"), *GetName());
	OnCombatExitReady.Broadcast();
}

UCombatSession* ACombatPlayerController::GetActiveCombatSession() const
{
	const UWorld* World = GetWorld();
	ACombatGameMode* GameMode = World ? World->GetAuthGameMode<ACombatGameMode>() : nullptr;
	return GameMode ? GameMode->GetCombatSession() : nullptr;
}

void ACombatPlayerController::BeginPlay() {
	Super::BeginPlay();

	// Показываем курсор даже при захвате мыши. GameAndUI даёт интерфейсу приоритет обработки ввода.
	SetShowMouseCursor(true);
	FInputModeGameAndUI InputMode {};
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	AddCombatWidget();

	UE_LOG(LogCombatPlayerController, Log, TEXT("Begin: %s"), *GetName() );
}

void ACombatPlayerController::AddCombatWidget()
{
	UCombatSession* Session = GetActiveCombatSession();
	if (!IsLocalController() || !CombatRootWidgetClass || !Session || CombatWidget)
	{
		return;
	}

	CombatWidget = CreateWidget<UCombatRootWidget>(this, CombatRootWidgetClass);
	if (!CombatWidget || !CombatWidget->ObserveSession(*Session) || !CombatWidget->AddToPlayerScreen())
	{
		RemoveCombatWidget();
		UE_LOG(LogCombatPlayerController, Warning, TEXT("Could not initialize combat HUD."));
		return;
	}

	UE_LOG(LogCombatPlayerController, Display, TEXT("Combat HUD added: %s"), *CombatWidget->GetName());
}

void ACombatPlayerController::RemoveCombatWidget()
{
	if (CombatWidget)
	{
		CombatWidget->StopObservingSession();
		CombatWidget->RemoveFromParent();
		CombatWidget = nullptr;
	}
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
	// Передаём клик сессии текущего мира.
	UCombatSession* Session = GetActiveCombatSession();
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
	RemoveCombatWidget();
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