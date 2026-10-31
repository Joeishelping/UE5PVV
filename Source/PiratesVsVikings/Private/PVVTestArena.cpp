#include "PVVTestArena.h"
#include "PVVBoat.h"
#include "PVVDetentionZone.h"
#include "PVVPlayerStart.h"
#include "Components/BoxComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Shore tops sit a little above the water; the lakebed is waist deep.
	constexpr float ShoreTop = 60.f;
	constexpr float LakeBedTop = -70.f;
	constexpr float ShoreEdgeX = 10800.f;
	constexpr float ArenaHalfX = 15000.f;
	constexpr float ArenaHalfY = 9000.f;
}

APVVTestArena::APVVTestArena()
{
	bReplicates = true;
	bAlwaysRelevant = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	CubeMesh = Cube.Object;

	// Water only needs to fill the gap between the shores.
	Surface->SetRelativeScale3D(FVector(ShoreEdgeX * 2.f / 100.f, ArenaHalfY * 2.f / 100.f, 1.f));

	LakeBed = CreateDefaultSubobject<UBoxComponent>(TEXT("LakeBed"));
	LakeBed->SetupAttachment(Root);
	LakeBed->SetBoxExtent(FVector(ShoreEdgeX, ArenaHalfY, 10.f));
	LakeBed->SetRelativeLocation(FVector(0.f, 0.f, LakeBedTop - 10.f));
	LakeBed->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	LakeBed->SetCollisionObjectType(ECC_WorldStatic);
	LakeBed->SetCollisionResponseToAllChannels(ECR_Ignore);
	LakeBed->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	LakeBed->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);

	const float ShoreLength = ArenaHalfX - ShoreEdgeX;
	const float ShoreCenterX = ShoreEdgeX + ShoreLength * 0.5f;
	const FVector ShoreSize(ShoreLength, ArenaHalfY * 2.f, 400.f);
	AddBlock(TEXT("Sand_PirateShore"), FVector(-ShoreCenterX, 0.f, ShoreTop - 200.f), ShoreSize);
	AddBlock(TEXT("Sand_VikingShore"), FVector(ShoreCenterX, 0.f, ShoreTop - 200.f), ShoreSize);

	// Beach ramps from the lakebed up to the shore (~10 degrees).
	const FVector RampSize(760.f, ArenaHalfY * 2.f, 100.f);
	AddBlock(TEXT("Sand_PirateBeach"), FVector(-10460.f, 0.f, -54.f), RampSize, FRotator(-10.5f, 0.f, 0.f));
	AddBlock(TEXT("Sand_VikingBeach"), FVector(10460.f, 0.f, -54.f), RampSize, FRotator(10.5f, 0.f, 0.f));

	// Docks.
	const FVector DockSize(1500.f, 300.f, 40.f);
	AddBlock(TEXT("Dock_Pirate1"), FVector(-10050.f, -2500.f, 20.f), DockSize);
	AddBlock(TEXT("Dock_Pirate2"), FVector(-10050.f, 2500.f, 20.f), DockSize);
	AddBlock(TEXT("Dock_Viking1"), FVector(10050.f, -2500.f, 20.f), DockSize);
	AddBlock(TEXT("Dock_Viking2"), FVector(10050.f, 2500.f, 20.f), DockSize);

	// Arena walls (breakwaters).
	AddBlock(TEXT("Wall_North"), FVector(0.f, ArenaHalfY + 100.f, 150.f), FVector(ArenaHalfX * 2.f + 400.f, 200.f, 500.f));
	AddBlock(TEXT("Wall_South"), FVector(0.f, -ArenaHalfY - 100.f, 150.f), FVector(ArenaHalfX * 2.f + 400.f, 200.f, 500.f));
	AddBlock(TEXT("Wall_West"), FVector(-ArenaHalfX - 100.f, 0.f, 150.f), FVector(200.f, ArenaHalfY * 2.f, 500.f));
	AddBlock(TEXT("Wall_East"), FVector(ArenaHalfX + 100.f, 0.f, 150.f), FVector(200.f, ArenaHalfY * 2.f, 500.f));

	// The Learning Center: open-fronted hall facing the lake.
	const float WallZ = ShoreTop + 150.f;
	AddBlock(TEXT("Pirate_LCBack"), FVector(-13800.f, 0.f, WallZ), FVector(40.f, 1600.f, 300.f));
	AddBlock(TEXT("Pirate_LCLeft"), FVector(-13000.f, -800.f, WallZ), FVector(1600.f, 40.f, 300.f));
	AddBlock(TEXT("Pirate_LCRight"), FVector(-13000.f, 800.f, WallZ), FVector(1600.f, 40.f, 300.f));
	AddBlock(TEXT("Pirate_LCFrontL"), FVector(-12200.f, -500.f, WallZ), FVector(40.f, 600.f, 300.f));
	AddBlock(TEXT("Pirate_LCFrontR"), FVector(-12200.f, 500.f, WallZ), FVector(40.f, 600.f, 300.f));
	AddSign(TEXT("Sign_LC"), TEXT("THE LEARNING CENTER"), FVector(-12150.f, 0.f, 420.f), 0.f, 140.f, FColor(25, 220, 190));

	// Viking Processing Facility: a sealed cell (the jailbreak teleports people out).
	AddBlock(TEXT("Viking_CellFront"), FVector(12600.f, 4000.f, WallZ), FVector(40.f, 640.f, 300.f));
	AddBlock(TEXT("Viking_CellBack"), FVector(13200.f, 4000.f, WallZ), FVector(40.f, 640.f, 300.f));
	AddBlock(TEXT("Viking_CellLeft"), FVector(12900.f, 3700.f, WallZ), FVector(640.f, 40.f, 300.f));
	AddBlock(TEXT("Viking_CellRight"), FVector(12900.f, 4300.f, WallZ), FVector(640.f, 40.f, 300.f));
	AddBlock(TEXT("Viking_Office"), FVector(13300.f, -2000.f, ShoreTop + 200.f), FVector(800.f, 1200.f, 400.f));
	AddSign(TEXT("Sign_Facility"), TEXT("VIKING PROCESSING FACILITY"), FVector(12550.f, 4000.f, 520.f), 180.f, 110.f, FColor(255, 130, 25));
	AddSign(TEXT("Sign_DropOff"), TEXT("DROP PRISONERS HERE"), FVector(12000.f, 4000.f, 400.f), 180.f, 55.f, FColor(255, 170, 60));
	AddSign(TEXT("Sign_Door"), TEXT("CELL DOOR - Pirates hold E"), FVector(12450.f, 4000.f, 300.f), 180.f, 35.f, FColor(80, 230, 255));

	// Lighting, removed at BeginPlay if the level brings its own.
	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetRelativeRotation(FRotator(-40.f, 35.f, 0.f));

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = true;

	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(Root);
}

UStaticMeshComponent* APVVTestArena::AddBlock(const TCHAR* Name, const FVector& Center, const FVector& SizeCm, const FRotator& Rotation)
{
	UStaticMeshComponent* Block = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Block->SetupAttachment(Root);
	Block->SetStaticMesh(CubeMesh);
	Block->SetRelativeLocationAndRotation(Center, Rotation);
	Block->SetRelativeScale3D(SizeCm / 100.f);
	Block->SetCollisionProfileName(TEXT("BlockAll"));
	return Block;
}

UTextRenderComponent* APVVTestArena::AddSign(const TCHAR* Name, const FString& Text, const FVector& Location, float Yaw, float Size, const FColor& Color)
{
	UTextRenderComponent* Sign = CreateDefaultSubobject<UTextRenderComponent>(Name);
	Sign->SetupAttachment(Root);
	Sign->SetText(FText::FromString(Text));
	Sign->SetRelativeLocationAndRotation(Location, FRotator(0.f, Yaw, 0.f));
	Sign->SetWorldSize(Size);
	Sign->SetHorizontalAlignment(EHTA_Center);
	Sign->SetTextRenderColor(Color);
	Sign->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	return Sign;
}

void APVVTestArena::BeginPlay()
{
	Super::BeginPlay();

	// Colour the greybox by name prefix.
	TArray<UStaticMeshComponent*> Meshes;
	GetComponents<UStaticMeshComponent>(Meshes);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		const FString Name = Mesh->GetName();
		FLinearColor Color;
		if (Name.StartsWith(TEXT("Sand"))) Color = FLinearColor(0.55f, 0.48f, 0.3f);
		else if (Name.StartsWith(TEXT("Dock"))) Color = FLinearColor(0.3f, 0.17f, 0.07f);
		else if (Name.StartsWith(TEXT("Wall"))) Color = FLinearColor(0.25f, 0.25f, 0.27f);
		else if (Name.StartsWith(TEXT("Pirate"))) Color = FLinearColor(0.1f, 0.5f, 0.45f);
		else if (Name.StartsWith(TEXT("Viking"))) Color = FLinearColor(0.6f, 0.25f, 0.05f);
		else continue;

		if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
		}
	}

	// Don't double up on the level's own sun/sky.
	if (TActorIterator<ADirectionalLight>(GetWorld()))
	{
		Sun->DestroyComponent();
	}
	if (TActorIterator<ASkyLight>(GetWorld()))
	{
		SkyLight->DestroyComponent();
	}
	if (TActorIterator<ASkyAtmosphere>(GetWorld()))
	{
		Atmosphere->DestroyComponent();
	}
}

void APVVTestArena::SpawnGameplayActors(TSubclassOf<APVVBoat> BoatClass, int32 BoatsPerSide)
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FTransform& Arena = GetActorTransform();
	auto SpawnAt = [&](UClass* Class, const FVector& Local, float Yaw) -> AActor*
	{
		return World->SpawnActor<AActor>(Class, FTransform(FRotator(0.f, Yaw, 0.f), Arena.TransformPosition(Local)), Params);
	};

	const float StartZ = ShoreTop + 110.f;
	const FVector2D PirateStarts[] = { {-13300.f, -400.f}, {-13300.f, 400.f}, {-12700.f, -400.f}, {-12700.f, 400.f} };
	const FVector2D VikingStarts[] = { {12900.f, -3800.f}, {12900.f, -3200.f}, {12300.f, -3800.f}, {12300.f, -3200.f} };
	for (const FVector2D& Spot : PirateStarts)
	{
		if (APVVPlayerStart* Start = Cast<APVVPlayerStart>(SpawnAt(APVVPlayerStart::StaticClass(), FVector(Spot, StartZ), 0.f)))
		{
			Start->Faction = EPVVFaction::Pirates;
		}
	}
	for (const FVector2D& Spot : VikingStarts)
	{
		if (APVVPlayerStart* Start = Cast<APVVPlayerStart>(SpawnAt(APVVPlayerStart::StaticClass(), FVector(Spot, StartZ), 180.f)))
		{
			Start->Faction = EPVVFaction::Vikings;
		}
	}

	// Rotated 180 so the cell sits inland (+X) of the drop-off box.
	SpawnAt(APVVDetentionZone::StaticClass(), FVector(12000.f, 4000.f, ShoreTop + 150.f), 180.f);

	if (BoatClass)
	{
		for (int32 i = 0; i < BoatsPerSide; ++i)
		{
			const float Y = BoatsPerSide > 1 ? FMath::Lerp(-6000.f, 6000.f, i / float(BoatsPerSide - 1)) : 0.f;
			SpawnAt(BoatClass, FVector(-9000.f, Y, 40.f), 0.f);
			SpawnAt(BoatClass, FVector(9000.f, Y, 40.f), 180.f);
		}
	}
}
