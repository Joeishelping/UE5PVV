#pragma once

// Border Patrol downtime: things to do at the base during prep (and between catches) that earn government budget.
//  - Shooting range: pop-up targets. Shoot them (any mode) for budget. Bullseyes pay double.
//  - Sparring mat: hold E to call out a recruit for a Gang Beasts style fist fight. Knock him out = budget. Each win makes the next one tougher.
//  - Duty desks are normal BR Stations with "Border Patrol Job" ticked (they pay the budget instead of your pocket).
// Everything pays full during prep and half once the Line is open.

#include "CoreMinimal.h"
#include "BRWorldActors.h"
#include "PVVInteractable.h"
#include "BRPatrolDuty.generated.h"

class ABRCharacter;

/** A pop-up shooting range target. Shoot it while it's standing up. */
UCLASS(meta = (DisplayName = "BR Range Target"))
class PIRATESVSVIKINGS_API ABRRangeTarget : public ABRGreyboxActor
{
	GENERATED_BODY()

public:
	ABRRangeTarget();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** Server: a bullet hit this target at Point. */
	void OnShot(ABRCharacter* By, const FVector& Point);

	/** Budget for a hit / a bullseye (prep; half once the Line is open). */
	UPROPERTY(EditAnywhere, Category = "BR|Range") int32 HitBudget = 20;
	UPROPERTY(EditAnywhere, Category = "BR|Range") int32 BullseyeBudget = 45;
	/** Seconds it stays down after a hit (random between these). */
	UPROPERTY(EditAnywhere, Category = "BR|Range") FVector2D DownSeconds = FVector2D(1.5f, 4.f);

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Pivot;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Bull;
	UPROPERTY(Replicated) bool bUp = true;
	float UpAt = 0.f;
	float Angle = 0.f;
};

/** A sparring mat: Border Patrol hold E to fight a recruit. Knock him out for budget. */
UCLASS(meta = (DisplayName = "BR Sparring Mat"))
class PIRATESVSVIKINGS_API ABRSparringMat : public ABRGreyboxActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRSparringMat();
	virtual void Tick(float DeltaSeconds) override;

	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override { return 0.6f; }
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

	/** Budget for the first knockout; each win in a row adds StepBudget (up to MaxBudget). */
	UPROPERTY(EditAnywhere, Category = "BR|Sparring") int32 BaseBudget = 150;
	UPROPERTY(EditAnywhere, Category = "BR|Sparring") int32 StepBudget = 50;
	UPROPERTY(EditAnywhere, Category = "BR|Sparring") int32 MaxBudget = 400;
	/** Seconds a bout lasts before the recruit taps out (no pay). */
	UPROPERTY(EditAnywhere, Category = "BR|Sparring") float BoutSeconds = 90.f;

protected:
	TWeakObjectPtr<ABRCharacter> Recruit;
	TWeakObjectPtr<ABRCharacter> Fighter;
	float BoutStart = 0.f;
	float NextBout = 0.f;
	/** Wins in a row (the next recruit is tougher). Replicated so the prompt shows the right money. */
	UPROPERTY(Replicated) int32 Streak = 0;
	UPROPERTY(Replicated) bool bBusy = false;
	void EndBout(bool bWon, const FText& Why);
	int32 Reward() const;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
