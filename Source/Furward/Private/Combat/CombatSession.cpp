// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Читает строку этапа, подгружает окружение и размещает участников для боевой сессии.
 */

#include "Combat/CombatSession.h"
#include "Combat/CombatArena.h"
#include "Combat/CombatAttributesComponent.h"
#include "Combat/CombatEnemy.h"
#include "Combat/CombatEnemyDefinition.h"
#include "Combat/CombatEnvironmentDefinition.h"
#include "Combat/CombatHero.h"
#include "Combat/CombatPlayerController.h"
#include "Combat/CombatStageRow.h"
#include "Engine/Level.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"



DEFINE_LOG_CATEGORY_STATIC( LogCombatSession, Log, All );



void UCombatSession::RequestInitialStage(UObject* WorldContextObject, const FDataTableRowHandle& StageHandle)
{
	// В Preparing допускаем один отслеживаемый запрос на начальное окружение.
	if (Phase != ECombatPhase::Preparing || EnvironmentStreaming) {
		UE_LOG( LogCombatSession, Log, TEXT("Initial environment request is not available.") );
		return;
	}

	// Таблица задаёт контент этапа по имени строки. Ошибки настройки оставляют сессию в Preparing.
	const FCombatStageRow* Stage = StageHandle.GetRow<FCombatStageRow>(TEXT("Initial combat stage"));
	if (!Stage || !IsValid(Stage->Environment.Get()) || Stage->Environment->GetEnvironmentMap().IsNull() ||
		Stage->EnemyRoster.IsEmpty() || !IsValid(Stage->EnemyRoster[0].Get()))
	{
		UE_LOG(LogCombatSession, Warning, TEXT("Initial combat stage %s requires a row, environment map and first enemy."),
			*StageHandle.RowName.ToString());
		return;
	}

	const TSoftObjectPtr<UWorld>& EnvironmentMap = Stage->Environment->GetEnvironmentMap();
	// Контекст выбирает мир, мягкая ссылка выбирает карту, а Identity сохраняет авторскую расстановку.
	bool RequestAccepted {};
	ULevelStreamingDynamic* Streaming = (
		ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(
			WorldContextObject,
			EnvironmentMap,
			FTransform::Identity,
			RequestAccepted
		)
	);

	// При отказе сессия остаётся в Preparing. Путь в логе указывает, какую карту запрашивали.
	if ( !RequestAccepted || !IsValid(Streaming) ) {
		UE_LOG( LogCombatSession, Error, TEXT("Could not request environment: %s"), *EnvironmentMap.ToSoftObjectPath().ToString() );
		return;
	}

	// Запоминаем объект, чтобы принять уведомление о показе и позже запросить выгрузку.
	EnvironmentStreaming = Streaming;
	InitialEnemy = Stage->EnemyRoster[0];

	// Unreal вызовет обработчик, когда добавит подуровень в мир и сделает его видимым.
	Streaming->OnLevelShown.AddDynamic(this, &UCombatSession::HandleEnvironmentShown);

	// Лог подтверждает приём запроса. Проверка видимости и регистрация арены произойдут в обработчике.
	UE_LOG(LogCombatSession, Display, TEXT("Initial combat stage: %s; Environment: %s; First enemy: %s"),
		*StageHandle.RowName.ToString(), *Stage->Environment->GetName(), *InitialEnemy->GetName());
	UE_LOG( LogCombatSession, Display, TEXT("Environment requested: %s"), *EnvironmentMap.ToSoftObjectPath().ToString() );
}

void UCombatSession::HandleEnvironmentShown()
{
    // Принимаем событие во время подготовки, пока подуровень видим и арена ещё не зарегистрирована.
    if (  Phase != ECombatPhase::Preparing        ||
         !IsValid(EnvironmentStreaming.Get())    ||
         !EnvironmentStreaming->IsLevelVisible() ||
          ActiveArena.IsValid() )
    {
        return;
    }

    UE_LOG( LogCombatSession, Display, TEXT("Environment shown: %s"), *EnvironmentStreaming->GetWorldAssetPackageName() );

    // Ищем арену в показанном подуровне, к которому относится уведомление.
    ULevel*       Level      = EnvironmentStreaming->GetLoadedLevel();
    ACombatArena* FoundArena = nullptr;
    int32         ArenaCount = 0;

    // Считаем экземпляры ACombatArena и её наследников, которые ещё действительны.
    for (AActor* Actor : Level->Actors) {
		if (ACombatArena* Arena = Cast <ACombatArena>(Actor); IsValid(Arena) ) {
            FoundArena = Arena;
            ++ArenaCount;
        }
    }

    // Источник камеры и точек спауна должен быть однозначным. При другой расстановке пишем число арен и останавливаем подготовку.
    if (ArenaCount != 1) {
        UE_LOG( LogCombatSession, Error, TEXT("Expected one combat arena in the environment, found %d."), ArenaCount );
        return;
    }

    // Регистрируем арену как источник ракурса и координат текущих участников.
    ActiveArena = FoundArena;
    UE_LOG( LogCombatSession, Display, TEXT("Arena registered: %s"), *FoundArena->GetName() );

    // Ищем боевой контроллер в том же мире. Без него подготовка остаётся незавершённой.
    UWorld* World = Level->GetWorld();
    ACombatPlayerController* Controller = World ? World->GetFirstPlayerController <ACombatPlayerController> () : nullptr;

    if ( !IsValid(Controller) ) {
        UE_LOG( LogCombatSession, Error, TEXT("Combat player controller unavailable.") );
        return;
    }

    // Переключаем камеру и создаём участников по точкам арены на постоянном уровне.
    Controller->UseArenaCamera(FoundArena);
    SpawnHero(*World, *FoundArena);
    SpawnEnemy(*World, *FoundArena);

    // Готовая расстановка остаётся в Preparing, пока сессия не получит характеристики обоих участников.
    if (Hero.IsValid() && Enemy.IsValid())
    {
        UE_LOG(LogCombatSession, Display, TEXT("Combat phase: Preparing; waiting for participant attributes."));
    }
}

void UCombatSession::SpawnHero(UWorld& World, const ACombatArena& Arena)
{
	// Сохраняем уже созданного героя при повторном вызове.
	if (Hero.IsValid())
	{
		return;
	}

	// Класс берём из настроек GameMode. При пустой настройке сообщаем об ошибке спауна.
	if (!HeroClass)
	{
		UE_LOG(LogCombatSession, Error, TEXT("Combat hero class is not configured."));
		return;
	}

	// Точка берётся с подуровня, а герой принадлежит L_Combat. Выгрузка окружения его не удаляет.
	FActorSpawnParameters SpawnParams;
	SpawnParams.OverrideLevel = World.PersistentLevel.Get();

	ACombatHero* SpawnedHero = World.SpawnActor<ACombatHero>(
		HeroClass.Get(), Arena.GetSpawnHeroTransform(), SpawnParams);

	// При сбое спауна Hero остаётся пустым и сцена не перейдёт в Ready.
	if (!IsValid(SpawnedHero))
	{
		UE_LOG(LogCombatSession, Error, TEXT("Could not spawn the combat hero."));
		return;
	}

	// Храним слабую ссылку. В логе указываем экземпляр и пакет уровня для проверки размещения.
	Hero = SpawnedHero;

	UE_LOG(LogCombatSession, Display, TEXT("Hero spawned: %s in %s"),
		*SpawnedHero->GetName(), *SpawnedHero->GetLevel()->GetOutermost()->GetName());
}

void UCombatSession::SpawnEnemy(UWorld& World, const ACombatArena& Arena)
{
	// Пока сессия отслеживает действующего противника, второй экземпляр не нужен.
	if (Enemy.IsValid())
	{
		return;
	}

	// Для спауна должны быть назначены описание вида, класс противника и модель.
	if (!IsValid(InitialEnemy.Get()) || !InitialEnemy->GetEnemyClass() || !IsValid(InitialEnemy->GetStaticMesh()))
	{
		UE_LOG(LogCombatSession, Error, TEXT("Initial enemy definition must specify a class and a static mesh."));
		return;
	}

	// Берём мировую точку арены и создаём актор в постоянном L_Combat.
	// Откладываем завершение спауна, чтобы применить настройки вида до запуска актора.
	const FTransform SpawnTransform = Arena.GetSpawnEnemyTransform();
	FActorSpawnParameters SpawnParams;
	SpawnParams.OverrideLevel = World.PersistentLevel.Get();
	SpawnParams.bDeferConstruction = true;

	ACombatEnemy* SpawnedEnemy = World.SpawnActor<ACombatEnemy>(
		InitialEnemy->GetEnemyClass().Get(), SpawnTransform, SpawnParams);

	// При ошибке оставляем Enemy пустым, в лог попадает причина остановки спауна.
	if (!IsValid(SpawnedEnemy))
	{
		UE_LOG(LogCombatSession, Error, TEXT("Could not spawn the combat enemy."));
		return;
	}

	// Устанавливаем модель и области клика до FinishSpawning(), затем регистрируем экземпляр как текущую цель.
	SpawnedEnemy->ApplyDefinition(*InitialEnemy);
	SpawnedEnemy->FinishSpawning(SpawnTransform);
	Enemy = SpawnedEnemy;

	UE_LOG(LogCombatSession, Display, TEXT("Enemy spawned: %s in %s using %s"),
		*SpawnedEnemy->GetName(), *SpawnedEnemy->GetLevel()->GetOutermost()->GetName(), *InitialEnemy->GetName());
}

void UCombatSession::Initialize(TSubclassOf<ACombatHero> InHeroClass) {
	// Сохраняем класс героя до появления арены. Outer в логе позволяет связать сессию с её GameMode.
	HeroClass = InHeroClass;
	UE_LOG( LogCombatSession, Log, TEXT("Initialize: %s; Outer: %s"), *GetName(), *GetOuter()->GetName() );
}

const ACombatEnemy* UCombatSession::GetActiveEnemy() const
{
	return Enemy.Get();
}

bool UCombatSession::InitializeParticipantAttributes(const FCombatBaseAttributes& HeroBase, float HeroCurrentHealth,
	const FCombatBaseAttributes& EnemyBase)
{
	// Принимаем данные после появления арены и обоих участников.
	if (Phase != ECombatPhase::Preparing || !Hero.IsValid() || !Enemy.IsValid() || !ActiveArena.IsValid() ||
		!IsValid(EnvironmentStreaming.Get()) || !EnvironmentStreaming->IsLevelVisible())
	{
		return false;
	}

	UCombatAttributesComponent& HeroAttributes = Hero->GetAttributes();
	UCombatAttributesComponent& EnemyAttributes = Enemy->GetAttributes();
	// Проверяем оба набора до записи. Герой получает переданное HP, новый противник начинает с полным HP.
	if (!HeroAttributes.CanInitializeAttributes(HeroBase, HeroCurrentHealth) ||
		!EnemyAttributes.CanInitializeAttributes(EnemyBase, EnemyBase.MaxHealth))
	{
		UE_LOG(LogCombatSession, Warning, TEXT("Participant attributes rejected; combat is still Preparing."));
		return false;
	}

	// Заполняем оба компонента и только затем сообщаем подписчикам о начальном здоровье.
	HeroAttributes.SetInitialAttributes(HeroBase, HeroCurrentHealth);
	EnemyAttributes.SetInitialAttributes(EnemyBase, EnemyBase.MaxHealth);
	Phase = ECombatPhase::Ready;
	UE_LOG(LogCombatSession, Display, TEXT("Combat phase: Ready; Hero HP %.2f/%.2f; Enemy HP %.2f/%.2f"),
		HeroAttributes.GetCurrentHealth(), HeroAttributes.GetMaxHealth(),
		EnemyAttributes.GetCurrentHealth(), EnemyAttributes.GetMaxHealth());
	HeroAttributes.PublishInitialHealth();
	EnemyAttributes.PublishInitialHealth();
	return true;
}

bool UCombatSession::TrySubmitClickAttack(ECombatClickResult Result, const ACombatEnemy* Target)
{
	// Допускаем только результаты, которые образуют команду. Ignore отбрасывается и при прямом вызове метода.
	if (Result != ECombatClickResult::Hit && Result != ECombatClickResult::Miss)
	{
		return false;
	}

	// Перепроверяем сцену в момент запроса. Запрос отклоняется, если герой или арена исчезли либо подуровень скрыт.
	if (Phase != ECombatPhase::Ready || !Hero.IsValid() || !ActiveArena.IsValid() ||
		!IsValid(EnvironmentStreaming.Get()) || !EnvironmentStreaming->IsLevelVisible())
	{
		UE_LOG(LogCombatSession, Verbose, TEXT("Click attack rejected: combat scene is not ready."));
		return false;
	}

	// Сравниваем экземпляры, чтобы результат по прошлой цели не применился к новой.
	if (!IsValid(Target) || Target != Enemy.Get())
	{
		UE_LOG(LogCombatSession, Verbose, TEXT("Click attack rejected: target is not the active enemy."));
		return false;
	}

	// Запрос относится только к живым участникам, даже если актор после смерти ещё находится на карте.
	if (Hero->GetAttributes().IsDead() || Target->GetAttributes().IsDead())
	{
		return false;
	}

	// Если игровой мир на паузе или недоступен, команда не проходит проверку.
	const UWorld* World = Target->GetWorld();
	if (!World || World->IsPaused())
	{
		UE_LOG(LogCombatSession, Verbose, TEXT("Click attack rejected: gameplay input is suspended."));
		return false;
	}

	// Журналируем принятый запрос с именем цели. На этом шаге здоровье и фаза остаются прежними.
	UE_LOG(LogCombatSession, Display, TEXT("Click attack accepted: %s; Target: %s"),
		Result == ECombatClickResult::Hit ? TEXT("Hit") : TEXT("Miss"), *Target->GetName());
	return true;
}

void UCombatSession::Shutdown() {
	// Сначала блокируем новые команды. Постоянный уровень продолжает владеть участниками до завершения мира.
	Phase = ECombatPhase::Leaving;
	Hero.Reset();
	Enemy.Reset();
	InitialEnemy = nullptr;
	ActiveArena.Reset();

	// Расцепляем динамическую подписку до запроса выгрузки, чтобы OnLevelShown больше не готовил сцену.
	if ( IsValid(EnvironmentStreaming.Get()) ) {
		EnvironmentStreaming->OnLevelShown.RemoveDynamic(this, &UCombatSession::HandleEnvironmentShown);
		// Unreal применит запрос на выгрузку и удаление подуровня при обработке стриминга.
		EnvironmentStreaming->SetIsRequestingUnloadAndRemoval(true);
	}

	// Сессия отпускает объект управления после отправки запроса. Shutdown в логе подтверждает завершение сессии.
	EnvironmentStreaming = nullptr;

	UE_LOG( LogCombatSession, Log, TEXT("Shutdown: %s"), *GetName() );
}