// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Связывает WBP_Root_Combat и панель здоровья участников с боевой сессией.
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatRootWidget.generated.h"

class UCombatSession;
class UProgressBar;
class UTextBlock;

/** @brief Подписывается на сессию и обновляет здоровье героя и врага в WBP_Combat_General. */
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

	/** @brief Слабая ссылка на сессию. Её удерживает GameMode. */
	TWeakObjectPtr<UCombatSession> CombatSession;

	/** @brief Подписка на изменения здоровья участников в текущей сессии. */
	FDelegateHandle HealthListener;
};
