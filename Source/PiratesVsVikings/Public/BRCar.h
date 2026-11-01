#pragma once

#include "CoreMinimal.h"
#include "PVVBoat.h"
#include "BRCar.generated.h"

/**
 * Arcade hover-car: reuses the boat's physics, but "floats" on the ground (line traces) instead of water.
 * Runner car: needs the Car Key to drive and starts with an empty tank (buy a Gas Can).
 * Warden cars: no key, infinite fuel.
 */
UCLASS()
class PIRATESVSVIKINGS_API ABRCar : public APVVBoat
{
	GENERATED_BODY()

public:
	ABRCar();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual bool GetSurfaceZ(const FVector& Point, float& OutZ) const override;
	virtual float GetDriveScale() const override;
	virtual bool CanDrive(const APVVCharacter* Character) const override;

	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BR|Car")
	bool bRequiresKey = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BR|Car")
	bool bInfiniteFuel = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BR|Car")
	FLinearColor PaintColor = FLinearColor(0.15f, 0.3f, 0.7f);

	/** 0..1 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR|Car")
	float Fuel = 0.f;

	/** Fuel used per second at full throttle (about 60 s of driving per full tank). */
	UPROPERTY(EditAnywhere, Category = "BR|Car")
	float FuelPerSecond = 1.f / 60.f;

	UPROPERTY(EditAnywhere, Category = "BR|Car")
	float RideHeight = 45.f;

private:
	bool WantsRefuel(const APVVCharacter* Interactor) const;
};
