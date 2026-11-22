// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Проверяет в L_Combat атаки, смену противника, варианты старта и исход поражения.
 */

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/CombatAttributesComponent.h"
#include "Combat/CombatEnemy.h"
#include "Combat/CombatEnemyDefinition.h"
#include "Combat/CombatGameMode.h"
#include "Combat/CombatHero.h"
#include "Combat/CombatRulesDefinition.h"
#include "Combat/CombatSession.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

/** @brief Проверяет допуск кликов, завершение боя и активацию следующего столкновения. */
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

	const FGuid FirstEncounterID = Session->GetEncounterID();
	Test->TestTrue(TEXT("The first enemy has an encounter ID"), FirstEncounterID.IsValid());
	Test->TestFalse(TEXT("An unfinished encounter has no result"), Session->GetEncounterResult().IsSet());

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

	// Оба окна блокируют одну сессию. Команда чужого окна не возобновляет бой.
	Test->TestTrue(TEXT("Ready can open the pause menu"), Session->SetPauseMenuOpen(true));
	Test->TestFalse(TEXT("Pause prevents opening exit confirmation"), Session->SetLeaveConfirmationOpen(true));
	Test->TestFalse(TEXT("Pause rejects the current enemy hit"), Session->TrySubmitClickAttack(ECombatClickResult::Hit, Target));
	Test->TestTrue(TEXT("Closing an absent exit confirmation is harmless"), Session->SetLeaveConfirmationOpen(false));
	Test->TestEqual(TEXT("Closing an absent exit confirmation preserves pause"), Session->ApplyResolvedDamageToEnemy(Target, 10.0f), 0.0f);
	Test->TestTrue(TEXT("Pause menu can close"), Session->SetPauseMenuOpen(false));
	Test->TestTrue(TEXT("Ready can open exit confirmation"), Session->SetLeaveConfirmationOpen(true));
	Test->TestFalse(TEXT("Exit confirmation prevents opening the pause menu"), Session->SetPauseMenuOpen(true));
	Test->TestTrue(TEXT("Exit confirmation can close"), Session->SetLeaveConfirmationOpen(false));
	Test->TestTrue(TEXT("Both overlays preserve the Ready phase"), Session->GetPhase() == ECombatPhase::Ready);

	// Проверяем оба допустимых исхода для текущей цели и исключаем Ignore и пустую цель.
	Test->TestTrue(TEXT("Ready accepts Hit for the active enemy"),
		Session->TrySubmitClickAttack(ECombatClickResult::Hit, Target));
	Test->TestTrue(TEXT("Ready accepts Miss for the active enemy"),
		Session->TrySubmitClickAttack(ECombatClickResult::Miss, Target));
	Test->TestEqual(TEXT("Miss leaves the enemy at its starting health"), EnemyAttributes.GetCurrentHealth(), EnemyBase.MaxHealth);
	Test->TestTrue(TEXT("Miss leaves the combat phase Ready"), Session->GetPhase() == ECombatPhase::Ready);
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
		Test->TestEqual(TEXT("Resolved damage is rejected for a different enemy"),
			Session->ApplyResolvedDamageToEnemy(OtherEnemy, 25.0f), 0.0f);
		// Убираем созданного тестом противника перед завершением проверки.
		OtherEnemy->Destroy();
	}

	// Новая сессия ещё не подготовила сцену. Даже передача действительного актора не разрешает запрос.
	UCombatSession* PreparingSession = NewObject<UCombatSession>(GameMode);
	Test->TestFalse(TEXT("Preparing rejects combat commands"),
		PreparingSession->TrySubmitClickAttack(ECombatClickResult::Hit, Target));
	Test->TestEqual(TEXT("Preparing rejects resolved damage"),
		PreparingSession->ApplyResolvedDamageToEnemy(Target, 25.0f), 0.0f);

	// Неэффективный удар не запускает бой. Подписчики результативного удара сразу видят Fighting.
	Test->TestEqual(TEXT("Zero resolved damage is rejected"),
		Session->ApplyResolvedDamageToEnemy(Target, 0.0f), 0.0f);
	Test->TestTrue(TEXT("Zero damage leaves combat Ready"), Session->GetPhase() == ECombatPhase::Ready);
	int32 HealthEventCount = 0;
	const FDelegateHandle DamageHandle = EnemyAttributes.OnHealthChanged().AddLambda(
		[Test = Test, Session, &HealthEventCount](float, float)
		{
			++HealthEventCount;
			Test->TestTrue(TEXT("Health observers see the Fighting phase"),
				Session->GetPhase() == ECombatPhase::Fighting);
		});
	int32 DeathEventCount = 0;
	const FDelegateHandle DeathHandle = EnemyAttributes.OnDeath().AddLambda(
		[&DeathEventCount](const UCombatAttributesComponent&) { ++DeathEventCount; });

	Test->TestEqual(TEXT("First positive damage is applied"),
		Session->ApplyResolvedDamageToEnemy(Target, 25.0f), 25.0f);
	Test->TestTrue(TEXT("First positive damage starts combat"), Session->GetPhase() == ECombatPhase::Fighting);
	Test->TestEqual(TEXT("Enemy loses the applied HP"), EnemyAttributes.GetCurrentHealth(), 60.0f);
	Test->TestFalse(TEXT("Nonlethal damage leaves the encounter without a result"), Session->GetEncounterResult().IsSet());
	Test->TestTrue(TEXT("Fighting accepts further clicks"),
		Session->TrySubmitClickAttack(ECombatClickResult::Hit, Target));

	// Смертельный удар закрывает обе области цели. Повторная команда не меняет результат боя.
	Test->TestEqual(TEXT("Lethal damage stops at the remaining health"),
		Session->ApplyResolvedDamageToEnemy(Target, 200.0f), 60.0f);
	Test->TestTrue(TEXT("Enemy death advances combat to ChangingEnemy"),
		Session->GetPhase() == ECombatPhase::ChangingEnemy);
	Test->TestTrue(TEXT("Dead enemy is no longer a click target"), Session->GetActiveEnemy() == nullptr);
	Test->TestFalse(TEXT("Dead enemy hit area is disabled"), Target->GetHitArea().IsQueryCollisionEnabled());
	Test->TestFalse(TEXT("Dead enemy miss area is disabled"), Target->GetMissArea().IsQueryCollisionEnabled());
	const TOptional<FCombatEncounterResult> FirstResult = Session->GetEncounterResult();
	Test->TestTrue(TEXT("Enemy death records a result for its encounter"),
		FirstResult.IsSet() && FirstResult.GetValue().EncounterID == FirstEncounterID);
	Test->TestEqual(TEXT("Dead targets reject further damage"),
		Session->ApplyResolvedDamageToEnemy(Target, 10.0f), 0.0f);
	Test->TestFalse(TEXT("Dead targets reject further clicks"),
		Session->TrySubmitClickAttack(ECombatClickResult::Hit, Target));
	Test->TestEqual(TEXT("Two health changes are published"), HealthEventCount, 2);
	Test->TestEqual(TEXT("Death is published once"), DeathEventCount, 1);
	const TOptional<FCombatEncounterResult> ResultAfterRetry = Session->GetEncounterResult();
	Test->TestTrue(TEXT("Repeated damage and clicks preserve the original result"),
		FirstResult.IsSet() && ResultAfterRetry.IsSet() &&
		ResultAfterRetry.GetValue().EncounterID == FirstResult.GetValue().EncounterID &&
		ResultAfterRetry.GetValue().EnemyDefinitionID == FirstResult.GetValue().EnemyDefinitionID);
	EnemyAttributes.OnHealthChanged().Remove(DamageHandle);
	EnemyAttributes.OnDeath().Remove(DeathHandle);

	// Повторно используем описание вида, чтобы проверить замену экземпляра в той же арене.
	const UCombatEnemyDefinition* NextDefinition = LoadObject<UCombatEnemyDefinition>(nullptr,
		TEXT("/Game/Combat-System/Data/DA_Combat_Enemy_Mutant.DA_Combat_Enemy_Mutant"));
	if (!Test->TestNotNull(TEXT("The mutant definition is available for the next encounter"), NextDefinition))
	{
		return true;
	}

	Test->TestTrue(TEXT("The result identifies the defeated mutant's Data Asset"),
		FirstResult.IsSet() && FirstResult.GetValue().EnemyDefinitionID.IsValid() &&
		FirstResult.GetValue().EnemyDefinitionID == NextDefinition->GetPrimaryAssetId());

	const FTransform PreviousTransform = Target->GetActorTransform();
	Test->TestFalse(TEXT("Incomplete attributes cannot replace the defeated enemy"),
		Session->PrepareNextEnemy(*NextDefinition, FCombatBaseAttributes{}));
	TArray<AActor*> Enemies;
	UGameplayStatics::GetAllActorsOfClass(World, ACombatEnemy::StaticClass(), Enemies);
	Test->TestTrue(TEXT("Failed preparation keeps only the defeated enemy"),
		Enemies.Num() == 1 && Enemies[0] == Target);
	const TOptional<FCombatEncounterResult> ResultAfterFailedReplacement = Session->GetEncounterResult();
	Test->TestTrue(TEXT("Failed preparation preserves the defeat result"),
		ResultAfterFailedReplacement.IsSet() && ResultAfterFailedReplacement.GetValue().EncounterID == FirstEncounterID);

	if (!Test->TestTrue(TEXT("Valid data prepares the next enemy"),
		Session->PrepareNextEnemy(*NextDefinition, EnemyBase)))
	{
		return true;
	}

	UGameplayStatics::GetAllActorsOfClass(World, ACombatEnemy::StaticClass(), Enemies);
	if (!Test->TestEqual(TEXT("Exactly one enemy remains after replacement"), Enemies.Num(), 1))
	{
		return true;
	}

	const ACombatEnemy* NextEnemy = CastChecked<ACombatEnemy>(Enemies[0]);
	const FGuid NextEncounterID = Session->GetEncounterID();
	Test->TestTrue(TEXT("Every encounter obtains a distinct ID, even for the same enemy definition"),
		NextEncounterID.IsValid() && NextEncounterID != FirstEncounterID);
	Test->TestFalse(TEXT("The new encounter starts with no result"), Session->GetEncounterResult().IsSet());
	Test->TestTrue(TEXT("The next enemy is a new instance at the same arena spawn point"),
		NextEnemy != Target && NextEnemy->GetActorTransform().Equals(PreviousTransform));
	Test->TestTrue(TEXT("The next enemy belongs to the persistent level"),
		NextEnemy->GetLevel() == World->PersistentLevel.Get());
	Test->TestEqual(TEXT("The next enemy starts at full health"),
		NextEnemy->GetAttributes().GetCurrentHealth(), EnemyBase.MaxHealth);
	Test->TestEqual(TEXT("Replacing the enemy preserves the hero's current HP"), HeroAttributes.GetCurrentHealth(), 70.0f);
	Test->TestTrue(TEXT("Preparing the next enemy keeps the session in ChangingEnemy"),
		Session->GetPhase() == ECombatPhase::ChangingEnemy);
	Test->TestTrue(TEXT("The next enemy stays unavailable to cursor tracing"),
		Session->GetActiveEnemy() == nullptr);
	Test->TestEqual(TEXT("The next enemy cannot take damage before the phase transition"),
		Session->ApplyResolvedDamageToEnemy(NextEnemy, 10.0f), 0.0f);
	Test->TestFalse(TEXT("Repeated preparation cannot replace a living enemy"),
		Session->PrepareNextEnemy(*NextDefinition, EnemyBase));

	// Создаём два явно настроенных варианта для проверки переходов в одной сессии.
	UCombatRulesDefinition* Rules = NewObject<UCombatRulesDefinition>();
	Test->TestFalse(TEXT("An unspecified rule keeps the replacement waiting"),
		Session->ActivatePreparedEncounter(NextEncounterID, *Rules));
	Test->TestTrue(TEXT("The phase stays ChangingEnemy while no rule was selected"),
		Session->GetPhase() == ECombatPhase::ChangingEnemy);

	Rules->NextEncounterStart = ECombatNextEncounterStartRule::OnFirstDamage;
	Test->TestFalse(TEXT("The defeated encounter cannot activate its replacement"),
		Session->ActivatePreparedEncounter(FirstEncounterID, *Rules));
	if (!Test->TestTrue(TEXT("A configured first-damage rule activates the replacement"),
		Session->ActivatePreparedEncounter(NextEncounterID, *Rules)))
	{
		return true;
	}

	Test->TestTrue(TEXT("The first-damage rule opens attacks in Ready"),
		Session->GetPhase() == ECombatPhase::Ready && Session->GetActiveEnemy() == NextEnemy);
	Test->TestTrue(TEXT("The replacement accepts a miss in Ready"),
		Session->TrySubmitClickAttack(ECombatClickResult::Miss, NextEnemy));
	Test->TestTrue(TEXT("A miss keeps the replacement in Ready"), Session->GetPhase() == ECombatPhase::Ready);
	Test->TestEqual(TEXT("The next positive hit applies damage"),
		Session->ApplyResolvedDamageToEnemy(NextEnemy, 10.0f), 10.0f);
	Test->TestTrue(TEXT("First damage starts the new fight"), Session->GetPhase() == ECombatPhase::Fighting);

	Session->ApplyResolvedDamageToEnemy(NextEnemy, EnemyBase.MaxHealth);
	Rules->NextEncounterStart = ECombatNextEncounterStartRule::Automatic;
	Test->TestFalse(TEXT("Automatic start needs a prepared replacement"),
		Session->ActivatePreparedEncounter(NextEncounterID, *Rules));
	if (!Test->TestTrue(TEXT("A third enemy can be prepared for automatic start"),
		Session->PrepareNextEnemy(*NextDefinition, EnemyBase)))
	{
		return true;
	}
	const FGuid AutomaticEncounterID = Session->GetEncounterID();
	Test->TestTrue(TEXT("An explicit automatic rule starts the prepared fight"),
		Session->ActivatePreparedEncounter(AutomaticEncounterID, *Rules));
	const ACombatEnemy* AutomaticEnemy = Session->GetActiveEnemy();
	Test->TestTrue(TEXT("Automatic start enters Fighting before any damage"),
		Session->GetPhase() == ECombatPhase::Fighting && AutomaticEnemy &&
		AutomaticEnemy->GetAttributes().GetCurrentHealth() == EnemyBase.MaxHealth);
	Test->TestFalse(TEXT("The same encounter cannot be activated twice"),
		Session->ActivatePreparedEncounter(AutomaticEncounterID, *Rules));

	// Отдельно доводим HP героя до нуля. Снимок должен зафиксировать именно остаток HP противника.
	Test->TestFalse(TEXT("A living hero has no defeat snapshot"), Session->GetDefeatSnapshot().IsSet());
	Test->TestEqual(TEXT("Damage before defeat reduces the enemy's current health"),
		Session->ApplyResolvedDamageToEnemy(AutomaticEnemy, 10.0f), 10.0f);
	int32 DefeatEvents = 0;
	const FDelegateHandle DefeatHandle = Session->OnDefeatRecorded().AddLambda([&]()
	{
		++DefeatEvents;
		const TOptional<FCombatDefeatSnapshot> Snapshot = Session->GetDefeatSnapshot();
		Test->TestTrue(TEXT("Defeat listeners see the finished phase and enemy snapshot"),
			Session->GetPhase() == ECombatPhase::Defeated && Snapshot.IsSet() &&
			Snapshot->EnemyCurrentHealth == 75.0f && Snapshot->EnemyMaxHealth == 85.0f);
	});
	Test->TestEqual(TEXT("Lethal damage to the hero consumes the remaining health"),
		HeroAttributes.ApplyDamage(100.0f), 70.0f);
	Test->TestTrue(TEXT("Hero death stops the fight in Defeated"),
		Session->GetPhase() == ECombatPhase::Defeated);
	const TOptional<FCombatDefeatSnapshot> Defeat = Session->GetDefeatSnapshot();
	Test->TestTrue(TEXT("Defeat preserves the current and maximum enemy HP"),
		Defeat.IsSet() && Defeat.GetValue().EnemyCurrentHealth == 75.0f && Defeat.GetValue().EnemyMaxHealth == 85.0f);
	Test->TestTrue(TEXT("Defeat hides the enemy from click targeting"), Session->GetActiveEnemy() == nullptr);
	Test->TestFalse(TEXT("No attack is accepted after defeat"),
		Session->TrySubmitClickAttack(ECombatClickResult::Hit, AutomaticEnemy));
	Test->TestFalse(TEXT("Defeat prevents a second exit confirmation from opening"),
		Session->SetLeaveConfirmationOpen(true));
	HeroAttributes.ApplyDamage(1.0f);
	Test->TestEqual(TEXT("Defeat notifies listeners exactly once"), DefeatEvents, 1);
	Session->OnDefeatRecorded().Remove(DefeatHandle);

	return true;
}

bool FCombatClickCommandTest::RunTest(const FString& Parameters)
{
	// Перезагружаем L_Combat, чтобы получить новую сессию после предыдущего теста этой карты.
	if (!AutomationOpenMap(TEXT("/Game/Combat-System/Maps/L_Combat"), true))
	{
		AddError(TEXT("Could not open L_Combat."));
		return false;
	}

	// Даём стримингу время создать цель. Остальные проверки выполнит отложенная команда автоматизации.
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyCombatClickCommands(this, FPlatformTime::Seconds() + 15.0));
	return true;
}

#endif

