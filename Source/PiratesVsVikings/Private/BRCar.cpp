#include "BRCar.h"
#include "BRCharacter.h"
#include "BRPlayerState.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "BRCar"

ABRCar::ABRCar()
{
	Hull->SetRelativeScale3D(FVector(4.2f, 2.f, 0.9f));
	MassKg = 900.f;
	BuoyancyMultiplier = 2.2f;	// suspension stiffness
	PontoonDepth = 60.f;		// suspension travel
	VerticalDamping = 3.f;
	ForwardAcceleration = 1500.f;
	ReverseAcceleration = 700.f;
	ForwardDrag = 0.75f;		// top speed ~= 2000 cm/s (72 km/h)
	LateralGrip = 5.f;
	MaxTurnRate = 1.8f;
	FullSteerSpeed = 400.f;
	RamKnockdownSpeed = 600.f;

	SeatOffsets = {
		FVector(-20.f, -45.f, 140.f),	// driver
		FVector(-20.f, 45.f, 140.f),
		FVector(-150.f, -45.f, 140.f),
		FVector(-150.f, 45.f, 140.f)
	};
	PontoonOffsets = {	// wheels
		FVector(170.f, 85.f, -45.f), FVector(170.f, -85.f, -45.f),
		FVector(-170.f, 85.f, -45.f), FVector(-170.f, -85.f, -45.f)
	};
}

void ABRCar::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRCar, Fuel);
}

void ABRCar::BeginPlay()
{
	Super::BeginPlay();
	if (bInfiniteFuel && HasAuthority())
	{
		Fuel = 1.f;
	}
	if (Hull->GetStaticMesh() && Hull->GetStaticMesh()->GetPathName().StartsWith(TEXT("/Engine/BasicShapes")))
	{
		if (UMaterialInstanceDynamic* MID = Hull->CreateDynamicMaterialInstance(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), PaintColor);
		}
	}
}

void ABRCar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority() && !bInfiniteFuel && GetDriver() && Fuel > 0.f)
	{
		Fuel = FMath::Max(0.f, Fuel - FMath::Abs(ThrottleInput) * FuelPerSecond * DeltaSeconds);
	}
}

bool ABRCar::GetSurfaceZ(const FVector& Point, float& OutZ) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BRCarWheel), false, this);
	const FVector Start = Point + FVector(0.f, 0.f, 120.f);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Start, Start - FVector(0.f, 0.f, 320.f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		OutZ = Hit.ImpactPoint.Z + RideHeight;
		return true;
	}
	return false;
}

float ABRCar::GetDriveScale() const
{
	return (bInfiniteFuel || Fuel > 0.f) ? 1.f : 0.f;
}

bool ABRCar::CanDrive(const APVVCharacter* Character) const
{
	if (!bRequiresKey)
	{
		return true;
	}
	const ABRPlayerState* PS = Character ? Cast<ABRPlayerState>(Character->GetPVVPlayerState()) : nullptr;
	return PS && PS->HasItem(EBRItem::CarKey);
}

bool ABRCar::WantsRefuel(const APVVCharacter* Interactor) const
{
	const ABRPlayerState* PS = Interactor ? Cast<ABRPlayerState>(Interactor->GetPVVPlayerState()) : nullptr;
	return !bInfiniteFuel && Fuel < 0.95f && PS && PS->HasItem(EBRItem::GasCan) && !Interactor->GetCurrentBoat();
}

bool ABRCar::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	if (WantsRefuel(Interactor) && Interactor->GetCondition() == EPVVCondition::Normal && !Interactor->GetCarriedCharacter())
	{
		return true;
	}
	return Super::CanInteract_Implementation(Interactor);
}

float ABRCar::GetInteractDuration_Implementation(APVVCharacter* Interactor) const
{
	return WantsRefuel(Interactor) ? 2.f : 0.f;
}

FText ABRCar::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	if (WantsRefuel(Interactor))
	{
		return LOCTEXT("Refuel", "Pour in gas (hold)");
	}
	if (!Interactor->GetCarriedCharacter() && Occupants.Num() > 0 && !Occupants[0])
	{
		if (!CanDrive(Interactor))
		{
			return LOCTEXT("NoKey", "Get in (no key - passenger)");
		}
		if (GetDriveScale() <= 0.f)
		{
			return LOCTEXT("NoGas", "Drive (TANK EMPTY - buy gas)");
		}
	}
	return Super::GetInteractPrompt_Implementation(Interactor);
}

void ABRCar::OnInteract_Implementation(APVVCharacter* Interactor)
{
	if (WantsRefuel(Interactor))
	{
		if (ABRPlayerState* PS = Cast<ABRPlayerState>(Interactor->GetPVVPlayerState()); PS && PS->RemoveItem(EBRItem::GasCan))
		{
			Fuel = FMath::Min(1.f, Fuel + 0.5f);
		}
		return;
	}
	Super::OnInteract_Implementation(Interactor);
}

#undef LOCTEXT_NAMESPACE
