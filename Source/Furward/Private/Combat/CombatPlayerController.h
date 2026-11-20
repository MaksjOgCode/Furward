// Copyright © 2026 Maksj.OG

 /**
  * @file     CombatPlayerController.h
  *
  * @brief    Путь команды игрока (Контроллер игрока на постоянной карте L_Combat)
  *
  * @details  Настраивает курсор и GameAndUI, выбирает камеру арены
  *           и подключает действие клика для локального игрока.
  */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CombatPlayerController.generated.h"



class ACombatArena;
class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
class UInputMappingContext;



/**
 * @class ACombatPlayerController
 *
 * @brief Подключает ввод локального игрока и выбирает камеру зарегистрированной арены L_Combat.
 */
UCLASS()
class ACombatPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	/** Назначает зарегистрированную арену целью камеры игрока */
	void UseArenaCamera(ACombatArena* Arena);
protected:
	/**
	 * Показывает курсор и сохраняет его видимым при захвате мыши.
	 * В режиме GameAndUI интерфейс первым получает возможность обработать ввод.
	 */
	void BeginPlay() override;

	/** Привязывает нажатие к обработчику и добавляет боевой контекст локальному игроку. */
	void SetupInputComponent() override;

	/** Снимает боевой контекст до завершения контроллера. */
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Принимает начало действия кликовой атаки и записывает запрос в лог. */
	void HandleCombatClick();

	/** Контекст клавиш боевой карты, выбранный в BP_Combat_PlayerController. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Input")
	TObjectPtr<UInputMappingContext> CombatMappingContext;

	/** Действие кликовой атаки из боевого контекста. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Input")
	TObjectPtr<UInputAction> CombatClickAction;

	/** Подсистема, в которой этот контроллер установил боевой контекст. */
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> CombatInputSubsystem;
};
