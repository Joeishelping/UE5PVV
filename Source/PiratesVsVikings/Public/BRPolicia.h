#pragma once

// Mexican police (Policía) and the town jail (cárcel).
// They ONLY care about crimes against Mexico: hurting / stabbing / shooting / killing townspeople (or the Policía) in town.
// Anything you do to Border Patrol, or a Border Patrol wanted level, is not their business.
// Crimes raise your "Policía heat"; at 40+ they come for you, beat you down and lock you in the cárcel for a while (plus a fine).

#include "CoreMinimal.h"
#include "BRWorldActors.h"
#include "PVVInteractable.h"
#include "BRPolicia.generated.h"

UCLASS(meta = (DisplayName = "BR Mexican Jail"))
class PIRATESVSVIKINGS_API ABRMexJail : public ABRGreyboxActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRMexJail();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override { return 2.5f; }
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;
	/** Server: something taken off an arrested person goes on the evidence table. */
	void AddEvidence(EBRItem Item);
	/** Inside the cell / just outside the door. */
	FVector GetCellSpot() const;
	FVector GetReleaseSpot() const;
	bool IsInCell(const FVector& WorldPoint) const;
	static ABRMexJail* Find(const UWorld* World);

	static constexpr float CellHalfX = 300.f;
	static constexpr float CellHalfY = 250.f;
	static constexpr float CellHeight = 340.f;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Evidence) TArray<EBRItem> Evidence;
	UFUNCTION() void OnRep_Evidence();
	UPROPERTY() TObjectPtr<UTextRenderComponent> EvidenceLabel;
	FVector TablePos = FVector::ZeroVector;
	bool NearTable(const APVVCharacter* Who) const;
};

/** A house's front door lock: pick it (Lockpick, hold E) to rob the place. A crime against Mexico - if a cop sees it. */
UCLASS(meta = (DisplayName = "BR House Stash"))
class PIRATESVSVIKINGS_API ABRHouseStash : public ABRGreyboxActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRHouseStash();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

	/** Seconds to pick the lock, and before the house has something worth stealing again. */
	UPROPERTY(EditAnywhere, Category = "BR|Burglary") float PickSeconds = 4.f;
	UPROPERTY(EditAnywhere, Category = "BR|Burglary") float RestockSeconds = 240.f;

private:
	UPROPERTY(Replicated) float RestockAt = 0.f;
	bool IsStocked() const;
	static bool HasLockpick(const APVVCharacter* Who);
};
