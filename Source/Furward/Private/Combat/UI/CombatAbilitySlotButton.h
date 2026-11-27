// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает кнопку, которая передаёт HUD индекс настроенного слота способности.
 */

#pragma once

#include "Components/Button.h"
#include "CombatAbilitySlotButton.generated.h"

DECLARE_DELEGATE_OneParam(FCombatAbilitySlotPressed, int32);

/** @brief Передаёт индекс динамической кнопки слота в HUD. */
UCLASS()
class UCombatAbilitySlotButton : public UButton
{
	GENERATED_BODY()

public:
	UCombatAbilitySlotButton(const FObjectInitializer& ObjectInitializer);

	/** @brief Привязывает кнопку к слоту, сохраняя исходный индекс героя. */
	void BindSlot(int32 InSlotIndex, FCombatAbilitySlotPressed InPressed);

private:
	UFUNCTION()
	void HandleClicked();

	int32 SlotIndex = INDEX_NONE;
	FCombatAbilitySlotPressed Pressed;
};
