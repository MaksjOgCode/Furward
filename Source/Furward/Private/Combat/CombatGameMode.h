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
 * В Class Defaults задают класс героя, строку начального этапа и ассет боевых правил.
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
	 * Сессия читает строку таблицы. Для включённой проверки PIE GameMode сперва загружает боевые правила.
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
	/**
	 * @brief Ссылка на таблицу и имя начальной строки из Class Defaults BP_Combat_GameMode.
	 *
	 * GameMode удерживает выбранную таблицу во время посещения L_Combat.
	 * Если строка не назначена, сессия остаётся в Preparing и пишет предупреждение.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Stage", meta = (RowType = "/Script/Furward.CombatStageRow"))
	FDataTableRowHandle InitialStage;

	/**
	 * @brief Ассет общих правил боя, доступный для замены в Class Defaults.
	 *
	 * Мягкая ссылка указывает на DA_Combat_Rules по умолчанию. Включённая проверка PIE загружает ассет перед запуском.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Rules")
	TSoftObjectPtr<UCombatRulesDefinition> CombatRules{
		FSoftObjectPath(TEXT("/Game/Combat-System/Data/DA_Combat_Rules.DA_Combat_Rules"))};

#if WITH_EDITORONLY_DATA
	/**
	 * @brief Разрешает пробный бой в Play In Editor при явном включении в Class Defaults.
	 *
	 * Выключенный флаг оставляет сессию ждать начальных игровых данных.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|PIE", meta = (DisplayName = "Enable PIE Combat Preview"))
	bool bEnablePIECombatPreview = false;
#endif

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