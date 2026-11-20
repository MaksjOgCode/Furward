// Copyright © 2026 Maksj.OG

/**
 * @file CombatEnemy.h
 * @brief Объявляет игрового актора противника, которого сессия создаёт в L_Combat.
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatEnemy.generated.h"

class UCombatEnemyDefinition;
class UStaticMeshComponent;

/** @brief Представляет текущего противника и владеет компонентом его модели. */
UCLASS()
class ACombatEnemy : public AActor
{
	GENERATED_BODY()

public:
	/** Создаёт корень актора и прикрепляет к нему компонент статической модели. */
	ACombatEnemy();

	/**
	 * Настраивает модель экземпляра по описанию вида.
	 * @param Definition Описание врага, выбранное сессией.
	 * @pre Сессия вызывает метод до FinishSpawning().
	 */
	void ApplyDefinition(const UCombatEnemyDefinition& Definition);

private:
	/** Визуальная модель. Области попадания будут отдельными компонентами. */
	UPROPERTY(VisibleAnywhere, Category = "Combat|Enemy")
	TObjectPtr<UStaticMeshComponent> StaticVisual;
};
