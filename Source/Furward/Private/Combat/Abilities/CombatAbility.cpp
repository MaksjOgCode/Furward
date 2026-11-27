// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Применяет лечение и запускает периодические автоклики героя.
 */

#include "Combat/Abilities/CombatAbility.h"
#include "Combat/Abilities/CombatAbilityComponent.h"
#include "Combat/Abilities/CombatAbilityDefinition.h"
#include "Combat/Participants/CombatAttributesComponent.h"
#include "Combat/Participants/CombatHero.h"
#include "Combat/Session/CombatSession.h"

bool UCombatMedkitAbility::TryActivate(UCombatSession&, ACombatHero& Hero, const UCombatAbilityDefinition& Definition)
{
	return Hero.GetAttributes().ApplyHealing(Definition.Power) > 0.0f;
}

bool UCombatAutoClickAbility::TryActivate(UCombatSession& Session, ACombatHero& Hero, const UCombatAbilityDefinition& Definition)
{
	return Hero.GetAbilities().StartPeriodicEffect(*this, Definition,
		FSimpleDelegate::CreateUObject(&Session, &UCombatSession::SubmitAutoClick));
}
