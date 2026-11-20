// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Проверяет очередность асинхронных записей, подтверждение ревизии и повтор после ошибки.
 */

#if WITH_DEV_AUTOMATION_TESTS

#include "Save/FurwardSaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Profile/FurwardProfileSubsystem.h"
#include "Save/FurwardSaveGame.h"
#include "Tests/AutomationCommon.h"
#include "UObject/UObjectGlobals.h"

namespace
{
/** @brief Удерживает данные двух временных слотов до завершения проверки очереди. */
struct FSaveQueueTestState
{
	/** @brief Подсистема из игрового GameInstance, которую проверяет автотест. */
	TWeakObjectPtr<UFurwardSaveSubsystem> Save;
	/** @brief Слот для последовательной записи ревизий 1 и 2. */
	FName OrderedSlot;
	/** @brief Слот, для которого первая попытка завершается ошибкой записи. */
	FName RetrySlot;
	/** @brief Путь к каталогу, созданному тестом на месте файла RetrySlot. */
	FString BlockedPath;
	/** @brief Неизменяемые данные первой ревизии для записи и повтора. */
	FFurwardProfileSnapshot FirstSnapshot;
	/** @brief Ревизии в порядке успешных callback слота OrderedSlot. */
	TArray<int64> WrittenRevisions;
	/** @brief Дескриптор слушателя, снимаемого в финале теста. */
	FDelegateHandle Listener;
	/** @brief Время, после которого тест завершится с диагностикой зависшей записи. */
	double Deadline = 0.0;
	/** @brief Количество полученных callback слота RetrySlot. */
	int32 RetryCallbacks = 0;
	/** @brief Отмечает окончание сценария ошибки и повтора. */
	bool bRetryFinished = false;
};
}

/** @brief Проверяет фактические файлы и состояние очереди в игровом GameInstance. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFurwardSaveQueueTest, "Furward.Save.SequentialQueue",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

/** @brief Ждёт обе последовательные записи и повтор, затем убирает временные слоты. */
DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FVerifyFurwardSaveQueue,
	FAutomationTestBase*, Test, TSharedRef<FSaveQueueTestState>, State);

bool FVerifyFurwardSaveQueue::Update()
{
	const bool bComplete = State->WrittenRevisions.Num() >= 2 && State->bRetryFinished;
	if (!bComplete && FPlatformTime::Seconds() < State->Deadline)
	{
		return false;
	}

	if (UFurwardSaveSubsystem* Save = State->Save.Get())
	{
		Save->OnSaveFinished().Remove(State->Listener);
	}

	if (!bComplete)
	{
		Test->AddError(TEXT("The save queue did not finish within the allotted time."));
		return true;
	}

	Test->TestTrue(TEXT("Only revisions one and two were written, in order"),
		State->WrittenRevisions.Num() == 2 && State->WrittenRevisions[0] == 1 && State->WrittenRevisions[1] == 2);

	IFileManager& Files = IFileManager::Get();
	if (Files.DirectoryExists(*State->BlockedPath))
	{
		Test->TestTrue(TEXT("Remove the empty blocker created by this test"),
			Files.DeleteDirectory(*State->BlockedPath, true, false));
	}
	for (const FName Slot : {State->OrderedSlot, State->RetrySlot})
	{
		if (UGameplayStatics::DoesSaveGameExist(Slot.ToString(), UFurwardSaveSubsystem::LocalUserIndex))
		{
			Test->TestTrue(TEXT("Remove the slot created by this test"),
				UGameplayStatics::DeleteGameInSlot(Slot.ToString(), UFurwardSaveSubsystem::LocalUserIndex));
		}
	}
	return true;
}

bool FFurwardSaveQueueTest::RunTest(const FString& Parameters)
{
	const UWorld* World = AutomationCommon::GetAnyGameWorld();
	UFurwardSaveSubsystem* Save = World
		? UGameInstance::GetSubsystem<UFurwardSaveSubsystem>(World->GetGameInstance())
		: nullptr;
	if (!TestNotNull(TEXT("The GameInstance created its save subsystem"), Save))
	{
		return false;
	}

	const TSharedRef<FSaveQueueTestState> State = MakeShared<FSaveQueueTestState>();
	State->Save = Save;
	const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
	State->OrderedSlot = FName(*(TEXT("FurwardQueue_") + Suffix));
	State->RetrySlot = FName(*(TEXT("FurwardRetry_") + Suffix));
	State->BlockedPath = FPaths::ProjectSavedDir() / TEXT("SaveGames") / (State->RetrySlot.ToString() + TEXT(".sav"));
	State->Deadline = FPlatformTime::Seconds() + 15.0;

	// Каталог по имени файла даёт настоящую ошибку открытия. Путь выбирается для этого теста.
	IFileManager& Files = IFileManager::Get();
	if (!TestFalse(TEXT("The test blocker path is unused"),
		Files.FileExists(*State->BlockedPath) || Files.DirectoryExists(*State->BlockedPath)) ||
		!TestTrue(TEXT("Create the test blocker"), Files.MakeDirectory(*State->BlockedPath, true)))
	{
		return false;
	}

	State->Listener = Save->OnSaveFinished().AddLambda([this, State](FName Slot, int64 Revision, bool bSuccess)
	{
		UFurwardSaveSubsystem* SaveSubsystem = State->Save.Get();
		const FFurwardSaveStatus Status = SaveSubsystem->GetSaveStatus(Slot);
		if (Slot == State->OrderedSlot)
		{
			State->WrittenRevisions.Add(Revision);
			TestTrue(TEXT("The ordered slot writes successfully"), bSuccess);
			TestEqual(TEXT("The latest request stays at revision two"), Status.LatestRequestedRevision, int64{2});
			TestEqual(TEXT("Only the completed write updates the confirmed revision"), Status.LastSavedRevision, Revision);
			TestEqual(TEXT("A previous write does not confirm the latest request"),
				Status.IsLatestRevisionSaved(2), Revision == 2);

			const UFurwardSaveGame* Stored = Cast<UFurwardSaveGame>(
				UGameplayStatics::LoadGameFromSlot(Slot.ToString(), UFurwardSaveSubsystem::LocalUserIndex));
			const FFurwardProfileSnapshot* StoredSnapshot = Stored ? Stored->GetProfileSnapshot() : nullptr;
			if (TestNotNull(TEXT("The completed write is readable before the next starts"), StoredSnapshot))
			{
				TestEqual(TEXT("The file contains the confirmed revision"), StoredSnapshot->Revision, Revision);
				TestEqual(TEXT("The file uses the accepted snapshot"), StoredSnapshot->ExpeditionClearsByStage.FindRef(TEXT("Stage_01")),
					static_cast<int32>(Revision));
			}
			return;
		}

		if (Slot != State->RetrySlot)
		{
			return;
		}

		++State->RetryCallbacks;
		if (State->RetryCallbacks == 1)
		{
			TestFalse(TEXT("A blocked file produces a save error"), bSuccess);
			TestTrue(TEXT("The latest revision reports a retryable error"), Status.bLatestWriteFailed);
			TestFalse(TEXT("A failed write never confirms the revision"), Status.IsLatestRevisionSaved(Revision));
			if (!TestTrue(TEXT("Unblock the test slot"), IFileManager::Get().DeleteDirectory(*State->BlockedPath, true, false)) ||
				!TestTrue(TEXT("Retry the same failed revision"), SaveSubsystem->RequestSave(Slot, State->FirstSnapshot)))
			{
				State->bRetryFinished = true;
			}
			return;
		}

		TestTrue(TEXT("The retry writes the original revision"), bSuccess && Revision == 1);
		TestTrue(TEXT("A successful retry confirms the latest revision"), Status.IsLatestRevisionSaved(1));
		State->bRetryFinished = true;
	});

	UFurwardProfileSubsystem* Profile = NewObject<UFurwardProfileSubsystem>(NewObject<UGameInstance>());
	const FName Stage(TEXT("Stage_01"));
	Profile->TryRecordExpeditionClear(Stage, FGuid::NewGuid());
	State->FirstSnapshot = Profile->CreateSnapshot();

	TestTrue(TEXT("Queue revision one"), Save->RequestSave(State->OrderedSlot, State->FirstSnapshot));
	Profile->TryRecordExpeditionClear(Stage, FGuid::NewGuid());
	const FFurwardProfileSnapshot SecondSnapshot = Profile->CreateSnapshot();
	TestTrue(TEXT("Queue revision two during the first write"), Save->RequestSave(State->OrderedSlot, SecondSnapshot));
	TestTrue(TEXT("A request for the queued revision is already covered"),
		Save->RequestSave(State->OrderedSlot, SecondSnapshot));
	TestFalse(TEXT("A stale snapshot cannot be queued behind a newer one"),
		Save->RequestSave(State->OrderedSlot, State->FirstSnapshot));
	TestFalse(TEXT("Pending writes have not confirmed the slot"), Save->GetSaveStatus(State->OrderedSlot).IsLatestRevisionSaved(2));
	Profile->TryRecordExpeditionClear(Stage, FGuid::NewGuid());

	TestTrue(TEXT("Queue the independent slot with a blocked file"),
		Save->RequestSave(State->RetrySlot, State->FirstSnapshot));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyFurwardSaveQueue(this, State));
	return true;
}

#endif
