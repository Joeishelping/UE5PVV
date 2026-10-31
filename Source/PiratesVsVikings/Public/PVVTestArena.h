#pragma once

#include "CoreMinimal.h"
#include "PVVLake.h"
#include "PVVTestArena.generated.h"

class APVVBoat;
class UStaticMesh;
class UBoxComponent;
class UDirectionalLightComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UTextRenderComponent;

/**
 * Self-building greybox map so the prototype is playable in an empty level.
 * The GameMode spawns one automatically when the level has no APVVLake.
 *
 * Layout (origin = water surface, X runs Pirates -> Vikings):
 *   West shore:  The Learning Center (Pirate spawn), two docks, beach ramp
 *   Lake:        waist-deep (characters wade slowly), boats float, walls around the edge
 *   East shore:  Viking Processing Facility (Viking spawn), detention cell, two docks, beach ramp
 */
UCLASS()
class PIRATESVSVIKINGS_API APVVTestArena : public APVVLake
{
	GENERATED_BODY()

public:
	APVVTestArena();

	virtual void BeginPlay() override;

	/** Server only: player starts, detention zone and boats. */
	void SpawnGameplayActors(TSubclassOf<APVVBoat> BoatClass, int32 BoatsPerSide);

protected:
	/** Invisible floor under the water that only people (and ragdolls) stand on; boats pass over it. */
	UPROPERTY(VisibleAnywhere, Category = "PVV|Arena")
	TObjectPtr<UBoxComponent> LakeBed;

	UPROPERTY(VisibleAnywhere, Category = "PVV|Arena")
	TObjectPtr<UDirectionalLightComponent> Sun;

	UPROPERTY(VisibleAnywhere, Category = "PVV|Arena")
	TObjectPtr<USkyLightComponent> SkyLight;

	UPROPERTY(VisibleAnywhere, Category = "PVV|Arena")
	TObjectPtr<USkyAtmosphereComponent> Atmosphere;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

private:
	UStaticMeshComponent* AddBlock(const TCHAR* Name, const FVector& Center, const FVector& SizeCm, const FRotator& Rotation = FRotator::ZeroRotator);
	UTextRenderComponent* AddSign(const TCHAR* Name, const FString& Text, const FVector& Location, float Yaw, float Size, const FColor& Color);
};
