// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Подключает ввод, камеру, боевой HUD и окно паузы, передаёт сессии команды и публикует готовность к выходу из боя.
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
#include "Kismet/KismetSystemLibrary.h"



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
	UCombatSession* Session = GetActiveCombatSession();
	return Session && Session->RequestNextStage(SaveSlot);
}

bool ACombatPlayerController::RequestEnterFarm()
{
	UCombatSession* Session = GetActiveCombatSession();
	return Session && Session->RequestEnterFarm();
}

bool ACombatPlayerController::RequestReturnToExpedition()
{
	UCombatSession* Session = GetActiveCombatSession();
	return Session && Session->RequestReturnToExpedition();
}

bool ACombatPlayerController::RequestExitCombat(FName SaveSlot)
{
	UCombatSession* Session = GetActiveCombatSession();
	return Session && Session->RequestExitCombat(SaveSlot);
}

bool ACombatPlayerController::RequestUseAbilitySlot(int32 SlotIndex)
{
	UCombatSession* Session = GetActiveCombatSession();
	return Session && Session->TryUseAbilitySlot(SlotIndex);
}

void ACombatPlayerController::HandleAbilitySlot(int32 SlotIndex)
{
	RequestUseAbilitySlot(SlotIndex);
}

bool ACombatPlayerController::SetLeaveConfirmationOpen(bool bOpen)
{
	UCombatSession* Session = GetActiveCombatSession();
	return Session && Session->SetLeaveConfirmationOpen(bOpen);
}

void ACombatPlayerController::NotifyCombatExitReady()
{
	RemovePauseWidget();
	RemoveCombatWidget();
	UE_LOG(LogCombatPlayerController, Display, TEXT("Combat exit ready: %s"), *GetName());
	OnCombatExitReady.Broadcast();
#if UE_BUILD_DEVELOPMENT && !WITH_EDITOR
	// Автономный прототип закрывает игру после подтверждённой записи.
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
#endif
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

void ACombatPlayerController::HandlePauseToggle()
{
	UCombatSession* Session = GetActiveCombatSession();
	if (!Session || !PauseWidgetClass)
	{
		return;
	}

	if (PauseWidget)
	{
		if (Session->SetPauseMenuOpen(false))
		{
			RemovePauseWidget();
		}
		return;
	}

	if (!Session->SetPauseMenuOpen(true))
	{
		return;
	}

	PauseWidget = CreateWidget<UUserWidget>(this, PauseWidgetClass);
	if (!PauseWidget || !PauseWidget->AddToPlayerScreen(20))
	{
		RemovePauseWidget();
		Session->SetPauseMenuOpen(false);
		UE_LOG(LogCombatPlayerController, Warning, TEXT("Could not open the pause widget."));
	}
}

void ACombatPlayerController::RemovePauseWidget()
{
	if (PauseWidget)
	{
		PauseWidget->RemoveFromParent();
		PauseWidget = nullptr;
	}
}

void ACombatPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// После базовой настройки берём подсистему локального игрока и созданный движком компонент ввода.
	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);

	if (!Subsystem || !EnhancedInput)
	{
		UE_LOG(LogCombatPlayerController, Warning, TEXT("Input requires a local Enhanced Input subsystem and component."));
		return;
	}
	LocalInputSubsystem = Subsystem;

	if (CommonMappingContext && PauseAction)
	{
		EnhancedInput->BindAction(PauseAction.Get(), ETriggerEvent::Started, this, &ACombatPlayerController::HandlePauseToggle);
		Subsystem->AddMappingContext(CommonMappingContext.Get(), 1);
		UE_LOG(LogCombatPlayerController, Log, TEXT("Common input context added: %s"), *CommonMappingContext->GetName());
	}

	if (!CombatMappingContext || !CombatClickAction)
	{
		UE_LOG(LogCombatPlayerController, Warning, TEXT("Combat input requires a mapping context and click action."));
		return;
	}

	// Started вызывает обработчик в начале нажатия. Удержание кнопки не повторяет запрос.
	EnhancedInput->BindAction(CombatClickAction.Get(), ETriggerEvent::Started, this,
		&ACombatPlayerController::HandleCombatClick);

	for (int32 SlotIndex = 0; SlotIndex < CombatAbilityActions.Num(); ++SlotIndex)
	{
		if (UInputAction* Action = CombatAbilityActions[SlotIndex].Get(); IsValid(Action))
		{
			EnhancedInput->BindAction(Action, ETriggerEvent::Started, this,
				&ACombatPlayerController::HandleAbilitySlot, SlotIndex);
		}
	}

	// Активируем назначения клавиш для локального игрока.
	Subsystem->AddMappingContext(CombatMappingContext.Get(), 0);
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
	RemovePauseWidget();
	RemoveCombatWidget();
	// Снимаем контексты из подсистемы того же локального игрока.
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalInputSubsystem.Get())
	{
		if (CommonMappingContext && PauseAction)
		{
			Subsystem->RemoveMappingContext(CommonMappingContext.Get());
			UE_LOG(LogCombatPlayerController, Log, TEXT("Common input context removed: %s"), *CommonMappingContext->GetName());
		}
		if (CombatMappingContext && CombatClickAction)
		{
			Subsystem->RemoveMappingContext(CombatMappingContext.Get());
			UE_LOG(LogCombatPlayerController, Log, TEXT("Combat input context removed: %s"), *CombatMappingContext->GetName());
		}
		LocalInputSubsystem.Reset();
	}

	UE_LOG(LogCombatPlayerController, Log, TEXT("End: %s"), *GetName());

	Super::EndPlay(EndPlayReason);
}