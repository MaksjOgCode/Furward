// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Учитывает победы над обычными противниками и боссами экспедиции.
 */

#include "Profile/FurwardProfileSubsystem.h"

bool UFurwardProfileSubsystem::TryRecordExpeditionClear(FName StageID, const FGuid& EncounterID)
{
	if (!CanRecordEncounter(StageID, EncounterID))
	{
		return false;
	}

	int32& ClearCount = ExpeditionClearsByStage.FindOrAdd(StageID);
	if (ClearCount >= RequiredExpeditionClears)
	{
		return false;
	}

	++ClearCount;
	RecordedEncounterIDs.Add(EncounterID);
	return true;
}

int32 UFurwardProfileSubsystem::GetExpeditionClearCount(FName StageID) const
{
	return ExpeditionClearsByStage.FindRef(StageID);
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

	CompletedExpeditionStages.Add(StageID);
	RecordedEncounterIDs.Add(EncounterID);
	return true;
}

bool UFurwardProfileSubsystem::IsExpeditionStageCompleted(FName StageID) const
{
	return CompletedExpeditionStages.Contains(StageID);
}

bool UFurwardProfileSubsystem::CanRecordEncounter(FName StageID, const FGuid& EncounterID) const
{
	return !StageID.IsNone() && EncounterID.IsValid() && !RecordedEncounterIDs.Contains(EncounterID);
}
