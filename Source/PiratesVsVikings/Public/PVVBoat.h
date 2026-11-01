#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "PVVInteractable.h"
#include "PVVBoat.generated.h"

class APVVCharacter;
class APVVLake;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class USpringArmComponent;
class UStaticMeshComponent;
struct FInputActionValue;

/**
 * Physics boat. The hull simulates on every machine and is corrected by server replication;
 * buoyancy comes from pontoon points sampled against a flat water height (APVVLake).
 *
 * Seat 0 is the driver: the driver's controller possesses the boat. Other seats are passengers
 * (who can still look and swing) or restrained prisoners being transported.
 *
 * Out of the box the hull is a scaled engine cube. For a real mesh: set it on Hull, reset the
 * Hull scale to 1 and drag the seat/pontoon widgets onto the mesh in the Blueprint viewport.
 */
UCLASS()
class PIRATESVSVIKINGS_API APVVBoat : public APawn, public IPVVInteractable
{
	GENERATED_BODY()

public:
	APVVBoat();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void FellOutOfWorld(const UDamageType& DmgType) override;

	// IPVVInteractable
	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

	UFUNCTION(BlueprintPure, Category = "PVV|Boat")
	APVVCharacter* GetDriver() const;

	UFUNCTION(BlueprintPure, Category = "PVV|Boat")
	float GetForwardSpeed() const;

	UStaticMeshComponent* GetHull() const { return Hull; }
	const TArray<TObjectPtr<APVVCharacter>>& GetOccupants() const { return Occupants; }

	// Server only.
	bool TryBoard(APVVCharacter* Character);
	bool LoadPrisoner(APVVCharacter* Carrier);
	/** Puts the character beside the boat. If it was the driver, they get their body back. */
	void Unseat(APVVCharacter* Character);
	void EjectAll();
	void EjectRandomOccupant();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PVV|Components")
	TObjectPtr<UStaticMeshComponent> Hull;

	/** Unscaled frame the occupants attach to, so hull scale never stretches players. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PVV|Components")
	TObjectPtr<USceneComponent> SeatRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PVV|Components")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PVV|Components")
	TObjectPtr<UCameraComponent> Camera;

	/** Capsule-centre positions, relative to the boat, unscaled. Index 0 = driver. */
	UPROPERTY(EditAnywhere, Category = "PVV|Boat|Seats", meta = (MakeEditWidget = true))
	TArray<FVector> SeatOffsets;

	/** Buoyancy sample points, relative to the boat, unscaled. Spread them to the hull corners. */
	UPROPERTY(EditAnywhere, Category = "PVV|Boat|Buoyancy", meta = (MakeEditWidget = true))
	TArray<FVector> PontoonOffsets;

	/** Used if no APVVLake is placed. */
	UPROPERTY(EditAnywhere, Category = "PVV|Boat|Buoyancy")
	float WaterSurfaceZ = 0.f;

	/** 2 = floats with each pontoon half submerged. */
	UPROPERTY(EditAnywhere, Category = "PVV|Boat|Buoyancy")
	float BuoyancyMultiplier = 2.f;

	/** Depth over which a pontoon goes from 0 to full lift. */
	UPROPERTY(EditAnywhere, Category = "PVV|Boat|Buoyancy")
	float PontoonDepth = 50.f;

	UPROPERTY(EditAnywhere, Category = "PVV|Boat|Buoyancy")
	float VerticalDamping = 2.f;

	UPROPERTY(EditAnywhere, Category = "PVV|Boat|Handling")
	float MassKg = 600.f;

	/** cm/s^2 at full throttle. Top speed ~= ForwardAcceleration / ForwardDrag. */
	UPROPERTY(EditAnywhere, Category = "PVV|Boat|Handling")
	float ForwardAcceleration = 900.f;

	UPROPERTY(EditAnywhere, Category = "PVV|Boat|Handling")
	float ReverseAcceleration = 400.f;

	UPROPERTY(EditAnywhere, Category = "PVV|Boat|Handling")
	float ForwardDrag = 0.7f;

	/** Sideways water resistance. Lower = driftier. */
	UPROPERTY(EditAnywhere, Category = "PVV|Boat|Handling")
	float LateralGrip = 2.5f;

	/** rad/s at full lock and speed. */
	UPROPERTY(EditAnywhere, Category = "PVV|Boat|Handling")
	float MaxTurnRate = 1.4f;

	UPROPERTY(EditAnywhere, Category = "PVV|Boat|Handling")
	float TurnResponsiveness = 3.f;

	/** Speed (cm/s) at which steering reaches full authority. */
	UPROPERTY(EditAnywhere, Category = "PVV|Boat|Handling")
	float FullSteerSpeed = 600.f;

	UPROPERTY(EditAnywhere, Category = "PVV|Boat|Handling")
	float MinSteerAuthority = 0.3f;

	/** Hitting a swimmer/walker above this speed knocks them down. */
	UPROPERTY(EditAnywhere, Category = "PVV|Boat|Ramming")
	float RamKnockdownSpeed = 700.f;

	UPROPERTY(EditAnywhere, Category = "PVV|Boat|Ramming")
	float RamLaunchStrength = 900.f;

	/** Closing speed against another boat that throws one of its occupants overboard. */
	UPROPERTY(EditAnywhere, Category = "PVV|Boat|Ramming")
	float RamEjectSpeed = 900.f;

	/** Upside-down this long -> occupants thrown clear and the boat is righted. */
	UPROPERTY(EditAnywhere, Category = "PVV|Boat")
	float FlipRecoveryTime = 3.f;

	// Input (leave empty for defaults: W/S throttle, A/D steer, mouse look, E exit)
	UPROPERTY(EditDefaultsOnly, Category = "PVV|Input")
	TObjectPtr<UInputMappingContext> BoatMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Input")
	TObjectPtr<UInputAction> ThrottleAction;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Input")
	TObjectPtr<UInputAction> SteerAction;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Input")
	TObjectPtr<UInputAction> ExitAction;

	UFUNCTION(BlueprintImplementableEvent, Category = "PVV|Boat")
	void OnOccupantsChanged();

	/** Height the hull floats at for a sample point. Boats: the lake. Cars: the ground under them. */
	virtual bool GetSurfaceZ(const FVector& Point, float& OutZ) const;
	/** Multiplies engine force (cars: 0 when out of fuel). */
	virtual float GetDriveScale() const { return 1.f; }
	/** Whether this character may take the driver's seat (cars: needs the key). */
	virtual bool CanDrive(const APVVCharacter* Character) const { return true; }

	UPROPERTY(Replicated)
	float ThrottleInput = 0.f;

	UPROPERTY(Replicated)
	float SteerInput = 0.f;

	TWeakObjectPtr<APVVLake> Lake;

	virtual void EnsureDefaultInput();
	void SeatCharacter(APVVCharacter* Character, int32 SeatIndex);
	int32 FindFreePassengerSeat() const;

	UPROPERTY(ReplicatedUsing = OnRep_Occupants, BlueprintReadOnly, Category = "PVV|Boat")
	TArray<TObjectPtr<APVVCharacter>> Occupants;

	/** 0..1 of pontoons in the water. */
	UPROPERTY(BlueprintReadOnly, Category = "PVV|Boat")
	float WetFraction = 0.f;

private:
	float SentThrottle = 0.f;
	float SentSteer = 0.f;
	float FlippedTime = 0.f;
	float LastRamTime = -100.f;
	FTransform SpawnTransform;

	UFUNCTION()
	void OnRep_Occupants();

	UFUNCTION()
	void OnHullHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	void ApplyWaterForces(float DeltaSeconds);
	void TickFlipRecovery(float DeltaSeconds);
	void ClearSeat(int32 SeatIndex);
	void ResetToTransform(const FTransform& Transform);
	void SendDriveInput();

	void Input_Throttle(const FInputActionValue& Value);
	void Input_ThrottleReleased();
	void Input_Steer(const FInputActionValue& Value);
	void Input_SteerReleased();
	void Input_Look(const FInputActionValue& Value);
	void Input_Exit();

	UFUNCTION(Server, Unreliable)
	void Server_SetDriveInput(float Throttle, float Steer);

	UFUNCTION(Server, Reliable)
	void Server_Exit();
};
