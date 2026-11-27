// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Учитывает валюты и прогресс в одной операции профиля и проверяет загружаемый снимок.
 */

#include "Profile/FurwardProfileSubsystem.h"

bool UFurwardProfileSubsystem::TryRecordExpeditionClear(FName StageID, const FGuid& EncounterID,
	const TMap<FName, int64>& CurrencyRewards)
{
	if (!CanRecordEncounter(StageID, EncounterID, CurrencyRewards))
	{
		return false;
	}

	int32& ClearCount = Progress.ExpeditionClearsByStage.FindOrAdd(StageID);
	if (ClearCount >= RequiredExpeditionClears)
	{
		return false;
	}

	++ClearCount;
	ApplyVictory(EncounterID, CurrencyRewards);
	return true;
}

bool UFurwardProfileSubsystem::TryRecordFarmVictory(FName StageID, const FGuid& EncounterID,
	const TMap<FName, int64>& CurrencyRewards)
{
	if (!CanRecordEncounter(StageID, EncounterID, CurrencyRewards))
	{
		return false;
	}

	++Progress.FarmVictoryCount;
	ApplyVictory(EncounterID, CurrencyRewards);
	return true;
}

int64 UFurwardProfileSubsystem::GetCurrencyBalance(FName CurrencyID) const
{
	return Progress.CurrencyBalances.FindRef(CurrencyID);
}

int32 UFurwardProfileSubsystem::GetExpeditionClearCount(FName StageID) const
{
	return Progress.ExpeditionClearsByStage.FindRef(StageID);
}

bool UFurwardProfileSubsystem::IsExpeditionClearComplete(FName StageID) const
{
	return GetExpeditionClearCount(StageID) >= RequiredExpeditionClears;
}

bool UFurwardProfileSubsystem::TryCompleteExpeditionStage(FName StageID, const FGuid& EncounterID,
	const TMap<FName, int64>& CurrencyRewards)
{
	if (!CanRecordEncounter(StageID, EncounterID, CurrencyRewards) ||
		!IsExpeditionClearComplete(StageID) || IsExpeditionStageCompleted(StageID))
	{
		return false;
	}

	Progress.CompletedExpeditionStages.Add(StageID);
	ApplyVictory(EncounterID, CurrencyRewards);
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

bool UFurwardProfileSubsystem::SetActiveExpeditionStage(FName StageID)
{
	if (StageID.IsNone() || StageID == Progress.ActiveExpeditionStage || Progress.Revision == MAX_int64)
	{
		return false;
	}

	Progress.ActiveExpeditionStage = StageID;
	++Progress.Revision;
	return true;
}

bool UFurwardProfileSubsystem::TryRestoreSnapshot(const FFurwardProfileSnapshot& Snapshot)
{
	// Победа добавляет EncounterID, а выбор этапа также может увеличить ревизию.
	int64 VictoryCount = Snapshot.CompletedExpeditionStages.Num();
	for (const TPair<FName, int32>& Stage : Snapshot.ExpeditionClearsByStage)
	{
		if (Stage.Key.IsNone() || Stage.Value < 0 || Stage.Value > RequiredExpeditionClears)
		{
			return false;
		}
		VictoryCount += Stage.Value;
	}

	const int64 RecordedVictories = Snapshot.RecordedEncounterIDs.Num();
	if (Snapshot.Revision < RecordedVictories || VictoryCount > RecordedVictories ||
		Snapshot.FarmVictoryCount != RecordedVictories - VictoryCount ||
		(Snapshot.Revision != RecordedVictories && Snapshot.ActiveExpeditionStage.IsNone()))
	{
		return false;
	}

	for (const TPair<FName, int64>& Currency : Snapshot.CurrencyBalances)
	{
		if (Currency.Key.IsNone() || Currency.Value <= 0)
		{
			return false;
		}
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

bool UFurwardProfileSubsystem::CanRecordEncounter(FName StageID, const FGuid& EncounterID,
	const TMap<FName, int64>& CurrencyRewards) const
{
	if (StageID.IsNone() || !EncounterID.IsValid() || Progress.RecordedEncounterIDs.Contains(EncounterID) ||
		Progress.Revision == MAX_int64)
	{
		return false;
	}

	for (const TPair<FName, int64>& Currency : CurrencyRewards)
	{
		if (Currency.Key.IsNone() || Currency.Value <= 0 || Currency.Value > MAX_int64 - GetCurrencyBalance(Currency.Key))
		{
			return false;
		}
	}
	return true;
}

void UFurwardProfileSubsystem::ApplyVictory(const FGuid& EncounterID, const TMap<FName, int64>& CurrencyRewards)
{
	for (const TPair<FName, int64>& Currency : CurrencyRewards)
	{
		Progress.CurrencyBalances.FindOrAdd(Currency.Key) += Currency.Value;
	}
	Progress.RecordedEncounterIDs.Add(EncounterID);
	++Progress.Revision;
}
