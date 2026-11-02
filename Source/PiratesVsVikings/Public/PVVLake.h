#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PVVLake.generated.h"

class UStaticMeshComponent;

/**
 * Flat water surface. Boats read their water height from this actor's Z.
 * Characters swim via a separate Physics Volume with "Water Volume" ticked covering the lake.
 */
UCLASS()
class PIRATESVSVIKINGS_API APVVLake : public AActor
{
	GENERATED_BODY()

public:
	APVVLake();

	UFUNCTION(BlueprintPure, Category = "PVV|Water")
	float GetWaterSurfaceZ() const { return GetActorLocation().Z; }

	virtual void BeginPlay() override;

	/** Is this point over/in this body of water? (Plain lakes: everywhere.) */
	virtual bool ContainsPoint(const FVector& WorldLocation) const { return true; }
	/** The water at this spot (first lake that contains it), or nullptr. */
	static APVVLake* FindWaterAt(const UWorld* World, const FVector& WorldLocation);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PVV|Water")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PVV|Water")
	TObjectPtr<UStaticMeshComponent> Surface;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PVV|Water")
	FLinearColor WaterColor = FLinearColor(0.02f, 0.15f, 0.3f);
};
