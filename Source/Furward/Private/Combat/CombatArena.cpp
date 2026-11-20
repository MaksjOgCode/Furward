// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Собирает камеру и маркеры арены и отдаёт их сессии и контроллеру.
 */

#include "CombatArena.h"
#include "Camera/CameraComponent.h"
#include "Components/ArrowComponent.h"



ACombatArena::ACombatArena() {
	// Арена хранит расстановку сцены. Ей хватает создания компонентов и чтения их трансформов по запросу.
	PrimaryActorTick.bCanEverTick = false;

	// Общий корень позволяет перемещать ракурс и обе точки одним трансформом арены.
	USceneComponent* SceneRoot = CreateDefaultSubobject <USceneComponent> ( TEXT("SceneRoot") );
	SetRootComponent(SceneRoot);

	// Камера сохраняет ракурс, выставленный для этой арены на карте окружения.
	CameraComponent = CreateDefaultSubobject <UCameraComponent> ( TEXT("CombatCameraComponent") );
	CameraComponent->SetupAttachment(SceneRoot);

	// В редакторе зелёная стрелка показывает спаун и направление героя.
	SpawnPointHero = CreateDefaultSubobject <UArrowComponent> ( TEXT("SpawnPointHero") );
	SpawnPointHero->SetupAttachment(SceneRoot);
	SpawnPointHero->SetArrowFColor(FColor::Green);

	// Красная стрелка показывает спаун и направление противника.
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