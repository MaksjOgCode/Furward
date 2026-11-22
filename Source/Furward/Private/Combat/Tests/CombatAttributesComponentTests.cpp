// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Проверяет расчёт характеристик противников, инициализацию здоровья, урон, лечение и однократное событие смерти.
 */

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/CombatAttributesComponent.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

#include <limits>

/** @brief Проверяет контракты здоровья на отдельном экземпляре компонента. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAttributesHealthTest, "Furward.Combat.Attributes.Health",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCombatAttributesHealthTest::RunTest(const FString& Parameters)
{
	UCombatAttributesComponent* Attributes = NewObject<UCombatAttributesComponent>();

	// Проверяем исходное состояние и отказ на некорректных входных данных.
	const float NaN = std::numeric_limits<float>::quiet_NaN();
	const FCombatBaseAttributes Base{100.0f, 20.0f, 5.0f};
	TestFalse(TEXT("Health starts uninitialized"), Attributes->IsHealthInitialized());
	TestFalse(TEXT("An uninitialized participant is not dead"), Attributes->IsDead());
	TestEqual(TEXT("Damage before initialization removes no health"), Attributes->ApplyDamage(25.0f), 0.0f);
	TestFalse(TEXT("Maximum health must be positive"), Attributes->InitializeAttributes(FCombatBaseAttributes{}, 20.0f));
	TestFalse(TEXT("Starting health must be positive"), Attributes->InitializeAttributes(Base, 0.0f));
	TestFalse(TEXT("Non-finite maximum is rejected"), Attributes->InitializeAttributes(FCombatBaseAttributes{NaN, 20.0f, 5.0f}, 20.0f));

	// Слушатели позволяют проверить как значения, так и порядок событий одного удара.
	TArray<TPair<float, float>> HealthChanges;
	int32 DeathCount = 0;
	const FDelegateHandle HealthHandle = Attributes->OnHealthChanged().AddLambda(
		[&HealthChanges](float Previous, float Current)
		{
			HealthChanges.Emplace(Previous, Current);
		});
	const FDelegateHandle DeathHandle = Attributes->OnDeath().AddLambda(
		[this, &DeathCount, &HealthChanges](const UCombatAttributesComponent& Source)
		{
			++DeathCount;
			TestTrue(TEXT("Death observers see the dead state"), Source.IsDead());
			TestEqual(TEXT("The health event precedes death"), HealthChanges.Num(), 3);
		});

	// Числа здесь служат только входными данными теста. Проверяем ограничение максимальным и нулевым здоровьем.
	TestTrue(TEXT("Valid data initializes health"), Attributes->InitializeAttributes(Base, 125.0f));
	TestTrue(TEXT("Maximum health marks initialization"), Attributes->IsHealthInitialized());
	TestEqual(TEXT("Maximum health is saved"), Attributes->GetMaxHealth(), 100.0f);
	TestEqual(TEXT("Base damage is saved"), Attributes->GetBaseAttributes().DamagePerHit, Base.DamagePerHit);
	TestEqual(TEXT("Base armor is saved"), Attributes->GetBaseAttributes().Armor, Base.Armor);
	TestEqual(TEXT("Starting health is clamped to the maximum"), Attributes->GetCurrentHealth(), 100.0f);
	TestEqual(TEXT("A nonlethal hit removes its amount"), Attributes->ApplyDamage(25.0f), 25.0f);
	TestEqual(TEXT("Negative damage is rejected"), Attributes->ApplyDamage(-2.0f), 0.0f);
	TestEqual(TEXT("Non-finite damage is rejected"), Attributes->ApplyDamage(NaN), 0.0f);
	TestEqual(TEXT("Excess damage removes only the remaining health"), Attributes->ApplyDamage(150.0f), 75.0f);
	TestEqual(TEXT("Lethal damage stops at zero"), Attributes->GetCurrentHealth(), 0.0f);
	TestTrue(TEXT("Zero health marks death"), Attributes->IsDead());
	TestEqual(TEXT("A second hit on the dead participant removes nothing"), Attributes->ApplyDamage(10.0f), 0.0f);
	TestFalse(TEXT("Initialization cannot revive the same instance"), Attributes->InitializeAttributes(Base, 100.0f));

	// В списке остаются инициализация и два эффективных попадания.
	TestEqual(TEXT("Death is sent once"), DeathCount, 1);
	if (TestEqual(TEXT("Only effective changes are published"), HealthChanges.Num(), 3))
	{
		TestEqual(TEXT("Initialization reports the starting health"), HealthChanges[0].Value, 100.0f);
		TestEqual(TEXT("Nonlethal hit reports its previous health"), HealthChanges[1].Key, 100.0f);
		TestEqual(TEXT("Nonlethal hit reports its new health"), HealthChanges[1].Value, 75.0f);
		TestEqual(TEXT("Lethal hit reports zero health"), HealthChanges[2].Value, 0.0f);
	}

	Attributes->OnHealthChanged().Remove(HealthHandle);
	Attributes->OnDeath().Remove(DeathHandle);
	return true;
}

/** @brief Проверяет фактический прирост HP и события лечения. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAttributesHealingTest, "Furward.Combat.Attributes.Healing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCombatAttributesHealingTest::RunTest(const FString& Parameters)
{
	UCombatAttributesComponent* Attributes = NewObject<UCombatAttributesComponent>();
	TestEqual(TEXT("Uninitialized health cannot be healed"), Attributes->ApplyHealing(25.0f), 0.0f);
	TestTrue(TEXT("Health initialized"), Attributes->InitializeAttributes(FCombatBaseAttributes{100.0f, 20.0f, 5.0f}, 90.0f));

	TArray<TPair<float, float>> HealthChanges;
	const FDelegateHandle Handle = Attributes->OnHealthChanged().AddLambda(
		[&HealthChanges](float Previous, float Current)
		{
			HealthChanges.Emplace(Previous, Current);
		});

	TestEqual(TEXT("Negative healing is rejected"), Attributes->ApplyHealing(-25.0f), 0.0f);
	TestEqual(TEXT("Non-finite healing is rejected"),
		Attributes->ApplyHealing(std::numeric_limits<float>::infinity()), 0.0f);
	TestEqual(TEXT("Healing is limited to missing HP"), Attributes->ApplyHealing(25.0f), 10.0f);
	TestEqual(TEXT("Healing at full health has no effect"), Attributes->ApplyHealing(25.0f), 0.0f);
	if (TestEqual(TEXT("Only effective healing is published"), HealthChanges.Num(), 1))
	{
		TestEqual(TEXT("Healing publishes previous health"), HealthChanges[0].Key, 90.0f);
		TestEqual(TEXT("Healing publishes maximum health"), HealthChanges[0].Value, 100.0f);
	}

	Attributes->ApplyDamage(100.0f);
	TestEqual(TEXT("Healing cannot revive a dead participant"), Attributes->ApplyHealing(25.0f), 0.0f);
	TestTrue(TEXT("Participant remains dead"), Attributes->IsDead());
	TestEqual(TEXT("Failed revival does not publish health"), HealthChanges.Num(), 2);
	Attributes->OnHealthChanged().Remove(Handle);
	return true;
}

/** @brief Проверяет баланс вида от строки этапа и отказ на недопустимых данных. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatEnemyBalanceTest, "Furward.Combat.Attributes.EnemyBalance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCombatEnemyBalanceTest::RunTest(const FString& Parameters)
{
	const FCombatBaseAttributes ExpectedHero{80.0f, 20.0f, 8.0f};
	FCombatEnemyAttributeMultipliers Enemy{0.5f, 0.75f, 0.625f};
	FCombatBaseAttributes Result;

	TestTrue(TEXT("Valid stage and species produce the enemy attributes"), Enemy.TryApplyTo(ExpectedHero, Result));
	TestEqual(TEXT("Health scales from the stage maximum"), Result.MaxHealth, 40.0f);
	TestEqual(TEXT("Damage scales from the stage damage"), Result.DamagePerHit, 15.0f);
	TestEqual(TEXT("Armor scales from the stage armor"), Result.Armor, 5.0f);
	TestFalse(TEXT("An unset stage maximum is rejected"), Enemy.TryApplyTo(FCombatBaseAttributes{}, Result));

	Enemy.ArmorMultiplier = 0.0f;
	TestTrue(TEXT("Zero armor is a supported instance value"), Enemy.TryApplyTo(ExpectedHero, Result));
	TestEqual(TEXT("Zero armor multiplier produces zero armor"), Result.Armor, 0.0f);

	Enemy.DamageMultiplier = -1.0f;
	TestFalse(TEXT("A negative multiplier is invalid even against zero base damage"),
		Enemy.TryApplyTo(FCombatBaseAttributes{80.0f, 0.0f, 8.0f}, Result));
	TestEqual(TEXT("A rejected calculation preserves the output"), Result.DamagePerHit, 15.0f);

	const volatile float RuntimeMultiplier = std::numeric_limits<float>::max();
	Enemy = FCombatEnemyAttributeMultipliers{RuntimeMultiplier, 1.0f, 1.0f};
	TestFalse(TEXT("Overflow in the result is rejected"), Enemy.TryApplyTo(ExpectedHero, Result));
	return true;
}

#endif
