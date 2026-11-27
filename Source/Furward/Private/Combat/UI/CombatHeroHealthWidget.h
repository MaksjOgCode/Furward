// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Отображает здоровье героя на шкале и выбирает общий цвет шкалы и линзы.
 */

#pragma once

#include "Blueprint/UserWidget.h"
#include "CombatHeroHealthWidget.generated.h"

class UImage;
class UProgressBar;
class UTextBlock;

/** @brief Получает значения от корневого HUD и обновляет оформление WBP_Combat_HeroHealth. */
UCLASS(Abstract)
class UCombatHeroHealthWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** @brief Обновляет число, видимую долю полосы и цвета по одному снимку здоровья. */
	void SetHealth(float CurrentHealth, float MaxHealth);

private:
	/** @brief Заполнение с альфа-маской, обрезаемое слева направо. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthFill;

	/** @brief Тонируемая вставка лампы внутри ободка корпуса. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> HealthLens;

	/** @brief Текущее и максимальное HP по центру шкалы. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HealthValue;
};
