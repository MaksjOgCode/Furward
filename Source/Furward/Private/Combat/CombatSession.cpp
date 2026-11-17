// Copyright © 2026 Maksj.OG


#include "Combat/CombatSession.h"



DEFINE_LOG_CATEGORY_STATIC( LogCombatSession, Log, All );



void UCombatSession::Initialize() {
	UE_LOG( LogCombatSession, Log, TEXT("Initialize: %s; Outer: %s"), *GetName(), *GetOuter()->GetName() );
}

void UCombatSession::Shutdown() {
	UE_LOG( LogCombatSession, Log, TEXT("Shutdown: %s"), *GetName() );
}