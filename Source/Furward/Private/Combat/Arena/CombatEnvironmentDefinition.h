// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает Data Asset, по которому GameMode узнаёт, какую карту окружения подгрузить в L_Combat.
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CombatEnvironmentDefinition.generated.h"



class UWorld;



/**
 * @brief Хранит ссылку на карту одного боевого окружения.
 *
 * Автор локации создаёт экземпляр ассета в редакторе и выбирает его в BP_Combat_GameMode.
 * GameMode удерживает выбранное описание и читает его при запуске карты.
 */
UCLASS()
class UCombatEnvironmentDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	/**
	 * @brief Даёт GameMode ссылку для запроса подгрузки окружения.
	 * @return Ссылка на выбранную карту. Пустая ссылка означает, что карту в ассете не назначили.
	 */
	const TSoftObjectPtr <UWorld>& GetEnvironmentMap() const { return EnvironmentMap; }

private:
	/**
	 * @brief Мягкая ссылка на карту, которую настраивают в экземпляре Data Asset.
	 *
	 * Мягкая ссылка хранит путь. Сессия запрашивает загрузку карты отдельно.
	 * Пустое значение приводит к предупреждению в BeginPlay() у GameMode.
	 */
	UPROPERTY(EditAnywhere, Category = "Combat|Environment")
	TSoftObjectPtr <UWorld> EnvironmentMap;
};