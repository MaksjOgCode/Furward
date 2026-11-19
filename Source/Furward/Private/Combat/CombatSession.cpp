// Copyright © 2026 Maksj.OG

#include "Combat/CombatSession.h"
#include "Combat/CombatArena.h"
#include "Combat/CombatPlayerController.h"
#include "Engine/Level.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"



DEFINE_LOG_CATEGORY_STATIC( LogCombatSession, Log, All );



void UCombatSession::RequestInitialEnvironment(UObject * WorldContextObject, const TSoftObjectPtr <UWorld>& EnvironmentMap) {
	// Повторный запрос пропускаем, если сессия уже отслеживает подгружаемый уровень:
	if (EnvironmentStreaming) {
		UE_LOG( LogCombatSession, Log, TEXT("An environment is already tracked") );
		return;
	}

	// Аргумент WorldContextObject помогает Unreal найти мир, в который нужно подгрузить карту
	// Аргумент EnvironmentMap указывает, какую карту подгрузить
	// Аргумент Identity сохраняет исходное размещение уровня без дополнительного смещения
	bool RequestAccepted {};
	ULevelStreamingDynamic* Streaming = (
		ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(
			WorldContextObject,
			EnvironmentMap,
			FTransform::Identity,
			RequestAccepted
		)
	);

	// Если запрос отклонён или не вернулся пригодный объект управления подуровнем:
	if ( !RequestAccepted || !IsValid(Streaming) ) {
		UE_LOG( LogCombatSession, Error, TEXT("Could not request environment: %s"), *EnvironmentMap.ToSoftObjectPath().ToString() );
		return;
	}

	// Объект управления: 
	EnvironmentStreaming = Streaming;

	// Когда подуровень появится в мире, тогда Unreal вызовет HandleEnvironmentShown() у данной сессии:
	Streaming->OnLevelShown.AddDynamic(this, &UCombatSession::HandleEnvironmentShown);

	// Запрос принят:
	UE_LOG( LogCombatSession, Display, TEXT("Environment requested: %s"), *EnvironmentMap.ToSoftObjectPath().ToString() );
}

void UCombatSession::HandleEnvironmentShown()
{
    // Продолжаем, только если подуровень видим и сессия ещё не выбрала арену
    if ( !IsValid(EnvironmentStreaming.Get())    ||
         !EnvironmentStreaming->IsLevelVisible() ||
          ActiveArena.IsValid() )
    {
        return;
    }

    UE_LOG( LogCombatSession, Display, TEXT("Environment shown: %s"), *EnvironmentStreaming->GetWorldAssetPackageName() );

    ULevel*       Level      = EnvironmentStreaming->GetLoadedLevel();
    ACombatArena* FoundArena = nullptr;
    int32         ArenaCount = 0;

    // Акторы из загруженного подуровня:
    for (AActor* Actor : Level->Actors) {
        // Если актор - ACombatArena или её наследник, тогда Cast вернёт указатель:
        // Считаем только действительные арены:
		if (ACombatArena* Arena = Cast <ACombatArena>(Actor); IsValid(Arena) ) {
            FoundArena = Arena;
            ++ArenaCount;
        }
    }

    // Уровень должен содержать ровно одну арену:
    if (ArenaCount != 1) {
        UE_LOG( LogCombatSession, Error, TEXT("Expected one combat arena in the environment, found %d."), ArenaCount );
        return;
    }

    // Запоминаем найденную арену как активную для этой сессии:
    ActiveArena = FoundArena;
    UE_LOG( LogCombatSession, Display, TEXT("Arena registered: %s"), *FoundArena->GetName() );

    // Первый контроллер игрока в этом мире - приводим к нашему типу:
    // Если мира нет, получаем nullptr:
    UWorld* World = Level->GetWorld();
    ACombatPlayerController* Controller = World ? World->GetFirstPlayerController <ACombatPlayerController> () : nullptr;

    if ( !IsValid(Controller) ) {
        UE_LOG( LogCombatSession, Error, TEXT("Combat player controller unavailable.") );
        return;
    }

    // Арена найдена, контроллер готов. Используем камеру арены:
    Controller->UseArenaCamera(FoundArena);
}

void UCombatSession::Initialize() {
	UE_LOG( LogCombatSession, Log, TEXT("Initialize: %s; Outer: %s"), *GetName(), *GetOuter()->GetName() );
}

void UCombatSession::Shutdown() {
	ActiveArena.Reset();

	// Если объект управления подуровнем ещё действителен:
	if ( IsValid(EnvironmentStreaming.Get()) ) {
		// Отписываем сессию от уведомления о появлении подуровня:
		EnvironmentStreaming->OnLevelShown.RemoveDynamic(this, &UCombatSession::HandleEnvironmentShown);
		// Выгружаем подуровень и убираем его из списка стриминга:
		EnvironmentStreaming->SetIsRequestingUnloadAndRemoval(true);
	}

	EnvironmentStreaming = nullptr;

	UE_LOG( LogCombatSession, Log, TEXT("Shutdown: %s"), *GetName() );
}