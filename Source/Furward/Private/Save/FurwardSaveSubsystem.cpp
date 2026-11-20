// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Запускает последовательные асинхронные записи и отмечает подтверждённые ревизии.
 */

#include "Save/FurwardSaveSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Save/FurwardSaveGame.h"

bool UFurwardSaveSubsystem::RequestSave(FName SlotName, FFurwardProfileSnapshot Snapshot)
{
	check(IsInGameThread());

	const FString SlotPath = SlotName.ToString();
	if (bDeinitializing || SlotName.IsNone() || Snapshot.Revision < 0 ||
		FPaths::GetCleanFilename(SlotPath) != SlotPath || !FPaths::ValidatePath(SlotPath))
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
	SaveFinished.Clear();
	Super::Deinitialize();
}
