// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает копию текущего прогресса профиля для записи и загрузки.
 */

#pragma once

#include "CoreMinimal.h"
#include "FurwardProfileSnapshot.generated.h"

/**
 * @brief Содержит значения прогресса одного профиля и номер его ревизии.
 *
 * Подсистема профиля возвращает копию по запросу сохранения. Снимок можно хранить
 * и сериализовать отдельно от GameInstance, активного мира и боевой сессии.
 */
USTRUCT()
struct FFurwardProfileSnapshot
{
	GENERATED_BODY()

	/** @brief Количество учтённых обычных побед по имени строки боевого этапа. */
	UPROPERTY()
	TMap<FName, int32> ExpeditionClearsByStage;

	/** @brief Этапы, на которых профиль засчитал победу над боссом. */
	UPROPERTY()
	TSet<FName> CompletedExpeditionStages;

	/** @brief Идентификаторы всех засчитанных побед для повторной проверки после загрузки. */
	UPROPERTY()
	TSet<FGuid> RecordedEncounterIDs;

	/** @brief Номер состояния профиля, увеличиваемый при каждом успешном применении результата. */
	UPROPERTY()
	int64 Revision = 0;
};
