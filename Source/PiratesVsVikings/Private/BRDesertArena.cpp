#include "BRDesertArena.h"
#include "BRSettings.h"
#include "BRAudio.h"
#include "BRCar.h"
#include "BRCantina.h"
#include "BRDog.h"
#include "BRGameState.h"
#include "BRWorldActors.h"
#include "PVVDetentionZone.h"
#include "PVVPlayerStart.h"
#include "Components/AudioComponent.h"
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
	const FLinearColor Sand(0.62f, 0.48f, 0.3f);
	const FLinearColor Adobe(0.7f, 0.5f, 0.35f);
	const FLinearColor Concrete(0.45f, 0.44f, 0.42f);
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
	AddBlock(TEXT("Sand_West"), FVector(-(BankX + FloorLen * 0.5f), 0.f, Ground - 200.f), FVector(FloorLen, HalfY * 2.f, 400.f), Sand);
	const float EastLen = EastX - BankX;
	AddBlock(TEXT("Sand_East"), FVector(BankX + EastLen * 0.5f, 0.f, Ground - 200.f), FVector(EastLen, HalfY * 2.f, 400.f), Sand);
	// Banks: from the desert floor (+100) down to the river bed (-150) over 8 m.
	AddBlock(TEXT("Sand_WestBank"), FVector(-1915.f, 0.f, -73.f), FVector(840.f, HalfY * 2.f, 100.f), Sand, FRotator(-17.4f, 0.f, 0.f));
	AddBlock(TEXT("Sand_EastBank"), FVector(1915.f, 0.f, -73.f), FVector(840.f, HalfY * 2.f, 100.f), Sand, FRotator(17.4f, 0.f, 0.f));

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
	AddBlock(TEXT("Edge_N"), FVector((EastX - HalfX) * 0.5f, HalfY + 100.f, 300.f), FVector(HalfX + EastX + 400.f, 200.f, 1000.f), Adobe);
	AddBlock(TEXT("Edge_S"), FVector((EastX - HalfX) * 0.5f, -HalfY - 100.f, 300.f), FVector(HalfX + EastX + 400.f, 200.f, 1000.f), Adobe);
	AddBlock(TEXT("Edge_W"), FVector(-HalfX - 100.f, 0.f, 300.f), FVector(200.f, HalfY * 2.f, 1000.f), Adobe);
	AddBlock(TEXT("Edge_E"), FVector(EastX + 100.f, 0.f, 300.f), FVector(200.f, HalfY * 2.f, 1000.f), Adobe);

	// Town.
	AddHut(TEXT("ToolShop"), FVector(-11500.f, -3000.f, Ground), FVector(1400.f, 1000.f, 400.f), Adobe);
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
	AddHut(TEXT("House1"), FVector(-8500.f, 4000.f, Ground), FVector(900.f, 900.f, 350.f), Adobe);
	AddHut(TEXT("House2"), FVector(-13500.f, 3000.f, Ground), FVector(800.f, 1000.f, 350.f), Adobe);
	AddSign(TEXT("Sign_Town"), TEXT("WELCOME TO DUSTY GULCH"), FVector(-15000.f, 0.f, Ground + 450.f), 0.f, 150.f, FColor(255, 230, 180));

	// Warden side.
	AddBlock(TEXT("Cell_Front"), FVector(6400.f, 4000.f, Ground + 150.f), FVector(40.f, 640.f, 300.f), Concrete);
	AddBlock(TEXT("Cell_Back"), FVector(5800.f, 4000.f, Ground + 150.f), FVector(40.f, 640.f, 300.f), Concrete);
	AddBlock(TEXT("Cell_Left"), FVector(6100.f, 3700.f, Ground + 150.f), FVector(640.f, 40.f, 300.f), Concrete);
	AddBlock(TEXT("Cell_Right"), FVector(6100.f, 4300.f, Ground + 150.f), FVector(640.f, 40.f, 300.f), Concrete);
	AddSign(TEXT("Sign_Pen"), TEXT("HOLDING PEN"), FVector(6450.f, 4000.f, Ground + 380.f), 180.f, 80.f, FColor(255, 140, 40));
	AddBlock(TEXT("Tower"), FVector(4400.f, -2400.f, Ground + 600.f), FVector(300.f, 300.f, 1200.f), Concrete);
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

	// Background music: every song in Content/Music/Ambient, played on each player's machine.
	AmbientMusic = CreateDefaultSubobject<UAudioComponent>(TEXT("AmbientMusic"));
	AmbientMusic->SetupAttachment(Root);
	AmbientMusic->bAutoActivate = false;
	AmbientMusic->bAllowSpatialization = false;
	AmbientMusic->bIsUISound = true;
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

	if (TActorIterator<ADirectionalLight>(GetWorld()))
	{
		Sun->DestroyComponent();
		Moon->DestroyComponent();
		Sun = nullptr;
		Moon = nullptr;
	}
	if (TActorIterator<ASkyLight>(GetWorld()))
	{
		SkyLight->DestroyComponent();
	}
	if (TActorIterator<ASkyAtmosphere>(GetWorld()))
	{
		Atmosphere->DestroyComponent();
	}
	if (TActorIterator<AExponentialHeightFog>(GetWorld()))
	{
		Fog->DestroyComponent();
		Fog = nullptr;
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
		Moon->SetIntensity(0.4f * FMath::Clamp(-Elevation * 2.f, 0.f, 1.f));
	}

	if (Fog)
	{
		const float TargetDensity = GS->Weather == EBRWeather::Clear ? 0.003f : (GS->Weather == EBRWeather::Rain ? 0.03f : 0.09f);
		FogDensity = FMath::FInterpTo(FogDensity, TargetDensity, DeltaSeconds, 0.5f);
		Fog->SetFogDensity(FogDensity);
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
	World->SpawnActor<ABRWall>(ABRWall::StaticClass(), At(WallX, 0.f, Ground), Params);
	Gate = World->SpawnActor<ABRCheckpointGate>(ABRCheckpointGate::StaticClass(), At(WallX, RoadY, Ground), Params);

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
