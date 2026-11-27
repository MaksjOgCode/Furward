// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает награды для DA_Loot
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FurwardLootDefinition.generated.h"

/**
 * @brief Хранит настроенный автором набор гарантированных валют.
 *
 * Описание противника удерживает ассет. При победе сессия копирует значения в результат столкновения.
 */
UCLASS()
class UFurwardLootDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** @brief Возвращает суммы по идентификаторам валют для копирования в результат. */
	const TMap<FName, int64>& GetGuaranteedCurrencies() const { return GuaranteedCurrencies; }

private:
	/** @brief Положительное количество каждой валюты, выдаваемое за победу. */
	UPROPERTY(EditAnywhere, Category = "Rewards")
	TMap<FName, int64> GuaranteedCurrencies;
};
