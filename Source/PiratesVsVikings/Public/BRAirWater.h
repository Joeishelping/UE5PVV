#pragma once

#include "CoreMinimal.h"
#include "PVVBoat.h"
#include "BRWorldActors.h"
#include "BRAirWater.generated.h"

class ABRCharacter;

/** Inflatable raft: a light boat that POPS if it's shot or hit by anti-air (everyone ends up in the river). */
UCLASS(meta = (DisplayName = "BR Inflatable Raft"))
class PIRATESVSVIKINGS_API ABRRaft : public APVVBoat
{
	GENERATED_BODY()

public:
	ABRRaft();
	virtual void BeginPlay() override;
	/** Server: hiss - pop - everyone in the drink. */
	void Pop(ABRCharacter* By);
};

/**
 * Hot air balloon at a launch pad in town. A Runner pays for the ride (E) and a second Runner can hop in before
 * it lifts off. It rises, drifts across the border (the riders steer a little with the mouse) and lands past the
 * Wall. Border Patrol can pop it with a gun or the anti-air guns at the watchtowers - the riders fall.
 */
UCLASS(meta = (DisplayName = "BR Hot Air Balloon"))
class PIRATESVSVIKINGS_API ABRBalloon : public ABRGreyboxActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRBalloon();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override { return 0.5f; }
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

	UPROPERTY(EditAnywhere, Category = "BR|Balloon") int32 Price = 1500;
	UPROPERTY(EditAnywhere, Category = "BR|Balloon") float CruiseHeight = 2200.f;
	UPROPERTY(EditAnywhere, Category = "BR|Balloon") float DriftSpeed = 380.f;
	/** Land once this far past the Wall (cm, along +X). */
	UPROPERTY(EditAnywhere, Category = "BR|Balloon") float LandPastWall = 4000.f;

	/** Server: popped (shot / anti-air): riders fall, the balloon drops and comes back to the pad later. */
	void Pop(ABRCharacter* By);
	bool IsFlying() const { return State == 2 || State == 3; }

protected:
	/** 0 = parked, 1 = boarding (lifts off soon), 2 = flying, 3 = landing, 4 = deflated / respawning. */
	UPROPERTY(Replicated) uint8 State = 0;
	UPROPERTY(Replicated) TArray<TObjectPtr<ABRCharacter>> Riders;
	FVector PadLocation;
	float StateTime = 0.f;
	float FlyYaw = 0.f;
	void PlaceRiders();
	void DropRiders(bool bFall);
};

/** Anti-air gun by a watchtower: Border Patrol hold E to fire at the nearest balloon or raft (good odds, not certain). */
UCLASS(meta = (DisplayName = "BR Anti-Air Gun"))
class PIRATESVSVIKINGS_API ABRAntiAir : public ABRGreyboxActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRAntiAir();
	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override { return 0.6f; }
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

	UPROPERTY(EditAnywhere, Category = "BR|AntiAir") float Range = 9000.f;
	UPROPERTY(EditAnywhere, Category = "BR|AntiAir") float HitChance = 0.7f;
	UPROPERTY(EditAnywhere, Category = "BR|AntiAir") float Cooldown = 8.f;

protected:
	float NextShot = 0.f;
	AActor* FindTarget() const;
};

/** Wheelbarrow: grab the handles (E) and push a teammate around in it (they hop in with E). Z / E again = let go. */
UCLASS(meta = (DisplayName = "BR Wheelbarrow"))
class PIRATESVSVIKINGS_API ABRWheelbarrow : public ABRGreyboxActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRWheelbarrow();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override { return 0.f; }
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

	/** Server: let go of the handles (the rider hops out). Also Z while pushing. */
	void LetGo();
	/** Server: drop whatever wheelbarrow this person is pushing / riding (returns true if there was one). */
	static bool ReleaseFor(ABRCharacter* Who);

protected:
	UPROPERTY(Replicated) TObjectPtr<ABRCharacter> Pusher;
	UPROPERTY(Replicated) TObjectPtr<ABRCharacter> Rider;
	/** The tray bumps into things only while nobody's pushing (the E zone always stays findable). */
	void SetSolid(bool bSolid);
};

namespace BRAir
{
	/** A bullet / anti-air round hit this actor: pop it if it's a balloon or raft. */
	PIRATESVSVIKINGS_API bool TryPop(AActor* Hit, ABRCharacter* By);
}
