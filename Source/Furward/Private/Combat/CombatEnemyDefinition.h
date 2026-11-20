// Copyright © 2026 Maksj.OG

/**
 * @file CombatEnemyDefinition.h
 * @brief Объявляет тип ассетов DA_Combat_Enemy_<Name> с настройками вида противника.
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Templates/SubclassOf.h"
#include "CombatEnemyDefinition.generated.h"

class ACombatEnemy;
class UStaticMesh;

/** @brief Хранит класс противника и настройки его текущего представления статической моделью. */
UCLASS()
class UCombatEnemyDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Возвращает класс, из которого сессия создаёт противника. */
	TSubclassOf<ACombatEnemy> GetEnemyClass() const { return EnemyClass; }

	/** Возвращает модель, которую противник устанавливает при создании. */
	UStaticMesh* GetStaticMesh() const { return StaticMesh.Get(); }

	/** Возвращает трансформ модели относительно корня противника. */
	const FTransform& GetStaticMeshTransform() const { return StaticMeshTransform; }

private:
	/** Игровой класс этого вида, обычно BP_Combat_Enemy. */
	UPROPERTY(EditAnywhere, Category = "Combat|Enemy")
	TSubclassOf<ACombatEnemy> EnemyClass;

	/** Статическая модель, выбранная в описании вида. */
	UPROPERTY(EditAnywhere, Category = "Combat|Enemy")
	TObjectPtr<UStaticMesh> StaticMesh;

	/** Смещение, ориентация и масштаб модели относительно корня актора. */
	UPROPERTY(EditAnywhere, Category = "Combat|Enemy")
	FTransform StaticMeshTransform = FTransform::Identity;
};
