#pragma once

#include "CoreMinimal.h"
#include "PVVGameMode.h"
#include "BRTypes.h"
#include "BRGameMode.generated.h"

class ABRDesertArena;
class ABRGameState;
class APVVCharacter;

/**
 * BORDER RUN
 *  Prep: Runners work, shop and plan in town; the Line is closed.
 *  Invasion: Runners try to reach the Safe Zone past razor wire, the river and the Wall.
 *  Wardens catch (not kill!) Runners: the holding pen sentence ends in a catapult ride home.
 *  Shooting a Runner who hasn't crossed the Line or attacked = SCANDAL (Approval drops; drones, then guns, get taken away).
 * Runners win if enough of them reach the Safe Zone before the Invasion timer runs out.
 */
UCLASS()
class PIRATESVSVIKINGS_API ABRGameMode : public APVVGameMode
{
	GENERATED_BODY()

public:
	ABRGameMode();

	virtual EPVVFaction PickBalancedFaction() const override;
	virtual void NotifyDetained(APVVCharacter* Victim, APVVCharacter* Captor) override;

	void AdvancePhase();
	void HandleLethalShot(APVVCharacter* Victim, APVVCharacter* Shooter);
	void HandleDrowned(APVVCharacter* Victim);
	/** A Warden ordered everyone out of a vehicle. Finding no Runners = harassing citizens. */
	void OnVehicleSearched(APVVCharacter* Warden, int32 RunnersFound);
	void ChangeApproval(float Delta);

protected:
	virtual void StartMatchFlow() override;
	virtual void CheckWinConditions() override;

	UPROPERTY(EditDefaultsOnly, Category = "BR|Timers") int32 PrepSeconds = 600;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Timers") int32 InvasionSeconds = 1200;
	/** Holding-pen time before the catapult. Repeat offenders get RepeatOffenderSeconds more each time. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Timers") int32 SentenceSeconds = 300;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Timers") int32 RepeatOffenderSeconds = 60;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Timers") float RunnerRespawnSeconds = 12.f;

	/** About a third of the players are Wardens. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Teams") float WardenRatio = 0.34f;

	/** Real minutes per in-game day. 24 = one game hour per real minute. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|World") float DayLengthMinutes = 24.f;
	/** Game starts in the afternoon so night falls around the start of the Invasion. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|World") float StartHour = 14.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|World") FVector2D WeatherChangeSeconds = FVector2D(240.f, 480.f);
	UPROPERTY(EditDefaultsOnly, Category = "BR|World") FVector ArenaLocation = FVector(0.f, 0.f, 10000.f);

	UPROPERTY(EditDefaultsOnly, Category = "BR|Approval") float ScandalPenalty = 35.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Approval") float WrongfulArrestPenalty = 10.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Approval") float GoodCatchBonus = 3.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Approval") float HarassmentPenalty = 5.f;

	/** NPC traffic through the checkpoint during the Invasion. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Traffic") FVector2D NPCTruckEverySeconds = FVector2D(40.f, 75.f);
	UPROPERTY(EditDefaultsOnly, Category = "BR|Traffic") int32 MaxNPCTrucks = 3;
	/** Class used for NPC passengers - set it to your Runner Blueprint so they look like players. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Traffic") TSubclassOf<APawn> NPCPassengerClass;

private:
	UPROPERTY() TObjectPtr<ABRDesertArena> Arena;

	FTimerHandle TickTimer;
	float NextWeatherChange = 0.f;
	float NextNPCTruck = 30.f;
	bool bCarKeyGiven = false;
	bool bAnnouncedDroneLoss = false;
	bool bAnnouncedGunLoss = false;

	ABRGameState* GetBRGameState() const;
	void TickBR();
	void TickRunners(ABRGameState* GS);
	void GiveCarKey();
	void LaunchFromPen(APVVCharacter* Prisoner);
	void RespawnRunner(TWeakObjectPtr<APVVCharacter> Runner);
	void SpawnNPCTruck();
	void SendHomeLater(APVVCharacter* Character, float Seconds);
	FString NameOf(const APVVCharacter* Character) const;
};
