// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает C++-родителя BP_Combat_Hero, его корень, характеристики и способности.
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatHero.generated.h"

class UCombatAbilityComponent;
class UCombatAttributesComponent;



/**
 * @brief Представляет героя на постоянной карте боя.
 *
 * UCombatSession создаёт экземпляр BP_Combat_Hero по точке арены после подгрузки окружения.
 * Актором владеет постоянный уровень L_Combat. Blueprint задаёт визуальную модель,
 * Attributes хранит здоровье, а Abilities хранит слоты и их кулдауны.
 */
UCLASS()
class ACombatHero : public AActor
{
	GENERATED_BODY()
public:
	/**
	 * @brief Создаёт корень для модели и оба боевых компонента экземпляра героя.
	 */
	ACombatHero();

	/**
	 * @brief Даёт сессии компонент для инициализации характеристик героя.
	 * @return Компонент, созданный вместе с этим экземпляром героя.
	 */
	UCombatAttributesComponent& GetAttributes() const;

	/** @brief Возвращает компонент слотов, принадлежащий этому экземпляру героя. */
	UCombatAbilityComponent& GetAbilities() const;

private:
	/** @brief Хранит созданные сессией слоты на время жизни героя. */
	UPROPERTY(VisibleAnywhere, Category = "Combat|Abilities")
	TObjectPtr<UCombatAbilityComponent> Abilities;

	/**
	 * @brief Компонент характеристик, созданный конструктором на время жизни героя.
	 *
	 * Здоровье появляется после инициализации рассчитанными значениями.
	 * VisibleAnywhere показывает компонент у Blueprint и его экземпляров.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Combat|Attributes")
	TObjectPtr<UCombatAttributesComponent> Attributes;
};