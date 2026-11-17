// Copyright © 2026 Maksj.OG


#include "CombatGameMode.h"
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
}

void ACombatGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	UE_LOG( LogCombatGameMode, Display, TEXT("EndPlay: %s"), *GetName() );

	if ( IsValid(CombatSession.Get()) )
		CombatSession->Shutdown();

	CombatSession = nullptr;

	Super::EndPlay(EndPlayReason);
}