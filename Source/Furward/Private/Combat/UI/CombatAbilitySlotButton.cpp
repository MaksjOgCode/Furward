// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Связывает нажатие динамической кнопки HUD с командой её слота.
 */

#include "Combat/UI/CombatAbilitySlotButton.h"

UCombatAbilitySlotButton::UCombatAbilitySlotButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InitIsFocusable(false);
}

void UCombatAbilitySlotButton::BindSlot(int32 InSlotIndex, FCombatAbilitySlotPressed InPressed)
{
	SlotIndex = InSlotIndex;
	Pressed = MoveTemp(InPressed);
	OnClicked.AddUniqueDynamic(this, &UCombatAbilitySlotButton::HandleClicked);
}

void UCombatAbilitySlotButton::HandleClicked()
{
	Pressed.ExecuteIfBound(SlotIndex);
}
