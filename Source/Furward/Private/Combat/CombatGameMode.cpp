// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Связывает начало и завершение L_Combat с жизненным циклом сессии.
 */

#include "CombatGameMode.h"
#include "CombatEnemyDefinition.h"
#include "CombatEnvironmentDefinition.h"
#include "CombatSession.h"
#include "UObject/UObjectGlobals.h"



DEFINE_LOG_CATEGORY_STATIC(LogCombatGameMode, Log, All);

void ACombatGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) {
	Super::InitGame(MapName, Options, ErrorMessage);

	// Создаём сессию с GameMode в качестве Outer и удерживаем её в отражаемом поле.
	CombatSession = NewObject <UCombatSession> (this);
	// Передаём настройки участников до первого запроса подгрузки.
	CombatSession->Initialize(HeroClass, InitialEnemy.Get());
}

UCombatSession* ACombatGameMode::GetCombatSession()
{
	return CombatSession.Get();
}

void ACombatGameMode::BeginPlay() {
	Super::BeginPlay();

	UE_LOG(LogCombatGameMode, Display, TEXT("BeginPlay: %s"), *GetName());

	// GameMode проверяет обе ссылки из Class Defaults. В логе можно отличить пустое описание от неназначенной карты.
	if (!InitialEnvironment) {
		UE_LOG(LogCombatGameMode, Warning, TEXT("Initial Environment is unset: assign it in GameMode Class Defaults"));
	}
	else if (InitialEnvironment->GetEnvironmentMap().IsNull()) {
		UE_LOG(LogCombatGameMode, Warning, TEXT("%s: Environment Map is unset: assign a level asset"), *InitialEnvironment->GetName());
	}
	else {
		// Передаём GameMode как контекст мира. Сессия продолжит подготовку по событию появления подуровня.
		CombatSession->RequestInitialEnvironment(this, InitialEnvironment->GetEnvironmentMap());
	}
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