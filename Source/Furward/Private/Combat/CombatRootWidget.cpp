// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Обновляет здоровье и прогресс экспедиции по событиям боевой сессии.
 */

#include "Combat/CombatRootWidget.h"
#include "Combat/CombatSession.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

#define LOCTEXT_NAMESPACE "FurwardCombatHUD"

namespace
{
/** @brief Обновляет индикатор участника или прячет его до получения начального HP. */
void UpdateHealthIndicator(UTextBlock& Text, UProgressBar& Bar, const FCombatParticipantHealth& Health, const FText& Pattern)
{
	if (Health.Maximum <= 0.0f)
	{
		Text.SetVisibility(ESlateVisibility::Collapsed);
		Bar.SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	Text.SetText(FText::Format(Pattern, FText::AsNumber(Health.Current), FText::AsNumber(Health.Maximum)));
	Bar.SetPercent(Health.Current / Health.Maximum);
	Text.SetVisibility(ESlateVisibility::HitTestInvisible);
	Bar.SetVisibility(ESlateVisibility::HitTestInvisible);
}

/** @brief Подбирает локализованную подпись к состоянию босса. */
FText GetBossStatusText(ECombatBossStatus Status)
{
	switch (Status)
	{
	case ECombatBossStatus::Available:
		return LOCTEXT("BossAvailable", "Босс открыт");
	case ECombatBossStatus::OnArena:
		return LOCTEXT("BossOnArena", "Босс на арене");
	case ECombatBossStatus::Defeated:
		return LOCTEXT("BossDefeated", "Босс побеждён");
	default:
		return LOCTEXT("BossLocked", "Босс закрыт");
	}
}
}

bool UCombatRootWidget::ObserveSession(UCombatSession& Session)
{
	StopObservingSession();
	if (!General)
	{
		return false;
	}

	HeroHealthText = Cast<UTextBlock>(General->GetWidgetFromName(TEXT("HeroHealthText")));
	EnemyHealthText = Cast<UTextBlock>(General->GetWidgetFromName(TEXT("EnemyHealthText")));
	HeroHealthBar = Cast<UProgressBar>(General->GetWidgetFromName(TEXT("HeroHealthBar")));
	EnemyHealthBar = Cast<UProgressBar>(General->GetWidgetFromName(TEXT("EnemyHealthBar")));
	if (!HeroHealthText || !EnemyHealthText || !HeroHealthBar || !EnemyHealthBar)
	{
		return false;
	}

	Progression = Cast<UUserWidget>(General->GetWidgetFromName(TEXT("Progression")));
	if (Progression)
	{
		StageText = Cast<UTextBlock>(Progression->GetWidgetFromName(TEXT("StageText")));
		ExpeditionClearText = Cast<UTextBlock>(Progression->GetWidgetFromName(TEXT("ExpeditionClearText")));
		ExpeditionClearBar = Cast<UProgressBar>(Progression->GetWidgetFromName(TEXT("ExpeditionClearBar")));
		BossStatusText = Cast<UTextBlock>(Progression->GetWidgetFromName(TEXT("BossStatusText")));
		if (!StageText || !ExpeditionClearText || !ExpeditionClearBar || !BossStatusText)
		{
			return false;
		}
		ProgressionListener = Session.OnProgressionChanged().AddUObject(this, &UCombatRootWidget::RefreshProgression);
	}

	CombatSession = &Session;
	HealthListener = Session.OnParticipantHealthChanged().AddUObject(this, &UCombatRootWidget::RefreshHealth);
	RefreshHealth();
	RefreshProgression();
	return true;
}

void UCombatRootWidget::StopObservingSession()
{
	if (UCombatSession* Session = CombatSession.Get())
	{
		Session->OnParticipantHealthChanged().Remove(HealthListener);
		Session->OnProgressionChanged().Remove(ProgressionListener);
	}
	HealthListener.Reset();
	ProgressionListener.Reset();
	CombatSession.Reset();
}

void UCombatRootWidget::NativeDestruct()
{
	StopObservingSession();
	Super::NativeDestruct();
}

void UCombatRootWidget::RefreshHealth()
{
	if (const UCombatSession* Session = CombatSession.Get())
	{
		UpdateHealthIndicator(*HeroHealthText, *HeroHealthBar, Session->GetHeroHealthForDisplay(), LOCTEXT("HeroHP", "Герой  {0}/{1}"));
		UpdateHealthIndicator(*EnemyHealthText, *EnemyHealthBar, Session->GetEnemyHealthForDisplay(), LOCTEXT("EnemyHP", "Противник  {0}/{1}"));
	}
}

void UCombatRootWidget::RefreshProgression()
{
	const UCombatSession* Session = CombatSession.Get();
	if (!Session || !Progression)
	{
		return;
	}

	const FCombatProgressionSnapshot Snapshot = Session->GetProgressionForDisplay();
	if (Snapshot.StageID.IsNone() || Snapshot.RequiredClears <= 0)
	{
		Progression->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	StageText->SetText(FText::Format(LOCTEXT("Stage", "Этап {0}"), FText::FromName(Snapshot.StageID)));
	ExpeditionClearText->SetText(FText::Format(LOCTEXT("ExpeditionClear", "Зачистка {0}/{1}"),
		FText::AsNumber(Snapshot.ClearedEnemies), FText::AsNumber(Snapshot.RequiredClears)));
	ExpeditionClearBar->SetPercent(static_cast<float>(Snapshot.ClearedEnemies) / Snapshot.RequiredClears);
	BossStatusText->SetText(GetBossStatusText(Snapshot.BossStatus));
	Progression->SetVisibility(ESlateVisibility::HitTestInvisible);
}

#undef LOCTEXT_NAMESPACE
