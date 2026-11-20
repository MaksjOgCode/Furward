// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает открытие слота профиля, очередь записи снимков и подтверждение ревизий.
 */

#pragma once

#include "CoreMinimal.h"
#include "Misc/Optional.h"
#include "Profile/FurwardProfileSnapshot.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FurwardSaveSubsystem.generated.h"

class UFurwardProfileSubsystem;

/** @brief Состояние записи одного слота в текущем GameInstance. */
struct FFurwardSaveStatus
{
	/** @brief Наибольшая ревизия, принятая из файла или в очередь. Минус один означает отсутствие данных. */
	int64 LatestRequestedRevision = -1;

	/** @brief Последняя ревизия, подтверждённая чтением или записью файла. Минус один означает отсутствие подтверждения. */
	int64 LastSavedRevision = -1;

	/** @brief Показывает, что последнее запрошенное состояние требует повторной записи. */
	bool bLatestWriteFailed = false;

	/**
	 * @brief Проверяет, достигла ли запись актуального запроса и требуемого состояния профиля.
	 * @param[in] RequiredRevision Ревизия профиля, которую должен дождаться вызывающий код.
	 * @return true, если последняя запрошенная ревизия записана и не старше требуемой.
	 */
	bool IsLatestRevisionSaved(int64 RequiredRevision) const
	{
		return RequiredRevision >= 0 && !bLatestWriteFailed &&
			LastSavedRevision >= RequiredRevision && LastSavedRevision == LatestRequestedRevision;
	}
};

/** @brief Сообщает слот, ревизию и итог конкретной попытки после обновления статуса слота. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FFurwardSaveFinished, FName, int64, bool);

/**
 * @brief Открывает слот профиля местного игрока и последовательно записывает снимки.
 *
 * Unreal создаёт подсистему вместе с GameInstance. Подсистема хранит копии ожидающих снимков
 * и запускает по одной операции. Профиль и бой продолжают жить независимо от записи.
 */
UCLASS()
class UFurwardSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** @brief Индекс единственного локального пользователя, которому принадлежат слоты. */
	static constexpr int32 LocalUserIndex = 0;

	/**
	 * @brief Привязывает новый профиль к свободному слоту или восстанавливает данные из существующего файла.
	 * @param[in] SlotName Имя слота без пути.
	 * @param[in,out] Profile Профиль того же GameInstance, которому передаётся снимок для проверки.
	 * @pre Вызывается в игровом потоке до изменения профиля. Повторный вход с открытым слотом использует профиль в памяти.
	 * @return true при успешном открытии. Ошибка чтения, смена активного слота и несогласованный снимок дают false.
	 * @post Подтверждённая ревизия существующего файла отмечена в статусе записи.
	 */
	bool TryOpenProfileSlot(FName SlotName, UFurwardProfileSubsystem& Profile);

	/**
	 * @brief Ставит снимок в очередь или повторяет запись ревизии после ошибки.
	 * @param[in] SlotName Имя слота без пути, в котором хранится выбранный профиль.
	 * @param[in] Snapshot Значения профиля на момент запроса.
	 * @pre Вызывается в игровом потоке для нового слота или того же профиля, который загружен из этого слота.
	 * @return true, если ревизия принята, уже записана или ожидает записи. Устаревший или неверный запрос даёт false.
	 */
	bool RequestSave(FName SlotName, FFurwardProfileSnapshot Snapshot);

	/**
	 * @brief Возвращает копию статуса записи слота.
	 * @param[in] SlotName Имя слота выбранного профиля.
	 * @return Статус, полученный при открытии файла или записи, либо исходный статус для неизвестного слота.
	 */
	FFurwardSaveStatus GetSaveStatus(FName SlotName) const;

	/**
	 * @brief Даёт другим системам подписаться на итог попытки записи.
	 * @return Делегат, который вызывается в игровом потоке.
	 */
	FFurwardSaveFinished& OnSaveFinished() { return SaveFinished; }

	/** @brief Прекращает приём запросов и освобождает очередь и подписки при завершении GameInstance. */
	virtual void Deinitialize() override;

private:
	/** @brief Копия данных одной попытки записи и признаки, связывающие её с callback. */
	struct FSaveRequest
	{
		/** @brief Имя слота, для которого принят снимок. */
		FName SlotName;
		/** @brief Копия прогресса, полученная в RequestSave(). */
		FFurwardProfileSnapshot Snapshot;
		/** @brief Отличает повторную попытку от предыдущей записи той же ревизии. */
		FGuid RequestID;
	};

	/** @brief Запускает следующий ожидающий запрос, когда завершена текущая попытка. */
	void StartNextSave();

	/**
	 * @brief Подтверждает результат текущей попытки и продолжает обработку очереди.
	 * @param[in] SlotName Имя слота, которое вернул движок.
	 * @param[in] UserIndex Индекс локального пользователя из callback Unreal.
	 * @param[in] bSuccess Результат записи на платформе.
	 * @param[in] RequestID Идентификатор попытки из StartNextSave().
	 */
	void HandleSaveFinished(const FString& SlotName, int32 UserIndex, bool bSuccess, FGuid RequestID);

	/** @brief Последняя запрошенная и подтверждённая ревизии каждого слота. */
	TMap<FName, FFurwardSaveStatus> StatusBySlot;

	/** @brief Слот профиля, выбранный до начала игры. Сохраняется при смене карты в том же GameInstance. */
	FName OpenedProfileSlot;

	/** @brief Принятые снимки в порядке вызовов RequestSave(). */
	TArray<FSaveRequest> PendingSaves;

	/** @brief Единственный запрос, переданный движку в данный момент. */
	TOptional<FSaveRequest> ActiveSave;

	/** @brief Слушатели результата записи на время работы подсистемы. */
	FFurwardSaveFinished SaveFinished;

	/** @brief Защищает запуск очереди от синхронного callback ошибки при старте. */
	bool bStartingSaves = false;

	/** @brief Блокирует новые запросы и поздние callback после Deinitialize(). */
	bool bDeinitializing = false;
};
