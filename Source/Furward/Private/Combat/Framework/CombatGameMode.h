// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает, как игровой режим L_Combat создаёт и завершает боевую сессию.
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameFramework/GameModeBase.h"
#include "UObject/ObjectPtr.h"
#include "UObject/SoftObjectPtr.h"
#include "Templates/SubclassOf.h"
#include "CombatGameMode.generated.h"



class UCombatRulesDefinition;
class UCombatSession;
class ACombatHero;



/**
 * @brief Владеет одной UCombatSession на время посещения карты L_Combat.
 *
 * Unreal создаёт экземпляр BP_Combat_GameMode для игрового мира.
 * В Class Defaults задают класс героя, начальный этап, боевые правила и слот профиля.
 */
UCLASS()
class ACombatGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	/**
	 * @brief Создаёт сессию и передаёт ей класс героя до начала игры.
	 *
	 * Unreal вызывает метод при инициализации карты. Сессия пишет Initialize в лог.
	 * @param[in] MapName Имя текущей карты.
	 * @param[in] Options Параметры запуска карты из URL.
	 * @param[out] ErrorMessage Сообщение об ошибке инициализации от базового GameMode.
	 * @post GameMode удерживает созданную сессию.
	 */
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	/**
	 * @brief Даёт контроллеру доступ к сессии при обработке клика.
	 * @return Сессия текущего посещения L_Combat или nullptr до InitGame() и после EndPlay().
	 */
	UCombatSession* GetCombatSession();

protected:
	/**
	 * @brief Передаёт сессии начальный этап, когда Unreal запускает карту.
	 *
	 * GameMode открывает слот профиля и передаёт сессии боевые правила до загрузки этапа.
	 * @pre InitGame() создал CombatSession.
	 * @see UCombatSession::RequestInitialStage
	 */
	virtual void BeginPlay() override;

	/**
	 * @brief Завершает сессию, когда Unreal заканчивает игру на карте.
	 * @param[in] EndPlayReason Причина завершения, которую получает базовый GameMode.
	 * @post Ссылка на CombatSession очищена.
	 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** @brief Поручает подсистеме сохранений загрузить профиль до запуска этапа. */
	bool TryOpenCombatProfile(FName Slot);

	/**
	 * @brief Ссылка на таблицу и имя начальной строки из Class Defaults BP_Combat_GameMode.
	 *
	 * GameMode удерживает выбранную таблицу во время посещения L_Combat.
	 * Если строка не назначена, сессия сообщает HUD об ошибке запуска.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Stage", meta = (RowType = "/Script/Furward.CombatStageRow"))
	FDataTableRowHandle InitialStage;

	/**
	 * @brief Ассет общих правил боя, доступный для замены в Class Defaults.
	 *
	 * Мягкая ссылка указывает на DA_Combat_Rules по умолчанию. GameMode загружает ассет перед подготовкой этапа.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Rules")
	TSoftObjectPtr<UCombatRulesDefinition> CombatRules{
		FSoftObjectPath(TEXT("/Game/Combat-System/Data/DA_Combat_Rules.DA_Combat_Rules"))};

	/** @brief Слот профиля. Параметр карты ProfileSlot позволяет открыть другой локальный профиль. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Profile")
	FName ProfileSlot = TEXT("FurwardProfile");

	/**
	 * @brief Удерживающая ссылка на сессию от InitGame() до EndPlay().
	 *
	 * Outer связывает сессию с GameMode, а UPROPERTY удерживает её для сборщика мусора.
	 * Transient исключает временную ссылку из обычного сохранения. Пустое поле означает отсутствие сессии.
	 */
	UPROPERTY(Transient)
	TObjectPtr <UCombatSession> CombatSession = nullptr;

	/**
	 * @brief Класс героя, назначенный в Class Defaults и передаваемый сессии в InitGame().
	 *
	 * Пустой класс приводит к ошибке спауна героя после появления окружения.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Participants")
	TSubclassOf <ACombatHero> HeroClass;
};