// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Проверяет запись баланса валют и прогресса в отдельный слот и восстановление в новом профиле.
 */

#if WITH_DEV_AUTOMATION_TESTS

#include "Save/FurwardSaveGame.h"
#include "Save/FurwardSaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Profile/FurwardProfileSubsystem.h"
#include "UObject/UObjectGlobals.h"

/** @brief Проверяет, что слот хранит именно выбранную ревизию прогресса. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFurwardProfileSaveRoundTripTest, "Furward.Save.ProfileRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FFurwardProfileSaveRoundTripTest::RunTest(const FString& Parameters)
{
	const FName FinishedStage(TEXT("Stage_01"));
	const FName PartialStage(TEXT("Stage_02"));
	constexpr int32 PartialClears = 7;
	const FGuid BossEncounterID = FGuid::NewGuid();
	const FGuid FarmEncounterID = FGuid::NewGuid();
	const FName Scrap(TEXT("Scrap"));
	const TMap<FName, int64> FarmReward{{Scrap, 5}};
	UFurwardProfileSubsystem* Original = NewObject<UFurwardProfileSubsystem>(NewObject<UGameInstance>());

	for (int32 Clear = 0; Clear < UFurwardProfileSubsystem::RequiredExpeditionClears; ++Clear)
	{
		TestTrue(TEXT("Record an ordinary victory on the first stage"),
			Original->TryRecordExpeditionClear(FinishedStage, FGuid::NewGuid()));
	}
	TestTrue(TEXT("Record the first stage's boss victory"), Original->TryCompleteExpeditionStage(FinishedStage, BossEncounterID));
	for (int32 Clear = 0; Clear < PartialClears; ++Clear)
	{
		TestTrue(TEXT("Record partial progress on the second stage"),
			Original->TryRecordExpeditionClear(PartialStage, FGuid::NewGuid()));
	}

	TestTrue(TEXT("Record a farm victory and its currency reward"),
		Original->TryRecordFarmVictory(PartialStage, FarmEncounterID, FarmReward));

	UFurwardSaveGame* SaveGame = CastChecked<UFurwardSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UFurwardSaveGame::StaticClass()));
	TestNull(TEXT("An empty save has no versioned snapshot"), SaveGame->GetProfileSnapshot());
	SaveGame->SetProfileSnapshot(Original->CreateSnapshot());
	const int64 SavedRevision = UFurwardProfileSubsystem::RequiredExpeditionClears + 2 + PartialClears;

	// Меняем профиль после формирования снимка, чтобы проверить, что запись использует прежнюю копию.
	TestTrue(TEXT("The live profile can receive another victory"),
		Original->TryRecordExpeditionClear(PartialStage, FGuid::NewGuid(), FarmReward));
	TestEqual(TEXT("The live currency balance has advanced"), Original->GetCurrencyBalance(Scrap), int64{10});
	TestEqual(TEXT("The live profile has a newer revision"), Original->CreateSnapshot().Revision, SavedRevision + 1);

	// Случайное имя отделяет проверку от любых игровых слотов. Очищаем только этот слот.
	const FString SlotName = FString::Printf(TEXT("FurwardAutomation_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	constexpr int32 UserIndex = 0;
	if (!TestFalse(TEXT("The generated slot starts empty"), UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex)))
	{
		return false;
	}
	ON_SCOPE_EXIT
	{
		if (UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
		{
			TestTrue(TEXT("Remove the automation slot"), UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex));
		}
	};

	if (!TestTrue(TEXT("Write the versioned snapshot to disk"), UGameplayStatics::SaveGameToSlot(SaveGame, SlotName, UserIndex)))
	{
		return false;
	}

	const UFurwardSaveGame* Loaded = Cast<UFurwardSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));
	if (!TestNotNull(TEXT("Read the saved class from disk"), Loaded))
	{
		return false;
	}
	const FFurwardProfileSnapshot* Snapshot = Loaded->GetProfileSnapshot();
	if (!TestNotNull(TEXT("The stored version is supported"), Snapshot))
	{
		return false;
	}

	UGameInstance* RestoredGame = NewObject<UGameInstance>();
	UFurwardProfileSubsystem* Restored = NewObject<UFurwardProfileSubsystem>(RestoredGame);
	UFurwardSaveSubsystem* Save = NewObject<UFurwardSaveSubsystem>(RestoredGame);
	const FName OpenedSlot(*SlotName);
	if (!TestTrue(TEXT("Open the existing slot for a new game instance"), Save->TryOpenProfileSlot(OpenedSlot, *Restored)))
	{
		return false;
	}

	TestEqual(TEXT("Load restores the saved revision"), Restored->GetRevision(), SavedRevision);
	TestEqual(TEXT("Load restores the currency from the snapshot"), Restored->GetCurrencyBalance(Scrap), int64{5});
	TestFalse(TEXT("The saved farm result cannot pay again"),
		Restored->TryRecordFarmVictory(PartialStage, FarmEncounterID, FarmReward));
	TestEqual(TEXT("Farm replay leaves the balance unchanged"), Restored->GetCurrencyBalance(Scrap), int64{5});
	TestTrue(TEXT("A loaded revision is already confirmed on disk"), Save->GetSaveStatus(OpenedSlot).IsLatestRevisionSaved(SavedRevision));
	TestTrue(TEXT("Load restores the completed stage"), Restored->IsExpeditionStageCompleted(FinishedStage));
	TestEqual(TEXT("Load restores ten clears for the completed stage"),
		Restored->GetExpeditionClearCount(FinishedStage), UFurwardProfileSubsystem::RequiredExpeditionClears);
	TestEqual(TEXT("Load restores seven clears from the snapshot"), Restored->GetExpeditionClearCount(PartialStage), PartialClears);
	TestFalse(TEXT("The saved encounter ledger rejects a replay"), Restored->TryRecordExpeditionClear(PartialStage, BossEncounterID));
	TestEqual(TEXT("A rejected replay does not change revision"), Restored->CreateSnapshot().Revision, SavedRevision);

	FFurwardProfileSnapshot InvalidSnapshot = *Snapshot;
	InvalidSnapshot.CompletedExpeditionStages.Add(PartialStage);
	++InvalidSnapshot.Revision;
	InvalidSnapshot.RecordedEncounterIDs.Add(FGuid::NewGuid());
	TestFalse(TEXT("Reject a corrupt snapshot with a completed stage below ten clears"),
		Restored->TryRestoreSnapshot(InvalidSnapshot));
	TestFalse(TEXT("Rejected data does not change the completion flag"), Restored->IsExpeditionStageCompleted(PartialStage));
	TestEqual(TEXT("Rejected data does not replace the active revision"), Restored->GetRevision(), SavedRevision);

	TestTrue(TEXT("The reopened profile accepts one more clear"), Restored->TryRecordExpeditionClear(PartialStage, FGuid::NewGuid()));
	TestTrue(TEXT("Reentering with the same slot succeeds"), Save->TryOpenProfileSlot(OpenedSlot, *Restored));
	TestEqual(TEXT("Reentry preserves unsaved progress within the same game instance"), Restored->GetRevision(), SavedRevision + 1);
	TestFalse(TEXT("A different slot cannot replace the active profile"), Save->TryOpenProfileSlot(TEXT("AnotherProfile"), *Restored));

	// Повреждённый снимок записывается только в выделенный тестовый слот.
	SaveGame->SetProfileSnapshot(InvalidSnapshot);
	if (!TestTrue(TEXT("Write an inconsistent test fixture"), UGameplayStatics::SaveGameToSlot(SaveGame, SlotName, UserIndex)))
	{
		return false;
	}
	UGameInstance* FreshGame = NewObject<UGameInstance>();
	UFurwardProfileSubsystem* FreshProfile = NewObject<UFurwardProfileSubsystem>(FreshGame);
	UFurwardSaveSubsystem* FreshSave = NewObject<UFurwardSaveSubsystem>(FreshGame);
	TestFalse(TEXT("Reject a slot with inconsistent progress"), FreshSave->TryOpenProfileSlot(OpenedSlot, *FreshProfile));
	TestEqual(TEXT("A failed load keeps the new profile empty"), FreshProfile->GetRevision(), int64{0});
	TestEqual(TEXT("A failed load confirms no revision"), FreshSave->GetSaveStatus(OpenedSlot).LastSavedRevision, int64{-1});

	return true;
}

#endif
