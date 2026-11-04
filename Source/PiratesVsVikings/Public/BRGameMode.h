#pragma once

#include "CoreMinimal.h"
#include "PVVGameMode.h"
#include "BRTypes.h"
#include "BRGameMode.generated.h"

class ABRDesertArena;
class ABRDog;
class ABRTruck;
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

	// --- Points race ----------------------------------------------------------------------
	void AwardPoints(APVVCharacter* Who, int32 Amount);
	virtual void NotifyKnockedDown(APVVCharacter* Victim, AActor* Cause) override;
	/** Kill-feed line on everyone's screen. */
	void Feed(const FString& Line);
	/** A player died: back to their side's start after this many seconds. */
	void RespawnAfterDeath(APVVCharacter* Character, float Seconds) { SendHomeLater(Character, Seconds); }
	/** Server: someone started trouble (punched a local, kicked off a bar fight). Too much = a bounty on them. */
	void AddTrouble(class ABRCharacter* Who, int32 Amount);
	/** Server: cash / an item on the ground (ReservedFor = only that player can pick it up). */
	void DropCash(const FVector& At, int32 Amount, APlayerState* ReservedFor = nullptr);
	void DropItem(const FVector& At, EBRItem Item, APlayerState* ReservedFor = nullptr);
	/** Trouble points before a Runner gets a bounty for being a menace. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Town") int32 TroubleForBounty = 6;
	/** Chance an NPC carries a fat wad of cash (knock them out to get it). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Town") float LootWadChance = 0.05f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Town") int32 LootWadAmount = 200;
	/** Roughly how many seconds between locals mugging someone in town. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Town") float MuggingEverySeconds = 150.f;
	/** Server: a Runner takes on a smuggling client (an NPC who follows them; paid on arrival in the Safe Zone). */
	void HireClient(class ABRCharacter* Runner, const FVector& At, int32 Pay);
	/** Server: Border Patrol seized contraband. */
	void OnContrabandSeized(APVVCharacter* Warden, int32 Count);
	/** Server: every passenger of a truck at the checkpoint was scanned and clean. */
	void OnTruckCleared(APVVCharacter* Warden);
	/** Server: a truck waited too long at the checkpoint and pushed through. */
	void OnTruckForcedThrough();
	/** Server: illegal NPCs / contraband got across the border without being caught - points for the Runners team. */
	void OnSmuggledThrough(int32 IllegalPeople, int32 Contraband, const FVector& Where);
	/** Points to a whole team (not a specific player). */
	void AwardTeamPoints(EPVVFaction Team, int32 Amount);
	/** BORDER ALARM: a siren everyone hears, a banner, and the spot pings on Border Patrol's map. Rate-limited. */
	void TriggerBorderAlarm(const FVector& Where, const FString& Why);
	/** Two NPC officers turn up near Where and chase Runners who crossed the Line (for ~2.5 minutes). */
	void CallReinforcements(const FVector& Where, class ABRCharacter* CalledBy);
	float LastBorderAlarm = -100.f;

	// --- Lobby / host settings / best of 3 ------------------------------------------------
	/** Multiplayer: wait for everyone to press Y (ready) before Prep starts. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Lobby") bool bUseLobby = true;
	/** Play a best-of-3 series (first team to win 2 rounds). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Lobby") bool bBestOf3 = false;
	/** The host changed a setting in the M menu. */
	void ApplyHostSettings(const FBRHostSettings& Settings);
	void GetHostSettings(FBRHostSettings& Out) const;

	/** Server: spawn a luchador for a ring fight (your Luchador Class, or the NPC class). */
	class ABRCharacter* SpawnLuchador(const FTransform& Where, UClass* LuchadorClass);
	/** Server: spawn one ordinary NPC (random look). */
	class ABRCharacter* SpawnNPC(const FTransform& Where);

	// --- Bots ------------------------------------------------------------------------------
	/** Server: add a computer player to this team (None = whichever team is smaller). */
	void AddBot(EPVVFaction Team);
	void RemoveBots();
	/** Keep each team topped up with bots (they leave again as people join). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Bots") bool bFillWithBots = false;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Bots") int32 FillRunnersTo = 4;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Bots") int32 FillWardensTo = 2;

	// --- Government budget + bounty ---------------------------------------------------------
	/** Server: take money from Border Patrol's budget. False (and nothing taken) if there isn't enough. */
	bool SpendBudget(int32 Amount);
	UClass* GetDogClass() const;
	void AddBudget(int32 Amount);
	/** Server: a Warden caught a Runner who had crossed (pen or catapult): budget + bounty. */
	void OnGoodCatch(APVVCharacter* Warden, class ABRPlayerState* Caught);

	/** Border Patrol's government budget at the start of each round. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Budget") int32 StartingBudget = 3000;
	/** Budget raise for each legal truck cleared at the checkpoint. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Budget") int32 BudgetPerClearedTruck = 400;
	/** Budget raise for each good catch. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Budget") int32 BudgetPerCatch = 150;
	/** The top Runner gets a bounty once they have at least this many points. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Bounty") int32 BountyMinPoints = 6;
	/** Extra points for the Warden who catches the bounty target. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Bounty") int32 BountyPoints = 3;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Bounty") int32 BountyBudget = 1000;
	/** How often (seconds) the bounty moves to whoever is on top. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Bounty") float BountyCheckSeconds = 20.f;

	/** Server: fling a Runner back across the border with this catapult (Warden = who loaded them, or null for the pen). */
	void CatapultRunner(APVVCharacter* Prisoner, class ABRCatapult* Catapult, APVVCharacter* Warden);

protected:
	virtual void StartMatchFlow() override;
	virtual void CheckWinConditions() override;

	// --- Your Blueprints: make Blueprint children of these classes, then pick them here. -----
	UPROPERTY(EditDefaultsOnly, Category = "BR|Your Blueprints") TSubclassOf<ABRDesertArena> ArenaClass;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Your Blueprints") TSubclassOf<ABRTruck> RunnerTruckClass;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Your Blueprints") TSubclassOf<ABRTruck> WardenTruckClass;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Your Blueprints") TSubclassOf<ABRTruck> NPCTruckClass;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Your Blueprints") TSubclassOf<ABRDog> DogClass;

	UPROPERTY(EditDefaultsOnly, Category = "BR|Timers") int32 PrepSeconds = 600;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Timers") int32 InvasionSeconds = 1200;
	/** Holding-pen time before the catapult. Repeat offenders get RepeatOffenderSeconds more each time. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Timers") int32 SentenceSeconds = 90;
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
	/** Off = never build the built-in test desert; always play on the level you made. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|World") bool bUseBuiltInTestMap = true;

	UPROPERTY(EditDefaultsOnly, Category = "BR|Approval") float ScandalPenalty = 35.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Approval") float WrongfulArrestPenalty = 10.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Approval") float GoodCatchBonus = 3.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Approval") float HarassmentPenalty = 5.f;

	/** NPC traffic through the checkpoint during the Invasion. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Traffic") FVector2D NPCTruckEverySeconds = FVector2D(12.f, 25.f);
	/** Most NPC trucks on the road at once. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Traffic") int32 MaxNPCTrucks = 8;
	/** NPCs per truck (driver included). Up to 4 seats - empty seats are where Runners hide. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Traffic") FIntPoint NPCsPerTruck = FIntPoint(1, 3);
	/**
	 * Class for NPC passengers AND village NPCs. Leave it on BRCharacter and they automatically
	 * use your Runner Blueprint (PVV|Factions -> Pirate Character Class), so they have your Runner's mesh, outfits and animations.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Traffic") TSubclassOf<APawn> NPCPassengerClass;

	// --- Points race: points per thing (team with more points at the end wins) -----------
	UPROPERTY(EditDefaultsOnly, Category = "BR|Points") int32 PointsPerCrossing = 3;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Points") int32 PointsPerClient = 2;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Points") int32 PointsPerContraband = 2;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Points") int32 PointsPerCatch = 2;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Points") int32 PointsPerIllegalNPC = 1;
	/** Of the NPCs carrying contraband, how many carry a GUN (they'll shoot at Border Patrol when busted). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Village") float NPCGunChance = 0.2f;
	/** Runners team gets this for every illegal NPC who makes it across uncaught (on foot or in a truck). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Points") int32 PointsPerIllegalNPCThrough = 1;
	/** ...and this per NPC carrying contraband who gets it across. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Points") int32 PointsPerNPCContrabandThrough = 1;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Points") int32 PointsPerSeizure = 1;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Points") int32 PointsPerClearedTruck = 1;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Points") int32 PointsPerWrongfulArrest = -1;
	/** Money a Runner gets for reaching the Safe Zone (they're then sent back to Mexico to go again). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Points") int32 CrossingPay = 150;
	/** Money per contraband item (drugs, guns) carried into the Safe Zone. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Points") int32 ContrabandPay = 500;
	/** Most smuggling clients one Runner can lead at once. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Points") int32 MaxClientsPerRunner = 3;
	/** Chance an NPC in a truck has no papers / carries contraband (0..1). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Traffic") float IllegalNPCChance = 0.15f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Traffic") float ContrabandNPCChance = 0.1f;
	/** Unemployed share of Runners (they have to rummage through trash). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Points") float UnemployedChance = 0.25f;
	/** Chance (0..1) an NPC wears a poncho, so disguised Runners blend in. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Traffic") float NPCPonchoChance = 0.25f;

	/** Townsfolk strolling around the Runner village. 0 = none. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Village") int32 VillagerCount = 12;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Village") float VillagerWanderRadius = 1100.f;
	/** Skin tone range for NPCs (villagers + truck passengers): 0 = lightest, 1 = darkest. Same scale as the menu's Skin slider. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Village", meta = (ClampMin = "0", ClampMax = "1")) float NPCSkinToneMin = 0.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Village", meta = (ClampMin = "0", ClampMax = "1")) float NPCSkinToneMax = 1.f;

private:
	float BountyClock = 0.f;
	float BotClock = 0.f;
	int32 BotNameIndex = 0;
	void TickBots();
	void TickLobby(class ABRGameState* GS);
	void TickBounty(class ABRGameState* GS);
	void TickTownLife(class ABRGameState* GS);
	TMap<TWeakObjectPtr<class ABRCharacter>, float> WardenInTown;
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
	void SpawnVillagers();
	void AssignJob(class ABRPlayerState* PS);
	void TickClients();
	bool bWarnedNoRoad = false;
	UClass* GetNPCClass() const;
	void SendHomeLater(APVVCharacter* Character, float Seconds);
	FString NameOf(const APVVCharacter* Character) const;
};
