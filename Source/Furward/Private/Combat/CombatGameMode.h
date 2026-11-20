// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает, как игровой режим L_Combat создаёт и завершает боевую сессию.
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UObject/ObjectPtr.h"
#include "Templates/SubclassOf.h"
#include "CombatGameMode.generated.h"



class UCombatSession;
class UCombatEnvironmentDefinition;
class ACombatHero;
class UCombatEnemyDefinition;



/**
 * @brief Владеет одной UCombatSession на время посещения карты L_Combat.
 *
 * Unreal создаёт экземпляр BP_Combat_GameMode для игрового мира.
 * В Class Defaults задают окружение, героя и описание первого противника.
 */
UCLASS()
class ACombatGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	/**
	 * @brief Создаёт сессию и передаёт ей настройки участников до начала игры.
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
	 * @brief Запрашивает подгрузку стартового окружения, когда Unreal запускает карту.
	 *
	 * Если описание или карта не назначены, пишет предупреждение. При принятии запроса сессия пишет Environment requested.
	 * @pre InitGame() создал CombatSession.
	 * @see UCombatSession::RequestInitialEnvironment
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
	 * @brief Описание стартового окружения из Class Defaults BP_Combat_GameMode.
	 *
	 * GameMode удерживает его как настройку на время жизни экземпляра.
	 * Пустое поле оставляет сцену без запроса загрузки и даёт предупреждение в BeginPlay().
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Environment")
	TObjectPtr <UCombatEnvironmentDefinition> InitialEnvironment = nullptr;

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

	/**
	 * @brief Стартовое описание вида, назначенное в Class Defaults.
	 *
	 * GameMode удерживает ассет и передаёт его сессии в InitGame().
	 * Если поле пусто, сессия пишет ошибку и пропускает спаун противника.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Participants")
	TObjectPtr<UCombatEnemyDefinition> InitialEnemy;
};