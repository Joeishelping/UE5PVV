#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "PVVTypes.h"
#include "PVVGameState.generated.h"

UCLASS()
class PIRATESVSVIKINGS_API APVVGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PVV|Match")
	int32 PirateScore = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PVV|Match")
	int32 VikingScore = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PVV|Match")
	int32 RemainingSeconds = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PVV|Match")
	EPVVFaction Winner = EPVVFaction::None;

	UPROPERTY(ReplicatedUsing = OnRep_MatchPhase, BlueprintReadOnly, Category = "PVV|Match")
	EPVVMatchPhase MatchPhase = EPVVMatchPhase::InProgress;

	/** Server only. */
	void AddScore(EPVVFaction Faction, int32 Points);

	/** Server only. */
	void SetMatchPhase(EPVVMatchPhase NewPhase, EPVVFaction InWinner);

	UFUNCTION(BlueprintPure, Category = "PVV|Match")
	int32 CountPlayers(EPVVFaction Faction) const;

	/** Kill-feed style message shown to everyone ("Viking Bjorn hauled Pirate Abdi off to processing!"). */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Announce(const FText& Message);

protected:
	UFUNCTION()
	void OnRep_MatchPhase();

	UFUNCTION(BlueprintImplementableEvent, Category = "PVV|Match")
	void OnAnnouncement(const FText& Message);

	UFUNCTION(BlueprintImplementableEvent, Category = "PVV|Match")
	void OnMatchEnded(EPVVFaction MatchWinner);
};
