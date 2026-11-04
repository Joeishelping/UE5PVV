#pragma once

#include "CoreMinimal.h"
#include "BRWorldActors.h"
#include "BRPolice.generated.h"

/**
 * Border Patrol security camera (buy it at the supply counter, select it and press F / use to place it where you stand).
 * Watches a cone in front of it: any Runner who has crossed the Line walking through it gets spotted (shows up for
 * Border Patrol) and sets off a BORDER ALARM. Runners can sneak up behind it and smash it (hold E).
 */
UCLASS(meta = (DisplayName = "BR Security Camera"))
class PIRATESVSVIKINGS_API ABRSurveillanceCamera : public ABRGreyboxActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRSurveillanceCamera();
	virtual void Tick(float DeltaSeconds) override;

	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override { return 2.f; }
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

	/** How far it sees (cm) and how wide (degrees either side). */
	UPROPERTY(EditAnywhere, Category = "BR|Camera") float Range = 3200.f;
	UPROPERTY(EditAnywhere, Category = "BR|Camera") float HalfAngle = 38.f;

	/** Border Patrol press L to look through this camera (live view). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BR|Camera") TObjectPtr<class UCameraComponent> ViewCamera;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Head;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Lamp;
	float NextLook = 0.f;
	float LastAlarm = -100.f;
	float Sweep = 0.f;
};
