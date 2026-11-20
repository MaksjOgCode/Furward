// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Проверяет инициализацию здоровья, фактический урон и однократное событие смерти.
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
	TestFalse(TEXT("Health starts uninitialized"), Attributes->IsHealthInitialized());
	TestFalse(TEXT("An uninitialized participant is not dead"), Attributes->IsDead());
	TestEqual(TEXT("Damage before initialization removes no health"), Attributes->ApplyDamage(25.0f), 0.0f);
	TestFalse(TEXT("Maximum health must be positive"), Attributes->InitializeHealth(0.0f, 20.0f));
	TestFalse(TEXT("Starting health must be positive"), Attributes->InitializeHealth(100.0f, 0.0f));
	TestFalse(TEXT("Non-finite maximum is rejected"), Attributes->InitializeHealth(NaN, 20.0f));

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
	TestTrue(TEXT("Valid data initializes health"), Attributes->InitializeHealth(100.0f, 125.0f));
	TestTrue(TEXT("Maximum health marks initialization"), Attributes->IsHealthInitialized());
	TestEqual(TEXT("Maximum health is saved"), Attributes->GetMaxHealth(), 100.0f);
	TestEqual(TEXT("Starting health is clamped to the maximum"), Attributes->GetCurrentHealth(), 100.0f);
	TestEqual(TEXT("A nonlethal hit removes its amount"), Attributes->ApplyDamage(25.0f), 25.0f);
	TestEqual(TEXT("Negative damage is rejected"), Attributes->ApplyDamage(-2.0f), 0.0f);
	TestEqual(TEXT("Non-finite damage is rejected"), Attributes->ApplyDamage(NaN), 0.0f);
	TestEqual(TEXT("Excess damage removes only the remaining health"), Attributes->ApplyDamage(150.0f), 75.0f);
	TestEqual(TEXT("Lethal damage stops at zero"), Attributes->GetCurrentHealth(), 0.0f);
	TestTrue(TEXT("Zero health marks death"), Attributes->IsDead());
	TestEqual(TEXT("A second hit on the dead participant removes nothing"), Attributes->ApplyDamage(10.0f), 0.0f);
	TestFalse(TEXT("Initialization cannot revive the same instance"), Attributes->InitializeHealth(100.0f, 100.0f));

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

#endif
