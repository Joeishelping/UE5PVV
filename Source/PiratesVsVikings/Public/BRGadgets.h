#pragma once

#include "CoreMinimal.h"
#include "BRWorldActors.h"
#include "BRGadgets.generated.h"

/** A smoke bomb's cloud: anyone inside is hidden from scanners, cameras and Border Patrol spotting. */
UCLASS(NotPlaceable)
class PIRATESVSVIKINGS_API ABRSmokeCloud : public AActor
{
	GENERATED_BODY()

public:
	ABRSmokeCloud();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "BR|Smoke") float Radius = 480.f;
	UPROPERTY(EditAnywhere, Category = "BR|Smoke") float Seconds = 14.f;

	/** Is this spot inside any smoke cloud? */
	static bool IsInSmoke(const UWorld* World, const FVector& Location);

protected:
	float Age = 0.f;
	float NextPuff = 0.f;
};

/** A planted bundle of TNT: beeps for a few seconds, then BOOM (knocks everyone down, blows holes in the Wall, wrecks fences). */
UCLASS(NotPlaceable)
class PIRATESVSVIKINGS_API ABRTNTCharge : public ABRGreyboxActor
{
	GENERATED_BODY()

public:
	ABRTNTCharge();
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "BR|TNT") float FuseSeconds = 5.f;
	UPROPERTY(EditAnywhere, Category = "BR|TNT") float BlastRadius = 550.f;
	TWeakObjectPtr<class ABRCharacter> PlantedBy;

protected:
	float Age = 0.f;
	int32 LastBeep = -1;
	void Explode();
};
