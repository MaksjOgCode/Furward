// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Проверяет на L_Combat, какие запросы кликовой атаки принимает боевая сессия.
 */

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/CombatEnemy.h"
#include "Combat/CombatGameMode.h"
#include "Combat/CombatSession.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

/** @brief Запускается системой автоматизации в игровом клиенте и проверяет контракт приёма клика. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatClickCommandTest, "Furward.Combat.ClickCommandValidation",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

/**
 * @brief Ждёт появления противника в загруженном L_Combat и проверяет запросы к сессии.
 *
 * Test принимает результаты проверок, Deadline ограничивает ожидание готовой сцены.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FVerifyCombatClickCommands,
	FAutomationTestBase*, Test, double, Deadline);

bool FVerifyCombatClickCommands::Update()
{
	// Берём сессию, которую GameMode создал при обычном запуске карты.
	UWorld* World = AutomationCommon::GetAnyGameWorld();
	ACombatGameMode* GameMode = World ? World->GetAuthGameMode<ACombatGameMode>() : nullptr;
	UCombatSession* Session = GameMode ? GameMode->GetCombatSession() : nullptr;
	const ACombatEnemy* Target = Session ? Session->GetActiveEnemy() : nullptr;
	// Стриминг заканчивается позже загрузки карты. Ждём активную цель до срока и затем сообщаем об ошибке.
	if (!Target)
	{
		if (FPlatformTime::Seconds() < Deadline)
		{
			return false;
		}
		Test->AddError(TEXT("L_Combat did not prepare an active enemy."));
		return true;
	}

	// Проверяем оба допустимых исхода для текущей цели и исключаем Ignore и пустую цель.
	Test->TestTrue(TEXT("Ready accepts Hit for the active enemy"),
		Session->TrySubmitClickAttack(ECombatClickResult::Hit, Target));
	Test->TestTrue(TEXT("Ready accepts Miss for the active enemy"),
		Session->TrySubmitClickAttack(ECombatClickResult::Miss, Target));
	Test->TestFalse(TEXT("Ignore is not a combat command"),
		Session->TrySubmitClickAttack(ECombatClickResult::Ignore, Target));
	Test->TestFalse(TEXT("A missing target is rejected"),
		Session->TrySubmitClickAttack(ECombatClickResult::Hit, nullptr));

	// Второй актор действителен и живёт в том же мире. Запрос к нему должен отклоняться, пока сессия отслеживает первую цель.
	ACombatEnemy* OtherEnemy = World->SpawnActor<ACombatEnemy>();
	if (Test->TestNotNull(TEXT("A second, valid enemy exists"), OtherEnemy))
	{
		Test->TestFalse(TEXT("Hit for a different enemy is rejected"),
			Session->TrySubmitClickAttack(ECombatClickResult::Hit, OtherEnemy));
		Test->TestFalse(TEXT("Miss for a different enemy is rejected"),
			Session->TrySubmitClickAttack(ECombatClickResult::Miss, OtherEnemy));
		// Убираем созданного тестом противника перед завершением проверки.
		OtherEnemy->Destroy();
	}

	// Новая сессия ещё не подготовила сцену. Даже передача действительного актора не разрешает запрос.
	UCombatSession* PreparingSession = NewObject<UCombatSession>(GameMode);
	Test->TestFalse(TEXT("Preparing rejects combat commands"),
		PreparingSession->TrySubmitClickAttack(ECombatClickResult::Hit, Target));

	return true;
}

bool FCombatClickCommandTest::RunTest(const FString& Parameters)
{
	// Запускаем ту же карту, на которой боевой режим подгружает окружение и создаёт участников.
	if (!AutomationOpenMap(TEXT("/Game/Combat-System/Maps/L_Combat")))
	{
		AddError(TEXT("Could not open L_Combat."));
		return false;
	}

	// Даём стримингу время создать цель. Остальные проверки выполнит отложенная команда автоматизации.
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyCombatClickCommands(this, FPlatformTime::Seconds() + 15.0));
	return true;
}

#endif

