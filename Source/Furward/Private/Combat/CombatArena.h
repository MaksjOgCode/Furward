// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает камеру и точки появления участников на подгружаемой карте окружения.
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatArena.generated.h"



class UCameraComponent;
class UArrowComponent;



/**
 * @brief Даёт сессии точки появления участников и камеру для текущего окружения.
 *
 * На карте окружения размещают один экземпляр BP_Combat_Arena.
 * Unreal создаёт его при загрузке подуровня и освобождает при выгрузке.
 */
UCLASS()
class ACombatArena : public AActor
{
	GENERATED_BODY()
public:
	/** @brief Создаёт камеру и две стрелки под общим корнем при создании арены. */
	ACombatArena();

	/**
	 * @brief Даёт доступ к параметрам камеры арены.
	 * @return Камера, созданная конструктором и принадлежащая арене.
	 */
	UCameraComponent* GetCameraComponent()     const;

	/**
	 * @brief Даёт сессии точку размещения героя после подгрузки окружения.
	 * @return Положение, поворот и масштаб стрелки героя в мировых координатах.
	 */
	FTransform        GetSpawnHeroTransform()  const;

	/**
	 * @brief Даёт сессии точку размещения противника после подгрузки окружения.
	 * @return Положение, поворот и масштаб стрелки противника в мировых координатах.
	 */
	FTransform        GetSpawnEnemyTransform() const;

private:
	/**
	 * @brief Компонент камеры, который движок использует при выборе арены как цели камеры.
	 *
	 * Создаётся конструктором и существует вместе с ареной.
	 * VisibleAnywhere показывает компонент в Details.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Combat | Arena")
	TObjectPtr <UCameraComponent> CameraComponent {};

	/**
	 * @brief Стрелка героя, созданная конструктором на время жизни арены.
	 *
	 * Её положение задаёт точку появления, направление стрелки задаёт ориентацию.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Combat | Arena")
	TObjectPtr <UArrowComponent> SpawnPointHero {};

	/**
	 * @brief Стрелка противника, созданная конструктором на время жизни арены.
	 *
	 * Редактируется у экземпляра на карте окружения и задаёт местоположение и ориентацию.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Combat | Arena")
	TObjectPtr <UArrowComponent> SpawnPointEnemy {};
};