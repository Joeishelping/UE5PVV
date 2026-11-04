// Walk-through swing doors (shops) and flapping saloon doors (cantina).

#include "BRDoors.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"

ABRSwingDoor::ABRSwingDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	HingeA = CreateDefaultSubobject<USceneComponent>(TEXT("HingeA"));
	HingeA->SetupAttachment(Root);
	HingeB = CreateDefaultSubobject<USceneComponent>(TEXT("HingeB"));
	HingeB->SetupAttachment(Root);
	PanelA = AddMesh(TEXT("PanelA"), CubeMesh, FVector::ZeroVector, FVector(6.f, 100.f, 100.f), FLinearColor(0.35f, 0.22f, 0.12f), FRotator::ZeroRotator, false);
	PanelA->SetupAttachment(HingeA);
	PanelB = AddMesh(TEXT("PanelB"), CubeMesh, FVector::ZeroVector, FVector(6.f, 100.f, 100.f), FLinearColor(0.35f, 0.22f, 0.12f), FRotator::ZeroRotator, false);
	PanelB->SetupAttachment(HingeB);
	for (UStaticMeshComponent* Panel : { PanelA.Get(), PanelB.Get() })
	{
		// Seen and hit by things, but never blocks a person (they push it open instead).
		Panel->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Panel->SetCollisionResponseToAllChannels(ECR_Ignore);
		Panel->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}
}

void ABRSwingDoor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ComponentColors.Add(TEXT("PanelA"), DoorColor);
	ComponentColors.Add(TEXT("PanelB"), DoorColor);
	Layout();
}

void ABRSwingDoor::Layout()
{
	const float Half = DoorWidth * 0.5f;
	if (bSaloon)
	{
		// Two half-doors hinged at the outside edges, chest height, gap above and below.
		const float Leaf = Half - 4.f;
		const float H = FMath::Min(DoorHeight * 0.55f, 140.f);
		HingeA->SetRelativeLocation(FVector(0.f, -Half, 0.f));
		HingeB->SetRelativeLocation(FVector(0.f, Half, 0.f));
		PanelA->SetRelativeLocation(FVector(0.f, Leaf * 0.5f, 60.f + H * 0.5f));
		PanelB->SetRelativeLocation(FVector(0.f, -Leaf * 0.5f, 60.f + H * 0.5f));
		PanelA->SetRelativeScale3D(FVector(6.f, Leaf, H) / 100.f);
		PanelB->SetRelativeScale3D(FVector(6.f, Leaf, H) / 100.f);
		PanelB->SetVisibility(true);
	}
	else
	{
		// One full door hinged on one side.
		HingeA->SetRelativeLocation(FVector(0.f, -Half, 0.f));
		PanelA->SetRelativeLocation(FVector(0.f, Half - 2.f, DoorHeight * 0.5f));
		PanelA->SetRelativeScale3D(FVector(6.f, DoorWidth - 4.f, DoorHeight) / 100.f);
		PanelB->SetVisibility(false);
	}
}

void ABRSwingDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (DeltaSeconds <= 0.f)
	{
		return;
	}
	// Anyone in the doorway pushes it open, away from the side they came from.
	float Target = 0.f;
	const float Half = DoorWidth * 0.5f;
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		const FVector L = GetActorTransform().InverseTransformPositionNoScale(It->GetActorLocation());
		if (FMath::Abs(L.Y) > Half + 30.f || L.Z < -100.f || L.Z > DoorHeight + 100.f || FMath::Abs(L.X) > 130.f)
		{
			continue;
		}
		// Which way are they going through? (the side they're on, or the way they're moving when right in it)
		const FVector LocalVel = GetActorTransform().InverseTransformVectorNoScale(It->GetVelocity());
		const float Dir = FMath::Abs(L.X) < 40.f && FMath::Abs(LocalVel.X) > 30.f ? FMath::Sign(LocalVel.X) : -FMath::Sign(L.X == 0.f ? -1.f : L.X);
		const float Push = FMath::Clamp((130.f - FMath::Abs(L.X)) / 70.f, 0.f, 1.f);
		if (FMath::Abs(Dir * MaxAngle * Push) > FMath::Abs(Target))
		{
			Target = Dir * MaxAngle * Push;
		}
	}
	// A springy swing: fast open, swings back shut and wobbles (saloon doors flap more).
	const float Stiff = bSaloon ? 60.f : 30.f;
	const float Damp = bSaloon ? 4.f : 9.f;
	AngleVel += ((Target - Angle) * Stiff - AngleVel * Damp) * DeltaSeconds;
	Angle = FMath::Clamp(Angle + AngleVel * DeltaSeconds, -MaxAngle, MaxAngle);
	// (Angle > 0 = swung towards +X. Hinge A's leaf points +Y, hinge B's points -Y, so they turn opposite ways.)
	HingeA->SetRelativeRotation(FRotator(0.f, -Angle, 0.f));
	HingeB->SetRelativeRotation(FRotator(0.f, Angle, 0.f));
}
