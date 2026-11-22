// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает слоты, кулдауны и активные эффекты одного героя.
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Delegates/Delegate.h"
#include "Engine/TimerHandle.h"
#include "CombatAbilityComponent.generated.h"

class UCombatAbility;
class UCombatAbilityDefinition;
class UCombatEffect;
class UCombatSession;

/** @brief Связывает настройки слота, его экземпляр эффекта и таймер кулдауна. */
USTRUCT()
struct FCombatAbilitySlot
{
	GENERATED_BODY()

	/** @brief Настройки эффекта из Data Asset. */
	UPROPERTY()
	TObjectPtr<UCombatAbilityDefinition> Definition;

	/** @brief Экземпляр эффекта, которым владеет компонент. */
	UPROPERTY()
	TObjectPtr<UCombatAbility> Ability;

	/** @brief Идентификатор одноразового таймера в мире героя. */
	FTimerHandle Cooldown;
};

/** @brief Владеет способностями и временными эффектами героя, запущенными после разрешения сессии. */
UCLASS()
class UCombatAbilityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** @brief Создаёт экземпляры способностей из настроек, сохраняя индексы пустых слотов. */
	void ConfigureSlots(const TArray<TObjectPtr<UCombatAbilityDefinition>>& Definitions);

	/** @brief Выполняет эффект готового слота. Индекс отсчитывается с нуля. */
	bool TryActivateSlot(int32 SlotIndex, UCombatSession& Session);

	/**
	 * @brief Создаёт один периодический эффект на экземпляр способности.
	 * @return true, если таймеры эффекта запущены. Пока эффект активен, повтор той же способности отклоняется.
	 */
	bool StartPeriodicEffect(UCombatAbility& Source, const UCombatAbilityDefinition& Definition, FSimpleDelegate PeriodicAction);

	/** @brief Возвращает описание настроенного слота. Пустой или недействительный слот даёт nullptr. */
	const UCombatAbilityDefinition* GetSlotDefinition(int32 SlotIndex) const;

	/** @brief Читает остаток кулдауна из таймера, включая приостановленный. Без кулдауна возвращает ноль. */
	float GetSlotCooldownRemaining(int32 SlotIndex) const;

	/** @brief Возвращает остаток активного эффекта способности в слоте. Без эффекта возвращает ноль. */
	float GetSlotEffectRemaining(int32 SlotIndex) const;

	/** @brief Приостанавливает либо продолжает кулдауны и эффекты с оставшегося времени. */
	void SetCombatTimePaused(bool bPaused);

	/** @brief Завершает все активные эффекты с отменой их таймеров. */
	void ClearEffects();

	/** @brief Очищает эффекты, кулдауны и экземпляры способностей. */
	void ClearSlots();

protected:
	/** @brief Освобождает эффекты и слоты при удалении героя или завершении мира. */
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	friend class UCombatEffect;

	/** @brief Завершает истёкший эффект и освобождает удерживающую ссылку. */
	void FinishEffect(UCombatEffect& Effect);

	/** @brief Удерживает эффекты до истечения срока или явного завершения. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCombatEffect>> ActiveEffects;

	/** @brief Слоты текущего героя, включая ссылки, отслеживаемые сборщиком мусора. */
	UPROPERTY(Transient)
	TArray<FCombatAbilitySlot> Slots;

	/** @brief Хранит последнее состояние паузы от сессии. */
	bool bCombatTimePaused = false;

	/** @brief Блокирует повторный запрос во время синхронной рассылки эффекта. */
	bool bActivating = false;
};
