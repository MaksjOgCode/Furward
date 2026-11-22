// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает экземпляр временного эффекта героя и его боевые таймеры.
 */

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Engine/TimerHandle.h"
#include "UObject/Object.h"
#include "CombatEffect.generated.h"

class ACombatHero;
class FTimerManager;
class UCombatAbility;
class UCombatAbilityComponent;
class UCombatAbilityDefinition;

/** @brief Выполняет периодическое действие, пока действует эффект и сессия разрешает боевое время. */
UCLASS()
class UCombatEffect : public UObject
{
	GENERATED_BODY()

private:
	friend class UCombatAbilityComponent;

	/** @brief Запускает таймер срока действия и первое периодическое срабатывание. */
	void Start(FTimerManager& Timers, UCombatAbility& InSource, ACombatHero& InTarget,
		const UCombatAbilityDefinition& Definition, FSimpleDelegate InPeriodicAction);

	/** @brief Проверяет, создавался ли эффект от этого экземпляра способности. */
	bool IsFrom(const UCombatAbility& Ability) const;

	/** @brief Возвращает остаток срока действия из таймера мира, в том числе на паузе. */
	float GetRemainingSeconds() const;

	/** @brief Сохраняет либо продолжает оставшееся время обоих таймеров. */
	void SetPaused(bool bInPaused);

	/** @brief Один раз завершает эффект и снимает оба таймера. */
	void Stop();

	/** @brief Вызывает действие, только пока источник, цель и срок действия актуальны. */
	void RunPeriodicAction();

	/** @brief Возвращает истёкший эффект владельцу для завершения. */
	void Expire();

	/** @brief Получает таймеры мира компонента, который создал эффект. */
	FTimerManager* GetTimers() const;

	/** @brief Экземпляр способности, запустивший эффект. */
	TWeakObjectPtr<UCombatAbility> Source;

	/** @brief Герой, на котором действует временный эффект. */
	TWeakObjectPtr<ACombatHero> Target;

	/** @brief Действие периода. У автокликов слабо связано с сессией через CreateUObject. */
	FSimpleDelegate PeriodicAction;

	/** @brief Завершает эффект при истечении его срока. */
	FTimerHandle ExpirationTimer;

	/** @brief Будит эффект для следующего периодического действия. */
	FTimerHandle PeriodicTimer;

	/** @brief Интервал, сохранённый на время жизни эффекта. */
	float TickIntervalSeconds = 0.0f;

	/** @brief Устанавливается при старте и сбрасывается до очистки таймеров. */
	bool bActive = false;

	/** @brief Учитывает приостановку от фазы боя или окна выхода. */
	bool bPaused = false;
};
