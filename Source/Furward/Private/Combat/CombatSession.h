// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает подготовку и активацию столкновений, приём атак и фиксацию исхода в L_Combat.
 */

#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatEncounterResult.h"
#include "Engine/TimerHandle.h"
#include "Misc/Optional.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"
#include "CombatSession.generated.h"



class ULevelStreamingDynamic;
class UWorld;
class ACombatArena;
class ACombatHero;
class ACombatEnemy;
class UCombatAttributesComponent;
class UCombatEnemyDefinition;
class UCombatRulesDefinition;
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
	/** @brief Сессия обрабатывает исход и готовит замену. Атаки закрыты до перехода в Ready или Fighting. */
	ChangingEnemy,
	/** @brief Герой погиб, боевые атаки остановлены и снимок противника сохранён. */
	Defeated,
	/** @brief Сессия блокирует команды и освобождает ссылки при завершении. */
	Leaving
};



/**
 * @brief Координирует сцену и принимает боевые команды на время одного входа в L_Combat.
 *
 * ACombatGameMode создаёт сессию в InitGame(), удерживает её и вызывает Shutdown() перед уходом с карты.
 * Сессия принимает сигналы смерти от компонентов участников, фиксирует исход и управляет таймером ответных атак.
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
	 * @brief Включает пробные характеристики, формулу урона и цикл ответных атак и замены по настроенным правилам.
	 * @param[in] InRules Загруженный GameMode ассет. При пустой ссылке пробный бой заканчивается в ChangingEnemy.
	 * @pre GameMode проверил тип мира и явное включение проверки до запроса этапа.
	 */
	void EnablePIECombatPreview(UCombatRulesDefinition* InRules);
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
	 * @brief Блокирует команды, очищает таймер и подписки участников и запрашивает выгрузку окружения.
	 *
	 * GameMode вызывает метод в EndPlay() перед освобождением сессии.
	 * @post Ссылки на сцену, идентификатор и данные исхода текущего столкновения очищены.
	 * @note Unreal может завершить выгрузку позже.
	 */
	void Shutdown();

	/**
	 * @brief Даёт контроллеру текущую цель перед трассировкой клика.
	 * @return Текущий живой противник или nullptr при смене цели, поражении, её отсутствии или смерти.
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
	 * @brief Заменяет погибшего обычного противника в точке спауна текущей арены.
	 * @param[in] Definition Выбранное описание следующего противника.
	 * @param[in] EnemyBase Рассчитанные характеристики следующего противника.
	 * @pre Сессия находится в ChangingEnemy и сформировала результат. Вызывающий код обработал его. Арена видима, герой жив.
	 * @return true, если погибший актор удалён и новый подготовлен с полным HP.
	 * @post У нового столкновения свой EncounterID. Фаза остаётся ChangingEnemy, атаки заблокированы, HP героя сохраняется.
	 */
	bool PrepareNextEnemy(const UCombatEnemyDefinition& Definition, const FCombatBaseAttributes& EnemyBase);

	/**
	 * @brief Открывает ввод для подготовленного столкновения согласно правилу старта.
	 * @param[in] ExpectedEncounterID Идентификатор, полученный после подготовки следующего противника.
	 * @param[in] Rules Настройки старта из описания боевых правил.
	 * @return true, если готовое столкновение переведено из ChangingEnemy в Ready или Fighting.
	 * @note Пауза, устаревший запрос и незаданное правило оставляют фазу прежней.
	 */
	bool ActivatePreparedEncounter(const FGuid& ExpectedEncounterID, const UCombatRulesDefinition& Rules);

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
	 * @note Первый положительный урон переводит сессию в Fighting до уведомлений о здоровье.
	 * Смертельный урон переводит сессию в ChangingEnemy после уведомления о здоровье.
	 */
	float ApplyResolvedDamageToEnemy(const ACombatEnemy* Target, float ResolvedDamage);

	/**
	 * @brief Даёт вызывающему коду фазу текущей сессии.
	 * @return Фаза подготовки, боя или завершения.
	 */
	ECombatPhase GetPhase() const { return Phase; }

	/**
	 * @brief Возвращает идентификатор столкновения, к которому относится текущий противник.
	 * @return EncounterID, созданный после спауна. До спауна и после Shutdown() идентификатор недействителен.
	 */
	FGuid GetEncounterID() const { return EncounterID; }

	/**
	 * @brief Возвращает сформированный результат текущего столкновения для дальнейшей обработки.
	 * @return Копия результата, если противник погиб. При регистрации нового противника результат очищается.
	 */
	TOptional<FCombatEncounterResult> GetEncounterResult() const { return EncounterResult; }

	/**
	 * @brief Возвращает копию данных о противнике для итогов после гибели героя.
	 * @return Снимок поражения или пустое значение, пока герой не погиб.
	 */
	TOptional<FCombatDefeatSnapshot> GetDefeatSnapshot() const { return DefeatSnapshot; }

private:
	/**
	 * @brief Фаза сессии, от которой зависит допуск команды кликовой атаки.
	 *
	 * Начинается с Preparing, становится Ready после инициализации характеристик,
	 * Fighting после первого положительного урона или автоматического старта, ChangingEnemy после смерти
	 * противника, Defeated после смерти героя и Leaving при Shutdown().
	 */
	ECombatPhase Phase = ECombatPhase::Preparing;

#if WITH_EDITOR
	/** @brief Разрешение на пробный бой, которое GameMode передаёт при входе в мир PIE. */
	bool bPIECombatPreview = false;

	/**
	 * @brief После обработки попадания готовит следующего пробного мутанта с полным HP.
	 * @pre Пробный бой включён, публикация результата урона завершена.
	 * @post После смерти и при заданном правиле сессия создаёт и активирует следующий экземпляр.
	 */
	void TryContinuePIECombatPreview();

	/**
	 * @brief Запускает повторяемые удары текущего противника, если пробный бой и интервалы настроены.
	 * @pre Сессия вошла в Fighting и проверяет обоих участников перед запуском.
	 */
	void StartPIEEnemyAttacks();

	/** @brief Отменяет таймер текущего столкновения и освобождает его дескриптор. */
	void StopPIEEnemyAttacks();

	/**
	 * @brief Применяет один ответный удар живого противника по герою.
	 * @param[in] AttackingEncounterID Идентификатор, переданный таймеру при старте.
	 * @note Фаза, актуальность столкновения и готовность участников проверяются при каждом ударе.
	 */
	void HandlePIEEnemyAttack(FGuid AttackingEncounterID);

	/** @brief Дескриптор одного таймера ответных ударов для текущего столкновения PIE. */
	FTimerHandle PIEEnemyAttackTimer;
#endif

#if WITH_EDITORONLY_DATA
	/** @brief Загруженные правила, которые сессия удерживает до завершения проверки PIE. */
	UPROPERTY(Transient)
	TObjectPtr<UCombatRulesDefinition> PIECombatRules;
#endif

	/**
	 * @brief Переводит подготовленное столкновение в активную фазу через общую точку старта.
	 * @pre Сессия в Ready и применила положительный урон либо приняла автоматический старт в ChangingEnemy.
	 */
	void EnterFighting();

	/**
	 * @brief Проверяет видимость арены, готовность участников, принадлежность цели и состояние паузы.
	 * @param[in] Target Противник, которого вызывающий код собирается активировать или атаковать.
	 * @return true, если сцена и оба участника готовы выполнять боевые действия.
	 */
	bool CanUseCurrentEnemy(const ACombatEnemy* Target) const;

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
	 * @brief Создаёт экземпляр противника и применяет настройки вида до завершения спауна.
	 * @param[in,out] World Игровой мир, чей постоянный уровень будет владеть актором.
	 * @param[in] Arena Арена с мировой точкой появления противника.
	 * @param[in] Definition Описание класса, модели и областей клика.
	 * @return Созданный актор или nullptr при неполном описании или сбое спауна.
	 */
	ACombatEnemy* SpawnEnemy(UWorld& World, const ACombatArena& Arena, const UCombatEnemyDefinition& Definition);

	/**
	 * @brief Регистрирует актор, выдаёт столкновению новый EncounterID и подписывается на смерть.
	 * @param[in,out] SpawnedEnemy Актор, созданный в постоянном уровне.
	 * @param[in] Definition Описание вида, применённое при спауне актора.
	 * @post Ссылка указывает на нового противника, а результат предыдущего столкновения очищен.
	 */
	void RegisterEnemy(ACombatEnemy& SpawnedEnemy, const UCombatEnemyDefinition& Definition);

	/**
	 * @brief Принимает смерть текущего противника и формирует результат столкновения.
	 * @param[in] Source Компонент характеристик, опубликовавший OnDeath после обнуления HP.
	 * @post Если сессия в Fighting и Source принадлежит текущей цели с нулевым HP,
	 * сессия переходит в ChangingEnemy, отключает области клика цели и сохраняет результат один раз.
	 */
	void HandleEnemyDeath(const UCombatAttributesComponent& Source);

	/**
	 * @brief Останавливает бой после гибели героя и сохраняет здоровье противника для итогов.
	 * @param[in] Source Компонент характеристик героя, опубликовавший OnDeath.
	 * @post Сессия в Fighting переходит в Defeated при смерти текущего героя. Атаки прекращаются.
	 */
	void HandleHeroDeath(const UCombatAttributesComponent& Source);

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
	 * @brief Слабая ссылка на экземпляр противника в постоянном уровне.
	 *
	 * В ChangingEnemy ссылка сначала указывает на погибшего актора, затем на подготовленную замену.
	 * Shutdown() снимает подписку на OnDeath и очищает поле.
	 */
	UPROPERTY(Transient)
	TWeakObjectPtr<ACombatEnemy> Enemy;

	/** @brief Уникальный идентификатор столкновения с момента регистрации противника до замены или Shutdown(). */
	FGuid EncounterID;

	/** @brief Идентификатор Data Asset текущего противника для результата столкновения. */
	FPrimaryAssetId EnemyDefinitionID;

	/** @brief Результат после смерти противника. Сохраняется до регистрации следующего или завершения сессии. */
	TOptional<FCombatEncounterResult> EncounterResult;

	/** @brief Снимок противника при поражении. Shutdown() очищает его вместе с результатом боя. */
	TOptional<FCombatDefeatSnapshot> DefeatSnapshot;
};