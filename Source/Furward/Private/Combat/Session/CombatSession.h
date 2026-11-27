// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает подготовку и активацию столкновений, приём атак и фиксацию исхода в L_Combat.
 */

#pragma once

#include "CoreMinimal.h"
#include "Combat/Participants/CombatBaseAttributes.h"
#include "Combat/Session/CombatEncounterResult.h"
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
class UFurwardLootDefinition;
class UFurwardSaveSubsystem;
class UDataTable;
struct FDataTableRowHandle;
struct FCombatStageRow;

/** @brief Копия здоровья одного участника для отображения. Нулевой максимум означает, что данные ещё не готовы. */
struct FCombatParticipantHealth
{
	float Current = 0.0f;
	float Maximum = 0.0f;
};

/** @brief Данные настроенного слота героя для отображения в HUD. */
struct FCombatAbilitySlotSnapshot
{
	/** @brief Название из описания способности. */
	FText DisplayName;

	/** @brief Оставшееся игровое время, прочитанное из таймера компонента. */
	float CooldownRemaining = 0.0f;

	/** @brief Сколько боевого времени осталось у эффекта, запущенного из этого слота. */
	float EffectRemaining = 0.0f;

	/** @brief Сейчас разрешена команда слота. Фактический эффект проверит сама способность. */
	bool bCanActivate = false;
};

/** @brief Сообщает интерфейсу об изменении здоровья или смене доступных участников. */
DECLARE_MULTICAST_DELEGATE(FCombatParticipantHealthChanged);

/** @brief Сообщает HUD о доступном балансе после открытия профиля или начисления валют. */
DECLARE_MULTICAST_DELEGATE(FCombatCurrencyBalancesChanged);

/** @brief Состояние босса для отображения в интерфейсе экспедиции. */
enum class ECombatBossStatus : uint8
{
	Locked,
	Available,
	OnArena,
	Defeated
};

/** @brief Доступность и результат команды продолжения для HUD экспедиции. */
enum class ECombatNextStageState : uint8
{
	Unavailable,
	Available,
	Saving,
	SaveFailed,
	StartFailed,
	EnvironmentLoading,
	EnvironmentFailed,
	EncounterFailed,
	ProfileUnavailable,
	InvalidConfiguration
};

/** @brief Этап, зачистка из профиля, состояние босса и доступность продолжения для HUD. */
struct FCombatProgressionSnapshot
{
	FName StageID;
	int32 ClearedEnemies = 0;
	int32 RequiredClears = 0;
	ECombatBossStatus BossStatus = ECombatBossStatus::Locked;
	ECombatNextStageState NextStageState = ECombatNextStageState::Unavailable;
};

/** @brief Сообщает HUD об изменении данных экспедиции или состояния перехода. */
DECLARE_MULTICAST_DELEGATE(FCombatProgressionChanged);

/** @brief Сообщает HUD о переходе в Defeated после захвата снимка HP противника. */
DECLARE_MULTICAST_DELEGATE(FCombatDefeatRecorded);

/** @brief Сообщает HUD, когда неудачную запись перед выходом можно повторить. */
DECLARE_MULTICAST_DELEGATE(FCombatExitSaveFailed);

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

/** @brief Различает режимы боя с разными правилами учёта зачистки. */
enum class ECombatMode : uint8
{
	Expedition,
	Farm
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
	/** @brief Сессия восстанавливает героя по правилам выбранного режима. */
	Recovering,
	/** @brief Профиль подтвердил победу над боссом, сессия ждёт выбора следующего этапа. */
	StageCompleted,
	/** @brief Сессия остановила атаки и ждёт сохранения перед завершением. */
	Leaving
};



/**
 * @brief Координирует сцену и принимает боевые команды на время одного входа в L_Combat.
 *
 * ACombatGameMode создаёт сессию в InitGame(), удерживает её и вызывает Shutdown() перед уходом с карты.
 * Сессия фиксирует исход, управляет таймером и переключает режим боя. После победы профиль
 * принимает валюты и прогресс. Запись идёт в очередь. Смена этапа и выход ждут актуальной ревизии.
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

	/**
	 * @brief Принимает правила боя и стартовые характеристики героя до загрузки этапа.
	 * @param[in] Rules Общие игровые данные, загруженные GameMode.
	 * @param[in] OpenedSlot Открытый слот профиля. None отключает автосохранение.
	 * @return true, если сессия ещё готовится и стартовые характеристики корректны.
	 */
	bool ConfigureCombatStart(UCombatRulesDefinition& Rules, FName OpenedSlot);

	/**
	 * @brief Запрашивает следующий этап после записи или повторяет подготовку окружения активного этапа.
	 * @param[in] SaveSlot Имя слота. None использует открытый слот.
	 * @return true, если принята команда продолжения или повтора загрузки.
	 * @post При смене этапа сессия дожидается актуальной ревизии в StageCompleted.
	 */
	bool RequestNextStage(FName SaveSlot);

	/**
	 * @brief После поражения подготавливает обычного противника и восстанавливает героя для фарма.
	 * @return true, если новый бой готов. При ошибке подготовки остаётся открытым выбор эвакуации.
	 */
	bool RequestEnterFarm();

	/**
	 * @brief Прерывает фарм и подготавливает сражение экспедиции по зачистке активного этапа.
	 * @return true после замены противника и восстановления HP. При сбое сохраняет текущий бой.
	 */
	bool RequestReturnToExpedition();

	/**
	 * @brief Принимает подтверждённый выход из боя и переводит сессию в Leaving.
	 * @param[in] SaveSlot Имя слота. None использует открытый слот.
	 * @return true, если сохранение перед выходом принято или очередь уже ждёт его записи.
	 * @pre Сессия не подготавливает участников при смене режима.
	 * @post Атаки остановлены. После ошибки записи ту же команду можно повторить.
	 */
	bool RequestExitCombat(FName SaveSlot);

	/**
	 * @brief Читает начальный этап и запрашивает подгрузку его окружения.
	 *
	 * GameMode вызывает метод в BeginPlay(). Повторный запрос и запрос вне Preparing пропускаются.
	 * @param[in] StageHandle Ссылка на таблицу этапов и имя начальной строки.
	 * @note OnLevelShown подтверждает видимость окружения после запроса.
	 */
	void RequestInitialStage(const FDataTableRowHandle& StageHandle);

	/** @brief Публикует ошибку входа до начала изменения профиля. */
	void ReportStartupFailure(ECombatNextStageState Failure);

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

	/** @brief Возвращает здоровье героя для HUD, включая ноль после поражения. */
	FCombatParticipantHealth GetHeroHealthForDisplay() const;

	/** @brief Возвращает здоровье текущего врага для HUD, включая ноль после победы. */
	FCombatParticipantHealth GetEnemyHealthForDisplay() const;

	/** @brief Даёт интерфейсу подписаться на обновления здоровья обоих участников. */
	FCombatParticipantHealthChanged& OnParticipantHealthChanged() { return ParticipantHealthChanged; }

	/** @brief Читает баланс выбранной валюты из профиля текущего мира. */
	int64 GetCurrencyBalanceForDisplay(FName CurrencyID) const;

	/** @brief Даёт HUD подписаться на обновление валют в текущей сессии. */
	FCombatCurrencyBalancesChanged& OnCurrencyBalancesChanged() { return CurrencyBalancesChanged; }

	/** @brief Читает прогресс активного этапа или состояние ошибки до выбора этапа. */
	FCombatProgressionSnapshot GetProgressionForDisplay() const;

	/** @brief Даёт интерфейсу подписаться на обновление прогресса и состояния перехода. */
	FCombatProgressionChanged& OnProgressionChanged() { return ProgressionChanged; }

	/**
	 * @brief Открывает или закрывает причину приостановки боя от окна подтверждения.
	 * @param[in] bOpen true при открытии. false возобновляет оставшееся время таймера.
	 * @return true при принятом состоянии окна.
	 */
	bool SetLeaveConfirmationOpen(bool bOpen);

	/** @brief Приостанавливает или возобновляет боевое время при переключении меню паузы. */
	bool SetPauseMenuOpen(bool bOpen);

	/**
	 * @brief Пытается выполнить способность живого героя во время активного столкновения.
	 * @param[in] SlotIndex Индекс слота с нуля.
	 * @return true, если способность выполнила эффект.
	 */
	bool TryUseAbilitySlot(int32 SlotIndex);

	/** @brief Применяет срабатывание автокликов по текущей цели через штатный расчёт попадания. */
	void SubmitAutoClick();

	/** @brief Возвращает снимок слота героя по индексу с нуля. Для пустого слота снимка нет. */
	TOptional<FCombatAbilitySlotSnapshot> GetAbilitySlotForDisplay(int32 SlotIndex) const;

	/** @brief Количество авторских слотов, включая пустые позиции для привязки клавиш. */
	int32 GetAbilitySlotCount() const;

	/** @brief Событие поражения. GetDefeatSnapshot() уже возвращает данные для итогов. */
	FCombatDefeatRecorded& OnDefeatRecorded() { return DefeatRecorded; }

	/** @brief Событие ошибки сохранения, после которого разрешён повтор выхода. */
	FCombatExitSaveFailed& OnExitSaveFailed() { return ExitSaveFailed; }

	/**
	 * @brief Принимает запрос контроллера после трассировки и рассчитывает урон при попадании.
	 * @param[in] Result Результат трассировки. Запрос может содержать Hit или Miss.
	 * @param[in] Target Тот же экземпляр противника, по которому выполнена трассировка.
	 * @return true при готовой сцене, актуальной цели и активном игровом времени. Иначе false.
	 * @note При настроенных правилах Hit применяет урон с учётом брони цели. Miss не меняет HP.
	 * @see ApplyResolvedDamageToEnemy
	 */
	bool TrySubmitClickAttack(ECombatClickResult Result, const ACombatEnemy* Target);

	/**
	 * @brief Даёт вызывающему коду фазу текущей сессии.
	 * @return Фаза подготовки, боя или завершения.
	 */
	ECombatPhase GetPhase() const { return Phase; }

	/** @brief Даёт HUD режим для выбора панели General. */
	ECombatMode GetMode() const { return Mode; }

	/** @brief Проверяет состояние поражения и настроенные правила входа в фарм. */
	bool CanEnterFarm() const;

	/** @brief Проверяет, что живой герой может прервать активное столкновение фарма. */
	bool CanReturnToExpedition() const;

	/**
	 * @brief Возвращает копию данных о противнике для итогов после гибели героя.
	 * @return Снимок поражения или пустое значение, пока герой не погиб.
	 */
	TOptional<FCombatDefeatSnapshot> GetDefeatSnapshot() const { return DefeatSnapshot; }

private:
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
	 * @brief Применяет рассчитанный урон по актуальному противнику.
	 * @param[in] Target Противник, для которого рассчитан урон.
	 * @param[in] ResolvedDamage Урон после учёта игровых характеристик и модификаторов.
	 * @return Фактическая потеря HP. Ноль означает отсутствие изменения.
	 * @note Первый положительный урон переводит сессию в Fighting до уведомлений о здоровье.
	 * Смертельный урон запускает обработку исхода. При настроенном правиле одновременной гибели исход подводится в конце кадра.
	 */
	float ApplyResolvedDamageToEnemy(const ACombatEnemy* Target, float ResolvedDamage);

	/**
	 * @brief Применяет рассчитанный ответный удар актуального противника по герою.
	 * @return Фактическая потеря HP. Возвращает ноль при отклонённом ударе.
	 */
	float ApplyResolvedDamageToHero(const ACombatEnemy* Attacker, float ResolvedDamage);

	/**
	 * @brief Фаза сессии, от которой зависит допуск команды кликовой атаки.
	 *
	 * Начинается с Preparing, становится Ready после инициализации характеристик,
	 * Fighting после первого положительного урона или автоматического старта, ChangingEnemy после смерти
	 * обычного противника, Defeated после смерти героя, StageCompleted после учёта победы над боссом
	 * и Leaving после подтверждения выхода, выбора эвакуации или при Shutdown().
	 */
	ECombatPhase Phase = ECombatPhase::Preparing;

	/** @brief Меняет фазу и синхронизирует паузу боевых таймеров. */
	void SetCombatPhase(ECombatPhase NewPhase);

	/** @brief Синхронизирует таймеры атак, эффектов и кулдаунов с фазой и открытым окном. */
	void RefreshCombatTime();

	/** @brief Проверяет общее правило допуска для команды и представления слота. */
	bool CanUseAbilities() const;

	/** @brief Режим столкновений в текущей сессии. Профиль отдельно хранит прогресс экспедиции. */
	ECombatMode Mode = ECombatMode::Expedition;

	/** @brief Отмечает выполненную очистку, когда после подтверждённого выхода движок вызывает EndPlay(). */
	bool bShutdown = false;

	/**
	 * @brief Передаёт снимок после зачисленной победы в очередь выбранного слота, если он задан.
	 * @param[in] Profile Профиль с уже обновлённой ревизией.
	 * @note Подготовка следующего столкновения идёт сразу. Запись подтверждает подсистема сохранений.
	 */
	void QueueProgressAutosave(const UFurwardProfileSubsystem& Profile);

	/** @brief Возвращает подсистему, чья очередь живёт в GameInstance текущего мира. */
	UFurwardSaveSubsystem* GetSaveSubsystem() const;

	/**
	 * @brief Проверяет строку следующего этапа, его окружение и характеристики первого противника.
	 * @param[out] NextStageID Имя следующей строки при успехе.
	 * @return Проверенная строка либо nullptr.
	 */
	const FCombatStageRow* FindNextStage(FName& NextStageID) const;

	/** @brief Действие, которое продолжится после записи актуальной ревизии. */
	enum class EProgressSaveAction : uint8
	{
		None,
		NextStage,
		Exit
	};

	/**
	 * @brief Связывает действие с открытым слотом и запускает ожидание записи.
	 * @param[in] Slot Имя слота текущей сессии.
	 * @param[in] Action Подготовка следующего этапа или подтверждённый выход.
	 * @param[in] NextStageID Имя следующего этапа, если выбрано продолжение.
	 * @return true, если действие принято. Подтверждённый выход имеет приоритет над продолжением.
	 */
	bool BeginProgressSave(FName Slot, EProgressSaveAction Action, FName NextStageID = NAME_None);

	/** @brief При результате записи своего слота повторно сверяет актуальную ревизию профиля. */
	void HandleProgressSaveFinished(FName Slot, int64 WrittenRevision, bool bSuccess);

	/** @brief Продолжает после подтверждения, запрашивает новый снимок или разрешает повтор команды после ошибки. */
	void CheckPendingProgressSave();

	/** @brief Освобождает сессию и разрешает контроллеру завершить игру. */
	void FinishCombatExit();

	/**
	 * @brief Готовит следующий этап после записи и меняет окружение, если назначена другая карта.
	 * @param[in] StageID Имя выбранной строки.
	 * @return true, если сессия сменила активный этап и запустила подготовку.
	 */
	bool BeginNextStage(FName StageID);

	/** @brief Удаляет противника предыдущего этапа и сбрасывает данные его столкновения. */
	bool ClearStageEnemy();

	/** @brief Снимает прежнюю арену и запускает наблюдение за подготовкой окружения активного этапа. */
	void StartEnvironmentPreparation();

	/** @brief Запрашивает карту после скрытия старой и проверяет состояние асинхронной загрузки. */
	void UpdateEnvironmentPreparation();

	/** @brief Оставляет Preparing и открывает повтор загрузки в HUD. */
	void FailEnvironmentPreparation();

	/** @brief Снимает подписку, останавливает наблюдение и запрашивает выгрузку уровня. */
	void ReleaseEnvironment();

	/** @brief Публикует состояние записи или ошибку подготовки для панели экспедиции. */
	void SetNextStageState(ECombatNextStageState State);

	/** @brief Снимает подписку и очищает действие, в том числе при Shutdown(). */
	void ResetPendingProgressSave();

	/** @brief Подсистема записи, на которую подписана сессия во время ожидания. */
	TWeakObjectPtr<UFurwardSaveSubsystem> ProgressSaveSubsystem;

	/** @brief Дескриптор подписки на результаты записи. */
	FDelegateHandle ProgressSaveListener;

	/** @brief Слот, открытый при входе или принятый при первой записи. */
	FName ProgressSlot;

	/** @brief Действие, которое ждёт сохранения актуальной ревизии. */
	EProgressSaveAction PendingSaveAction = EProgressSaveAction::None;

	/** @brief Строка, которую сессия подготавливает после записи при продолжении этапов. */
	FName PendingNextStageID;

	/** @brief Состояние продолжения текущего завершённого этапа для HUD. */
	ECombatNextStageState NextStageState = ECombatNextStageState::Unavailable;

	/**
	 * @brief Зачисляет победу один раз и завершает этап либо подготавливает новое столкновение.
	 * @pre Правила загружены, фаза ChangingEnemy, уведомления о смерти завершены.
	 * @post Профиль применяет результат один раз по EncounterID. При зачистке 10/10 в экспедиции появляется босс.
	 * @note При заданном слоте снимок отправляется в очередь сохранений.
	 */
	void ContinueAfterVictory();

	/**
	 * @brief Выбирает случайного противника из состава этапа или назначенного босса.
	 * @param[in] bBossPending true, если профиль уже достиг порога зачистки.
	 * @return Обычный противник либо отдельное описание босса. Отсутствующее описание босса даёт nullptr.
	 */
	const UCombatEnemyDefinition* SelectEnemyDefinition(bool bBossPending) const;

	/** @brief Возвращает строку текущего этапа из удерживаемой таблицы. */
	const FCombatStageRow* GetActiveStage() const;

	/**
	 * @brief Запускает ответные удары по задержке и интервалу из правил боя.
	 * @pre Сессия вошла в Fighting и проверяет обоих участников перед запуском.
	 */
	void StartEnemyAttacks();

	/** @brief Отменяет таймер текущего столкновения и освобождает его дескриптор. */
	void StopEnemyAttacks();

	/**
	 * @brief Применяет один ответный удар живого противника по герою.
	 * @param[in] AttackingEncounterID Идентификатор, переданный таймеру при старте.
	 * @note Фаза, актуальность столкновения и готовность участников проверяются при каждом ударе.
	 */
	void HandleEnemyAttack(FGuid AttackingEncounterID);

	/** @brief Дескриптор таймера ответных ударов текущего столкновения. */
	FTimerHandle EnemyAttackTimer;

	/** @brief Загруженные правила, которые сессия удерживает до Shutdown(). */
	UPROPERTY(Transient)
	TObjectPtr<UCombatRulesDefinition> CombatRules;

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

	/** @brief Сторона-источник удара для допуска в кадре первой гибели. */
	enum class ECombatAttackSource : uint8
	{
		None,
		Hero,
		Enemy
	};

	/** @brief Проверяет готовность арены, паузу и участников. В кадре гибели допускает удар погибшего источника по живой цели. */
	bool CanUseCurrentEnemy(const ACombatEnemy* Target, ECombatAttackSource Source = ECombatAttackSource::None) const;

	/** @brief Планирует фиксацию исхода в конце кадра, если это задано в правилах боя. */
	bool DeferEncounterOutcome();

	/** @brief Разрешает последний удар того же столкновения в кадре первой смерти. */
	bool IsEncounterOutcomePendingThisFrame() const;

	/** @brief Обрабатывает одну или обе смерти, относящиеся к ожидаемому столкновению. */
	void ResolveDeferredEncounterOutcome();

	/** @brief Снимает подписку конца кадра и освобождает ожидающий идентификатор. */
	void ResetDeferredEncounterOutcome();

	/** @brief Сохраняет исход победы для идемпотентного применения к профилю. */
	void RecordEnemyVictory(const ACombatEnemy& CurrentEnemy);

	/** @brief Завершает обычную победу над одним противником. */
	void FinalizeEnemyVictory(ACombatEnemy& CurrentEnemy);

	/** @brief Фиксирует поражение и при взаимной гибели учитывает уже подтверждённую победу. */
	void FinalizeHeroDefeat(const ACombatHero& CurrentHero, ACombatEnemy& CurrentEnemy, bool bCreditVictory);

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
	 * @brief Создаёт участников на видимой арене и, когда получены стартовые данные, заполняет их характеристики.
	 * @param[in,out] World Мир, в постоянном уровне которого создаются участники.
	 * @param[in] Arena Арена, зарегистрированная в показанном окружении.
	 * @pre Сессия в Preparing, выбран активный этап.
	 */
	void PrepareStageParticipants(UWorld& World, const ACombatArena& Arena);

	/**
	 * @brief Рассчитывает характеристики противника из ожидаемого баланса строки этапа и множителей вида.
	 * @param[in] Definition Описание вида, который сессия готовится создать.
	 * @param[out] EnemyBase Рассчитанные значения при успехе.
	 * @return true, если этап и описание позволяют подготовить компонент характеристик.
	 */
	bool ResolveEnemyAttributes(const UCombatEnemyDefinition& Definition, FCombatBaseAttributes& EnemyBase) const;

	/** @brief Скопированные данные первого спауна героя, полученные до готовности арены. */
	struct FInitialHeroAttributes
	{
		FCombatBaseAttributes Base;
		float CurrentHealth;
	};

	/** @brief Появляется после приёма стартового набора. Живой герой при смене этапа сохраняет своё здоровье. */
	TOptional<FInitialHeroAttributes> InitialHeroAttributes;

	/** @brief Открывает ввод после заполнения характеристик обоих участников. */
	void EnterStageReady();

	/**
	 * @brief Создаёт и регистрирует следующего противника после проверки его характеристик.
	 * @param[in] Definition Описание нового вида.
	 * @param[in] EnemyBase Рассчитанные базовые характеристики.
	 * @return true после замены. При сбое подготовки предыдущий противник остаётся зарегистрирован.
	 */
	bool ReplaceCurrentEnemy(const UCombatEnemyDefinition& Definition, const FCombatBaseAttributes& EnemyBase);

	/**
	 * @brief Объект управления подуровнем от принятия запроса до Shutdown().
	 *
	 * Сессия держит его для получения OnLevelShown и запроса выгрузки.
	 * Transient исключает временную ссылку из сохранения. Пустая ссылка означает, что сессия не отслеживает подуровень.
	 */
	UPROPERTY(Transient)
	TObjectPtr <ULevelStreamingDynamic> EnvironmentStreaming = nullptr;

	/** @brief Ссылка на прежнее окружение до его скрытия. Владение остаётся у мира. */
	TWeakObjectPtr<ULevelStreamingDynamic> RetiringEnvironment;

	/** @brief Наблюдает за состоянием подгрузки только во время подготовки. */
	FTimerHandle EnvironmentLoadTimer;

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
	 * @brief Принимает смерть текущего противника и копирует гарантированные валюты в результат.
	 * @param[in] Source Компонент характеристик, опубликовавший OnDeath после обнуления HP.
	 * @post Для текущего погибшего врага сессия отключает области клика и сохраняет результат один раз.
	 * Обычная победа ведёт в ChangingEnemy. После подтверждённой победы над боссом сессия входит в StageCompleted.
	 */
	void HandleEnemyDeath(const UCombatAttributesComponent& Source);

	/** @brief Передаёт подтверждённый результат профилю для однократного начисления валют и прогресса. */
	bool ApplyEncounterResult(UFurwardProfileSubsystem& Profile);

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

	/** @brief Лут текущего вида. При победе сессия копирует результат до удаления актора. */
	UPROPERTY(Transient)
	TObjectPtr<UFurwardLootDefinition> EnemyLoot;

	/** @brief Показывает, относится ли текущий экземпляр врага к описанию босса этапа. */
	bool bCurrentEnemyIsBoss = false;

	/** @brief Результат после смерти противника. Сохраняется до регистрации следующего или завершения сессии. */
	TOptional<FCombatEncounterResult> EncounterResult;

	/** @brief Отмечает, что профиль уже принял результат до повторяемой подготовки цели. */
	bool bEncounterResultApplied = false;

	/** @brief Уведомляет интерфейс после того, как компонент участника записал новое HP. */
	void HandleParticipantHealthChanged(float Previous, float Current);

	/** @brief Подписчики HUD на время жизни сессии. */
	FCombatParticipantHealthChanged ParticipantHealthChanged;

	/** @brief Подписчики уведомлений о балансе валют для HUD. */
	FCombatCurrencyBalancesChanged CurrencyBalancesChanged;

	/** @brief Подписчики панели прогресса на время жизни сессии. */
	FCombatProgressionChanged ProgressionChanged;

	/** @brief Варианты модальных окон, которые приостанавливают ход боя. */
	enum class ECombatOverlay : uint8
	{
		None,
		LeaveConfirmation,
		PauseMenu
	};

	/** @brief Текущее модальное окно. Сессия допускает одно окно за раз. */
	ECombatOverlay ActiveOverlay = ECombatOverlay::None;

	/** @brief Режим и фаза боя сохраняются, пока окно блокирует команды. */
	bool IsCombatInputSuspended() const { return ActiveOverlay != ECombatOverlay::None; }

	/** @brief Меняет состояние своего окна и пересчитывает боевое время. */
	bool SetCombatOverlay(ECombatOverlay Overlay, bool bOpen);

	/** @brief Подписчики результата неудачной записи во время Leaving. */
	FCombatExitSaveFailed ExitSaveFailed;

	/** @brief Подписчики события поражения текущей сессии. */
	FCombatDefeatRecorded DefeatRecorded;

	/** @brief Снимок противника при поражении. Shutdown() очищает его вместе с результатом боя. */
	TOptional<FCombatDefeatSnapshot> DefeatSnapshot;

	/** @brief Идентификатор столкновения до разрешения смерти в конце этого кадра. */
	FGuid DeferredOutcomeEncounterID;

	/** @brief Номер кадра, в котором погиб первый участник. */
	uint64 DeferredOutcomeFrame = 0;

	/** @brief Одноразовая подписка на конец игрового кадра, снимается при завершении сессии. */
	FDelegateHandle DeferredOutcomeListener;
};