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
		FMath::IsFinite(InBaseAttributes.MaxHealth) && InBaseAttributes.MaxHealth > 0.0f &&
		FMath::IsFinite(InBaseAttributes.DamagePerHit) && InBaseAttributes.DamagePerHit >= 0.0f &&
		FMath::IsFinite(InBaseAttributes.Armor) && InBaseAttributes.Armor >= 0.0f &&
		FMath::IsFinite(InCurrentHealth) && InCurrentHealth > 0.0f;
}

void UCombatAttributesComponent::SetInitialAttributes(const FCombatBaseAttributes& InBaseAttributes, float InCurrentHealth)
{
	// Копируем базовый набор и ограничиваем стартовое здоровье максимумом экземпляра.
	BaseAttributes = InBaseAttributes;
	CurrentHealth = FMath::Min(InCurrentHealth, BaseAttributes.MaxHealth);
}

void UCombatAttributesComponent::PublishInitialHealth()
{
	HealthChanged.Broadcast(0.0f, CurrentHealth);
}

float UCombatAttributesComponent::ApplyDamage(float Damage)
{
	// Только живой участник с готовым здоровьем может потерять HP.
	if (CurrentHealth <= 0.0f || !FMath::IsFinite(Damage) || Damage <= 0.0f)
	{
		return 0.0f;
	}

	// Ограничиваем потерю оставшимся здоровьем и вычисляем фактическое изменение.
	const float PreviousHealth = CurrentHealth;
	CurrentHealth = FMath::Max(0.0f, PreviousHealth - Damage);
	const float AppliedDamage = PreviousHealth - CurrentHealth;
	if (AppliedDamage <= 0.0f)
	{
		return 0.0f;
	}

	// Запоминаем результат этого изменения до вызова подписчиков. Повторный удар по нулю будет отклонён.
	const bool bDied = IsDead();
	HealthChanged.Broadcast(PreviousHealth, CurrentHealth);
	if (bDied)
	{
		Death.Broadcast(*this);
	}
	return AppliedDamage;
}
