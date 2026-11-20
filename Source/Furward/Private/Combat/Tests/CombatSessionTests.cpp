// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Проверяет инициализацию участников на L_Combat и допуск запросов кликовой атаки.
 */

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/CombatAttributesComponent.h"
#include "Combat/CombatEnemy.h"
#include "Combat/CombatGameMode.h"
#include "Combat/CombatHero.h"
#include "Combat/CombatSession.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
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

	ACombatHero* Hero = Cast<ACombatHero>(UGameplayStatics::GetActorOfClass(World, ACombatHero::StaticClass()));
	if (!Test->TestNotNull(TEXT("The combat hero was spawned"), Hero))
	{
		return true;
	}

	// Имитируем два независимых источника готовых характеристик.
	const FCombatBaseAttributes HeroBase{120.0f, 22.0f, 7.0f};
	const FCombatBaseAttributes EnemyBase{85.0f, 12.0f, 4.0f};
	UCombatAttributesComponent& HeroAttributes = Hero->GetAttributes();
	UCombatAttributesComponent& EnemyAttributes = Target->GetAttributes();
	Test->TestFalse(TEXT("Spawn alone does not authorize Hit"),
		Session->TrySubmitClickAttack(ECombatClickResult::Hit, Target));
	Test->TestFalse(TEXT("Invalid enemy attributes prevent initialization"),
		Session->InitializeParticipantAttributes(HeroBase, 70.0f, FCombatBaseAttributes{}));
	Test->TestFalse(TEXT("Invalid enemy attributes leave the hero uninitialized"), HeroAttributes.IsHealthInitialized());
	Test->TestFalse(TEXT("Invalid enemy attributes leave the enemy uninitialized"), EnemyAttributes.IsHealthInitialized());

	// Первое уведомление уже видит заполненные характеристики обоих участников.
	const FDelegateHandle HeroHealthHandle = HeroAttributes.OnHealthChanged().AddLambda(
		[Test = Test, &EnemyAttributes](float Previous, float Current)
		{
			Test->TestTrue(TEXT("Both participants are initialized before the first health event"),
				EnemyAttributes.IsHealthInitialized());
		});
	const bool bPrepared = Session->InitializeParticipantAttributes(HeroBase, 70.0f, EnemyBase);
	HeroAttributes.OnHealthChanged().Remove(HeroHealthHandle);
	if (!Test->TestTrue(TEXT("Valid attributes bring the session to Ready"), bPrepared))
	{
		return true;
	}
	Test->TestEqual(TEXT("Hero starts at the supplied current health"), HeroAttributes.GetCurrentHealth(), 70.0f);
	Test->TestEqual(TEXT("Hero retains the supplied damage"), HeroAttributes.GetBaseAttributes().DamagePerHit, HeroBase.DamagePerHit);
	Test->TestEqual(TEXT("New enemy starts at its own full health"), EnemyAttributes.GetCurrentHealth(), EnemyBase.MaxHealth);
	Test->TestEqual(TEXT("Enemy retains the supplied armor"), EnemyAttributes.GetBaseAttributes().Armor, EnemyBase.Armor);
	Test->TestFalse(TEXT("A prepared session does not overwrite its attributes"),
		Session->InitializeParticipantAttributes(HeroBase, HeroBase.MaxHealth, EnemyBase));

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

