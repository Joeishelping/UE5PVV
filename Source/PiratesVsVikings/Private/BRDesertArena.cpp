#include "BRDesertArena.h"
#include "BRSettings.h"
#include "BRAudio.h"
#include "BRCar.h"
#include "BRCantina.h"
#include "PVVCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "BRDog.h"
#include "BRGameState.h"
#include "BRWorldActors.h"
#include "PVVDetentionZone.h"
#include "PVVPlayerStart.h"
#include "Components/AudioComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Components/BoxComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Sound/SoundBase.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FLinearColor ArenaSand(0.62f, 0.48f, 0.3f);
	const FLinearColor ArenaAdobe(0.7f, 0.5f, 0.35f);
	const FLinearColor ArenaConcrete(0.45f, 0.44f, 0.42f);
}

ABRDesertArena::ABRDesertArena()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	CubeMesh = Cube.Object;

	// River water: the lake surface, only between the banks.
	Surface->SetRelativeScale3D(FVector(RiverHalfWidth * 2.f / 100.f + 2.f, HalfY * 2.f / 100.f, 1.f));
	WaterColor = FLinearColor(0.05f, 0.2f, 0.18f);

	RiverBed = CreateDefaultSubobject<UBoxComponent>(TEXT("RiverBed"));
	RiverBed->SetupAttachment(Root);
	RiverBed->SetBoxExtent(FVector(RiverHalfWidth + 100.f, HalfY, 10.f));
	RiverBed->SetRelativeLocation(FVector(0.f, 0.f, RiverBedZ - 10.f));
	RiverBed->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	RiverBed->SetCollisionObjectType(ECC_WorldStatic);
	RiverBed->SetCollisionResponseToAllChannels(ECR_Ignore);
	RiverBed->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	RiverBed->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);

	// Desert floor on both sides, sloped banks down into the river.
	const float BankX = RiverHalfWidth + 800.f;
	const float FloorLen = HalfX - BankX;
	AddBlock(TEXT("Sand_West"), FVector(-(BankX + FloorLen * 0.5f), 0.f, Ground - 200.f), FVector(FloorLen, HalfY * 2.f, 400.f), ArenaSand);
	const float EastLen = EastX - BankX;
	AddBlock(TEXT("Sand_East"), FVector(BankX + EastLen * 0.5f, 0.f, Ground - 200.f), FVector(EastLen, HalfY * 2.f, 400.f), ArenaSand);
	// Banks: from the desert floor (+100) down to the river bed (-150) over 8 m.
	AddBlock(TEXT("Sand_WestBank"), FVector(-1915.f, 0.f, -73.f), FVector(840.f, HalfY * 2.f, 100.f), ArenaSand, FRotator(-17.4f, 0.f, 0.f));
	AddBlock(TEXT("Sand_EastBank"), FVector(1915.f, 0.f, -73.f), FVector(840.f, HalfY * 2.f, 100.f), ArenaSand, FRotator(17.4f, 0.f, 0.f));

	// The road: town -> bridge -> checkpoint -> far side.
	// Narrow street through town, then a wide two-lane road all the way to the safe zone.
	const float TownLen = TownEdgeX + HalfX;
	AddBlock(TEXT("Road_Town"), FVector(-HalfX + TownLen * 0.5f, RoadY, Ground + 2.f), FVector(TownLen, 700.f, 4.f), FLinearColor(0.12f, 0.12f, 0.13f), FRotator::ZeroRotator, false);
	const float RoadLen = EastX - TownEdgeX;
	AddBlock(TEXT("Road_Strip"), FVector(TownEdgeX + RoadLen * 0.5f, RoadY, Ground + 2.f), FVector(RoadLen, RoadHalfWidth * 2.f, 4.f), FLinearColor(0.12f, 0.12f, 0.13f), FRotator::ZeroRotator, false);
	AddBlock(TEXT("Paint_CenterLine"), FVector(TownEdgeX + RoadLen * 0.5f, RoadY, Ground + 5.f), FVector(RoadLen, 20.f, 2.f), FLinearColor(0.9f, 0.8f, 0.2f), FRotator::ZeroRotator, false);
	AddBlock(TEXT("Bridge_Deck"), FVector(0.f, RoadY, Ground - 50.f), FVector(BankX * 2.f + 200.f, RoadHalfWidth * 2.f + 100.f, 100.f), FLinearColor(0.4f, 0.4f, 0.42f));
	AddBlock(TEXT("Bridge_RailN"), FVector(0.f, RoadY + RoadHalfWidth + 60.f, Ground + 50.f), FVector(BankX * 2.f, 20.f, 100.f), FLinearColor(0.3f, 0.3f, 0.32f));
	AddBlock(TEXT("Bridge_RailS"), FVector(0.f, RoadY - RoadHalfWidth - 60.f, Ground + 50.f), FVector(BankX * 2.f, 20.f, 100.f), FLinearColor(0.3f, 0.3f, 0.32f));

	// Map edges.
	AddBlock(TEXT("Edge_N"), FVector((EastX - HalfX) * 0.5f, HalfY + 100.f, 300.f), FVector(HalfX + EastX + 400.f, 200.f, 1000.f), ArenaAdobe);
	AddBlock(TEXT("Edge_S"), FVector((EastX - HalfX) * 0.5f, -HalfY - 100.f, 300.f), FVector(HalfX + EastX + 400.f, 200.f, 1000.f), ArenaAdobe);
	AddBlock(TEXT("Edge_W"), FVector(-HalfX - 100.f, 0.f, 300.f), FVector(200.f, HalfY * 2.f, 1000.f), ArenaAdobe);
	AddBlock(TEXT("Edge_E"), FVector(EastX + 100.f, 0.f, 300.f), FVector(200.f, HalfY * 2.f, 1000.f), ArenaAdobe);

	// Town.
	AddHut(TEXT("ToolShop"), FVector(-11500.f, -3000.f, Ground), FVector(1400.f, 1000.f, 400.f), ArenaAdobe);
	AddSign(TEXT("Sign_ToolShop"), TEXT("TOOL SHOP & GAS"), FVector(-10780.f, -3000.f, Ground + 480.f), 0.f, 90.f, FColor(255, 210, 120));
	AddBlock(TEXT("Pump_1"), FVector(-10300.f, -4250.f, Ground + 90.f), FVector(60.f, 60.f, 180.f), FLinearColor(0.7f, 0.1f, 0.1f));
	AddBlock(TEXT("Pump_2"), FVector(-10300.f, -3750.f, Ground + 90.f), FVector(60.f, 60.f, 180.f), FLinearColor(0.7f, 0.1f, 0.1f));
	AddHut(TEXT("Taco"), FVector(-11500.f, 1800.f, Ground), FVector(800.f, 800.f, 300.f), FLinearColor(0.85f, 0.6f, 0.2f));
	AddSign(TEXT("Sign_Taco"), TEXT("TACOS"), FVector(-11080.f, 1800.f, Ground + 380.f), 0.f, 120.f, FColor(255, 180, 40));
	AddHut(TEXT("BlackMarket"), FVector(-14600.f, 5600.f, Ground), FVector(700.f, 600.f, 300.f), FLinearColor(0.12f, 0.1f, 0.1f));
	AddSign(TEXT("Sign_Black"), TEXT("BLACK MARKET (opens at night)"), FVector(-14230.f, 5600.f, Ground + 360.f), 0.f, 50.f, FColor(200, 40, 40));
	AddHut(TEXT("Marina"), FVector(-6400.f, -6300.f, Ground), FVector(600.f, 600.f, 300.f), FLinearColor(0.2f, 0.4f, 0.7f));
	AddSign(TEXT("Sign_Marina"), TEXT("MARINA (boats)"), FVector(-6080.f, -6300.f, Ground + 360.f), 0.f, 60.f, FColor(120, 190, 255));
	AddSign(TEXT("Sign_Scrap"), TEXT("SCRAPYARD"), FVector(-13800.f, -6200.f, Ground + 300.f), 0.f, 70.f, FColor(200, 200, 200));
	AddBlock(TEXT("Junk_1"), FVector(-14100.f, -6700.f, Ground + 60.f), FVector(300.f, 200.f, 120.f), FLinearColor(0.3f, 0.25f, 0.2f), FRotator(0.f, 20.f, 8.f));
	AddBlock(TEXT("Junk_2"), FVector(-13500.f, -7000.f, Ground + 80.f), FVector(250.f, 250.f, 160.f), FLinearColor(0.35f, 0.3f, 0.25f), FRotator(10.f, -15.f, 0.f));
	AddHut(TEXT("House1"), FVector(-8500.f, 4000.f, Ground), FVector(900.f, 900.f, 350.f), ArenaAdobe);
	AddHut(TEXT("House2"), FVector(-13500.f, 3000.f, Ground), FVector(800.f, 1000.f, 350.f), ArenaAdobe);
	AddSign(TEXT("Sign_Town"), TEXT("WELCOME TO DUSTY GULCH"), FVector(-15000.f, 0.f, Ground + 450.f), 0.f, 150.f, FColor(255, 230, 180));

	// Warden side.
	AddBlock(TEXT("Cell_Front"), FVector(6400.f, 4000.f, Ground + 150.f), FVector(40.f, 640.f, 300.f), ArenaConcrete);
	AddBlock(TEXT("Cell_Back"), FVector(5800.f, 4000.f, Ground + 150.f), FVector(40.f, 640.f, 300.f), ArenaConcrete);
	AddBlock(TEXT("Cell_Left"), FVector(6100.f, 3700.f, Ground + 150.f), FVector(640.f, 40.f, 300.f), ArenaConcrete);
	AddBlock(TEXT("Cell_Right"), FVector(6100.f, 4300.f, Ground + 150.f), FVector(640.f, 40.f, 300.f), ArenaConcrete);
	AddSign(TEXT("Sign_Pen"), TEXT("HOLDING PEN"), FVector(6450.f, 4000.f, Ground + 380.f), 180.f, 80.f, FColor(255, 140, 40));
	AddBlock(TEXT("Tower"), FVector(4400.f, -2400.f, Ground + 600.f), FVector(300.f, 300.f, 1200.f), ArenaConcrete);
	AddHut(TEXT("WardenHQ"), FVector(8500.f, 4000.f, Ground), FVector(1200.f, 1400.f, 400.f), FLinearColor(0.3f, 0.35f, 0.3f));
	AddSign(TEXT("Sign_HQ"), TEXT("WARDEN HQ"), FVector(7880.f, 4000.f, Ground + 480.f), 180.f, 100.f, FColor(255, 140, 40));

	// Safe zone.
	AddBlock(TEXT("Safe_Stripe"), FVector(SafeX + (EastX - SafeX) * 0.5f, 0.f, Ground + 2.f), FVector(EastX - SafeX, HalfY * 2.f, 4.f), FLinearColor(0.1f, 0.8f, 0.2f), FRotator::ZeroRotator, false);
	AddSign(TEXT("Sign_Safe"), TEXT("SAFE ZONE"), FVector(SafeX - 50.f, 0.f, Ground + 400.f), 180.f, 220.f, FColor(80, 255, 120));

	// --- Dressing: street lamps along the big road, a welcome arch, cacti out in the desert.
	{
		const FLinearColor Iron(0.12f, 0.12f, 0.13f), Glow(1.f, 0.85f, 0.45f), Cactus(0.22f, 0.5f, 0.22f), ArchColor(0.7f, 0.45f, 0.28f);
		int32 LampIndex = 0;
		for (float X = TownEdgeX + 500.f; X < EastX - 800.f; X += 2000.f)
		{
			if (FMath::Abs(X) < BankX + 300.f || FMath::Abs(X - WallX) < 500.f || FMath::Abs(X - FenceX) < 400.f || FMath::Abs(X - LineX) < 400.f)
			{
				continue; // bridge, Wall, fence, Line
			}
			for (const float Side : { -1.f, 1.f })
			{
				const float Y = RoadY + Side * (RoadHalfWidth + 90.f);
				AddBlock(*FString::Printf(TEXT("Lamp_Post%d"), LampIndex), FVector(X, Y, Ground + 250.f), FVector(18.f, 18.f, 500.f), Iron);
				AddBlock(*FString::Printf(TEXT("Lamp_Arm%d"), LampIndex), FVector(X, Y - Side * 70.f, Ground + 495.f), FVector(12.f, 150.f, 10.f), Iron, FRotator::ZeroRotator, false);
				AddBlock(*FString::Printf(TEXT("Lamp_Light%d"), LampIndex), FVector(X, Y - Side * 135.f, Ground + 480.f), FVector(40.f, 40.f, 20.f), Glow, FRotator::ZeroRotator, false);
				AddNightLight(*FString::Printf(TEXT("Lamp_Bulb%d"), LampIndex), FVector(X, Y - Side * 135.f, Ground + 450.f), 1.f, 2400.f);
				++LampIndex;
			}
		}
		// "Welcome to San Pedrito" arch where the town street meets the big road.
		AddBlock(TEXT("Arch_PillarN"), FVector(TownEdgeX + 150.f, RoadY + RoadHalfWidth + 80.f, Ground + 300.f), FVector(90.f, 90.f, 600.f), ArchColor);
		AddBlock(TEXT("Arch_PillarS"), FVector(TownEdgeX + 150.f, RoadY - RoadHalfWidth - 80.f, Ground + 300.f), FVector(90.f, 90.f, 600.f), ArchColor);
		AddBlock(TEXT("Arch_Beam"), FVector(TownEdgeX + 150.f, RoadY, Ground + 640.f), FVector(70.f, RoadHalfWidth * 2.f + 250.f, 90.f), ArchColor, FRotator::ZeroRotator, false);
		AddSign(TEXT("Sign_ArchEast"), TEXT("BIENVENIDOS A SAN PEDRITO"), FVector(TownEdgeX + 195.f, RoadY, Ground + 625.f), 0.f, 60.f, FColor(255, 230, 120));
		AddSign(TEXT("Sign_ArchWest"), TEXT("BUEN VIAJE - BORDER AHEAD"), FVector(TownEdgeX + 105.f, RoadY, Ground + 625.f), 180.f, 60.f, FColor(255, 230, 120));
		AddSign(TEXT("Sign_Checkpoint"), TEXT("CHECKPOINT 1 KM - HAVE YOUR PAPERS READY"), FVector(-BankX - 1600.f, RoadY - RoadHalfWidth - 150.f, Ground + 300.f), 180.f, 50.f, FColor(255, 255, 255));
		// Cacti out in the desert (same every time).
		FRandomStream Rand(1337);
		int32 CactusIndex = 0;
		for (int32 Try = 0; Try < 140 && CactusIndex < 45; ++Try)
		{
			const float X = Rand.FRandRange(BankX + 1500.f, EastX - 600.f);
			const float Y = Rand.FRandRange(-HalfY + 400.f, HalfY - 400.f);
			const bool bNearRoad = FMath::Abs(Y - RoadY) < RoadHalfWidth + 600.f;
			const bool bBorderPatrolBase = X < 10000.f && Y > -800.f && Y < 5200.f; // HQ, pen, supply counters
			const bool bNearWall = FMath::Abs(X - WallX) < 900.f;
			if (bNearRoad || bBorderPatrolBase || bNearWall || (X > SafeX - 500.f && X < SafeX + 500.f))
			{
				continue;
			}
			const float Tall = Rand.FRandRange(180.f, 380.f);
			AddBlock(*FString::Printf(TEXT("Cactus_Trunk%d"), CactusIndex), FVector(X, Y, Ground + Tall * 0.5f), FVector(55.f, 55.f, Tall), Cactus);
			AddBlock(*FString::Printf(TEXT("Cactus_ArmA%d"), CactusIndex), FVector(X + 50.f, Y, Ground + Tall * 0.55f), FVector(30.f, 30.f, Tall * 0.35f), Cactus, FRotator::ZeroRotator, false);
			AddBlock(*FString::Printf(TEXT("Cactus_ArmB%d"), CactusIndex), FVector(X - 50.f, Y, Ground + Tall * 0.7f), FVector(30.f, 30.f, Tall * 0.3f), Cactus, FRotator::ZeroRotator, false);
			++CactusIndex;
		}
		// Car lot (north of the plaza): ground, fence, flags and a sign. The cars for sale are spawned in BeginPlay.
		const FLinearColor LotGround(0.3f, 0.3f, 0.32f), Fence(0.85f, 0.85f, 0.85f);
		AddBlock(TEXT("Road_CarLot"), FVector(-7550.f, 3000.f, Ground + 3.f), FVector(2000.f, 900.f, 6.f), LotGround, FRotator::ZeroRotator, false);
		AddBlock(TEXT("Lot_FenceN"), FVector(-7550.f, 3470.f, Ground + 50.f), FVector(2000.f, 15.f, 100.f), Fence);
		AddBlock(TEXT("Lot_FenceW"), FVector(-8560.f, 3000.f, Ground + 50.f), FVector(15.f, 940.f, 100.f), Fence);
		AddBlock(TEXT("Lot_FenceE"), FVector(-6540.f, 3000.f, Ground + 50.f), FVector(15.f, 940.f, 100.f), Fence);
		for (int32 i = 0; i < 6; ++i)
		{
			const FLinearColor Flag = (i % 3 == 0) ? FLinearColor(0.1f, 0.55f, 0.25f) : (i % 3 == 1 ? FLinearColor(0.95f, 0.95f, 0.92f) : FLinearColor(0.8f, 0.1f, 0.12f));
			AddBlock(*FString::Printf(TEXT("Lot_Flag%d"), i), FVector(-8450.f + i * 380.f, 3470.f, Ground + 280.f), FVector(4.f, 120.f, 70.f), Flag, FRotator::ZeroRotator, false);
			AddBlock(*FString::Printf(TEXT("Lot_FlagPole%d"), i), FVector(-8450.f + i * 380.f, 3470.f, Ground + 160.f), FVector(8.f, 8.f, 320.f), Iron, FRotator::ZeroRotator, false);
		}
		AddSign(TEXT("Sign_CarLot"), TEXT("AUTOS EL GATO - CARROS USADOS\nwalk up to a car + hold E to buy"), FVector(-7550.f, 3480.f, Ground + 420.f), -90.f, 60.f, FColor(255, 220, 80));

		// More night lights: car lot, cantina front, the arch, checkpoint, bridge, Border Patrol base, pen, safe zone, town squares.
		const FVector Extra[] = {
			{-8100.f, 3000.f, Ground + 500.f}, {-7000.f, 3000.f, Ground + 500.f},				// car lot
			{-8000.f, -5000.f, Ground + 450.f},												// cantina porch
			{TownEdgeX + 150.f, RoadY, Ground + 560.f},										// arch
			{WallX - 400.f, RoadY - 500.f, Ground + 500.f}, {WallX - 400.f, RoadY + 500.f, Ground + 500.f},	// checkpoint
			{WallX + 400.f, RoadY, Ground + 500.f},
			{-1200.f, RoadY, Ground + 350.f}, {1200.f, RoadY, Ground + 350.f},				// bridge
			{6100.f, 4000.f, Ground + 450.f}, {8500.f, 3100.f, Ground + 450.f}, {7800.f, 800.f, Ground + 450.f},	// pen, HQ, supply
			{SafeX + 300.f, RoadY, Ground + 500.f}, {SafeX + 300.f, 3000.f, Ground + 500.f}, {SafeX + 300.f, -5000.f, Ground + 500.f},
			{-10450.f, 1000.f, Ground + 550.f}, {-12000.f, 1500.f, Ground + 550.f}, {-9000.f, 2200.f, Ground + 550.f},	// plaza, pit
			{-11000.f, -6200.f, Ground + 600.f}, {-10300.f, 3000.f, Ground + 500.f},			// lucha ring, mariachi
			{-13500.f, -1200.f, Ground + 500.f}, {-11500.f, -1200.f, Ground + 500.f}, {-9500.f, -1200.f, Ground + 500.f}, {-7500.f, -1200.f, Ground + 500.f},	// town street
		};
		int32 ExtraIndex = 0;
		for (const FVector& At : Extra)
		{
			AddNightLight(*FString::Printf(TEXT("Night_Light%d"), ExtraIndex++), At, 1.3f, 2800.f);
		}
		// Floodlights along both river banks and the Wall, so you can SEE people sneaking across at night.
		int32 FloodIndex = 0;
		for (const float LineXs : { -BankX - 250.f, BankX + 250.f, WallX - 350.f })
		{
			for (float Y = -HalfY + 900.f; Y <= HalfY - 900.f; Y += 1700.f)
			{
				if (FMath::Abs(Y - RoadY) < RoadHalfWidth + 300.f)
				{
					continue; // the road has its own lamps
				}
				AddBlock(*FString::Printf(TEXT("Flood_Pole%d"), FloodIndex), FVector(LineXs, Y, Ground + 350.f), FVector(20.f, 20.f, 700.f), Iron);
				AddBlock(*FString::Printf(TEXT("Flood_Head%d"), FloodIndex), FVector(LineXs, Y, Ground + 700.f), FVector(60.f, 90.f, 30.f), Glow, FRotator::ZeroRotator, false);
				AddNightLight(*FString::Printf(TEXT("Flood_Light%d"), FloodIndex), FVector(LineXs, Y, Ground + 680.f), 1.6f, 3200.f);
				++FloodIndex;
			}
		}
	}

	// Lighting (removed at BeginPlay if the level has its own). Day/night + weather are driven by the GameState.
	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);
	Moon = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Moon"));
	Moon->SetupAttachment(Root);
	Moon->SetMobility(EComponentMobility::Movable);
	Moon->SetLightColor(FLinearColor(0.6f, 0.7f, 1.f));
	Moon->SetIntensity(0.f);
	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = true;
	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(Root);
	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(Root);

	// Night + weather colour grading (unbound: affects the whole map).
	Grade = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Grade"));
	Grade->SetupAttachment(Root);
	Grade->bUnbound = true;
	Grade->Priority = 5.f;
	// Rain / sand particles (built-in fallback). Absolute positions, follow the local camera.
	RainDrops = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RainDrops"));
	RainDrops->SetupAttachment(Root);
	RainDrops->SetUsingAbsoluteLocation(true);
	RainDrops->SetUsingAbsoluteRotation(true);
	RainDrops->SetStaticMesh(CubeMesh);
	RainDrops->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RainDrops->SetCastShadow(false);
	DustBits = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("DustBits"));
	DustBits->SetupAttachment(Root);
	DustBits->SetUsingAbsoluteLocation(true);
	DustBits->SetUsingAbsoluteRotation(true);
	DustBits->SetStaticMesh(CubeMesh);
	DustBits->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DustBits->SetCastShadow(false);
	Footprints = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Footprints"));
	Footprints->SetupAttachment(Root);
	Footprints->SetUsingAbsoluteLocation(true);
	Footprints->SetUsingAbsoluteRotation(true);
	Footprints->SetUsingAbsoluteScale(true);
	Footprints->SetStaticMesh(CubeMesh);
	Footprints->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Footprints->SetCastShadow(false);
	CricketsAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("AmbienceCrickets"));
	TownAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("AmbienceTown"));
	DesertAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("AmbienceDesert"));
	for (UAudioComponent* Amb : { CricketsAudio.Get(), TownAudio.Get(), DesertAudio.Get() })
	{
		Amb->SetupAttachment(Root);
		Amb->bAutoActivate = false;
		Amb->bAllowSpatialization = false;
	}
	RainAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("RainAudio"));
	RainAudio->SetupAttachment(Root);
	RainAudio->bAutoActivate = false;
	RainAudio->bAllowSpatialization = false;
	WindAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("WindAudio"));
	WindAudio->SetupAttachment(Root);
	WindAudio->bAutoActivate = false;
	WindAudio->bAllowSpatialization = false;

	// Background music: every song in Content/Music/Ambient, played on each player's machine.
	AmbientMusic = CreateDefaultSubobject<UAudioComponent>(TEXT("AmbientMusic"));
	AmbientMusic->SetupAttachment(Root);
	AmbientMusic->bAutoActivate = false;
	AmbientMusic->bAllowSpatialization = false;
	AmbientMusic->bIsUISound = true;
}

UPointLightComponent* ABRDesertArena::AddNightLight(const TCHAR* Name, const FVector& Location, float Strength, float Radius)
{
	UPointLightComponent* Bulb = CreateDefaultSubobject<UPointLightComponent>(Name);
	Bulb->SetupAttachment(Root);
	Bulb->SetRelativeLocation(Location);
	Bulb->SetIntensityUnits(ELightUnits::Candelas);
	Bulb->SetIntensity(0.f);
	Bulb->SetAttenuationRadius(Radius);
	Bulb->SetLightColor(FLinearColor(1.f, 0.8f, 0.55f));
	Bulb->SetCastShadows(false);
	Bulb->SetMobility(EComponentMobility::Movable);
	Bulb->ComponentTags.Add(*FString::Printf(TEXT("Strength_%.2f"), Strength));
	StreetLights.Add(Bulb);
	return Bulb;
}

UStaticMeshComponent* ABRDesertArena::AddBlock(const TCHAR* Name, const FVector& Center, const FVector& SizeCm, const FLinearColor& Color, const FRotator& Rotation, bool bCollide)
{
	UStaticMeshComponent* Block = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Block->SetupAttachment(Root);
	Block->SetStaticMesh(CubeMesh);
	Block->SetRelativeLocationAndRotation(Center, Rotation);
	Block->SetRelativeScale3D(SizeCm / 100.f);
	if (bCollide)
	{
		Block->SetCollisionProfileName(TEXT("BlockAll"));
	}
	else
	{
		Block->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	BlockColors.Add(FName(Name), Color);
	return Block;
}

UTextRenderComponent* ABRDesertArena::AddSign(const TCHAR* Name, const FString& Text, const FVector& Location, float Yaw, float Size, const FColor& Color)
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

void ABRDesertArena::AddHut(const TCHAR* Prefix, const FVector& Center, const FVector& Size, const FLinearColor& Color)
{
	// Four walls, doorway in the +X (east-facing) wall.
	const float T = 40.f;
	const FVector Half = Size * 0.5f;
	const float Z = Center.Z + Half.Z;
	const float Door = 300.f;
	const float SideLen = (Size.Y - Door) * 0.5f;
	AddBlock(*FString::Printf(TEXT("%s_Back"), Prefix), FVector(Center.X - Half.X, Center.Y, Z), FVector(T, Size.Y, Size.Z), Color);
	AddBlock(*FString::Printf(TEXT("%s_Left"), Prefix), FVector(Center.X, Center.Y - Half.Y, Z), FVector(Size.X, T, Size.Z), Color);
	AddBlock(*FString::Printf(TEXT("%s_Right"), Prefix), FVector(Center.X, Center.Y + Half.Y, Z), FVector(Size.X, T, Size.Z), Color);
	AddBlock(*FString::Printf(TEXT("%s_FrontL"), Prefix), FVector(Center.X + Half.X, Center.Y - (Door + SideLen) * 0.5f, Z), FVector(T, SideLen, Size.Z), Color);
	AddBlock(*FString::Printf(TEXT("%s_FrontR"), Prefix), FVector(Center.X + Half.X, Center.Y + (Door + SideLen) * 0.5f, Z), FVector(T, SideLen, Size.Z), Color);
}

void ABRDesertArena::BeginPlay()
{
	Super::BeginPlay();

	TArray<UStaticMeshComponent*> Meshes;
	GetComponents<UStaticMeshComponent>(Meshes);
	if (!bBuildTestMap)
	{
		// Map Kit mode: throw away the built-in desert, keep the sky / weather / music.
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			Mesh->DestroyComponent();
		}
		Meshes.Reset();
		TArray<UTextRenderComponent*> Signs;
		GetComponents<UTextRenderComponent>(Signs);
		for (UTextRenderComponent* SignText : Signs)
		{
			SignText->DestroyComponent();
		}
		if (RiverBed)
		{
			RiverBed->DestroyComponent();
			RiverBed = nullptr;
		}
	}
	const UBRSettings* Look = GetDefault<UBRSettings>();
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (const TObjectPtr<UMaterialInterface>* Material = BlockMaterials.Find(Mesh->GetFName()); Material && *Material)
		{
			Mesh->SetMaterial(0, *Material);
		}
		else if (Look->ApplySurface(Mesh, TEXT("Map"), Mesh->GetName()))
		{
			// your texture from Project Settings -> Border Run -> Test Map Look
		}
		else if (const FLinearColor* Color = BlockColors.Find(Mesh->GetFName()))
		{
			if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(0))
			{
				MID->SetVectorParameterValue(TEXT("Color"), *Color);
			}
		}
	}

	// The level already has a sun? Use THAT one for day/night (so night really gets dark) instead of switching day/night off.
	if (TActorIterator<ADirectionalLight> LevelSun(GetWorld()); LevelSun)
	{
		if (UDirectionalLightComponent* Theirs = Cast<UDirectionalLightComponent>(LevelSun->GetLightComponent()))
		{
			Sun->DestroyComponent();
			Sun = Theirs;
			Sun->SetMobility(EComponentMobility::Movable);
			Sun->SetAtmosphereSunLight(true);
		}
		// Any other directional lights in the level would keep it bright at night.
		for (TActorIterator<ADirectionalLight> Other(GetWorld()); Other; ++Other)
		{
			if (*Other != *LevelSun && Other->GetLightComponent())
			{
				Other->GetLightComponent()->SetMobility(EComponentMobility::Movable);
				Other->GetLightComponent()->SetIntensity(0.f);
			}
		}
	}
	if (TActorIterator<ASkyLight> LevelSky(GetWorld()); LevelSky)
	{
		SkyLight->DestroyComponent();
		SkyLight = LevelSky->GetLightComponent();
		if (SkyLight)
		{
			SkyLight->SetMobility(EComponentMobility::Movable);
			SkyLight->bRealTimeCapture = true; // follows the sun (dark sky at night)
		}
	}
	if (TActorIterator<ASkyAtmosphere>(GetWorld()))
	{
		Atmosphere->DestroyComponent();
	}
	if (TActorIterator<AExponentialHeightFog> LevelFog(GetWorld()); LevelFog)
	{
		Fog->DestroyComponent();
		Fog = LevelFog->GetComponent(); // drive the level's own fog for sandstorms / rain
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		const UBRSettings* FXSettings = GetDefault<UBRSettings>();
		if (UNiagaraSystem* RainSystem = FXSettings->RainEffect.LoadSynchronous())
		{
			RainFX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, RainSystem, GetActorLocation(), FRotator::ZeroRotator, FVector(1.f), false, false);
		}
		if (UNiagaraSystem* SandSystem = FXSettings->SandstormEffect.LoadSynchronous())
		{
			SandFX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, SandSystem, GetActorLocation(), FRotator::ZeroRotator, FVector(1.f), false, false);
		}
		RainAudio->SetSound(FXSettings->RainLoopSound.LoadSynchronous());
		CricketsAudio->SetSound(FXSettings->NightCricketsLoop.LoadSynchronous());
		TownAudio->SetSound(FXSettings->TownChatterLoop.LoadSynchronous());
		DesertAudio->SetSound(FXSettings->DesertWindLoop.LoadSynchronous());
		WindAudio->SetSound(FXSettings->WindLoopSound.LoadSynchronous());
		if (UMaterialInstanceDynamic* RainMID = RainDrops->CreateDynamicMaterialInstance(0))
		{
			RainMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.65f, 0.75f, 0.9f));
		}
		if (UMaterialInstanceDynamic* PrintMID = Footprints->CreateDynamicMaterialInstance(0))
		{
			PrintMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.42f, 0.31f, 0.18f));
		}
		if (UMaterialInstanceDynamic* DustMID = DustBits->CreateDynamicMaterialInstance(0))
		{
			DustMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.75f, 0.58f, 0.36f));
		}
	}

	if (GetNetMode() != NM_DedicatedServer)
	{
		TArray<USoundBase*> Tracks;
		BRAudio::LoadSounds(TEXT("/Game/Music/Ambient"), Tracks);
		AmbientTracks.Reset();
		for (USoundBase* Track : Tracks) { AmbientTracks.Add(Track); }
		AmbientMusic->OnAudioFinished.AddDynamic(this, &ABRDesertArena::PlayNextAmbient);
		PlayNextAmbient();
	}
}

void ABRDesertArena::PlayNextAmbient()
{
	if (AmbientTracks.Num() == 0)
	{
		return;
	}
	AmbientIndex = (AmbientIndex + 1 + (AmbientTracks.Num() > 1 ? FMath::RandRange(0, AmbientTracks.Num() - 2) : 0)) % AmbientTracks.Num();
	AmbientMusic->SetSound(AmbientTracks[AmbientIndex]);
	AmbientMusic->SetVolumeMultiplier(AmbientVolume * FPVVPlayerOptions::MusicVolume);
	AmbientMusic->Play();
}

void ABRDesertArena::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>();
	if (!GS)
	{
		return;
	}

	// Smooth the once-per-second replicated clock.
	if (DisplayedTime < 0.f || FMath::Abs(GS->TimeOfDay - DisplayedTime) > 1.f)
	{
		DisplayedTime = GS->TimeOfDay;
	}
	else
	{
		DisplayedTime = FMath::FInterpTo(DisplayedTime, GS->TimeOfDay, DeltaSeconds, 2.f);
	}

	if (AmbientMusic && AmbientMusic->IsPlaying())
	{
		const APlayerController* PC = GetWorld()->GetFirstPlayerController();
		const ABRCar* Car = PC ? Cast<ABRCar>(PC->GetPawn()) : nullptr;
		float Target = AmbientVolume * FPVVPlayerOptions::MusicVolume;
		if (Car && Car->IsRadioOn())
		{
			Target *= 0.15f; // radio takes over
		}
		AmbientMusic->SetVolumeMultiplier(FMath::FInterpTo(AmbientMusic->VolumeMultiplier, Target, DeltaSeconds, 2.f));
	}

	const float WeatherDim = GS->Weather == EBRWeather::Clear ? 1.f : (GS->Weather == EBRWeather::Rain ? 0.4f : 0.5f);
	if (Sun && Moon)
	{
		const float Angle = (DisplayedTime - 6.f) / 24.f * 360.f;
		const float Elevation = FMath::Sin(FMath::DegreesToRadians(Angle));
		Sun->SetWorldRotation(FRotator(-Angle, 30.f, 0.f));
		Sun->SetIntensity(10.f * FMath::Clamp(Elevation * 2.f, 0.f, 1.f) * WeatherDim);
		Moon->SetWorldRotation(FRotator(-(Angle + 180.f), 210.f, 0.f));
		Moon->SetIntensity(1.8f * FMath::Clamp(-Elevation * 2.f, 0.f, 1.f)); // bright moonlight so night is playable
	}

	if (Fog)
	{
		const float TargetDensity = GS->Weather == EBRWeather::Clear ? 0.003f : (GS->Weather == EBRWeather::Rain ? 0.03f : 0.09f);
		FogDensity = FMath::FInterpTo(FogDensity, TargetDensity, DeltaSeconds, 0.5f);
		Fog->SetFogDensity(FogDensity);
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		TickSkyAndWeather(DeltaSeconds, GS);
	}
}

float ABRDesertArena::LightStrength(const UPointLightComponent* Bulb)
{
	for (const FName& Tag : Bulb->ComponentTags)
	{
		FString S = Tag.ToString();
		if (S.RemoveFromStart(TEXT("Strength_")))
		{
			return FCString::Atof(*S);
		}
	}
	return 1.f;
}

void ABRDesertArena::TickSkyAndWeather(float DeltaSeconds, const ABRGameState* GS)
{
	const float Angle = (DisplayedTime - 6.f) / 24.f * 360.f;
	const float Elevation = FMath::Sin(FMath::DegreesToRadians(Angle));
	const float Night = FMath::Clamp((0.12f - Elevation) / 0.32f, 0.f, 1.f); // 0 = day, 1 = full night
	RainAmount = FMath::FInterpConstantTo(RainAmount, GS->Weather == EBRWeather::Rain ? 1.f : 0.f, DeltaSeconds, 0.25f);
	SandAmount = FMath::FInterpConstantTo(SandAmount, GS->Weather == EBRWeather::Sandstorm ? 1.f : 0.f, DeltaSeconds, 0.25f);

	// Night really gets dark (auto-exposure would brighten it back up otherwise), and blue. Sandstorms go orange.
	if (Grade)
	{
		FPostProcessSettings& S = Grade->Settings;
		S.bOverride_AutoExposureBias = true;
		S.AutoExposureBias = -0.7f * Night - 0.4f * RainAmount - 0.3f * SandAmount; // dark enough to feel like night, bright enough to see
		S.bOverride_ColorSaturation = true;
		const float Sat = 1.f - 0.25f * Night - 0.3f * RainAmount;
		S.ColorSaturation = FVector4(Sat, Sat, Sat, 1.f);
		S.bOverride_SceneColorTint = true;
		FLinearColor Tint = FMath::Lerp(FLinearColor::White, FLinearColor(0.78f, 0.85f, 1.f), Night);
		Tint = FMath::Lerp(Tint, Tint * FLinearColor(1.f, 0.82f, 0.58f), SandAmount);
		Tint = FMath::Lerp(Tint, Tint * FLinearColor(0.85f, 0.9f, 1.f), RainAmount);
		S.SceneColorTint = Tint;
		S.bOverride_VignetteIntensity = true;
		S.VignetteIntensity = 0.4f + 0.2f * Night + 0.3f * SandAmount;
	}
	if (Fog)
	{
		const FLinearColor DayFog(0.75f, 0.82f, 0.95f), NightFog(0.08f, 0.1f, 0.18f), SandFog(0.85f, 0.55f, 0.25f);
		FLinearColor FogColor = FMath::Lerp(DayFog, NightFog, Night);
		FogColor = FMath::Lerp(FogColor, SandFog * (1.f - 0.8f * Night), SandAmount);
		Fog->SetFogInscatteringColor(FogColor);
	}
	// Street lamps on at dusk.
	const bool bWantLights = Night > 0.2f || SandAmount > 0.6f || RainAmount > 0.7f;
	if (bWantLights != bLightsOn)
	{
		bLightsOn = bWantLights;
		for (UPointLightComponent* Bulb : StreetLights)
		{
			Bulb->SetIntensity(bLightsOn ? StreetLightCandelas * LightStrength(Bulb) : 0.f);
		}
	}
	// A few old street lamps flicker and cut out now and then.
	if (bLightsOn)
	{
		const float T = GetWorld()->GetTimeSeconds();
		for (int32 i = 0; i < StreetLights.Num(); ++i)
		{
			if (i % 7 != 3 || !StreetLights[i]->GetName().StartsWith(TEXT("Lamp_")))
			{
				continue; // most lamps are fine (only a few old road lamps flicker)
			}
			const float Noise = FMath::PerlinNoise1D(T * 6.f + i * 17.3f);
			const bool bOut = Noise < -0.25f || FMath::PerlinNoise1D(T * 0.3f + i * 5.1f) < -0.4f; // stutters + longer outages
			const float Full = StreetLightCandelas * LightStrength(StreetLights[i]);
			StreetLights[i]->SetIntensity(bOut ? Full * 0.08f : Full * (0.85f + 0.15f * FMath::Sin(T * 40.f + i)));
		}
	}
	// Lightning in the rain: a white flash, thunder a moment later.
	const float Now = GetWorld()->GetTimeSeconds();
	if (RainAmount > 0.7f && Now >= NextLightning)
	{
		NextLightning = Now + FMath::FRandRange(7.f, 22.f);
		LightningFlash = 1.f;
		ThunderAt = Now + FMath::FRandRange(0.4f, 2.f);
	}
	if (ThunderAt > 0.f && Now >= ThunderAt)
	{
		ThunderAt = -1.f;
		if (USoundBase* Thunder = GetDefault<UBRSettings>()->ThunderSound.LoadSynchronous())
		{
			UGameplayStatics::PlaySound2D(this, Thunder, FMath::FRandRange(0.6f, 1.f));
		}
	}
	LightningFlash = FMath::Max(0.f, LightningFlash - DeltaSeconds * 4.f);
	if (Moon)
	{
		Moon->SetIntensity(1.8f * FMath::Clamp(-Elevation * 2.f, 0.f, 1.f) + 12.f * LightningFlash * (FMath::Fmod(Now * 23.f, 1.f) > 0.3f ? 1.f : 0.3f));
	}
	// Weather sounds.
	if (RainAudio && RainAudio->GetSound())
	{
		if (RainAmount > 0.01f && !RainAudio->IsPlaying()) RainAudio->Play();
		RainAudio->SetVolumeMultiplier(RainAmount);
	}
	if (WindAudio && WindAudio->GetSound())
	{
		if (SandAmount > 0.01f && !WindAudio->IsPlaying()) WindAudio->Play();
		WindAudio->SetVolumeMultiplier(SandAmount);
	}
	TickParticles(DeltaSeconds);
	TickFootprints();
	TickAmbience();
}

void ABRDesertArena::TickAmbience()
{
	const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Cam)
	{
		return;
	}
	const FVector Local = Cam->GetCameraLocation() - GetActorLocation();
	const float Angle = (DisplayedTime - 6.f) / 24.f * 360.f;
	const float Night = FMath::Clamp((0.12f - FMath::Sin(FMath::DegreesToRadians(Angle))) / 0.32f, 0.f, 1.f);
	const float InTown = 1.f - FMath::Clamp((FVector::Dist2D(Local, FVector(-10500.f, 0.f, 0.f)) - 4000.f) / 3000.f, 0.f, 1.f);
	const float Music = FPVVPlayerOptions::MusicVolume;
	auto Loop = [](UAudioComponent* A, float Volume)
	{
		if (!A || !A->GetSound())
		{
			return;
		}
		if (Volume > 0.02f && !A->IsPlaying())
		{
			A->Play();
		}
		A->SetVolumeMultiplier(FMath::Max(Volume, 0.001f));
	};
	Loop(CricketsAudio, Night * (1.f - RainAmount) * 0.7f * Music);
	Loop(TownAudio, InTown * (1.f - 0.6f * Night) * (1.f - 0.5f * RainAmount) * 0.6f * Music);
	Loop(DesertAudio, (1.f - InTown) * (1.f - SandAmount) * 0.4f * Music);
	// A dog barking somewhere far off at night.
	const float Now = GetWorld()->GetTimeSeconds();
	if (Night > 0.5f && Now >= NextBark)
	{
		NextBark = Now + FMath::FRandRange(15.f, 45.f);
		const TArray<TSoftObjectPtr<USoundBase>>& Barks = GetDefault<UBRSettings>()->DistantDogBarks;
		if (Barks.Num() > 0)
		{
			if (USoundBase* Bark = Barks[FMath::RandRange(0, Barks.Num() - 1)].LoadSynchronous())
			{
				const FVector Far = Cam->GetCameraLocation() + FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector() * FMath::FRandRange(2500.f, 5000.f);
				BRAudio::PlayAt(this, Bark, Far, 8000.f, FMath::FRandRange(0.5f, 0.9f));
			}
		}
	}
}

void ABRDesertArena::TickFootprints()
{
	if (!Footprints)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	constexpr int32 MaxPrints = 300;
	constexpr float Life = 22.f;
	bool bChanged = false;
	for (TActorIterator<APVVCharacter> It(GetWorld()); It; ++It)
	{
		APVVCharacter* C = *It;
		const UCharacterMovementComponent* Move = C->GetCharacterMovement();
		if (!Move || !Move->IsMovingOnGround() || C->GetVelocity().Size2D() < 120.f || C->GetCurrentBoat() || C->GetCarrier())
		{
			continue;
		}
		const FVector Feet = C->GetActorLocation() - FVector(0.f, 0.f, C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		FVector& Last = LastStep.FindOrAdd(C, Feet);
		if (FVector::Dist2D(Last, Feet) < 75.f)
		{
			continue;
		}
		Last = Feet;
		// Only in sand (not on the road, floors, the bridge...).
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BRFootprint), false, C);
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Feet + FVector(0.f, 0.f, 40.f), Feet - FVector(0.f, 0.f, 80.f), ECC_Visibility, Params)
			|| !Hit.GetComponent() || !Hit.GetComponent()->GetName().StartsWith(TEXT("Sand")))
		{
			continue;
		}
		bool& bLeft = StepLeft.FindOrAdd(C, false);
		bLeft = !bLeft;
		const FRotator Facing(0.f, C->GetVelocity().Rotation().Yaw, 0.f);
		const FVector Side = FRotationMatrix(Facing).GetUnitAxis(EAxis::Y) * (bLeft ? -11.f : 11.f);
		const FTransform T(Facing, Hit.ImpactPoint + Side + FVector(0.f, 0.f, 0.8f), FVector(0.16f, 0.07f, 0.01f));
		if (Prints.Num() < MaxPrints)
		{
			Prints.Add({ T, Now });
			Footprints->AddInstance(T, true);
		}
		else
		{
			Prints[NextPrint] = { T, Now };
			NextPrint = (NextPrint + 1) % MaxPrints;
		}
		bChanged = true;
	}
	// Fade old prints (shrink away).
	TArray<FTransform> All;
	All.Reserve(Prints.Num());
	for (const FPrint& P : Prints)
	{
		const float Age = Now - P.Time;
		const float Keep = FMath::Clamp((Life - Age) / 8.f, 0.f, 1.f);
		FTransform T = P.T;
		T.SetScale3D(P.T.GetScale3D() * FVector(Keep, Keep, 1.f));
		All.Add(T);
		bChanged |= Age > Life - 8.f && Age < Life + 1.f;
	}
	if (bChanged && All.Num() > 0)
	{
		Footprints->BatchUpdateInstancesTransforms(0, All, true, true, false);
	}
}

void ABRDesertArena::TickParticles(float DeltaSeconds)
{
	const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Cam)
	{
		return;
	}
	const FVector Eye = Cam->GetCameraLocation();
	// Your own Niagara effects (Project Settings -> Border Run -> Effects) follow the camera.
	if (RainFX)
	{
		RainFX->SetWorldLocation(Eye + FVector(0.f, 0.f, 600.f));
		if (RainAmount > 0.05f && !RainFX->IsActive()) RainFX->Activate();
		if (RainAmount <= 0.05f && RainFX->IsActive()) RainFX->Deactivate();
	}
	if (SandFX)
	{
		SandFX->SetWorldLocation(Eye);
		if (SandAmount > 0.05f && !SandFX->IsActive()) SandFX->Activate();
		if (SandAmount <= 0.05f && SandFX->IsActive()) SandFX->Deactivate();
	}
	// Built-in: falling rain streaks.
	if (!RainFX && RainDrops)
	{
		const int32 Want = FMath::RoundToInt(RainAmount * 450.f);
		while (DropPos.Num() < Want)
		{
			DropPos.Add(Eye + FVector(FMath::FRandRange(-1800.f, 1800.f), FMath::FRandRange(-1800.f, 1800.f), FMath::FRandRange(-300.f, 1200.f)));
			RainDrops->AddInstance(FTransform(DropPos.Last()), true);
		}
		while (DropPos.Num() > Want)
		{
			DropPos.Pop();
			RainDrops->RemoveInstance(RainDrops->GetInstanceCount() - 1);
		}
		TArray<FTransform> Moved;
		Moved.Reserve(DropPos.Num());
		for (FVector& P : DropPos)
		{
			P += FVector(120.f, 60.f, -1900.f) * DeltaSeconds;
			if (P.Z < Eye.Z - 500.f || FVector::Dist2D(P, Eye) > 2200.f)
			{
				P = Eye + FVector(FMath::FRandRange(-1800.f, 1800.f), FMath::FRandRange(-1800.f, 1800.f), FMath::FRandRange(600.f, 1300.f));
			}
			Moved.Add(FTransform(FRotator(-4.f, 0.f, 0.f), P, FVector(0.012f, 0.012f, 0.55f)));
		}
		if (Moved.Num() > 0)
		{
			RainDrops->BatchUpdateInstancesTransforms(0, Moved, true, true, true);
		}
	}
	// Built-in: sand whipping sideways.
	if (!SandFX && DustBits)
	{
		const int32 Want = FMath::RoundToInt(SandAmount * 350.f);
		while (DustPos.Num() < Want)
		{
			DustPos.Add(Eye + FVector(FMath::FRandRange(-1600.f, 1600.f), FMath::FRandRange(-1600.f, 1600.f), FMath::FRandRange(-250.f, 400.f)));
			DustBits->AddInstance(FTransform(DustPos.Last()), true);
		}
		while (DustPos.Num() > Want)
		{
			DustPos.Pop();
			DustBits->RemoveInstance(DustBits->GetInstanceCount() - 1);
		}
		TArray<FTransform> Moved;
		Moved.Reserve(DustPos.Num());
		const float T = GetWorld()->GetTimeSeconds();
		int32 i = 0;
		for (FVector& P : DustPos)
		{
			P += FVector(1500.f, 350.f * FMath::Sin(T * 2.f + i), 60.f * FMath::Sin(T * 3.f + i * 0.7f)) * DeltaSeconds;
			if (FVector::Dist2D(P, Eye) > 1900.f)
			{
				P = Eye + FVector(-1600.f, FMath::FRandRange(-1600.f, 1600.f), FMath::FRandRange(-250.f, 400.f));
			}
			const float Size = 0.02f + 0.03f * ((i * 37) % 10) / 10.f;
			Moved.Add(FTransform(FRotator(T * 90.f + i, i * 13.f, 0.f), P, FVector(Size * 2.f, Size, Size)));
			++i;
		}
		if (Moved.Num() > 0)
		{
			DustBits->BatchUpdateInstancesTransforms(0, Moved, true, true, true);
		}
	}
}

bool ABRDesertArena::IsInRiver(const FVector& WorldLocation) const
{
	const FVector Local = WorldLocation - GetActorLocation();
	return FMath::Abs(Local.X) < RiverHalfWidth && FMath::Abs(Local.Y) < HalfY && Local.Z < 15.f;
}

bool ABRDesertArena::IsDeepWater(const FVector& WorldLocation) const
{
	const FVector Local = WorldLocation - GetActorLocation();
	const bool bOnBridge = FMath::Abs(Local.Y - RoadY) < RoadHalfWidth + 70.f && Local.Z > Ground - 60.f;
	return !bOnBridge && FMath::Abs(Local.X) < DeepHalfWidth && FMath::Abs(Local.Y) < HalfY && Local.Z < 15.f;
}

void ABRDesertArena::GetNPCRoute(TArray<FVector>& OutRoute, int32& OutCheckpointIndex) const
{
	const FVector O = GetActorLocation();
	const float Z = Ground + 90.f;
	OutRoute = {
		O + FVector(TownEdgeX + 1500.f, RoadY, Z),
		O + FVector(0.f, RoadY, Z),
		O + FVector(WallX - 700.f, RoadY, Z),	// wait here for the checkpoint
		O + FVector(WallX + 900.f, RoadY, Z),
		O + FVector(11000.f, RoadY, Z),
		O + FVector(17000.f, RoadY, Z),
		O + FVector(EastX - 600.f, RoadY, Z)
	};
	OutCheckpointIndex = 2;
}

FTransform ABRDesertArena::GetNPCSpawn() const
{
	// Trucks pull onto the wide road at the edge of town (the narrow town street jammed them up).
	return FTransform(FRotator::ZeroRotator, GetActorLocation() + FVector(TownEdgeX + 300.f, RoadY, Ground + 90.f));
}

void ABRDesertArena::GetDogHomes(TArray<FVector>& OutHomes) const
{
	const FVector O = GetActorLocation();
	OutHomes = {
		O + FVector(2700.f, -5000.f, Ground + 60.f),
		O + FVector(2700.f, 4000.f, Ground + 60.f),
		O + FVector(5500.f, -2700.f, Ground + 60.f),
		O + FVector(-2600.f, 1500.f, Ground + 60.f)
	};
}

void ABRDesertArena::GetVillageSpots(TArray<FVector>& OutSpots) const
{
	const FVector O = GetActorLocation();
	const float Z = Ground + 100.f;
	OutSpots = {
		O + FVector(-10000.f, 1300.f, Z),	// plaza
		O + FVector(-10000.f, -350.f, Z),	// market stalls
		O + FVector(-10700.f, 2000.f, Z),	// taco stand
		O + FVector(-9000.f, 1500.f, Z),	// cockfight crowd
		O + FVector(-12000.f, 4500.f, Z),	// laundry / north houses
		O + FVector(-13500.f, -2400.f, Z),	// church
		O + FVector(-8000.f, -2900.f, Z),	// south houses
		O + FVector(-7800.f, 900.f, Z),		// east houses
		O + FVector(-12300.f, 1600.f, Z),	// well
		O + FVector(-10300.f, -2000.f, Z)	// tool shop
	};
}

FVector ABRDesertArena::GetCatapultLanding() const
{
	return GetActorLocation() + FVector(-8500.f, 0.f, Ground + 50.f);
}

void ABRDesertArena::SpawnGameplayActors(TSubclassOf<ABRTruck> RunnerTruckClass, TSubclassOf<ABRTruck> WardenTruckClass, TSubclassOf<ABRDog> DogClass)
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || !bBuildTestMap || bGameplaySpawned)
	{
		return;
	}
	bGameplaySpawned = true;
	const FVector O = GetActorLocation();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto At = [&](float X, float Y, float Z, float Yaw = 0.f) { return FTransform(FRotator(0.f, Yaw, 0.f), O + FVector(X, Y, Z)); };

	auto Station = [&](float X, float Y, EBRStationKind Kind, EBRItem Item, int32 Price, const FString& Title, const FLinearColor& Color,
		float Work = 0.f, float Cooldown = 0.f, bool bNight = false) -> ABRStation*
	{
		const FTransform T = At(X, Y, Ground);
		ABRStation* S = World->SpawnActorDeferred<ABRStation>(ABRStation::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!S) return nullptr;
		S->Kind = Kind; S->Item = Item; S->Price = Price; S->Title = Title; S->Color = Color;
		S->WorkSeconds = Work; S->CooldownSeconds = Cooldown; S->bNightOnly = bNight;
		S->FinishSpawning(T);
		return S;
	};

	// --- Shops & jobs (Runner side)
	const FLinearColor Shop(0.8f, 0.7f, 0.4f), Job(0.3f, 0.7f, 0.3f), Shady(0.5f, 0.1f, 0.1f);
	Station(-12000.f, -3400.f, EBRStationKind::BuyItem, EBRItem::Ladder, 60, TEXT("LADDERS $60"), Shop);
	Station(-12000.f, -3000.f, EBRStationKind::BuyItem, EBRItem::WireCutters, 50, TEXT("WIRE CUTTERS $50"), Shop);
	Station(-12000.f, -2600.f, EBRStationKind::BuyItem, EBRItem::GasCan, 25, TEXT("GAS $25"), Shop);
	Station(-11000.f, -3400.f, EBRStationKind::SellItem, EBRItem::Scrap, 25, TEXT("WE BUY SCRAP"), Shop);
	Station(-11000.f, -3000.f, EBRStationKind::BuyItem, EBRItem::CannonSuit, 500, TEXT("CANNONBALL SUIT $500"), FLinearColor(0.8f, 0.15f, 0.1f));
	Station(-11500.f, -3300.f, EBRStationKind::BuyItem, EBRItem::Shovel, 80, TEXT("SHOVEL $80"), Shop);
	// Weapon store (expensive): a knife doubles your punch damage; a gun is contraband (shows up on a scan!).
	{
		const FLinearColor Armas(0.25f, 0.25f, 0.28f);
		Station(-12600.f, -3400.f, EBRStationKind::BuyItem, EBRItem::Knife, 600, TEXT("ARMAS DON CHUY\nKNIFE $600"), Armas);
		Station(-12600.f, -3000.f, EBRStationKind::BuyItem, EBRItem::IllegalGun, 1800, TEXT("ARMAS DON CHUY\nPISTOL $1800 (contraband!)"), Armas);
		// Forger: a fake passport passes the papers scan (but not if you're carrying contraband).
		Station(-12600.f, -2600.f, EBRStationKind::BuyItem, EBRItem::FakePassport, 1000, TEXT("PAPELES CHUCHO\nFAKE PASSPORT $1000"), FLinearColor(0.35f, 0.2f, 0.45f));
		// Gadgets.
		const FLinearColor Gadget(0.3f, 0.35f, 0.3f);
		Station(-13200.f, -3400.f, EBRStationKind::BuyItem, EBRItem::SmokeBomb, 150, TEXT("SMOKE BOMB $150"), Gadget);
		Station(-13200.f, -3000.f, EBRStationKind::BuyItem, EBRItem::GrappleHook, 350, TEXT("GRAPPLING HOOK $350"), Gadget);
		Station(-13200.f, -2600.f, EBRStationKind::BuyItem, EBRItem::Lockpick, 250, TEXT("LOCKPICK $250"), Gadget);
		Station(-13800.f, -3000.f, EBRStationKind::BuyItem, EBRItem::FakeUniform, 1500, TEXT("FAKE BORDER PATROL UNIFORM $1500"), FLinearColor(0.15f, 0.25f, 0.55f));
	}
	Station(-11500.f, -2700.f, EBRStationKind::BuyItem, EBRItem::Trumpet, 120, TEXT("MARIACHI TRUMPET $120"), FLinearColor(0.9f, 0.6f, 0.1f));
	Station(-12000.f, -2200.f, EBRStationKind::BuyItem, EBRItem::Poncho, 200, TEXT("PONCHO DISGUISE $200"), FLinearColor(0.75f, 0.25f, 0.15f));
	Station(-11600.f, -2200.f, EBRStationKind::BuyItem, EBRItem::Lasso, 150, TEXT("LASSO $150"), Shop);
	Station(-11200.f, -2200.f, EBRStationKind::BuyItem, EBRItem::CardboardBox, 30, TEXT("CARDBOARD BOX $30"), Shop);
	Station(-11000.f, -2600.f, EBRStationKind::Job, EBRItem::None, 20, TEXT("Work the register"), Job, 5.f, 3.f);
	Station(-10300.f, -4000.f, EBRStationKind::BuyItem, EBRItem::GasCan, 25, TEXT("GAS PUMP $25"), Shop);
	Station(-11700.f, 1550.f, EBRStationKind::Job, EBRItem::None, 15, TEXT("Cook & sell tacos"), Job, 4.f, 2.f);
	Station(-11700.f, 2050.f, EBRStationKind::BuyItem, EBRItem::Taco, 8, TEXT("TACO $8"), Shop);
	Station(-11300.f, 2050.f, EBRStationKind::BuyItem, EBRItem::TacoPlatter, 40, TEXT("TACO PLATTER $40 (feeds 8)"), Shop);
	Station(-13000.f, 1500.f, EBRStationKind::HireClient, EBRItem::None, 250, TEXT(""), FLinearColor(0.3f, 0.25f, 0.2f), 0.f, 10.f);
	Station(-14600.f, 4900.f, EBRStationKind::BuyItem, EBRItem::Drugs, 150, TEXT("??? PACKAGE $150"), Shady);
	Station(-14600.f, 5600.f, EBRStationKind::BuyItem, EBRItem::IllegalGun, 150, TEXT("??? $150"), Shady, 0.f, 0.f, true);
	if (ABRStation* Marina = Station(-6400.f, -6300.f, EBRStationKind::BuyBoat, EBRItem::None, 120, TEXT("BOATS $120"), FLinearColor(0.3f, 0.5f, 0.9f)))
	{
		Marina->BoatSpawn = O + FVector(-900.f, -6300.f, 40.f);
	}
	const FVector2D ScrapSpots[] = { {-13800.f, -6200.f}, {-15200.f, -500.f}, {-8200.f, 6600.f}, {-9000.f, -6800.f}, {-6600.f, 3000.f} };
	for (const FVector2D& Spot : ScrapSpots)
	{
		Station(Spot.X, Spot.Y, EBRStationKind::Job, EBRItem::Scrap, 0, TEXT("Dig through junk"), FLinearColor(0.35f, 0.3f, 0.25f), 3.f, 25.f);
	}

	// --- Border
	World->SpawnActor<ABRLineBarrier>(ABRLineBarrier::StaticClass(), At(LineX, 0.f, Ground), Params);
	for (float Y = -HalfY + 600.f; Y <= HalfY - 600.f; Y += 400.f)	// gaps at both far ends (risky routes)
	{
		if (FMath::Abs(Y - RoadY) < RoadHalfWidth + 150.f)
		{
			continue; // the road goes through
		}
		World->SpawnActor<ABRSpikeFence>(ABRSpikeFence::StaticClass(), At(FenceX, Y, Ground), Params);
	}
	// Spike fences along both sides of the road from the border fence to the checkpoint: stay on the road
	// (the legal way, through the checkpoint) or cut through with wire cutters to get at the river / Wall.
	for (float X = FenceX + 450.f; X <= WallX - 700.f; X += 400.f)
	{
		for (const float Side : { -1.f, 1.f })
		{
			const bool bBridge = FMath::Abs(X) < RiverHalfWidth + 900.f; // on the bridge: just inside the rails, standing on the deck
			World->SpawnActor<ABRSpikeFence>(ABRSpikeFence::StaticClass(), At(X, RoadY + Side * (RoadHalfWidth + (bBridge ? 25.f : 120.f)), Ground, 90.f), Params);
		}
	}
	World->SpawnActor<ABRWall>(ABRWall::StaticClass(), At(WallX, 0.f, Ground), Params);
	Gate = World->SpawnActor<ABRCheckpointGate>(ABRCheckpointGate::StaticClass(), At(WallX, RoadY, Ground), Params);
	World->SpawnActor<ABRWallDoor>(ABRWallDoor::StaticClass(), At(WallX, ABRWall::DoorY, Ground), Params); // Border Patrol's key door

	// --- Cockfight pit + betting counters
	if (bBuildVillage)
	{
		World->SpawnActor<ABRVillage>(VillageClass ? VillageClass.Get() : ABRVillage::StaticClass(), At(0.f, 0.f, 0.f), Params);
	}
	UClass* PitClass = CockfightPitClass ? CockfightPitClass.Get() : ABRCockfightPit::StaticClass();
	World->SpawnActor<ABRCockfightPit>(PitClass, At(-9000.f, 2200.f, Ground), Params); // spawns its own betting counters

	// --- Pinata in the plaza, tunnels under the border (exit past the Wall)
	World->SpawnActor<ABRPinata>(ABRPinata::StaticClass(), At(-10450.f, 1250.f, Ground), Params);
	// --- Mariachi band stage north of the plaza (facing the plaza)
	World->SpawnActor<ABRMariachiBand>(ABRMariachiBand::StaticClass(), At(-10300.f, 3200.f, Ground, -90.f), Params);

	// --- Lucha Libre ring out in the open (spawns its own challenge + betting counters)
	World->SpawnActor<ABRLuchaRing>(ABRLuchaRing::StaticClass(), At(-11000.f, -6200.f, Ground), Params); // open desert by the scrapyard
	if (ABRTunnel* T1 = World->SpawnActorDeferred<ABRTunnel>(ABRTunnel::StaticClass(), At(-5900.f, -5200.f, Ground), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		T1->ExitOffset = FVector(WallX + 900.f - (-5900.f), -400.f, 0.f);
		T1->FinishSpawning(At(-5900.f, -5200.f, Ground));
	}
	if (ABRTunnel* T2 = World->SpawnActorDeferred<ABRTunnel>(ABRTunnel::StaticClass(), At(-6200.f, 6000.f, Ground), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		T2->ExitOffset = FVector(WallX + 900.f - (-6200.f), 500.f, 0.f);
		T2->FinishSpawning(At(-6200.f, 6000.f, Ground));
	}

	// --- Stuff to grab, swing and throw (Z)
	{
		const FVector2D PropSpots[] = { {-10900.f, -560.f}, {-10650.f, -480.f}, {-9700.f, 300.f}, {-10300.f, 1150.f}, {-11050.f, 2500.f},
			{-12100.f, -1900.f}, {-9100.f, 1700.f}, {-8300.f, -2300.f}, {-6000.f, -2600.f}, {5600.f, -2500.f}, {6400.f, 2600.f} };
		int32 PropIndex = 0;
		for (const FVector2D& Spot : PropSpots)
		{
			const FTransform T = At(Spot.X, Spot.Y, Ground + 60.f, PropIndex * 37.f);
			if (ABRPhysicsProp* Prop = World->SpawnActorDeferred<ABRPhysicsProp>(ABRPhysicsProp::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn))
			{
				Prop->Color = (PropIndex % 3 == 0) ? FLinearColor(0.35f, 0.2f, 0.1f) : FLinearColor(0.6f, 0.45f, 0.25f);
				Prop->FinishSpawning(T);
			}
			++PropIndex;
		}
	}

	// --- Human cannon on the near bank, aimed across the border
	World->SpawnActor<ABRCannon>(ABRCannon::StaticClass(), At(-5800.f, 2500.f, Ground), Params);

	// --- Watchtowers with spotlights, looking back over Mexico
	World->SpawnActor<ABRWatchtower>(ABRWatchtower::StaticClass(), At(WallX + 700.f, -4800.f, Ground, 180.f), Params);
	World->SpawnActor<ABRWatchtower>(ABRWatchtower::StaticClass(), At(WallX + 700.f, 4200.f, Ground, 180.f), Params);

	// --- Border Patrol supply counters (paid from the government budget)
	auto Supply = [&](float X, float Y, EBRWardenGear Gear, EBRItem SupplyItem, int32 Cost, float Cooldown)
	{
		const FTransform T = At(X, Y, Ground, 180.f);
		if (ABRStation* S = World->SpawnActorDeferred<ABRStation>(ABRStation::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			S->Kind = EBRStationKind::WardenSupply; S->WardenGear = Gear; S->Item = SupplyItem; S->Price = Cost;
			S->CooldownSeconds = Cooldown; S->Color = FLinearColor(0.15f, 0.25f, 0.55f);
			S->FinishSpawning(T);
		}
	};
	Supply(7800.f, -150.f, EBRWardenGear::ExtraTraps, EBRItem::None, 600, 0.f);
	Supply(7800.f, 250.f, EBRWardenGear::DroneRecharge, EBRItem::None, 300, 0.f);
	Supply(7800.f, 650.f, EBRWardenGear::HardHat, EBRItem::None, 400, 0.f);
	Supply(7800.f, 1050.f, EBRWardenGear::PatrolDog, EBRItem::None, 1500, 30.f);
	Supply(7800.f, 1450.f, EBRWardenGear::None, EBRItem::Taco, 20, 0.f);
	Supply(7800.f, 1800.f, EBRWardenGear::None, EBRItem::Lasso, 250, 0.f);
	Supply(7800.f, 2150.f, EBRWardenGear::None, EBRItem::SecurityCamera, 350, 0.f);
	Supply(7800.f, 2850.f, EBRWardenGear::None, EBRItem::SmokeBomb, 150, 0.f);
	Supply(7800.f, 2500.f, EBRWardenGear::Reinforcements, EBRItem::None, 1200, 60.f);

	// --- Hiding spots (dumpsters, porta-potties, hay bales) on both sides
	auto Hide = [&](float X, float Y, float Yaw, EBRHideStyle HideStyle)
	{
		const FTransform T = At(X, Y, Ground, Yaw);
		if (ABRHidingSpot* H = World->SpawnActorDeferred<ABRHidingSpot>(ABRHidingSpot::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			H->Style = HideStyle;
			H->FinishSpawning(T);
		}
	};
	Hide(-9800.f, -3500.f, 0.f, EBRHideStyle::Dumpster);
	Hide(-13300.f, 700.f, 0.f, EBRHideStyle::PortaPotty);
	Hide(-7600.f, -1900.f, 90.f, EBRHideStyle::HayBale);
	Hide(-4300.f, -3000.f, 0.f, EBRHideStyle::PortaPotty);
	Hide(5400.f, -4300.f, 180.f, EBRHideStyle::HayBale);
	Hide(6200.f, 5600.f, 180.f, EBRHideStyle::Dumpster);

	// --- Warden base
	World->SpawnActor<APVVDetentionZone>(APVVDetentionZone::StaticClass(), At(7000.f, 4000.f, Ground + 150.f), Params);
	Catapult = World->SpawnActor<ABRCatapult>(ABRCatapult::StaticClass(), At(7200.f, 1500.f, Ground), Params);

	auto Car = [&](float X, float Y, float Yaw, bool bWarden, const FLinearColor& Paint)
	{
		const FTransform T = At(X, Y, Ground + 80.f, Yaw);
		UClass* TruckClass = bWarden ? WardenTruckClass.Get() : RunnerTruckClass.Get();
		if (!TruckClass) TruckClass = ABRTruck::StaticClass();
		if (ABRTruck* C = World->SpawnActorDeferred<ABRTruck>(TruckClass, T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			C->bRequiresKey = !bWarden;
			C->bInfiniteFuel = bWarden;
			if (TruckClass == ABRTruck::StaticClass())
			{
				C->PaintColor = Paint; // your Blueprint trucks keep their own colour
			}
			C->FinishSpawning(T);
		}
	};
	Car(5200.f, -3000.f, 0.f, true, FLinearColor(0.85f, 0.88f, 0.85f));
	Car(5200.f, -5000.f, 0.f, true, FLinearColor(0.85f, 0.88f, 0.85f));
	Car(-9500.f, -2400.f, 0.f, false, FLinearColor(0.5f, 0.2f, 0.1f));

	// --- Car lot: buy your own ride (hold E on a car)
	{
		struct FLotCar { float X; EBRCarModel Model; int32 Price; FLinearColor Paint; };
		const FLotCar Lot[] = {
			{ -8150.f, EBRCarModel::Pickup, 500, FLinearColor(0.75f, 0.2f, 0.1f) },
			{ -7700.f, EBRCarModel::Sedan, 500, FLinearColor(0.15f, 0.3f, 0.7f) },
			{ -7250.f, EBRCarModel::Lowrider, 800, FLinearColor(0.55f, 0.1f, 0.6f) },
			{ -6800.f, EBRCarModel::Van, 1200, FLinearColor(0.9f, 0.88f, 0.8f) },
		};
		for (const FLotCar& Def : Lot)
		{
			const FTransform T = At(Def.X, 2950.f, Ground + 80.f, -90.f);
			if (ABRTruck* C = World->SpawnActorDeferred<ABRTruck>(ABRTruck::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				C->Model = Def.Model;
				C->PaintColor = Def.Paint;
				C->bForSale = true;
				C->SalePrice = Def.Price;
				C->bRequiresKey = true;
				C->FinishSpawning(T);
			}
		}
	}

	// --- La Cantina del Diablo (bar fights, drinks, drinking contest, bouncer). Door faces the road.
	World->SpawnActor<ABRCantina>(ABRCantina::StaticClass(), At(-8000.f, -6300.f, Ground, 90.f), Params);

	// --- Smuggling job board in town
	World->SpawnActor<ABRMissionBoard>(ABRMissionBoard::StaticClass(), At(-9800.f, 1900.f, Ground, 0.f), Params);

	// --- Planks lying around town (grab with Z and swing)
	{
		const FVector2D PlankSpots[] = { {-12400.f, 900.f}, {-9600.f, -1950.f}, {-13000.f, -3800.f}, {-7200.f, 2300.f}, {-11500.f, 4300.f}, {-6200.f, -4000.f} };
		for (const FVector2D& P : PlankSpots)
		{
			const FTransform T = FTransform(FRotator(0.f, FMath::FRandRange(0.f, 180.f), 0.f), GetActorLocation() + FVector(P.X, P.Y, Ground + 30.f), FVector(1.3f, 0.18f, 0.05f));
			if (ABRPhysicsProp* Plank = World->SpawnActorDeferred<ABRPhysicsProp>(ABRPhysicsProp::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn))
			{
				Plank->MassKg = 4.f;
				Plank->KnockoutSpeed = 600.f;
				Plank->Color = FLinearColor(0.55f, 0.38f, 0.2f);
				Plank->FinishSpawning(T);
			}
		}
	}

	// --- Wanted posters around town
	{
		const FVector Posters[] = { {-11800.f, -420.f, -90.f}, {-8000.f, -420.f, -90.f}, {-11000.f, -1950.f, 90.f}, {-7300.f, -5000.f, 90.f}, {-7550.f, 2420.f, -90.f}, {-5600.f, -2050.f, 90.f} };
		for (const FVector& P : Posters)
		{
			World->SpawnActor<ABRWantedPoster>(ABRWantedPoster::StaticClass(), At(P.X, P.Y, Ground, P.Z), Params);
		}
	}

	// --- Patrol dogs (Warden side + the near bank)
	TArray<FVector> Homes;
	GetDogHomes(Homes);
	for (const FVector& Home : Homes)
	{
		FActorSpawnParameters DogParams;
		DogParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		World->SpawnActor<ABRDog>(DogClass ? DogClass.Get() : ABRDog::StaticClass(), Home, FRotator::ZeroRotator, DogParams);
	}

	// --- Player starts
	auto Start = [&](float X, float Y, float Yaw, EPVVFaction Faction)
	{
		if (APVVPlayerStart* S = World->SpawnActor<APVVPlayerStart>(APVVPlayerStart::StaticClass(), At(X, Y, Ground + 110.f, Yaw), Params))
		{
			S->Faction = Faction;
		}
	};
	for (int32 i = 0; i < 8; ++i)
	{
		Start(-12600.f + (i % 4) * 250.f, -400.f + (i / 4) * 800.f, 0.f, EPVVFaction::Pirates);
		Start(8000.f + (i % 4) * 250.f, 1500.f + (i / 4) * 600.f, 180.f, EPVVFaction::Vikings);
	}
}
