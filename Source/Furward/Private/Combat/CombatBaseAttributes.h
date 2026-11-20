// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает итоговые базовые параметры и множители, применяемые к ожидаемым значениям этапа.
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

	/** @brief Проверяет набор до расчёта характеристик врага или инициализации участника. */
	bool IsValid() const
	{
		return FMath::IsFinite(MaxHealth) && MaxHealth > 0.0f &&
			FMath::IsFinite(DamagePerHit) && DamagePerHit >= 0.0f &&
			FMath::IsFinite(Armor) && Armor >= 0.0f;
	}
};

/** @brief Задаёт относительные параметры вида противника для любого этапа. */
USTRUCT()
struct FCombatEnemyAttributeMultipliers
{
	GENERATED_BODY()

	/** @brief Умножается на ожидаемый максимум HP героя. */
	UPROPERTY(EditAnywhere, Category = "Combat|Balance", meta = (ClampMin = "0.0"))
	float HealthMultiplier = 1.0f;

	/** @brief Умножается на ожидаемый урон героя за попадание. */
	UPROPERTY(EditAnywhere, Category = "Combat|Balance", meta = (ClampMin = "0.0"))
	float DamageMultiplier = 1.0f;

	/** @brief Умножается на ожидаемую броню героя. */
	UPROPERTY(EditAnywhere, Category = "Combat|Balance", meta = (ClampMin = "0.0"))
	float ArmorMultiplier = 1.0f;

	/**
	 * @brief Вычисляет базовые характеристики врага от значений этапа.
	 * @param[in] ExpectedHero Ожидаемые характеристики героя из строки этапа.
	 * @param[out] Result Рассчитанные характеристики при успехе.
	 * @return true, если входные данные и расчёт пригодны для инициализации участника.
	 */
	bool TryApplyTo(const FCombatBaseAttributes& ExpectedHero, FCombatBaseAttributes& Result) const
	{
		if (!ExpectedHero.IsValid() || HealthMultiplier <= 0.0f || DamageMultiplier < 0.0f || ArmorMultiplier < 0.0f)
		{
			return false;
		}

		const FCombatBaseAttributes Calculated{
			ExpectedHero.MaxHealth * HealthMultiplier,
			ExpectedHero.DamagePerHit * DamageMultiplier,
			ExpectedHero.Armor * ArmorMultiplier};
		if (!Calculated.IsValid())
		{
			return false;
		}
		Result = Calculated;
		return true;
	}
};
