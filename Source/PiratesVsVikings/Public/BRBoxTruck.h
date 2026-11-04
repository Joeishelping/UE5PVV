#pragma once

// Box truck: a cab plus a big enclosed cargo hold with two swinging back doors.
//  - Stuff it with smuggling packages and people, cover them with crates. Everything back there is real physics
//    (it rides locked down while the truck is moving, and tumbles about when it brakes / stops).
//  - The back doors swing on their hinges (they flap when the truck brakes or turns) and stay latched once closed.
//  - Border Patrol have to open the doors, climb in, shift the crates and grab the packages (= confiscated)
//    and arrest anyone hiding back there.
//  - Runners get one from the SMUGGLING TRUCK JOB board in Mexico; some NPC traffic is box trucks too.

#include "CoreMinimal.h"
#include "BRCar.h"
#include "BRWorldActors.h"
#include "PVVInteractable.h"
#include "BRBoxTruck.generated.h"

class ABRCharacter;
class ABRPhysicsProp;
class UBoxComponent;

UCLASS(meta = (DisplayName = "BR Box Truck"))
class PIRATESVSVIKINGS_API ABRBoxTruck : public ABRTruck
{
	GENERATED_BODY()

public:
	ABRBoxTruck();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual FString GetModelName() const override { return TEXT("Box Truck"); }

	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;
	virtual bool HoldsAtCheckpoint() const override { return bDoorsOpen; }

	/** Server. */
	void SetDoorsOpen(bool bOpen, APVVCharacter* By);
	bool AreDoorsOpen() const { return bDoorsOpen; }
	/** Is this world point inside the cargo hold? */
	bool IsInHold(const FVector& WorldPoint, float Margin = 0.f) const;
	/** Middle of the hold (world), and just outside the back doors (world). */
	FVector GetHoldCenter() const;
	FVector GetDoorStep() const;
	/** Server: NPC traffic - fill the back with crates, maybe smuggling packages and stowaways. */
	void LoadNPCCargo();
	/** Server: spawn a cargo prop (shape 0 crate / 5 package) at a world spot. */
	ABRPhysicsProp* SpawnCargo(const FVector& At, bool bPackage, class ABRPlayerState* Owner);
	/** The client's coyote owns a box truck nearby with its doors open: climb in the back. */
	static ABRBoxTruck* FindHoldForClient(const ABRCharacter* Client);

	/** Hold size (local cm, behind the cab). */
	static constexpr float HoldFrontX = -30.f;
	static constexpr float HoldBackX = -440.f;
	static constexpr float HoldHalfY = 100.f;
	static constexpr float HoldFloorZ = 40.f;
	static constexpr float HoldTopZ = 265.f;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> HoldFloor;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> HingeL;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> HingeR;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> DoorL;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> DoorR;
	/** Invisible wall across the back while the doors are latched. */
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> DoorBlocker;
	/** Loading ramp: down while the doors are open and the truck's stopped. */
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Ramp;
	bool bRampDown = false;
	UPROPERTY(ReplicatedUsing = OnRep_Doors) bool bDoorsOpen = false;
	UFUNCTION() void OnRep_Doors();

private:
	// Swinging doors: angle (deg, 0 = shut, 175 = flat against the sides) and speed, simulated on every screen.
	float DoorAngle[2] = { 0.f, 0.f };
	float DoorSpeed[2] = { 0.f, 0.f };
	FVector LastVelocity = FVector::ZeroVector;
	bool bBedHidden = false;
	bool bCargoFrozen = false;
	float SlowSince = -1.f;
	float NoCopNearSince = -1.f;
	UPROPERTY() TArray<TObjectPtr<ABRPhysicsProp>> Cargo;
	/** NPC stowaways (and Runners' clients) locked in the back: they ride attached until the doors open. */
	TArray<TWeakObjectPtr<ABRCharacter>> Stowed;
	void TickDoors(float DeltaSeconds);
	void TickCargo(float DeltaSeconds);
	void StowPeople();
	void ReleaseStowed(ABRCharacter* Finder);
	void ApplyDoorCollision();
	bool WantsDoor(const APVVCharacter* Interactor) const;
	UStaticMeshComponent* HoldPart(const TCHAR* Name, const FVector& At, const FVector& SizeCm, bool bSolid);
};

/** Mexico: hold E to take a smuggling truck job - a box truck, packages to load and two clients to hide in the back. */
UCLASS(meta = (DisplayName = "BR Smuggling Truck Job"))
class PIRATESVSVIKINGS_API ABRTruckJobBoard : public ABRGreyboxActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRTruckJobBoard();
	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override { return 1.f; }
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

	/** Paid per package delivered to the Safe Zone (fined 300 per package Border Patrol seize). */
	UPROPERTY(EditAnywhere, Category = "BR|Job") int32 PayPerPackage = 400;
	UPROPERTY(EditAnywhere, Category = "BR|Job") int32 Packages = 4;
	UPROPERTY(EditAnywhere, Category = "BR|Job") int32 Clients = 2;
	UPROPERTY(EditAnywhere, Category = "BR|Job") float CooldownSeconds = 90.f;
	/** Where the truck is parked (local offset; drag the diamond). */
	UPROPERTY(EditAnywhere, Category = "BR|Job", meta = (MakeEditWidget = "true")) FVector TruckSpot = FVector(0.f, 900.f, 0.f);

private:
	float NextJob = 0.f;
	TWeakObjectPtr<ABRBoxTruck> ActiveTruck;
};
