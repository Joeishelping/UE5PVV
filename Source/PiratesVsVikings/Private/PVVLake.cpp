#include "PVVLake.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

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

void APVVLake::BeginPlay()
{
	Super::BeginPlay();

	// Tint the engine basic-shape material so the placeholder reads as water.
	if (UMaterialInstanceDynamic* MID = Surface->CreateDynamicMaterialInstance(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), WaterColor);
	}
}
