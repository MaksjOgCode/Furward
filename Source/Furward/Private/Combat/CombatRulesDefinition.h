// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает настраиваемый переход к бою после подготовки следующего противника.
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CombatRulesDefinition.generated.h"

/** @brief Определяет, когда активируется бой после замены противника. */
UENUM()
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
 * Автор контента выбирает правило старта в Data Asset. Сессия читает его при активации
 * подготовленной замены. Экземпляр ассета не хранит состояние боя.
 */
UCLASS()
class UCombatRulesDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** @brief Правило выхода из ChangingEnemy, которое задают после согласования GDD. */
	UPROPERTY(EditAnywhere, Category = "Combat|Encounter")
	ECombatNextEncounterStartRule NextEncounterStart = ECombatNextEncounterStartRule::Unspecified;
};
