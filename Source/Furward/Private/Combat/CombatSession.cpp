// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Готовит столкновения, сохраняет победы и ждёт актуальной записи перед сменой этапа или выходом.
 */

#include "Combat/CombatSession.h"
#include "Combat/Abilities/CombatAbilityComponent.h"
#include "Combat/Abilities/CombatAbilityDefinition.h"
#include "Combat/CombatArena.h"
#include "Combat/CombatAttributesComponent.h"
#include "Combat/CombatEnemy.h"
#include "Combat/CombatEnemyDefinition.h"
#include "Combat/CombatEnvironmentDefinition.h"
#include "Combat/CombatHero.h"
#include "Combat/CombatPlayerController.h"
#include "Combat/CombatRulesDefinition.h"
#include "Combat/CombatStageRow.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CoreDelegates.h"
#include "Profile/FurwardProfileSubsystem.h"
#include "Rewards/FurwardLootDefinition.h"
#include "Save/FurwardSaveSubsystem.h"
#include "TimerManager.h"



DEFINE_LOG_CATEGORY_STATIC( LogCombatSession, Log, All );



#if WITH_EDITOR
void UCombatSession::EnablePIECombatPreview(UCombatRulesDefinition* InRules, FName OpenedSlot)
{
	bPIECombatPreview = true;
	if (!IsValid(InRules) || !ConfigureCombatStart(InRules, InRules->PIEHeroBaseAttributes,
		InRules->PIEHeroStartingHealth, OpenedSlot))
	{
		UE_LOG(LogCombatSession, Warning, TEXT("PIE combat requires valid starting hero attributes in Combat Rules."));
	}
}
#endif

bool UCombatSession::RequestNextStage(FName SaveSlot)
{
	const UWorld* World = GetOuter()->GetWorld();
	if (!IsValid(CombatRules.Get()) || !World || World->IsPaused() || IsCombatInputSuspended())
	{
		return false;
	}

	if (Phase == ECombatPhase::Preparing && NextStageState == ECombatNextStageState::EnvironmentFailed)
	{
		StartEnvironmentPreparation();
		return true;
	}

	const UFurwardProfileSubsystem* Profile = GetProfile();
	if (Phase != ECombatPhase::StageCompleted || !Profile || !Profile->IsExpeditionStageCompleted(ActiveStageID))
	{
		return false;
	}

	FName NextStageID;
	return FindNextStage(NextStageID) &&
		BeginProgressSave(SaveSlot.IsNone() ? ProgressSlot : SaveSlot, EProgressSaveAction::NextStage, NextStageID);
}

bool UCombatSession::RequestEnterFarm()
{
	if (!CanEnterFarm())
	{
		return false;
	}

	const UCombatEnemyDefinition& Definition = *InitialEnemy;
	FCombatBaseAttributes EnemyBase;
	if (!ResolveEnemyAttributes(Definition, EnemyBase))
	{
		return false;
	}

	// Спаун и удаление актора могут вызвать Blueprint-события в этом же стеке.
	SetCombatPhase(ECombatPhase::Recovering);
	if (!ReplaceCurrentEnemy(Definition, EnemyBase))
	{
		SetCombatPhase(ECombatPhase::Defeated);
		return false;
	}

	Mode = ECombatMode::Farm;
	DefeatSnapshot.Reset();
	UE_LOG(LogCombatSession, Display, TEXT("Combat mode: Farm; Phase: Recovering"));
	Hero->GetAttributes().RestoreFullHealth();

	SetCombatPhase(ECombatPhase::Preparing);
	UE_LOG(LogCombatSession, Display, TEXT("Combat phase: Preparing; Mode: Farm"));
	EnterStageReady();
	Enemy->GetAttributes().PublishInitialHealth();
	ProgressionChanged.Broadcast();
	return true;
}

bool UCombatSession::RequestReturnToExpedition()
{
	if (!CanReturnToExpedition())
	{
		return false;
	}

	const UFurwardProfileSubsystem* Profile = GetProfile();
	const bool bBossPending = Profile->IsExpeditionClearComplete(ActiveStageID);
	const UCombatEnemyDefinition* Definition = SelectEnemyDefinition(bBossPending);
	FCombatBaseAttributes EnemyBase;
	if (!Definition || !ResolveEnemyAttributes(*Definition, EnemyBase))
	{
		return false;
	}

	// До успешной замены старый таймер и состояние столкновения остаются доступны для отката.
	const ECombatPhase PreviousPhase = Phase;
	SetCombatPhase(ECombatPhase::Preparing);
	if (!ReplaceCurrentEnemy(*Definition, EnemyBase))
	{
		SetCombatPhase(PreviousPhase);
		return false;
	}

	StopEnemyAttacks();
	Hero->GetAttributes().RestoreFullHealth();
	Mode = ECombatMode::Expedition;
	UE_LOG(LogCombatSession, Display, TEXT("Combat mode: Expedition; Stage: %s; Clear: %d/%d; Target: %s"),
		*ActiveStageID.ToString(), Profile->GetExpeditionClearCount(ActiveStageID),
		UFurwardProfileSubsystem::RequiredExpeditionClears, bBossPending ? TEXT("Boss") : TEXT("Ordinary"));
	EnterStageReady();
	Enemy->GetAttributes().PublishInitialHealth();
	ProgressionChanged.Broadcast();
	return true;
}

bool UCombatSession::RequestExitCombat(FName SaveSlot)
{
	return !DeferredOutcomeEncounterID.IsValid() && ActiveOverlay != ECombatOverlay::PauseMenu &&
		Phase != ECombatPhase::Recovering &&
		!(Mode == ECombatMode::Farm && Phase == ECombatPhase::Preparing) &&
		BeginProgressSave(SaveSlot.IsNone() ? ProgressSlot : SaveSlot, EProgressSaveAction::Exit);
}

bool UCombatSession::BeginProgressSave(FName Slot, EProgressSaveAction Action, FName NextStageID)
{
	const bool bSavePending = PendingSaveAction != EProgressSaveAction::None;
	const bool bExit = Action == EProgressSaveAction::Exit;
	if (!IsValid(CombatRules.Get()) || bShutdown || Slot.IsNone() || (!ProgressSlot.IsNone() && ProgressSlot != Slot) ||
		(bSavePending && (!bExit || PendingSaveAction != EProgressSaveAction::NextStage)))
	{
		return false;
	}

	const UFurwardProfileSubsystem* Profile = GetProfile();
	UFurwardSaveSubsystem* Save = GetSaveSubsystem();
	if (!Profile || !Save || (ProgressSlot.IsNone() &&
		UGameplayStatics::DoesSaveGameExist(Slot.ToString(), UFurwardSaveSubsystem::LocalUserIndex)))
	{
		return false;
	}

	FFurwardProfileSnapshot Snapshot = Profile->CreateSnapshot();
	const int64 Revision = Snapshot.Revision;
	if (!bSavePending)
	{
		if (!Save->RequestSave(Slot, MoveTemp(Snapshot)))
		{
			return false;
		}
		ProgressSaveSubsystem = Save;
		ProgressSaveListener = Save->OnSaveFinished().AddUObject(this, &UCombatSession::HandleProgressSaveFinished);
	}

	ProgressSlot = Slot;
	PendingSaveAction = Action;
	PendingNextStageID = NextStageID;
	if (bExit)
	{
		SetCombatPhase(ECombatPhase::Leaving);
		StopEnemyAttacks();
		UE_LOG(LogCombatSession, Display, TEXT("Combat phase: Leaving; Awaiting save revision: %lld"), Revision);
		ProgressionChanged.Broadcast();
	}
	else
	{
		UE_LOG(LogCombatSession, Display, TEXT("Stage transition awaiting save: %s -> %s; Revision: %lld"),
			*ActiveStageID.ToString(), *NextStageID.ToString(), Revision);
		SetNextStageState(ECombatNextStageState::Saving);
	}

	// Сверяем статус сразу на случай уже записанной ревизии или синхронной ошибки запуска записи.
	CheckPendingProgressSave();
	return true;
}

const FCombatStageRow* UCombatSession::FindNextStage(FName& NextStageID) const
{
	if (!ActiveStageTable || !ActiveArena.IsValid() || !IsValid(EnvironmentStreaming.Get()) ||
		!EnvironmentStreaming->IsLevelVisible())
	{
		return nullptr;
	}

	const FCombatStageRow* Current = ActiveStageTable->FindRow<FCombatStageRow>(ActiveStageID, TEXT("Next combat stage"));
	if (!Current || Current->NextStage.IsNone() || Current->NextStage == ActiveStageID)
	{
		return nullptr;
	}

	const FCombatStageRow* Next = ActiveStageTable->FindRow<FCombatStageRow>(Current->NextStage, TEXT("Next combat stage"));
	if (!Next || !IsValid(Next->Environment.Get()) || Next->Environment->GetEnvironmentMap().IsNull() ||
		Next->EnemyRoster.IsEmpty() || !IsValid(Next->EnemyRoster[0].Get()))
	{
		UE_LOG(LogCombatSession, Warning, TEXT("Next combat stage requires a configured enemy and environment map."));
		return nullptr;
	}

	FCombatBaseAttributes EnemyBase;
	if (!Next->EnemyRoster[0]->GetAttributeMultipliers().TryApplyTo(Next->ExpectedHeroAttributes, EnemyBase))
	{
		UE_LOG(LogCombatSession, Warning, TEXT("Invalid enemy balance for next stage %s; check Expected Hero Attributes and Attribute Multipliers."),
			*Current->NextStage.ToString());
		return nullptr;
	}

	NextStageID = Current->NextStage;
	return Next;
}

void UCombatSession::HandleProgressSaveFinished(FName Slot, int64 /*WrittenRevision*/, bool /*bSuccess*/)
{
	if (Slot == ProgressSlot && PendingSaveAction != EProgressSaveAction::None)
	{
		CheckPendingProgressSave();
	}
}

void UCombatSession::CheckPendingProgressSave()
{
	UFurwardSaveSubsystem* Save = ProgressSaveSubsystem.Get();
	const UFurwardProfileSubsystem* Profile = GetProfile();
	if (!Save || !Profile)
	{
		const bool bNextStage = PendingSaveAction == EProgressSaveAction::NextStage;
		ResetPendingProgressSave();
		if (bNextStage)
		{
			SetNextStageState(ECombatNextStageState::SaveFailed);
		}
		return;
	}

	const FFurwardSaveStatus Status = Save->GetSaveStatus(ProgressSlot);
	const bool bExit = PendingSaveAction == EProgressSaveAction::Exit;
	FFurwardProfileSnapshot Snapshot = Profile->CreateSnapshot();
	if (Status.IsLatestRevisionSaved(Snapshot.Revision))
	{
		const FName NextStageID = PendingNextStageID;
		ResetPendingProgressSave();
		if (bExit)
		{
			UE_LOG(LogCombatSession, Display, TEXT("Combat exit save confirmed: Revision %lld"), Status.LastSavedRevision);
			FinishCombatExit();
		}
		else
		{
			UE_LOG(LogCombatSession, Display, TEXT("Stage transition save confirmed: %s; Revision: %lld"),
				*NextStageID.ToString(), Status.LastSavedRevision);
			if (!BeginNextStage(NextStageID))
			{
				UE_LOG(LogCombatSession, Warning, TEXT("Stage transition could not start: %s"), *NextStageID.ToString());
				SetNextStageState(ECombatNextStageState::StartFailed);
			}
		}
	}
	else if (Status.bLatestWriteFailed || (Snapshot.Revision > Status.LatestRequestedRevision &&
		!Save->RequestSave(ProgressSlot, MoveTemp(Snapshot))))
	{
		ResetPendingProgressSave();
		if (bExit)
		{
			UE_LOG(LogCombatSession, Warning, TEXT("Combat exit save failed; request the exit again to retry."));
			ExitSaveFailed.Broadcast();
		}
		else
		{
			UE_LOG(LogCombatSession, Warning, TEXT("Stage transition save failed; request the next stage again to retry."));
			SetNextStageState(ECombatNextStageState::SaveFailed);
		}
	}
}

void UCombatSession::FinishCombatExit()
{
	UWorld* World = GetOuter()->GetWorld();
	ACombatPlayerController* Controller = World ? World->GetFirstPlayerController<ACombatPlayerController>() : nullptr;
	Shutdown();
	if (IsValid(Controller))
	{
		Controller->NotifyCombatExitReady();
	}
}

void UCombatSession::ResetPendingProgressSave()
{
	if (UFurwardSaveSubsystem* Save = ProgressSaveSubsystem.Get())
	{
		Save->OnSaveFinished().Remove(ProgressSaveListener);
	}
	ProgressSaveListener.Reset();
	ProgressSaveSubsystem.Reset();
	PendingSaveAction = EProgressSaveAction::None;
	PendingNextStageID = NAME_None;
}

void UCombatSession::SetNextStageState(ECombatNextStageState State)
{
	NextStageState = State;
	ProgressionChanged.Broadcast();
}

bool UCombatSession::BeginNextStage(FName StageID)
{
	FName NextStageID;
	const FCombatStageRow* Next = FindNextStage(NextStageID);
	ACombatArena* Arena = ActiveArena.Get();
	UWorld* World = Arena ? Arena->GetWorld() : nullptr;
	if (!Next || NextStageID != StageID || !World || World->IsPaused())
	{
		return false;
	}

	const FCombatStageRow* Current = ActiveStageTable->FindRow<FCombatStageRow>(ActiveStageID, TEXT("Active environment"));
	const bool bKeepArena = Current && IsValid(Current->Environment.Get()) &&
		Current->Environment->GetEnvironmentMap() == Next->Environment->GetEnvironmentMap();

	SetCombatPhase(ECombatPhase::Preparing);
	if (!ClearStageEnemy())
	{
		SetCombatPhase(ECombatPhase::StageCompleted);
		return false;
	}
	StopEnemyAttacks();
	ActiveStageID = NextStageID;
	InitialEnemy = Next->EnemyRoster[0];
	BossEnemy = Next->Boss;
	UE_LOG(LogCombatSession, Display, TEXT("Combat phase: Preparing; Stage: %s"), *ActiveStageID.ToString());

	if (bKeepArena)
	{
		SetNextStageState(ECombatNextStageState::Unavailable);
		PrepareStageParticipants(*World, *Arena);
		if (Phase == ECombatPhase::Preparing)
		{
			FailEnvironmentPreparation();
		}
	}
	else
	{
		StartEnvironmentPreparation();
	}
	return true;
}

bool UCombatSession::ClearStageEnemy()
{
	if (ACombatEnemy* PreviousEnemy = Enemy.Get())
	{
		UCombatAttributesComponent& Attributes = PreviousEnemy->GetAttributes();
		if (!PreviousEnemy->Destroy())
		{
			return false;
		}
		Attributes.OnDeath().RemoveAll(this);
		Attributes.OnHealthChanged().RemoveAll(this);
	}
	Enemy.Reset();
	EncounterID.Invalidate();
	EnemyDefinitionID = FPrimaryAssetId();
	EnemyLoot = nullptr;
	EncounterResult.Reset();
	bCurrentEnemyIsBoss = false;
	ParticipantHealthChanged.Broadcast();
	return true;
}

UFurwardSaveSubsystem* UCombatSession::GetSaveSubsystem() const
{
	const UWorld* World = GetOuter()->GetWorld();
	return World ? UGameInstance::GetSubsystem<UFurwardSaveSubsystem>(World->GetGameInstance()) : nullptr;
}

void UCombatSession::QueueProgressAutosave(const UFurwardProfileSubsystem& Profile)
{
	if (ProgressSlot.IsNone())
	{
		return;
	}

	UFurwardSaveSubsystem* Save = GetSaveSubsystem();
	const int64 Revision = Profile.GetRevision();
	if (Save && Save->RequestSave(ProgressSlot, Profile.CreateSnapshot()))
	{
		UE_LOG(LogCombatSession, Display, TEXT("Combat autosave queued: %s; Revision: %lld"), *ProgressSlot.ToString(), Revision);
		return;
	}
	UE_LOG(LogCombatSession, Warning, TEXT("Combat autosave request rejected: %s; Revision: %lld"), *ProgressSlot.ToString(), Revision);
}

void UCombatSession::ContinueAfterOrdinaryVictory()
{
	const UCombatRulesDefinition* Rules = CombatRules.Get();
	if (Phase != ECombatPhase::ChangingEnemy || !EncounterResult.IsSet() || !IsValid(Rules))
	{
		return;
	}

	// Сначала применяем валюты и прогресс. Замена цели запускается после принятия результата.
	UFurwardProfileSubsystem* Profile = GetProfile();
	if (!Profile || !ApplyEncounterResult(*Profile))
	{
		return;
	}

	const bool bExpedition = Mode == ECombatMode::Expedition;
	if (bExpedition)
	{
		const int32 ClearedEnemies = Profile->GetExpeditionClearCount(ActiveStageID);
		UE_LOG(LogCombatSession, Display, TEXT("Expedition clear: %s; %d/%d"),
			*ActiveStageID.ToString(), ClearedEnemies, UFurwardProfileSubsystem::RequiredExpeditionClears);
		ProgressionChanged.Broadcast();
	}
	else
	{
		UE_LOG(LogCombatSession, Display, TEXT("Farm encounter won: %s"), *ActiveStageID.ToString());
	}
	QueueProgressAutosave(*Profile);

	const bool bBossPending = bExpedition && Profile->IsExpeditionClearComplete(ActiveStageID);
	const UCombatEnemyDefinition* NextDefinition = SelectEnemyDefinition(bBossPending);
	const ACombatEnemy* DefeatedEnemy = Enemy.Get();
	if (!DefeatedEnemy || !NextDefinition)
	{
		return;
	}

	const ECombatNextEncounterStartRule StartRule = Rules->NextEncounterStart;
	if (StartRule != ECombatNextEncounterStartRule::OnFirstDamage && StartRule != ECombatNextEncounterStartRule::Automatic)
	{
		return;
	}

	const UWorld* World = DefeatedEnemy->GetWorld();
	if (!World || World->IsPaused())
	{
		return;
	}

	FCombatBaseAttributes EnemyBase;
	if (!ResolveEnemyAttributes(*NextDefinition, EnemyBase))
	{
		return;
	}
	if (!PrepareNextEnemy(*NextDefinition, EnemyBase) || !ActivatePreparedEncounter(EncounterID, *Rules))
	{
		UE_LOG(LogCombatSession, Warning, TEXT("Combat could not activate the next encounter."));
		return;
	}

	if (bBossPending)
	{
		UE_LOG(LogCombatSession, Display, TEXT("Expedition boss activated: %s"), *ActiveStageID.ToString());
	}
}

const UCombatEnemyDefinition* UCombatSession::SelectEnemyDefinition(bool bBossPending) const
{
	if (!bBossPending)
	{
		return InitialEnemy.Get();
	}

	const UCombatEnemyDefinition* Definition = BossEnemy.Get();
	if (!IsValid(Definition) || Definition == InitialEnemy.Get())
	{
		UE_LOG(LogCombatSession, Warning, TEXT("Combat stage %s requires a distinct boss definition."),
			*ActiveStageID.ToString());
		return nullptr;
	}

	return Definition;
}

void UCombatSession::StartEnemyAttacks()
{
	const UCombatRulesDefinition* Rules = CombatRules.Get();
	const ACombatEnemy* CurrentEnemy = Enemy.Get();
	if (!IsValid(Rules) || !CanUseCurrentEnemy(CurrentEnemy))
	{
		return;
	}

	const float FirstDelay = Rules->FirstEnemyAttackDelaySeconds;
	const float Interval = Rules->EnemyAttackIntervalSeconds;
	if (!FMath::IsFinite(FirstDelay) || FirstDelay < 0.0f ||
		!FMath::IsFinite(Interval) || Interval <= 0.0f)
	{
		return;
	}

	// Идентификатор связывает callback с тем столкновением, для которого создан таймер.
	CurrentEnemy->GetWorld()->GetTimerManager().SetTimer(EnemyAttackTimer,
		FTimerDelegate::CreateUObject(this, &UCombatSession::HandleEnemyAttack, EncounterID),
		Interval, true, FirstDelay);
	UE_LOG(LogCombatSession, Display, TEXT("Enemy attacks scheduled: first %.2fs; interval %.2fs"), FirstDelay, Interval);
}

void UCombatSession::StopEnemyAttacks()
{
	// Outer сессии — GameMode. Он даёт доступ к миру даже при потере ссылки на противника.
	if (UWorld* World = GetOuter()->GetWorld())
	{
		World->GetTimerManager().ClearTimer(EnemyAttackTimer);
	}
	EnemyAttackTimer.Invalidate();
}

void UCombatSession::HandleEnemyAttack(FGuid AttackingEncounterID)
{
	const ACombatEnemy* CurrentEnemy = Enemy.Get();
	if (AttackingEncounterID != EncounterID || !CurrentEnemy || !Hero.IsValid())
	{
		return;
	}

	const float DamagePerHit = CurrentEnemy->GetAttributes().GetBaseAttributes().DamagePerHit;
	const float Armor = Hero->GetAttributes().GetBaseAttributes().Armor;
	ApplyResolvedDamageToHero(CurrentEnemy, FMath::Max(0.0f, DamagePerHit - Armor));
}

bool UCombatSession::ResolveEnemyAttributes(const UCombatEnemyDefinition& Definition, FCombatBaseAttributes& EnemyBase) const
{
	const FCombatStageRow* Stage = ActiveStageTable ? ActiveStageTable->FindRow<FCombatStageRow>(
		ActiveStageID, TEXT("Combat enemy balance"), false) : nullptr;
	if (!Stage || !Definition.GetAttributeMultipliers().TryApplyTo(Stage->ExpectedHeroAttributes, EnemyBase))
	{
		UE_LOG(LogCombatSession, Warning, TEXT("Invalid enemy balance: Stage %s; Enemy %s. Check Expected Hero Attributes and Attribute Multipliers."),
			*ActiveStageID.ToString(), *Definition.GetName());
		return false;
	}
	return true;
}

void UCombatSession::RequestInitialStage(const FDataTableRowHandle& StageHandle)
{
	if (Phase != ECombatPhase::Preparing || !ActiveStageID.IsNone())
	{
		return;
	}

	const FCombatStageRow* Stage = StageHandle.GetRow<FCombatStageRow>(TEXT("Initial combat stage"));
	if (!Stage || Stage->EnemyRoster.IsEmpty() || !IsValid(Stage->EnemyRoster[0].Get()))
	{
		UE_LOG(LogCombatSession, Error, TEXT("Initial combat stage %s requires a row and first enemy."), *StageHandle.RowName.ToString());
		return;
	}

	ActiveStageTable = StageHandle.DataTable;
	ActiveStageID = StageHandle.RowName;
	InitialEnemy = Stage->EnemyRoster[0];
	BossEnemy = Stage->Boss;
	UE_LOG(LogCombatSession, Display, TEXT("Initial combat stage: %s; Environment: %s; First enemy: %s"),
		*ActiveStageID.ToString(), *GetNameSafe(Stage->Environment.Get()), *InitialEnemy->GetName());
	StartEnvironmentPreparation();
}

void UCombatSession::StartEnvironmentPreparation()
{
	if (!ClearStageEnemy())
	{
		FailEnvironmentPreparation();
		return;
	}

	ReleaseEnvironment();
	if (UWorld* World = GetOuter()->GetWorld())
	{
		SetNextStageState(ECombatNextStageState::EnvironmentLoading);
		World->GetTimerManager().SetTimer(EnvironmentLoadTimer, this, &UCombatSession::UpdateEnvironmentPreparation, 0.1f, true);
		UpdateEnvironmentPreparation();
	}
	else
	{
		FailEnvironmentPreparation();
	}
}

void UCombatSession::UpdateEnvironmentPreparation()
{
	if (bShutdown || Phase != ECombatPhase::Preparing)
	{
		return;
	}

	if (IsValid(EnvironmentStreaming.Get()))
	{
		if (EnvironmentStreaming->GetLevelStreamingState() == ELevelStreamingState::FailedToLoad)
		{
			FailEnvironmentPreparation();
		}
		return;
	}

	// Ждём, пока старая арена исчезнет из мира, прежде чем показать новую.
	if (RetiringEnvironment.IsValid() && RetiringEnvironment->IsLevelVisible())
	{
		return;
	}
	RetiringEnvironment.Reset();

	const FCombatStageRow* Stage = ActiveStageTable ?
		ActiveStageTable->FindRow<FCombatStageRow>(ActiveStageID, TEXT("Combat environment")) : nullptr;
	if (!Stage || !IsValid(Stage->Environment.Get()) || Stage->Environment->GetEnvironmentMap().IsNull())
	{
		FailEnvironmentPreparation();
		return;
	}

	const TSoftObjectPtr<UWorld>& EnvironmentMap = Stage->Environment->GetEnvironmentMap();
	bool bRequested = false;
	ULevelStreamingDynamic* Streaming = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(
		GetOuter(), EnvironmentMap, FTransform::Identity, bRequested);
	if (!bRequested || !IsValid(Streaming))
	{
		UE_LOG(LogCombatSession, Error, TEXT("Could not request environment: %s"), *EnvironmentMap.ToSoftObjectPath().ToString());
		FailEnvironmentPreparation();
		return;
	}

	EnvironmentStreaming = Streaming;
	Streaming->OnLevelShown.AddDynamic(this, &UCombatSession::HandleEnvironmentShown);
	UE_LOG(LogCombatSession, Display, TEXT("Environment requested: %s"), *EnvironmentMap.ToSoftObjectPath().ToString());
}

void UCombatSession::FailEnvironmentPreparation()
{
	ReleaseEnvironment();
	SetNextStageState(ECombatNextStageState::EnvironmentFailed);
	UE_LOG(LogCombatSession, Error, TEXT("Combat environment preparation failed: %s; Retry is available."), *ActiveStageID.ToString());
}

void UCombatSession::ReleaseEnvironment()
{
	if (UWorld* World = GetOuter()->GetWorld())
	{
		World->GetTimerManager().ClearTimer(EnvironmentLoadTimer);
	}
	ActiveArena.Reset();
	if (IsValid(EnvironmentStreaming.Get()))
	{
		EnvironmentStreaming->OnLevelShown.RemoveDynamic(this, &UCombatSession::HandleEnvironmentShown);
		EnvironmentStreaming->SetIsRequestingUnloadAndRemoval(true);
		RetiringEnvironment = EnvironmentStreaming.Get();
	}
	EnvironmentStreaming = nullptr;
}

void UCombatSession::HandleEnvironmentShown()
{
    // Принимаем событие во время подготовки, пока подуровень видим и арена ещё не зарегистрирована.
    if (  Phase != ECombatPhase::Preparing        ||
         !IsValid(EnvironmentStreaming.Get())    ||
         !EnvironmentStreaming->IsLevelVisible() ||
          ActiveArena.IsValid() )
    {
        return;
    }

    UE_LOG( LogCombatSession, Display, TEXT("Environment shown: %s"), *EnvironmentStreaming->GetWorldAssetPackageName() );

    // Ищем арену в показанном подуровне, к которому относится уведомление.
    ULevel* Level = EnvironmentStreaming->GetLoadedLevel();
    if (!Level)
    {
        FailEnvironmentPreparation();
        return;
    }

    ACombatArena* FoundArena = nullptr;
    int32         ArenaCount = 0;

    // Считаем экземпляры ACombatArena и её наследников, которые ещё действительны.
    for (AActor* Actor : Level->Actors) {
		if (ACombatArena* Arena = Cast <ACombatArena>(Actor); IsValid(Arena) ) {
            FoundArena = Arena;
            ++ArenaCount;
        }
    }

    // Источник камеры и точек спауна должен быть однозначным. При другой расстановке пишем число арен и останавливаем подготовку.
    if (ArenaCount != 1) {
        UE_LOG( LogCombatSession, Error, TEXT("Expected one combat arena in the environment, found %d."), ArenaCount );
        FailEnvironmentPreparation();
        return;
    }

    UWorld* World = Level->GetWorld();
    ACombatPlayerController* Controller = World ? World->GetFirstPlayerController<ACombatPlayerController>() : nullptr;
    if (!IsValid(Controller) || !FoundArena->HasActiveCameraComponent())
    {
        UE_LOG(LogCombatSession, Error, TEXT("Combat environment requires a player controller and an active arena camera."));
        FailEnvironmentPreparation();
        return;
    }

    World->GetTimerManager().ClearTimer(EnvironmentLoadTimer);
    ActiveArena = FoundArena;
    UE_LOG(LogCombatSession, Display, TEXT("Arena registered: %s"), *FoundArena->GetName());

    // При смене арены переносим живой экземпляр героя, сохраняя его HP и способности.
    if (ACombatHero* CurrentHero = Hero.Get())
    {
        const FTransform Spawn = FoundArena->GetSpawnHeroTransform();
        CurrentHero->SetActorLocationAndRotation(Spawn.GetLocation(), Spawn.Rotator(), false, nullptr, ETeleportType::TeleportPhysics);
    }
    Controller->UseArenaCamera(FoundArena);
    SetNextStageState(ECombatNextStageState::Unavailable);
    PrepareStageParticipants(*World, *FoundArena);
    if (Phase == ECombatPhase::Preparing && InitialHeroAttributes.IsSet())
    {
        FailEnvironmentPreparation();
    }
}

void UCombatSession::PrepareStageParticipants(UWorld& World, const ACombatArena& Arena)
{
    const UCombatEnemyDefinition* StartingDefinition = InitialEnemy.Get();
    const UFurwardProfileSubsystem* Profile = IsValid(CombatRules.Get()) ? GetProfile() : nullptr;
    if (Profile)
    {
        UE_LOG(LogCombatSession, Display, TEXT("Expedition progress at entry: %s; %d/%d"),
            *ActiveStageID.ToString(), Profile->GetExpeditionClearCount(ActiveStageID),
            UFurwardProfileSubsystem::RequiredExpeditionClears);
    }
    if (Profile && Profile->IsExpeditionStageCompleted(ActiveStageID))
    {
        EnterStageCompleted();
        return;
    }

    if (Profile && Profile->IsExpeditionClearComplete(ActiveStageID))
    {
        StartingDefinition = SelectEnemyDefinition(true);
        if (!StartingDefinition)
        {
            return;
        }
        UE_LOG(LogCombatSession, Display, TEXT("Expedition resumes with boss at 10/10: %s"),
            *ActiveStageID.ToString());
    }

    // Создаём участников по точкам арены на постоянном уровне, когда выбран следующий бой.
    SpawnHero(World, Arena);
    if (IsValid(StartingDefinition))
    {
        if (ACombatEnemy* SpawnedEnemy = SpawnEnemy(World, Arena, *StartingDefinition))
        {
            RegisterEnemy(*SpawnedEnemy, *StartingDefinition);
        }
    }

    if (!Hero.IsValid() || !Enemy.IsValid())
    {
        return;
    }
    if (!InitialHeroAttributes.IsSet())
    {
        UE_LOG(LogCombatSession, Display, TEXT("Combat phase: Preparing; waiting for participant attributes."));
        return;
    }

    FCombatBaseAttributes EnemyBase;
    if (!ResolveEnemyAttributes(*StartingDefinition, EnemyBase))
    {
        return;
    }

    // На той же арене продолжение сохраняет живого героя и инициализирует только нового противника.
    const UCombatAttributesComponent& HeroAttributes = Hero->GetAttributes();
    if (HeroAttributes.IsHealthInitialized())
    {
        UCombatAttributesComponent& EnemyAttributes = Enemy->GetAttributes();
        if (!HeroAttributes.IsDead() && EnemyAttributes.CanInitializeAttributes(EnemyBase, EnemyBase.MaxHealth))
        {
            EnemyAttributes.SetInitialAttributes(EnemyBase, EnemyBase.MaxHealth);
            EnterStageReady();
            EnemyAttributes.PublishInitialHealth();
        }
        return;
    }

    const FInitialHeroAttributes& HeroStart = InitialHeroAttributes.GetValue();
    const bool bReady = InitializeParticipantAttributes(HeroStart.Base, HeroStart.CurrentHealth, EnemyBase);
#if WITH_EDITOR
    if (bReady && bPIECombatPreview)
    {
        UE_LOG(LogCombatSession, Display, TEXT("PIE combat preview enabled."));
    }
#else
    if (bReady)
    {
        UE_LOG(LogCombatSession, Display, TEXT("Game combat participants initialized."));
    }
#endif
}

void UCombatSession::SpawnHero(UWorld& World, const ACombatArena& Arena)
{
	// Сохраняем уже созданного героя при повторном вызове.
	if (Hero.IsValid())
	{
		return;
	}

	// Класс берём из настроек GameMode. При пустой настройке сообщаем об ошибке спауна.
	if (!HeroClass)
	{
		UE_LOG(LogCombatSession, Error, TEXT("Combat hero class is not configured."));
		return;
	}

	// Точка берётся с подуровня, а герой принадлежит L_Combat. Выгрузка окружения его не удаляет.
	FActorSpawnParameters SpawnParams;
	SpawnParams.OverrideLevel = World.PersistentLevel.Get();

	ACombatHero* SpawnedHero = World.SpawnActor<ACombatHero>(
		HeroClass.Get(), Arena.GetSpawnHeroTransform(), SpawnParams);

	// При сбое спауна Hero остаётся пустым и сцена не перейдёт в Ready.
	if (!IsValid(SpawnedHero))
	{
		UE_LOG(LogCombatSession, Error, TEXT("Could not spawn the combat hero."));
		return;
	}

	// Храним слабую ссылку. В логе указываем экземпляр и пакет уровня для проверки размещения.
	Hero = SpawnedHero;
	if (const UCombatRulesDefinition* Rules = CombatRules.Get(); IsValid(Rules))
	{
#if WITH_EDITOR
		const auto& Slots = bPIECombatPreview ? Rules->PIEAbilitySlots : Rules->DevelopmentAbilitySlots;
#else
		const auto& Slots = Rules->DevelopmentAbilitySlots;
#endif
		SpawnedHero->GetAbilities().ConfigureSlots(Slots);
	}
	UCombatAttributesComponent& HeroAttributes = SpawnedHero->GetAttributes();
	HeroAttributes.OnDeath().AddUObject(this, &UCombatSession::HandleHeroDeath);
	HeroAttributes.OnHealthChanged().AddUObject(this, &UCombatSession::HandleParticipantHealthChanged);

	UE_LOG(LogCombatSession, Display, TEXT("Hero spawned: %s in %s"),
		*SpawnedHero->GetName(), *SpawnedHero->GetLevel()->GetOutermost()->GetName());
}

ACombatEnemy* UCombatSession::SpawnEnemy(UWorld& World, const ACombatArena& Arena,
	const UCombatEnemyDefinition& Definition)
{
	if (!Definition.GetEnemyClass() || !IsValid(Definition.GetStaticMesh()))
	{
		UE_LOG(LogCombatSession, Error, TEXT("Enemy definition %s requires an enemy class and static mesh."),
			*Definition.GetName());
		return nullptr;
	}

	// Берём мировую точку арены и создаём актор в постоянном L_Combat.
	// Откладываем завершение спауна, чтобы применить настройки вида до запуска актора.
	const FTransform SpawnTransform = Arena.GetSpawnEnemyTransform();
	FActorSpawnParameters SpawnParams;
	SpawnParams.OverrideLevel = World.PersistentLevel.Get();
	SpawnParams.bDeferConstruction = true;

	ACombatEnemy* SpawnedEnemy = World.SpawnActor<ACombatEnemy>(
		Definition.GetEnemyClass().Get(), SpawnTransform, SpawnParams);

	if (!IsValid(SpawnedEnemy))
	{
		UE_LOG(LogCombatSession, Error, TEXT("Could not spawn the combat enemy."));
		return nullptr;
	}

	// Применяем модель и области клика до завершения спауна и BeginPlay актора.
	SpawnedEnemy->ApplyDefinition(Definition);
	SpawnedEnemy->FinishSpawning(SpawnTransform);
	if (!IsValid(SpawnedEnemy) || SpawnedEnemy->IsActorBeingDestroyed())
	{
		return nullptr;
	}

	UE_LOG(LogCombatSession, Display, TEXT("Enemy spawned: %s in %s using %s"),
		*SpawnedEnemy->GetName(), *SpawnedEnemy->GetLevel()->GetOutermost()->GetName(), *Definition.GetName());
	return SpawnedEnemy;
}

void UCombatSession::RegisterEnemy(ACombatEnemy& SpawnedEnemy, const UCombatEnemyDefinition& Definition)
{
	Enemy = &SpawnedEnemy;
	EncounterID = FGuid::NewGuid();
	EnemyDefinitionID = Definition.GetPrimaryAssetId();
	EnemyLoot = Definition.GetLootDefinition();
	bCurrentEnemyIsBoss = BossEnemy.Get() == &Definition && InitialEnemy.Get() != &Definition;
	EncounterResult.Reset();
	UCombatAttributesComponent& EnemyAttributes = SpawnedEnemy.GetAttributes();
	EnemyAttributes.OnDeath().AddUObject(this, &UCombatSession::HandleEnemyDeath);
	EnemyAttributes.OnHealthChanged().AddUObject(this, &UCombatSession::HandleParticipantHealthChanged);
	if (bCurrentEnemyIsBoss)
	{
		ProgressionChanged.Broadcast();
	}
}

void UCombatSession::Initialize(TSubclassOf<ACombatHero> InHeroClass) {
	// Сохраняем класс героя до появления арены. Outer в логе позволяет связать сессию с её GameMode.
	HeroClass = InHeroClass;
	UE_LOG( LogCombatSession, Log, TEXT("Initialize: %s; Outer: %s"), *GetName(), *GetOuter()->GetName() );
}

bool UCombatSession::ConfigureCombatStart(UCombatRulesDefinition* InRules, const FCombatBaseAttributes& HeroBase,
	float StartingHealth, FName OpenedSlot)
{
	if (Phase != ECombatPhase::Preparing || !IsValid(InRules) || !HeroBase.IsValid() ||
		!FMath::IsFinite(StartingHealth) || StartingHealth <= 0.0f)
	{
		return false;
	}

	CombatRules = InRules;
	InitialHeroAttributes = FInitialHeroAttributes{HeroBase, StartingHealth};
	ProgressSlot = OpenedSlot;
	CurrencyBalancesChanged.Broadcast();
	return true;
}

bool UCombatSession::CanEnterFarm() const
{
	const UCombatRulesDefinition* Rules = CombatRules.Get();
	const UWorld* World = GetOuter()->GetWorld();
	const ACombatHero* CurrentHero = Hero.Get();
	return Phase == ECombatPhase::Defeated && World && !World->IsPaused() &&
		CurrentHero && CurrentHero->GetAttributes().IsDead() && InitialEnemy && IsValid(Rules) &&
		Rules->FarmAccess == ECombatFarmAccessRule::AfterAnyDefeat &&
		Rules->FarmRecovery == ECombatFarmRecoveryRule::RestoreFullHealth;
}

bool UCombatSession::CanReturnToExpedition() const
{
	const UCombatRulesDefinition* Rules = CombatRules.Get();
	const UFurwardProfileSubsystem* Profile = GetProfile();
	return Mode == ECombatMode::Farm &&
		(Phase == ECombatPhase::Ready || Phase == ECombatPhase::Fighting) &&
		IsValid(Rules) && Rules->ExpeditionReturnHealth == ECombatFarmRecoveryRule::RestoreFullHealth &&
		Profile && !Profile->IsExpeditionStageCompleted(ActiveStageID) && CanUseCurrentEnemy(Enemy.Get());
}

const ACombatEnemy* UCombatSession::GetActiveEnemy() const
{
	if (Phase == ECombatPhase::ChangingEnemy || Phase == ECombatPhase::Defeated ||
		Phase == ECombatPhase::Recovering || Phase == ECombatPhase::StageCompleted)
	{
		return nullptr;
	}

	const ACombatEnemy* CurrentEnemy = Enemy.Get();
	return CurrentEnemy && !CurrentEnemy->GetAttributes().IsDead() ? CurrentEnemy : nullptr;
}

FCombatParticipantHealth UCombatSession::GetHeroHealthForDisplay() const
{
	const ACombatHero* CurrentHero = Hero.Get();
	if (CurrentHero)
	{
		const UCombatAttributesComponent& Attributes = CurrentHero->GetAttributes();
		return {Attributes.GetCurrentHealth(), Attributes.GetMaxHealth()};
	}
	return {};
}

FCombatParticipantHealth UCombatSession::GetEnemyHealthForDisplay() const
{
	const ACombatEnemy* CurrentEnemy = Enemy.Get();
	if (CurrentEnemy)
	{
		const UCombatAttributesComponent& Attributes = CurrentEnemy->GetAttributes();
		return {Attributes.GetCurrentHealth(), Attributes.GetMaxHealth()};
	}
	return {};
}

FCombatProgressionSnapshot UCombatSession::GetProgressionForDisplay() const
{
	const UFurwardProfileSubsystem* Profile = GetProfile();
	if (ActiveStageID.IsNone() || !Profile)
	{
		return {};
	}

	FCombatProgressionSnapshot Snapshot{ActiveStageID, Profile->GetExpeditionClearCount(ActiveStageID),
		UFurwardProfileSubsystem::RequiredExpeditionClears};
	Snapshot.NextStageState = NextStageState;
	if (Profile->IsExpeditionStageCompleted(ActiveStageID))
	{
		Snapshot.BossStatus = ECombatBossStatus::Defeated;
	}
	else if (Profile->IsExpeditionClearComplete(ActiveStageID))
	{
		Snapshot.BossStatus = bCurrentEnemyIsBoss && Enemy.IsValid() ? ECombatBossStatus::OnArena : ECombatBossStatus::Available;
	}
	return Snapshot;
}

int64 UCombatSession::GetCurrencyBalanceForDisplay(FName CurrencyID) const
{
	const UFurwardProfileSubsystem* Profile = GetProfile();
	return Profile ? Profile->GetCurrencyBalance(CurrencyID) : 0;
}

void UCombatSession::HandleParticipantHealthChanged(float /*Previous*/, float /*Current*/)
{
	ParticipantHealthChanged.Broadcast();
}

bool UCombatSession::InitializeParticipantAttributes(const FCombatBaseAttributes& HeroBase, float HeroCurrentHealth,
	const FCombatBaseAttributes& EnemyBase)
{
	// Принимаем данные после появления арены и обоих участников.
	if (Phase != ECombatPhase::Preparing || !Hero.IsValid() || !Enemy.IsValid() || !ActiveArena.IsValid() ||
		!IsValid(EnvironmentStreaming.Get()) || !EnvironmentStreaming->IsLevelVisible())
	{
		return false;
	}

	UCombatAttributesComponent& HeroAttributes = Hero->GetAttributes();
	UCombatAttributesComponent& EnemyAttributes = Enemy->GetAttributes();
	// Проверяем оба набора до записи. Герой получает переданное HP, новый противник начинает с полным HP.
	if (!HeroAttributes.CanInitializeAttributes(HeroBase, HeroCurrentHealth) ||
		!EnemyAttributes.CanInitializeAttributes(EnemyBase, EnemyBase.MaxHealth))
	{
		UE_LOG(LogCombatSession, Warning, TEXT("Participant attributes rejected; combat is still Preparing."));
		return false;
	}

	// Заполняем оба компонента и только затем сообщаем подписчикам о начальном здоровье.
	HeroAttributes.SetInitialAttributes(HeroBase, HeroCurrentHealth);
	EnemyAttributes.SetInitialAttributes(EnemyBase, EnemyBase.MaxHealth);
	EnterStageReady();
	HeroAttributes.PublishInitialHealth();
	EnemyAttributes.PublishInitialHealth();
	return true;
}

void UCombatSession::EnterStageReady()
{
	const UCombatAttributesComponent& HeroAttributes = Hero->GetAttributes();
	const UCombatAttributesComponent& EnemyAttributes = Enemy->GetAttributes();
	SetCombatPhase(ECombatPhase::Ready);
	UE_LOG(LogCombatSession, Display, TEXT("Combat phase: Ready; Hero HP %.2f/%.2f; Enemy HP %.2f/%.2f"),
		HeroAttributes.GetCurrentHealth(), HeroAttributes.GetMaxHealth(),
		EnemyAttributes.GetCurrentHealth(), EnemyAttributes.GetMaxHealth());
}

bool UCombatSession::PrepareNextEnemy(const UCombatEnemyDefinition& Definition, const FCombatBaseAttributes& EnemyBase)
{
	const ACombatEnemy* PreviousEnemy = Enemy.Get();
	const ACombatHero* CurrentHero = Hero.Get();
	if (Phase != ECombatPhase::ChangingEnemy || !EncounterResult.IsSet() || EncounterResult->bDefeatedBoss ||
		!PreviousEnemy || !PreviousEnemy->GetAttributes().IsDead() || !CurrentHero ||
		CurrentHero->GetAttributes().IsDead() || !ReplaceCurrentEnemy(Definition, EnemyBase))
	{
		return false;
	}

	Enemy->GetAttributes().PublishInitialHealth();
	return true;
}

bool UCombatSession::ReplaceCurrentEnemy(const UCombatEnemyDefinition& Definition, const FCombatBaseAttributes& EnemyBase)
{
	ACombatEnemy* PreviousEnemy = Enemy.Get();
	const ACombatArena* Arena = ActiveArena.Get();
	if (!PreviousEnemy || !Arena || !IsValid(EnvironmentStreaming.Get()) || !EnvironmentStreaming->IsLevelVisible())
	{
		return false;
	}

	UWorld* World = Arena->GetWorld();
	ACombatEnemy* NextEnemy = World ? SpawnEnemy(*World, *Arena, Definition) : nullptr;
	if (!NextEnemy)
	{
		return false;
	}

	// Проверяем замену до удаления прежней цели, чтобы при сбое сохранить текущее состояние.
	UCombatAttributesComponent& NextAttributes = NextEnemy->GetAttributes();
	if (!NextAttributes.CanInitializeAttributes(EnemyBase, EnemyBase.MaxHealth))
	{
		NextEnemy->Destroy();
		return false;
	}

	UCombatAttributesComponent& PreviousAttributes = PreviousEnemy->GetAttributes();
	if (!PreviousEnemy->Destroy())
	{
		NextEnemy->Destroy();
		return false;
	}
	// Destroy удаляет актора из мира, а его UObject и компоненты освобождает сборщик мусора.
	PreviousAttributes.OnDeath().RemoveAll(this);
	PreviousAttributes.OnHealthChanged().RemoveAll(this);
	NextAttributes.SetInitialAttributes(EnemyBase, EnemyBase.MaxHealth);
	RegisterEnemy(*NextEnemy, Definition);

	UE_LOG(LogCombatSession, Display, TEXT("Next enemy prepared: %s; HP: %.2f/%.2f"),
		*NextEnemy->GetName(), NextAttributes.GetCurrentHealth(), NextAttributes.GetMaxHealth());
	return true;
}

bool UCombatSession::ActivatePreparedEncounter(const FGuid& ExpectedEncounterID, const UCombatRulesDefinition& Rules)
{
	// Отложенный запрос действует только для той замены, для которой был получен EncounterID.
	if (Phase != ECombatPhase::ChangingEnemy || !ExpectedEncounterID.IsValid() ||
		ExpectedEncounterID != EncounterID || EncounterResult.IsSet() || !CanUseCurrentEnemy(Enemy.Get()))
	{
		return false;
	}

	switch (Rules.NextEncounterStart)
	{
	case ECombatNextEncounterStartRule::OnFirstDamage:
		SetCombatPhase(ECombatPhase::Ready);
		UE_LOG(LogCombatSession, Display, TEXT("Combat phase: Ready; Next encounter awaits damage."));
		return true;
	case ECombatNextEncounterStartRule::Automatic:
		EnterFighting();
		return true;
	default:
		return false;
	}
}

bool UCombatSession::TrySubmitClickAttack(ECombatClickResult Result, const ACombatEnemy* Target)
{
	// Допускаем только результаты, которые образуют команду. Ignore отбрасывается и при прямом вызове метода.
	if (Result != ECombatClickResult::Hit && Result != ECombatClickResult::Miss)
	{
		return false;
	}

	if (!CanAttackEnemy(Target))
	{
		return false;
	}

	UE_LOG(LogCombatSession, Display, TEXT("Click attack accepted: %s; Target: %s"),
		Result == ECombatClickResult::Hit ? TEXT("Hit") : TEXT("Miss"), *Target->GetName());

	if (Result == ECombatClickResult::Miss || !IsValid(CombatRules.Get()))
	{
		return true;
	}

	const float DamagePerHit = Hero->GetAttributes().GetBaseAttributes().DamagePerHit;
	const float Armor = Target->GetAttributes().GetBaseAttributes().Armor;
	ApplyResolvedDamageToEnemy(Target, FMath::Max(0.0f, DamagePerHit - Armor));
	return true;
}

float UCombatSession::ApplyResolvedDamageToEnemy(const ACombatEnemy* Target, float ResolvedDamage)
{
	// Проверяем актуальность результата непосредственно перед изменением HP.
	if (!CanAttackEnemy(Target))
	{
		return 0.0f;
	}

	UCombatAttributesComponent& Attributes = Target->GetAttributes();
	const float PreviousHealth = Attributes.GetCurrentHealth();
	const float AppliedDamage = Attributes.ReduceHealth(ResolvedDamage);
	if (AppliedDamage <= 0.0f)
	{
		return 0.0f;
	}

	// Для первого эффективного удара фиксируем Fighting до уведомлений, включая уведомление о смерти.
	if (Phase == ECombatPhase::Ready)
	{
		EnterFighting();
	}

	UE_LOG(LogCombatSession, Display, TEXT("Damage applied: %.2f; Target: %s; HP: %.2f/%.2f"),
		AppliedDamage, *Target->GetName(), Attributes.GetCurrentHealth(), Attributes.GetMaxHealth());
	Attributes.PublishDamage(PreviousHealth);
	// При немедленном исходе меняем цель после завершения всех уведомлений о смерти.
	ContinueAfterOrdinaryVictory();
	return AppliedDamage;
}

float UCombatSession::ApplyResolvedDamageToHero(const ACombatEnemy* Attacker, float ResolvedDamage)
{
	if (Phase != ECombatPhase::Fighting || !CanUseCurrentEnemy(Attacker, ECombatAttackSource::Enemy))
	{
		return 0.0f;
	}

	UCombatAttributesComponent& Attributes = Hero->GetAttributes();
	const float PreviousHealth = Attributes.GetCurrentHealth();
	const float AppliedDamage = Attributes.ReduceHealth(ResolvedDamage);
	if (AppliedDamage > 0.0f)
	{
		UE_LOG(LogCombatSession, Display, TEXT("Enemy attack applied: %.2f; Hero HP: %.2f/%.2f"),
			AppliedDamage, Attributes.GetCurrentHealth(), Attributes.GetMaxHealth());
		Attributes.PublishDamage(PreviousHealth);
	}
	return AppliedDamage;
}

void UCombatSession::SetCombatPhase(ECombatPhase NewPhase)
{
	Phase = NewPhase;
	RefreshCombatTime();
}

void UCombatSession::RefreshCombatTime()
{
	const bool bPaused = Phase != ECombatPhase::Fighting || IsCombatInputSuspended();
	if (ACombatHero* CurrentHero = Hero.Get())
	{
		CurrentHero->GetAbilities().SetCombatTimePaused(bPaused);
	}

	if (UWorld* World = GetOuter()->GetWorld(); World && EnemyAttackTimer.IsValid())
	{
		if (bPaused)
		{
			World->GetTimerManager().PauseTimer(EnemyAttackTimer);
		}
		else
		{
			World->GetTimerManager().UnPauseTimer(EnemyAttackTimer);
		}
	}
}

bool UCombatSession::CanUseAbilities() const
{
	const UCombatRulesDefinition* Rules = CombatRules.Get();
	return IsValid(Rules) && Rules->AbilityActivation == ECombatAbilityActivationRule::FightingOnly &&
		Phase == ECombatPhase::Fighting && !IsCombatInputSuspended() && CanUseCurrentEnemy(Enemy.Get());
}

bool UCombatSession::TryUseAbilitySlot(int32 SlotIndex)
{
	if (!CanUseAbilities())
	{
		UE_LOG(LogCombatSession, Display, TEXT("Ability rejected: combat phase or pause blocks activation."));
		return false;
	}

	return Hero->GetAbilities().TryActivateSlot(SlotIndex, *this);
}

void UCombatSession::SubmitAutoClick()
{
	if (Phase == ECombatPhase::Fighting && (CanUseAbilities() || IsEncounterOutcomePendingThisFrame()))
	{
		TrySubmitClickAttack(ECombatClickResult::Hit, Enemy.Get());
	}
}

TOptional<FCombatAbilitySlotSnapshot> UCombatSession::GetAbilitySlotForDisplay(int32 SlotIndex) const
{
	const ACombatHero* CurrentHero = Hero.Get();
	if (!CurrentHero)
	{
		return {};
	}

	const UCombatAbilityComponent& Abilities = CurrentHero->GetAbilities();
	const UCombatAbilityDefinition* Definition = Abilities.GetSlotDefinition(SlotIndex);
	if (!Definition)
	{
		return {};
	}

	const float CooldownRemaining = Abilities.GetSlotCooldownRemaining(SlotIndex);
	const float EffectRemaining = Abilities.GetSlotEffectRemaining(SlotIndex);
	return FCombatAbilitySlotSnapshot{Definition->DisplayName, CooldownRemaining, EffectRemaining,
		CooldownRemaining <= 0.0f && EffectRemaining <= 0.0f && CanUseAbilities()};
}

void UCombatSession::EnterFighting()
{
	SetCombatPhase(ECombatPhase::Fighting);
	UE_LOG(LogCombatSession, Display, TEXT("Combat phase: Fighting"));
	StartEnemyAttacks();
}

void UCombatSession::EnterStageCompleted()
{
	SetCombatPhase(ECombatPhase::StageCompleted);
	FName NextStageID;
	NextStageState = IsValid(CombatRules.Get()) && FindNextStage(NextStageID)
		? ECombatNextStageState::Available : ECombatNextStageState::Unavailable;
	ProgressionChanged.Broadcast();
	UE_LOG(LogCombatSession, Display, TEXT("Combat phase: StageCompleted; Stage: %s"), *ActiveStageID.ToString());
}

UFurwardProfileSubsystem* UCombatSession::GetProfile() const
{
	const UWorld* World = GetOuter()->GetWorld();
	return World ? UGameInstance::GetSubsystem<UFurwardProfileSubsystem>(World->GetGameInstance()) : nullptr;
}

void UCombatSession::HandleEnemyDeath(const UCombatAttributesComponent& Source)
{
	ACombatEnemy* CurrentEnemy = Enemy.Get();
	if (Phase != ECombatPhase::Fighting || EncounterResult.IsSet() || !CurrentEnemy ||
		&CurrentEnemy->GetAttributes() != &Source || !Source.IsDead())
	{
		return;
	}

	if (!DeferEncounterOutcome())
	{
		FinalizeEnemyVictory(*CurrentEnemy);
	}
}

bool UCombatSession::DeferEncounterOutcome()
{
	const UCombatRulesDefinition* Rules = CombatRules.Get();
	if (!IsValid(Rules) || Rules->SimultaneousDeath != ECombatSimultaneousDeathRule::DefeatWithVictoryCredit)
	{
		return false;
	}

	if (!DeferredOutcomeEncounterID.IsValid())
	{
		DeferredOutcomeEncounterID = EncounterID;
		DeferredOutcomeFrame = GFrameCounter;
		// В UE OnEndFrame следует за игровым вводом и TimerManager, до увеличения GFrameCounter.
		DeferredOutcomeListener = FCoreDelegates::OnEndFrame.AddUObject(this, &UCombatSession::ResolveDeferredEncounterOutcome);
	}
	return true;
}

bool UCombatSession::IsEncounterOutcomePendingThisFrame() const
{
	return DeferredOutcomeEncounterID.IsValid() && DeferredOutcomeEncounterID == EncounterID &&
		DeferredOutcomeFrame == GFrameCounter;
}

void UCombatSession::ResolveDeferredEncounterOutcome()
{
	const bool bCurrentOutcome = IsEncounterOutcomePendingThisFrame();
	ResetDeferredEncounterOutcome();
	ACombatHero* CurrentHero = Hero.Get();
	ACombatEnemy* CurrentEnemy = Enemy.Get();
	if (!bCurrentOutcome || bShutdown || Phase != ECombatPhase::Fighting || !CurrentHero || !CurrentEnemy)
	{
		return;
	}

	const bool bEnemyDied = CurrentEnemy->GetAttributes().IsDead();
	if (CurrentHero->GetAttributes().IsDead())
	{
		FinalizeHeroDefeat(*CurrentHero, *CurrentEnemy, bEnemyDied);
	}
	else if (bEnemyDied)
	{
		FinalizeEnemyVictory(*CurrentEnemy);
		ContinueAfterOrdinaryVictory();
	}
}

void UCombatSession::ResetDeferredEncounterOutcome()
{
	FCoreDelegates::OnEndFrame.Remove(DeferredOutcomeListener);
	DeferredOutcomeListener.Reset();
	DeferredOutcomeEncounterID.Invalidate();
}

void UCombatSession::RecordEnemyVictory(const ACombatEnemy& CurrentEnemy)
{
	// Копируем данные победы до смены актора. Профиль применит исход один раз по EncounterID.
	EncounterResult = FCombatEncounterResult{EncounterID, EnemyDefinitionID, bCurrentEnemyIsBoss,
		EnemyLoot ? EnemyLoot->GetGuaranteedCurrencies() : TMap<FName, int64>{}};
	UE_LOG(LogCombatSession, Display, TEXT("Combat enemy defeated: %s"), *CurrentEnemy.GetName());
	UE_LOG(LogCombatSession, Display, TEXT("Combat encounter result recorded: %s; Enemy definition: %s"),
		*EncounterID.ToString(EGuidFormats::DigitsWithHyphens), *EnemyDefinitionID.ToString());
}

void UCombatSession::FinalizeEnemyVictory(ACombatEnemy& CurrentEnemy)
{
	StopEnemyAttacks();
	CurrentEnemy.DisableClickAreas();
	RecordEnemyVictory(CurrentEnemy);

	if (!EncounterResult.GetValue().bDefeatedBoss)
	{
		SetCombatPhase(ECombatPhase::ChangingEnemy);
		UE_LOG(LogCombatSession, Display, TEXT("Combat phase: ChangingEnemy"));
		return;
	}

	UFurwardProfileSubsystem* Profile = GetProfile();
	if (!Profile || !ApplyEncounterResult(*Profile))
	{
		UE_LOG(LogCombatSession, Warning, TEXT("Boss result could not complete expedition stage %s."), *ActiveStageID.ToString());
		return;
	}

	EnterStageCompleted();
	QueueProgressAutosave(*Profile);
}

bool UCombatSession::ApplyEncounterResult(UFurwardProfileSubsystem& Profile)
{
	const FCombatEncounterResult& Result = EncounterResult.GetValue();
	bool bApplied;
	if (Mode == ECombatMode::Farm)
	{
		bApplied = Profile.TryRecordFarmVictory(ActiveStageID, Result.EncounterID, Result.CurrencyRewards);
	}
	else if (Result.bDefeatedBoss)
	{
		bApplied = Profile.TryCompleteExpeditionStage(ActiveStageID, Result.EncounterID, Result.CurrencyRewards);
	}
	else
	{
		bApplied = Profile.TryRecordExpeditionClear(ActiveStageID, Result.EncounterID, Result.CurrencyRewards);
	}

	if (!bApplied)
	{
		UE_LOG(LogCombatSession, Warning, TEXT("Combat profile rejected encounter: %s"), *Result.EncounterID.ToString());
		return false;
	}

	for (const TPair<FName, int64>& Currency : Result.CurrencyRewards)
	{
		UE_LOG(LogCombatSession, Display, TEXT("Combat reward applied: %s +%lld; Balance: %lld"),
			*Currency.Key.ToString(), Currency.Value, Profile.GetCurrencyBalance(Currency.Key));
	}
	if (!Result.CurrencyRewards.IsEmpty())
	{
		CurrencyBalancesChanged.Broadcast();
	}
	return true;
}

void UCombatSession::HandleHeroDeath(const UCombatAttributesComponent& Source)
{
	const ACombatHero* CurrentHero = Hero.Get();
	ACombatEnemy* CurrentEnemy = Enemy.Get();
	if (Phase != ECombatPhase::Fighting || !CurrentHero || !CurrentEnemy ||
		&CurrentHero->GetAttributes() != &Source || !Source.IsDead())
	{
		return;
	}

	if (!DeferEncounterOutcome())
	{
		FinalizeHeroDefeat(*CurrentHero, *CurrentEnemy, false);
	}
}

void UCombatSession::FinalizeHeroDefeat(const ACombatHero& CurrentHero, ACombatEnemy& CurrentEnemy, bool bCreditVictory)
{
	SetCombatPhase(ECombatPhase::Defeated);
	CurrentHero.GetAbilities().ClearEffects();
	StopEnemyAttacks();
	CurrentEnemy.DisableClickAreas();

	const UCombatAttributesComponent& EnemyAttributes = CurrentEnemy.GetAttributes();
	DefeatSnapshot = FCombatDefeatSnapshot{EnemyAttributes.GetCurrentHealth(), EnemyAttributes.GetMaxHealth()};
	if (bCreditVictory)
	{
		UE_LOG(LogCombatSession, Display, TEXT("Simultaneous defeat: %s; Boss: %d"), *EncounterID.ToString(), bCurrentEnemyIsBoss);
		RecordEnemyVictory(CurrentEnemy);
		UFurwardProfileSubsystem* Profile = GetProfile();
		if (Profile && ApplyEncounterResult(*Profile))
		{
			QueueProgressAutosave(*Profile);
			ProgressionChanged.Broadcast();
		}
	}
	UE_LOG(LogCombatSession, Display, TEXT("Combat phase: Defeated; Enemy HP: %.2f/%.2f"),
		EnemyAttributes.GetCurrentHealth(), EnemyAttributes.GetMaxHealth());
	DefeatRecorded.Broadcast();
}

bool UCombatSession::CanAttackEnemy(const ACombatEnemy* Target) const
{
	if (Phase != ECombatPhase::Ready && Phase != ECombatPhase::Fighting)
	{
		UE_LOG(LogCombatSession, Verbose, TEXT("Combat attack rejected: combat phase does not allow attacks."));
		return false;
	}

	return CanUseCurrentEnemy(Target, ECombatAttackSource::Hero);
}

bool UCombatSession::SetLeaveConfirmationOpen(bool bOpen)
{
	if (bOpen && ActiveOverlay != ECombatOverlay::LeaveConfirmation)
	{
		if (Phase == ECombatPhase::Defeated || Phase == ECombatPhase::Leaving ||
			Phase == ECombatPhase::Recovering || (Mode == ECombatMode::Farm && Phase == ECombatPhase::Preparing))
		{
			return false;
		}
		if (PendingSaveAction != EProgressSaveAction::None)
		{
			return false;
		}
	}

	return SetCombatOverlay(ECombatOverlay::LeaveConfirmation, bOpen);
}

bool UCombatSession::SetPauseMenuOpen(bool bOpen)
{
	if (bOpen && Phase != ECombatPhase::Ready && Phase != ECombatPhase::Fighting)
	{
		return false;
	}
	return SetCombatOverlay(ECombatOverlay::PauseMenu, bOpen);
}

bool UCombatSession::SetCombatOverlay(ECombatOverlay Overlay, bool bOpen)
{
	if (bOpen == (ActiveOverlay == Overlay))
	{
		return true;
	}
	if (bOpen && (bShutdown || IsCombatInputSuspended() || DeferredOutcomeEncounterID.IsValid()))
	{
		return false;
	}

	ActiveOverlay = bOpen ? Overlay : ECombatOverlay::None;
	RefreshCombatTime();
	UE_LOG(LogCombatSession, Display, TEXT("%s %s"),
		Overlay == ECombatOverlay::PauseMenu ? TEXT("Combat pause") : TEXT("Combat exit confirmation"),
		bOpen ? TEXT("opened") : TEXT("closed"));
	return true;
}

bool UCombatSession::CanUseCurrentEnemy(const ACombatEnemy* Target, ECombatAttackSource Source) const
{
	if (!Hero.IsValid() || !ActiveArena.IsValid() ||
		!IsValid(EnvironmentStreaming.Get()) || !EnvironmentStreaming->IsLevelVisible())
	{
		UE_LOG(LogCombatSession, Verbose, TEXT("Combat attack rejected: combat scene is not ready."));
		return false;
	}

	// Сравниваем экземпляры, чтобы результат по прошлой цели не применился к новой.
	if (!IsValid(Target) || Target != Enemy.Get())
	{
		UE_LOG(LogCombatSession, Verbose, TEXT("Combat attack rejected: target is not the active enemy."));
		return false;
	}

	// Спауна недостаточно: оба компонента должны получить характеристики и иметь положительное здоровье.
	const UCombatAttributesComponent& HeroAttributes = Hero->GetAttributes();
	const UCombatAttributesComponent& EnemyAttributes = Target->GetAttributes();
	const bool bPendingThisFrame = IsEncounterOutcomePendingThisFrame();
	if (!HeroAttributes.IsHealthInitialized() || !EnemyAttributes.IsHealthInitialized() ||
		(HeroAttributes.IsDead() && !(bPendingThisFrame && Source == ECombatAttackSource::Hero)) ||
		(EnemyAttributes.IsDead() && !(bPendingThisFrame && Source == ECombatAttackSource::Enemy)))
	{
		return false;
	}

	// Приостановка сохраняет фазу и запрещает атаки до закрытия окна или снятия паузы.
	const UWorld* World = Target->GetWorld();
	if (!World || World->IsPaused() || IsCombatInputSuspended())
	{
		UE_LOG(LogCombatSession, Verbose, TEXT("Combat attack rejected: gameplay input is suspended."));
		return false;
	}

	return true;
}

void UCombatSession::Shutdown()
{
	if (bShutdown)
	{
		return;
	}
	bShutdown = true;
	ResetDeferredEncounterOutcome();
	ActiveOverlay = ECombatOverlay::None;
	ExitSaveFailed.Clear();
	DefeatRecorded.Clear();

	// Постоянный уровень продолжает владеть участниками до завершения мира.
	SetCombatPhase(ECombatPhase::Leaving);
	ResetPendingProgressSave();
	ProgressSlot = NAME_None;
	StopEnemyAttacks();
	if (ACombatHero* CurrentHero = Hero.Get())
	{
		CurrentHero->GetAbilities().ClearSlots();
		UCombatAttributesComponent& Attributes = CurrentHero->GetAttributes();
		Attributes.OnDeath().RemoveAll(this);
		Attributes.OnHealthChanged().RemoveAll(this);
	}
	if (ACombatEnemy* CurrentEnemy = Enemy.Get())
	{
		UCombatAttributesComponent& Attributes = CurrentEnemy->GetAttributes();
		Attributes.OnDeath().RemoveAll(this);
		Attributes.OnHealthChanged().RemoveAll(this);
	}

	Hero.Reset();
	Enemy.Reset();
	ParticipantHealthChanged.Broadcast();
	ParticipantHealthChanged.Clear();
	CurrencyBalancesChanged.Clear();
	EncounterID.Invalidate();
	EnemyDefinitionID = FPrimaryAssetId();
	EnemyLoot = nullptr;
	bCurrentEnemyIsBoss = false;
	EncounterResult.Reset();
	DefeatSnapshot.Reset();
	InitialEnemy = nullptr;
	BossEnemy = nullptr;
	ActiveStageID = NAME_None;
	ActiveStageTable = nullptr;
	InitialHeroAttributes.Reset();
	ProgressionChanged.Broadcast();
	ProgressionChanged.Clear();
	ActiveArena.Reset();

	CombatRules = nullptr;

	ReleaseEnvironment();
	RetiringEnvironment.Reset();
	NextStageState = ECombatNextStageState::Unavailable;

	UE_LOG( LogCombatSession, Log, TEXT("Shutdown: %s"), *GetName() );
}