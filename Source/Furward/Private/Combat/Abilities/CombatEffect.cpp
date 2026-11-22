// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Отсчитывает срок действия, запускает периоды и очищает таймеры временного эффекта.
 */

#include "Combat/Abilities/CombatEffect.h"
#include "Combat/Abilities/CombatAbility.h"
#include "Combat/Abilities/CombatAbilityComponent.h"
#include "Combat/Abilities/CombatAbilityDefinition.h"
#include "Combat/CombatHero.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCombatEffect, Log, All);

void UCombatEffect::Start(FTimerManager& Timers, UCombatAbility& InSource, ACombatHero& InTarget,
	const UCombatAbilityDefinition& Definition, FSimpleDelegate InPeriodicAction)
{
	Source = &InSource;
	Target = &InTarget;
	PeriodicAction = MoveTemp(InPeriodicAction);
	TickIntervalSeconds = Definition.TickIntervalSeconds;
	bActive = true;

	Timers.SetTimer(ExpirationTimer, this, &UCombatEffect::Expire, Definition.DurationSeconds);
	Timers.SetTimer(PeriodicTimer, this, &UCombatEffect::RunPeriodicAction, TickIntervalSeconds);

	UE_LOG(LogCombatEffect, Display, TEXT("Combat effect started: %s; Duration: %.2fs; Interval: %.2fs"),
		*InSource.GetClass()->GetName(), Definition.DurationSeconds, Definition.TickIntervalSeconds);
}

bool UCombatEffect::IsFrom(const UCombatAbility& Ability) const
{
	return Source.Get() == &Ability;
}

float UCombatEffect::GetRemainingSeconds() const
{
	const FTimerManager* Timers = GetTimers();
	return bActive && Timers ? FMath::Max(0.0f, Timers->GetTimerRemaining(ExpirationTimer)) : 0.0f;
}

void UCombatEffect::RunPeriodicAction()
{
	FTimerManager* Timers = GetTimers();
	// Срок действия имеет приоритет, если оба таймера сработали в одном кадре.
	if (!bActive || bPaused || !Source.IsValid() || !Target.IsValid() ||
		!Timers || Timers->GetTimerRemaining(ExpirationTimer) <= 0.0f)
	{
		return;
	}

	// Назначаем следующий удар до команды. При замене врага сессия приостановит уже этот таймер.
	Timers->SetTimer(PeriodicTimer, this, &UCombatEffect::RunPeriodicAction, TickIntervalSeconds);

	// Действие может синхронно завершить бой и очистить этот эффект.
	const FSimpleDelegate Action = PeriodicAction;
	Action.ExecuteIfBound();
}

void UCombatEffect::SetPaused(bool bInPaused)
{
	if (!bActive || bInPaused == bPaused)
	{
		return;
	}

	bPaused = bInPaused;
	if (FTimerManager* Timers = GetTimers())
	{
		if (bPaused)
		{
			Timers->PauseTimer(ExpirationTimer);
			Timers->PauseTimer(PeriodicTimer);
		}
		else
		{
			Timers->UnPauseTimer(ExpirationTimer);
			Timers->UnPauseTimer(PeriodicTimer);
		}
	}
}

void UCombatEffect::Expire()
{
	CastChecked<UCombatAbilityComponent>(GetOuter())->FinishEffect(*this);
}

void UCombatEffect::Stop()
{
	if (!bActive)
	{
		return;
	}

	bActive = false;
	if (FTimerManager* Timers = GetTimers())
	{
		Timers->ClearTimer(PeriodicTimer);
		Timers->ClearTimer(ExpirationTimer);
	}
	PeriodicAction.Unbind();
	if (const UCombatAbility* Ability = Source.Get())
	{
		UE_LOG(LogCombatEffect, Display, TEXT("Combat effect ended: %s"), *Ability->GetClass()->GetName());
	}
	Source.Reset();
	Target.Reset();
}

FTimerManager* UCombatEffect::GetTimers() const
{
	UWorld* World = GetOuter()->GetWorld();
	return World ? &World->GetTimerManager() : nullptr;
}
