#pragma once

#include "CoreMinimal.h"
#include "BRWorldActors.h"
#include "BRTunnels.generated.h"

class UPointLightComponent;

/**
 * A REAL tunnel. Nothing teleports: you walk down a ramp, along an underground corridor, and up a ramp on the far side.
 * The corridor is split into sections (5 m each) between the two pits.
 *
 * Runners (need a Shovel - it wears out): break ground at the TUNNEL sign (Mexican side), walk down, dig section after
 * section towards the USA side. If they get all the way, the last dig breaks into the exit pit.
 *
 * Border Patrol: the seismic sensors tell them it's coming. They break ground at the exit pit (USA side) and dig their own
 * section after section towards the Mexicans - to meet them in the middle. Once the two meet (or the Runners get all the way
 * through) the tunnel is COMPLETE, and only then can Border Patrol fill it: standing inside, facing the next open section,
 * hold E ("FILL WITH DIRT"). They can fill in either direction from anywhere - from the middle out to the USA end and back
 * to the Mexican entrance. Filling an end section seals that pit. When every section is filled, the tunnel is gone.
 * Runners with a shovel can dig a filled section back out.
 *
 * The arena cuts the holes for the two pits in its sand floor (see GetHoleRects); lids cover them while they're closed.
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
	/** Length of one section (cm). */
	static constexpr float SectionLength = 500.f;
	/** Most sections a tunnel can have. */
	static constexpr int32 MaxSections = 30;

	/** How far the corridor runs (cm, along the actor's forward) before the exit pit. Snapped to whole sections. */
	UPROPERTY(EditAnywhere, Replicated, Category = "BR|Tunnel") float ExitDistance = 8000.f;
	/** Seconds of holding E to dig one section, and to break ground. */
	UPROPERTY(EditAnywhere, Category = "BR|Tunnel") float DigSeconds = 3.f;
	UPROPERTY(EditAnywhere, Category = "BR|Tunnel") float BreakGroundSeconds = 4.f;
	/** Seconds of holding E for Border Patrol to fill one section with dirt. */
	UPROPERTY(EditAnywhere, Category = "BR|Tunnel") float FillSeconds = 2.f;
	/** Once the Runners' corridor is this long, seismic sensors hear the digging (the river is in the way). */
	UPROPERTY(EditAnywhere, Category = "BR|Tunnel") float DetectAfter = 2200.f;

	/** Snap a corridor length to whole sections (what the arena and the tunnel both use). */
	static float SnapLength(float Raw) { return FMath::Clamp(FMath::CeilToInt(Raw / SectionLength - 0.01f), 1, MaxSections) * SectionLength; }
	/** The two holes (start pit, exit pit) in the tunnel's local space. The arena leaves these out of its floor. */
	static void GetHoleRects(float InExitDistance, FBox2D& OutStart, FBox2D& OutExit);

	/** Is this spot down inside any tunnel? (hidden from Border Patrol, cameras, drones) */
	static bool IsUnderground(const UWorld* World, const FVector& Location);

protected:
	/** Sections dug by the Runners (from the Mexican end) and by Border Patrol (from the USA end). */
	UPROPERTY(ReplicatedUsing = OnRep_Tunnel) int32 MexDug = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Tunnel) int32 BPDug = 0;
	/** Bit i = section i is filled with dirt. */
	UPROPERTY(ReplicatedUsing = OnRep_Tunnel) int32 FilledMask = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Tunnel) bool bDetected = false;

	/** Per section: floor, ceiling, wall, wall, dirt fill (5 each). Null until dug. */
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> SectionMeshes;
	UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> SectionLamps;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ExitPieces;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> StartLid;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> ExitLid;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> ExitPile;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> DirtMarker;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> MexFace;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> BPFace;
	UPROPERTY() TObjectPtr<UTextRenderComponent> ExitSign;

	UFUNCTION() void OnRep_Tunnel();

	int32 NumSections() const { return FMath::Clamp(FMath::RoundToInt(ExitDistance / SectionLength), 1, MaxSections); }
	bool IsStarted() const { return MexDug > 0 || BPDug > 0; }
	/** The two ends have met (or one side dug all the way): it's a complete tunnel. */
	bool IsConnected() const { return MexDug + BPDug >= NumSections(); }
	bool IsDug(int32 i) const { return i >= 0 && i < NumSections() && (i < MexDug || i >= NumSections() - BPDug); }
	bool IsFilled(int32 i) const { return i >= 0 && i < 32 && (FilledMask & (1 << i)) != 0; }
	bool IsOpen(int32 i) const { return IsDug(i) && !IsFilled(i); }
	bool AllFilled() const;

	enum class EJob : uint8 { None, BreakGround, Dig, DigOut, BPBreakGround, BPDig, Fill };
	/** What this person can do here, and (for Fill / DigOut) which section. */
	EJob JobFor(const APVVCharacter* C, int32* OutSection = nullptr) const;
	/** Which section a local X is in: -1 = start pit, NumSections() = exit pit. */
	int32 SectionAt(float LocalX) const;
	bool IsInside(const FVector& Local) const;
	void FillSection(int32 Section, int32 FromSide, APVVCharacter* By);
	void ShoveOutOf(int32 Section, int32 Dir, APVVCharacter* Except);
	void ResetTunnel();
	void SetPiece(UStaticMeshComponent* M, bool bOn);
	void OnRunnerDug(APVVCharacter* Digger);
};
