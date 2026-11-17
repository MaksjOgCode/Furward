// Copyright © 2026 Maksj.OG

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UObject/ObjectPtr.h"
#include "CombatGameMode.generated.h"



class UCombatSession;

/**
 * 
 */
UCLASS()
class ACombatGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	UPROPERTY(Transient)
	TObjectPtr <UCombatSession> CombatSession = nullptr;
};
