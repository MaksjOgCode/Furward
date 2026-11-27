// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Создаёт корень модели и боевые компоненты героя.
 */

#include "Combat/Participants/CombatHero.h"
#include "Combat/Abilities/CombatAbilityComponent.h"
#include "Combat/Participants/CombatAttributesComponent.h"
#include "Components/SceneComponent.h"



ACombatHero::ACombatHero()
{
	// Корень получает трансформ из точки арены. Модель в Blueprint сохраняет своё смещение относительно него.
	SetRootComponent( CreateDefaultSubobject <USceneComponent> (TEXT("SceneRoot")) );

	// Каждый экземпляр героя получает собственное состояние здоровья.
	Attributes = CreateDefaultSubobject<UCombatAttributesComponent>(TEXT("Attributes"));
	Abilities = CreateDefaultSubobject<UCombatAbilityComponent>(TEXT("Abilities"));
}

UCombatAttributesComponent& ACombatHero::GetAttributes() const
{
	return *Attributes;
}

UCombatAbilityComponent& ACombatHero::GetAbilities() const
{
	return *Abilities;
}