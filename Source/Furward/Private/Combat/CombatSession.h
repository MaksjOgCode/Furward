// Copyright © 2026 Maksj.OG

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CombatSession.generated.h"

/**
 *		
 */
UCLASS()
class UCombatSession : public UObject
{
	GENERATED_BODY()
public:
	void Initialize();
	void Shutdown();
};
