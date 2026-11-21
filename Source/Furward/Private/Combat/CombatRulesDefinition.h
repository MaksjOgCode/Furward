// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает старт столкновения, ответные атаки, фарм и стартовые характеристики героя для проверки в PIE.
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

/** @brief Определяет, когда окно поражения разрешает перейти к фарму. */
UENUM(BlueprintType)
enum class ECombatFarmAccessRule : uint8
{
	/** @brief Переход остаётся недоступен до задания правила. */
	Unspecified,
	/** @brief Переход доступен после любого поражения героя. */
	AfterAnyDefeat UMETA(DisplayName = "After Any Defeat")
};

/** @brief Определяет восстановление HP при входе в фарм после поражения. */
UENUM(BlueprintType)
enum class ECombatFarmRecoveryRule : uint8
{
	/** @brief Восстановление для фарма не настроено. */
	Unspecified,
	/** @brief Герой сразу возвращается к своему максимальному HP. */
	RestoreFullHealth UMETA(DisplayName = "Restore Full Health")
};

/**
 * @brief Хранит настройки боевых правил для DA_Combat_Rules.
 *
 * Автор контента задаёт правила в Data Asset. Сессия читает их при запуске столкновения, настройке таймера и входе в фарм.
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

	/** @brief Допуск к фарму после гибели героя. */
	UPROPERTY(EditAnywhere, Category = "Combat|Farm")
	ECombatFarmAccessRule FarmAccess = ECombatFarmAccessRule::Unspecified;

	/** @brief Восстановление героя при принятом переходе к фарму. */
	UPROPERTY(EditAnywhere, Category = "Combat|Farm")
	ECombatFarmRecoveryRule FarmRecovery = ECombatFarmRecoveryRule::Unspecified;

#if WITH_EDITORONLY_DATA
	/** @brief Фактический набор героя для первого входа в пробный бой. */
	UPROPERTY(EditAnywhere, Category = "Combat|PIE")
	FCombatBaseAttributes PIEHeroBaseAttributes;

	/** @brief Текущее HP героя при первом создании в PIE. При смене этапа остаётся здоровье живого экземпляра. */
	UPROPERTY(EditAnywhere, Category = "Combat|PIE", meta = (ClampMin = "0.0"))
	float PIEHeroStartingHealth = 0.0f;
#endif
};
