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

	/** "Day 3": how many rounds this host has played (+1). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR")
	int32 DayNumber = 1;
	/** The best moments of the round (shown on the end screen). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR")
	TArray<FString> TopMoments;
	TArray<float> TopMomentScores;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR")
	int32 RunnersSafe = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR")
	int32 RunnersNeeded = 1;

	/** Points race: the team with more points when the Invasion timer runs out wins. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR")
	int32 RunnerScore = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR")
	int32 WardenScore = 0;

	/** Border Patrol's government budget (shared by the team). Supply counters spend it; cleared trucks and catches raise it. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR")
	int32 WardenBudget = 0;

	/** Extra traps the team bought on top of each Warden's Max Team Traps. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR")
	int32 BonusTraps = 0;

	/** The top Runner with a price on their head (catch them for bonus points). Null = no bounty. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR")
	TObjectPtr<APlayerState> BountyTarget;

	/** Lobby: the round waits until every player is ready (Y). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR")
	bool bLobby = false;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR")
	int32 LobbyCountdown = 0;

	/** Best of 3: rounds won so far in this series (0 when not playing a series). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR")
	bool bBestOf3 = false;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR")
	int32 SeriesRunnerWins = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR")
	int32 SeriesWardenWins = 0;

	/** Server: play a game sound for everyone (at Location, or everywhere when b2D). */
	static void Sfx(const UObject* Context, EBRSfx Sound, const FVector& Location, bool b2D = false);
	/** Server: show a puff / splash / smoke / "+$150" popup for everyone. */
	static void FX(const UObject* Context, EBRFX Kind, const FVector& Location, const FString& Text = FString());
	/** This screen only: your Niagara effect from Project Settings, or the simple built-in one. */
	static void SpawnLocalFX(UObject* Context, EBRFX Kind, const FVector& Location, const FString& Text = FString(), float Scale = 1.f);

	UFUNCTION(NetMulticast, Unreliable) void Multicast_Sfx(EBRSfx Sound, FVector_NetQuantize Location, bool b2D);
	UFUNCTION(NetMulticast, Unreliable) void Multicast_FX(EBRFX Kind, FVector_NetQuantize Location, const FString& Text);

	/** Server: something worth replaying happened (the biggest one becomes the Play of the Round). */
	static void Highlight(const UObject* Context, class ABRCharacter* Star, float Score, const FString& Caption);
	UFUNCTION(NetMulticast, Reliable) void Multicast_Highlight(class ABRCharacter* Star, float Score, const FString& Caption);

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
