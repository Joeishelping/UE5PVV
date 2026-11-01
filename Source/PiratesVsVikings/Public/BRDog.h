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
	UPROPERTY(EditAnywhere, Category = "BR|Dog") float PatrolRadius = 2500.f;
	UPROPERTY(EditAnywhere, Category = "BR|Dog") float ChaseSpeed = 680.f;
	UPROPERTY(EditAnywhere, Category = "BR|Dog") float PatrolSpeed = 260.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Dog") TObjectPtr<USoundBase> BarkSound;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Visual;

	UFUNCTION(NetMulticast, Unreliable) void Multicast_Bark();

private:
	FVector Home = FVector::ZeroVector;
	FVector PatrolTarget = FVector::ZeroVector;
	TWeakObjectPtr<ABRCharacter> Target;
	float NextThink = 0.f;
	float NextBark = 0.f;
	float NextBite = 0.f;

	void Think();
	void PickPatrolPoint();
};
