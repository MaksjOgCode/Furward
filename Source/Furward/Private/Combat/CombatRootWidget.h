// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Связывает WBP_Root_Combat с данными сессии и направляет команды выхода и эвакуации в контроллер.
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatRootWidget.generated.h"

class UButton;
class UCombatSession;
class UProgressBar;
class UTextBlock;

/** @brief Обновляет HUD, показывает подтверждение выхода и итоги поражения. */
UCLASS(Abstract)
class UCombatRootWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Находит элементы панели, подписывается и сразу отображает текущее состояние.
	 * @param[in] Session Сессия мира, в котором контроллер создал виджет.
	 * @return true, когда все элементы панели найдены и обновляются.
	 */
	bool ObserveSession(UCombatSession& Session);

	/** @brief Снимает подписку при выходе, удалении виджета или смене сессии. */
	void StopObservingSession();

protected:
	/** @brief Снимает подписку при уничтожении экранного представления. */
	void NativeDestruct() override;

private:
	/** @brief Читает состояние участников после уведомления и обновляет текст и полосы. */
	void RefreshHealth();

	/** @brief Отображает активный этап, зачистку и состояние босса по данным сессии. */
	void RefreshProgression();

	/** @brief Отображает HP противника и зачистку по событию поражения. */
	void ShowDefeat();

	/** @brief Находит элементы окна поражения и подписывается на кнопку эвакуации. */
	bool BindDefeatControls();

	/** @brief Проверяет элементы подтверждения выхода и подписывается на нажатия. */
	bool BindLeaveControls();

	/** @brief Открывает модальное окно и приостанавливает бой. */
	UFUNCTION()
	void HandleOpenLeave();

	/** @brief Снимает приостановку и возвращает к прежнему столкновению. */
	UFUNCTION()
	void HandleCancelLeave();

	/** @brief Показывает ожидание и передаёт подтверждение выхода контроллеру. */
	UFUNCTION()
	void HandleConfirmLeave();

	/** @brief Запрашивает эвакуацию через общий путь сохранения перед выходом. */
	UFUNCTION()
	void HandleEvacuate();

	/**
	 * @brief Показывает ожидание записи и блокирует кнопку до результата.
	 * @param[in,out] Action Кнопка выхода или эвакуации.
	 * @param[in,out] Status Строка состояния активного окна.
	 * @return true, если контроллер принял запрос.
	 */
	bool SubmitExit(UButton& Action, UTextBlock& Status);

	/** @brief Показывает ошибку записи в активном окне и разрешает повтор. */
	void HandleExitSaveFailed();

	/** @brief Вложенный WBP_Combat_General с именем General в дереве WBP_Root_Combat. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUserWidget> General;

	/** @brief Числовое здоровье героя из панели General. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HeroHealthText;

	/** @brief Числовое здоровье врага из панели General. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EnemyHealthText;

	/** @brief Доля HP героя из панели General. */
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> HeroHealthBar;

	/** @brief Доля HP врага из панели General. */
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> EnemyHealthBar;

	/** @brief Вложенная панель WBP_Combat_Progression в General. */
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> Progression;

	/** @brief Имя активного этапа в панели зачистки. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StageText;

	/** @brief Числовой счётчик зачистки в панели. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ExpeditionClearText;

	/** @brief Заполнение полосы зачистки от нуля до порога появления босса. */
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> ExpeditionClearBar;

	/** @brief Доступность, появление или победа над боссом. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BossStatusText;

	/** @brief Модальное окно WBP_Combat_Leave в корневом HUD. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UUserWidget> LeaveConfirmation;

	/** @brief Кнопка открытия окна в General. */
	UPROPERTY(Transient)
	TObjectPtr<UButton> LeaveButton;

	/** @brief Подтверждение выхода или повтор записи. */
	UPROPERTY(Transient)
	TObjectPtr<UButton> ConfirmLeaveButton;

	/** @brief Закрывает окно и продолжает бой до принятия команды выхода. */
	UPROPERTY(Transient)
	TObjectPtr<UButton> CancelLeaveButton;

	/** @brief Вопрос, ожидание или причина ошибки записи. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LeaveStatusText;

	/** @brief Вложенный WBP_Combat_Defeat в корневом HUD. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UUserWidget> DefeatWindow;

	/** @brief Показывает HP противника на момент поражения. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DefeatEnemyHealthText;

	/** @brief Показывает зачистку текущего этапа на момент поражения. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DefeatProgressText;

	/** @brief Показывает состояние записи при эвакуации. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EvacuationStatusText;

	/** @brief Начинает эвакуацию или повторяет запись после ошибки. */
	UPROPERTY(Transient)
	TObjectPtr<UButton> EvacuateButton;

	/** @brief Слабая ссылка на сессию. Её удерживает GameMode. */
	TWeakObjectPtr<UCombatSession> CombatSession;

	/** @brief Подписка на изменения здоровья участников в текущей сессии. */
	FDelegateHandle HealthListener;

	/** @brief Подписка на смену этапа, зачистку и состояние босса. */
	FDelegateHandle ProgressionListener;

	/** @brief Подписка на поражение героя после захвата данных об итогах. */
	FDelegateHandle DefeatListener;

	/** @brief Подписка на ошибку записи перед выходом или эвакуацией. */
	FDelegateHandle ExitSaveFailedListener;
};
