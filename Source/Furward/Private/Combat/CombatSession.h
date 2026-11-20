// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает подготовку сцены, время жизни участников и проверку команд в L_Combat.
 */

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"
#include "CombatSession.generated.h"



class ULevelStreamingDynamic;
class UWorld;
class ACombatArena;
class ACombatHero;
class ACombatEnemy;
class UCombatEnemyDefinition;

/** @brief Результат проверки луча от курсора, который контроллер вычисляет перед отправкой команды. */
enum class ECombatClickResult : uint8
{
	/** @brief Луч пересёк область попадания текущего противника. */
	Hit,
	/** @brief Луч прошёл мимо HitArea и пересёк область допустимого промаха. */
	Miss,
	/** @brief Клик не образует команду атаки. */
	Ignore
};

/** @brief Определяет, на каком этапе подготовки или завершения находится сессия. */
enum class ECombatPhase : uint8
{
	/** @brief Окружение или участники ещё не готовы к приёму команд. */
	Preparing,
	/** @brief Окружение показано и оба участника готовы к приёму кликовых запросов. */
	Ready,
	/** @brief Сессия блокирует команды и освобождает ссылки при завершении. */
	Leaving
};



/**
 * @brief Координирует сцену и принимает боевые команды на время одного входа в L_Combat.
 *
 * ACombatGameMode создаёт сессию в InitGame(), удерживает её и вызывает Shutdown() перед уходом с карты.
 * Сессия сохраняет ссылки на арену и участников, созданных в игровом мире.
 */
UCLASS()
class UCombatSession : public UObject
{
	GENERATED_BODY()
public:
	/**
	 * @brief Принимает настройки участников от GameMode перед подгрузкой окружения.
	 * @param[in] InHeroClass Класс героя из Class Defaults. Пустой класс остановит спаун героя.
	 * @param[in] InInitialEnemy Описание первого противника. Пустое описание остановит спаун противника.
	 * @pre GameMode создал сессию с собой в качестве Outer.
	 * @post Настройки доступны для спауна по точкам арены.
	 */
	void Initialize(TSubclassOf<ACombatHero> InHeroClass, UCombatEnemyDefinition* InInitialEnemy);

	/**
	 * @brief Запрашивает подгрузку начального окружения в фазе Preparing.
	 *
	 * GameMode вызывает метод в BeginPlay(). Повторный запрос и запрос вне Preparing пропускаются.
	 * @param[in] WorldContextObject Объект, по которому Unreal находит игровой мир L_Combat.
	 * @param[in] EnvironmentMap Мягкая ссылка на карту окружения.
	 * @pre Контекст действителен, карта назначена.
	 * @note Environment requested означает приём запроса. Появление уровня обрабатывает HandleEnvironmentShown().
	 */
	void RequestInitialEnvironment (UObject* WorldContextObject, const TSoftObjectPtr <UWorld>& EnvironmentMap);

	/**
	 * @brief Блокирует команды и просит Unreal выгрузить окружение при завершении карты.
	 *
	 * GameMode вызывает метод в EndPlay() перед освобождением сессии.
	 * @post Ссылки на участников, арену, описание противника и объект стриминга очищены.
	 * @note Unreal может завершить выгрузку позже.
	 */
	void Shutdown();

	/**
	 * @brief Даёт контроллеру текущую цель перед трассировкой клика.
	 * @return Противник, зарегистрированный сессией, или nullptr до его появления и после потери ссылки.
	 */
	const ACombatEnemy* GetActiveEnemy() const;

	/**
	 * @brief Проверяет запрос атаки, который контроллер передаёт после трассировки клика.
	 * @param[in] Result Результат трассировки. Запрос может содержать Hit или Miss.
	 * @param[in] Target Тот же экземпляр противника, по которому выполнена трассировка.
	 * @return true при готовой сцене, актуальной цели и активном игровом времени. Иначе false.
	 * @note Лог Click attack accepted подтверждает валидацию. Здоровье и фаза боя здесь не меняются.
	 */
	bool TrySubmitClickAttack(ECombatClickResult Result, const ACombatEnemy* Target);

private:
	/**
	 * @brief Фаза сессии, от которой зависит допуск команды кликовой атаки.
	 *
	 * Начинается с Preparing, становится Ready после подготовки участников и Leaving при Shutdown().
	 */
	ECombatPhase Phase = ECombatPhase::Preparing;

	/**
	 * @brief Принимает уведомление OnLevelShown и готовит участников показанного окружения.
	 *
	 * UFUNCTION позволяет подписать метод на динамический делегат. Обработчик продолжает работу только в Preparing.
	 * Для продолжения нужны видимый подуровень, ровно одна действительная арена и боевой контроллер.
	 */
	UFUNCTION()
	void HandleEnvironmentShown();

	/**
	 * @brief Объект управления подуровнем от принятия запроса до Shutdown().
	 *
	 * Сессия держит его для получения OnLevelShown и запроса выгрузки.
	 * Transient исключает временную ссылку из сохранения. Пустая ссылка означает, что сессия не отслеживает подуровень.
	 */
	UPROPERTY(Transient)
	TObjectPtr <ULevelStreamingDynamic> EnvironmentStreaming = nullptr;

	/**
	 * @brief Слабая ссылка на арену, зарегистрированную по OnLevelShown.
	 *
	 * Арена принадлежит подуровню. Ссылка пуста до регистрации, после потери арены или после Shutdown().
	 */
	UPROPERTY(Transient)
	TWeakObjectPtr <ACombatArena> ActiveArena;

	/**
	 * @brief Создаёт героя после регистрации арены, если у сессии ещё нет действующего экземпляра.
	 * @param[in,out] World Игровой мир, чей постоянный уровень будет владеть героем.
	 * @param[in] Arena Арена с мировой точкой появления героя.
	 * @note При ошибке спауна или неназначенном классе метод пишет ошибку и не заполняет Hero.
	 */
	void SpawnHero(UWorld& World, const ACombatArena& Arena);

	/**
	 * @brief Создаёт противника после регистрации арены и применяет настройки вида до завершения спауна.
	 * @param[in,out] World Игровой мир, чей постоянный уровень будет владеть актором.
	 * @param[in] Arena Арена с мировой точкой появления противника.
	 * @note Если цель ещё существует, повторный спаун пропускается. Неполное описание и сбой спауна дают ошибку в логе.
	 */
	void SpawnEnemy(UWorld& World, const ACombatArena& Arena);

	/**
	 * @brief Класс героя, полученный от GameMode в Initialize().
	 *
	 * Хранится до уничтожения сессии. Пустое значение останавливает спаун героя.
	 */
	UPROPERTY(Transient)
	TSubclassOf<ACombatHero> HeroClass;

	/**
	 * @brief Слабая ссылка на героя после успешного спауна в постоянном уровне.
	 *
	 * Пустая или недействительная ссылка означает, что герой недоступен для боя. Shutdown() очищает поле.
	 */
	UPROPERTY(Transient)
	TWeakObjectPtr<ACombatHero> Hero;

	/**
	 * @brief Стартовое описание противника, полученное в Initialize().
	 *
	 * Сессия удерживает ассет до Shutdown(). Без назначения спаун противника пропускается.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UCombatEnemyDefinition> InitialEnemy;

	/**
	 * @brief Слабая ссылка на экземпляр противника, доступный как цель клика.
	 *
	 * Появляется после успешного спауна и применения описания. Shutdown() очищает поле.
	 * Пустая или недействительная ссылка означает, что актуальной цели нет.
	 */
	UPROPERTY(Transient)
	TWeakObjectPtr<ACombatEnemy> Enemy;
};