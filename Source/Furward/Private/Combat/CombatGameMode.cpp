// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Связывает начало и завершение L_Combat с жизненным циклом сессии.
 */

#include "CombatGameMode.h"
#include "CombatRulesDefinition.h"
#include "CombatSession.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Profile/FurwardProfileSubsystem.h"
#include "Save/FurwardSaveSubsystem.h"
#include "UObject/UObjectGlobals.h"



DEFINE_LOG_CATEGORY_STATIC(LogCombatGameMode, Log, All);

void ACombatGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) {
	Super::InitGame(MapName, Options, ErrorMessage);

	// Создаём сессию с GameMode в качестве Outer и удерживаем её в отражаемом поле.
	CombatSession = NewObject <UCombatSession> (this);
	// Класс героя нужен сессии, когда окружение появится в игровом мире.
	CombatSession->Initialize(HeroClass);
}

UCombatSession* ACombatGameMode::GetCombatSession()
{
	return CombatSession.Get();
}

void ACombatGameMode::BeginPlay() {
	Super::BeginPlay();

	UE_LOG(LogCombatGameMode, Display, TEXT("BeginPlay: %s"), *GetName());

#if WITH_EDITOR
	// Play In Editor должен быть выбран и как тип мира, и как режим проверки в Class Defaults.
	if (bEnablePIECombatPreview && GetWorld()->WorldType == EWorldType::PIE)
	{
		if (!PIEProfileSlot.IsNone() && !TryOpenCombatProfile(PIEProfileSlot))
		{
			return;
		}
		CombatSession->EnablePIECombatPreview(CombatRules.LoadSynchronous(), PIEProfileSlot);
	}
#elif UE_BUILD_DEVELOPMENT
	const FName DevelopmentSlot(TEXT("FurwardCombatDevelopment"));
	if (!TryOpenCombatProfile(DevelopmentSlot))
	{
		return;
	}

	UCombatRulesDefinition* Rules = CombatRules.LoadSynchronous();
	if (IsValid(Rules) && CombatSession->ConfigureCombatStart(Rules, Rules->DevelopmentHeroBaseAttributes,
		Rules->DevelopmentHeroStartingHealth, DevelopmentSlot))
	{
		UE_LOG(LogCombatGameMode, Display, TEXT("Development combat starting attributes configured: %s"), *Rules->GetName());
	}
	else
	{
		UE_LOG(LogCombatGameMode, Warning, TEXT("Development combat requires valid starting hero attributes in Combat Rules."));
	}
#endif

	// Сессия получает строку этапа. Мир для подгрузки она берёт у GameMode.
	CombatSession->RequestInitialStage(InitialStage);
}

bool ACombatGameMode::TryOpenCombatProfile(FName Slot)
{
	UGameInstance* GameInstance = GetGameInstance();
	UFurwardSaveSubsystem* Save = UGameInstance::GetSubsystem<UFurwardSaveSubsystem>(GameInstance);
	UFurwardProfileSubsystem* Profile = UGameInstance::GetSubsystem<UFurwardProfileSubsystem>(GameInstance);
	if (Save && Profile && Save->TryOpenProfileSlot(Slot, *Profile))
	{
		return true;
	}

	UE_LOG(LogCombatGameMode, Error, TEXT("Combat profile slot unavailable: %s; combat remains in Preparing."), *Slot.ToString());
	return false;
}

void ACombatGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	UE_LOG( LogCombatGameMode, Display, TEXT("EndPlay: %s"), *GetName() );

	// Сессия блокирует команды и снимает подписку, пока GameMode ещё удерживает её.
	if ( IsValid(CombatSession.Get()) )
		CombatSession->Shutdown();

	// После Shutdown() отпускаем сессию и передаём движку исходную причину завершения.
	CombatSession = nullptr;

	Super::EndPlay(EndPlayReason);
}