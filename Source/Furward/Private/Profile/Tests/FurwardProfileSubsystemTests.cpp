// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Проверяет однократный учёт побед, независимость этапов и порог зачистки.
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
	const FGuid FirstEncounterID = FGuid::NewGuid();

	TestEqual(TEXT("A new stage starts at zero clears"), Profile->GetExpeditionClearCount(FirstStage), 0);
	TestFalse(TEXT("An invalid encounter ID is rejected"), Profile->TryRecordExpeditionClear(FirstStage, FGuid()));
	TestFalse(TEXT("An empty stage ID is rejected"), Profile->TryRecordExpeditionClear(NAME_None, FirstEncounterID));
	TestTrue(TEXT("A valid victory is counted"), Profile->TryRecordExpeditionClear(FirstStage, FirstEncounterID));
	TestFalse(TEXT("A victory cannot count twice"), Profile->TryRecordExpeditionClear(FirstStage, FirstEncounterID));
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
	TestTrue(TEXT("The tenth distinct victory counts"), Profile->TryRecordExpeditionClear(FirstStage, FGuid::NewGuid()));
	TestTrue(TEXT("Ten victories complete the ordinary clear"), Profile->IsExpeditionClearComplete(FirstStage));
	TestFalse(TEXT("Further ordinary victories cannot overfill the stage"),
		Profile->TryRecordExpeditionClear(FirstStage, FGuid::NewGuid()));
	TestEqual(TEXT("The completed stage remains at ten clears"), Profile->GetExpeditionClearCount(FirstStage), 10);
	TestTrue(TEXT("Another stage still accepts a victory"), Profile->TryRecordExpeditionClear(SecondStage, FGuid::NewGuid()));
	TestEqual(TEXT("The second stage has its own count"), Profile->GetExpeditionClearCount(SecondStage), 1);

	return true;
}

#endif
