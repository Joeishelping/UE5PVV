#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PVVInteractable.h"
#include "BRTypes.h"
#include "BRWorldActors.generated.h"

class ABRCockfightPit;
class ABRPlayerState;
class APVVCharacter;
class UBoxComponent;
class UPointLightComponent;
class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UStaticMesh;

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
	/** Boat stations: where the boat appears (drag the diamond in the viewport; should be on the water). */
	UPROPERTY(EditAnywhere, Category = "BR|Station", meta = (MakeEditWidget = "true")) FVector BoatSpawnOffset = FVector(600.f, 0.f, 0.f);
	FVector BoatSpawn = FVector::ZeroVector;

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
	static constexpr float GateHalfWidth = 350.f;
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
/** Launches released detainees back over the wall. No damage, maximum comedy. */
UCLASS()
class PIRATESVSVIKINGS_API ABRCatapult : public ABRGreyboxActor
{
	GENERATED_BODY()

public:
	ABRCatapult();
	virtual void Tick(float DeltaSeconds) override;

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

	bool IsOpen() const { return bOpen; }
	/** Server. */
	void OpenFor(float Seconds);

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Barrier;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> ArmPivot;

private:
	UPROPERTY(Replicated) bool bOpen = false;
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
	UPROPERTY(EditDefaultsOnly, Category = "BR|Village") float LampBrightness = 6000.f;

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
