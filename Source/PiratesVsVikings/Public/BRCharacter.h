#pragma once

#include "CoreMinimal.h"
#include "PVVCharacter.h"
#include "BRTypes.h"
#include "BRCharacter.generated.h"

class ABRDesertArena;
class ABRGameState;
class ABRPlayerState;
class UInputAction;
class UPhysicalAnimationComponent;
class USkeletalMeshComponent;
class USoundBase;

/**
 * Border Run character.
 *  Everyone: hunger (Runners), stealth, crouch, swimming/drowning, wobbly physics body, outfit slots.
 *  Guns: hold RMB to aim, LMB to fire, R to reload. Wardens: X = taser/gun, Q = drone, V = scanner, B = place trap.
 *  NPC passengers (bIsNPC) use the same class so they look exactly like players.
 */
UCLASS()
class PIRATESVSVIKINGS_API ABRCharacter : public APVVCharacter
{
	GENERATED_BODY()

public:
	ABRCharacter();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	ABRPlayerState* GetBRPlayerState() const;
	ABRGameState* GetBRGameState() const;
	bool IsRunner() const { return GetFaction() == BR::Runners; }
	bool IsWarden() const { return GetFaction() == BR::Wardens; }
	bool IsNPC() const { return bIsNPC; }

	/** Call before FinishSpawning: a computer-controlled look-alike passenger. */
	void InitAsNPC();

	float GetHunger() const { return Hunger; }
	float GetVisibility() const { return Visibility; }
	float GetBreath() const { return Breath; }
	bool IsLethalMode() const { return bLethalMode; }
	bool IsAiming() const { return bAiming; }
	bool IsReloading() const { return bReloading; }
	bool IsSlowed() const;
	bool IsSpotted() const;
	bool HasWeapon() const;
	int32 GetAmmo() const;
	int32 GetMagSize() const;
	FString GetWeaponName() const;

	/** Server: short message on this player's screen. */
	void Notify(const FText& Message) const;

	// Server-side effects used by traps, dogs, drones and food.
	void Eat(float Amount) { Hunger = FMath::Min(100.f, Hunger + Amount); }
	void Spot(float Seconds);
	void ApplySlow(float Seconds, float SpeedFactor);
	void OnDroneFinished();

protected:
	virtual void EnsureDefaultInput() override;
	virtual void UpdateMovementSpeed() override;
	virtual void OnStateEffectsRefreshed() override;
	virtual bool WantsMeleeOnAttack() const override { return !bAiming; }

	// --- Components -------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BR|Components")
	TObjectPtr<UPhysicalAnimationComponent> PhysicalAnimation;

	/** Outfit slots: assign Skeletal Meshes made on the body's skeleton (they follow the body via Leader Pose). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BR|Outfit") TObjectPtr<USkeletalMeshComponent> OutfitTop;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BR|Outfit") TObjectPtr<USkeletalMeshComponent> OutfitBottom;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BR|Outfit") TObjectPtr<USkeletalMeshComponent> OutfitHead;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BR|Outfit") TObjectPtr<USkeletalMeshComponent> OutfitFace;

	/** The gun in your hand (greybox parts; set CustomGunMesh to use a real model instead). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BR|Weapons") TObjectPtr<USceneComponent> WeaponRoot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BR|Weapons") TObjectPtr<UStaticMeshComponent> GunBody;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BR|Weapons") TObjectPtr<UStaticMeshComponent> GunBarrel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BR|Weapons") TObjectPtr<UStaticMeshComponent> GunGrip;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BR|Weapons") TObjectPtr<UStaticMeshComponent> GunExtra;

	// --- Input (built at runtime unless assigned) -------------------------------------

	UPROPERTY(EditDefaultsOnly, Category = "BR|Input") TObjectPtr<UInputAction> AimAction;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Input") TObjectPtr<UInputAction> ReloadAction;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Input") TObjectPtr<UInputAction> CrouchAction;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Input") TObjectPtr<UInputAction> EatAction;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Input") TObjectPtr<UInputAction> DroneAction;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Input") TObjectPtr<UInputAction> WeaponModeAction;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Input") TObjectPtr<UInputAction> ScanAction;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Input") TObjectPtr<UInputAction> TrapAction;

	// --- Tuning -----------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, Category = "BR|Hunger") float HungerPerSecond = 100.f / 480.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Hunger") float TacoFood = 45.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Hunger") float WeakHunger = 25.f;

	/** Seconds you can stay in deep water before drowning. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Water") float BreathSeconds = 14.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Water") float RiverCurrent = 0.65f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Water") float DeepWaterSpeedMultiplier = 0.55f;

	UPROPERTY(EditDefaultsOnly, Category = "BR|Weapons") float WeaponRange = 6000.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Weapons") float AimSpreadDegrees = 0.6f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Weapons") float RecoilPitch = 0.9f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Weapons") float AimArmLength = 170.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Weapons") float AimFOV = 60.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Weapons") TObjectPtr<USoundBase> GunSound;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Weapons") TObjectPtr<USoundBase> TaserSound;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Weapons") TObjectPtr<USoundBase> ReloadSound;
	/** Optional real gun models (e.g. from Fab). Leave empty for the built-in greybox guns. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Weapons") TObjectPtr<UStaticMesh> CustomPistolMesh;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Weapons") TObjectPtr<UStaticMesh> CustomTaserMesh;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Weapons") TObjectPtr<UStaticMesh> CustomRevolverMesh;
	/** Hand bone the gun attaches to, and the offset/rotation that makes it sit right in your model's hand. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Weapons") FName HandSocket = TEXT("hand_r");
	UPROPERTY(EditDefaultsOnly, Category = "BR|Weapons") FTransform HandGripOffset;
	/** Where the gun floats while aiming (relative to the character, cartoon style). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Weapons") FVector AimGunOffset = FVector(55.f, 24.f, 48.f);

	UPROPERTY(EditDefaultsOnly, Category = "BR|Drone") float DroneCooldown = 20.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Wardens") int32 MaxTeamTraps = 8;

	/** Wobbly active-ragdoll body (needs a skeletal mesh with a Physics Asset). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Physics") bool bWobblyBody = true;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Physics") float WobbleStrength = 900.f;

private:
	UPROPERTY(Replicated) bool bIsNPC = false;
	UPROPERTY(Replicated) float Hunger = 100.f;
	UPROPERTY(Replicated) float Visibility = 1.f;
	UPROPERTY(Replicated) float SpottedUntil = 0.f;
	UPROPERTY(Replicated) float Breath = 1.f;
	UPROPERTY(Replicated) bool bLethalMode = false;
	UPROPERTY(ReplicatedUsing = OnRep_Aiming) bool bAiming = false;
	UPROPERTY(Replicated) bool bReloading = false;
	UPROPERTY(Replicated) int32 AmmoTaser = 1;
	UPROPERTY(Replicated) int32 AmmoGun = 12;
	UPROPERTY(Replicated) int32 AmmoRunnerGun = 6;
	UPROPERTY(ReplicatedUsing = OnRep_Slow) float SlowUntil = 0.f;
	UPROPERTY(ReplicatedUsing = OnRep_Slow) float SlowFactor = 1.f;

	TWeakObjectPtr<ABRDesertArena> Arena;
	float LastFireTime = -100.f;
	float LastLocalFireTime = -100.f;
	float LastDroneTime = -100.f;
	float LastFaintTime = -100.f;
	float LastScanTime = -100.f;
	float NextVisibilityUpdate = 0.f;
	float DefaultArmLength = 450.f;
	float DefaultFOV = 90.f;
	FVector DefaultSocketOffset = FVector::ZeroVector;
	int32 NextTrapType = 0;
	bool bInDrone = false;
	bool bStealthHidden = false;
	bool bInDeepWater = false;
	bool bSlowApplied = false;
	bool bWobbleOn = false;
	int32 GunStyle = -1;   // 0 none, 1 taser, 2 pistol, 3 revolver
	int32 GunPlacement = -1; // 0 hand, 1 aiming
	FTimerHandle ReloadTimer;

	ABRDesertArena* FindArena();
	void UpdateLocalVisibility();
	void UpdateAimCamera(float DeltaSeconds);
	void ApplyAimRotationMode();
	void EnableWobble();
	int32& AmmoRef();
	float FireCooldown() const;
	float ReloadSeconds() const;
	void FinishReload();
	void Drown();
	void UpdateGunVisual();
	FVector GetMuzzleLocation() const;

	UFUNCTION() void OnRep_Aiming();
	UFUNCTION() void OnRep_Slow();

	void Input_AimStart();
	void Input_AimStop();
	void Input_FireIfAiming();
	void Input_Reload();
	void Input_Crouch();
	void Input_Eat();
	void Input_Drone();
	void Input_WeaponMode();
	void Input_Scan();
	void Input_Trap();

	UFUNCTION(Server, Reliable) void Server_SetAiming(bool bNewAiming);
	UFUNCTION(Server, Reliable) void Server_Fire(FVector_NetQuantize Start, FVector_NetQuantizeNormal Direction);
	UFUNCTION(Server, Reliable) void Server_Reload();
	UFUNCTION(Server, Reliable) void Server_Eat();
	UFUNCTION(Server, Reliable) void Server_LaunchDrone();
	UFUNCTION(Server, Reliable) void Server_ToggleWeaponMode();
	UFUNCTION(Server, Reliable) void Server_Scan(FVector_NetQuantize Start, FVector_NetQuantizeNormal Direction);
	UFUNCTION(Server, Reliable) void Server_PlaceTrap();
	UFUNCTION(NetMulticast, Unreliable) void Multicast_Shot(FVector_NetQuantize Start, FVector_NetQuantize End, bool bLethal, bool bHitSomeone);
};
