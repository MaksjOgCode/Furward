// Copyright © 2026 Maksj.OG

#include "Combat/CombatPlayerController.h"
#include "Combat/CombatArena.h"



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

void ACombatPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	UE_LOG(LogCombatPlayerController, Log, TEXT("End: %s"), *GetName());

	Super::EndPlay(EndPlayReason);
}