#include "BRDesertArena.h"
#include "BRAudio.h"
#include "BRCar.h"
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
	AddBlock(TEXT("Sand_East"), FVector(BankX + FloorLen * 0.5f, 0.f, Ground - 200.f), FVector(FloorLen, HalfY * 2.f, 400.f), Sand);
	// Banks: from the desert floor (+100) down to the river bed (-150) over 8 m.
	AddBlock(TEXT("Sand_WestBank"), FVector(-1915.f, 0.f, -73.f), FVector(840.f, HalfY * 2.f, 100.f), Sand, FRotator(-17.4f, 0.f, 0.f));
	AddBlock(TEXT("Sand_EastBank"), FVector(1915.f, 0.f, -73.f), FVector(840.f, HalfY * 2.f, 100.f), Sand, FRotator(17.4f, 0.f, 0.f));

	// The road: town -> bridge -> checkpoint -> far side.
	AddBlock(TEXT("Road_Strip"), FVector(0.f, RoadY, Ground + 2.f), FVector(HalfX * 2.f, 700.f, 4.f), FLinearColor(0.12f, 0.12f, 0.13f), FRotator::ZeroRotator, false);
	AddBlock(TEXT("Bridge_Deck"), FVector(0.f, RoadY, Ground - 50.f), FVector(BankX * 2.f + 200.f, 800.f, 100.f), FLinearColor(0.4f, 0.4f, 0.42f));
	AddBlock(TEXT("Bridge_RailN"), FVector(0.f, RoadY + 410.f, Ground + 50.f), FVector(BankX * 2.f, 20.f, 100.f), FLinearColor(0.3f, 0.3f, 0.32f));
	AddBlock(TEXT("Bridge_RailS"), FVector(0.f, RoadY - 410.f, Ground + 50.f), FVector(BankX * 2.f, 20.f, 100.f), FLinearColor(0.3f, 0.3f, 0.32f));

	// Map edges.
	AddBlock(TEXT("Edge_N"), FVector(0.f, HalfY + 100.f, 300.f), FVector(HalfX * 2.f + 400.f, 200.f, 1000.f), Adobe);
	AddBlock(TEXT("Edge_S"), FVector(0.f, -HalfY - 100.f, 300.f), FVector(HalfX * 2.f + 400.f, 200.f, 1000.f), Adobe);
	AddBlock(TEXT("Edge_W"), FVector(-HalfX - 100.f, 0.f, 300.f), FVector(200.f, HalfY * 2.f, 1000.f), Adobe);
	AddBlock(TEXT("Edge_E"), FVector(HalfX + 100.f, 0.f, 300.f), FVector(200.f, HalfY * 2.f, 1000.f), Adobe);

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
	AddBlock(TEXT("Tower"), FVector(4400.f, -1000.f, Ground + 600.f), FVector(300.f, 300.f, 1200.f), Concrete);
	AddHut(TEXT("WardenHQ"), FVector(8500.f, 4000.f, Ground), FVector(1200.f, 1400.f, 400.f), FLinearColor(0.3f, 0.35f, 0.3f));
	AddSign(TEXT("Sign_HQ"), TEXT("WARDEN HQ"), FVector(7880.f, 4000.f, Ground + 480.f), 180.f, 100.f, FColor(255, 140, 40));

	// Safe zone.
	AddBlock(TEXT("Safe_Stripe"), FVector(SafeX + (HalfX - SafeX) * 0.5f, 0.f, Ground + 2.f), FVector(HalfX - SafeX, HalfY * 2.f, 4.f), FLinearColor(0.1f, 0.8f, 0.2f), FRotator::ZeroRotator, false);
	AddSign(TEXT("Sign_Safe"), TEXT("SAFE ZONE"), FVector(SafeX - 50.f, 0.f, Ground + 400.f), 180.f, 220.f, FColor(80, 255, 120));

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
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (const TObjectPtr<UMaterialInterface>* Material = BlockMaterials.Find(Mesh->GetFName()); Material && *Material)
		{
			Mesh->SetMaterial(0, *Material);
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
	AmbientMusic->SetVolumeMultiplier(AmbientVolume);
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
		float Target = AmbientVolume;
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
	const bool bOnBridge = FMath::Abs(Local.Y - RoadY) < 420.f && Local.Z > Ground - 60.f;
	return !bOnBridge && FMath::Abs(Local.X) < DeepHalfWidth && FMath::Abs(Local.Y) < HalfY && Local.Z < 15.f;
}

void ABRDesertArena::GetNPCRoute(TArray<FVector>& OutRoute, int32& OutCheckpointIndex) const
{
	const FVector O = GetActorLocation();
	const float Z = Ground + 90.f;
	OutRoute = {
		O + FVector(-6000.f, RoadY, Z),
		O + FVector(0.f, RoadY, Z),
		O + FVector(WallX - 700.f, RoadY, Z),	// wait here for the checkpoint
		O + FVector(WallX + 900.f, RoadY, Z),
		O + FVector(11000.f, RoadY, Z),
		O + FVector(HalfX - 600.f, RoadY, Z)
	};
	OutCheckpointIndex = 2;
}

FTransform ABRDesertArena::GetNPCSpawn() const
{
	return FTransform(FRotator::ZeroRotator, GetActorLocation() + FVector(-HalfX + 900.f, RoadY, Ground + 90.f));
}

void ABRDesertArena::GetDogHomes(TArray<FVector>& OutHomes) const
{
	const FVector O = GetActorLocation();
	OutHomes = {
		O + FVector(2700.f, -5000.f, Ground + 60.f),
		O + FVector(2700.f, 4000.f, Ground + 60.f),
		O + FVector(5500.f, -1200.f, Ground + 60.f),
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
	Station(-11000.f, -2600.f, EBRStationKind::Job, EBRItem::None, 20, TEXT("Work the register"), Job, 5.f, 3.f);
	Station(-10300.f, -4000.f, EBRStationKind::BuyItem, EBRItem::GasCan, 25, TEXT("GAS PUMP $25"), Shop);
	Station(-11700.f, 1550.f, EBRStationKind::Job, EBRItem::None, 15, TEXT("Cook & sell tacos"), Job, 4.f, 2.f);
	Station(-11700.f, 2050.f, EBRStationKind::BuyItem, EBRItem::Taco, 8, TEXT("TACO $8"), Shop);
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
		if (FMath::Abs(Y - RoadY) < 500.f)
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
