// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает результат победы и снимок столкновения при поражении.
 */

#pragma once

#include "Containers/Map.h"
#include "Misc/Guid.h"
#include "UObject/PrimaryAssetId.h"

/**
 * @brief Фиксирует победу над противником как значение, пригодное после удаления актора.
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

	/** @brief Признак победы над боссом этапа для отдельной обработки результата. */
	bool bDefeatedBoss = false;

	/** @brief Копия рассчитанных валют по идентификаторам, которую профиль применяет один раз. */
	TMap<FName, int64> CurrencyRewards;
};

/**
 * @brief Сохраняет здоровье текущего противника на момент поражения героя для окна итогов.
 *
 * Сессия создаёт снимок после OnDeath героя и хранит значения до завершения сессии.
 */
struct FCombatDefeatSnapshot
{
	/** @brief Текущее здоровье противника на момент поражения героя. */
	float EnemyCurrentHealth = 0.0f;

	/** @brief Максимальное здоровье противника в завершившемся столкновении. */
	float EnemyMaxHealth = 0.0f;
};
