#include "PVVLake.h"
#include "BRSettings.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"

APVVLake::APVVLake()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Surface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Surface"));
	Surface->SetupAttachment(Root);
	Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Surface->SetRelativeScale3D(FVector(400.f, 400.f, 1.f)); // 400m x 400m from the engine 1m plane

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		Surface->SetStaticMesh(PlaneMesh.Object);
	}
}

APVVLake* APVVLake::FindWaterAt(const UWorld* World, const FVector& WorldLocation)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<APVVLake> It(World); It; ++It)
	{
		if (It->ContainsPoint(WorldLocation))
		{
			return *It;
		}
	}
	return nullptr;
}

void APVVLake::BeginPlay()
{
	Super::BeginPlay();

	// Your water material (Project Settings -> Border Run -> Test Map Look -> Water), or a tinted placeholder.
	if (GetDefault<UBRSettings>()->ApplySurface(Surface, TEXT("Water"), TEXT("Surface")))
	{
		return;
	}
	if (UMaterialInstanceDynamic* MID = Surface->CreateDynamicMaterialInstance(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), WaterColor);
	}
}
