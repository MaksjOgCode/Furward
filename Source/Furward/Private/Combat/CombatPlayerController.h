// Copyright © 2026 Maksj.OG

 /**
  * @file     CombatPlayerController.h
  *
  * @brief    Путь команды игрока (Контроллер игрока на постоянной карте L_Combat)
  *
  * @details  Настраивает курсор и режим ввода GameAndUI. Контроллер выбирает
  *		      камеру зарегистрированной арены, различает Hit, Miss и Ignore
  *           и передаёт боевые команды сессии.
  */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CombatPlayerController.generated.h"



class ACombatArena;



/**
 * @class ACombatPlayerController
 *
 * @brief Настраивает курсорб режим ввода игрока и выбирает камеру зарегистрированной арены L_Combat
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

	/** Записывает завершение игры в лог перед вызовом базового EndPlay() */
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
