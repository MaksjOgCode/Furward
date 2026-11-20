// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает данные о завершённом столкновении, которые сессия передаёт для дальнейшей обработки.
 */

#pragma once

#include "Misc/Guid.h"
#include "UObject/PrimaryAssetId.h"

/**
 * @brief Фиксирует победу над обычным противником как значение, пригодное после удаления актора.
 *
 * Сессия создаёт результат после OnDeath. EncounterID отличает повторную обработку той же победы
 * от победы над другим экземпляром противника с тем же описанием.
 */
struct FCombatEncounterResult
{
	/** @brief Идентификатор столкновения, выделенный сессией при регистрации противника. */
	FGuid EncounterID;

	/** @brief Идентификатор Data Asset вида побеждённого противника. */
	FPrimaryAssetId EnemyDefinitionID;
};
