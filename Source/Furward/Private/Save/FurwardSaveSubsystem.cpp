// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Открывает слот, передаёт данные профилю и отмечает подтверждённые ревизии последовательной записи.
 */

#include "Save/FurwardSaveSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Profile/FurwardProfileSubsystem.h"
#include "Save/FurwardSaveGame.h"

DEFINE_LOG_CATEGORY_STATIC(LogFurwardSave, Log, All);

namespace
{
/** @brief Проверяет, что имя соответствует одному слоту без пути к каталогу. */
bool IsValidSlotName(FName SlotName)
{
	const FString Name = SlotName.ToString();
	return !SlotName.IsNone() && FPaths::GetCleanFilename(Name) == Name && FPaths::ValidatePath(Name);
}
}

bool UFurwardSaveSubsystem::TryOpenProfileSlot(FName SlotName, UFurwardProfileSubsystem& Profile)
{
	check(IsInGameThread());

	if (bDeinitializing || !IsValidSlotName(SlotName) || Profile.GetGameInstance() != GetGameInstance())
	{
		return false;
	}
	if (OpenedProfileSlot == SlotName)
	{
		return true;
	}
	if (!OpenedProfileSlot.IsNone() || Profile.GetRevision() != 0 || ActiveSave.IsSet() || !PendingSaves.IsEmpty())
	{
		return false;
	}

	const FString Name = SlotName.ToString();
	if (UGameplayStatics::DoesSaveGameExist(Name, LocalUserIndex))
	{
		const UFurwardSaveGame* SavedGame = Cast<UFurwardSaveGame>(UGameplayStatics::LoadGameFromSlot(Name, LocalUserIndex));
		const FFurwardProfileSnapshot* Snapshot = SavedGame ? SavedGame->GetProfileSnapshot() : nullptr;
		if (!Snapshot || !Profile.TryRestoreSnapshot(*Snapshot))
		{
			UE_LOG(LogFurwardSave, Warning, TEXT("Profile slot could not be loaded: %s"), *Name);
			return false;
		}

		StatusBySlot.Add(SlotName, FFurwardSaveStatus{Snapshot->Revision, Snapshot->Revision, false});
		UE_LOG(LogFurwardSave, Display, TEXT("Profile slot loaded: %s; Revision: %lld"), *Name, Snapshot->Revision);
	}
	else
	{
		UE_LOG(LogFurwardSave, Display, TEXT("New profile slot: %s"), *Name);
	}

	OpenedProfileSlot = SlotName;
	return true;
}

bool UFurwardSaveSubsystem::RequestSave(FName SlotName, FFurwardProfileSnapshot Snapshot)
{
	check(IsInGameThread());

	if (bDeinitializing || Snapshot.Revision < 0 || !IsValidSlotName(SlotName))
	{
		return false;
	}

	FFurwardSaveStatus& Status = StatusBySlot.FindOrAdd(SlotName);
	if (Snapshot.Revision < Status.LatestRequestedRevision)
	{
		return false;
	}
	if (Snapshot.Revision == Status.LatestRequestedRevision && !Status.bLatestWriteFailed)
	{
		return true;
	}

	Status.LatestRequestedRevision = Snapshot.Revision;
	Status.bLatestWriteFailed = false;
	PendingSaves.Add(FSaveRequest{SlotName, MoveTemp(Snapshot), FGuid::NewGuid()});
	StartNextSave();
	return true;
}

FFurwardSaveStatus UFurwardSaveSubsystem::GetSaveStatus(FName SlotName) const
{
	return StatusBySlot.FindRef(SlotName);
}

void UFurwardSaveSubsystem::StartNextSave()
{
	if (bStartingSaves || bDeinitializing)
	{
		return;
	}

	// При синхронной ошибке callback очистит ActiveSave. Цикл тогда возьмёт следующий запрос.
	TGuardValue<bool> StartingGuard(bStartingSaves, true);
	while (!ActiveSave.IsSet() && !PendingSaves.IsEmpty())
	{
		ActiveSave.Emplace(MoveTemp(PendingSaves[0]));
		PendingSaves.RemoveAt(0, 1, EAllowShrinking::No);

		const FSaveRequest& Request = ActiveSave.GetValue();
		UFurwardSaveGame* SaveGame = NewObject<UFurwardSaveGame>(this);
		SaveGame->SetProfileSnapshot(Request.Snapshot);

		// Unreal сериализует SaveGame в игровом потоке. Фоновая запись получает готовые байты.
		UGameplayStatics::AsyncSaveGameToSlot(SaveGame, Request.SlotName.ToString(), LocalUserIndex,
			FAsyncSaveGameToSlotDelegate::CreateUObject(this, &UFurwardSaveSubsystem::HandleSaveFinished, Request.RequestID));
	}
}

void UFurwardSaveSubsystem::HandleSaveFinished(const FString& SlotName, int32 UserIndex, bool bSuccess, FGuid RequestID)
{
	check(IsInGameThread());

	const FName SavedSlot(*SlotName);
	if (bDeinitializing || !ActiveSave.IsSet() || UserIndex != LocalUserIndex ||
		ActiveSave.GetValue().RequestID != RequestID || ActiveSave.GetValue().SlotName != SavedSlot)
	{
		return;
	}

	const int64 WrittenRevision = ActiveSave.GetValue().Snapshot.Revision;
	FFurwardSaveStatus& Status = StatusBySlot.FindChecked(SavedSlot);
	if (bSuccess)
	{
		Status.LastSavedRevision = WrittenRevision;
	}
	if (WrittenRevision == Status.LatestRequestedRevision)
	{
		Status.bLatestWriteFailed = !bSuccess;
	}

	// Пока идёт уведомление, запросы из слушателей могут пополнить очередь. Их запуск начнётся после Broadcast().
	SaveFinished.Broadcast(SavedSlot, WrittenRevision, bSuccess);
	ActiveSave.Reset();
	StartNextSave();
}

void UFurwardSaveSubsystem::Deinitialize()
{
	bDeinitializing = true;
	PendingSaves.Reset();
	ActiveSave.Reset();
	StatusBySlot.Reset();
	OpenedProfileSlot = NAME_None;
	SaveFinished.Clear();
	Super::Deinitialize();
}
