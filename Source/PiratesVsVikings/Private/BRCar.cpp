#include "BRCar.h"
#include "BRAudio.h"
#include "BRCharacter.h"
#include "BRDesertArena.h"
#include "BRMapKit.h"
#include "BRGameMode.h"
#include "BRGameState.h"
#include "BRPlayerController.h"
#include "BRPlayerState.h"
#include "BRWorldActors.h"
#include "PVVInputUtils.h"
#include "Components/AudioComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

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
		FVector(-20.f, -45.f, 140.f),
		FVector(-20.f, 45.f, 140.f),
		FVector(-150.f, -45.f, 140.f),
		FVector(-150.f, 45.f, 140.f)
	};
	PontoonOffsets = {
		FVector(170.f, 85.f, -45.f), FVector(170.f, -85.f, -45.f),
		FVector(-170.f, 85.f, -45.f), FVector(-170.f, -85.f, -45.f)
	};

	RadioAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("RadioAudio"));
	RadioAudio->SetupAttachment(SeatRoot);
	RadioAudio->bAutoActivate = false;
	RadioAudio->bOverrideAttenuation = true;
	RadioAudio->AttenuationOverrides.bAttenuate = true;
	RadioAudio->AttenuationOverrides.bSpatialize = true;
	RadioAudio->AttenuationOverrides.AttenuationShapeExtents = FVector(500.f, 0.f, 0.f);
	RadioAudio->AttenuationOverrides.FalloffDistance = 2500.f;
}

void ABRCar::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRCar, Fuel);
	DOREPLIFETIME(ABRCar, RadioStation);
	DOREPLIFETIME(ABRCar, RadioStartTime);
}

void ABRCar::BeginPlay()
{
	Super::BeginPlay();
	if (bInfiniteFuel && HasAuthority())
	{
		Fuel = 1.f;
	}

	// Paint the greybox: hull + "Body*" parts in the paint colour, wheels black, lights yellow.
	TArray<UStaticMeshComponent*> Meshes;
	GetComponents<UStaticMeshComponent>(Meshes);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (!Mesh->GetStaticMesh() || !Mesh->GetStaticMesh()->GetPathName().StartsWith(TEXT("/Engine/BasicShapes")))
		{
			continue;
		}
		const FString Name = Mesh->GetName();
		FLinearColor Color = PaintColor;
		if (Name.StartsWith(TEXT("Wheel"))) Color = FLinearColor(0.03f, 0.03f, 0.03f);
		else if (Name.StartsWith(TEXT("Light"))) Color = FLinearColor(1.f, 0.9f, 0.5f);
		else if (Name.StartsWith(TEXT("Trim"))) Color = FLinearColor(0.08f, 0.08f, 0.08f);
		if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
		}
	}

	// Radio stations: songs dropped in Content/Music/Radio + any assigned on the Blueprint.
	TArray<USoundBase*> FolderSongs;
	BRAudio::LoadSounds(TEXT("/Game/Music/Radio"), FolderSongs);
	LoadedStations.Reset();
	for (USoundBase* Song : Stations) { if (Song) LoadedStations.Add(Song); }
	for (USoundBase* Song : FolderSongs) { LoadedStations.Add(Song); }
	RadioAudio->OnAudioFinished.AddDynamic(this, &ABRCar::OnRadioFinished);
	OnRep_Radio();
}

void ABRCar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority() && !bInfiniteFuel && GetDriver() && Fuel > 0.f)
	{
		Fuel = FMath::Max(0.f, Fuel - FMath::Abs(ThrottleInput) * FuelPerSecond * DeltaSeconds);
	}
}

// -------------------------------------------------------------------------------------------
// Driving

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

bool ABRCar::IsFlooded() const
{
	return BRMap::IsDeepWater(GetWorld(), GetActorLocation());
}

float ABRCar::GetDriveScale() const
{
	if (IsFlooded())
	{
		return 0.f; // engine drowned - you needed a boat
	}
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

// -------------------------------------------------------------------------------------------
// Interaction: refuel, Warden search, otherwise board/drive

bool ABRCar::WantsRefuel(const APVVCharacter* Interactor) const
{
	const ABRPlayerState* PS = Interactor ? Cast<ABRPlayerState>(Interactor->GetPVVPlayerState()) : nullptr;
	return !bInfiniteFuel && Fuel < 0.95f && PS && PS->HasItem(EBRItem::GasCan) && !Interactor->GetCurrentBoat();
}

bool ABRCar::WantsSearch(const APVVCharacter* Interactor) const
{
	if (!Interactor || Interactor->GetFaction() != BR::Wardens || bInfiniteFuel || Interactor->GetCurrentBoat()
		|| Interactor->GetCondition() != EPVVCondition::Normal || Interactor->GetCarriedCharacter())
	{
		return false;
	}
	for (const APVVCharacter* Occupant : Occupants)
	{
		if (Occupant)
		{
			return true;
		}
	}
	return false;
}

APVVCharacter* ABRCar::FindPrisonerToUnload(const APVVCharacter* Interactor) const
{
	if (!Interactor || Interactor->GetFaction() != BR::Wardens || Interactor->GetCurrentBoat() || Interactor->GetCarriedCharacter()
		|| Interactor->GetCondition() != EPVVCondition::Normal)
	{
		return nullptr;
	}
	for (APVVCharacter* Occupant : Occupants)
	{
		if (Occupant && Occupant->GetCondition() == EPVVCondition::Restrained)
		{
			return Occupant;
		}
	}
	return nullptr;
}

bool ABRCar::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	if (FindPrisonerToUnload(Interactor))
	{
		return true;
	}
	if (WantsSearch(Interactor))
	{
		return true;
	}
	if (WantsRefuel(Interactor) && Interactor->GetCondition() == EPVVCondition::Normal && !Interactor->GetCarriedCharacter())
	{
		return true;
	}
	return Super::CanInteract_Implementation(Interactor);
}

float ABRCar::GetInteractDuration_Implementation(APVVCharacter* Interactor) const
{
	if (FindPrisonerToUnload(Interactor)) return 0.6f;
	if (WantsSearch(Interactor)) return 2.5f;
	if (WantsRefuel(Interactor)) return 2.f;
	return 0.f;
}

FText ABRCar::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	if (const APVVCharacter* Prisoner = FindPrisonerToUnload(Interactor))
	{
		return FText::Format(LOCTEXT("Unload", "Take {0} out (carry them to the pen or the catapult)"), FText::FromString(Prisoner->GetPlayerDisplayName()));
	}
	if (WantsSearch(Interactor))
	{
		return LOCTEXT("Search", "Order everyone OUT of the truck (hold)");
	}
	if (WantsRefuel(Interactor))
	{
		return LOCTEXT("Refuel", "Pour in gas (hold)");
	}
	if (Interactor && !Interactor->GetCarriedCharacter() && Occupants.Num() > 0 && !Occupants[0])
	{
		if (!CanDrive(Interactor))
		{
			return LOCTEXT("NoKey", "Get in (no key - passenger)");
		}
		if (GetDriveScale() <= 0.f)
		{
			return IsFlooded() ? LOCTEXT("Flooded", "Drive (ENGINE FLOODED)") : LOCTEXT("NoGas", "Drive (TANK EMPTY - buy gas)");
		}
	}
	return Super::GetInteractPrompt_Implementation(Interactor);
}

void ABRCar::OnInteract_Implementation(APVVCharacter* Interactor)
{
	if (!HasAuthority() || !Interactor)
	{
		return;
	}
	if (APVVCharacter* Prisoner = FindPrisonerToUnload(Interactor))
	{
		Interactor->StartCarrying(Prisoner); // pulls them out of the seat onto your shoulder
		return;
	}
	if (WantsSearch(Interactor))
	{
		// Pull out the suspects: Runners, people with no papers, people with contraband.
		const ABRTruck* AsTruck = Cast<ABRTruck>(this);
		const bool bNPCTruck = AsTruck && AsTruck->IsNPCTruck();
		int32 Found = 0;
		int32 Seized = 0;
		TArray<APVVCharacter*> Out;
		for (APVVCharacter* Occupant : Occupants)
		{
			ABRCharacter* BRC = Cast<ABRCharacter>(Occupant);
			if (!BRC)
			{
				continue;
			}
			if (BRC->bCarriesContraband)
			{
				BRC->bCarriesContraband = false;
				++Seized;
				if (!BRC->bIllegal)
				{
					continue; // contraband taken, papers fine: they can stay
				}
			}
			if (BRC->IsSuspect())
			{
				Out.Add(BRC);
				++Found;
			}
		}
		if (!bNPCTruck)
		{
			EjectAll(); // a Runner-owned truck: everybody out
		}
		else
		{
			for (APVVCharacter* Suspect : Out)
			{
				Unseat(Suspect);
				Suspect->LaunchCharacter(FVector(FMath::FRandRange(-300.f, 300.f), FMath::FRandRange(-300.f, 300.f), 500.f), true, true);
				if (ABRCharacter* NPC = Cast<ABRCharacter>(Suspect); NPC && NPC->IsNPC())
				{
					NPC->StartWandering(NPC->GetActorLocation(), 1500.f);
					if (NPC->bIllegal)
					{
						NPC->StartFleeing(30.f); // no papers: RUN! (tase them, zip-tie them, haul them off)
					}
				}
			}
		}
		if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
		{
			if (Seized > 0)
			{
				GM->OnContrabandSeized(Interactor, Seized);
			}
			GM->OnVehicleSearched(Interactor, Found + Seized);
		}
		return;
	}
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

// -------------------------------------------------------------------------------------------
// Radio + horn

void ABRCar::EnsureDefaultInput()
{
	const bool bBuildingDefaults = BoatMappingContext == nullptr;
	Super::EnsureDefaultInput();
	if (!bBuildingDefaults)
	{
		return;
	}
	RadioNextAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	RadioPrevAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	RadioOffAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	HornAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	PVVInput::Map(BoatMappingContext, RadioNextAction, EKeys::R);
	PVVInput::Map(BoatMappingContext, RadioNextAction, EKeys::Gamepad_DPad_Right);
	PVVInput::Map(BoatMappingContext, RadioPrevAction, EKeys::T);
	PVVInput::Map(BoatMappingContext, RadioPrevAction, EKeys::Gamepad_DPad_Left);
	PVVInput::Map(BoatMappingContext, RadioOffAction, EKeys::BackSpace);
	PVVInput::Map(BoatMappingContext, RadioOffAction, EKeys::Gamepad_DPad_Down);
	PVVInput::Map(BoatMappingContext, HornAction, EKeys::H);
	PVVInput::Map(BoatMappingContext, HornAction, EKeys::Gamepad_LeftThumbstick);
}

void ABRCar::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		return;
	}
	if (RadioNextAction) Input->BindAction(RadioNextAction, ETriggerEvent::Started, this, &ABRCar::Input_RadioNext);
	if (RadioPrevAction) Input->BindAction(RadioPrevAction, ETriggerEvent::Started, this, &ABRCar::Input_RadioPrev);
	if (RadioOffAction) Input->BindAction(RadioOffAction, ETriggerEvent::Started, this, &ABRCar::Input_RadioOff);
	if (HornAction) Input->BindAction(HornAction, ETriggerEvent::Started, this, &ABRCar::Input_Horn);
}

void ABRCar::Input_RadioNext() { Server_ChangeStation(1, false); }
void ABRCar::Input_RadioPrev() { Server_ChangeStation(-1, false); }
void ABRCar::Input_RadioOff() { Server_ChangeStation(0, true); }
void ABRCar::Input_Horn() { Server_Horn(); }

void ABRCar::Server_ChangeStation_Implementation(int32 Delta, bool bTurnOff)
{
	if (bTurnOff)
	{
		SetRadioStation(-1);
		return;
	}
	const int32 Count = LoadedStations.Num();
	if (Count == 0)
	{
		if (ABRPlayerController* PC = Cast<ABRPlayerController>(GetController()))
		{
			PC->Client_Notify(LOCTEXT("NoSongs", "No songs loaded. Import .wav files into Content/Music/Radio."));
		}
		return;
	}
	const int32 Next = RadioStation < 0 ? (Delta > 0 ? 0 : Count - 1) : (RadioStation + Delta + Count) % Count;
	SetRadioStation(Next);
}

void ABRCar::SetRadioStation(int32 Station)
{
	if (!HasAuthority())
	{
		return;
	}
	RadioStation = LoadedStations.IsValidIndex(Station) ? Station : -1;
	const ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>();
	RadioStartTime = GS ? GS->GetServerWorldTimeSeconds() : 0.f;
	OnRep_Radio();
}

void ABRCar::OnRep_Radio()
{
	if (GetNetMode() == NM_DedicatedServer || !RadioAudio)
	{
		return;
	}
	if (!IsRadioOn())
	{
		RadioAudio->Stop();
		return;
	}
	USoundBase* Song = LoadedStations[RadioStation];
	const ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>();
	const float Duration = Song->GetDuration();
	float Offset = 0.f;
	if (GS && Duration > 1.f && Duration < 3600.f)
	{
		Offset = FMath::Fmod(FMath::Max(0.f, GS->GetServerWorldTimeSeconds() - RadioStartTime), Duration);
	}
	RadioAudio->SetSound(Song);
	RadioAudio->SetVolumeMultiplier(RadioVolume);
	RadioAudio->Play(Offset);
}

void ABRCar::OnRadioFinished()
{
	// Keep the station looping (songs imported without the Looping flag).
	if (IsRadioOn())
	{
		RadioAudio->Play(0.f);
	}
}

FString ABRCar::GetStationName() const
{
	if (!IsRadioOn())
	{
		return TEXT("Radio off");
	}
	return FString::Printf(TEXT("%d/%d  %s"), RadioStation + 1, LoadedStations.Num(), *LoadedStations[RadioStation]->GetName().Replace(TEXT("_"), TEXT(" ")));
}

void ABRCar::Server_Horn_Implementation()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastHornTime < 0.8f)
	{
		return;
	}
	LastHornTime = Now;
	Multicast_Horn();
	if (ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>())
	{
		GS->Multicast_NoisePing(GetActorLocation());
	}
}

void ABRCar::Multicast_Horn_Implementation()
{
	if (HornSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, HornSound, GetActorLocation());
	}
}

// ===========================================================================================
// Truck

ABRTruck::ABRTruck()
{
	PrimaryActorTick.bCanEverTick = true;
	Hull->SetRelativeScale3D(FVector(5.2f, 2.2f, 0.6f)); // chassis 520 x 220 x 60
	MassKg = 1400.f;
	PaintColor = FLinearColor(0.55f, 0.15f, 0.08f);

	// Capsule centres: standing in the open cab and in the bed.
	SeatOffsets = {
		FVector(80.f, -50.f, 125.f),	// driver
		FVector(80.f, 50.f, 125.f),		// shotgun
		FVector(-120.f, -50.f, 125.f),	// bed
		FVector(-200.f, 50.f, 125.f)	// bed
	};
	PontoonOffsets = {
		FVector(170.f, 95.f, -30.f), FVector(170.f, -95.f, -30.f),
		FVector(-170.f, 95.f, -30.f), FVector(-170.f, -95.f, -30.f)
	};

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	auto Part = [this](const TCHAR* Name, UStaticMesh* Mesh, const FVector& At, const FVector& SizeCm, const FRotator& Rot = FRotator::ZeroRotator)
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(SeatRoot);
		C->SetStaticMesh(Mesh);
		C->SetRelativeLocationAndRotation(At, Rot);
		C->SetRelativeScale3D(SizeCm / 100.f);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return C;
	};
	UStaticMesh* CubeMesh = Cube.Object;
	UStaticMesh* CylMesh = Cylinder.Object;

	// Hood + front.
	Part(TEXT("Body_Hood"), CubeMesh, FVector(210.f, 0.f, 55.f), FVector(100.f, 215.f, 50.f));
	Part(TEXT("Trim_Grille"), CubeMesh, FVector(262.f, 0.f, 45.f), FVector(6.f, 160.f, 40.f));
	Part(TEXT("Trim_Bumper"), CubeMesh, FVector(268.f, 0.f, 5.f), FVector(12.f, 225.f, 22.f));
	Part(TEXT("Light_L"), CubeMesh, FVector(264.f, -85.f, 50.f), FVector(6.f, 30.f, 20.f));
	Part(TEXT("Light_R"), CubeMesh, FVector(264.f, 85.f, 50.f), FVector(6.f, 30.f, 20.f));

	// Open cab: doors, dash, pillars and a roof (you can see who's inside).
	Part(TEXT("Body_DoorL"), CubeMesh, FVector(70.f, -105.f, 70.f), FVector(180.f, 10.f, 80.f));
	Part(TEXT("Body_DoorR"), CubeMesh, FVector(70.f, 105.f, 70.f), FVector(180.f, 10.f, 80.f));
	Part(TEXT("Body_Dash"), CubeMesh, FVector(158.f, 0.f, 75.f), FVector(12.f, 210.f, 90.f));
	Part(TEXT("Body_PillarFL"), CubeMesh, FVector(160.f, -102.f, 175.f), FVector(8.f, 8.f, 120.f), FRotator(-12.f, 0.f, 0.f));
	Part(TEXT("Body_PillarFR"), CubeMesh, FVector(160.f, 102.f, 175.f), FVector(8.f, 8.f, 120.f), FRotator(-12.f, 0.f, 0.f));
	Part(TEXT("Body_PillarBL"), CubeMesh, FVector(-18.f, -102.f, 175.f), FVector(8.f, 8.f, 120.f));
	Part(TEXT("Body_PillarBR"), CubeMesh, FVector(-18.f, 102.f, 175.f), FVector(8.f, 8.f, 120.f));
	Part(TEXT("Body_Roof"), CubeMesh, FVector(65.f, 0.f, 238.f), FVector(185.f, 215.f, 8.f));
	Part(TEXT("Body_CabBack"), CubeMesh, FVector(-20.f, 0.f, 85.f), FVector(8.f, 210.f, 110.f));

	// Bed.
	Part(TEXT("Body_BedL"), CubeMesh, FVector(-140.f, -105.f, 60.f), FVector(240.f, 10.f, 60.f));
	Part(TEXT("Body_BedR"), CubeMesh, FVector(-140.f, 105.f, 60.f), FVector(240.f, 10.f, 60.f));
	Part(TEXT("Body_Tailgate"), CubeMesh, FVector(-258.f, 0.f, 60.f), FVector(10.f, 215.f, 60.f));

	// Wheels.
	const FRotator Axle(0.f, 0.f, 90.f);
	Part(TEXT("Wheel_FL"), CylMesh, FVector(170.f, -112.f, -35.f), FVector(95.f, 95.f, 40.f), Axle);
	Part(TEXT("Wheel_FR"), CylMesh, FVector(170.f, 112.f, -35.f), FVector(95.f, 95.f, 40.f), Axle);
	Part(TEXT("Wheel_BL"), CylMesh, FVector(-170.f, -112.f, -35.f), FVector(95.f, 95.f, 40.f), Axle);
	Part(TEXT("Wheel_BR"), CylMesh, FVector(-170.f, 112.f, -35.f), FVector(95.f, 95.f, 40.f), Axle);

	// Driver's bench + steering wheel (spins with your steering).
	Part(TEXT("Body_Bench"), CubeMesh, FVector(55.f, 0.f, 55.f), FVector(60.f, 200.f, 40.f));
	Part(TEXT("Trim_Column"), CylMesh, FVector(140.f, -50.f, 110.f), FVector(6.f, 6.f, 45.f), FRotator(-60.f, 0.f, 0.f));
	SteeringWheel = CreateDefaultSubobject<USceneComponent>(TEXT("SteeringWheel"));
	SteeringWheel->SetupAttachment(SeatRoot);
	SteeringWheel->SetRelativeLocationAndRotation(FVector(122.f, -50.f, 128.f), FRotator(-30.f, 0.f, 0.f));
	auto WheelPart = [&](const TCHAR* Name, UStaticMesh* Mesh, const FVector& SizeCm, const FRotator& Rot)
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(SteeringWheel);
		C->SetStaticMesh(Mesh);
		C->SetRelativeRotation(Rot);
		C->SetRelativeScale3D(SizeCm / 100.f);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	};
	WheelPart(TEXT("Wheel_SteeringRim"), CylMesh, FVector(42.f, 42.f, 4.f), FRotator(90.f, 0.f, 0.f));
	WheelPart(TEXT("Trim_SteeringSpoke"), CubeMesh, FVector(3.f, 40.f, 5.f), FRotator::ZeroRotator);

	// Headlights (night driving - and they make you easy to spot).
	USpotLightComponent* Beam = CreateDefaultSubobject<USpotLightComponent>(TEXT("Headlights"));
	Beam->SetupAttachment(SeatRoot);
	Beam->SetRelativeLocation(FVector(270.f, 0.f, 50.f));
	Beam->SetIntensity(30000.f);
	Beam->SetAttenuationRadius(4500.f);
	Beam->SetOuterConeAngle(35.f);
	Beam->SetInnerConeAngle(15.f);
	Beam->SetCastShadows(false);
	Beam->SetMobility(EComponentMobility::Movable);
}

void ABRTruck::StartNPCRoute(const TArray<FVector>& InRoute, int32 InCheckpointIndex, AActor* InGate)
{
	bNPCDriven = true;
	Route = InRoute;
	RouteIndex = 0;
	CheckpointIndex = InCheckpointIndex;
	Gate = InGate;
}

bool ABRTruck::AddNPC(APVVCharacter* NPC)
{
	if (!HasAuthority() || !NPC)
	{
		return false;
	}
	const int32 Seat = (Occupants.Num() > 0 && !Occupants[0]) ? 0 : FindFreePassengerSeat();
	if (Seat == INDEX_NONE)
	{
		return false;
	}
	SeatCharacter(NPC, Seat);
	SpawnedNPCs.Add(NPC);
	return true;
}

void ABRTruck::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (SteeringWheel && GetNetMode() != NM_DedicatedServer)
	{
		FRotator Rot = SteeringWheel->GetRelativeRotation();
		Rot.Roll = FMath::FInterpTo(Rot.Roll, SteerInput * SteeringWheelTurn, DeltaSeconds, 8.f);
		SteeringWheel->SetRelativeRotation(Rot);
	}
	if (HasAuthority() && bNPCDriven)
	{
		DriveNPC(DeltaSeconds);
		// Driver got dragged out and nobody's driving: the truck gets towed away after a while.
		const float Now = GetWorld()->GetTimeSeconds();
		if (GetDriver())
		{
			NoDriverSince = -1.f;
		}
		else if (NoDriverSince < 0.f)
		{
			NoDriverSince = Now;
		}
		else if (Now - NoDriverSince > 40.f)
		{
			Destroy();
		}
	}
}

void ABRTruck::DriveNPC(float DeltaSeconds)
{
	const ABRCharacter* Driver = Cast<ABRCharacter>(GetDriver());
	if (!Driver || !Driver->IsNPC() || !Route.IsValidIndex(RouteIndex))
	{
		ThrottleInput = 0.f; // driver got pulled out: the truck just stops
		SteerInput = 0.f;
		return;
	}

	const FVector ToTarget = FVector(Route[RouteIndex] - GetActorLocation()) * FVector(1.f, 1.f, 0.f);
	const float Distance = ToTarget.Size();
	const float Now = GetWorld()->GetTimeSeconds();

	if (RouteIndex == CheckpointIndex && Distance < 500.f)
	{
		ThrottleInput = 0.f;
		SteerInput = 0.f;
		if (CheckpointArrivalTime < 0.f)
		{
			CheckpointArrivalTime = Now;
		}
		ABRCheckpointGate* CheckGate = Cast<ABRCheckpointGate>(Gate.Get());
		// Border Patrol must walk up and scan EVERY person aboard. Only a clean truck goes through.
		bool bAllScanned = true;
		bool bAnySuspect = false;
		ABRCharacter* Inspector = nullptr;
		for (APVVCharacter* Occupant : Occupants)
		{
			if (ABRCharacter* BRC = Cast<ABRCharacter>(Occupant))
			{
				if (BRC->LastScannedTime < CheckpointArrivalTime)
				{
					bAllScanned = false;
				}
				else if (!Inspector)
				{
					Inspector = BRC->ScannedBy.Get();
				}
				bAnySuspect |= BRC->IsSuspect();
			}
		}
		const bool bCleared = bAllScanned && !bAnySuspect;
		const bool bFedUp = Now - CheckpointArrivalTime > MaxCheckpointWait;
		if (!bCheckpointDone && (bCleared || bFedUp || !CheckGate))
		{
			bCheckpointDone = true;
			if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
			{
				if (bCleared && CheckGate)
				{
					GM->OnTruckCleared(Inspector);
				}
				else if (bFedUp)
				{
					GM->OnTruckForcedThrough();
				}
			}
		}
		if (bCheckpointDone)
		{
			if (CheckGate && !CheckGate->IsOpen())
			{
				CheckGate->OpenFor(8.f);
				return; // wait for the barrier to lift
			}
			++RouteIndex;
		}
		return;
	}
	if (Distance < 400.f)
	{
		++RouteIndex;
		if (!Route.IsValidIndex(RouteIndex))
		{
			Destroy(); // reached the far side
		}
		return;
	}

	const float Delta = FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, ToTarget.Rotation().Yaw);
	SteerInput = FMath::Clamp(Delta / 35.f, -1.f, 1.f);
	const bool bNearCheckpoint = RouteIndex == CheckpointIndex && Distance < 2000.f;
	ThrottleInput = FMath::Clamp(0.75f * (1.f - FMath::Abs(Delta) / 120.f), 0.25f, bNearCheckpoint ? 0.35f : 0.75f);
}

void ABRTruck::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason); // ejects everyone first
	if (HasAuthority() && EndPlayReason == EEndPlayReason::Destroyed)
	{
		for (const TWeakObjectPtr<APVVCharacter>& NPC : SpawnedNPCs)
		{
			if (NPC.IsValid())
			{
				NPC->Destroy();
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
