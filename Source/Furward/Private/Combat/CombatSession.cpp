// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Готовит столкновения, фиксирует исход и ждёт записи перед сменой этапа или выходом.
 */

#include "Combat/CombatSession.h"
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
#include "Profile/FurwardProfileSubsystem.h"
#include "Save/FurwardSaveSubsystem.h"
#include "TimerManager.h"



DEFINE_LOG_CATEGORY_STATIC( LogCombatSession, Log, All );



#if WITH_EDITOR
void UCombatSession::EnablePIECombatPreview(UCombatRulesDefinition* InRules)
{
	bPIECombatPreview = true;
	PIECombatRules = InRules;
}

bool UCombatSession::RequestNextStage(FName SaveSlot)
{
	const UWorld* World = GetOuter()->GetWorld();
	const UFurwardProfileSubsystem* Profile = GetProfile();
	if (!bPIECombatPreview || !World || World->IsPaused() || Phase != ECombatPhase::StageCompleted ||
		!Profile || !Profile->IsExpeditionStageCompleted(ActiveStageID))
	{
		return false;
	}

	FName NextStageID;
	return FindNextStageOnActiveArena(NextStageID) &&
		BeginPIEProgressSave(SaveSlot, EProgressSaveAction::NextStage, NextStageID);
}

bool UCombatSession::RequestExitCombat(FName SaveSlot)
{
	return BeginPIEProgressSave(SaveSlot, EProgressSaveAction::Exit);
}

bool UCombatSession::BeginPIEProgressSave(FName Slot, EProgressSaveAction Action, FName NextStageID)
{
	const bool bSavePending = PendingSaveAction != EProgressSaveAction::None;
	const bool bExit = Action == EProgressSaveAction::Exit;
	if (!bPIECombatPreview || bShutdown || Slot.IsNone() || (!PIEProgressSlot.IsNone() && PIEProgressSlot != Slot) ||
		(bSavePending && (!bExit || PendingSaveAction != EProgressSaveAction::NextStage)))
	{
		return false;
	}

	const UWorld* World = GetOuter()->GetWorld();
	const UFurwardProfileSubsystem* Profile = GetProfile();
	UFurwardSaveSubsystem* Save = World ? UGameInstance::GetSubsystem<UFurwardSaveSubsystem>(World->GetGameInstance()) : nullptr;
	if (!Profile || !Save || (PIEProgressSlot.IsNone() &&
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

	PIEProgressSlot = Slot;
	PendingSaveAction = Action;
	PendingNextStageID = NextStageID;
	if (bExit)
	{
		Phase = ECombatPhase::Leaving;
		StopPIEEnemyAttacks();
		UE_LOG(LogCombatSession, Display, TEXT("Combat phase: Leaving; Awaiting save revision: %lld"), Revision);
	}
	else
	{
		UE_LOG(LogCombatSession, Display, TEXT("Stage transition awaiting save: %s -> %s; Revision: %lld"),
			*ActiveStageID.ToString(), *NextStageID.ToString(), Revision);
	}

	// Сверяем статус сразу на случай уже записанной ревизии или синхронной ошибки запуска записи.
	CheckPendingProgressSave();
	return true;
}

const FCombatStageRow* UCombatSession::FindNextStageOnActiveArena(FName& NextStageID) const
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
	if (!Next || !IsValid(Current->Environment.Get()) || !IsValid(Next->Environment.Get()) ||
		Current->Environment->GetEnvironmentMap() != Next->Environment->GetEnvironmentMap() ||
		Next->EnemyRoster.IsEmpty() || !IsValid(Next->EnemyRoster[0].Get()))
	{
		UE_LOG(LogCombatSession, Warning, TEXT("PIE next stage requires a configured enemy and the active environment."));
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
	if (Slot == PIEProgressSlot && PendingSaveAction != EProgressSaveAction::None)
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
		ResetPendingProgressSave();
		return;
	}

	const FFurwardSaveStatus Status = Save->GetSaveStatus(PIEProgressSlot);
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
			BeginNextStageOnActiveArena(NextStageID);
		}
	}
	else if (Status.bLatestWriteFailed || (Snapshot.Revision > Status.LatestRequestedRevision &&
		!Save->RequestSave(PIEProgressSlot, MoveTemp(Snapshot))))
	{
		if (bExit)
		{
			UE_LOG(LogCombatSession, Warning, TEXT("Combat exit save failed; request the exit again to retry."));
		}
		else
		{
			UE_LOG(LogCombatSession, Warning, TEXT("Stage transition save failed; request the next stage again to retry."));
		}
		ResetPendingProgressSave();
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

void UCombatSession::BeginNextStageOnActiveArena(FName StageID)
{
	FName NextStageID;
	const FCombatStageRow* Next = FindNextStageOnActiveArena(NextStageID);
	ACombatArena* Arena = ActiveArena.Get();
	UWorld* World = Arena ? Arena->GetWorld() : nullptr;
	if (!Next || NextStageID != StageID || !World || World->IsPaused())
	{
		return;
	}

	if (ACombatEnemy* PreviousEnemy = Enemy.Get())
	{
		UCombatAttributesComponent& Attributes = PreviousEnemy->GetAttributes();
		if (!PreviousEnemy->Destroy())
		{
			return;
		}
		Attributes.OnDeath().RemoveAll(this);
	}
	Enemy.Reset();
	EncounterID.Invalidate();
	EnemyDefinitionID = FPrimaryAssetId();
	EncounterResult.Reset();
	bCurrentEnemyIsBoss = false;

	Phase = ECombatPhase::Preparing;
	ActiveStageID = NextStageID;
	InitialEnemy = Next->EnemyRoster[0];
	BossEnemy = Next->Boss;
	UE_LOG(LogCombatSession, Display, TEXT("Combat phase: Preparing; Stage: %s"), *ActiveStageID.ToString());
	PrepareStageParticipants(*World, *Arena);
}

void UCombatSession::TryContinuePIECombatPreview()
{
	if (Phase != ECombatPhase::ChangingEnemy || !EncounterResult.IsSet())
	{
		return;
	}

	// Зачитываем победу в профиле до решения о создании следующего противника.
	UFurwardProfileSubsystem* Profile = GetProfile();
	if (!Profile || !Profile->TryRecordExpeditionClear(ActiveStageID, EncounterResult.GetValue().EncounterID))
	{
		return;
	}

	const int32 ClearedEnemies = Profile->GetExpeditionClearCount(ActiveStageID);
	UE_LOG(LogCombatSession, Display, TEXT("PIE expedition clear: %s; %d/%d"),
		*ActiveStageID.ToString(), ClearedEnemies, UFurwardProfileSubsystem::RequiredExpeditionClears);
	const bool bBossPending = Profile->IsExpeditionClearComplete(ActiveStageID);
	const UCombatEnemyDefinition* NextDefinition = SelectPIEEnemyDefinition(bBossPending);

	const UCombatRulesDefinition* Rules = PIECombatRules.Get();
	const ACombatEnemy* DefeatedEnemy = Enemy.Get();
	if (!IsValid(Rules) || !DefeatedEnemy || !NextDefinition)
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
	if (!ResolvePIEEnemyAttributes(*NextDefinition, EnemyBase))
	{
		return;
	}
	if (!PrepareNextEnemy(*NextDefinition, EnemyBase) || !ActivatePreparedEncounter(EncounterID, *Rules))
	{
		UE_LOG(LogCombatSession, Warning, TEXT("PIE combat preview could not activate the next encounter."));
		return;
	}

	if (bBossPending)
	{
		UE_LOG(LogCombatSession, Display, TEXT("PIE expedition boss activated: %s"), *ActiveStageID.ToString());
	}
}

const UCombatEnemyDefinition* UCombatSession::SelectPIEEnemyDefinition(bool bBossPending) const
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

bool UCombatSession::ResolvePIEEnemyAttributes(const UCombatEnemyDefinition& Definition, FCombatBaseAttributes& EnemyBase) const
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

void UCombatSession::StartPIEEnemyAttacks()
{
	const UCombatRulesDefinition* Rules = PIECombatRules.Get();
	const ACombatEnemy* CurrentEnemy = Enemy.Get();
	if (!bPIECombatPreview || !IsValid(Rules) || !CanUseCurrentEnemy(CurrentEnemy))
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
	CurrentEnemy->GetWorld()->GetTimerManager().SetTimer(PIEEnemyAttackTimer,
		FTimerDelegate::CreateUObject(this, &UCombatSession::HandlePIEEnemyAttack, EncounterID),
		Interval, true, FirstDelay);
	UE_LOG(LogCombatSession, Display, TEXT("Enemy attacks scheduled: first %.2fs; interval %.2fs"), FirstDelay, Interval);
}

void UCombatSession::StopPIEEnemyAttacks()
{
	// Outer сессии — GameMode. Он даёт доступ к миру даже при потере ссылки на противника.
	if (UWorld* World = GetOuter()->GetWorld())
	{
		World->GetTimerManager().ClearTimer(PIEEnemyAttackTimer);
	}
	PIEEnemyAttackTimer.Invalidate();
}

void UCombatSession::HandlePIEEnemyAttack(FGuid AttackingEncounterID)
{
	const ACombatEnemy* CurrentEnemy = Enemy.Get();
	if (Phase != ECombatPhase::Fighting || AttackingEncounterID != EncounterID ||
		!CanUseCurrentEnemy(CurrentEnemy))
	{
		return;
	}

	UCombatAttributesComponent& HeroAttributes = Hero->GetAttributes();
	const float DamagePerHit = CurrentEnemy->GetAttributes().GetBaseAttributes().DamagePerHit;
	const float Armor = HeroAttributes.GetBaseAttributes().Armor;
	const float PreviousHealth = HeroAttributes.GetCurrentHealth();
	const float AppliedDamage = HeroAttributes.ReduceHealth(FMath::Max(0.0f, DamagePerHit - Armor));
	if (AppliedDamage <= 0.0f)
	{
		return;
	}

	UE_LOG(LogCombatSession, Display, TEXT("Enemy attack applied: %.2f; Hero HP: %.2f/%.2f"),
		AppliedDamage, HeroAttributes.GetCurrentHealth(), HeroAttributes.GetMaxHealth());
	HeroAttributes.PublishDamage(PreviousHealth);
}
#endif

void UCombatSession::RequestInitialStage(UObject* WorldContextObject, const FDataTableRowHandle& StageHandle)
{
	// В Preparing допускаем один отслеживаемый запрос на начальное окружение.
	if (Phase != ECombatPhase::Preparing || EnvironmentStreaming) {
		UE_LOG( LogCombatSession, Log, TEXT("Initial environment request is not available.") );
		return;
	}

	// Таблица задаёт контент этапа по имени строки. Ошибки настройки оставляют сессию в Preparing.
	const FCombatStageRow* Stage = StageHandle.GetRow<FCombatStageRow>(TEXT("Initial combat stage"));
	if (!Stage || !IsValid(Stage->Environment.Get()) || Stage->Environment->GetEnvironmentMap().IsNull() ||
		Stage->EnemyRoster.IsEmpty() || !IsValid(Stage->EnemyRoster[0].Get()))
	{
		UE_LOG(LogCombatSession, Warning, TEXT("Initial combat stage %s requires a row, environment map and first enemy."),
			*StageHandle.RowName.ToString());
		return;
	}

	const TSoftObjectPtr<UWorld>& EnvironmentMap = Stage->Environment->GetEnvironmentMap();
	// Контекст выбирает мир, мягкая ссылка выбирает карту, а Identity сохраняет авторскую расстановку.
	bool RequestAccepted {};
	ULevelStreamingDynamic* Streaming = (
		ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(
			WorldContextObject,
			EnvironmentMap,
			FTransform::Identity,
			RequestAccepted
		)
	);

	// При отказе сессия остаётся в Preparing. Путь в логе указывает, какую карту запрашивали.
	if ( !RequestAccepted || !IsValid(Streaming) ) {
		UE_LOG( LogCombatSession, Error, TEXT("Could not request environment: %s"), *EnvironmentMap.ToSoftObjectPath().ToString() );
		return;
	}

	// Запоминаем объект, чтобы принять уведомление о показе и позже запросить выгрузку.
	EnvironmentStreaming = Streaming;
	InitialEnemy = Stage->EnemyRoster[0];
	BossEnemy = Stage->Boss;
	ActiveStageID = StageHandle.RowName;
	ActiveStageTable = StageHandle.DataTable;

	// Unreal вызовет обработчик, когда добавит подуровень в мир и сделает его видимым.
	Streaming->OnLevelShown.AddDynamic(this, &UCombatSession::HandleEnvironmentShown);

	// Лог подтверждает приём запроса. Проверка видимости и регистрация арены произойдут в обработчике.
	UE_LOG(LogCombatSession, Display, TEXT("Initial combat stage: %s; Environment: %s; First enemy: %s"),
		*StageHandle.RowName.ToString(), *Stage->Environment->GetName(), *InitialEnemy->GetName());
	UE_LOG( LogCombatSession, Display, TEXT("Environment requested: %s"), *EnvironmentMap.ToSoftObjectPath().ToString() );
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
    ULevel*       Level      = EnvironmentStreaming->GetLoadedLevel();
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
        return;
    }

    // Регистрируем арену как источник ракурса и координат текущих участников.
    ActiveArena = FoundArena;
    UE_LOG( LogCombatSession, Display, TEXT("Arena registered: %s"), *FoundArena->GetName() );

    // Ищем боевой контроллер в том же мире. Без него подготовка остаётся незавершённой.
    UWorld* World = Level->GetWorld();
    ACombatPlayerController* Controller = World ? World->GetFirstPlayerController <ACombatPlayerController> () : nullptr;

    if ( !IsValid(Controller) ) {
        UE_LOG( LogCombatSession, Error, TEXT("Combat player controller unavailable.") );
        return;
    }

    Controller->UseArenaCamera(FoundArena);
    PrepareStageParticipants(*World, *FoundArena);
}

void UCombatSession::PrepareStageParticipants(UWorld& World, const ACombatArena& Arena)
{
    const UCombatEnemyDefinition* StartingDefinition = InitialEnemy.Get();
#if WITH_EDITOR
    const UFurwardProfileSubsystem* Profile = bPIECombatPreview ? GetProfile() : nullptr;
    if (Profile && Profile->IsExpeditionStageCompleted(ActiveStageID))
    {
        EnterStageCompleted();
        return;
    }

    if (Profile && Profile->IsExpeditionClearComplete(ActiveStageID))
    {
        StartingDefinition = SelectPIEEnemyDefinition(true);
        if (!StartingDefinition)
        {
            return;
        }
        UE_LOG(LogCombatSession, Display, TEXT("PIE expedition resumes with boss at 10/10: %s"),
            *ActiveStageID.ToString());
    }
#endif

    // Создаём участников по точкам арены на постоянном уровне, когда выбран следующий бой.
    SpawnHero(World, Arena);
    if (IsValid(StartingDefinition))
    {
        if (ACombatEnemy* SpawnedEnemy = SpawnEnemy(World, Arena, *StartingDefinition))
        {
            RegisterEnemy(*SpawnedEnemy, *StartingDefinition);
        }
    }

    if (Hero.IsValid() && Enemy.IsValid())
    {
#if WITH_EDITOR
        if (bPIECombatPreview)
        {
            const UCombatRulesDefinition* Rules = PIECombatRules.Get();
            if (!IsValid(Rules))
            {
                UE_LOG(LogCombatSession, Warning, TEXT("PIE combat requires a configured Combat Rules asset."));
                return;
            }

            FCombatBaseAttributes EnemyBase;
            if (!ResolvePIEEnemyAttributes(*StartingDefinition, EnemyBase))
            {
                return;
            }

            // Пакет героя из правил применяется при первом спауне, продолжение сохраняет живой экземпляр.
            if (Hero->GetAttributes().IsHealthInitialized())
            {
                // На той же арене герой остаётся тем же актором. Инициализируем нового противника отдельно.
                UCombatAttributesComponent& EnemyAttributes = Enemy->GetAttributes();
                if (!Hero->GetAttributes().IsDead() &&
                    EnemyAttributes.CanInitializeAttributes(EnemyBase, EnemyBase.MaxHealth))
                {
                    EnemyAttributes.SetInitialAttributes(EnemyBase, EnemyBase.MaxHealth);
                    EnterStageReady();
                    EnemyAttributes.PublishInitialHealth();
                    return;
                }
            }
            else if (InitializeParticipantAttributes(Rules->PIEHeroBaseAttributes, Rules->PIEHeroStartingHealth, EnemyBase))
            {
                UE_LOG(LogCombatSession, Display, TEXT("PIE combat preview enabled."));
                return;
            }
        }
#endif

        // Без начальных характеристик сцена остаётся в Preparing.
        UE_LOG(LogCombatSession, Display, TEXT("Combat phase: Preparing; waiting for participant attributes."));
    }
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
	SpawnedHero->GetAttributes().OnDeath().AddUObject(this, &UCombatSession::HandleHeroDeath);

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
	bCurrentEnemyIsBoss = BossEnemy.Get() == &Definition && InitialEnemy.Get() != &Definition;
	EncounterResult.Reset();
	SpawnedEnemy.GetAttributes().OnDeath().AddUObject(this, &UCombatSession::HandleEnemyDeath);
}

void UCombatSession::Initialize(TSubclassOf<ACombatHero> InHeroClass) {
	// Сохраняем класс героя до появления арены. Outer в логе позволяет связать сессию с её GameMode.
	HeroClass = InHeroClass;
	UE_LOG( LogCombatSession, Log, TEXT("Initialize: %s; Outer: %s"), *GetName(), *GetOuter()->GetName() );
}

const ACombatEnemy* UCombatSession::GetActiveEnemy() const
{
	if (Phase == ECombatPhase::ChangingEnemy || Phase == ECombatPhase::Defeated || Phase == ECombatPhase::StageCompleted)
	{
		return nullptr;
	}

	const ACombatEnemy* CurrentEnemy = Enemy.Get();
	return CurrentEnemy && !CurrentEnemy->GetAttributes().IsDead() ? CurrentEnemy : nullptr;
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
	Phase = ECombatPhase::Ready;
	UE_LOG(LogCombatSession, Display, TEXT("Combat phase: Ready; Hero HP %.2f/%.2f; Enemy HP %.2f/%.2f"),
		HeroAttributes.GetCurrentHealth(), HeroAttributes.GetMaxHealth(),
		EnemyAttributes.GetCurrentHealth(), EnemyAttributes.GetMaxHealth());
}

bool UCombatSession::PrepareNextEnemy(const UCombatEnemyDefinition& Definition, const FCombatBaseAttributes& EnemyBase)
{
	ACombatEnemy* PreviousEnemy = Enemy.Get();
	ACombatArena* Arena = ActiveArena.Get();
	const ACombatHero* CurrentHero = Hero.Get();
	if (Phase != ECombatPhase::ChangingEnemy || !EncounterResult.IsSet() || EncounterResult.GetValue().bDefeatedBoss ||
		!PreviousEnemy || !PreviousEnemy->GetAttributes().IsDead() ||
		!Arena || !CurrentHero || CurrentHero->GetAttributes().IsDead() ||
		!IsValid(EnvironmentStreaming.Get()) || !EnvironmentStreaming->IsLevelVisible())
	{
		return false;
	}

	UWorld* World = Arena->GetWorld();
	ACombatEnemy* NextEnemy = World ? SpawnEnemy(*World, *Arena, Definition) : nullptr;
	if (!NextEnemy)
	{
		return false;
	}

	// Пока замена не готова, оставляем погибшего актора и даём вызывающему коду повторить операцию.
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
	NextAttributes.SetInitialAttributes(EnemyBase, EnemyBase.MaxHealth);
	RegisterEnemy(*NextEnemy, Definition);

	UE_LOG(LogCombatSession, Display, TEXT("Next enemy prepared: %s; HP: %.2f/%.2f"),
		*NextEnemy->GetName(), NextAttributes.GetCurrentHealth(), NextAttributes.GetMaxHealth());
	NextAttributes.PublishInitialHealth();
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
		Phase = ECombatPhase::Ready;
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

#if WITH_EDITOR
	// В режиме проверки только Hit применяет формулу. Смена цели ждёт завершения публикации урона и смерти.
	if (bPIECombatPreview && Result == ECombatClickResult::Hit)
	{
		const float DamagePerHit = Hero->GetAttributes().GetBaseAttributes().DamagePerHit;
		const float Armor = Target->GetAttributes().GetBaseAttributes().Armor;
		if (ApplyResolvedDamageToEnemy(Target, FMath::Max(0.0f, DamagePerHit - Armor)) > 0.0f)
		{
			TryContinuePIECombatPreview();
		}
	}
#endif
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
	return AppliedDamage;
}

void UCombatSession::EnterFighting()
{
	Phase = ECombatPhase::Fighting;
	UE_LOG(LogCombatSession, Display, TEXT("Combat phase: Fighting"));
#if WITH_EDITOR
	StartPIEEnemyAttacks();
#endif
}

void UCombatSession::EnterStageCompleted()
{
	Phase = ECombatPhase::StageCompleted;
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

	// Сначала останавливаем атаки и отключаем области клика погибшей цели.
#if WITH_EDITOR
	StopPIEEnemyAttacks();
#endif
	CurrentEnemy->DisableClickAreas();

	// Фиксируем исход по идентификатору столкновения и типу контента. Актор может быть удалён при замене.
	EncounterResult = FCombatEncounterResult{EncounterID, EnemyDefinitionID, bCurrentEnemyIsBoss};
	UE_LOG(LogCombatSession, Display, TEXT("Combat enemy defeated: %s"), *CurrentEnemy->GetName());
	UE_LOG(LogCombatSession, Display, TEXT("Combat encounter result recorded: %s; Enemy definition: %s"),
		*EncounterID.ToString(EGuidFormats::DigitsWithHyphens), *EnemyDefinitionID.ToString());

	if (!EncounterResult.GetValue().bDefeatedBoss)
	{
		Phase = ECombatPhase::ChangingEnemy;
		UE_LOG(LogCombatSession, Display, TEXT("Combat phase: ChangingEnemy"));
		return;
	}

	UFurwardProfileSubsystem* Profile = GetProfile();
	if (!Profile || !Profile->TryCompleteExpeditionStage(ActiveStageID, EncounterResult.GetValue().EncounterID))
	{
		UE_LOG(LogCombatSession, Warning, TEXT("Boss result could not complete expedition stage %s."), *ActiveStageID.ToString());
		return;
	}

	EnterStageCompleted();
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

	Phase = ECombatPhase::Defeated;
#if WITH_EDITOR
	StopPIEEnemyAttacks();
#endif
	CurrentEnemy->DisableClickAreas();

	const UCombatAttributesComponent& EnemyAttributes = CurrentEnemy->GetAttributes();
	DefeatSnapshot = FCombatDefeatSnapshot{EnemyAttributes.GetCurrentHealth(), EnemyAttributes.GetMaxHealth()};
	UE_LOG(LogCombatSession, Display, TEXT("Combat phase: Defeated; Enemy HP: %.2f/%.2f"),
		EnemyAttributes.GetCurrentHealth(), EnemyAttributes.GetMaxHealth());
}

bool UCombatSession::CanAttackEnemy(const ACombatEnemy* Target) const
{
	if (Phase != ECombatPhase::Ready && Phase != ECombatPhase::Fighting)
	{
		UE_LOG(LogCombatSession, Verbose, TEXT("Combat attack rejected: combat phase does not allow attacks."));
		return false;
	}

	return CanUseCurrentEnemy(Target);
}

bool UCombatSession::CanUseCurrentEnemy(const ACombatEnemy* Target) const
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
	if (!HeroAttributes.IsHealthInitialized() || !EnemyAttributes.IsHealthInitialized() ||
		HeroAttributes.IsDead() || EnemyAttributes.IsDead())
	{
		return false;
	}

	// Если игровой мир на паузе или недоступен, команда не проходит проверку.
	const UWorld* World = Target->GetWorld();
	if (!World || World->IsPaused())
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

	// Постоянный уровень продолжает владеть участниками до завершения мира.
	Phase = ECombatPhase::Leaving;
#if WITH_EDITOR
	ResetPendingProgressSave();
	PIEProgressSlot = NAME_None;
	StopPIEEnemyAttacks();
#endif
	if (ACombatHero* CurrentHero = Hero.Get())
	{
		CurrentHero->GetAttributes().OnDeath().RemoveAll(this);
	}
	if (ACombatEnemy* CurrentEnemy = Enemy.Get())
	{
		CurrentEnemy->GetAttributes().OnDeath().RemoveAll(this);
	}

	Hero.Reset();
	Enemy.Reset();
	EncounterID.Invalidate();
	EnemyDefinitionID = FPrimaryAssetId();
	bCurrentEnemyIsBoss = false;
	EncounterResult.Reset();
	DefeatSnapshot.Reset();
	InitialEnemy = nullptr;
	BossEnemy = nullptr;
	ActiveStageID = NAME_None;
	ActiveStageTable = nullptr;
	ActiveArena.Reset();

#if WITH_EDITORONLY_DATA
	PIECombatRules = nullptr;
#endif

	// Расцепляем динамическую подписку до запроса выгрузки, чтобы OnLevelShown больше не готовил сцену.
	if ( IsValid(EnvironmentStreaming.Get()) ) {
		EnvironmentStreaming->OnLevelShown.RemoveDynamic(this, &UCombatSession::HandleEnvironmentShown);
		// Unreal применит запрос на выгрузку и удаление подуровня при обработке стриминга.
		EnvironmentStreaming->SetIsRequestingUnloadAndRemoval(true);
	}

	// Сессия отпускает объект управления после отправки запроса. Shutdown в логе подтверждает завершение сессии.
	EnvironmentStreaming = nullptr;

	UE_LOG( LogCombatSession, Log, TEXT("Shutdown: %s"), *GetName() );
}