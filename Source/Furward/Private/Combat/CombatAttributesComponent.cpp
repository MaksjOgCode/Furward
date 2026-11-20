// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Задаёт здоровье экземпляра и публикует изменения после применения урона.
 */

#include "Combat/CombatAttributesComponent.h"

bool UCombatAttributesComponent::InitializeHealth(float InMaxHealth, float InCurrentHealth)
{
	// Проверяем оба значения до записи. Уже инициализированный экземпляр сохраняет состояние боя.
	if (IsHealthInitialized() || !FMath::IsFinite(InMaxHealth) || !FMath::IsFinite(InCurrentHealth) ||
		InMaxHealth <= 0.0f || InCurrentHealth <= 0.0f)
	{
		return false;
	}

	// Подписчики получают начальное здоровье после того, как оба поля готовы для чтения.
	MaxHealth = InMaxHealth;
	CurrentHealth = FMath::Min(InCurrentHealth, MaxHealth);
	HealthChanged.Broadcast(0.0f, CurrentHealth);
	return true;
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
