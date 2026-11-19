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
#include "UObject/Object.h"
#include "CombatSession.generated.h"



class ULevelStreamingDynamic;
class UWorld;
class ACombatArena;



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
	 * Записывает в лог начало работы сессии и имя её GameMode.
	 *
	 * @pre Сессия создана с ACombatGameMode в качестве Outer.
	 * @see ACombatGameMode::InitGame
	 */
	void Initialize();

	/**
	* Запрашивает подгрузку стартового окружения с исходной расстановкой объектов.
	*
	* @param WorldContextObject это объект, по которому движок определяет игровой мир для подгрузки уровня
	* @param EnvironmentMap     это мягкая ссылка на карту окружения
	*/
	void RequestInitialEnvironment (UObject* WorldContextObject, const TSoftObjectPtr <UWorld>& EnvironmentMap);

	/** Снимает подписку на показ уровня и запрашивает удаление окружения из мира */
	void Shutdown();
private:
	/** Регистрирует единственную арену показанного уровня и передаёт её контроллеру */
	UFUNCTION()
	void HandleEnvironmentShown();

	/** Экземпляр подгружаемого уровня, который сессия отслеживает до завершения */
	UPROPERTY(Transient)
	TObjectPtr <ULevelStreamingDynamic> EnvironmentStreaming = nullptr;

	/** Арена активного окружения. Экземпляр принадлежит подгруженному уровню */
	UPROPERTY(Transient)
	TWeakObjectPtr <ACombatArena> ActiveArena;
};