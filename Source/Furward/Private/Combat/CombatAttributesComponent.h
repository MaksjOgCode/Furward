// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает здоровье участника и события изменения здоровья и смерти.
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Delegates/Delegate.h"
#include "CombatAttributesComponent.generated.h"

class UCombatAttributesComponent;

/** @brief Передаёт подписчикам предыдущее и новое здоровье после изменения. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FCombatHealthChanged, float, float);

/** @brief Передаёт подписчикам компонент участника, здоровье которого дошло до нуля. */
DECLARE_MULTICAST_DELEGATE_OneParam(FCombatDeath, const UCombatAttributesComponent&);

/**
 * @brief Хранит текущее и максимальное здоровье одного участника боя.
 *
 * Конструктор героя или противника создаёт компонент, который существует вместе с актором.
 * Состояние здоровья появляется после явной инициализации рассчитанными значениями.
 */
UCLASS()
class UCombatAttributesComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/**
	 * @brief Задаёт здоровье участника один раз перед применением урона.
	 * @param[in] InMaxHealth Конечное положительное значение максимального здоровья.
	 * @param[in] InCurrentHealth Конечное положительное начальное здоровье.
	 * @return true при успешной инициализации. При неверных данных или повторном вызове возвращает false.
	 * @post При успехе текущее здоровье ограничено максимумом и отправлено событие HealthChanged.
	 */
	bool InitializeHealth(float InMaxHealth, float InCurrentHealth);

	/**
	 * @brief Вычитает рассчитанный урон и публикует результат изменения здоровья.
	 * @param[in] Damage Положительный конечный урон, рассчитанный вызывающей боевой логикой.
	 * @return Сколько здоровья потеряно. Возвращает ноль, если здоровье не изменилось.
	 * @post Здоровье остаётся в допустимом диапазоне. При первом переходе к нулю отправляется Death.
	 */
	float ApplyDamage(float Damage);

	/**
	 * @brief Позволяет боевой логике проверить, можно ли применять операции со здоровьем.
	 * @return true после успешного InitializeHealth().
	 */
	bool IsHealthInitialized() const { return MaxHealth > 0.0f; }

	/**
	 * @brief Определяет, исчерпал ли участник своё здоровье.
	 * @return true, если здоровье успели инициализировать и оно дошло до нуля.
	 */
	bool IsDead() const { return IsHealthInitialized() && CurrentHealth <= 0.0f; }

	/**
	 * @brief Даёт текущее здоровье для отображения или расчётов.
	 * @return Текущее здоровье. До инициализации равно нулю.
	 */
	float GetCurrentHealth() const { return CurrentHealth; }

	/**
	 * @brief Даёт максимум здоровья для отображения или расчётов.
	 * @return Максимальное здоровье. Ноль означает ожидание инициализации.
	 */
	float GetMaxHealth() const { return MaxHealth; }

	/**
	 * @brief Даёт C++-коду точку подписки на начальное значение и дальнейшие изменения.
	 * @return Делегат с предыдущим и новым значениями здоровья.
	 */
	FCombatHealthChanged& OnHealthChanged() { return HealthChanged; }

	/**
	 * @brief Даёт C++-коду точку подписки на первый переход здоровья к нулю.
	 * @return Делегат, который передаёт компонент погибшего участника.
	 */
	FCombatDeath& OnDeath() { return Death; }

private:
	/**
	 * @brief Максимум здоровья, заданный один раз на экземпляре компонента.
	 *
	 * Ноль обозначает ожидание инициализации. Видно у экземпляра в Details.
	 * Transient исключает поле из обычного сохранения объекта.
	 */
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "Combat|Health")
	float MaxHealth = 0.0f;

	/**
	 * @brief Остаток здоровья, доступный для просмотра у экземпляра в Details.
	 *
	 * Задаётся при InitializeHealth() и уменьшается через ApplyDamage().
	 * До инициализации ноль означает отсутствие данных о здоровье.
	 */
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "Combat|Health")
	float CurrentHealth = 0.0f;

	/** @brief Хранит подписчиков изменений здоровья на время жизни компонента. */
	FCombatHealthChanged HealthChanged;

	/** @brief Хранит подписчиков смерти на время жизни компонента. */
	FCombatDeath Death;
};
