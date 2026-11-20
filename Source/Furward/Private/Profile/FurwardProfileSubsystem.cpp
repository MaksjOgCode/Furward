// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Применяет однократное изменение зачистки экспедиции в профиле.
 */

#include "Profile/FurwardProfileSubsystem.h"

bool UFurwardProfileSubsystem::TryRecordExpeditionClear(FName StageID, const FGuid& EncounterID)
{
	if (StageID.IsNone() || !EncounterID.IsValid() || RecordedEncounterIDs.Contains(EncounterID))
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
