// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Создаёт SceneRoot для модели героя, которую настраивают в BP_Combat_Hero.
 */

#include "Combat/CombatHero.h"
#include "Components/SceneComponent.h"



ACombatHero::ACombatHero()
{
	// Корень получает трансформ из точки арены. Модель в Blueprint сохраняет своё смещение относительно него.
	SetRootComponent( CreateDefaultSubobject <USceneComponent> (TEXT("SceneRoot")) );
}