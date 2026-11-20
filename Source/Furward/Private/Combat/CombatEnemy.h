// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает противника, его модель, области клика и компонент характеристик.
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatEnemy.generated.h"

class UBoxComponent;
class UCombatAttributesComponent;
class UCombatEnemyDefinition;
class UStaticMeshComponent;

/**
 * @brief Представляет экземпляр противника, по которому контроллер проверяет клик.
 *
 * UCombatSession создаёт его в постоянном L_Combat и применяет описание вида до завершения спауна.
 * Модель, обе области и компонент характеристик принадлежат актору и существуют вместе с ним.
 */
UCLASS()
class ACombatEnemy : public AActor
{
	GENERATED_BODY()

public:
	/** @brief Создаёт SceneRoot, модель, области клика и компонент характеристик противника. */
	ACombatEnemy();

	/**
	 * @brief Устанавливает модель, её трансформ и области клика из описания вида.
	 * @param[in] Definition Описание, выбранное сессией для этого экземпляра.
	 * @pre Сессия вызывает метод на акторе, созданном с отложенным завершением спауна.
	 * @post Геометрия взята из описания. Область активна для трассировки при положительных полуразмерах.
	 */
	void ApplyDefinition(const UCombatEnemyDefinition& Definition);

	/**
	 * @brief Даёт контроллеру область для первой проверки луча от курсора.
	 * @return Компонент HitArea, принадлежащий этому актору.
	 */
	UBoxComponent& GetHitArea() const;

	/**
	 * @brief Даёт контроллеру область для проверки после промаха по HitArea.
	 * @return Компонент MissArea, принадлежащий этому актору.
	 */
	UBoxComponent& GetMissArea() const;

private:
	/**
	 * @brief Компонент характеристик, созданный конструктором на время жизни противника.
	 *
	 * Здоровье противника задаётся после расчёта его характеристик.
	 * VisibleAnywhere показывает компонент у Blueprint и его экземпляров.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Combat|Attributes")
	TObjectPtr<UCombatAttributesComponent> Attributes;

	/**
	 * @brief Статическое представление противника, созданное конструктором.
	 *
	 * ApplyDefinition() назначает модель при спауне. VisibleAnywhere показывает его в Details.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Combat|Enemy")
	TObjectPtr<UStaticMeshComponent> StaticVisual;

	/**
	 * @brief Область попадания, которую контроллер проверяет первой.
	 *
	 * Компонент существует вместе с актором. Конструктор оставляет коллизию выключенной.
	 * ApplyDefinition() задаёт размеры и при наличии размеров включает запросы.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Combat|Click Areas")
	TObjectPtr<UBoxComponent> HitArea;

	/**
	 * @brief Область допустимого промаха, которую проверяют после HitArea.
	 *
	 * Компонент существует вместе с актором. До настройки размеров через ApplyDefinition() коллизия выключена.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Combat|Click Areas")
	TObjectPtr<UBoxComponent> MissArea;
};
