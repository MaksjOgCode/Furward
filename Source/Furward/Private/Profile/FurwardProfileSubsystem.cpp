// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Учитывает победы экспедиции и проверяет согласованность загружаемого прогресса.
 */

#include "Profile/FurwardProfileSubsystem.h"

bool UFurwardProfileSubsystem::TryRecordExpeditionClear(FName StageID, const FGuid& EncounterID)
{
	if (!CanRecordEncounter(StageID, EncounterID))
	{
		return false;
	}

	int32& ClearCount = Progress.ExpeditionClearsByStage.FindOrAdd(StageID);
	if (ClearCount >= RequiredExpeditionClears)
	{
		return false;
	}

	++ClearCount;
	Progress.RecordedEncounterIDs.Add(EncounterID);
	++Progress.Revision;
	return true;
}

int32 UFurwardProfileSubsystem::GetExpeditionClearCount(FName StageID) const
{
	return Progress.ExpeditionClearsByStage.FindRef(StageID);
}

bool UFurwardProfileSubsystem::IsExpeditionClearComplete(FName StageID) const
{
	return GetExpeditionClearCount(StageID) >= RequiredExpeditionClears;
}

bool UFurwardProfileSubsystem::TryCompleteExpeditionStage(FName StageID, const FGuid& EncounterID)
{
	if (!CanRecordEncounter(StageID, EncounterID) ||
		!IsExpeditionClearComplete(StageID) || IsExpeditionStageCompleted(StageID))
	{
		return false;
	}

	Progress.CompletedExpeditionStages.Add(StageID);
	Progress.RecordedEncounterIDs.Add(EncounterID);
	++Progress.Revision;
	return true;
}

bool UFurwardProfileSubsystem::IsExpeditionStageCompleted(FName StageID) const
{
	return Progress.CompletedExpeditionStages.Contains(StageID);
}

FFurwardProfileSnapshot UFurwardProfileSubsystem::CreateSnapshot() const
{
	return Progress;
}

bool UFurwardProfileSubsystem::TryRestoreSnapshot(const FFurwardProfileSnapshot& Snapshot)
{
	// В текущей версии каждая учтённая победа даёт одну ревизию и один EncounterID.
	int64 VictoryCount = Snapshot.CompletedExpeditionStages.Num();
	for (const TPair<FName, int32>& Stage : Snapshot.ExpeditionClearsByStage)
	{
		if (Stage.Key.IsNone() || Stage.Value < 0 || Stage.Value > RequiredExpeditionClears)
		{
			return false;
		}
		VictoryCount += Stage.Value;
	}

	if (VictoryCount != Snapshot.Revision || VictoryCount != Snapshot.RecordedEncounterIDs.Num())
	{
		return false;
	}

	for (const FName StageID : Snapshot.CompletedExpeditionStages)
	{
		if (Snapshot.ExpeditionClearsByStage.FindRef(StageID) != RequiredExpeditionClears)
		{
			return false;
		}
	}

	for (const FGuid& EncounterID : Snapshot.RecordedEncounterIDs)
	{
		if (!EncounterID.IsValid())
		{
			return false;
		}
	}

	Progress = Snapshot;
	return true;
}

bool UFurwardProfileSubsystem::CanRecordEncounter(FName StageID, const FGuid& EncounterID) const
{
	return !StageID.IsNone() && EncounterID.IsValid() && !Progress.RecordedEncounterIDs.Contains(EncounterID);
}
