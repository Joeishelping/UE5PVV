#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PVVTypes.h"
#include "PVVGameMode.generated.h"

class APVVCharacter;
class APVVGameState;

/**
 * Prototype rules ("Open Water Brawl"): free-for-all capture loop with scoring.
 * Vikings win instantly if every Pirate is detained; otherwise highest score when the clock runs out.
 * Dedicated modes (Waterway Pursuit, Learning Center Defense, Prisoner Escape, Minnesota Takeover)
 * will subclass this and override CheckWinConditions / scoring.
 */
UCLASS()
class PIRATESVSVIKINGS_API APVVGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APVVGameMode();

	virtual void BeginPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual bool ShouldSpawnAtStartSpot_Implementation(AController* Player) override { return false; }
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

	void RequestFaction(APlayerController* PC, EPVVFaction Faction);
	EPVVFaction PickBalancedFaction() const;
	AActor* ChooseStartForFaction(EPVVFaction Faction) const;
	void RespawnCharacter(APVVCharacter* Character);

	bool AllowsFriendlyDamage() const { return bFriendlyFireDamage; }

	// Gameplay events (server). Used for scoring and the announcement feed.
	virtual void NotifyKnockedDown(APVVCharacter* Victim, AActor* Cause);
	virtual void NotifyCaptured(APVVCharacter* Victim, APVVCharacter* Captor);
	virtual void NotifyFreed(APVVCharacter* Victim, APVVCharacter* Rescuer);
	virtual void NotifyDetained(APVVCharacter* Victim, APVVCharacter* Captor);
	virtual void NotifyJailbreak(APVVCharacter* Rescuer, int32 NumFreed);

protected:
	/** Blueprint children of APVVCharacter with faction meshes/tuning. Fall back to DefaultPawnClass. */
	UPROPERTY(EditDefaultsOnly, Category = "PVV|Factions")
	TSubclassOf<APawn> PirateCharacterClass;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Factions")
	TSubclassOf<APawn> VikingCharacterClass;

	/** On for quick PIE testing. Turn off once a faction-select widget exists (see PlayerController::ShowFactionSelect). */
	UPROPERTY(EditDefaultsOnly, Category = "PVV|Factions")
	bool bAutoAssignFactions = true;

	/** Refuse a faction switch that would make teams uneven by more than this. 0 = no limit. */
	UPROPERTY(EditDefaultsOnly, Category = "PVV|Factions")
	int32 MaxTeamImbalance = 0;

	/** Teammates always get knocked back; this makes friendly hits drain balance too. */
	UPROPERTY(EditDefaultsOnly, Category = "PVV|Rules")
	bool bFriendlyFireDamage = false;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Rules", meta = (ClampMin = "30"))
	int32 MatchDurationSeconds = 600;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Rules")
	bool bVikingsWinWhenAllPiratesDetained = true;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Rules")
	float RestartDelaySeconds = 15.f;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Scoring")
	int32 PointsPerCapture = 1;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Scoring")
	int32 PointsPerDetention = 3;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Scoring")
	int32 PointsPerRescue = 2;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Scoring")
	int32 PointsPerJailbreakPrisoner = 2;

	APVVGameState* GetPVVGameState() const;
	void Announce(const FText& Message) const;

	virtual void CheckWinConditions();
	void EndMatch(EPVVFaction Winner);

private:
	void TickMatchClock();
	void RestartMatch();

	FTimerHandle MatchClockTimer;
	FTimerHandle RestartTimer;
};
