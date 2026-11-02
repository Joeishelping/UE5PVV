#pragma once

#include "CoreMinimal.h"
#include "PVVGameState.h"
#include "BRTypes.h"
#include "BRGameState.generated.h"

UCLASS()
class PIRATESVSVIKINGS_API ABRGameState : public APVVGameState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR")
	EBRPhase Phase = EBRPhase::Prep;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR")
	int32 PhaseSecondsLeft = 0;

	/** Warden public approval, 0..100. Bad shootings drop it; low approval takes away drones and guns. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR")
	float Approval = 100.f;

	/** Hours, 0..24. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR")
	float TimeOfDay = 7.5f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR")
	EBRWeather Weather = EBRWeather::Clear;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR")
	int32 RunnersSafe = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR")
	int32 RunnersNeeded = 1;

	/** Points race: the team with more points when the Invasion timer runs out wins. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR")
	int32 RunnerScore = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR")
	int32 WardenScore = 0;

	/** Kill-feed line in the top-right corner of everyone's screen ("Jorge got YEETED by Dave"). */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Feed(const FString& Line);

	// Local (HUD) copy of the kill feed.
	TArray<FString> FeedLines;
	TArray<float> FeedTimes;

	static constexpr float DroneApproval = 60.f;
	static constexpr float LethalApproval = 30.f;

	bool IsNight() const { return TimeOfDay < 6.f || TimeOfDay >= 19.5f; }
	bool DronesAllowed() const { return Approval >= DroneApproval; }
	bool LethalAllowed() const { return Approval >= LethalApproval; }

	/** Loud noise (wire cutters, gunshots): Wardens see a ping on their HUD for a few seconds. */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_NoisePing(FVector_NetQuantize Location);

	// Local (HUD) copies of recent pings.
	TArray<FVector> NoisePings;
	TArray<float> NoisePingTimes;
};
