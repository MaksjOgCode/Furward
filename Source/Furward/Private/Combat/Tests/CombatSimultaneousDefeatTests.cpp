// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Проверяет исход двух смертельных ударов на реальном L_Combat.
 */

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Combat/CombatAttributesComponent.h"
#include "Combat/CombatEnemy.h"
#include "Combat/CombatGameMode.h"
#include "Combat/CombatHero.h"
#include "Combat/CombatRulesDefinition.h"
#include "Combat/CombatSession.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Profile/FurwardProfileSubsystem.h"
#include "Tests/AutomationCommon.h"

/** @brief Сравнивает оба порядка ударов и отделяет гибель в разных кадрах. */
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FCombatSimultaneousDefeatTest, "Furward.Combat.SimultaneousDefeat",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

namespace
{
const FName StageID(TEXT("Stage_01"));
const FName CurrencyID(TEXT("Scrap"));

enum class EDefeatScenario : uint8
{
	HeroFirst,
	BossFirst,
	HeroOnly
};

/** @brief Подаёт удары в одном кадре и проверяет итог после обычного конца кадра движка. */
class FVerifySimultaneousDefeat final : public IAutomationLatentCommand
{
public:
	FVerifySimultaneousDefeat(FAutomationTestBase& InTest, EDefeatScenario InScenario)
		: Test(InTest), Scenario(InScenario), Deadline(FPlatformTime::Seconds() + 15.0)
	{
	}

	bool Update() override
	{
		UWorld* World = AutomationCommon::GetAnyGameWorld();
		ACombatGameMode* GameMode = World ? World->GetAuthGameMode<ACombatGameMode>() : nullptr;
		UCombatSession* Session = GameMode ? GameMode->GetCombatSession() : nullptr;
		UFurwardProfileSubsystem* Profile = World ? UGameInstance::GetSubsystem<UFurwardProfileSubsystem>(World->GetGameInstance()) : nullptr;
		if (Session && Profile && TiedEncounterID.IsValid())
		{
			VerifyResult(*Session, *Profile);
			return true;
		}

		const ACombatEnemy* Target = Session ? Session->GetActiveEnemy() : nullptr;
		ACombatHero* Hero = World ? Cast<ACombatHero>(UGameplayStatics::GetActorOfClass(World, ACombatHero::StaticClass())) : nullptr;
		if (!Target || !Hero || !Profile)
		{
			if (FPlatformTime::Seconds() < Deadline)
			{
				return false;
			}
			Test.AddError(TEXT("The map did not prepare participants for the simultaneous defeat scenario."));
			return true;
		}

		if (Session->GetPhase() == ECombatPhase::Preparing && !Initialize(*Session, *Profile))
		{
			return true;
		}

		if (Scenario == EDefeatScenario::BossFirst && !Profile->IsExpeditionClearComplete(StageID))
		{
			const float Health = Target->GetAttributes().GetCurrentHealth();
			if (!Test.TestTrue(TEXT("Each ordinary victory advances the actual expedition"), Session->ApplyResolvedDamageToEnemy(Target, Health) > 0.0f))
			{
				return true;
			}
			return false;
		}

		if (Scenario == EDefeatScenario::BossFirst)
		{
			Test.TestTrue(TEXT("Ten clears prepared the boss"), Session->GetProgressionForDisplay().BossStatus == ECombatBossStatus::OnArena);
		}
		SubmitLethalHits(*Session, *Profile, *Hero, *Target);
		return false;
	}

private:
	bool Initialize(UCombatSession& Session, UFurwardProfileSubsystem& Profile)
	{
		const UCombatRulesDefinition* Authored = LoadObject<UCombatRulesDefinition>(nullptr,
			TEXT("/Game/Combat-System/Data/DA_Combat_Rules.DA_Combat_Rules"));
		if (!Test.TestTrue(TEXT("Combat rules configure simultaneous defeat"), Authored &&
			Authored->SimultaneousDeath == ECombatSimultaneousDeathRule::DefeatWithVictoryCredit) ||
			!Test.TestTrue(TEXT("Test profile reset"), Profile.TryRestoreSnapshot(FFurwardProfileSnapshot{})))
		{
			return false;
		}

		// Выключаем автономный таймер в копии правил, чтобы тест подавал ответ в выбранном порядке.
		UCombatRulesDefinition* Rules = DuplicateObject<UCombatRulesDefinition>(Authored, &Session);
		Rules->EnemyAttackIntervalSeconds = 0.0f;
		Session.EnablePIECombatPreview(Rules, NAME_None);
		return Test.TestTrue(TEXT("Participants initialized"),
			Session.InitializeParticipantAttributes(FCombatBaseAttributes{120.0f, 22.0f, 7.0f}, 70.0f,
				FCombatBaseAttributes{72.0f, 13.2f, 4.2f}));
	}

	void SubmitLethalHits(UCombatSession& Session, const UFurwardProfileSubsystem& Profile, const ACombatHero& Hero,
		const ACombatEnemy& Target)
	{
		Session.ApplyResolvedDamageToEnemy(&Target, Target.GetAttributes().GetCurrentHealth() - 1.0f);
		TiedEncounterID = Session.GetEncounterID();
		InitialRevision = Profile.GetRevision();
		const float HeroHealth = Hero.GetAttributes().GetCurrentHealth();

		if (Scenario == EDefeatScenario::BossFirst)
		{
			Test.TestTrue(TEXT("The player can land the final hit first"), Session.TrySubmitClickAttack(ECombatClickResult::Hit, &Target));
			Test.TestTrue(TEXT("The defeated boss can land the reply in the same frame"),
				Session.ApplyResolvedDamageToHero(&Target, HeroHealth) > 0.0f);
		}
		else
		{
			Test.TestTrue(TEXT("The enemy can land the final hit first"), Session.ApplyResolvedDamageToHero(&Target, HeroHealth) > 0.0f);
			if (Scenario == EDefeatScenario::HeroFirst)
			{
				Session.SubmitAutoClick();
				Test.TestTrue(TEXT("The last auto-click can kill the same enemy"), Target.GetAttributes().IsDead());
			}
		}
		Test.TestTrue(TEXT("The encounter is still Fighting before frame end"), Session.GetPhase() == ECombatPhase::Fighting);
		Test.TestEqual(TEXT("The profile has no partial result"), Profile.GetRevision(), InitialRevision);
	}

	void VerifyResult(UCombatSession& Session, const UFurwardProfileSubsystem& Profile)
	{
		const bool bVictory = Scenario != EDefeatScenario::HeroOnly;
		const bool bBoss = Scenario == EDefeatScenario::BossFirst;
		Test.TestTrue(TEXT("The hero ends in Defeated"), Session.GetPhase() == ECombatPhase::Defeated);
		Test.TestTrue(TEXT("The encounter did not replace its enemy"), Session.GetEncounterID() == TiedEncounterID);
		const TOptional<FCombatDefeatSnapshot> Snapshot = Session.GetDefeatSnapshot();
		Test.TestTrue(TEXT("Defeat shows the enemy HP after the damage batch"),
			Snapshot.IsSet() && Snapshot->EnemyCurrentHealth == (bVictory ? 0.0f : 1.0f));
		const TOptional<FCombatEncounterResult> Result = Session.GetEncounterResult();
		Test.TestTrue(TEXT("Victory result matches the simultaneous death"),
			bVictory ? Result.IsSet() && Result->EncounterID == TiedEncounterID && Result->bDefeatedBoss == bBoss : !Result.IsSet());
		Test.TestEqual(TEXT("Exactly one new revision for a credited kill"), Profile.GetRevision(), InitialRevision + (bVictory ? 1 : 0));
		Test.TestEqual(TEXT("The profile credits the expected Scrap"), Profile.GetCurrencyBalance(CurrencyID),
			int64(bBoss ? 70 : (bVictory ? 5 : 0)));
		Test.TestEqual(TEXT("Expedition clears are updated once"), Profile.GetExpeditionClearCount(StageID),
			bBoss ? UFurwardProfileSubsystem::RequiredExpeditionClears : (bVictory ? 1 : 0));
		Test.TestEqual(TEXT("Boss completion is stored even with a defeat screen"), Profile.IsExpeditionStageCompleted(StageID), bBoss);

		Session.SubmitAutoClick();
		Test.TestEqual(TEXT("Later attack cannot add another reward"), Profile.GetRevision(), InitialRevision + (bVictory ? 1 : 0));
	}

	FAutomationTestBase& Test;
	EDefeatScenario Scenario;
	double Deadline;
	FGuid TiedEncounterID;
	int64 InitialRevision = 0;
};
}

void FCombatSimultaneousDefeatTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Append({TEXT("HeroFirst"), TEXT("BossFirst"), TEXT("HeroOnly")});
	OutTestCommands = OutBeautifiedNames;
}

bool FCombatSimultaneousDefeatTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/Combat-System/Maps/L_Combat"), true))
	{
		AddError(TEXT("Could not open L_Combat."));
		return false;
	}

	EDefeatScenario Scenario = EDefeatScenario::HeroOnly;
	if (Parameters == TEXT("HeroFirst"))
	{
		Scenario = EDefeatScenario::HeroFirst;
	}
	else if (Parameters == TEXT("BossFirst"))
	{
		Scenario = EDefeatScenario::BossFirst;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FVerifySimultaneousDefeat(*this, Scenario));
	return true;
}

#endif
