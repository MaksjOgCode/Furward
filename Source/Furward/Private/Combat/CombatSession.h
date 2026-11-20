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
class UFurwardProfileSubsystem;
class UFurwardSaveSubsystem;
class UDataTable;
struct FCombatBaseAttributes;
struct FDataTableRowHandle;
struct FCombatStageRow;

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
	/** @brief Профиль подтвердил победу над боссом, сессия ждёт выбора следующего этапа. */
	StageCompleted,
	/** @brief Сессия остановила атаки и ждёт сохранения перед завершением. */
	Leaving
};



/**
 * @brief Координирует сцену и принимает боевые команды на время одного входа в L_Combat.
 *
 * ACombatGameMode создаёт сессию в InitGame(), удерживает её и вызывает Shutdown() перед уходом с карты.
 * Сессия фиксирует исход и управляет таймером атак. В пробной экспедиции смена этапа и выход ждут записи профиля.
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
	 * @brief Включает пробный расчёт характеристик и урона, ответные атаки и замену противника по правилам.
	 * @param[in] InRules Загруженный GameMode ассет. Пустая ссылка оставляет пробный бой в Preparing.
	 * @param[in] OpenedSlot Открытый GameMode слот пробного профиля или None до выбора слота.
	 * @pre GameMode проверил тип мира, включение PIE и открыл указанный слот до запроса этапа.
	 */
	void EnablePIECombatPreview(UCombatRulesDefinition* InRules, FName OpenedSlot);

	/**
	 * @brief Принимает команду на следующий этап пробной экспедиции и запрашивает запись прогресса.
	 * @param[in] SaveSlot Имя выделенного слота текущей проверки PIE.
	 * @return true, если запрос принят после завершения активного этапа.
	 * @post До подтверждения актуальной ревизии сессия остаётся в StageCompleted.
	 */
	bool RequestNextStage(FName SaveSlot);

	/**
	 * @brief Принимает подтверждённый выход из пробного боя и переводит сессию в Leaving.
	 * @param[in] SaveSlot Имя выделенного слота этой проверки PIE.
	 * @return true, если сохранение перед выходом принято или очередь уже ждёт его записи.
	 * @post Атаки остановлены. После ошибки записи ту же команду можно повторить.
	 */
	bool RequestExitCombat(FName SaveSlot);
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
	 * После сохранения при выходе сессия вызывает метод сама. GameMode вызывает его в EndPlay(), повторный вызов безопасен.
	 * @post Ссылки на сцену, идентификатор и данные исхода текущего столкновения очищены.
	 * @note Unreal может завершить выгрузку позже.
	 */
	void Shutdown();

	/**
	 * @brief Даёт контроллеру текущую цель перед трассировкой клика.
	 * @return Текущий живой противник или nullptr при смене цели, завершении этапа, поражении, её отсутствии или смерти.
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
	 * @brief Заменяет погибшего обычного противника на следующий вид, включая босса этапа.
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
	 * обычного противника, Defeated после смерти героя, StageCompleted после учёта победы над боссом
	 * и Leaving после подтверждения выхода или при Shutdown().
	 */
	ECombatPhase Phase = ECombatPhase::Preparing;

	/** @brief Отмечает выполненную очистку, когда после подтверждённого выхода движок вызывает EndPlay(). */
	bool bShutdown = false;

#if WITH_EDITOR
	/** @brief Разрешение на пробный бой, которое GameMode передаёт при входе в мир PIE. */
	bool bPIECombatPreview = false;

	/**
	 * @brief Передаёт профилю победу над обычным противником и подготавливает очередного участника пробной экспедиции.
	 * @pre Пробный бой включён, публикация результата урона завершена.
	 * @post Победа учитывается по EncounterID. При 10/10 сессия подготавливает босса вместо обычного противника.
	 */
	void TryContinuePIECombatPreview();

	/**
	 * @brief Читает следующую строку активного этапа для продолжения на загруженной арене.
	 * @param[out] NextStageID Имя следующей строки при успехе.
	 * @return Подготовленная строка с тем же окружением либо nullptr.
	 */
	const FCombatStageRow* FindNextStageOnActiveArena(FName& NextStageID) const;

	/** @brief Действие, которое продолжится после записи актуальной ревизии. */
	enum class EProgressSaveAction : uint8
	{
		None,
		NextStage,
		Exit
	};

	/**
	 * @brief Связывает действие с пробным слотом и запускает ожидание записи.
	 * @param[in] Slot Имя слота текущей проверки PIE.
	 * @param[in] Action Подготовка следующего этапа или подтверждённый выход.
	 * @param[in] NextStageID Имя следующего этапа, если выбрано продолжение.
	 * @return true, если действие принято. Подтверждённый выход имеет приоритет над продолжением.
	 */
	bool BeginPIEProgressSave(FName Slot, EProgressSaveAction Action, FName NextStageID = NAME_None);

	/** @brief При результате записи своего слота повторно сверяет актуальную ревизию профиля. */
	void HandleProgressSaveFinished(FName Slot, int64 WrittenRevision, bool bSuccess);

	/** @brief Продолжает после подтверждения, запрашивает новый снимок или разрешает повтор команды после ошибки. */
	void CheckPendingProgressSave();

	/** @brief Освобождает сессию и уведомляет контроллер, что можно начать переход карты. */
	void FinishCombatExit();

	/**
	 * @brief Готовит следующую строку на существующей арене после подтверждения записи.
	 * @param[in] StageID Имя выбранной строки.
	 */
	void BeginNextStageOnActiveArena(FName StageID);

	/** @brief Снимает подписку и очищает действие, в том числе при Shutdown(). */
	void ResetPendingProgressSave();

	/** @brief Подсистема записи, на которую подписана сессия во время ожидания. */
	TWeakObjectPtr<UFurwardSaveSubsystem> ProgressSaveSubsystem;

	/** @brief Дескриптор подписки на результаты записи. */
	FDelegateHandle ProgressSaveListener;

	/** @brief Выделенный слот PIE, открытый при входе или принятый при первой записи. */
	FName PIEProgressSlot;

	/** @brief Действие, которое ждёт сохранения актуальной ревизии. */
	EProgressSaveAction PendingSaveAction = EProgressSaveAction::None;

	/** @brief Строка, которую сессия подготавливает после записи при продолжении этапов. */
	FName PendingNextStageID;

	/**
	 * @brief Выбирает описание следующего участника пробной экспедиции по зачистке.
	 * @param[in] bBossPending true, если профиль уже достиг порога зачистки.
	 * @return Обычный противник либо отдельное описание босса. Отсутствующее описание босса даёт nullptr.
	 */
	const UCombatEnemyDefinition* SelectPIEEnemyDefinition(bool bBossPending) const;

	/**
	 * @brief Получает характеристики вида для текущего этапа, включая повторный спаун и босса.
	 * @param[in] Definition Описание вида, который сессия готовится создать.
	 * @param[out] EnemyBase Рассчитанные значения при успехе.
	 * @return true, если этап и описание позволяют подготовить компонент характеристик.
	 */
	bool ResolvePIEEnemyAttributes(const UCombatEnemyDefinition& Definition, FCombatBaseAttributes& EnemyBase) const;

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
	 * @brief Закрывает бой после победы над боссом или при входе на уже завершённый этап.
	 * @pre Профиль подтвердил, что активный этап завершён.
	 */
	void EnterStageCompleted();

	/**
	 * @brief Находит владельца прогресса в текущем GameInstance.
	 * @return Подсистема профиля либо nullptr, пока игровой мир недоступен.
	 */
	UFurwardProfileSubsystem* GetProfile() const;

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
	 * @brief Создаёт участников активного этапа на видимой арене и применяет пробные характеристики в PIE.
	 * @param[in,out] World Мир, в постоянном уровне которого создаются участники.
	 * @param[in] Arena Арена, зарегистрированная в показанном окружении.
	 * @pre Сессия в Preparing, выбран активный этап.
	 */
	void PrepareStageParticipants(UWorld& World, const ACombatArena& Arena);

	/** @brief Открывает ввод после заполнения характеристик обоих участников. */
	void EnterStageReady();

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
	 * @post Для текущего погибшего врага сессия отключает области клика и сохраняет результат один раз.
	 * Обычная победа ведёт в ChangingEnemy. После подтверждённой победы над боссом сессия входит в StageCompleted.
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

	/** @brief Описание босса из строки активного этапа. Сессия удерживает его до Shutdown(). */
	UPROPERTY(Transient)
	TObjectPtr<UCombatEnemyDefinition> BossEnemy;

	/** @brief Имя строки этапа из DT_Combat_Stages, под которым профиль хранит зачистку. */
	FName ActiveStageID;

	/** @brief Таблица активной строки, из которой сессия выбирает продолжение этапа. */
	UPROPERTY(Transient)
	TObjectPtr<const UDataTable> ActiveStageTable;

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

	/** @brief Показывает, относится ли текущий экземпляр врага к описанию босса этапа. */
	bool bCurrentEnemyIsBoss = false;

	/** @brief Результат после смерти противника. Сохраняется до регистрации следующего или завершения сессии. */
	TOptional<FCombatEncounterResult> EncounterResult;

	/** @brief Снимок противника при поражении. Shutdown() очищает его вместе с результатом боя. */
	TOptional<FCombatDefeatSnapshot> DefeatSnapshot;
};