// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает параметры, которые автор контента задаёт в DA_Combat_Ability.
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CombatAbilityDefinition.generated.h"

class UCombatAbility;

/** @brief Хранит общие настройки для всех экземпляров одной способности. */
UCLASS()
class UCombatAbilityDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** @brief Название способности для подписи слота в HUD. */
	UPROPERTY(EditAnywhere, Category = "Combat|Ability")
	FText DisplayName;

	/** @brief Тип, из которого компонент создаёт экземпляр для слота. */
	UPROPERTY(EditAnywhere, Category = "Combat|Ability")
	TSubclassOf<UCombatAbility> AbilityClass;

	/** @brief Время до повторной активации после успешного эффекта, в секундах игры. */
	UPROPERTY(EditAnywhere, Category = "Combat|Ability", meta = (ClampMin = "0.0"))
	float CooldownSeconds = 0.0f;

	/** @brief Время действия периодического эффекта в секундах игры. */
	UPROPERTY(EditAnywhere, Category = "Combat|Effect", meta = (ClampMin = "0.0"))
	float DurationSeconds = 0.0f;

	/** @brief Интервал периодического действия и задержка первого срабатывания. */
	UPROPERTY(EditAnywhere, Category = "Combat|Effect", meta = (ClampMin = "0.0"))
	float TickIntervalSeconds = 0.0f;

	/** @brief Величина эффекта. Аптечка читает её как количество HP. */
	UPROPERTY(EditAnywhere, Category = "Combat|Ability", meta = (ClampMin = "0.0"))
	float Power = 0.0f;
};
