#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PVVLake.h"
#include "PVVInteractable.h"
#include "BRMapKit.generated.h"

class ABRCatapult;
class ABRCheckpointGate;
class UArrowComponent;
class UMaterialInterface;
class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/*
 * MAP KIT - build your own Border Run map.
 * Drag these into your level (Place Actors panel: search "BR "). The game finds them when the match starts.
 * If your level has none of them, the built-in test map is used instead.
 *
 *   BR Border Line        - where the Line is. The arrow points to the side Runners are trying to reach.
 *   BR River              - deep water: drowning, the current, trucks flood, boats float. Scale it to fit.
 *   BR Safe Zone          - Runners who reach it are safe. Scale it to fit.
 *   BR Road Point         - NPC truck route. Order 0 = where trucks appear, then 1, 2, 3... Tick "Wait For Checkpoint" on the one in front of the gate.
 *   BR Villager Spot      - NPC townsfolk hang out and wander around these.
 *   BR Catapult Landing   - where the catapult throws released detainees.
 *   BR Wall Segment       - a piece of the Wall (Runners lean ladders on it). Set its Length.
 * Also placeable: BR Station (shops/jobs), BR Spike Fence, BR Checkpoint Gate, BR Catapult, BR Cockfight Pit,
 * PVV Detention Zone (holding pen), PVV Player Start (set Faction), your truck and dog Blueprints.
 */

/** Shared helpers: one place that answers "where is the river / line / safe zone" for the whole game. */
namespace BRMap
{
	PIRATESVSVIKINGS_API bool IsInRiver(const UWorld* World, const FVector& Location);
	PIRATESVSVIKINGS_API bool IsDeepWater(const UWorld* World, const FVector& Location);
	/** Direction the river pushes swimmers at this spot (zero if not in a river). */
	PIRATESVSVIKINGS_API FVector RiverFlow(const UWorld* World, const FVector& Location);
	PIRATESVSVIKINGS_API bool HasCrossedLine(const UWorld* World, const FVector& Location);
	PIRATESVSVIKINGS_API bool IsInSafeZone(const UWorld* World, const FVector& Location);
	/** Where to head for the Safe Zone from here (nearest zone, or straight east on the test map). */
	PIRATESVSVIKINGS_API FVector GetSafeZoneGoal(const UWorld* World, const FVector& From);
	/** True when the level uses the map kit (has a Border Line or Safe Zone). */
	PIRATESVSVIKINGS_API bool IsCustomMap(const UWorld* World);
	/** NPC truck route from Road Points. False if there are none. */
	PIRATESVSVIKINGS_API bool GetRoadRoute(const UWorld* World, TArray<FVector>& OutRoute, int32& OutCheckpointIndex, FTransform& OutSpawn, ABRCheckpointGate*& OutGate);
	PIRATESVSVIKINGS_API void GetVillagerSpots(const UWorld* World, TArray<FVector>& OutSpots);
	PIRATESVSVIKINGS_API bool GetCatapultLanding(const UWorld* World, FVector& OutLocation);
	PIRATESVSVIKINGS_API ABRCatapult* FindCatapult(const UWorld* World);
}

/** Base for the box-shaped zones. Scale the actor (or set Box Extent) to cover the area. */
UCLASS(Abstract)
class PIRATESVSVIKINGS_API ABRZoneBase : public AActor
{
	GENERATED_BODY()

public:
	ABRZoneBase();
	bool ContainsPoint(const FVector& WorldLocation) const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BR|Zone") TObjectPtr<UBoxComponent> Area;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BR|Zone") TObjectPtr<UTextRenderComponent> Label;
	/** Show the zone's outline and name while playing (handy while building). */
	UPROPERTY(EditAnywhere, Category = "BR|Zone") bool bShowInGame = false;

	virtual void BeginPlay() override;
};

/** Runners who get in here are SAFE. */
UCLASS(meta = (DisplayName = "BR Safe Zone"))
class PIRATESVSVIKINGS_API ABRSafeZone : public ABRZoneBase
{
	GENERATED_BODY()

public:
	ABRSafeZone();
};

/**
 * River: the actor sits on the WATER SURFACE; the box hangs down below it.
 * Flow goes along the blue arrow. Deep (drowning) everywhere except the shallow edges.
 * Bridges work automatically: anything above the water surface isn't "in the river".
 */
UCLASS(meta = (DisplayName = "BR River"))
class PIRATESVSVIKINGS_API ABRRiverZone : public APVVLake
{
	GENERATED_BODY()

public:
	ABRRiverZone();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual bool ContainsPoint(const FVector& WorldLocation) const override;

	bool IsDeep(const FVector& WorldLocation) const;
	FVector GetFlowDirection() const;

	/** How wide the wade-able shallow strip is along both banks (cm). */
	UPROPERTY(EditAnywhere, Category = "BR|River") float ShallowEdge = 350.f;
	/** Turn off to have calm water (lake, pond). */
	UPROPERTY(EditAnywhere, Category = "BR|River") bool bHasCurrent = true;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BR|River") TObjectPtr<UBoxComponent> Area;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BR|River") TObjectPtr<UArrowComponent> Flow;

	bool LocalPoint(const FVector& WorldLocation, FVector& OutLocal) const;
};

/**
 * The Line. Runners are "across" once they pass it in the direction of the yellow arrow.
 * Also a barrier during the Prep phase (nobody crosses before the Line opens).
 */
UCLASS(meta = (DisplayName = "BR Border Line"))
class PIRATESVSVIKINGS_API ABRBorderLine : public AActor
{
	GENERATED_BODY()

public:
	ABRBorderLine();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	bool IsAcross(const FVector& WorldLocation) const;

	/** How long the painted line + Prep barrier is (cm). The rule itself goes on forever both ways. */
	UPROPERTY(EditAnywhere, Category = "BR|Line") float Length = 16000.f;
	UPROPERTY(EditAnywhere, Category = "BR|Line") bool bBlockDuringPrep = true;
	UPROPERTY(EditAnywhere, Category = "BR|Line") bool bShowStripe = true;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UArrowComponent> AcrossArrow;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Barrier;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Stripe;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Sign;

private:
	int32 AppliedState = -1;
};

/** Base for point markers: visible in the editor, invisible in game. */
UCLASS(Abstract)
class PIRATESVSVIKINGS_API ABRMarker : public AActor
{
	GENERATED_BODY()

public:
	ABRMarker();

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Pole;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Label;
	void Setup(const TCHAR* Text, const FColor& Color);
};

/** One stop on the NPC truck route. Trucks appear at Order 0 and drive 1, 2, 3... then disappear at the last one. */
UCLASS(meta = (DisplayName = "BR Road Point"))
class PIRATESVSVIKINGS_API ABRRoadPoint : public ABRMarker
{
	GENERATED_BODY()

public:
	ABRRoadPoint();
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(EditAnywhere, Category = "BR|Road") int32 Order = 0;
	/** Trucks stop here until a Warden opens the nearest Checkpoint Gate (or after a while). */
	UPROPERTY(EditAnywhere, Category = "BR|Road") bool bWaitForCheckpoint = false;
};

UCLASS(meta = (DisplayName = "BR Villager Spot"))
class PIRATESVSVIKINGS_API ABRVillagerSpot : public ABRMarker
{
	GENERATED_BODY()

public:
	ABRVillagerSpot();
};

UCLASS(meta = (DisplayName = "BR Catapult Landing"))
class PIRATESVSVIKINGS_API ABRCatapultLanding : public ABRMarker
{
	GENERATED_BODY()

public:
	ABRCatapultLanding();
};

/** A straight piece of the Wall. Runners with a ladder can lean it here and climb over. Wardens kick ladders down. */
UCLASS(meta = (DisplayName = "BR Wall Segment"))
class PIRATESVSVIKINGS_API ABRWallSegment : public AActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRWallSegment();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override { return 2.f; }
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

	UPROPERTY(EditAnywhere, Category = "BR|Wall") float Length = 2000.f;
	UPROPERTY(EditAnywhere, Category = "BR|Wall") float Height = 600.f;
	UPROPERTY(EditAnywhere, Category = "BR|Wall") FLinearColor Color = FLinearColor(0.42f, 0.38f, 0.33f);
	/** Optional real material (concrete, steel slats...). */
	UPROPERTY(EditAnywhere, Category = "BR|Wall") TObjectPtr<UMaterialInterface> Material;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> WallMesh;
};
