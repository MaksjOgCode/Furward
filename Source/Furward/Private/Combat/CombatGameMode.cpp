// Copyright © 2026 Maksj.OG

#include "CombatGameMode.h"
#include "CombatEnvironmentDefinition.h"
#include "CombatSession.h"
#include "UObject/UObjectGlobals.h"



DEFINE_LOG_CATEGORY_STATIC(LogCombatGameMode, Log, All);

void ACombatGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) {
	Super::InitGame(MapName, Options, ErrorMessage);

	CombatSession = NewObject <UCombatSession> (this);
	CombatSession->Initialize();
}

void ACombatGameMode::BeginPlay() {
	Super::BeginPlay();

	UE_LOG(LogCombatGameMode, Display, TEXT("BeginPlay: %s"), *GetName());

	if (!InitialEnvironment) {
		UE_LOG(LogCombatGameMode, Warning, TEXT("Initial Environment is unset: assign it in GameMode Class Defaults"));
	}
	else if (InitialEnvironment->GetEnvironmentMap().IsNull()) {
		UE_LOG(LogCombatGameMode, Warning, TEXT("%s: Environment Map is unset: assign a level asset"), *InitialEnvironment->GetName());
	}
	else {
		CombatSession->RequestInitialEnvironment(this, InitialEnvironment->GetEnvironmentMap());
	}
}

void ACombatGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	UE_LOG( LogCombatGameMode, Display, TEXT("EndPlay: %s"), *GetName() );

	if ( IsValid(CombatSession.Get()) )
		CombatSession->Shutdown();

	CombatSession = nullptr;

	Super::EndPlay(EndPlayReason);
}