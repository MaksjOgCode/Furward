// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает актора противника, его визуальную модель и области Hit и Miss.
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatEnemy.generated.h"

class UBoxComponent;
class UCombatEnemyDefinition;
class UStaticMeshComponent;

/**
 * @brief Представляет экземпляр противника, по которому контроллер проверяет клик.
 *
 * UCombatSession создаёт его в постоянном L_Combat и применяет описание вида до завершения спауна.
 * Модель и обе области принадлежат актору и существуют вместе с ним.
 */
UCLASS()
class ACombatEnemy : public AActor
{
	GENERATED_BODY()

public:
	/** @brief Создаёт SceneRoot, компонент модели и две области клика при создании актора. */
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
