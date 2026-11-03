#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PVVInteractable.h"
#include "BRTypes.h"
#include "BRWorldActors.generated.h"

class ABRCockfightPit;
class ABRCharacter;
class ABRLuchaRing;
class ABRPlayerState;
class APVVCharacter;
class UBoxComponent;
class UPointLightComponent;
class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UStaticMesh;
class USoundBase;
class UAudioComponent;

/** Shared helpers for the greybox world pieces. */
UCLASS(Abstract)
class PIRATESVSVIKINGS_API ABRGreyboxActor : public AActor
{
	GENERATED_BODY()

public:
	ABRGreyboxActor();
	virtual void BeginPlay() override;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	/** Colour per mesh component name, applied at BeginPlay on every machine. */
	UPROPERTY() TMap<FName, FLinearColor> ComponentColors;
	UPROPERTY() TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> ConeMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> CylinderMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> SphereMesh;

	UStaticMeshComponent* AddMesh(const TCHAR* Name, UStaticMesh* Mesh, const FVector& Location, const FVector& SizeCm, const FLinearColor& Color,
		const FRotator& Rotation = FRotator::ZeroRotator, bool bCollide = true);
	UTextRenderComponent* AddLabel(const TCHAR* Name, const FVector& Location, float Size, const FColor& Color);
	/** Invisible ball that lets players find this with E (things with no collision are invisible to the E key). */
	USphereComponent* AddPickupZone(float Radius);
	/** Dropped things vanish after this many seconds (blinking for the last 5). Things you placed in the level stay. */
	void StartDespawn(float Seconds);
	FTimerHandle BlinkTimer;
	bool bBlinkVisible = true;
	static void Paint(UStaticMeshComponent* Mesh, const FLinearColor& Color);
};

// ===========================================================================================
/** A counter or work spot in town: buy, sell, buy a boat, or work a job for cash. Runners only. */
UCLASS()
class PIRATESVSVIKINGS_API ABRStation : public ABRGreyboxActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRStation();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

	// Set before FinishSpawning (replicated so clients can show prompts).
	// --- Place one in your level and set these in Details -------------------------------
	/** Buy an item, sell an item, work a job for cash, buy a boat, or bet on the cockfight. */
	UPROPERTY(Replicated, EditAnywhere, Category = "BR|Station") EBRStationKind Kind = EBRStationKind::BuyItem;
	UPROPERTY(Replicated, EditAnywhere, Category = "BR|Station") EBRItem Item = EBRItem::None;
	/** Warden Supply counters: what Border Patrol buys with the government budget (None = sell the Item above). */
	UPROPERTY(Replicated, EditAnywhere, Category = "BR|Station") EBRWardenGear WardenGear = EBRWardenGear::None;
	/** Cost (buy/boat/bet), or pay (job/sell). Bets: 0 = ALL IN. */
	UPROPERTY(Replicated, EditAnywhere, Category = "BR|Station") int32 Price = 10;
	/** Jobs: seconds of holding E. */
	UPROPERTY(Replicated, EditAnywhere, Category = "BR|Station") float WorkSeconds = 0.f;
	/** Seconds before it can be used again. */
	UPROPERTY(Replicated, EditAnywhere, Category = "BR|Station") float CooldownSeconds = 0.f;
	UPROPERTY(Replicated, EditAnywhere, Category = "BR|Station") bool bNightOnly = false;
	/** Sign text. Leave empty to make one automatically (e.g. "LADDER $60"). */
	UPROPERTY(Replicated, EditAnywhere, Category = "BR|Station") FString Title;
	UPROPERTY(Replicated, EditAnywhere, Category = "BR|Station") FLinearColor Color = FLinearColor(0.6f, 0.6f, 0.6f);
	/** Bet stations: which rooster (0 = RED, 1 = BLUE). Leave Pit empty to use the nearest pit. */
	UPROPERTY(Replicated, EditAnywhere, Category = "BR|Station") int32 BetSide = 0;
	UPROPERTY(Replicated, EditAnywhere, Category = "BR|Station") TObjectPtr<ABRCockfightPit> Pit;
	/** Lucha stations: which ring (empty = the nearest one). */
	UPROPERTY(Replicated, EditAnywhere, Category = "BR|Station") TObjectPtr<ABRLuchaRing> Ring;
	/** Boat stations: where the boat appears (drag the diamond in the viewport; should be on the water). */
	UPROPERTY(EditAnywhere, Category = "BR|Station", meta = (MakeEditWidget = "true")) FVector BoatSpawnOffset = FVector(600.f, 0.f, 0.f);
	/** Untick to hide the grey counter box (e.g. it sits inside your own shop counter model). It still works when you walk up to it. */
	UPROPERTY(EditAnywhere, Category = "BR|Station") bool bShowCounter = true;
	/** Untick to hide the floating sign (if your building already has one). */
	UPROPERTY(EditAnywhere, Category = "BR|Station") bool bShowSign = true;
	FVector BoatSpawn = FVector::ZeroVector;
	/** Jobs: anyone can work here (e.g. digging through trash). Off = only Runners assigned to this job. Jobs that pay $0 are always open. */
	UPROPERTY(EditAnywhere, Category = "BR|Station") bool bAnyoneCanWork = false;
	/** The sign text (worked out at BeginPlay). Job assignments use it as the job's name. */
	FString SignText;
	bool IsAssignableJob() const { return Kind == EBRStationKind::Job && Price > 0 && !bAnyoneCanWork; }

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Counter;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Label;

private:
	UPROPERTY(Replicated) float NextUseServerTime = 0.f;
	bool IsOpen() const;
};

// ===========================================================================================
/** 4 m razor-wire fence segment. Touching it hurts. Runners with wire cutters can cut it (loud). */
UCLASS()
class PIRATESVSVIKINGS_API ABRSpikeFence : public ABRGreyboxActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRSpikeFence();
	virtual void BeginPlay() override;

	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override { return 8.f; }
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> HurtZone;

	UFUNCTION()
	void OnHurtOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};

// ===========================================================================================
/** The Wall. Runners holding a ladder can lean one against it. */
UCLASS()
class PIRATESVSVIKINGS_API ABRWall : public ABRGreyboxActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRWall();

	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override { return 2.f; }
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

	static constexpr float Height = 600.f;
	static constexpr float HalfLength = 8100.f;
	/** The road passes through a gate in the wall here (relative Y). */
	static constexpr float GateY = -1200.f;
	static constexpr float GateHalfWidth = 700.f;
};

// ===========================================================================================
/** A ladder leaning over the wall. Runners climb it; Wardens kick it down. */
UCLASS()
class PIRATESVSVIKINGS_API ABRLadder : public ABRGreyboxActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRLadder();

	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> UseZone;
};

// ===========================================================================================
/** The Line. A solid invisible barrier during Prep; open during the Invasion. */
UCLASS()
class PIRATESVSVIKINGS_API ABRLineBarrier : public ABRGreyboxActor
{
	GENERATED_BODY()

public:
	ABRLineBarrier();
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Barrier;

private:
	int32 AppliedState = -1;
};

// ===========================================================================================
/**
 * Launches Runners back over the border. No damage, maximum comedy.
 * Wardens: carry a zip-tied Runner here and hold E. Also fires detainees automatically when their pen time is up.
 */
UCLASS(meta = (DisplayName = "BR Catapult"))
class PIRATESVSVIKINGS_API ABRCatapult : public ABRGreyboxActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRCatapult();
	virtual void Tick(float DeltaSeconds) override;

	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override { return 1.5f; }
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

	/** Random spread around the landing spot (cm) so people don't stack up. */
	UPROPERTY(EditAnywhere, Category = "BR|Catapult") float LandingSpread = 700.f;
	UPROPERTY(EditAnywhere, Category = "BR|Catapult") TObjectPtr<USoundBase> LaunchSound;

	/** Server: fling this detainee at Target. */
	void Launch(APVVCharacter* Passenger, const FVector& Target);

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> ArmPivot;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Basket;

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Fling();

private:
	float FlingTime = -100.f;
};

// ===========================================================================================
/** The checkpoint gate in the Wall where the road (and NPC traffic) crosses. Wardens hold E to open/close. */
UCLASS()
class PIRATESVSVIKINGS_API ABRCheckpointGate : public ABRGreyboxActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRCheckpointGate();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override { return 0.6f; }
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

	bool IsOpen() const { return bOpen || bSmashed; }
	bool IsSmashed() const { return bSmashed; }
	/** Server. */
	void OpenFor(float Seconds);
	/** Server: a vehicle floored it through the barrier - the arm snaps off and flies. Repaired after a while. */
	void Smash(AActor* Smasher);
	/** Seconds before Border Patrol fixes a smashed barrier. */
	UPROPERTY(EditAnywhere, Category = "BR|Gate") float RepairSeconds = 30.f;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Barrier;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> ArmPivot;

private:
	UPROPERTY(Replicated) bool bOpen = false;
	UPROPERTY(ReplicatedUsing = OnRep_Smashed) bool bSmashed = false;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> ArmMesh;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Debris;
	UFUNCTION() void OnRep_Smashed();
	float RepairAt = 0.f;
	float CloseAt = 0.f;
	float ArmAngle = 0.f;
	int32 AppliedState = -1;
};

// ===========================================================================================
/** Warden trap. Runners only see it up close. Bear = rooted, Glue = slowed, Alarm = tagged. Runners can disarm (hold E). */
UCLASS()
class PIRATESVSVIKINGS_API ABRTrap : public ABRGreyboxActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRTrap();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override { return 3.f; }
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

	UPROPERTY(Replicated) EBRTrap Type = EBRTrap::Bear;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Trigger;

	UFUNCTION()
	void OnTriggered(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	bool bHiddenLocally = false;
};

namespace BRPit
{
	enum EState : uint8 { Betting = 0, Fighting = 1, Result = 2 };
}

struct FBRPitBet
{
	TWeakObjectPtr<ABRPlayerState> Player;
	int32 Side = 0;
	int32 Amount = 0;
	bool bAllIn = false;
};

// ===========================================================================================
/** Cockfight pit in town. 40 s of betting, a 12 s fight, then payout. Bet stations around it take the money. */
UCLASS()
class PIRATESVSVIKINGS_API ABRCockfightPit : public ABRGreyboxActor
{
	GENERATED_BODY()

public:
	ABRCockfightPit();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	bool IsTakingBets() const { return State == BRPit::Betting; }
	bool HasBet(const ABRPlayerState* Player) const;
	/** Server. Amount 0 = ALL IN. Returns the amount actually bet. */
	int32 PlaceBet(ABRPlayerState* Player, int32 Side, int32 Amount);

	UPROPERTY(EditAnywhere, Category = "BR|Cockfight") float BettingSeconds = 40.f;
	UPROPERTY(EditAnywhere, Category = "BR|Cockfight") float FightSeconds = 12.f;
	UPROPERTY(EditAnywhere, Category = "BR|Cockfight") float ResultSeconds = 6.f;
	/** Puts 4 betting counters around the pit automatically (RED $20 / ALL IN, BLUE $20 / ALL IN). */
	UPROPERTY(EditAnywhere, Category = "BR|Cockfight") bool bSpawnBetCounters = true;

	/** Your own rooster model (e.g. from Fab). Replaces the greybox birds; the coloured collar + RED/BLUE sign stay. */
	UPROPERTY(EditAnywhere, Category = "BR|Cockfight") TObjectPtr<UStaticMesh> RoosterModel;
	/** Size / turn / height of your model so it stands on the floor facing forward (+X). */
	UPROPERTY(EditAnywhere, Category = "BR|Cockfight") FVector RoosterModelScale = FVector(1.f);
	UPROPERTY(EditAnywhere, Category = "BR|Cockfight") FRotator RoosterModelRotation = FRotator::ZeroRotator;
	UPROPERTY(EditAnywhere, Category = "BR|Cockfight") FVector RoosterModelOffset = FVector::ZeroVector;
	/** Where the team collar sits on the bird (move it to your model's neck) and how big it is (cm). */
	UPROPERTY(EditAnywhere, Category = "BR|Cockfight") FVector CollarLocation = FVector(28.f, 0.f, 68.f);
	UPROPERTY(EditAnywhere, Category = "BR|Cockfight") FVector CollarSize = FVector(34.f, 34.f, 9.f);
	UPROPERTY(EditAnywhere, Category = "BR|Cockfight") FLinearColor RedTeamColor = FLinearColor(0.85f, 0.05f, 0.03f);
	UPROPERTY(EditAnywhere, Category = "BR|Cockfight") FLinearColor BlueTeamColor = FLinearColor(0.05f, 0.2f, 0.95f);

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> RoosterRed;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> RoosterBlue;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Board;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> RedTag;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> BlueTag;

private:
	UPROPERTY(Replicated) uint8 State = BRPit::Betting;
	UPROPERTY(Replicated) float StateEndsAt = 0.f;
	UPROPERTY(Replicated) int32 Winner = 0;
	UPROPERTY(Replicated) int32 PotRed = 0;
	UPROPERTY(Replicated) int32 PotBlue = 0;

	TArray<FBRPitBet> Bets;

	void AdvanceState(float ServerNow);
	void PayOut();
	void BuildRooster(USceneComponent* Parent, const TCHAR* Prefix, const FLinearColor& Accent);
	void SetupRoosterLooks();
};

// ===========================================================================================
/**
 * Human cannon. A Runner with a Cannonball Suit (buy it at a Station) climbs in (hold E),
 * aims with the mouse and presses E to FIRE themselves over the border. Very loud.
 * Wardens can plug it (hold E) to put it out of action for a while.
 */
UCLASS(meta = (DisplayName = "BR Human Cannon"))
class PIRATESVSVIKINGS_API ABRCannon : public ABRGreyboxActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRCannon();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

	/** Launch speed (cm/s). 4200 = about 180 m at 45 degrees. */
	UPROPERTY(EditAnywhere, Category = "BR|Cannon") float LaunchSpeed = 4200.f;
	UPROPERTY(EditAnywhere, Category = "BR|Cannon") float MinPitch = 15.f;
	UPROPERTY(EditAnywhere, Category = "BR|Cannon") float MaxPitch = 70.f;
	/** How far left/right it can turn from where you placed it (degrees). */
	UPROPERTY(EditAnywhere, Category = "BR|Cannon") float YawRange = 60.f;
	/** Seconds a Warden's plug keeps it out of action. */
	UPROPERTY(EditAnywhere, Category = "BR|Cannon") float PluggedSeconds = 90.f;
	/** Usable during Prep too (off = only once the Line is open). */
	UPROPERTY(EditAnywhere, Category = "BR|Cannon") bool bUsableDuringPrep = false;
	UPROPERTY(EditAnywhere, Category = "BR|Cannon") TObjectPtr<USoundBase> FireSound;
	/** Seconds the fuse burns before BOOM (everyone sitting on the barrel goes flying). */
	UPROPERTY(EditAnywhere, Category = "BR|Cannon") float FuseSeconds = 3.f;
	/** Fuse sizzle sound (plays when it's lit). */
	UPROPERTY(EditAnywhere, Category = "BR|Cannon") TObjectPtr<USoundBase> FuseSound;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> BarrelPivot;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Muzzle;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Sign;

	UFUNCTION(NetMulticast, Unreliable) void Multicast_Boom();

private:
	UPROPERTY(Replicated) TObjectPtr<APVVCharacter> Loaded;
	UPROPERTY(Replicated) FRotator Aim = FRotator(45.f, 0.f, 0.f);
	UPROPERTY(Replicated) float PluggedUntil = 0.f;
	/** Server time the fuse was lit (0 = not lit). */
	UPROPERTY(Replicated) float FuseLitAt = 0.f;
	float BoomTime = -100.f;
	float NextSpark = 0.f;
	int32 LastFuseCount = -1;

	bool IsPlugged() const;
	bool PhaseAllowsFiring() const;
	/** Everyone sitting / standing on the barrel right now. */
	void FindRiders(TArray<ABRCharacter*>& Out) const;
	bool IsRider(const APVVCharacter* Who) const;
	void FireRiders();
	FVector FusePoint() const;
	void Fire();
	void Unload();
};

// ===========================================================================================
/**
 * A physics object anyone can pick up (Z), swing (left click) and throw (Z again).
 * Anything it hits hard gets knocked over. Put your own mesh on it (crates, chairs, cactus pots, chickens...).
 */
UCLASS(meta = (DisplayName = "BR Physics Prop"))
class PIRATESVSVIKINGS_API ABRPhysicsProp : public AActor
{
	GENERATED_BODY()

public:
	ABRPhysicsProp();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	/** Your mesh (leave empty for a crate). */
	UPROPERTY(EditAnywhere, Category = "BR|Prop") TObjectPtr<UStaticMesh> PropMesh;
	UPROPERTY(EditAnywhere, Category = "BR|Prop") FLinearColor Color = FLinearColor(0.6f, 0.45f, 0.25f);
	/** Speed (cm/s) at which it knocks people over when it hits them. */
	UPROPERTY(EditAnywhere, Category = "BR|Prop") float KnockoutSpeed = 800.f;
	/** Heavier = harder to throw far. */
	UPROPERTY(EditAnywhere, Category = "BR|Prop") float MassKg = 15.f;

	bool IsHeld() const { return Holder != nullptr; }
	/** Server. */
	void PickUp(class ABRCharacter* By);
	void Throw(const FVector& Velocity);
	void Drop();

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;

	UFUNCTION() void OnRep_Holder();
	UFUNCTION() void OnPropHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

private:
	UPROPERTY(ReplicatedUsing = OnRep_Holder) TObjectPtr<class ABRCharacter> Holder;
	TWeakObjectPtr<class ABRCharacter> LastThrower;
	float ThrownTime = -100.f;
	void ApplyHeldState();
};

/** A chicken. It wanders and hops around; grab it (Z), carry it, and THROW it (Z) at people. */
UCLASS(meta = (DisplayName = "BR Chicken"))
class PIRATESVSVIKINGS_API ABRChicken : public ABRPhysicsProp
{
	GENERATED_BODY()

public:
	ABRChicken();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** How far (cm) it wanders from where it started. */
	UPROPERTY(EditAnywhere, Category = "BR|Chicken") float WanderRadius = 600.f;
	/** Drag in a cluck / squawk sound. */
	UPROPERTY(EditAnywhere, Category = "BR|Chicken") TObjectPtr<USoundBase> CluckSound;

protected:
	UFUNCTION(NetMulticast, Unreliable) void Multicast_Cluck();
	FVector Home = FVector::ZeroVector;
	FVector Heading = FVector::ZeroVector;
	float PanicUntil = -1.f;
	FVector PanicFrom = FVector::ZeroVector;
public:
	/** Server: this chicken panics and runs from Where. */
	void Panic(const FVector& Where, float Seconds);
	/** Server: every chicken within Radius goes nuts (a chicken got thrown at them). */
	static void Stampede(UWorld* World, const FVector& Where, float Radius);
protected:
	bool bPecking = false;
	float NextHop = 0.f;
};

/**
 * Border Patrol watchtower with a big spotlight. It sweeps on its own; a Warden standing on top aims it with their view.
 * Runners caught in the beam are SPOTTED (Border Patrol sees them through walls for a few seconds). Disguised Runners are ignored.
 */
UCLASS(meta = (DisplayName = "BR Watchtower"))
class PIRATESVSVIKINGS_API ABRWatchtower : public ABRGreyboxActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRWatchtower();
	virtual void Tick(float DeltaSeconds) override;

	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override { return 0.8f; }
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

	/** Platform height (cm). */
	UPROPERTY(EditAnywhere, Category = "BR|Watchtower") float Height = 650.f;
	/** How far the beam reaches (cm). */
	UPROPERTY(EditAnywhere, Category = "BR|Watchtower") float Range = 6000.f;
	/** Half-width of the beam (degrees). */
	UPROPERTY(EditAnywhere, Category = "BR|Watchtower") float BeamAngle = 9.f;
	/** Unmanned sweep: degrees left/right of the arrow. */
	UPROPERTY(EditAnywhere, Category = "BR|Watchtower") float SweepAngle = 55.f;
	UPROPERTY(EditAnywhere, Category = "BR|Watchtower") float SpotSeconds = 4.f;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<class USpotLightComponent> Light;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Lamp;
	float NextScan = 0.f;
	class ABRCharacter* FindOperator() const;
	FRotator GetBeamRotation() const;
};

// ===========================================================================================
/** An item lying on the ground (dropped with G). Walk up and press E to pick it up. Border Patrol confiscate contraband. */
UCLASS(meta = (DisplayName = "BR Item Pickup"))
class PIRATESVSVIKINGS_API ABRItemPickup : public ABRGreyboxActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRItemPickup();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override { return 0.3f; }
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

	/** Seconds before a DROPPED item disappears (ones you place in the level never do). */
	UPROPERTY(EditAnywhere, Category = "BR|Pickup") float DespawnSeconds = 45.f;
	/** What's lying here (you can also place these in your level as free loot). */
	UPROPERTY(Replicated, EditAnywhere, Category = "BR|Pickup") EBRItem Item = EBRItem::Taco;
	/** Wire cutter cuts left. */
	UPROPERTY(Replicated) int32 Charges = 0;
	/** Only this player can pick it up (e.g. the stuff a thief stole from them). */
	UPROPERTY(Replicated) TObjectPtr<APlayerState> ReservedFor;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Tag;
};

/** A hat that got knocked off someone's head. Anyone can pick it up and wear it (E). */
UCLASS(meta = (DisplayName = "BR Hat Pickup"))
class PIRATESVSVIKINGS_API ABRHatPickup : public ABRGreyboxActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRHatPickup();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override { return 0.3f; }
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

	UPROPERTY(ReplicatedUsing = OnRep_HatInfo, EditAnywhere, Category = "BR|Hat") EBRHat Hat = EBRHat::CowboyHat;
	/** Seconds before a knocked-off hat disappears. */
	UPROPERTY(EditAnywhere, Category = "BR|Hat") float DespawnSeconds = 30.f;
	UPROPERTY(ReplicatedUsing = OnRep_HatInfo, EditAnywhere, Category = "BR|Hat") TObjectPtr<class USkeletalMesh> HatMesh;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<class USkeletalMeshComponent> Visual;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Tag;
	UFUNCTION() void OnRep_HatInfo();
};

/** Cash someone dropped (H). Anyone can choose to pick it up (E). Border Patrol who take it put it in the budget. */
UCLASS(meta = (DisplayName = "BR Cash Pickup"))
class PIRATESVSVIKINGS_API ABRCashPickup : public ABRGreyboxActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRCashPickup();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override { return 0.4f; }
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

	UPROPERTY(Replicated, EditAnywhere, Category = "BR|Cash") int32 Amount = 50;
	/** Seconds before dropped cash disappears. */
	UPROPERTY(EditAnywhere, Category = "BR|Cash") float DespawnSeconds = 60.f;
	UPROPERTY(Replicated) FString DropperName;
	UPROPERTY(Replicated) EPVVFaction DropperTeam = EPVVFaction::None;
	/** Only this player can pick it up (bar-fight winnings). */
	UPROPERTY(Replicated) TObjectPtr<APlayerState> ReservedFor;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Tag;
};

/** A taco platter someone put down: anyone can grab a taco (E) until it's empty. */
UCLASS(meta = (DisplayName = "BR Taco Platter"))
class PIRATESVSVIKINGS_API ABRTacoPlatter : public ABRGreyboxActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRTacoPlatter();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override { return 0.5f; }
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

	UPROPERTY(Replicated, EditAnywhere, Category = "BR|Tacos") int32 Servings = 8;

private:
	int32 ShownServings = -1;
};

// ===========================================================================================
/** Candy from a smashed pinata: grab it (E) for full balance (shake off a beating). */
UCLASS(NotPlaceable)
class PIRATESVSVIKINGS_API ABRCandy : public ABRGreyboxActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRCandy();
	virtual void BeginPlay() override;
	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override { return 0.f; }
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;
};

// ===========================================================================================
/** Pinata: Runners whack it (hold E). After enough whacks it bursts: cash or a random item for whoever broke it. Comes back later. */
UCLASS(meta = (DisplayName = "BR Pinata"))
class PIRATESVSVIKINGS_API ABRPinata : public ABRGreyboxActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRPinata();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override { return 0.4f; }
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

	/** Whacks needed to burst it (from anyone - steal the last hit!). */
	UPROPERTY(EditAnywhere, Category = "BR|Pinata") int32 WhacksToBreak = 10;
	UPROPERTY(EditAnywhere, Category = "BR|Pinata") float RespawnSeconds = 150.f;
	UPROPERTY(EditAnywhere, Category = "BR|Pinata") int32 MinCash = 20;
	UPROPERTY(EditAnywhere, Category = "BR|Pinata") int32 MaxCash = 120;
	UPROPERTY(EditAnywhere, Category = "BR|Pinata") TObjectPtr<USoundBase> WhackSound;
	UPROPERTY(EditAnywhere, Category = "BR|Pinata") TObjectPtr<USoundBase> BurstSound;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Swing;
	UFUNCTION(NetMulticast, Unreliable) void Multicast_Whack(bool bBurst);

private:
	UPROPERTY(Replicated) int32 Whacks = 0;
	UPROPERTY(Replicated) float BackAt = 0.f;
	float WhackTime = -100.f;
	bool IsBroken() const;
};

/**
 * Tunnel. Soft dirt on the near side: a Runner with a Shovel digs it open (hold E, slow but quiet).
 * Then any Runner can crawl through (hold E) and pop out at the far end (drag the diamond to place it).
 * Border Patrol can cave it in (hold E) from either end.
 */
UCLASS(meta = (DisplayName = "BR Tunnel"))
class PIRATESVSVIKINGS_API ABRTunnel : public ABRGreyboxActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRTunnel();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

	/** Where you come out (relative to the entrance). Drag the diamond in the viewport. */
	UPROPERTY(EditAnywhere, Category = "BR|Tunnel", meta = (MakeEditWidget = "true")) FVector ExitOffset = FVector(9000.f, 0.f, 0.f);
	UPROPERTY(EditAnywhere, Category = "BR|Tunnel") float DigSeconds = 25.f;
	UPROPERTY(EditAnywhere, Category = "BR|Tunnel") float CrawlSeconds = 2.f;
	UPROPERTY(EditAnywhere, Category = "BR|Tunnel") float CaveInSeconds = 3.f;
	/** The shovel breaks after digging one tunnel. */
	UPROPERTY(EditAnywhere, Category = "BR|Tunnel") bool bShovelBreaks = true;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> ExitPoint;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Sign;

private:
	UPROPERTY(Replicated) bool bOpen = false;
	int32 AppliedState = -1;
	bool IsNearExit(const APVVCharacter* C) const;
};

// ===========================================================================================
/**
 * Village life for the Runner town: colourful houses, a church, plaza with fountain, market stalls,
 * papel picado flags, lamps that light up at night, cacti, palms, laundry, a wrecked car and wandering chickens.
 * Spawned by the Desert Arena (turn off with Build Village). Purely decoration.
 */
UCLASS()
class PIRATESVSVIKINGS_API ABRVillage : public ABRGreyboxActor
{
	GENERATED_BODY()

public:
	ABRVillage();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Recolour any piece by name (House3_Body, Church_Tower, Stall2_Awning ...). See the names in the Components list. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Village") TMap<FName, FLinearColor> ColorOverrides;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Village") float LampBrightness = 800.f;

protected:
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> Chickens;
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> Lamps;

private:
	TArray<FVector> ChickenTargets;
	FVector ChickenYard = FVector::ZeroVector;
	float NextLampCheck = 0.f;
	bool bLampsOn = false;
	int32 PieceCount = 0;

	FString NextName(const TCHAR* Prefix);
	void House(const TCHAR* Name, float X, float Y, float W, float D, float H, const FLinearColor& Wall, const FLinearColor& Trim);
	void Stall(float X, float Y, const FLinearColor& Awning);
	void Picado(const FVector2D& A, const FVector2D& B, float Z, int32 Count);
	void Cactus(float X, float Y, float H);
	void Palm(float X, float Y);
	void Lamp(float X, float Y);
	void Barrel(float X, float Y, const FLinearColor& Color);
	void Crate(float X, float Y, float Yaw);
	void Bench(float X, float Y, float Yaw);
	void Chicken(float X, float Y);
};

// ===========================================================================================
/** Local-only bullet / taser tracer with a muzzle flash and an impact puff. */
UCLASS()
class PIRATESVSVIKINGS_API ABRTracerFX : public AActor
{
	GENERATED_BODY()

public:
	ABRTracerFX();
	void Init(const FVector& Start, const FVector& End, bool bLethal, bool bHitSomeone);

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Beam;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Impact;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Flash;
};

// ===========================================================================================
/** Local-only puff of dust / water splash / cannon smoke, or a floating "+$150" popup. */
UCLASS()
class PIRATESVSVIKINGS_API ABRFX : public AActor
{
	GENERATED_BODY()

public:
	ABRFX();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	EBRFX Kind = EBRFX::Dust;
	FString Text;

protected:
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Blobs;
	UPROPERTY() TObjectPtr<UTextRenderComponent> Popup;
	TArray<FVector> Drift;
	float Age = 0.f;
	float Life = 1.f;
	float BlobSize = 40.f;
};

// ===========================================================================================
/**
 * Lucha Libre ring. A Runner pays to FIGHT THE LUCHADOR (counters around the ring: $50 / $100 / $250).
 * Then there are 15 seconds of betting: anyone can bet on the challenger or the luchador.
 * The fight: first to get knocked down twice loses (time up = the luchador wins on points). Leaving the ring = forfeit.
 * The winner's side gets double their money back. Beat him and you win his mask (Lucha Mask hat).
 * Bigger fights = tougher luchador.
 */
UCLASS(meta = (DisplayName = "BR Lucha Ring"))
class PIRATESVSVIKINGS_API ABRLuchaRing : public ABRGreyboxActor
{
	GENERATED_BODY()

public:
	ABRLuchaRing();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Your own luchador Blueprint (leave empty to use the NPC class). */
	UPROPERTY(EditAnywhere, Category = "BR|Lucha") TSubclassOf<class ABRCharacter> LuchadorClass;
	/** Ring names picked at random. */
	UPROPERTY(EditAnywhere, Category = "BR|Lucha") TArray<FString> LuchadorNames;
	/** Put the challenge + betting counters around the ring automatically. */
	UPROPERTY(EditAnywhere, Category = "BR|Lucha") bool bSpawnCounters = true;
	UPROPERTY(EditAnywhere, Category = "BR|Lucha") float BettingSeconds = 15.f;
	UPROPERTY(EditAnywhere, Category = "BR|Lucha") float FightSeconds = 150.f;
	UPROPERTY(EditAnywhere, Category = "BR|Lucha") int32 KnockdownsToWin = 2;
	/** How much tougher he gets per $100 of wager (1 = +100% balance). */
	UPROPERTY(EditAnywhere, Category = "BR|Lucha") float ToughnessPer100 = 0.35f;
	/** Size of the square ring (cm, edge to edge). */
	static constexpr float RingSize = 700.f;
	static constexpr float DeckHeight = 60.f;

	bool IsOpenForChallenge() const;
	bool IsTakingBets() const;
	bool HasBet(const ABRPlayerState* Player) const;
	bool IsChallenger(const APVVCharacter* Who) const;
	/** Server: start a fight. */
	bool Challenge(class ABRCharacter* Runner, int32 Wager);
	/** Server: returns the amount taken (0 = refused). Side 0 = challenger, 1 = luchador. */
	int32 PlaceBet(ABRPlayerState* Player, int32 Side, int32 Amount);
	/** Server: a station or the pit's own counters. */
	void SpawnCounters();

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Board;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> BoardBack;
	UPROPERTY(Replicated) uint8 State = 0; // 0 open, 1 betting, 2 fighting, 3 result
	UPROPERTY(Replicated) float StateEndsAt = 0.f;
	UPROPERTY(Replicated) FString ChallengerName;
	UPROPERTY(Replicated) FString FighterName;
	UPROPERTY(Replicated) int32 ChallengerKOs = 0;
	UPROPERTY(Replicated) int32 LuchadorKOs = 0;
	UPROPERTY(Replicated) int32 Wager = 0;
	UPROPERTY(Replicated) int32 PotChallenger = 0;
	UPROPERTY(Replicated) int32 PotLuchador = 0;
	UPROPERTY(Replicated) FString ResultText;

private:
	struct FLuchaBet { TWeakObjectPtr<ABRPlayerState> Player; int32 Side = 0; int32 Amount = 0; };
	TArray<FLuchaBet> Bets;
	TWeakObjectPtr<class ABRCharacter> Challenger;
	TWeakObjectPtr<class ABRCharacter> Luchador;
	bool bChallengerWasDown = false;
	bool bLuchadorWasDown = false;
	/** Invisible walls along the ropes while a fight is on (you bounce off them like WWE). */
	UPROPERTY() TArray<TObjectPtr<UBoxComponent>> RopeWalls;
	int32 RopesLive = -1;
	float NextSwing = 0.f;
	FVector Corner(int32 Which) const;
	void TickFight(float ServerNow);
	void Finish(int32 WinnerSide, const FString& Why);
	void UpdateBoard(float ServerNow);
};

// ===========================================================================================
/**
 * Somewhere to hide: a Runner climbs in (E) and vanishes. Border Patrol can search it (hold E):
 * if someone's inside they get dragged out onto the ground. Spotlights, scanners, dogs and bots can't see inside.
 */
UCLASS(meta = (DisplayName = "BR Hiding Spot"))
class PIRATESVSVIKINGS_API ABRHidingSpot : public ABRGreyboxActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRHidingSpot();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

	UPROPERTY(Replicated, EditAnywhere, Category = "BR|Hiding") EBRHideStyle Style = EBRHideStyle::Dumpster;

protected:
	UPROPERTY(Replicated) TObjectPtr<ABRCharacter> Hider;
	FString StyleName() const;
};

// ===========================================================================================
/**
 * Mariachi band on a little stage. The band plays (put your song in Music); bring a Trumpet,
 * press E on the stage to join in, then press F ON THE BEAT. Hit notes in a row and the crowd tips you.
 */
UCLASS(meta = (DisplayName = "BR Mariachi Band"))
class PIRATESVSVIKINGS_API ABRMariachiBand : public ABRGreyboxActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRMariachiBand();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override { return 0.3f; }
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

	/** The band's song (loops). Set Beats Per Minute to match it so the beat lines up. */
	UPROPERTY(EditAnywhere, Category = "BR|Band") TObjectPtr<USoundBase> Music;
	UPROPERTY(EditAnywhere, Category = "BR|Band") float BeatsPerMinute = 110.f;
	/** Short trumpet notes played when you hit the beat (random). */
	UPROPERTY(EditAnywhere, Category = "BR|Band") TArray<TObjectPtr<USoundBase>> NoteSounds;
	/** Boo / wrong note sound. */
	UPROPERTY(EditAnywhere, Category = "BR|Band") TObjectPtr<USoundBase> MissSound;
	/** Most tips one performer can earn per minute (the crowd's wallet). */
	UPROPERTY(EditAnywhere, Category = "BR|Band") int32 MaxTipsPerMinute = 120;
	/** How far off the beat still counts (seconds). */
	UPROPERTY(EditAnywhere, Category = "BR|Band") float BeatWindow = 0.15f;

	bool IsPerformer(const ABRCharacter* Who) const;
	/** Server: a performer pressed F. */
	void PlayNote(ABRCharacter* Who);
	/** 0..1 through the current beat (for the HUD), on any machine. */
	float GetBeatPhase() const;
	int32 GetStreak(const ABRCharacter* Who) const;

protected:
	UPROPERTY(Replicated) float BeatStartTime = 0.f;
	UPROPERTY(Replicated) TArray<TObjectPtr<ABRCharacter>> Performers;
	UPROPERTY(Replicated) TArray<int32> Streaks;
	UPROPERTY() TObjectPtr<UAudioComponent> MusicAudio;
	UPROPERTY() TArray<TObjectPtr<ABRCharacter>> Musicians;
	TArray<int32> TipsThisMinute;
	TArray<float> TipWindowStart;
	float NextMusicianNote = 0.f;
	UFUNCTION(NetMulticast, Unreliable) void Multicast_Note(FVector_NetQuantize Where, bool bGood);
	void RemovePerformer(int32 Index);
};
