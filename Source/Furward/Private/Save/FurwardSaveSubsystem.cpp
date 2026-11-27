// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Открывает слот, передаёт данные профилю и отмечает подтверждённые ревизии последовательной записи.
 */

#include "Save/FurwardSaveSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Crc.h"
#include "PlatformFeatures.h"
#include "SaveGameSystem.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
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

constexpr uint32 ProfileFileTag = 0x50535746; // FWSP
constexpr uint32 UnrealSaveTag = 0x53415647; // GVAS
constexpr int32 ProfileFileHeaderBytes = 3 * sizeof(uint32);

/** @brief Проверяет длину строки заголовка до создания FString. */
bool ReadSaveHeaderString(FMemoryReader& Reader, FString& Out)
{
	const int64 Start = Reader.Tell();
	int32 Length = 0;
	Reader << Length;
	const int64 Characters = FMath::Abs(int64(Length));
	const int64 Bytes = Characters * (Length < 0 ? sizeof(UTF16CHAR) : 1);
	if (Reader.IsError() || Characters >= NAME_SIZE || Bytes > Reader.TotalSize() - Reader.Tell())
	{
		return false;
	}
	Reader.Seek(Start);
	Reader << Out;
	return !Reader.IsError();
}

/** @brief Проверяет заголовок сохранений Furward, записанных штатным Unreal в UE5. */
bool IsFurwardSavePayload(const TArray<uint8>& Data)
{
	FMemoryReader Reader(Data, true);
	uint32 Tag = 0;
	int32 Format = 0;
	Reader << Tag << Format;
	if (Reader.IsError() || Tag != UnrealSaveTag || Format != 3)
	{
		return false;
	}

	// Формат 3 хранит версии пакета UE4 и UE5, затем FEngineVersion.
	constexpr int32 EngineVersionPrefixBytes = 2 * sizeof(int32) + 3 * sizeof(uint16) + sizeof(uint32);
	if (Reader.TotalSize() - Reader.Tell() < EngineVersionPrefixBytes)
	{
		return false;
	}
	Reader.Seek(Reader.Tell() + EngineVersionPrefixBytes);
	FString Branch;
	if (!ReadSaveHeaderString(Reader, Branch))
	{
		return false;
	}

	int32 CustomVersionFormat = 0;
	int32 CustomVersionCount = 0;
	Reader << CustomVersionFormat << CustomVersionCount;
	constexpr int32 CustomVersionBytes = 5 * sizeof(uint32);
	if (Reader.IsError() || CustomVersionFormat != 3 || CustomVersionCount < 0 ||
		int64(CustomVersionCount) * CustomVersionBytes > Reader.TotalSize() - Reader.Tell())
	{
		return false;
	}
	Reader.Seek(Reader.Tell() + int64(CustomVersionCount) * CustomVersionBytes);

	FString ClassPath;
	return ReadSaveHeaderString(Reader, ClassPath) &&
		ClassPath == UFurwardSaveGame::StaticClass()->GetPathName() && Reader.Tell() < Reader.TotalSize();
}

/** @brief Отделяет проверенный payload от контрольной суммы. Старые слоты читаются по сигнатуре UE. */
bool ReadProfileSave(const FString& Slot, TArray<uint8>& Payload)
{
	TArray<uint8> Data;
	if (!UGameplayStatics::LoadDataFromSlot(Data, Slot, UFurwardSaveSubsystem::LocalUserIndex))
	{
		return false;
	}

	FMemoryReader Reader(Data, true);
	uint32 Tag = 0;
	Reader << Tag;
	if (Reader.IsError())
	{
		return false;
	}
	if (Tag == UnrealSaveTag)
	{
		Payload = MoveTemp(Data);
		return IsFurwardSavePayload(Payload);
	}

	uint32 Size = 0;
	uint32 Checksum = 0;
	Reader << Size << Checksum;
	if (Reader.IsError() || Tag != ProfileFileTag || int64(Size) != Data.Num() - ProfileFileHeaderBytes ||
		FCrc::MemCrc32(Data.GetData() + ProfileFileHeaderBytes, Size) != Checksum)
	{
		return false;
	}

	Payload.Append(Data.GetData() + ProfileFileHeaderBytes, Size);
	return IsFurwardSavePayload(Payload);
}

bool WriteProfileSave(UFurwardSaveGame& SaveGame, TArray<uint8>& Data)
{
	TArray<uint8> Payload;
	if (!UGameplayStatics::SaveGameToMemory(&SaveGame, Payload))
	{
		return false;
	}

	FMemoryWriter Writer(Data, true);
	uint32 Tag = ProfileFileTag;
	uint32 Size = Payload.Num();
	uint32 Checksum = FCrc::MemCrc32(Payload.GetData(), Size);
	Writer << Tag << Size << Checksum;
	Writer.Serialize(Payload.GetData(), Size);
	return !Writer.IsError();
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
		TArray<uint8> Payload;
		const UFurwardSaveGame* SavedGame = ReadProfileSave(Name, Payload)
			? Cast<UFurwardSaveGame>(UGameplayStatics::LoadGameFromMemory(Payload)) : nullptr;
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
	FSaveRequest Latest{SlotName, MoveTemp(Snapshot), FGuid::NewGuid()};
	if (FSaveRequest* Queued = PendingSaves.FindByPredicate([SlotName](const FSaveRequest& Request)
		{ return Request.SlotName == SlotName; }))
	{
		*Queued = MoveTemp(Latest);
	}
	else
	{
		PendingSaves.Add(MoveTemp(Latest));
	}
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

		const FString Slot = Request.SlotName.ToString();
		const FGuid RequestID = Request.RequestID;
		TSharedRef<TArray<uint8>> Data = MakeShared<TArray<uint8>>();
		ISaveGameSystem* SaveSystem = IPlatformFeaturesModule::Get().GetSaveGameSystem();
		if (!SaveSystem || !WriteProfileSave(*SaveGame, *Data))
		{
			HandleSaveFinished(Slot, LocalUserIndex, false, RequestID);
			continue;
		}

		const FPlatformUserId User = FPlatformMisc::GetPlatformUserForUserIndex(LocalUserIndex);
		SaveSystem->SaveGameAsync(false, *Slot, User, Data,
			[WeakThis = TWeakObjectPtr<UFurwardSaveSubsystem>(this), RequestID](const FString& SavedSlot, FPlatformUserId, bool bSuccess)
			{
				if (UFurwardSaveSubsystem* Self = WeakThis.Get())
				{
					Self->HandleSaveFinished(SavedSlot, LocalUserIndex, bSuccess, RequestID);
				}
			});
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

	if (bSuccess)
	{
		UE_LOG(LogFurwardSave, Display, TEXT("Profile save confirmed: %s; Revision: %lld"), *SlotName, WrittenRevision);
	}
	else
	{
		UE_LOG(LogFurwardSave, Warning, TEXT("Profile save failed: %s; Revision: %lld"), *SlotName, WrittenRevision);
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
