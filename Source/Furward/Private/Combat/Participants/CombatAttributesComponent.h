// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает базовые характеристики, здоровье экземпляра и события его изменения.
 */

#pragma once

#include "CoreMinimal.h"
#include "Combat/Participants/CombatBaseAttributes.h"
#include "Components/ActorComponent.h"
#include "Delegates/Delegate.h"
#include "CombatAttributesComponent.generated.h"

class UCombatAttributesComponent;
class UCombatSession;

/** @brief Передаёт подписчикам предыдущее и новое здоровье после изменения. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FCombatHealthChanged, float, float);

/** @brief Передаёт подписчикам компонент участника, здоровье которого дошло до нуля. */
DECLARE_MULTICAST_DELEGATE_OneParam(FCombatDeath, const UCombatAttributesComponent&);

/**
 * @brief Хранит базовые параметры и текущее здоровье одного участника боя.
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
	 * @brief Лечит живого участника до предела его максимального HP.
	 * @param[in] Amount Запрошенное лечение.
	 * @return Фактический прирост HP. Возвращает ноль, если здоровье не изменилось.
	 */
	float ApplyHealing(float Amount);

	/**
	 * @brief Позволяет боевой логике проверить, можно ли применять операции со здоровьем.
	 * @return true после установки базового набора и стартового здоровья.
	 */
	bool IsHealthInitialized() const { return BaseAttributes.MaxHealth > 0.0f; }

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
	float GetMaxHealth() const { return BaseAttributes.MaxHealth; }

	/**
	 * @brief Даёт боевой логике базовые значения участника.
	 * @return Копия исходных значений, хранящаяся в компоненте. MaxHealth равен нулю до инициализации.
	 */
	const FCombatBaseAttributes& GetBaseAttributes() const { return BaseAttributes; }

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
	friend class UCombatSession;

	/**
	 * @brief Проверяет набор, прежде чем сессия начнёт инициализацию обоих участников.
	 * @param[in] InBaseAttributes Готовые базовые значения участника.
	 * @param[in] InCurrentHealth Стартовое текущее здоровье.
	 * @return true для незаполненного компонента и допустимого набора значений.
	 */
	bool CanInitializeAttributes(const FCombatBaseAttributes& InBaseAttributes, float InCurrentHealth) const;

	/**
	 * @brief Записывает подготовленные данные перед отправкой событий.
	 * @param[in] InBaseAttributes Готовые базовые значения участника.
	 * @param[in] InCurrentHealth Стартовое текущее здоровье.
	 * @pre CanInitializeAttributes() подтвердил входные данные.
	 */
	void SetInitialAttributes(const FCombatBaseAttributes& InBaseAttributes, float InCurrentHealth);

	/**
	 * @brief Восстанавливает инициализированного участника до максимума и уведомляет об изменении HP.
	 * @pre Сессия приняла команду смены режима. Атаки заблокированы до готовности нового боя.
	 */
	void RestoreFullHealth();

	/** @brief Оповещает подписчиков после первого задания HP. */
	void PublishInitialHealth();

	/**
	 * @brief Уменьшает HP и даёт сессии обработать переход фазы перед уведомлениями.
	 * @param[in] Damage Рассчитанный урон.
	 * @return Фактическая потеря HP или ноль для недопустимого удара.
	 */
	float ReduceHealth(float Damage);

	/**
	 * @brief Сообщает о подтверждённой потере HP и при достижении нуля отправляет Death.
	 * @param[in] PreviousHealth Здоровье перед применением урона.
	 * @pre ReduceHealth() вернул положительное изменение для этого удара.
	 */
	void PublishDamage(float PreviousHealth);

	/**
	 * @brief Копия базовых характеристик, заданная при инициализации экземпляра.
	 *
	 * Нулевой максимум означает ожидание данных. Transient оставляет значения состоянием текущего боя.
	 */
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "Combat|Attributes")
	FCombatBaseAttributes BaseAttributes;

	/**
	 * @brief Остаток здоровья, доступный для просмотра у экземпляра в Details.
	 *
	 * Задаётся вместе с базовыми характеристиками, уменьшается от урона и восстанавливается по команде сессии.
	 * До инициализации ноль означает отсутствие данных о здоровье.
	 */
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "Combat|Health")
	float CurrentHealth = 0.0f;

	/** @brief Хранит подписчиков изменений здоровья на время жизни компонента. */
	FCombatHealthChanged HealthChanged;

	/** @brief Хранит подписчиков смерти на время жизни компонента. */
	FCombatDeath Death;
};
