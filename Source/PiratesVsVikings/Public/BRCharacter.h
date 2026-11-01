#pragma once

#include "CoreMinimal.h"
#include "PVVCharacter.h"
#include "BRTypes.h"
#include "BRCharacter.generated.h"

class ABRDesertArena;
class ABRGameState;
class ABRPlayerState;
class UInputAction;

/**
 * Border Run character: adds hunger, stealth (visibility), crouch, eating, guns/tasers,
 * drones (Wardens), the river current and short on-screen messages.
 */
UCLASS()
class PIRATESVSVIKINGS_API ABRCharacter : public APVVCharacter
{
	GENERATED_BODY()

public:
	ABRCharacter();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	ABRPlayerState* GetBRPlayerState() const;
	ABRGameState* GetBRGameState() const;
	bool IsRunner() const { return GetFaction() == BR::Runners; }
	bool IsWarden() const { return GetFaction() == BR::Wardens; }

	float GetHunger() const { return Hunger; }
	float GetVisibility() const { return Visibility; }
	bool IsLethalMode() const { return bLethalMode; }
	bool IsSpotted() const;

	/** Server: a short message on this player's screen (via their PlayerController). */
	void Notify(const FText& Message) const;

	/** Server: feed. */
	void Eat(float Amount) { Hunger = FMath::Min(100.f, Hunger + Amount); }

	/** Server: mark visible to Wardens for a while (drone tag). */
	void Spot(float Seconds);

	/** Server: called by the drone when the pilot comes back. */
	void OnDroneFinished();

protected:
	virtual void EnsureDefaultInput() override;
	virtual void UpdateMovementSpeed() override;

	UPROPERTY(EditDefaultsOnly, Category = "BR|Input") TObjectPtr<UInputAction> FireAction;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Input") TObjectPtr<UInputAction> CrouchAction;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Input") TObjectPtr<UInputAction> EatAction;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Input") TObjectPtr<UInputAction> DroneAction;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Input") TObjectPtr<UInputAction> WeaponModeAction;

	/** Full -> empty in about 8 minutes. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Hunger") float HungerPerSecond = 100.f / 480.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Hunger") float TacoFood = 45.f;
	/** Below this you can't sprint. At 0 you faint now and then. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Hunger") float WeakHunger = 25.f;

	UPROPERTY(EditDefaultsOnly, Category = "BR|Weapons") float WeaponRange = 5000.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Weapons") float TaserCooldown = 1.5f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Weapons") float GunCooldown = 0.45f;

	UPROPERTY(EditDefaultsOnly, Category = "BR|Drone") float DroneCooldown = 20.f;

	/** Strength of the river current pushing swimmers downstream (0..1 of walk input). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|River") float RiverCurrent = 0.65f;

private:
	UPROPERTY(Replicated) float Hunger = 100.f;
	UPROPERTY(Replicated) float Visibility = 1.f;
	UPROPERTY(Replicated) float SpottedUntil = 0.f;
	UPROPERTY(Replicated) bool bLethalMode = false;

	TWeakObjectPtr<ABRDesertArena> Arena;
	float LastFireTime = -100.f;
	float LastDroneTime = -100.f;
	float LastFaintTime = -100.f;
	float NextVisibilityUpdate = 0.f;
	bool bInDrone = false;
	bool bStealthHidden = false;

	ABRDesertArena* FindArena();
	void UpdateStealthRendering();

	void Input_Fire();
	void Input_Crouch();
	void Input_Eat();
	void Input_Drone();
	void Input_WeaponMode();

	UFUNCTION(Server, Reliable) void Server_Fire(FVector_NetQuantize Start, FVector_NetQuantizeNormal Direction);
	UFUNCTION(Server, Reliable) void Server_Eat();
	UFUNCTION(Server, Reliable) void Server_LaunchDrone();
	UFUNCTION(Server, Reliable) void Server_ToggleWeaponMode();
	UFUNCTION(NetMulticast, Unreliable) void Multicast_Shot(FVector_NetQuantize Start, FVector_NetQuantize End, bool bLethal);
};
