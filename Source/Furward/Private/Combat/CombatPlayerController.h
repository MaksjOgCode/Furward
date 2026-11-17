// Copyright © 2026 Maksj.OG

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CombatPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class ACombatPlayerController : public APlayerController
{
	GENERATED_BODY()
protected:
	void BeginPlay() override;
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
