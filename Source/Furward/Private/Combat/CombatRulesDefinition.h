// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает старт столкновения, ответные атаки и стартовые характеристики героя для проверки в PIE.
 */

#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatBaseAttributes.h"
#include "Engine/DataAsset.h"
#include "CombatRulesDefinition.generated.h"

/** @brief Определяет, когда активируется бой после замены противника. */
UENUM(BlueprintType)
enum class ECombatNextEncounterStartRule : uint8
{
	/** @brief Выбор ещё не сделан. Сессия продолжает ждать в ChangingEnemy. */
	Unspecified,
	/** @brief Сессия переходит в Ready и ждёт первого положительного урона. */
	OnFirstDamage UMETA(DisplayName = "On First Damage"),
	/** @brief Сессия сразу входит в Fighting при активации подготовленного столкновения. */
	Automatic
};

/**
 * @brief Хранит настройки боевых правил для DA_Combat_Rules.
 *
 * Автор контента задаёт правила в Data Asset. Сессия читает их при активации нового боя и настройке таймера.
 */
UCLASS()
class UCombatRulesDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** @brief Правило выхода из ChangingEnemy, которое задают после согласования GDD. */
	UPROPERTY(EditAnywhere, Category = "Combat|Encounter")
	ECombatNextEncounterStartRule NextEncounterStart = ECombatNextEncounterStartRule::Unspecified;

	/** @brief Задержка первого ответного удара в секундах игрового времени после входа в Fighting. */
	UPROPERTY(EditAnywhere, Category = "Combat|Enemy Attacks", meta = (ClampMin = "0.0"))
	float FirstEnemyAttackDelaySeconds = 0.0f;

	/** @brief Интервал ответных ударов в секундах. Ноль оставляет таймер выключенным. */
	UPROPERTY(EditAnywhere, Category = "Combat|Enemy Attacks", meta = (ClampMin = "0.0"))
	float EnemyAttackIntervalSeconds = 0.0f;

#if WITH_EDITORONLY_DATA
	/** @brief Фактический набор героя для первого входа в пробный бой. */
	UPROPERTY(EditAnywhere, Category = "Combat|PIE")
	FCombatBaseAttributes PIEHeroBaseAttributes;

	/** @brief Текущее HP героя при первом создании в PIE. При смене этапа остаётся здоровье живого экземпляра. */
	UPROPERTY(EditAnywhere, Category = "Combat|PIE", meta = (ClampMin = "0.0"))
	float PIEHeroStartingHealth = 0.0f;
#endif
};
