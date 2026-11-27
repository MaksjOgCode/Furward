// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает строку таблицы DT_Combat_Stages для подготовки боевого этапа.
 */

#pragma once

#include "Combat/Participants/CombatBaseAttributes.h"
#include "Engine/DataTable.h"
#include "CombatStageRow.generated.h"

class UCombatEnemyDefinition;
class UCombatEnvironmentDefinition;

/**
 * @brief Связывает окружение, состав обычных противников, босса и ожидаемые характеристики этапа.
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
	 * Каждое обычное столкновение выбирает вид из списка с равной вероятностью для каждой записи.
	 */
	UPROPERTY(EditAnywhere, Category = "Combat|Stage")
	TArray<TObjectPtr<UCombatEnemyDefinition>> EnemyRoster;

	/**
	 * @brief Отдельный Data Asset босса, доступного после зачистки обычных противников этапа.
	 *
	 * Сессия использует это описание при достижении 10/10 и при повторном входе с полной зачисткой.
	 */
	UPROPERTY(EditAnywhere, Category = "Combat|Stage")
	TObjectPtr<UCombatEnemyDefinition> Boss;

	/** @brief Имя следующей строки той же таблицы. Пустое значение завершает доступную цепочку. */
	UPROPERTY(EditAnywhere, Category = "Combat|Stage")
	FName NextStage;

	/**
	 * @brief Ожидаемые максимум HP, урон и броня героя для расчёта характеристик противников этого этапа.
	 *
	 * Для расчёта нужен положительный максимум HP. Сессия умножает параметры этапа на множители вида.
	 */
	UPROPERTY(EditAnywhere, Category = "Combat|Balance")
	FCombatBaseAttributes ExpectedHeroAttributes;

	/** @brief Возвращает причину, по которой этап нельзя запустить. Пустая строка означает готовность конфигурации. */
	FString FindConfigurationError() const;
};
