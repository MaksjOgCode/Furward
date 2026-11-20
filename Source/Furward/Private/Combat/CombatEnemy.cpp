// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Настраивает модель и области клика противника и отключает области после смерти.
 */

#include "Combat/CombatEnemy.h"
#include "Combat/CombatAttributesComponent.h"
#include "Combat/CombatEnemyDefinition.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"

namespace
{
/**
 * @brief Подготавливает компонент для прямой трассировки после применения описания вида.
 * @param[in,out] Area Компонент области, созданный конструктором противника.
 * @param[in] Root Корень актора, за которым следует область.
 * @param[in] Color Цвет рамки области в редакторе.
 */
void ConfigureClickArea(UBoxComponent& Area, USceneComponent* Root, FColor Color)
{
	// Область следует за корнем противника. Размеры поступят из Data Asset при спауне.
	Area.SetupAttachment(Root);
	Area.SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Общие запросы сцены и события пересечения пропускают эту область. Контроллер трассирует её напрямую.
	Area.SetCollisionResponseToAllChannels(ECR_Ignore);
	Area.SetGenerateOverlapEvents(false);
	Area.ShapeColor = Color;
}

/**
 * @brief Настраивает геометрию одной области и включает прямые запросы при заданном объёме.
 * @param[in,out] Area Компонент, принадлежащий противнику.
 * @param[in] Center Центр относительно корня актора.
 * @param[in] Extent Полуразмеры области в сантиметрах.
 */
void ApplyClickArea(UBoxComponent& Area, const FVector& Center, const FVector& Extent)
{
	// Размеры из ассета относятся к корню актора и задают положение и объём области.
	Area.SetRelativeLocation(Center);
	Area.SetBoxExtent(Extent);
	// Для коллизии нужны положительные полуразмеры по всем осям. Для полного объёма достаточно QueryOnly.
	Area.SetCollisionEnabled(Extent.GetMin() > 0.0 ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}
}

ACombatEnemy::ACombatEnemy()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")));

	// У каждого противника своё состояние здоровья. Компонент создаётся вместе с актором.
	Attributes = CreateDefaultSubobject<UCombatAttributesComponent>(TEXT("Attributes"));

	// Модель даёт внешний вид. Коллизии для клика принадлежат отдельным компонентам HitArea и MissArea.
	StaticVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticVisual"));
	StaticVisual->SetupAttachment(GetRootComponent());
	StaticVisual->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	// Зелёная рамка Hit и жёлтая рамка Miss помогают различать области в редакторе.
	HitArea = CreateDefaultSubobject<UBoxComponent>(TEXT("HitArea"));
	ConfigureClickArea(*HitArea, GetRootComponent(), FColor::Green);

	MissArea = CreateDefaultSubobject<UBoxComponent>(TEXT("MissArea"));
	ConfigureClickArea(*MissArea, GetRootComponent(), FColor::Yellow);
}

UCombatAttributesComponent& ACombatEnemy::GetAttributes() const
{
	return *Attributes;
}

void ACombatEnemy::ApplyDefinition(const UCombatEnemyDefinition& Definition)
{
	// Трансформ модели из описания сохраняет авторский поворот, масштаб и смещение от точки спауна.
	StaticVisual->SetStaticMesh(Definition.GetStaticMesh());
	StaticVisual->SetRelativeTransform(Definition.GetStaticMeshTransform());

	// Обе области используют общий центр и собственные полуразмеры из описания. Они готовы к проверке после спауна.
	ApplyClickArea(*HitArea, Definition.GetClickAreaCenter(), Definition.GetHitAreaExtent());
	ApplyClickArea(*MissArea, Definition.GetClickAreaCenter(), Definition.GetMissAreaExtent());
}

UBoxComponent& ACombatEnemy::GetHitArea() const
{
	return *HitArea;
}

UBoxComponent& ACombatEnemy::GetMissArea() const
{
	return *MissArea;
}

void ACombatEnemy::DisableClickAreas()
{
	HitArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MissArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
