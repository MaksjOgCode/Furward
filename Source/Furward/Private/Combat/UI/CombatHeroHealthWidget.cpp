// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Считает долю здоровья героя и переводит авторскую палитру из sRGB для UMG.
 */

#include "Combat/UI/CombatHeroHealthWidget.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

namespace
{
struct FHeroHealthPalette
{
	FColor Fill;
	FColor Lens;
};

FHeroHealthPalette ChoosePalette(float Fraction)
{
	if (Fraction > 0.70f)
	{
		return {FColor(0x7B, 0x84, 0x29), FColor(0xB8, 0xBA, 0x3A)};
	}
	if (Fraction >= 0.35f)
	{
		return {FColor(0xC8, 0x96, 0x33), FColor(0xF2, 0xC8, 0x62)};
	}
	return {FColor(0xC5, 0x3A, 0x2E), FColor(0xD9, 0x5F, 0x48)};
}
}

void UCombatHeroHealthWidget::SetHealth(float CurrentHealth, float MaxHealth)
{
	const float Maximum = FMath::IsFinite(MaxHealth) && MaxHealth > 0.0f ? MaxHealth : 0.0f;
	const float Current = FMath::IsFinite(CurrentHealth) ? FMath::Clamp(CurrentHealth, 0.0f, Maximum) : 0.0f;
	const float Fraction = Maximum > 0.0f ? Current / Maximum : 0.0f;
	const FHeroHealthPalette Palette = ChoosePalette(Fraction);

	HealthFill->SetPercent(Fraction);
	HealthFill->SetFillColorAndOpacity(FLinearColor::FromSRGBColor(Palette.Fill));
	HealthLens->SetColorAndOpacity(FLinearColor::FromSRGBColor(Palette.Lens));

	FNumberFormattingOptions Format;
	Format.SetUseGrouping(false).SetMaximumFractionalDigits(0).SetRoundingMode(ERoundingMode::HalfFromZero);
	HealthValue->SetText(FText::Format(NSLOCTEXT("FurwardCombatHUD", "HeroHealthValue", "{0}/{1}"),
		FText::AsNumber(Current, &Format), FText::AsNumber(Maximum, &Format)));
}
