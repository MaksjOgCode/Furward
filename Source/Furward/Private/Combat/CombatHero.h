// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает C++-родителя BP_Combat_Hero, его корень и компонент характеристик.
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatHero.generated.h"

class UCombatAttributesComponent;



/**
 * @brief Представляет героя на постоянной карте боя.
 *
 * UCombatSession создаёт экземпляр BP_Combat_Hero по точке арены после подгрузки окружения.
 * Актором владеет постоянный уровень L_Combat. Blueprint задаёт визуальную модель,
 * а компонент Attributes хранит здоровье этого героя.
 */
UCLASS()
class ACombatHero : public AActor
{
	GENERATED_BODY()
public:
	/**
	 * @brief Создаёт корень для модели и компонент характеристик для экземпляра героя.
	 */
	ACombatHero();

private:
	/**
	 * @brief Компонент характеристик, созданный конструктором на время жизни героя.
	 *
	 * Здоровье появляется после инициализации рассчитанными значениями.
	 * VisibleAnywhere показывает компонент у Blueprint и его экземпляров.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Combat|Attributes")
	TObjectPtr<UCombatAttributesComponent> Attributes;
};