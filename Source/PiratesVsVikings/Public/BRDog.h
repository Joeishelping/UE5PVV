#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BRDog.generated.h"

class ABRCharacter;
class UStaticMeshComponent;
class USoundBase;

/**
 * Patrol dog. Wanders around its home spot; during the Invasion it SMELLS nearby Runners
 * (darkness doesn't help, crouching a bit, water hides your scent), chases, barks (pings Wardens)
 * and grabs a leg (big slow). Simple server-side brain, no NavMesh needed.
 */
UCLASS()
class PIRATESVSVIKINGS_API ABRDog : public ACharacter
{
	GENERATED_BODY()

public:
	ABRDog();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "BR|Dog") float SmellRange = 2200.f;
	/** Dogs run to help any Border Patrol agent being attacked this close (cm). */
	UPROPERTY(EditAnywhere, Category = "BR|Dog") float DefendRange = 3500.f;
	/** Anyone scanned as illegal / carrying contraband in the last this-many seconds is fair game. */
	UPROPERTY(EditAnywhere, Category = "BR|Dog") float ScanMemorySeconds = 120.f;
	UPROPERTY(EditAnywhere, Category = "BR|Dog") float PatrolRadius = 2500.f;
	UPROPERTY(EditAnywhere, Category = "BR|Dog") float ChaseSpeed = 600.f;
	UPROPERTY(EditAnywhere, Category = "BR|Dog") float PatrolSpeed = 260.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Dog") TObjectPtr<USoundBase> BarkSound;
	/** Leash length (cm) when a Border Patrol agent walks the dog. */
	UPROPERTY(EditAnywhere, Category = "BR|Dog") float LeashLength = 380.f;

	/** Server: a Border Patrol agent takes the leash (Z) / lets the dog loose to chase (Z again). */
	void SetHandler(ABRCharacter* NewHandler);
	void ReleaseLeash();
	bool HasHandler() const { return Handler.IsValid(); }

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Visual;

	UFUNCTION(NetMulticast, Unreliable) void Multicast_Bark();

private:
	FVector Home = FVector::ZeroVector;
	FVector PatrolTarget = FVector::ZeroVector;
	TWeakObjectPtr<ABRCharacter> Target;
	/** The target is running or fighting (bite = take them down) vs. just standing there busted (bark and guard them). */
	bool bTargetResisting = false;
	/** Gives up on someone it can't catch after this long (and leaves them alone for a bit). */
	UPROPERTY(EditAnywhere, Category = "BR|Dog") float GiveUpSeconds = 15.f;
	float ChaseSince = 0.f;
	TWeakObjectPtr<ABRCharacter> IgnoreTarget;
	float IgnoreUntil = 0.f;
	int32 SteerSide = 0;
	TWeakObjectPtr<ABRCharacter> Handler;
	float NextThink = 0.f;
	float NextBark = 0.f;
	float NextBite = 0.f;

	void Think();
	void PickPatrolPoint();
};
