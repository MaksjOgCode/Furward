// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает базовые параметры, которые получает компонент характеристик участника.
 */

#pragma once

#include "CoreMinimal.h"
#include "CombatBaseAttributes.generated.h"

/**
 * @brief Группирует максимальное здоровье, урон за попадание и броню до временных модификаторов.
 *
 * Сессия получает готовый набор для каждого участника. Компонент хранит свою копию на время жизни актора.
 */
USTRUCT()
struct FCombatBaseAttributes
{
	GENERATED_BODY()

	/** @brief Максимальное здоровье. Ноль оставляет набор неготовым к инициализации. */
	UPROPERTY(EditAnywhere, Category = "Combat|Attributes")
	float MaxHealth = 0.0f;

	/** @brief Базовый урон за одно попадание до расчёта защиты цели. */
	UPROPERTY(EditAnywhere, Category = "Combat|Attributes")
	float DamagePerHit = 0.0f;

	/** @brief Базовая броня, которую расчёт урона получит от цели. */
	UPROPERTY(EditAnywhere, Category = "Combat|Attributes")
	float Armor = 0.0f;
};
