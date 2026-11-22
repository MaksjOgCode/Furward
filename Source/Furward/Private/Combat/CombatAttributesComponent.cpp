// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Принимает базовые характеристики экземпляра и публикует изменения здоровья.
 */

#include "Combat/CombatAttributesComponent.h"

bool UCombatAttributesComponent::InitializeAttributes(const FCombatBaseAttributes& InBaseAttributes, float InCurrentHealth)
{
	// Уже подготовленный экземпляр сохраняет состояние боя при повторном вызове.
	if (!CanInitializeAttributes(InBaseAttributes, InCurrentHealth))
	{
		return false;
	}
	SetInitialAttributes(InBaseAttributes, InCurrentHealth);
	PublishInitialHealth();
	return true;
}

bool UCombatAttributesComponent::CanInitializeAttributes(const FCombatBaseAttributes& InBaseAttributes, float InCurrentHealth) const
{
	// Игровая логика получает конечные значения. Нулевые урон и броня допустимы.
	return !IsHealthInitialized() &&
		InBaseAttributes.IsValid() &&
		FMath::IsFinite(InCurrentHealth) && InCurrentHealth > 0.0f;
}

void UCombatAttributesComponent::SetInitialAttributes(const FCombatBaseAttributes& InBaseAttributes, float InCurrentHealth)
{
	// Копируем базовый набор и ограничиваем стартовое здоровье максимумом экземпляра.
	BaseAttributes = InBaseAttributes;
	CurrentHealth = FMath::Min(InCurrentHealth, BaseAttributes.MaxHealth);
}

void UCombatAttributesComponent::RestoreFullHealth()
{
	check(IsHealthInitialized());
	const float PreviousHealth = CurrentHealth;
	CurrentHealth = BaseAttributes.MaxHealth;
	if (CurrentHealth > PreviousHealth)
	{
		HealthChanged.Broadcast(PreviousHealth, CurrentHealth);
	}
}

void UCombatAttributesComponent::PublishInitialHealth()
{
	HealthChanged.Broadcast(0.0f, CurrentHealth);
}

float UCombatAttributesComponent::ApplyDamage(float Damage)
{
	const float PreviousHealth = CurrentHealth;
	const float AppliedDamage = ReduceHealth(Damage);
	if (AppliedDamage > 0.0f)
	{
		PublishDamage(PreviousHealth);
	}
	return AppliedDamage;
}

float UCombatAttributesComponent::ApplyHealing(float Amount)
{
	if (CurrentHealth <= 0.0f || !FMath::IsFinite(Amount) || Amount <= 0.0f)
	{
		return 0.0f;
	}

	const float PreviousHealth = CurrentHealth;
	CurrentHealth += FMath::Min(Amount, BaseAttributes.MaxHealth - CurrentHealth);
	const float RestoredHealth = CurrentHealth - PreviousHealth;
	if (RestoredHealth > 0.0f)
	{
		HealthChanged.Broadcast(PreviousHealth, CurrentHealth);
	}
	return RestoredHealth;
}

float UCombatAttributesComponent::ReduceHealth(float Damage)
{
	// Только живой участник с готовым здоровьем может потерять HP.
	if (CurrentHealth <= 0.0f || !FMath::IsFinite(Damage) || Damage <= 0.0f)
	{
		return 0.0f;
	}

	// Ограничиваем потерю оставшимся здоровьем и вычисляем фактическое изменение.
	const float PreviousHealth = CurrentHealth;
	CurrentHealth = FMath::Max(0.0f, PreviousHealth - Damage);
	return PreviousHealth - CurrentHealth;
}

void UCombatAttributesComponent::PublishDamage(float PreviousHealth)
{
	// Запоминаем результат этого изменения до вызова подписчиков. Повторный удар по нулю будет отклонён.
	const bool bDied = IsDead();
	HealthChanged.Broadcast(PreviousHealth, CurrentHealth);
	if (bDied)
	{
		Death.Broadcast(*this);
	}
}
