// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Отображает состояние сессии и передаёт контроллеру действия окон выхода и поражения.
 */

#include "Combat/CombatRootWidget.h"
#include "Combat/CombatPlayerController.h"
#include "Combat/CombatSession.h"
#include "Components/Button.h"
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
	}

	if (!BindDefeatControls() || !BindLeaveControls())
	{
		return false;
	}

	CombatSession = &Session;
	HealthListener = Session.OnParticipantHealthChanged().AddUObject(this, &UCombatRootWidget::RefreshHealth);
	if (Progression)
	{
		ProgressionListener = Session.OnProgressionChanged().AddUObject(this, &UCombatRootWidget::RefreshProgression);
	}
	if (DefeatWindow)
	{
		DefeatListener = Session.OnDefeatRecorded().AddUObject(this, &UCombatRootWidget::ShowDefeat);
	}
	if (LeaveConfirmation || DefeatWindow)
	{
		ExitSaveFailedListener = Session.OnExitSaveFailed().AddUObject(this, &UCombatRootWidget::HandleExitSaveFailed);
	}
	RefreshHealth();
	RefreshProgression();
	ShowDefeat();
	return true;
}

bool UCombatRootWidget::BindDefeatControls()
{
	if (!DefeatWindow)
	{
		return true;
	}

	DefeatEnemyHealthText = Cast<UTextBlock>(DefeatWindow->GetWidgetFromName(TEXT("DefeatEnemyHealthText")));
	DefeatProgressText = Cast<UTextBlock>(DefeatWindow->GetWidgetFromName(TEXT("DefeatProgressText")));
	EvacuationStatusText = Cast<UTextBlock>(DefeatWindow->GetWidgetFromName(TEXT("EvacuationStatusText")));
	EvacuateButton = Cast<UButton>(DefeatWindow->GetWidgetFromName(TEXT("EvacuateButton")));
	if (!DefeatEnemyHealthText || !DefeatProgressText || !EvacuationStatusText || !EvacuateButton)
	{
		return false;
	}

	EvacuateButton->OnClicked.AddUniqueDynamic(this, &UCombatRootWidget::HandleEvacuate);
	DefeatWindow->SetVisibility(ESlateVisibility::Collapsed);
	return true;
}

void UCombatRootWidget::ShowDefeat()
{
	const UCombatSession* Session = CombatSession.Get();
	if (!DefeatWindow || !Session || Session->GetPhase() != ECombatPhase::Defeated)
	{
		return;
	}

	const TOptional<FCombatDefeatSnapshot> Defeat = Session->GetDefeatSnapshot();
	if (!Defeat.IsSet())
	{
		return;
	}

	DefeatEnemyHealthText->SetText(FText::Format(LOCTEXT("DefeatEnemyHealth", "HP противника: {0}/{1}"),
		FText::AsNumber(Defeat->EnemyCurrentHealth), FText::AsNumber(Defeat->EnemyMaxHealth)));
	const FCombatProgressionSnapshot Progress = Session->GetProgressionForDisplay();
	if (Progress.RequiredClears > 0)
	{
		DefeatProgressText->SetText(FText::Format(LOCTEXT("DefeatProgress", "Зачистка: {0}/{1}"),
			FText::AsNumber(Progress.ClearedEnemies), FText::AsNumber(Progress.RequiredClears)));
	}
	DefeatProgressText->SetVisibility(Progress.RequiredClears > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	EvacuationStatusText->SetText(FText::GetEmpty());
	EvacuateButton->SetIsEnabled(true);
	if (LeaveButton)
	{
		LeaveButton->SetIsEnabled(false);
	}
	DefeatWindow->SetVisibility(ESlateVisibility::Visible);
}

bool UCombatRootWidget::BindLeaveControls()
{
	LeaveButton = Cast<UButton>(General->GetWidgetFromName(TEXT("LeaveButton")));
	if (!LeaveConfirmation)
	{
		return !LeaveButton;
	}

	ConfirmLeaveButton = Cast<UButton>(LeaveConfirmation->GetWidgetFromName(TEXT("ConfirmLeaveButton")));
	CancelLeaveButton = Cast<UButton>(LeaveConfirmation->GetWidgetFromName(TEXT("CancelLeaveButton")));
	LeaveStatusText = Cast<UTextBlock>(LeaveConfirmation->GetWidgetFromName(TEXT("LeaveStatusText")));
	if (!LeaveButton || !ConfirmLeaveButton || !CancelLeaveButton || !LeaveStatusText)
	{
		return false;
	}

	LeaveButton->OnClicked.AddUniqueDynamic(this, &UCombatRootWidget::HandleOpenLeave);
	ConfirmLeaveButton->OnClicked.AddUniqueDynamic(this, &UCombatRootWidget::HandleConfirmLeave);
	CancelLeaveButton->OnClicked.AddUniqueDynamic(this, &UCombatRootWidget::HandleCancelLeave);
	LeaveConfirmation->SetVisibility(ESlateVisibility::Collapsed);
	LeaveButton->SetIsEnabled(true);
	return true;
}

void UCombatRootWidget::HandleOpenLeave()
{
	ACombatPlayerController* Controller = GetOwningPlayer<ACombatPlayerController>();
	if (Controller && Controller->SetLeaveConfirmationOpen(true))
	{
		LeaveStatusText->SetText(LOCTEXT("LeavePrompt", "Покинуть бой? Зачистка сохранится."));
		ConfirmLeaveButton->SetIsEnabled(true);
		CancelLeaveButton->SetIsEnabled(true);
		LeaveConfirmation->SetVisibility(ESlateVisibility::Visible);
	}
}

void UCombatRootWidget::HandleCancelLeave()
{
	ACombatPlayerController* Controller = GetOwningPlayer<ACombatPlayerController>();
	if (Controller && Controller->SetLeaveConfirmationOpen(false))
	{
		LeaveConfirmation->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UCombatRootWidget::HandleConfirmLeave()
{
	CancelLeaveButton->SetIsEnabled(false);
	if (!SubmitExit(*ConfirmLeaveButton, *LeaveStatusText))
	{
		const UCombatSession* Session = CombatSession.Get();
		CancelLeaveButton->SetIsEnabled(Session && Session->GetPhase() != ECombatPhase::Leaving);
	}
}

void UCombatRootWidget::HandleEvacuate()
{
	SubmitExit(*EvacuateButton, *EvacuationStatusText);
}

bool UCombatRootWidget::SubmitExit(UButton& Action, UTextBlock& Status)
{
	Status.SetText(LOCTEXT("LeaveSaving", "Сохраняем прогресс…"));
	Action.SetIsEnabled(false);

	ACombatPlayerController* Controller = GetOwningPlayer<ACombatPlayerController>();
	if (Controller && Controller->RequestExitCombat(NAME_None))
	{
		return true;
	}

	Status.SetText(LOCTEXT("LeaveUnavailable", "Выход недоступен. Проверь слот сохранения."));
	Action.SetIsEnabled(true);
	return false;
}

void UCombatRootWidget::HandleExitSaveFailed()
{
	if (DefeatWindow && DefeatWindow->GetVisibility() == ESlateVisibility::Visible)
	{
		EvacuationStatusText->SetText(LOCTEXT("EvacuationSaveFailed", "Не удалось сохранить. Повтори эвакуацию."));
		EvacuateButton->SetIsEnabled(true);
	}
	else if (LeaveConfirmation && LeaveConfirmation->GetVisibility() == ESlateVisibility::Visible)
	{
		LeaveStatusText->SetText(LOCTEXT("LeaveSaveFailed", "Не удалось сохранить. Нажми «Выйти» для повтора."));
		ConfirmLeaveButton->SetIsEnabled(true);
		CancelLeaveButton->SetIsEnabled(false);
	}
}

void UCombatRootWidget::StopObservingSession()
{
	if (UCombatSession* Session = CombatSession.Get())
	{
		Session->OnParticipantHealthChanged().Remove(HealthListener);
		Session->OnProgressionChanged().Remove(ProgressionListener);
		Session->OnDefeatRecorded().Remove(DefeatListener);
		Session->OnExitSaveFailed().Remove(ExitSaveFailedListener);
		Session->SetLeaveConfirmationOpen(false);
	}
	if (LeaveButton)
	{
		LeaveButton->OnClicked.RemoveDynamic(this, &UCombatRootWidget::HandleOpenLeave);
	}
	if (ConfirmLeaveButton)
	{
		ConfirmLeaveButton->OnClicked.RemoveDynamic(this, &UCombatRootWidget::HandleConfirmLeave);
	}
	if (CancelLeaveButton)
	{
		CancelLeaveButton->OnClicked.RemoveDynamic(this, &UCombatRootWidget::HandleCancelLeave);
	}
	if (EvacuateButton)
	{
		EvacuateButton->OnClicked.RemoveDynamic(this, &UCombatRootWidget::HandleEvacuate);
	}
	DefeatListener.Reset();
	ExitSaveFailedListener.Reset();
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
