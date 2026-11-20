// Copyright © 2026 Maksj.OG

 /**
  * @file     CombatSession.h
  *
  * @brief    Жизненный цикл боя (Боевая сессия одного входа в L_Combat. Сессию создаёт и удерживает ACombatGameMode)
  *
  * @details  Сессия сохраняется при замене окружения. Сессия ведёт фазу боя
  *           и текущее столкновение, хранит ссылки на участников и активный уровень окружения.
  */

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"
#include "CombatSession.generated.h"



class ULevelStreamingDynamic;
class UWorld;
class ACombatArena;
class ACombatHero;
class ACombatEnemy;
class UCombatEnemyDefinition;



/**
 * @class	UCombatSession
 *
 * @brief   Создаётся и удерживается ACombatGameMode. Существует в течение пребывания игрока в L_Combat. При замене окружения сохраняется
 */
UCLASS()
class UCombatSession : public UObject
{
	GENERATED_BODY()
public:
	/**
	 * Сохраняет класс героя и описание первого противника для подготовки сцены.
	 *
	 * @param InHeroClass Класс героя из настроек GameMode.
	 * @param InInitialEnemy Описание первого противника из настроек GameMode.
	 * @pre Сессия создана с ACombatGameMode в качестве Outer
	 */
	void Initialize(TSubclassOf<ACombatHero> InHeroClass, UCombatEnemyDefinition* InInitialEnemy);

	/**
	* Запрашивает подгрузку стартового окружения с исходной расстановкой объектов.
	*
	* @param WorldContextObject это объект, по которому движок определяет игровой мир для подгрузки уровня
	* @param EnvironmentMap     это мягкая ссылка на карту окружения
	*/
	void RequestInitialEnvironment (UObject* WorldContextObject, const TSoftObjectPtr <UWorld>& EnvironmentMap);

	/** Освобождает ссылки на участников и арену, снимает подписку и запрашивает выгрузку окружения. */
	void Shutdown();
private:
	/** Регистрирует единственную арену показанного уровня, передаёт её контроллеру и создаёт участников. */
	UFUNCTION()
	void HandleEnvironmentShown();

	/** Экземпляр подгружаемого уровня, который сессия отслеживает до завершения */
	UPROPERTY(Transient)
	TObjectPtr <ULevelStreamingDynamic> EnvironmentStreaming = nullptr;

	/** Арена активного окружения. Экземпляр принадлежит подгруженному уровню */
	UPROPERTY(Transient)
	TWeakObjectPtr <ACombatArena> ActiveArena;

	/** Создаёт одного героя в постоянном уровне по мировой точке арены. */
	void SpawnHero(UWorld& World, const ACombatArena& Arena);

	/** Создаёт противника из описания вида по точке активной арены в L_Combat. */
	void SpawnEnemy(UWorld& World, const ACombatArena& Arena);

	/** Класс, из которого сессия создаёт героя. */
	UPROPERTY(Transient)
	TSubclassOf<ACombatHero> HeroClass;

	/** Экземпляр героя, принадлежащий постоянному уровню. */
	UPROPERTY(Transient)
	TWeakObjectPtr<ACombatHero> Hero;

	/** Описание первого противника, переданное при запуске карты. */
	UPROPERTY(Transient)
	TObjectPtr<UCombatEnemyDefinition> InitialEnemy;

	/** Текущий противник. Экземпляр принадлежит постоянному уровню. */
	UPROPERTY(Transient)
	TWeakObjectPtr<ACombatEnemy> Enemy;
};