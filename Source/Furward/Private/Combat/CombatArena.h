// Copyright © 2026 Maksj.OG

 /**
  * @file     CombatArena.h
  *
  * @brief    Принадлежность окружению (Камера, точки размещения героя и противника для одной карты окружения)
  *
  * @details  На каждой карте окружения размещается ровно один BP_Combat_Arena.
  *           Арена принадлежит этому уровню. Геттеры точек возвращают мировые преобразования,
  *           по которым сессия сможет разместить участников в постоянном L_Combat.
  */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatArena.generated.h"



class UCameraComponent;
class UArrowComponent;



/**
 * @class	ACombatArena
 * 
 * @brief   Содержит камеру и точки размещения участников. Находится в активном окружении и регистрируется после загрузки окружения
 *
 * @details На каждой карте окружения размещается один BP_Combat_Arena.
 *          Экземпляр арены принадлежит этому уровню. Геттеры возвращают мировые
 *          преобразования точек для размещения участников в постоянном L_Combat.
 */
UCLASS()
class ACombatArena : public AActor
{
	GENERATED_BODY()
public:
	/** Создаёт камеру и две точки появления под общим корневым компонентом */
	ACombatArena();

	/** Возвращает указатель на компонент камеры, которым владеет арена */
	UCameraComponent* GetCameraComponent()     const;

	/** Возвращает положение, поворот и масштаб точки героя в мировых координатах */
	FTransform        GetSpawnHeroTransform()  const;

	/** Возвращает положение, поворот и масштаб точки противника в мировых координатах */
	FTransform        GetSpawnEnemyTransform() const;

private:
	/** Положение и параметры камеры задаются у экземпляра арены на карте окружения */
	UPROPERTY(VisibleAnywhere, Category = "Combat | Arena")
	TObjectPtr <UCameraComponent> CameraComponent {};

	/** Отмечает место появления героя. Стрелка задаёт начальное направление участника при появлении */
	UPROPERTY(VisibleAnywhere, Category = "Combat | Arena")
	TObjectPtr <UArrowComponent> SpawnPointHero {};

	/** Отмечает место появления противника. Стрелка задаёт начальное направление участника при появлении */
	UPROPERTY(VisibleAnywhere, Category = "Combat | Arena")
	TObjectPtr <UArrowComponent> SpawnPointEnemy {};
};