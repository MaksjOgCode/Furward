// Copyright © 2026 Maksj.OG

/**
 * @file CombatHero.cpp
 * @brief Создаёт корень, к которому Blueprint крепит модель героя.
 */

#include "Combat/CombatHero.h"
#include "Components/SceneComponent.h"



ACombatHero::ACombatHero()
{
	SetRootComponent( CreateDefaultSubobject <USceneComponent> (TEXT("SceneRoot")) );
}