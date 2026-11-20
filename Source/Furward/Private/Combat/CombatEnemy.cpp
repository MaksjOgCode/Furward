// Copyright © 2026 Maksj.OG

/**
 * @file CombatEnemy.cpp
 * @brief Создаёт компоненты противника и применяет к модели настройки вида.
 */

#include "Combat/CombatEnemy.h"
#include "Combat/CombatEnemyDefinition.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"

ACombatEnemy::ACombatEnemy()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")));

	StaticVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticVisual"));
	StaticVisual->SetupAttachment(GetRootComponent());
	StaticVisual->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
}

void ACombatEnemy::ApplyDefinition(const UCombatEnemyDefinition& Definition)
{
	StaticVisual->SetStaticMesh(Definition.GetStaticMesh());
	StaticVisual->SetRelativeTransform(Definition.GetStaticMeshTransform());
}
