#pragma once

#include "CoreMinimal.h"
#include "BRWorldActors.h"
#include "BRTunnels.generated.h"

/**
 * A REAL tunnel you dig and walk through. Climb down at the start (Mexican side), then dig the dirt face ahead of you
 * with a shovel (hold E) one section at a time. Each section is an actual underground corridor with a lamp.
 * Once it reaches past the Wall a hole opens on the USA side and you climb out.
 * Border Patrol: once a tunnel gets past the river, seismic sensors ping the map and "fresh dirt" shows up above the
 * tunnel's end. Hold E there (or at the exit hole) to COLLAPSE it - everyone inside is buried (knocked out back at the start).
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

	/** How far the tunnel has to go (cm, along the actor's forward) to come out past the Wall. */
	UPROPERTY(EditAnywhere, Category = "BR|Tunnel") float ExitDistance = 9500.f;
	/** How deep the corridor is under the start (cm). */
	UPROPERTY(EditAnywhere, Category = "BR|Tunnel") float Depth = 1200.f;
	/** Length of one dug section (cm) and how long it takes (s). */
	UPROPERTY(EditAnywhere, Category = "BR|Tunnel") float SectionLength = 500.f;
	UPROPERTY(EditAnywhere, Category = "BR|Tunnel") float DigSeconds = 3.f;
	/** Sections past which seismic sensors pick it up (the river is in the way). */
	UPROPERTY(EditAnywhere, Category = "BR|Tunnel") float DetectAfter = 4500.f;

	/** Is this spot underground inside any tunnel? (hidden from cameras / drones) */
	static bool IsUnderground(const UWorld* World, const FVector& Location);

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Sections) int32 Sections = 0;
	UPROPERTY(Replicated) bool bDetected = false;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Built;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> ExitMound;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> DirtMarker;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Face;
	UFUNCTION() void OnRep_Sections();
	float Length() const { return Sections * SectionLength; }
	bool IsComplete() const { return Length() >= ExitDistance; }
	FVector CorridorPoint(float Along) const;
	enum class EJob : uint8 { None, Start, Enter, ClimbOutStart, Dig, ClimbOutEnd, EnterEnd, Collapse };
	EJob JobFor(const APVVCharacter* C) const;
	void Collapse(APVVCharacter* By);
};
