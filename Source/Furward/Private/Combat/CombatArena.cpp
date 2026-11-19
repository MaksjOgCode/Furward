// Copyright © 2026 Maksj.OG

#include "CombatArena.h"
#include "Camera/CameraComponent.h"
#include "Components/ArrowComponent.h"



ACombatArena::ACombatArena() {
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* SceneRoot = CreateDefaultSubobject <USceneComponent> ( TEXT("SceneRoot") );
	SetRootComponent(SceneRoot);

	CameraComponent = CreateDefaultSubobject <UCameraComponent> ( TEXT("CombatCameraComponent") );
	CameraComponent->SetupAttachment(SceneRoot);

	SpawnPointHero = CreateDefaultSubobject <UArrowComponent> ( TEXT("SpawnPointHero") );
	SpawnPointHero->SetupAttachment(SceneRoot);
	SpawnPointHero->SetArrowFColor(FColor::Green);

	SpawnPointEnemy = CreateDefaultSubobject <UArrowComponent> ( TEXT("SpawnPointEnemy") );
	SpawnPointEnemy->SetupAttachment(SceneRoot);
	SpawnPointEnemy->SetArrowFColor(FColor::Red);
}

UCameraComponent* ACombatArena::GetCameraComponent() const {
	return CameraComponent.Get();
}

FTransform ACombatArena::GetSpawnHeroTransform() const {
	return SpawnPointHero->GetComponentTransform();
}

FTransform ACombatArena::GetSpawnEnemyTransform() const {
	return SpawnPointEnemy->GetComponentTransform();
}