// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает старт столкновения, ответные атаки, одновременную гибель, фарм, допуск к способностям и стартовые характеристики и слоты героя.
 */

#pragma once

#include "CoreMinimal.h"
#include "Combat/Participants/CombatBaseAttributes.h"
#include "Engine/DataAsset.h"
#include "CombatRulesDefinition.generated.h"

class UCombatAbilityDefinition;

/** @brief Определяет, когда активируется бой после замены противника. */
UENUM(BlueprintType)
enum class ECombatNextEncounterStartRule : uint8
{
	/** @brief Выбор ещё не сделан. Сессия сообщает об ошибке конфигурации при запуске. */
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

/** @brief Определяет восстановление HP при переходе между фармом и экспедицией. */
UENUM(BlueprintType)
enum class ECombatFarmRecoveryRule : uint8
{
	/** @brief Восстановление при смене режима не настроено. */
	Unspecified,
	/** @brief Герой сразу возвращается к своему максимальному HP. */
	RestoreFullHealth UMETA(DisplayName = "Restore Full Health")
};

/** @brief Определяет, в какой фазе сессия принимает команды способностей. */
UENUM()
enum class ECombatAbilityActivationRule : uint8
{
	/** @brief Способности недоступны, пока правило не задано. */
	Unspecified,
	/** @brief Команда разрешена в активном столкновении. */
	FightingOnly UMETA(DisplayName = "Fighting Only")
};

/** @brief Определяет способ учёта двух смертельных ударов в одном кадре. */
UENUM()
enum class ECombatSimultaneousDeathRule : uint8
{
	/** @brief Смерть обрабатывается при её получении, как у ранее созданных ассетов. */
	Unspecified,
	/** @brief Профиль учитывает победу и награду, а сессия фиксирует поражение героя. */
	DefeatWithVictoryCredit UMETA(DisplayName = "Defeat With Victory Credit")
};

/**
 * @brief Хранит настройки боевых правил для DA_Combat_Rules.
 *
 * Автор контента задаёт правила в Data Asset. Сессия читает их при запуске столкновения, настройке таймера, смене режима и при запросе способности.
 */
UCLASS()
class UCombatRulesDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** @brief Правило выхода из ChangingEnemy, которое задают после согласования GDD. */
	UPROPERTY(EditAnywhere, Category = "Combat|Encounter")
	ECombatNextEncounterStartRule NextEncounterStart = ECombatNextEncounterStartRule::Unspecified;

	/** @brief Политика исхода при смерти двух участников в одном игровом кадре. */
	UPROPERTY(EditAnywhere, Category = "Combat|Encounter")
	ECombatSimultaneousDeathRule SimultaneousDeath = ECombatSimultaneousDeathRule::Unspecified;

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

	/** @brief Восстановление живого героя при возврате из фарма в экспедицию. */
	UPROPERTY(EditAnywhere, Category = "Combat|Farm")
	ECombatFarmRecoveryRule ExpeditionReturnHealth = ECombatFarmRecoveryRule::Unspecified;

	/** @brief Правило допуска героя к способностям в сессии. */
	UPROPERTY(EditAnywhere, Category = "Combat|Abilities")
	ECombatAbilityActivationRule AbilityActivation = ECombatAbilityActivationRule::Unspecified;

	/** @brief Базовые максимум HP, урон и броня героя при входе в бой. */
	UPROPERTY(EditAnywhere, Category = "Combat|Hero")
	FCombatBaseAttributes HeroBaseAttributes;

	/** @brief Текущее HP при первом спауне героя на карте. */
	UPROPERTY(EditAnywhere, Category = "Combat|Hero", meta = (ClampMin = "0.0"))
	float HeroStartingHealth = 0.0f;

	/** @brief Способности в стартовом наборе героя. Индекс 0 соответствует первому слоту. */
	UPROPERTY(EditAnywhere, Category = "Combat|Hero")
	TArray<TObjectPtr<UCombatAbilityDefinition>> HeroAbilitySlots;
};
