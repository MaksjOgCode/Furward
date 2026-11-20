// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает строку таблицы DT_Combat_Stages для подготовки боевого этапа.
 */

#pragma once

#include "Combat/CombatBaseAttributes.h"
#include "Engine/DataTable.h"
#include "CombatStageRow.generated.h"

class UCombatEnemyDefinition;
class UCombatEnvironmentDefinition;

/**
 * @brief Связывает окружение, состав противников и ожидаемые характеристики этапа.
 *
 * Data Table хранит строки по именам. Сессия читает выбранную строку при входе в L_Combat.
 */
USTRUCT(BlueprintType)
struct FCombatStageRow : public FTableRowBase
{
	GENERATED_BODY()

	/** @brief Описание окружения, из которого сессия берёт мягкую ссылку на карту. */
	UPROPERTY(EditAnywhere, Category = "Combat|Stage")
	TObjectPtr<UCombatEnvironmentDefinition> Environment;

	/**
	 * @brief Описания обычных противников этапа.
	 *
	 * При первом входе сессия использует первый элемент. Пустой список останавливает подготовку сцены.
	 */
	UPROPERTY(EditAnywhere, Category = "Combat|Stage")
	TArray<TObjectPtr<UCombatEnemyDefinition>> EnemyRoster;

	/**
	 * @brief Ожидаемый набор героя для расчёта баланса противников этого этапа.
	 *
	 * Нулевой максимум HP означает, что ожидаемые характеристики ещё не заданы.
	 */
	UPROPERTY(EditAnywhere, Category = "Combat|Balance")
	FCombatBaseAttributes ExpectedHeroAttributes;
};
