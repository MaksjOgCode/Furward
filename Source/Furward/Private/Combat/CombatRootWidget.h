// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Связывает WBP_Root_Combat с данными здоровья и экспедиции боевой сессии.
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatRootWidget.generated.h"

class UCombatSession;
class UProgressBar;
class UTextBlock;

/** @brief Подписывается на сессию и обновляет здоровье участников и прогресс экспедиции. */
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

	/** @brief Слабая ссылка на сессию. Её удерживает GameMode. */
	TWeakObjectPtr<UCombatSession> CombatSession;

	/** @brief Подписка на изменения здоровья участников в текущей сессии. */
	FDelegateHandle HealthListener;

	/** @brief Подписка на смену этапа, зачистку и состояние босса. */
	FDelegateHandle ProgressionListener;
};
