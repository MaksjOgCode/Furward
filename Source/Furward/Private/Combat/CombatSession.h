// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает выбор этапа, время жизни участников и проверку команд в L_Combat.
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
struct FCombatBaseAttributes;
struct FDataTableRowHandle;

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

/** @brief Определяет, на каком этапе подготовки, боя или завершения находится сессия. */
enum class ECombatPhase : uint8
{
	/** @brief Сцена, участники или их характеристики ещё не готовы к приёму команд. */
	Preparing,
	/** @brief Окружение показано, участники созданы и получили начальные характеристики. */
	Ready,
	/** @brief По текущему противнику уже применён первый положительный урон. */
	Fighting,
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
	 * @brief Принимает класс героя от GameMode перед подготовкой этапа.
	 * @param[in] InHeroClass Класс героя из Class Defaults. Пустой класс остановит спаун героя.
	 * @pre GameMode создал сессию с собой в качестве Outer.
	 * @post Класс героя доступен для спауна по точке арены.
	 */
	void Initialize(TSubclassOf<ACombatHero> InHeroClass);

#if WITH_EDITOR
	/**
	 * @brief Включает пробные характеристики и формулу урона для текущего запуска PIE.
	 * @pre GameMode проверил тип мира и явное включение проверки до запроса этапа.
	 */
	void EnablePIECombatPreview();
#endif

	/**
	 * @brief Читает начальный этап и запрашивает подгрузку его окружения.
	 *
	 * GameMode вызывает метод в BeginPlay(). Повторный запрос и запрос вне Preparing пропускаются.
	 * @param[in] WorldContextObject Объект из игрового мира L_Combat.
	 * @param[in] StageHandle Ссылка на таблицу этапов и имя начальной строки.
	 * @note Initial combat stage подтверждает выбор строки после принятия запроса. OnLevelShown приходит позже.
	 */
	void RequestInitialStage(UObject* WorldContextObject, const FDataTableRowHandle& StageHandle);

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
	 * @brief Инициализирует обоих участников готовыми данными и открывает приём команд.
	 * @param[in] HeroBase Фактический набор героя по данным профиля и экипировки.
	 * @param[in] HeroCurrentHealth Начальное здоровье героя, выбранное правилами входа в бой.
	 * @param[in] EnemyBase Набор противника, рассчитанный из ожидаемого баланса этапа и данных вида.
	 * @return true, если видимая арена и оба участника получили корректные стартовые данные.
	 * @post При успехе оба набора записаны, сессия перешла в Ready и уведомила подписчиков о здоровье.
	 * @note Новый противник получает полное здоровье. Неверный набор не изменяет ни один компонент.
	 */
	bool InitializeParticipantAttributes(const FCombatBaseAttributes& HeroBase, float HeroCurrentHealth,
		const FCombatBaseAttributes& EnemyBase);

	/**
	 * @brief Проверяет запрос атаки, который контроллер передаёт после трассировки клика.
	 * @param[in] Result Результат трассировки. Запрос может содержать Hit или Miss.
	 * @param[in] Target Тот же экземпляр противника, по которому выполнена трассировка.
	 * @return true при готовой сцене, актуальной цели и активном игровом времени. Иначе false.
	 * @note В пробном бое PIE результат Hit проходит расчёт и применение урона.
	 * @see ApplyResolvedDamageToEnemy
	 */
	bool TrySubmitClickAttack(ECombatClickResult Result, const ACombatEnemy* Target);

	/**
	 * @brief Применяет рассчитанный урон по актуальному противнику.
	 * @param[in] Target Противник, для которого рассчитан урон.
	 * @param[in] ResolvedDamage Урон после учёта игровых характеристик и модификаторов.
	 * @return Фактическая потеря HP. Ноль означает отсутствие изменения.
	 * @note После первого положительного результата сессия переходит в Fighting до уведомлений о здоровье.
	 */
	float ApplyResolvedDamageToEnemy(const ACombatEnemy* Target, float ResolvedDamage);

	/**
	 * @brief Даёт вызывающему коду фазу текущей сессии.
	 * @return Фаза подготовки, боя или завершения.
	 */
	ECombatPhase GetPhase() const { return Phase; }

private:
	/**
	 * @brief Фаза сессии, от которой зависит допуск команды кликовой атаки.
	 *
	 * Начинается с Preparing, становится Ready после инициализации характеристик,
	 * Fighting после первого урона и Leaving при Shutdown().
	 */
	ECombatPhase Phase = ECombatPhase::Preparing;

#if WITH_EDITOR
	/** @brief Разрешение на пробный бой, которое GameMode передаёт при входе в мир PIE. */
	bool bPIECombatPreview = false;
#endif

	/**
	 * @brief Повторно проверяет фазу, цель, участников и паузу перед боевой операцией.
	 * @param[in] Target Экземпляр, на который направлена операция.
	 * @return true, если живой противник остаётся актуальной целью в готовой сцене.
	 */
	bool CanAttackEnemy(const ACombatEnemy* Target) const;

	/**
	 * @brief Принимает уведомление OnLevelShown и создаёт участников показанного окружения.
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
	 * @brief Описание первого противника, прочитанное из строки этапа.
	 *
	 * Сессия удерживает ассет от принятия запроса стриминга до Shutdown().
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