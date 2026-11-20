// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает старт следующего столкновения и интервалы ответных атак противника.
 */

#pragma once

#include "CoreMinimal.h"
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
};
