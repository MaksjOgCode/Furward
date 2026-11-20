// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает версию формата и данные профиля для сохранения Unreal.
 */

#pragma once

#include "GameFramework/SaveGame.h"
#include "Profile/FurwardProfileSnapshot.h"
#include "FurwardSaveGame.generated.h"

/**
 * @brief Хранит один снимок профиля в формате USaveGame.
 *
 * Вызывающий код создаёт отдельный экземпляр для записи. При загрузке Unreal читает
 * сохранённые поля, а владелец профиля проверяет снимок перед восстановлением.
 */
UCLASS()
class UFurwardSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** @brief Поддерживаемая версия схемы файла. Ревизия внутри снимка относится к состоянию игрока. */
	static constexpr int32 CurrentSchemaVersion = 1;

	/**
	 * @brief Заполняет новый объект копией прогресса перед передачей на запись.
	 * @param[in] Snapshot Снимок, полученный от подсистемы профиля.
	 * @pre Вызывающий код ещё не передал этот объект на запись.
	 */
	void SetProfileSnapshot(const FFurwardProfileSnapshot& Snapshot)
	{
		ProfileSnapshot = Snapshot;
		SchemaVersion = CurrentSchemaVersion;
	}

	/**
	 * @brief Даёт прочитать загруженное состояние, если версия формата поддерживается.
	 * @return Снимок, хранящийся в этом объекте, или nullptr при неподдерживаемой версии.
	 */
	const FFurwardProfileSnapshot* GetProfileSnapshot() const
	{
		return SchemaVersion == CurrentSchemaVersion ? &ProfileSnapshot : nullptr;
	}

private:
	/** @brief Версия, сохранённая в файле. Ноль означает отсутствие записанной версии. */
	UPROPERTY()
	int32 SchemaVersion = 0;

	/** @brief Значения профиля, скопированные перед началом записи. */
	UPROPERTY()
	FFurwardProfileSnapshot ProfileSnapshot;
};
