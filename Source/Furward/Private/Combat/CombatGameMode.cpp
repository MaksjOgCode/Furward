// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Связывает начало и завершение L_Combat с жизненным циклом сессии.
 */

#include "CombatGameMode.h"
#include "CombatSession.h"
#include "UObject/UObjectGlobals.h"



DEFINE_LOG_CATEGORY_STATIC(LogCombatGameMode, Log, All);

void ACombatGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) {
	Super::InitGame(MapName, Options, ErrorMessage);

	// Создаём сессию с GameMode в качестве Outer и удерживаем её в отражаемом поле.
	CombatSession = NewObject <UCombatSession> (this);
	// Класс героя нужен сессии, когда окружение появится в игровом мире.
	CombatSession->Initialize(HeroClass);
}

UCombatSession* ACombatGameMode::GetCombatSession()
{
	return CombatSession.Get();
}

void ACombatGameMode::BeginPlay() {
	Super::BeginPlay();

	UE_LOG(LogCombatGameMode, Display, TEXT("BeginPlay: %s"), *GetName());

	// Сессия получает строку этапа и использует GameMode как контекст для загрузки окружения.
	CombatSession->RequestInitialStage(this, InitialStage);
}

void ACombatGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	UE_LOG( LogCombatGameMode, Display, TEXT("EndPlay: %s"), *GetName() );

	// Сессия блокирует команды и снимает подписку, пока GameMode ещё удерживает её.
	if ( IsValid(CombatSession.Get()) )
		CombatSession->Shutdown();

	// После Shutdown() отпускаем сессию и передаём движку исходную причину завершения.
	CombatSession = nullptr;

	Super::EndPlay(EndPlayReason);
}