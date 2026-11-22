// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Объявляет контракт исполнения эффекта лечения и автокликов.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CombatAbility.generated.h"

class ACombatHero;
class UCombatSession;
class UCombatAbilityDefinition;

/** @brief Экземпляр способности, создаваемый компонентом для одного слота героя. */
UCLASS(Abstract)
class UCombatAbility : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * @brief Применяет эффект после проверки фазы и кулдауна.
	 * @param[in,out] Session Сессия, которая разрешила команду слота.
	 * @param[in,out] Hero Герой, использующий способность.
	 * @param[in] Definition Настройки эффекта из Data Asset.
	 * @return true, если эффект изменил состояние и слот должен запустить кулдаун.
	 */
	virtual bool TryActivate(UCombatSession& Session, ACombatHero& Hero, const UCombatAbilityDefinition& Definition)
		PURE_VIRTUAL(UCombatAbility::TryActivate, return false;);
};

/** @brief Лечит героя в пределах его максимального здоровья. */
UCLASS()
class UCombatMedkitAbility : public UCombatAbility
{
	GENERATED_BODY()

public:
	/** @brief Использует Power как запрошенное лечение и сообщает о фактическом эффекте. */
	bool TryActivate(UCombatSession& Session, ACombatHero& Hero, const UCombatAbilityDefinition& Definition) override;
};

/** @brief Даёт герою периодические атаки по актуальному противнику сессии. */
UCLASS()
class UCombatAutoClickAbility : public UCombatAbility
{
	GENERATED_BODY()

public:
	/** @brief Запускает эффект с длительностью и интервалом из описания способности. */
	bool TryActivate(UCombatSession& Session, ACombatHero& Hero, const UCombatAbilityDefinition& Definition) override;
};
