// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает настройки вида противника, из которых сессия выбирает класс, внешний вид, области клика, множители характеристик и награды.
 */

#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatBaseAttributes.h"
#include "Engine/DataAsset.h"
#include "Templates/SubclassOf.h"
#include "CombatEnemyDefinition.generated.h"

class ACombatEnemy;
class UFurwardLootDefinition;
class UStaticMesh;

/**
 * @brief Хранит данные одного вида, которые читают при создании противника.
 *
 * Автор контента создаёт экземпляр DA_Combat_Enemy_<Name> в редакторе.
 * GameMode и сессия удерживают выбранное стартовое описание на время боя.
 */
UCLASS()
class UCombatEnemyDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * @brief Даёт сессии класс для создания противника.
	 * @return Выбранный класс или пустое значение, если его не назначили.
	 */
	TSubclassOf<ACombatEnemy> GetEnemyClass() const { return EnemyClass; }

	/**
	 * @brief Даёт противнику модель перед завершением спауна.
	 * @return Назначенный меш или nullptr, если описание неполное.
	 */
	UStaticMesh* GetStaticMesh() const { return StaticMesh.Get(); }

	/**
	 * @brief Даёт противнику авторское размещение модели.
	 * @return Трансформ модели относительно корня актора.
	 */
	const FTransform& GetStaticMeshTransform() const { return StaticMeshTransform; }

	/**
	 * @brief Даёт противнику общий центр областей при применении данных вида.
	 * @return Смещение центра от корня актора в сантиметрах.
	 */
	const FVector& GetClickAreaCenter() const { return ClickAreaCenter; }

	/**
	 * @brief Даёт противнику размеры области попадания.
	 * @return Полуразмеры по локальным осям в сантиметрах. Ноль по любой оси отключает область.
	 */
	const FVector& GetHitAreaExtent() const { return HitAreaExtent; }

	/**
	 * @brief Даёт противнику размеры области промаха.
	 * @return Полуразмеры по локальным осям в сантиметрах. Ноль по любой оси отключает область.
	 */
	const FVector& GetMissAreaExtent() const { return MissAreaExtent; }

	/** @brief Возвращает множители, применяемые к ожидаемым характеристикам этапа при спауне. */
	const FCombatEnemyAttributeMultipliers& GetAttributeMultipliers() const { return AttributeMultipliers; }

	/** @brief Возвращает назначенный набор лута. Пустая ссылка оставляет победу без валютной награды. */
	UFurwardLootDefinition* GetLootDefinition() const { return LootDefinition.Get(); }

private:
	/** @brief Ассет лута вида. Обычный мутант использует эту ссылку в экспедиции и фарме. */
	UPROPERTY(EditAnywhere, Category = "Combat|Rewards")
	TObjectPtr<UFurwardLootDefinition> LootDefinition;

	/** @brief Относительные HP, урон и броня этого вида. Итоговые значения рассчитывает сессия. */
	UPROPERTY(EditAnywhere, Category = "Combat|Balance")
	FCombatEnemyAttributeMultipliers AttributeMultipliers;

	/**
	 * @brief Класс актора для данного вида, обычно BP_Combat_Enemy.
	 *
	 * Назначается в ассете. Без класса сессия прекращает спаун и пишет ошибку.
	 */
	UPROPERTY(EditAnywhere, Category = "Combat|Enemy")
	TSubclassOf<ACombatEnemy> EnemyClass;

	/**
	 * @brief Модель, выбранная автором вида при настройке Data Asset.
	 *
	 * Ссылка удерживает ассет модели. При пустой ссылке сессия пишет ошибку и пропускает спаун.
	 */
	UPROPERTY(EditAnywhere, Category = "Combat|Enemy")
	TObjectPtr<UStaticMesh> StaticMesh;

	/**
	 * @brief Размещение модели относительно SceneRoot, настроенное в Data Asset.
	 *
	 * Identity означает исходный поворот и масштаб модели без дополнительного смещения.
	 */
	UPROPERTY(EditAnywhere, Category = "Combat|Enemy")
	FTransform StaticMeshTransform = FTransform::Identity;

	/**
	 * @brief Общий центр областей попадания и промаха в координатах актора.
	 *
	 * Настраивается в Data Asset. По умолчанию центр совпадает с корнем.
	 */
	UPROPERTY(EditAnywhere, Category = "Combat|Click Areas")
	FVector ClickAreaCenter = FVector::ZeroVector;

	/**
	 * @brief Полуразмеры HitArea, заданные в Data Asset в сантиметрах.
	 *
	 * При нуле по любой оси актор оставляет область выключенной.
	 */
	UPROPERTY(EditAnywhere, Category = "Combat|Click Areas", meta = (ClampMin = "0.0"))
	FVector HitAreaExtent = FVector::ZeroVector;

	/**
	 * @brief Полуразмеры MissArea, заданные в Data Asset в сантиметрах.
	 *
	 * Контроллер проверяет эту область после HitArea. Ноль по любой оси оставляет область выключенной.
	 */
	UPROPERTY(EditAnywhere, Category = "Combat|Click Areas", meta = (ClampMin = "0.0"))
	FVector MissAreaExtent = FVector::ZeroVector;
};
