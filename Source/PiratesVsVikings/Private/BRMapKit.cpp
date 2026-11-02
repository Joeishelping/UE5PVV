#include "BRMapKit.h"
#include "BRDesertArena.h"
#include "BRGameState.h"
#include "BRPlayerState.h"
#include "BRWorldActors.h"
#include "PVVCharacter.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "BRMapKit"

namespace
{
	ABRDesertArena* BuiltInMap(const UWorld* World)
	{
		for (TActorIterator<ABRDesertArena> It(World); It; ++It)
		{
			if (It->bBuildTestMap)
			{
				return *It;
			}
		}
		return nullptr;
	}

	UStaticMesh* KitMesh(const TCHAR* Path)
	{
		return LoadObject<UStaticMesh>(nullptr, Path);
	}

	void KitPaint(UStaticMeshComponent* Mesh, const FLinearColor& Color)
	{
		if (UMaterialInstanceDynamic* MID = Mesh ? Mesh->CreateDynamicMaterialInstance(0) : nullptr)
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
		}
	}
}

// ===========================================================================================
// Rules

namespace BRMap
{
	bool IsInRiver(const UWorld* World, const FVector& Location)
	{
		if (!World)
		{
			return false;
		}
		for (TActorIterator<ABRRiverZone> It(World); It; ++It)
		{
			if (It->ContainsPoint(Location))
			{
				return true;
			}
		}
		const ABRDesertArena* Arena = BuiltInMap(World);
		return Arena && Arena->IsInRiver(Location);
	}

	bool IsDeepWater(const UWorld* World, const FVector& Location)
	{
		if (!World)
		{
			return false;
		}
		for (TActorIterator<ABRRiverZone> It(World); It; ++It)
		{
			if (It->IsDeep(Location))
			{
				return true;
			}
		}
		const ABRDesertArena* Arena = BuiltInMap(World);
		return Arena && Arena->IsDeepWater(Location);
	}

	FVector RiverFlow(const UWorld* World, const FVector& Location)
	{
		if (!World)
		{
			return FVector::ZeroVector;
		}
		for (TActorIterator<ABRRiverZone> It(World); It; ++It)
		{
			if (It->ContainsPoint(Location))
			{
				return It->bHasCurrent ? It->GetFlowDirection() : FVector::ZeroVector;
			}
		}
		const ABRDesertArena* Arena = BuiltInMap(World);
		return Arena && Arena->IsInRiver(Location) ? Arena->GetRiverFlowDirection() : FVector::ZeroVector;
	}

	bool HasCrossedLine(const UWorld* World, const FVector& Location)
	{
		if (!World)
		{
			return false;
		}
		bool bAnyLine = false;
		for (TActorIterator<ABRBorderLine> It(World); It; ++It)
		{
			bAnyLine = true;
			if (It->IsAcross(Location))
			{
				return true;
			}
		}
		if (bAnyLine)
		{
			return false;
		}
		const ABRDesertArena* Arena = BuiltInMap(World);
		return Arena && Location.X > Arena->GetLineX();
	}

	bool IsInSafeZone(const UWorld* World, const FVector& Location)
	{
		if (!World)
		{
			return false;
		}
		bool bAnyZone = false;
		for (TActorIterator<ABRSafeZone> It(World); It; ++It)
		{
			bAnyZone = true;
			if (It->ContainsPoint(Location))
			{
				return true;
			}
		}
		if (bAnyZone)
		{
			return false;
		}
		const ABRDesertArena* Arena = BuiltInMap(World);
		return Arena && Location.X > Arena->GetSafeX();
	}

	bool IsCustomMap(const UWorld* World)
	{
		// Any Map Kit piece or a Player Start you placed yourself = it's your map.
		return World && (TActorIterator<ABRBorderLine>(World) || TActorIterator<ABRSafeZone>(World) || TActorIterator<ABRRiverZone>(World)
			|| TActorIterator<ABRRoadPoint>(World) || TActorIterator<ABRVillagerSpot>(World) || TActorIterator<APlayerStart>(World));
	}

	bool GetRoadRoute(const UWorld* World, TArray<FVector>& OutRoute, int32& OutCheckpointIndex, FTransform& OutSpawn, ABRCheckpointGate*& OutGate)
	{
		OutRoute.Reset();
		OutCheckpointIndex = INDEX_NONE;
		OutGate = nullptr;
		if (!World)
		{
			return false;
		}
		TArray<ABRRoadPoint*> Points;
		for (TActorIterator<ABRRoadPoint> It(World); It; ++It)
		{
			Points.Add(*It);
		}
		if (Points.Num() < 2)
		{
			return false;
		}
		Points.Sort([](const ABRRoadPoint& A, const ABRRoadPoint& B) { return A.Order < B.Order; });
		for (int32 i = 0; i < Points.Num(); ++i)
		{
			OutRoute.Add(Points[i]->GetActorLocation() + FVector(0.f, 0.f, 90.f));
			if (Points[i]->bWaitForCheckpoint && OutCheckpointIndex == INDEX_NONE)
			{
				OutCheckpointIndex = i;
			}
		}
		const FVector First = OutRoute[0];
		const FVector Heading = (OutRoute[1] - OutRoute[0]).GetSafeNormal2D();
		OutSpawn = FTransform(Heading.IsNearlyZero() ? FRotator::ZeroRotator : Heading.Rotation(), First);
		if (OutCheckpointIndex != INDEX_NONE)
		{
			float Best = MAX_flt;
			for (TActorIterator<ABRCheckpointGate> It(World); It; ++It)
			{
				const float D = FVector::DistSquared(It->GetActorLocation(), OutRoute[OutCheckpointIndex]);
				if (D < Best)
				{
					Best = D;
					OutGate = *It;
				}
			}
		}
		return true;
	}

	void GetVillagerSpots(const UWorld* World, TArray<FVector>& OutSpots)
	{
		OutSpots.Reset();
		if (!World)
		{
			return;
		}
		for (TActorIterator<ABRVillagerSpot> It(World); It; ++It)
		{
			OutSpots.Add(It->GetActorLocation() + FVector(0.f, 0.f, 100.f));
		}
	}

	bool GetCatapultLanding(const UWorld* World, FVector& OutLocation)
	{
		if (!World)
		{
			return false;
		}
		for (TActorIterator<ABRCatapultLanding> It(World); It; ++It)
		{
			OutLocation = It->GetActorLocation() + FVector(0.f, 0.f, 50.f);
			return true;
		}
		return false;
	}

	ABRCatapult* FindCatapult(const UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		for (TActorIterator<ABRCatapult> It(World); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}
}

// ===========================================================================================
// Zones

ABRZoneBase::ABRZoneBase()
{
	Area = CreateDefaultSubobject<UBoxComponent>(TEXT("Area"));
	SetRootComponent(Area);
	Area->SetBoxExtent(FVector(1000.f, 1000.f, 500.f));
	Area->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Area->SetLineThickness(20.f);
	Area->SetHiddenInGame(true);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Area);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(200.f);
	Label->SetRelativeLocation(FVector(0.f, 0.f, 520.f));
	Label->SetHiddenInGame(true);
}

void ABRZoneBase::BeginPlay()
{
	Super::BeginPlay();
	Area->SetHiddenInGame(!bShowInGame);
	Label->SetHiddenInGame(!bShowInGame);
}

bool ABRZoneBase::ContainsPoint(const FVector& WorldLocation) const
{
	const FVector Local = Area->GetComponentTransform().InverseTransformPosition(WorldLocation);
	const FVector Extent = Area->GetUnscaledBoxExtent();
	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}

ABRSafeZone::ABRSafeZone()
{
	Area->ShapeColor = FColor(60, 255, 90);
	Label->SetText(LOCTEXT("SafeZone", "SAFE ZONE"));
	Label->SetTextRenderColor(FColor(60, 255, 90));
}

// ===========================================================================================
// River

ABRRiverZone::ABRRiverZone()
{
	// Root = the water surface. The box hangs below it.
	Area = CreateDefaultSubobject<UBoxComponent>(TEXT("Area"));
	Area->SetupAttachment(Root);
	Area->SetBoxExtent(FVector(2000.f, 600.f, 200.f));
	Area->SetRelativeLocation(FVector(0.f, 0.f, -200.f + 15.f));
	Area->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Area->ShapeColor = FColor(40, 140, 255);
	Area->SetLineThickness(15.f);
	Area->SetHiddenInGame(true);

	Flow = CreateDefaultSubobject<UArrowComponent>(TEXT("Flow"));
	Flow->SetupAttachment(Root);
	Flow->SetArrowColor(FLinearColor(0.2f, 0.5f, 1.f));
	Flow->ArrowSize = 8.f;
	Flow->SetRelativeLocation(FVector(0.f, 0.f, 30.f));

	WaterColor = FLinearColor(0.05f, 0.2f, 0.18f);
}

void ABRRiverZone::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Water plane matches the box (the engine plane is 1 m x 1 m).
	const FVector Extent = Area->GetUnscaledBoxExtent();
	Surface->SetRelativeScale3D(FVector(Extent.X * 2.f / 100.f, Extent.Y * 2.f / 100.f, 1.f));
	Area->SetRelativeLocation(FVector(0.f, 0.f, -Extent.Z + 15.f));
}

bool ABRRiverZone::LocalPoint(const FVector& WorldLocation, FVector& OutLocal) const
{
	OutLocal = Area->GetComponentTransform().InverseTransformPosition(WorldLocation);
	const FVector Extent = Area->GetUnscaledBoxExtent();
	return FMath::Abs(OutLocal.X) <= Extent.X && FMath::Abs(OutLocal.Y) <= Extent.Y && FMath::Abs(OutLocal.Z) <= Extent.Z;
}

bool ABRRiverZone::ContainsPoint(const FVector& WorldLocation) const
{
	FVector Local;
	return LocalPoint(WorldLocation, Local);
}

bool ABRRiverZone::IsDeep(const FVector& WorldLocation) const
{
	FVector Local;
	if (!LocalPoint(WorldLocation, Local))
	{
		return false;
	}
	const float HalfWidthCm = Area->GetUnscaledBoxExtent().Y * Area->GetComponentScale().Y;
	const float FromCentreCm = FMath::Abs(Local.Y) * Area->GetComponentScale().Y;
	return FromCentreCm < HalfWidthCm - ShallowEdge;
}

FVector ABRRiverZone::GetFlowDirection() const
{
	return Flow->GetForwardVector().GetSafeNormal2D();
}

// ===========================================================================================
// Border line

ABRBorderLine::ABRBorderLine()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;
	bReplicates = true;
	bAlwaysRelevant = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	AcrossArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("AcrossArrow"));
	AcrossArrow->SetupAttachment(Root);
	AcrossArrow->SetArrowColor(FLinearColor(1.f, 0.85f, 0.f));
	AcrossArrow->ArrowSize = 10.f;
	AcrossArrow->SetRelativeLocation(FVector(0.f, 0.f, 200.f));

	Barrier = CreateDefaultSubobject<UBoxComponent>(TEXT("Barrier"));
	Barrier->SetupAttachment(Root);
	Barrier->SetCollisionProfileName(TEXT("NoCollision"));

	Stripe = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Stripe"));
	Stripe->SetupAttachment(Root);
	Stripe->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Stripe->SetStaticMesh(Cube.Object);

	Sign = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Sign"));
	Sign->SetupAttachment(Root);
	Sign->SetText(LOCTEXT("TheLine", "THE LINE"));
	Sign->SetWorldSize(120.f);
	Sign->SetHorizontalAlignment(EHTA_Center);
	Sign->SetTextRenderColor(FColor(255, 220, 0));
	Sign->SetRelativeLocationAndRotation(FVector(-120.f, 0.f, 260.f), FRotator(0.f, 180.f, 0.f));
}

void ABRBorderLine::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Barrier->SetBoxExtent(FVector(50.f, Length * 0.5f, 1500.f));
	Barrier->SetRelativeLocation(FVector(0.f, 0.f, 1400.f));
	Stripe->SetRelativeLocation(FVector(0.f, 0.f, 2.f));
	Stripe->SetRelativeScale3D(FVector(2.f, Length / 100.f, 0.04f));
	Stripe->SetVisibility(bShowStripe);
}

void ABRBorderLine::BeginPlay()
{
	Super::BeginPlay();
	KitPaint(Stripe, FLinearColor(1.f, 0.85f, 0.f));
}

bool ABRBorderLine::IsAcross(const FVector& WorldLocation) const
{
	return FVector::DotProduct(WorldLocation - GetActorLocation(), GetActorForwardVector().GetSafeNormal2D()) > 0.f;
}

void ABRBorderLine::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>();
	const int32 Closed = (bBlockDuringPrep && GS && GS->Phase == EBRPhase::Prep) ? 1 : 0;
	if (Closed != AppliedState)
	{
		AppliedState = Closed;
		Barrier->SetCollisionProfileName(Closed ? TEXT("BlockAll") : TEXT("NoCollision"));
	}
}

// ===========================================================================================
// Markers

ABRMarker::ABRMarker()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Pole = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pole"));
	Pole->SetupAttachment(Root);
	Pole->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Pole->SetHiddenInGame(true);
	Pole->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
	Pole->SetRelativeScale3D(FVector(0.15f, 0.15f, 2.f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Pole->SetStaticMesh(Cylinder.Object);
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Root);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(60.f);
	Label->SetRelativeLocation(FVector(0.f, 0.f, 230.f));
	Label->SetHiddenInGame(true);
	SetActorHiddenInGame(true);
}

void ABRMarker::Setup(const TCHAR* Text, const FColor& Color)
{
	Label->SetText(FText::FromString(Text));
	Label->SetTextRenderColor(Color);
}

ABRRoadPoint::ABRRoadPoint()
{
	Setup(TEXT("ROAD 0"), FColor(255, 160, 40));
}

void ABRRoadPoint::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Label->SetText(FText::FromString(FString::Printf(TEXT("ROAD %d%s"), Order, bWaitForCheckpoint ? TEXT(" (WAIT)") : TEXT(""))));
}

ABRVillagerSpot::ABRVillagerSpot()
{
	Setup(TEXT("VILLAGERS"), FColor(255, 120, 200));
}

ABRCatapultLanding::ABRCatapultLanding()
{
	Setup(TEXT("CATAPULT LANDS HERE"), FColor(255, 80, 80));
}

// ===========================================================================================
// Wall segment

ABRWallSegment::ABRWallSegment()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	WallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallMesh"));
	WallMesh->SetupAttachment(Root);
	WallMesh->SetCollisionProfileName(TEXT("BlockAll"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	WallMesh->SetStaticMesh(Cube.Object);
}

void ABRWallSegment::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Runs along the actor's Y (the red/green gizmo: green). Thickness 1 m.
	WallMesh->SetRelativeLocation(FVector(0.f, 0.f, Height * 0.5f));
	WallMesh->SetRelativeScale3D(FVector(1.f, Length / 100.f, Height / 100.f));
	if (Material)
	{
		WallMesh->SetMaterial(0, Material);
	}
}

void ABRWallSegment::BeginPlay()
{
	Super::BeginPlay();
	if (!Material)
	{
		KitPaint(WallMesh, Color);
	}
}

bool ABRWallSegment::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	const ABRPlayerState* PS = Interactor ? Cast<ABRPlayerState>(Interactor->GetPVVPlayerState()) : nullptr;
	const ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>();
	if (!Interactor || !PS || !GS || GS->Phase != EBRPhase::Invasion || Interactor->GetFaction() != BR::Runners
		|| Interactor->GetCondition() != EPVVCondition::Normal || Interactor->GetCurrentBoat() || !PS->HasItem(EBRItem::Ladder))
	{
		return false;
	}
	const FVector Local = GetActorTransform().InverseTransformPositionNoScale(Interactor->GetActorLocation());
	return FMath::Abs(Local.X) < 300.f && FMath::Abs(Local.Y) < Length * 0.5f;
}

FText ABRWallSegment::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	return LOCTEXT("PlaceLadder", "Lean your ladder on the wall (hold)");
}

void ABRWallSegment::OnInteract_Implementation(APVVCharacter* Interactor)
{
	ABRPlayerState* PS = Interactor ? Cast<ABRPlayerState>(Interactor->GetPVVPlayerState()) : nullptr;
	if (!HasAuthority() || !PS || !PS->RemoveItem(EBRItem::Ladder))
	{
		return;
	}
	// Ladder goes on the wall right where you're standing, facing across it.
	FVector Local = GetActorTransform().InverseTransformPositionNoScale(Interactor->GetActorLocation());
	Local.X = 0.f;
	Local.Z = 0.f;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	GetWorld()->SpawnActor<ABRLadder>(ABRLadder::StaticClass(), GetActorTransform().TransformPositionNoScale(Local), FRotator(0.f, GetActorRotation().Yaw, 0.f), Params);
}

#undef LOCTEXT_NAMESPACE
