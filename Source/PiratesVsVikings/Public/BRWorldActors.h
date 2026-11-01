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
	UPROPERTY(Replicated) EBRStationKind Kind = EBRStationKind::BuyItem;
	UPROPERTY(Replicated) EBRItem Item = EBRItem::None;
	UPROPERTY(Replicated) int32 Price = 10;
	UPROPERTY(Replicated) float WorkSeconds = 0.f;
	UPROPERTY(Replicated) float CooldownSeconds = 0.f;
	UPROPERTY(Replicated) bool bNightOnly = false;
	UPROPERTY(Replicated) FString Title;
	UPROPERTY(Replicated) FLinearColor Color = FLinearColor(0.6f, 0.6f, 0.6f);
	/** Bet stations: which rooster (0 = RED, 1 = BLUE) and the pit. Price 0 = ALL IN. */
	UPROPERTY(Replicated) int32 BetSide = 0;
	UPROPERTY(Replicated) TObjectPtr<ABRCockfightPit> Pit;
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

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> RoosterRed;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> RoosterBlue;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Board;

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
