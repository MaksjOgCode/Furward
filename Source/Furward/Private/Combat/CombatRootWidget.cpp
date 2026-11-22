// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Отображает состояние сессии и передаёт контроллеру действия слотов способностей, продолжения, смены режима и выхода.
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

	CurrencyBalanceText = Cast<UTextBlock>(General->GetWidgetFromName(TEXT("CurrencyBalanceText")));
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

		NextStageButton = Cast<UButton>(Progression->GetWidgetFromName(TEXT("NextStageButton")));
		NextStageStatusText = Cast<UTextBlock>(Progression->GetWidgetFromName(TEXT("NextStageStatusText")));
		if (NextStageButton)
		{
			NextStageButton->OnClicked.AddUniqueDynamic(this, &UCombatRootWidget::HandleNextStage);
		}
	}

	Farm = Cast<UUserWidget>(General->GetWidgetFromName(TEXT("Farm")));
	FarmStageText = Farm ? Cast<UTextBlock>(Farm->GetWidgetFromName(TEXT("FarmStageText"))) : nullptr;
	ReturnToExpeditionButton = Farm ? Cast<UButton>(Farm->GetWidgetFromName(TEXT("ReturnToExpeditionButton"))) : nullptr;
	FarmStatusText = Farm ? Cast<UTextBlock>(Farm->GetWidgetFromName(TEXT("FarmStatusText"))) : nullptr;
	if ((Farm && !FarmStageText) || (ReturnToExpeditionButton && !FarmStatusText) ||
		!BindDefeatControls() || !BindLeaveControls())
	{
		return false;
	}

	if (ReturnToExpeditionButton)
	{
		ReturnToExpeditionButton->OnClicked.AddUniqueDynamic(this, &UCombatRootWidget::HandleReturnToExpedition);
	}

	CombatSession = &Session;
	BindAbilitySlotControls();
	HealthListener = Session.OnParticipantHealthChanged().AddUObject(this, &UCombatRootWidget::RefreshHealth);
	if (CurrencyBalanceText && !DisplayCurrencyID.IsNone())
	{
		CurrencyListener = Session.OnCurrencyBalancesChanged().AddUObject(this, &UCombatRootWidget::RefreshCurrencyBalance);
	}
	if (Progression || Farm)
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
	RefreshCurrencyBalance();
	RefreshAbilitySlots();
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
	EnterFarmButton = Cast<UButton>(DefeatWindow->GetWidgetFromName(TEXT("EnterFarmButton")));
	if (EnterFarmButton)
	{
		EnterFarmButton->OnClicked.AddUniqueDynamic(this, &UCombatRootWidget::HandleEnterFarm);
	}
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
	if (EnterFarmButton)
	{
		EnterFarmButton->SetVisibility(Session->CanEnterFarm() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		EnterFarmButton->SetIsEnabled(true);
	}
	if (LeaveButton)
	{
		LeaveButton->SetIsEnabled(false);
	}
	if (ReturnToExpeditionButton)
	{
		ReturnToExpeditionButton->SetIsEnabled(false);
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
	if (SubmitExit(*EvacuateButton, *EvacuationStatusText) && EnterFarmButton)
	{
		EnterFarmButton->SetIsEnabled(false);
	}
}

void UCombatRootWidget::HandleEnterFarm()
{
	ACombatPlayerController* Controller = GetOwningPlayer<ACombatPlayerController>();
	if (!Controller || !Controller->RequestEnterFarm())
	{
		EvacuationStatusText->SetText(LOCTEXT("EnterFarmFailed", "Не удалось начать фарм. Повтори попытку."));
		return;
	}

	DefeatWindow->SetVisibility(ESlateVisibility::Collapsed);
	if (LeaveButton)
	{
		LeaveButton->SetIsEnabled(true);
	}
}

void UCombatRootWidget::HandleReturnToExpedition()
{
	ACombatPlayerController* Controller = GetOwningPlayer<ACombatPlayerController>();
	if (Controller && Controller->RequestReturnToExpedition())
	{
		return;
	}

	FarmStatusText->SetText(LOCTEXT("ReturnToExpeditionFailed", "Не удалось продолжить экспедицию. Повтори попытку."));
	FarmStatusText->SetVisibility(ESlateVisibility::HitTestInvisible);
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
		Session->OnCurrencyBalancesChanged().Remove(CurrencyListener);
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
	if (NextStageButton)
	{
		NextStageButton->OnClicked.RemoveDynamic(this, &UCombatRootWidget::HandleNextStage);
	}
	if (EnterFarmButton)
	{
		EnterFarmButton->OnClicked.RemoveDynamic(this, &UCombatRootWidget::HandleEnterFarm);
	}
	if (ReturnToExpeditionButton)
	{
		ReturnToExpeditionButton->OnClicked.RemoveDynamic(this, &UCombatRootWidget::HandleReturnToExpedition);
	}
	for (const FAbilitySlotControls& Controls : AbilityControls)
	{
		if (UButton* Button = Controls.Button.Get())
		{
			Button->OnClicked.RemoveAll(this);
			Button->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	AbilityControls.Reset();

	DefeatListener.Reset();
	ExitSaveFailedListener.Reset();
	HealthListener.Reset();
	CurrencyListener.Reset();
	ProgressionListener.Reset();
	CombatSession.Reset();
}

void UCombatRootWidget::BindAbilitySlotControls()
{
	BindAbilitySlot(TEXT("AbilitySlotOneButton"), TEXT("AbilitySlotOneText"),
		GET_FUNCTION_NAME_CHECKED(UCombatRootWidget, HandleUseAbilitySlotOne));
	BindAbilitySlot(TEXT("AbilitySlotTwoButton"), TEXT("AbilitySlotTwoText"),
		GET_FUNCTION_NAME_CHECKED(UCombatRootWidget, HandleUseAbilitySlotTwo));
}

void UCombatRootWidget::BindAbilitySlot(FName ButtonName, FName TextName, FName ClickHandler)
{
	FAbilitySlotControls& Controls = AbilityControls.AddDefaulted_GetRef();
	Controls.Label = Cast<UTextBlock>(General->GetWidgetFromName(TextName));
	if (UButton* Button = Cast<UButton>(General->GetWidgetFromName(ButtonName)))
	{
		Controls.Button = Button;
		Button->SetVisibility(ESlateVisibility::Collapsed);
		if (Controls.Label.IsValid())
		{
			FScriptDelegate Listener;
			Listener.BindUFunction(this, ClickHandler);
			Button->OnClicked.AddUnique(Listener);
		}
	}
}

void UCombatRootWidget::HandleUseAbilitySlotOne()
{
	RequestAbilitySlot(0);
}

void UCombatRootWidget::HandleUseAbilitySlotTwo()
{
	RequestAbilitySlot(1);
}

void UCombatRootWidget::RequestAbilitySlot(int32 SlotIndex)
{
	if (ACombatPlayerController* Controller = GetOwningPlayer<ACombatPlayerController>())
	{
		Controller->RequestUseAbilitySlot(SlotIndex);
		RefreshAbilitySlots();
	}
}

void UCombatRootWidget::RefreshAbilitySlots()
{
	const UCombatSession* Session = CombatSession.Get();
	if (!Session)
	{
		return;
	}

	for (int32 SlotIndex = 0; SlotIndex < AbilityControls.Num(); ++SlotIndex)
	{
		FAbilitySlotControls& Controls = AbilityControls[SlotIndex];
		UButton* Button = Controls.Button.Get();
		UTextBlock* Label = Controls.Label.Get();
		if (!Button || !Label)
		{
			continue;
		}

		const TOptional<FCombatAbilitySlotSnapshot> AbilitySlot = Session->GetAbilitySlotForDisplay(SlotIndex);
		const ESlateVisibility SlotVisibility = AbilitySlot.IsSet() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
		if (Button->GetVisibility() != SlotVisibility)
		{
			Button->SetVisibility(SlotVisibility);
		}
		if (!AbilitySlot.IsSet())
		{
			continue;
		}

		const int32 CooldownSeconds = FMath::CeilToInt(AbilitySlot->CooldownRemaining);
		const int32 EffectSeconds = FMath::CeilToInt(AbilitySlot->EffectRemaining);
		if (CooldownSeconds != Controls.ShownCooldownSeconds || EffectSeconds != Controls.ShownEffectSeconds ||
			!Controls.ShownName.EqualTo(AbilitySlot->DisplayName))
		{
			const FText Name = AbilitySlot->DisplayName.IsEmpty() ? LOCTEXT("DefaultAbilityName", "Способность") : AbilitySlot->DisplayName;
			const FText Title = FText::Format(LOCTEXT("AbilitySlotTitle", "{0} [{1}]"), Name, FText::AsNumber(SlotIndex + 1));
			if (EffectSeconds > 0 && CooldownSeconds > 0)
			{
				Label->SetText(FText::Format(LOCTEXT("AbilitySlotEffectAndCooldown", "{0}\nАктивно {1} с · Кулдаун {2} с"),
					Title, FText::AsNumber(EffectSeconds), FText::AsNumber(CooldownSeconds)));
			}
			else if (EffectSeconds > 0)
			{
				Label->SetText(FText::Format(LOCTEXT("AbilitySlotEffect", "{0}\nАктивно {1} с"), Title, FText::AsNumber(EffectSeconds)));
			}
			else
			{
				Label->SetText(CooldownSeconds > 0
					? FText::Format(LOCTEXT("AbilitySlotCooldown", "{0}\nКулдаун {1} с"), Title, FText::AsNumber(CooldownSeconds))
					: Title);
			}

			Controls.ShownCooldownSeconds = CooldownSeconds;
			Controls.ShownEffectSeconds = EffectSeconds;
			Controls.ShownName = AbilitySlot->DisplayName;
		}

		// Оформление показывает состояние, а кнопка продолжает перехватывать клик до проверки сессией.
		FLinearColor Color(0.22f, 0.25f, 0.30f);
		if (EffectSeconds > 0)
		{
			Color = FLinearColor(0.48f, 0.38f, 0.16f);
		}
		else if (AbilitySlot->bCanActivate)
		{
			Color = FLinearColor(0.24f, 0.56f, 0.37f);
		}
		if (Button->GetBackgroundColor() != Color)
		{
			Button->SetBackgroundColor(Color);
		}
	}
}

void UCombatRootWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshAbilitySlots();
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

void UCombatRootWidget::RefreshCurrencyBalance()
{
	const UCombatSession* Session = CombatSession.Get();
	if (!CurrencyBalanceText || !Session)
	{
		return;
	}
	if (DisplayCurrencyID.IsNone())
	{
		CurrencyBalanceText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	CurrencyBalanceText->SetText(FText::Format(LOCTEXT("CurrencyBalance", "{0}  {1}"),
		FText::FromName(DisplayCurrencyID), FText::AsNumber(Session->GetCurrencyBalanceForDisplay(DisplayCurrencyID))));
	CurrencyBalanceText->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UCombatRootWidget::RefreshProgression()
{
	const UCombatSession* Session = CombatSession.Get();
	if (!Session)
	{
		return;
	}

	const FCombatProgressionSnapshot Snapshot = Session->GetProgressionForDisplay();
	const bool bFarmMode = Session->GetMode() == ECombatMode::Farm;
	if (Farm)
	{
		const bool bShowFarm = bFarmMode && !Snapshot.StageID.IsNone();
		Farm->SetVisibility(bShowFarm ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (bShowFarm)
		{
			FarmStageText->SetText(FText::Format(LOCTEXT("FarmStage", "Этап {0}"), FText::FromName(Snapshot.StageID)));
			if (ReturnToExpeditionButton)
			{
				ReturnToExpeditionButton->SetIsEnabled(Session->CanReturnToExpedition());
				FarmStatusText->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}

	if (!Progression)
	{
		return;
	}

	if (bFarmMode || Snapshot.StageID.IsNone() || Snapshot.RequiredClears <= 0)
	{
		Progression->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	StageText->SetText(FText::Format(LOCTEXT("Stage", "Этап {0}"), FText::FromName(Snapshot.StageID)));
	ExpeditionClearText->SetText(FText::Format(LOCTEXT("ExpeditionClear", "Зачистка {0}/{1}"),
		FText::AsNumber(Snapshot.ClearedEnemies), FText::AsNumber(Snapshot.RequiredClears)));
	ExpeditionClearBar->SetPercent(static_cast<float>(Snapshot.ClearedEnemies) / Snapshot.RequiredClears);
	BossStatusText->SetText(GetBossStatusText(Snapshot.BossStatus));
	Progression->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	RefreshNextStage(Snapshot.NextStageState);
}

void UCombatRootWidget::RefreshNextStage(ECombatNextStageState State)
{
	if (!NextStageButton || !NextStageStatusText)
	{
		return;
	}

	const bool bAvailable = State != ECombatNextStageState::Unavailable && State != ECombatNextStageState::EnvironmentLoading;
	NextStageButton->SetVisibility(bAvailable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	NextStageButton->SetIsEnabled(bAvailable && State != ECombatNextStageState::Saving);
	if (UTextBlock* Label = Cast<UTextBlock>(NextStageButton->GetContent()))
	{
		Label->SetText(State == ECombatNextStageState::EnvironmentFailed
			? LOCTEXT("RetryEnvironment", "Повторить загрузку")
			: LOCTEXT("NextStage", "Следующий этап"));
	}

	FText Message;
	switch (State)
	{
	case ECombatNextStageState::Saving:
		Message = LOCTEXT("NextStageSaving", "Сохраняем прогресс…");
		break;
	case ECombatNextStageState::SaveFailed:
		Message = LOCTEXT("NextStageSaveFailed", "Не удалось сохранить. Повтори переход.");
		break;
	case ECombatNextStageState::StartFailed:
		Message = LOCTEXT("NextStageStartFailed", "Не удалось начать следующий этап. Повтори переход.");
		break;
	case ECombatNextStageState::EnvironmentLoading:
		Message = LOCTEXT("EnvironmentLoading", "Загружаем окружение…");
		break;
	case ECombatNextStageState::EnvironmentFailed:
		Message = LOCTEXT("EnvironmentFailed", "Не удалось загрузить арену. Повтори загрузку или покинь бой.");
		break;
	default:
		break;
	}

	NextStageStatusText->SetText(Message);
	NextStageStatusText->SetVisibility(Message.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UCombatRootWidget::HandleNextStage()
{
	ACombatPlayerController* Controller = GetOwningPlayer<ACombatPlayerController>();
	if (Controller && Controller->RequestNextStage(NAME_None))
	{
		return;
	}

	if (NextStageStatusText)
	{
		NextStageStatusText->SetText(LOCTEXT("NextStageUnavailable", "Переход недоступен. Проверь слот сохранения и настройки этапа."));
		NextStageStatusText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

#undef LOCTEXT_NAMESPACE
