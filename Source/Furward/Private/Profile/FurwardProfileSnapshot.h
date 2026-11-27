// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает копию баланса валют и прогресса профиля для записи и загрузки.
 */

#pragma once

#include "CoreMinimal.h"
#include "FurwardProfileSnapshot.generated.h"

/**
 * @brief Содержит баланс валют, значения прогресса и номер ревизии одного профиля.
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

	/** @brief Баланс каждой полученной валюты по идентификатору контента. */
	UPROPERTY()
	TMap<FName, int64> CurrencyBalances;

	/** @brief Учтённые победы в фарме. Они увеличивают ревизию без изменения зачистки. */
	UPROPERTY()
	int64 FarmVictoryCount = 0;

	/** @brief Последний выбранный этап. None означает начальный этап экспедиции. */
	UPROPERTY()
	FName ActiveExpeditionStage;

	/** @brief Ревизия профиля. Увеличивается после победы или выбора этапа. */
	UPROPERTY()
	int64 Revision = 0;
};
