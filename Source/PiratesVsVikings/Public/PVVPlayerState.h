#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "PVVTypes.h"
#include "PVVPlayerState.generated.h"

UCLASS()
class PIRATESVSVIKINGS_API APVVPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "PVV")
	EPVVFaction GetFaction() const { return Faction; }

	/** Server only. */
	void SetFaction(EPVVFaction NewFaction);

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PVV|Stats")
	int32 Captures = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PVV|Stats")
	int32 Detentions = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PVV|Stats")
	int32 Rescues = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PVV|Stats")
	int32 TimesDetained = 0;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Faction, BlueprintReadOnly, Category = "PVV")
	EPVVFaction Faction = EPVVFaction::None;

	UFUNCTION()
	void OnRep_Faction();

	UFUNCTION(BlueprintImplementableEvent, Category = "PVV")
	void OnFactionChanged(EPVVFaction NewFaction);
};
