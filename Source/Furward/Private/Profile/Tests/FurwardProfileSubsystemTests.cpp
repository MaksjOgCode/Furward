// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Проверяет учёт побед, порог зачистки и завершение этапа после босса.
 */

#if WITH_DEV_AUTOMATION_TESTS

#include "Profile/FurwardProfileSubsystem.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

/** @brief Проверяет изменение зачистки на отдельном экземпляре профиля. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFurwardExpeditionClearTest, "Furward.Profile.ExpeditionClear",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FFurwardExpeditionClearTest::RunTest(const FString& Parameters)
{
	UFurwardProfileSubsystem* Profile = NewObject<UFurwardProfileSubsystem>(NewObject<UGameInstance>());
	const FName FirstStage(TEXT("Stage_01"));
	const FName SecondStage(TEXT("Stage_02"));
	const FName Scrap(TEXT("Scrap"));
	const TMap<FName, int64> OrdinaryReward{{Scrap, 5}};
	const TMap<FName, int64> BossReward{{Scrap, 20}};
	const FGuid FirstEncounterID = FGuid::NewGuid();
	const FGuid BossEncounterID = FGuid::NewGuid();

	TestEqual(TEXT("A new stage starts at zero clears"), Profile->GetExpeditionClearCount(FirstStage), 0);
	TestFalse(TEXT("An invalid encounter ID is rejected"), Profile->TryRecordExpeditionClear(FirstStage, FGuid()));
	TestFalse(TEXT("An empty stage ID is rejected"), Profile->TryRecordExpeditionClear(NAME_None, FirstEncounterID));
	TestEqual(TEXT("A new wallet starts at zero"), Profile->GetCurrencyBalance(Scrap), int64{0});
	TestTrue(TEXT("A valid victory counts and grants its reward"),
		Profile->TryRecordExpeditionClear(FirstStage, FirstEncounterID, OrdinaryReward));
	TestEqual(TEXT("Rewards and clear use one revision"), Profile->GetRevision(), int64{1});
	TestFalse(TEXT("A victory cannot grant currency twice"),
		Profile->TryRecordExpeditionClear(FirstStage, FirstEncounterID, OrdinaryReward));
	TestFalse(TEXT("A recorded expedition victory also cannot be replayed in farm"),
		Profile->TryRecordFarmVictory(FirstStage, FirstEncounterID, OrdinaryReward));
	TestEqual(TEXT("Replay leaves the wallet untouched"), Profile->GetCurrencyBalance(Scrap), int64{5});

	const FGuid FarmEncounterID = FGuid::NewGuid();
	TestTrue(TEXT("A distinct farm win grants the configured currency"),
		Profile->TryRecordFarmVictory(FirstStage, FarmEncounterID, OrdinaryReward));
	TestEqual(TEXT("Farm preserves expedition clears"), Profile->GetExpeditionClearCount(FirstStage), 1);
	TestEqual(TEXT("Farm adds currency once"), Profile->GetCurrencyBalance(Scrap), int64{10});
	TestEqual(TEXT("Farm advances the same profile revision"), Profile->GetRevision(), int64{2});
	TestFalse(TEXT("Reject a farm replay through expedition"),
		Profile->TryRecordExpeditionClear(FirstStage, FarmEncounterID, OrdinaryReward));

	const FGuid InvalidRewardEncounter = FGuid::NewGuid();
	const FName OtherCurrency(TEXT("Other"));
	const TMap<FName, int64> OverflowingReward{{OtherCurrency, 1}, {Scrap, MAX_int64}};
	TestFalse(TEXT("Reject a reward bundle if one balance would overflow"),
		Profile->TryRecordExpeditionClear(FirstStage, InvalidRewardEncounter, OverflowingReward));
	TestEqual(TEXT("No partial reward after a rejected bundle"), Profile->GetCurrencyBalance(OtherCurrency), int64{0});
	TestEqual(TEXT("Rejected reward does not advance clears"), Profile->GetExpeditionClearCount(FirstStage), 1);
	TestEqual(TEXT("Rejected reward does not advance the revision"), Profile->GetRevision(), int64{2});
	TestFalse(TEXT("A recorded encounter cannot be reused for another stage"),
		Profile->TryRecordExpeditionClear(SecondStage, FirstEncounterID));
	TestEqual(TEXT("Only the first stage advances"), Profile->GetExpeditionClearCount(FirstStage), 1);
	TestEqual(TEXT("The second stage remains untouched"), Profile->GetExpeditionClearCount(SecondStage), 0);

	for (int32 Clear = 2; Clear < UFurwardProfileSubsystem::RequiredExpeditionClears; ++Clear)
	{
		TestTrue(TEXT("A distinct victory advances the expedition"),
			Profile->TryRecordExpeditionClear(FirstStage, FGuid::NewGuid()));
	}
	TestEqual(TEXT("The stage reaches nine clears"), Profile->GetExpeditionClearCount(FirstStage), 9);
	TestFalse(TEXT("Nine victories are below the boss threshold"), Profile->IsExpeditionClearComplete(FirstStage));
	TestFalse(TEXT("A boss victory requires the complete ordinary clear"),
		Profile->TryCompleteExpeditionStage(FirstStage, BossEncounterID, BossReward));
	TestEqual(TEXT("An early boss result does not pay its reward"), Profile->GetCurrencyBalance(Scrap), int64{10});
	TestFalse(TEXT("An early boss result leaves the stage unfinished"), Profile->IsExpeditionStageCompleted(FirstStage));
	TestTrue(TEXT("The tenth distinct victory counts"), Profile->TryRecordExpeditionClear(FirstStage, FGuid::NewGuid()));
	TestTrue(TEXT("Ten victories complete the ordinary clear"), Profile->IsExpeditionClearComplete(FirstStage));
	TestFalse(TEXT("Further ordinary victories cannot overfill the stage"),
		Profile->TryRecordExpeditionClear(FirstStage, FGuid::NewGuid()));
	TestEqual(TEXT("The completed stage remains at ten clears"), Profile->GetExpeditionClearCount(FirstStage), 10);
	TestFalse(TEXT("Ten ordinary clears alone leave the stage unfinished"), Profile->IsExpeditionStageCompleted(FirstStage));
	TestFalse(TEXT("A boss result requires a valid encounter ID"), Profile->TryCompleteExpeditionStage(FirstStage, FGuid()));
	TestFalse(TEXT("An ordinary encounter ID cannot be used for boss completion"),
		Profile->TryCompleteExpeditionStage(FirstStage, FirstEncounterID));
	TestTrue(TEXT("A valid boss result completes the stage after ten clears"),
		Profile->TryCompleteExpeditionStage(FirstStage, BossEncounterID, BossReward));
	TestEqual(TEXT("The boss pays when its stage is completed"), Profile->GetCurrencyBalance(Scrap), int64{30});
	TestTrue(TEXT("The profile remembers the completed stage"), Profile->IsExpeditionStageCompleted(FirstStage));
	TestEqual(TEXT("Boss completion preserves the ordinary clear"), Profile->GetExpeditionClearCount(FirstStage), 10);
	TestFalse(TEXT("Replaying the boss result does not apply it twice"),
		Profile->TryCompleteExpeditionStage(FirstStage, BossEncounterID, BossReward));
	TestEqual(TEXT("A replayed boss keeps the same balance"), Profile->GetCurrencyBalance(Scrap), int64{30});
	TestFalse(TEXT("A new encounter cannot complete an already completed stage"),
		Profile->TryCompleteExpeditionStage(FirstStage, FGuid::NewGuid()));
	TestFalse(TEXT("An applied boss result cannot count as an ordinary victory on another stage"),
		Profile->TryRecordExpeditionClear(SecondStage, BossEncounterID));
	TestFalse(TEXT("Boss completion applies only to its own stage"), Profile->IsExpeditionStageCompleted(SecondStage));
	TestTrue(TEXT("Another stage still accepts a victory"), Profile->TryRecordExpeditionClear(SecondStage, FGuid::NewGuid()));
	TestEqual(TEXT("The second stage has its own count"), Profile->GetExpeditionClearCount(SecondStage), 1);

	return true;
}

#endif
