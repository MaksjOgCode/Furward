// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Создаёт корень модели и компонент характеристик героя.
 */

#include "Combat/CombatHero.h"
#include "Combat/CombatAttributesComponent.h"
#include "Components/SceneComponent.h"



ACombatHero::ACombatHero()
{
	// Корень получает трансформ из точки арены. Модель в Blueprint сохраняет своё смещение относительно него.
	SetRootComponent( CreateDefaultSubobject <USceneComponent> (TEXT("SceneRoot")) );

	// Каждый экземпляр героя получает собственное состояние здоровья.
	Attributes = CreateDefaultSubobject<UCombatAttributesComponent>(TEXT("Attributes"));
}

UCombatAttributesComponent& ACombatHero::GetAttributes() const
{
	return *Attributes;
}