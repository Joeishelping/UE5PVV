#pragma once

#include "CoreMinimal.h"
#include "BRWorldActors.h"
#include "BRDoors.generated.h"

/**
 * A door you just walk through: it swings open away from whoever pushes into it and swings shut again behind them
 * (no button). Saloon style = two short half-doors that flap both ways and wobble closed.
 * Every machine swings it from where the people are, so it never blocks anyone or gets out of sync.
 */
UCLASS(meta = (DisplayName = "BR Swing Door"))
class PIRATESVSVIKINGS_API ABRSwingDoor : public ABRGreyboxActor
{
	GENERATED_BODY()

public:
	ABRSwingDoor();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Doorway width (cm). The door faces along the actor's X (people walk through along X). */
	UPROPERTY(EditAnywhere, Category = "BR|Door") float DoorWidth = 300.f;
	UPROPERTY(EditAnywhere, Category = "BR|Door") float DoorHeight = 260.f;
	/** Two short half-doors that swing both ways (cantina). Off = one full door (shops). */
	UPROPERTY(EditAnywhere, Category = "BR|Door") bool bSaloon = false;
	UPROPERTY(EditAnywhere, Category = "BR|Door") float MaxAngle = 95.f;
	UPROPERTY(EditAnywhere, Category = "BR|Door") FLinearColor DoorColor = FLinearColor(0.35f, 0.22f, 0.12f);

private:
	UPROPERTY() TObjectPtr<USceneComponent> HingeA;
	UPROPERTY() TObjectPtr<USceneComponent> HingeB;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> PanelA;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> PanelB;
	float Angle = 0.f;
	float AngleVel = 0.f;
	void Layout();
};
