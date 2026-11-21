// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает начисление валют и учёт побед в профиле с общим снимком для записи.
 */

#pragma once

#include "CoreMinimal.h"
#include "Profile/FurwardProfileSnapshot.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FurwardProfileSubsystem.generated.h"

/**
 * @brief Владеет балансом валют, зачисткой и завершёнными этапами на время работы GameInstance.
 *
 * Unreal создаёт экземпляр автоматически. Он хранит значения между сменами карты и боевой сессии.
 * Боевой код передаёт подтверждённый EncounterID, имя строки этапа и рассчитанную награду.
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
	 * @param[in] CurrencyRewards Рассчитанные положительные суммы по идентификаторам валют.
	 * @return true, если зачистка и награда применены. При ошибке профиль сохраняет прежнее состояние.
	 * @post При успехе профиль помнит EncounterID и увеличивает ревизию один раз.
	 */
	bool TryRecordExpeditionClear(FName StageID, const FGuid& EncounterID, const TMap<FName, int64>& CurrencyRewards = {});

	/**
	 * @brief Начисляет валюты за подтверждённую победу в фарме и фиксирует её идентификатор.
	 * @param[in] StageID Имя этапа, на котором шёл фарм.
	 * @param[in] EncounterID Уникальный идентификатор завершённого столкновения.
	 * @param[in] CurrencyRewards Рассчитанные положительные суммы по идентификаторам валют.
	 * @return true при первом успешном применении результата.
	 * @post Зачистка и завершение этапов сохраняют свои значения.
	 */
	bool TryRecordFarmVictory(FName StageID, const FGuid& EncounterID, const TMap<FName, int64>& CurrencyRewards);

	/** @brief Возвращает баланс валюты по её идентификатору. Новая валюта имеет нулевой баланс. */
	int64 GetCurrencyBalance(FName CurrencyID) const;

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
	 * @param[in] CurrencyRewards Рассчитанные положительные суммы по идентификаторам валют.
	 * @return true, если этап впервые завершён и награда применена.
	 * @post При успехе профиль хранит завершённый этап и EncounterID. Зачистка остаётся на 10/10.
	 */
	bool TryCompleteExpeditionStage(FName StageID, const FGuid& EncounterID, const TMap<FName, int64>& CurrencyRewards = {});

	/**
	 * @brief Проверяет, засчитана ли победа над боссом этапа.
	 * @param[in] StageID Имя строки боевого этапа.
	 * @return true после применения результата победы над боссом.
	 */
	bool IsExpeditionStageCompleted(FName StageID) const;

	/**
	 * @brief Копирует валюты, прогресс и ревизию для запроса сохранения.
	 * @return Снимок, который сохраняет значения на момент вызова.
	 */
	FFurwardProfileSnapshot CreateSnapshot() const;

	/** @brief Возвращает ревизию профиля без копирования снимка. Ноль соответствует новому профилю. */
	int64 GetRevision() const { return Progress.Revision; }

	/**
	 * @brief Проверяет и восстанавливает прогресс из загруженного снимка.
	 * @param[in] Snapshot Данные поддерживаемой версии сохранения.
	 * @return true, если счётчики, валюты, завершённые этапы, идентификаторы и ревизия согласованы.
	 * @post При успехе профиль принимает весь снимок. При ошибке профиль сохраняет текущее состояние.
	 */
	bool TryRestoreSnapshot(const FFurwardProfileSnapshot& Snapshot);

private:
	/**
	 * @brief Проверяет идентификаторы, повтор результата и возможность начислить весь набор валют.
	 * @param[in] StageID Имя строки этапа.
	 * @param[in] EncounterID Идентификатор столкновения.
	 * @param[in] CurrencyRewards Суммы, которые должны целиком поместиться в балансах.
	 * @return true, если результат можно применить без дубликата или переполнения.
	 */
	bool CanRecordEncounter(FName StageID, const FGuid& EncounterID, const TMap<FName, int64>& CurrencyRewards) const;

	/** @brief После общей проверки начисляет валюты, запоминает столкновение и увеличивает ревизию. */
	void ApplyVictory(const FGuid& EncounterID, const TMap<FName, int64>& CurrencyRewards);

	/** @brief Текущее состояние профиля. Каждая засчитанная победа увеличивает ревизию на единицу. */
	FFurwardProfileSnapshot Progress;
};
