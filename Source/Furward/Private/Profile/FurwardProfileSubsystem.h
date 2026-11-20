// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает зачистку, завершение этапов и создание снимка прогресса профиля.
 */

#pragma once

#include "CoreMinimal.h"
#include "Profile/FurwardProfileSnapshot.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FurwardProfileSubsystem.generated.h"

/**
 * @brief Владеет зачисткой и завершёнными этапами на время работы GameInstance.
 *
 * Unreal создаёт экземпляр автоматически. Он хранит значения между сменами карты и боевой сессии.
 * Боевой код передаёт подтверждённый EncounterID и имя строки этапа.
 */
UCLASS()
class UFurwardProfileSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** @brief Количество побед над обычными противниками, после которого предстоит бой с боссом. */
	static constexpr int32 RequiredExpeditionClears = 10;

	/**
	 * @brief Учитывает одну победу над обычным противником экспедиции.
	 * @param[in] StageID Имя строки боевого этапа из DT_Combat_Stages.
	 * @param[in] EncounterID Уникальный идентификатор завершённого столкновения.
	 * @return true, если зачистка выросла. Пустые данные, повторная победа и полный счётчик дают false.
	 * @post При успехе профиль хранит увеличенный счётчик и помнит EncounterID.
	 */
	bool TryRecordExpeditionClear(FName StageID, const FGuid& EncounterID);

	/**
	 * @brief Возвращает зачистку выбранного этапа для определения следующего противника.
	 * @param[in] StageID Имя строки боевого этапа.
	 * @return Количество учтённых побед. Новый этап начинается с нуля.
	 */
	int32 GetExpeditionClearCount(FName StageID) const;

	/**
	 * @brief Проверяет, пройдена ли зачистка обычных противников этапа.
	 * @param[in] StageID Имя строки боевого этапа.
	 * @return true при 10/10. Победа над боссом учитывается отдельно.
	 */
	bool IsExpeditionClearComplete(FName StageID) const;

	/**
	 * @brief Учитывает победу над боссом после полной зачистки этапа.
	 * @param[in] StageID Имя строки этапа, на котором побеждён босс.
	 * @param[in] EncounterID Уникальный идентификатор столкновения с боссом.
	 * @return true, если этап впервые отмечен завершённым. Неполная зачистка, неверные данные и дубликаты дают false.
	 * @post При успехе профиль хранит завершённый этап и EncounterID. Зачистка остаётся на 10/10.
	 */
	bool TryCompleteExpeditionStage(FName StageID, const FGuid& EncounterID);

	/**
	 * @brief Проверяет, засчитана ли победа над боссом этапа.
	 * @param[in] StageID Имя строки боевого этапа.
	 * @return true после применения результата победы над боссом.
	 */
	bool IsExpeditionStageCompleted(FName StageID) const;

	/**
	 * @brief Копирует текущий прогресс и его ревизию для запроса сохранения.
	 * @return Снимок, который сохраняет значения на момент вызова.
	 */
	FFurwardProfileSnapshot CreateSnapshot() const;

	/** @brief Возвращает ревизию прогресса без копирования снимка. Ноль соответствует новому профилю. */
	int64 GetRevision() const { return Progress.Revision; }

	/**
	 * @brief Проверяет и восстанавливает прогресс из загруженного снимка.
	 * @param[in] Snapshot Данные поддерживаемой версии сохранения.
	 * @return true, если счётчики, завершённые этапы, идентификаторы и ревизия согласованы.
	 * @post При успехе профиль принимает весь снимок. При ошибке профиль сохраняет текущее состояние.
	 */
	bool TryRestoreSnapshot(const FFurwardProfileSnapshot& Snapshot);

private:
	/**
	 * @brief Проверяет идентификаторы этапа и победы перед изменением профиля.
	 * @param[in] StageID Имя строки этапа.
	 * @param[in] EncounterID Идентификатор столкновения.
	 * @return true, если оба идентификатора заданы и профиль ещё не засчитал столкновение.
	 */
	bool CanRecordEncounter(FName StageID, const FGuid& EncounterID) const;

	/** @brief Текущее состояние профиля. Каждая засчитанная победа увеличивает ревизию на единицу. */
	FFurwardProfileSnapshot Progress;
};
