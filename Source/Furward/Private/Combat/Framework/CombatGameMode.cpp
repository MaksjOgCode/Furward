// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Связывает начало и завершение L_Combat с жизненным циклом сессии.
 */

#include "Combat/Framework/CombatGameMode.h"
#include "Combat/Data/CombatRulesDefinition.h"
#include "Combat/Session/CombatSession.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Profile/FurwardProfileSubsystem.h"
#include "Save/FurwardSaveSubsystem.h"
#include "UObject/UObjectGlobals.h"



DEFINE_LOG_CATEGORY_STATIC(LogCombatGameMode, Log, All);

void ACombatGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) {
	Super::InitGame(MapName, Options, ErrorMessage);

	const FString RequestedSlot = UGameplayStatics::ParseOption(Options, TEXT("ProfileSlot"));
	if (!RequestedSlot.IsEmpty())
	{
		ProfileSlot = FName(*RequestedSlot);
	}

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

	UCombatRulesDefinition* Rules = CombatRules.LoadSynchronous();
	if (!IsValid(Rules))
	{
		UE_LOG(LogCombatGameMode, Error, TEXT("Could not load the assigned combat rules."));
		CombatSession->ReportStartupFailure(ECombatNextStageState::InvalidConfiguration);
		return;
	}
	if (!TryOpenCombatProfile(ProfileSlot))
	{
		CombatSession->ReportStartupFailure(ECombatNextStageState::ProfileUnavailable);
		return;
	}
	if (!CombatSession->ConfigureCombatStart(*Rules, ProfileSlot))
	{
		UE_LOG(LogCombatGameMode, Error, TEXT("Invalid hero attributes or encounter start rule in %s."), *Rules->GetName());
		CombatSession->ReportStartupFailure(ECombatNextStageState::InvalidConfiguration);
		return;
	}

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