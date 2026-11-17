// Copyright © 2026 Maksj.OG


#include "Combat/CombatPlayerController.h"



DEFINE_LOG_CATEGORY_STATIC(LogCombatPlayerController, Log, All);



void ACombatPlayerController::BeginPlay() {
	Super::BeginPlay();

	SetShowMouseCursor(true);
	FInputModeGameAndUI InputMode {};
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);

	UE_LOG(LogCombatPlayerController, Log, TEXT("Begin: %s"), *GetName() );
}

void ACombatPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	UE_LOG(LogCombatPlayerController, Log, TEXT("End: %s"), *GetName());

	Super::EndPlay(EndPlayReason);
}