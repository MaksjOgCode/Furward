// Copyright © 2026 Maksj.OG

#include "Combat/CombatPlayerController.h"
#include "Combat/CombatArena.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"



DEFINE_LOG_CATEGORY_STATIC(LogCombatPlayerController, Log, All);



void ACombatPlayerController::UseArenaCamera(ACombatArena* Arena) {
    // Если арена недействительна:
    if (!IsValid(Arena) || !Arena->HasActiveCameraComponent())
    {
        // Вернём имя арены или "None":
        UE_LOG( LogCombatPlayerController, Error, TEXT("Cannot select arena camera: %s"), *GetNameSafe(Arena) );
        return;
    }

    // Назначаем арену источником ракурса для этого игрока
    // Аргумент BlendTime - длительность перехода в секундах
    SetViewTargetWithBlend(Arena, 0.0f);

    // Выводим имя актора, который сейчас выбран как источник ракурса
    UE_LOG( LogCombatPlayerController, Display, TEXT("Combat view target: %s"), *GetNameSafe(GetViewTarget()) );
}

void ACombatPlayerController::BeginPlay() {
	Super::BeginPlay();

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

	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);

	if (!Subsystem || !EnhancedInput || !CombatMappingContext || !CombatClickAction)
	{
		UE_LOG(LogCombatPlayerController, Warning,
			TEXT("Combat input requires a local Enhanced Input subsystem, component, mapping context and click action."));
		return;
	}

	EnhancedInput->BindAction(CombatClickAction.Get(), ETriggerEvent::Started, this,
		&ACombatPlayerController::HandleCombatClick);
	Subsystem->AddMappingContext(CombatMappingContext.Get(), 0);
	CombatInputSubsystem = Subsystem;
	UE_LOG(LogCombatPlayerController, Log, TEXT("Combat input context added: %s"), *CombatMappingContext->GetName());
}

void ACombatPlayerController::HandleCombatClick()
{
	UE_LOG(LogCombatPlayerController, Display, TEXT("Combat click requested: %s"), *GetName());
}

void ACombatPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = CombatInputSubsystem.Get())
	{
		Subsystem->RemoveMappingContext(CombatMappingContext.Get());
		CombatInputSubsystem.Reset();
		UE_LOG(LogCombatPlayerController, Log, TEXT("Combat input context removed: %s"), *GetNameSafe(CombatMappingContext.Get()));
	}

	UE_LOG(LogCombatPlayerController, Log, TEXT("End: %s"), *GetName());

	Super::EndPlay(EndPlayReason);
}