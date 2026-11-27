// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Создаёт способности, управляет их кулдаунами и удерживает активные эффекты.
 */

#include "Combat/Abilities/CombatAbilityComponent.h"
#include "Combat/Abilities/CombatAbility.h"
#include "Combat/Abilities/CombatAbilityDefinition.h"
#include "Combat/Abilities/CombatEffect.h"
#include "Combat/Participants/CombatHero.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCombatAbility, Log, All);

void UCombatAbilityComponent::ConfigureSlots(const TArray<TObjectPtr<UCombatAbilityDefinition>>& Definitions)
{
	ClearSlots();
	Slots.SetNum(Definitions.Num());
	for (int32 SlotIndex = 0; SlotIndex < Definitions.Num(); ++SlotIndex)
	{
		UCombatAbilityDefinition* Definition = Definitions[SlotIndex].Get();
		UClass* AbilityClass = IsValid(Definition) ? Definition->AbilityClass.Get() : nullptr;
		if (!AbilityClass || AbilityClass->HasAnyClassFlags(CLASS_Abstract) ||
			!FMath::IsFinite(Definition->CooldownSeconds) || Definition->CooldownSeconds < 0.0f)
		{
			continue;
		}

		FCombatAbilitySlot& Slot = Slots[SlotIndex];
		Slot.Definition = Definition;
		Slot.Ability = NewObject<UCombatAbility>(this, AbilityClass);
	}
}

bool UCombatAbilityComponent::TryActivateSlot(int32 SlotIndex, UCombatSession& Session)
{
	ACombatHero* Hero = Cast<ACombatHero>(GetOwner());
	UWorld* World = GetWorld();
	if (bActivating || bCombatTimePaused || !IsValid(Hero) || !World || !Slots.IsValidIndex(SlotIndex))
	{
		return false;
	}

	FCombatAbilitySlot& Slot = Slots[SlotIndex];
	UCombatAbility* Ability = Slot.Ability.Get();
	const UCombatAbilityDefinition* Definition = Slot.Definition.Get();
	if (!IsValid(Ability) || !IsValid(Definition))
	{
		return false;
	}

	FTimerManager& Timers = World->GetTimerManager();
	if (Timers.TimerExists(Slot.Cooldown))
	{
		UE_LOG(LogCombatAbility, Display, TEXT("Ability rejected: slot %d is on cooldown."), SlotIndex + 1);
		return false;
	}

	TGuardValue<bool> Activating(bActivating, true);
	if (!Ability->TryActivate(Session, *Hero, *Definition))
	{
		UE_LOG(LogCombatAbility, Display, TEXT("Ability rejected: slot %d had no effect."), SlotIndex + 1);
		return false;
	}

	// Подписчики изменения HP вызываются синхронно и могут завершить сессию.
	if (Slots.IsValidIndex(SlotIndex) && Slots[SlotIndex].Ability.Get() == Ability)
	{
		FTimerHandle& Cooldown = Slots[SlotIndex].Cooldown;
		Timers.SetTimer(Cooldown, Definition->CooldownSeconds, false);
		if (bCombatTimePaused)
		{
			Timers.PauseTimer(Cooldown);
		}
	}

	UE_LOG(LogCombatAbility, Display, TEXT("Ability activated: %s; Slot: %d; Cooldown: %.2fs"),
		*Definition->GetName(), SlotIndex + 1, Definition->CooldownSeconds);
	return true;
}

const UCombatAbilityDefinition* UCombatAbilityComponent::GetSlotDefinition(int32 SlotIndex) const
{
	if (!Slots.IsValidIndex(SlotIndex) || !IsValid(Slots[SlotIndex].Ability.Get()))
	{
		return nullptr;
	}

	const UCombatAbilityDefinition* Definition = Slots[SlotIndex].Definition.Get();
	return IsValid(Definition) ? Definition : nullptr;
}

float UCombatAbilityComponent::GetSlotCooldownRemaining(int32 SlotIndex) const
{
	const UWorld* World = GetWorld();
	if (!World || !Slots.IsValidIndex(SlotIndex))
	{
		return 0.0f;
	}

	return FMath::Max(0.0f, World->GetTimerManager().GetTimerRemaining(Slots[SlotIndex].Cooldown));
}

float UCombatAbilityComponent::GetSlotEffectRemaining(int32 SlotIndex) const
{
	const UCombatAbility* Ability = Slots.IsValidIndex(SlotIndex) ? Slots[SlotIndex].Ability.Get() : nullptr;
	if (!IsValid(Ability))
	{
		return 0.0f;
	}

	for (const UCombatEffect* Effect : ActiveEffects)
	{
		if (Effect->IsFrom(*Ability))
		{
			return Effect->GetRemainingSeconds();
		}
	}
	return 0.0f;
}

void UCombatAbilityComponent::SetCombatTimePaused(bool bPaused)
{
	bCombatTimePaused = bPaused;
	for (UCombatEffect* Effect : ActiveEffects)
	{
		Effect->SetPaused(bPaused);
	}
	if (UWorld* World = GetWorld())
	{
		FTimerManager& Timers = World->GetTimerManager();
		for (FCombatAbilitySlot& Slot : Slots)
		{
			if (bPaused)
			{
				Timers.PauseTimer(Slot.Cooldown);
			}
			else
			{
				Timers.UnPauseTimer(Slot.Cooldown);
			}
		}
	}
}

bool UCombatAbilityComponent::StartPeriodicEffect(UCombatAbility& Source,
	const UCombatAbilityDefinition& Definition, FSimpleDelegate PeriodicAction)
{
	ACombatHero* Hero = Cast<ACombatHero>(GetOwner());
	UWorld* World = GetWorld();
	if (bCombatTimePaused || !IsValid(Hero) || !World || !PeriodicAction.IsBound() ||
		!FMath::IsFinite(Definition.DurationSeconds) || !FMath::IsFinite(Definition.TickIntervalSeconds) ||
		Definition.TickIntervalSeconds <= 0.0f || Definition.DurationSeconds <= Definition.TickIntervalSeconds)
	{
		return false;
	}

	if (ActiveEffects.ContainsByPredicate([&Source](const UCombatEffect* Effect) { return Effect->IsFrom(Source); }))
	{
		return false;
	}

	UCombatEffect* Effect = NewObject<UCombatEffect>(this);
	ActiveEffects.Add(Effect);
	Effect->Start(World->GetTimerManager(), Source, *Hero, Definition, MoveTemp(PeriodicAction));
	return true;
}

void UCombatAbilityComponent::FinishEffect(UCombatEffect& Effect)
{
	if (ActiveEffects.RemoveSingle(&Effect) > 0)
	{
		Effect.Stop();
	}
}

void UCombatAbilityComponent::ClearEffects()
{
	for (UCombatEffect* Effect : ActiveEffects)
	{
		Effect->Stop();
	}
	ActiveEffects.Reset();
}

void UCombatAbilityComponent::ClearSlots()
{
	ClearEffects();
	if (UWorld* World = GetWorld())
	{
		FTimerManager& Timers = World->GetTimerManager();
		for (FCombatAbilitySlot& Slot : Slots)
		{
			Timers.ClearTimer(Slot.Cooldown);
		}
	}
	Slots.Reset();
}

void UCombatAbilityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearSlots();
	Super::EndPlay(EndPlayReason);
}
