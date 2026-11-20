// Copyright © 2026 Maksj.OG

/**
 * @file     CombatGameMode.h
 * 
 * @brief    Владелец сессии (Игровой режим постоянной карты L_Combat)
 * 
 * @details  Создаёт UCombatSession в InitGame(), удерживает боевую сессию через UPROPERTY
 *           и вызывает Shutdown() перед освобождением ссылки в EndPlay().
 *           Описание стартового окружения выбирается в BP_Combat_GameMode.
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UObject/ObjectPtr.h"
#include "Templates/SubclassOf.h"
#include "CombatGameMode.generated.h"



class UCombatSession;
class UCombatEnvironmentDefinition;
class ACombatHero;
class UCombatEnemyDefinition;



/**
 * @class	ACombatGameMode
 *
 * @brief   Создаёт, инициализирует и завершает UCombatSession. Предоставляет конфигурацию боевой системы и координирует завершение работы карты
 */
UCLASS()
class ACombatGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	/**
	 * Создаёт боевую сессию и передаёт ей класс героя и описание первого противника до BeginPlay().
	 *
	 * @param      MapName      Имя запускаемой карты
	 * @param      Options      Параметры запуска карты из URL
	 * @param[out] ErrorMessage Сообщение, которое движок получит при ошибке инициализации
	 */
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

protected:
	/** Передаёт сессии стартовое окружение после вызова базового BeginPlay() */
	virtual void BeginPlay() override;

	/** Вызывает Shutdown() у действующей сессии и освобождает ссылку до базового EndPlay() */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Описание стартового окружения, выбранное в настройках BP_Combat_GameMode */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Environment")
	TObjectPtr <UCombatEnvironmentDefinition> InitialEnvironment = nullptr;

	/** Удерживает сессию с момента создания в InitGame() до освобождения ссылки в EndPlay() */
	UPROPERTY(Transient)
	TObjectPtr <UCombatSession> CombatSession = nullptr;

	/** Класс героя, выбранный в настройках BP_Combat_GameMode */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Participants")
	TSubclassOf <ACombatHero> HeroClass;

	/** Описание первого противника, выбранное в настройках BP_Combat_GameMode. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Participants")
	TObjectPtr<UCombatEnemyDefinition> InitialEnemy;
};