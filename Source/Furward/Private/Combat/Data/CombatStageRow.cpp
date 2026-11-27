// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Проверяет ссылки на окружение и параметры противников до запуска этапа.
 */

#include "Combat/Data/CombatStageRow.h"
#include "Combat/Arena/CombatEnvironmentDefinition.h"
#include "Combat/Participants/CombatEnemy.h"
#include "Combat/Participants/CombatEnemyDefinition.h"
#include "Engine/StaticMesh.h"
#include "Rewards/FurwardLootDefinition.h"

namespace
{
FString FindEnemyConfigurationError(const UCombatEnemyDefinition* Definition, const FCombatBaseAttributes& ExpectedHero)
{
	if (!IsValid(Definition))
	{
		return TEXT("Enemy definition is missing.");
	}
	const UClass* EnemyClass = Definition->GetEnemyClass().Get();
	if (!EnemyClass || EnemyClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return FString::Printf(TEXT("%s: assign a concrete Enemy Class."), *Definition->GetPathName());
	}
	if (!IsValid(Definition->GetStaticMesh()))
	{
		return FString::Printf(TEXT("%s: assign a Static Mesh."), *Definition->GetPathName());
	}

	FCombatBaseAttributes EnemyBase;
	if (!Definition->GetAttributeMultipliers().TryApplyTo(ExpectedHero, EnemyBase))
	{
		return FString::Printf(TEXT("%s: invalid Attribute Multipliers for Expected Hero Attributes."), *Definition->GetPathName());
	}

	if (const UFurwardLootDefinition* Loot = Definition->GetLootDefinition())
	{
		for (const TPair<FName, int64>& Reward : Loot->GetGuaranteedCurrencies())
		{
			if (Reward.Key.IsNone() || Reward.Value <= 0)
			{
				return FString::Printf(TEXT("%s: currency '%s' must have a name and a positive amount (got %lld)."),
					*Loot->GetPathName(), *Reward.Key.ToString(), Reward.Value);
			}
		}
	}
	return {};
}
}

FString FCombatStageRow::FindConfigurationError() const
{
	if (!IsValid(Environment.Get()) || Environment->GetEnvironmentMap().IsNull())
	{
		return FString::Printf(TEXT("Environment %s must reference a map."), *GetPathNameSafe(Environment.Get()));
	}
	if (!ExpectedHeroAttributes.IsValid())
	{
		return TEXT("Invalid Expected Hero Attributes.");
	}
	if (EnemyRoster.IsEmpty())
	{
		return TEXT("Enemy Roster must contain at least one ordinary enemy.");
	}
	const FString BossError = FindEnemyConfigurationError(Boss.Get(), ExpectedHeroAttributes);
	if (!BossError.IsEmpty())
	{
		return FString::Printf(TEXT("Boss: %s"), *BossError);
	}
	for (int32 Index = 0; Index < EnemyRoster.Num(); ++Index)
	{
		const UCombatEnemyDefinition* Enemy = EnemyRoster[Index].Get();
		if (Enemy == Boss.Get())
		{
			return FString::Printf(TEXT("Enemy Roster[%d] refers to the boss definition."), Index);
		}
		const FString Error = FindEnemyConfigurationError(Enemy, ExpectedHeroAttributes);
		if (!Error.IsEmpty())
		{
			return FString::Printf(TEXT("Enemy Roster[%d]: %s"), Index, *Error);
		}
	}
	return {};
}
