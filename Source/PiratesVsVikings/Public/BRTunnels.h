#pragma once

#include "CoreMinimal.h"
#include "BRWorldActors.h"
#include "BRTunnels.generated.h"

/**
 * A REAL tunnel. Nothing teleports: you walk down a dug ramp, along an underground corridor, and up a ramp on the far side.
 *
 * Runners: hold E at the TUNNEL spot with a shovel to break ground. A pit with a ramp opens; walk down it. At the dirt face
 * at the end of the corridor, hold E to dig the next section. The corridor really grows section by section, each one lit.
 * When it reaches past the Wall, the last dig breaks through into an exit ramp on the USA side, and you walk out.
 *
 * Border Patrol do the opposite: only once a tunnel is COMPLETE, they go down into it and fill it back in with dirt, one
 * section at a time, starting at the Mexican end and backing out towards the USA exit. The last fill seals the exit.
 * Anyone in a section that gets filled is shoved out of the dirt and knocked down.
 *
 * The arena cuts the holes for the two ramps into its sand floor (see GetHoleRects); lids cover them while they're closed.
 */
UCLASS(meta = (DisplayName = "BR Dig Tunnel"))
class PIRATESVSVIKINGS_API ABRDigTunnel : public ABRGreyboxActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRDigTunnel();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

	/** Floor depth of the corridor under the ground (cm). Deep enough that the corridor runs below the map's sand floor. */
	static constexpr float Depth = 720.f;
	/** Horizontal length of each ramp (about a 38 degree slope - walkable). */
	static constexpr float RampRun = 920.f;
	/** Half width of the hole the ramp needs in the ground (shaft walls included). */
	static constexpr float HoleHalfWidth = 210.f;
	/** Length of one dug section (cm). */
	static constexpr float SectionLength = 500.f;

	/** How far the corridor runs (cm, along the actor's forward) before the exit ramp. Snapped to whole sections. */
	UPROPERTY(EditAnywhere, Replicated, Category = "BR|Tunnel") float ExitDistance = 8000.f;
	/** Seconds of holding E to dig one section, and to break ground at the start. */
	UPROPERTY(EditAnywhere, Category = "BR|Tunnel") float DigSeconds = 3.f;
	UPROPERTY(EditAnywhere, Category = "BR|Tunnel") float BreakGroundSeconds = 4.f;
	/** Seconds of holding E for Border Patrol to fill one section back in. */
	UPROPERTY(EditAnywhere, Category = "BR|Tunnel") float FillSeconds = 2.5f;
	/** Once the corridor is this long, seismic sensors hear the digging (the river is in the way). */
	UPROPERTY(EditAnywhere, Category = "BR|Tunnel") float DetectAfter = 2200.f;

	/** Snap a corridor length to whole sections (what the arena and the tunnel both use). */
	static float SnapLength(float Raw) { return FMath::Max(1, FMath::CeilToInt(Raw / SectionLength - 0.01f)) * SectionLength; }
	/** The two holes (start pit, exit pit) in the tunnel's local space: min/max X and Y. The arena leaves these out of its floor. */
	static void GetHoleRects(float InExitDistance, FBox2D& OutStart, FBox2D& OutExit);

	/** Is this spot down inside any tunnel? (hidden from Border Patrol, cameras, drones) */
	static bool IsUnderground(const UWorld* World, const FVector& Location);

protected:
	/** Sections dug (0 = not started). */
	UPROPERTY(ReplicatedUsing = OnRep_Sections) int32 Sections = 0;
	/** Border Patrol backfill, counted from the Mexican end: 0 = none, 1 = start pit filled, 2 = +first section, ...
	 *  Sections + 1 = everything but the exit pit. Filling the exit pit resets the tunnel. */
	UPROPERTY(ReplicatedUsing = OnRep_Sections) int32 Filled = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Sections) bool bDetected = false;

	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Built;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ExitPieces;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> StartLid;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> ExitLid;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> ExitPile;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> DirtMarker;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Face;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Backfill;

	UFUNCTION() void OnRep_Sections();
	float Length() const { return Sections * SectionLength; }
	bool IsComplete() const { return Sections > 0 && Length() >= ExitDistance - 1.f; }
	/** Far (USA-side) end X of backfill unit U: 0 = start pit, 1..Sections = corridor sections, Sections+1 = exit pit. */
	float UnitEndX(int32 U) const;
	float UnitStartX(int32 U) const;

	enum class EJob : uint8 { None, BreakGround, Dig, Fill };
	EJob JobFor(const APVVCharacter* C) const;
	void FillNext(APVVCharacter* By);
	void ShoveOutOf(int32 Unit, APVVCharacter* Except);
	void SetPiece(UStaticMeshComponent* M, bool bOn);
};
