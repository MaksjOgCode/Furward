// Copyright © 2026 Maksj.OG

 /**
  * @file     CombatEnvironmentDefinition.h
  *
  * @brief    Назначение данных (Тип для ассетов DA_Combat_Environment, описывающих боевые локации)
  *
  * @details  Хранит мягкую ссылку на карту окружения для отдельного запроса подгрузки.
  *           Во время боя описание используется только для чтения.
  */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CombatEnvironmentDefinition.generated.h"



class UWorld;



/**
 * @class	UCombatEnvironmentDefinition
 *
 * @brief   Описывает окружение, предназначенное для подгрузки в L_Combat
 */
UCLASS()
class UCombatEnvironmentDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	/** Возвращает мягкую ссылку на карту для запроса подгрузки */
	const TSoftObjectPtr <UWorld>& GetEnvironmentMap() const { return EnvironmentMap; }

private:
	/** Карта окружения, выбранная в экземпляре Data Asset */
	UPROPERTY(EditAnywhere, Category = "Combat|Environment")
	TSoftObjectPtr <UWorld> EnvironmentMap;
};