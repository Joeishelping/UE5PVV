#include "BRCar.h"
#include "BRPolice.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
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
#include "Components/TextRenderComponent.h"
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

// Nearest Border Patrol player (not an NPC) within Range of Where.
static ABRCharacter* BRNearestCop(UWorld* World, const FVector& Where, float Range)
{
	ABRCharacter* Cop = nullptr;
	float Closest = Range;
	for (TActorIterator<ABRCharacter> It(World); It; ++It)
	{
		const float D = FVector::Dist(It->GetActorLocation(), Where);
		if (It->IsWarden() && !It->IsNPC() && It->GetCondition() == EPVVCondition::Normal && D < Closest)
		{
			Closest = D;
			Cop = *It;
		}
	}
	return Cop;
}

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

	PriceTag = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PriceTag"));
	PriceTag->SetupAttachment(SeatRoot);
	PriceTag->SetRelativeLocation(FVector(0.f, 0.f, 330.f));
	PriceTag->SetHorizontalAlignment(EHTA_Center);
	PriceTag->SetWorldSize(42.f);
	PriceTag->SetTextRenderColor(FColor(120, 255, 120));
	PriceTag->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PriceTag->SetHiddenInGame(true);
}

void ABRCar::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRCar, FlatTyresUntil);
	DOREPLIFETIME(ABRCar, Fuel);
	DOREPLIFETIME(ABRCar, RadioStation);
	DOREPLIFETIME(ABRCar, RadioStartTime);
	DOREPLIFETIME(ABRCar, PaintColor);
	DOREPLIFETIME(ABRCar, bForSale);
	DOREPLIFETIME(ABRCar, SalePrice);
	DOREPLIFETIME(ABRCar, OwnerPlayer);
}

void ABRCar::OnRep_ForSale()
{
	if (PriceTag)
	{
		PriceTag->SetText(FText::FromString(FString::Printf(TEXT("FOR SALE\n%s  $%d"), *GetModelName().ToUpper(), SalePrice)));
		PriceTag->SetHiddenInGame(!bForSale);
	}
}

bool ABRCar::WantsBuy(const APVVCharacter* Interactor) const
{
	return bForSale && Interactor && Interactor->GetCondition() == EPVVCondition::Normal && !Interactor->GetCurrentBoat() && !Interactor->GetCarriedCharacter();
}

void ABRCar::BeginPlay()
{
	Super::BeginPlay();
	if (bInfiniteFuel && HasAuthority())
	{
		Fuel = 1.f;
	}
	OnRep_ForSale();

	// Camera: the car body never blocks the camera (otherwise the camera snaps INSIDE the cab), and a steadier,
	// higher chase view that clears the roof.
	{
		TArray<UPrimitiveComponent*> Parts;
		GetComponents<UPrimitiveComponent>(Parts);
		for (UPrimitiveComponent* Part : Parts)
		{
			Part->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		}
		if (CameraBoom)
		{
			CameraBoom->TargetArmLength = 1050.f;
			CameraBoom->SocketOffset = FVector(0.f, 0.f, 320.f);
			CameraBoom->ProbeSize = 20.f;
			CameraBoom->bEnableCameraLag = true;
			CameraBoom->CameraLagSpeed = 9.f;
			CameraBoom->bEnableCameraRotationLag = true;
			CameraBoom->CameraRotationLagSpeed = 10.f;
			CameraBoom->bInheritRoll = false;
		}
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
	if (IsLocallyControlled() && CameraBoom)
	{
		// Aiming out of the window: pull the camera in over the cab so you can line up a shot.
		const float Default = 1050.f; // (set in BeginPlay)
		CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, bDriveAiming ? Default * 0.55f : Default, DeltaSeconds, 6.f);
		if (bDriveAiming && !Cast<ABRCharacter>(GetDriver()))
		{
			bDriveAiming = false;
		}
	}
	if (HasAuthority() && !bInfiniteFuel && GetDriver() && Fuel > 0.f)
	{
		Fuel = FMath::Max(0.f, Fuel - FMath::Abs(ThrottleInput) * FuelPerSecond * DeltaSeconds);
	}
	if (HasAuthority())
	{
		TickRunOver();
	}
}

void ABRCar::TickRunOver()
{
	UStaticMeshComponent* Body = GetHull();
	if (!Body || !Body->IsSimulatingPhysics())
	{
		return;
	}
	const FVector Vel = Body->GetPhysicsLinearVelocity();
	const float Speed = Vel.Size2D();
	if (Speed < RunOverSpeed)
	{
		return;
	}
	const FVector Dir = Vel.GetSafeNormal2D();
	const FBoxSphereBounds Local = Body->CalcBounds(FTransform::Identity);
	const FVector Scale = Body->GetComponentScale();
	const FVector Extent = Local.BoxExtent * Scale + FVector(55.f, 55.f, 90.f);
	const FVector Center = Body->GetComponentTransform().TransformPosition(Local.Origin);
	const float Now = GetWorld()->GetTimeSeconds();
	APVVCharacter* Driver = GetDriver();
	for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
	{
		ABRCharacter* C = *It;
		if (C->GetCurrentBoat() || C->GetCarrier() || C->IsInCannon() || C->GetCondition() == EPVVCondition::Detained
			|| C->GetAttachParentActor() == this || (C->GetMovementBase() && C->GetMovementBase()->GetOwner() == this))
		{
			continue; // riding in / on this vehicle (box truck cargo hold)
		}
		const FVector Rel = Body->GetComponentQuat().UnrotateVector(C->GetActorLocation() - Center);
		if (FMath::Abs(Rel.X) > Extent.X || FMath::Abs(Rel.Y) > Extent.Y || FMath::Abs(Rel.Z) > Extent.Z)
		{
			continue;
		}
		if (FVector::DotProduct(C->GetActorLocation() - Center, Dir) < 0.f)
		{
			continue; // only the end that's moving into them
		}
		const float* Last = LastRunOver.Find(C);
		if (Last && Now - *Last < 1.5f)
		{
			continue;
		}
		LastRunOver.Add(C, Now);
		// Get out of the way of the car (so it doesn't stop dead against them) and go FLYING. No damage - just knocked down.
		C->Multicast_IgnoreVehicles(static_cast<uint8>(Body->GetCollisionObjectType()), 1.2f);
		const FVector Launch = Dir * Speed * RunOverLaunch + FVector(0.f, 0.f, 250.f + Speed * 0.15f);
		C->LastTosser = Cast<ABRCharacter>(Driver);
		C->LastTossTime = Now;
		if (C->GetCondition() == EPVVCondition::Normal)
		{
			C->ApplyKnockdown(Driver ? static_cast<AActor*>(Driver) : this, Launch);
		}
		else
		{
			C->Multicast_FlingBody(Launch * 0.7f); // already down: tumble under the wheels
		}
		Body->SetPhysicsLinearVelocity(Vel * 0.9f); // THUMP
		ABRGameState::FX(this, EBRFX::Smoke, C->GetActorLocation());
		if (Driver)
		{
			ABRGameState::Highlight(this, C, 65.f, FString::Printf(TEXT("%s ran over %s"), *Driver->GetPlayerDisplayName(), *C->GetPlayerDisplayName()));
			if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
			{
				GM->Feed(FString::Printf(TEXT("%s ran over %s!"), *Driver->GetPlayerDisplayName(), *C->GetPlayerDisplayName()));
			}
		}
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

void ABRCar::PopTyres(float Seconds)
{
	if (const ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>())
	{
		FlatTyresUntil = GS->GetServerWorldTimeSeconds() + Seconds;
		ForceNetUpdate();
	}
	ABRGameState::FX(this, EBRFX::PointsText, GetActorLocation() + FVector(0.f, 0.f, 220.f), TEXT("FLAT TYRES!"));
}

bool ABRCar::HasFlatTyres() const
{
	const ABRGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABRGameState>() : nullptr;
	return GS && GS->GetServerWorldTimeSeconds() < FlatTyresUntil;
}

float ABRCar::GetDriveScale() const
{
	if (HasFlatTyres())
	{
		return IsFlooded() ? 0.f : 0.25f; // limping along on the rims
	}
	if (IsFlooded())
	{
		return 0.f; // engine drowned - you needed a boat
	}
	return (bInfiniteFuel || Fuel > 0.f) ? 1.f : 0.f;
}

bool ABRCar::CanDrive(const APVVCharacter* Character) const
{
	if (bForSale)
	{
		return false; // buy it first
	}
	if (OwnerPlayer)
	{
		const APlayerState* PS = Character ? Character->GetPlayerState() : nullptr;
		const ABRPlayerState* BRPS = Cast<ABRPlayerState>(PS);
		const ABRPlayerState* OwnerBR = Cast<ABRPlayerState>(OwnerPlayer);
		return PS == OwnerPlayer || (BRPS && OwnerBR && BRPS->GetFaction() == OwnerBR->GetFaction());
	}
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
	const ABRTruck* SelfTruck = Cast<ABRTruck>(this);
	const bool bPatrolVehicle = bInfiniteFuel && !(SelfTruck && SelfTruck->IsNPCTruck()); // Border Patrol's own trucks
	if (!Interactor || Interactor->GetFaction() != BR::Wardens || bPatrolVehicle || Interactor->GetCurrentBoat()
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

bool ABRCar::WantsDrive(const APVVCharacter* Interactor) const
{
	return Interactor && !bForSale && !Interactor->GetCarriedCharacter() && !Interactor->GetCurrentBoat()
		&& Interactor->GetCondition() == EPVVCondition::Normal && Occupants.Num() > 0 && !Occupants[0] && CanDrive(Interactor)
		// Border Patrol at a suspect vehicle: SEARCH it first (E), drive it after.
		&& !WantsSearch(Interactor) && !WantsCargoSearch(Interactor);
}

bool ABRCar::WantsCargoSearch(const APVVCharacter* Interactor) const
{
	const ABRTruck* SelfTruck = Cast<ABRTruck>(this);
	const bool bPatrolVehicle = bInfiniteFuel && !(SelfTruck && SelfTruck->IsNPCTruck());
	return Interactor && Interactor->GetFaction() == BR::Wardens && !bPatrolVehicle && !bForSale && !bCargoSearched
		&& Interactor->GetCondition() == EPVVCondition::Normal && !Interactor->GetCurrentBoat() && !Interactor->GetCarriedCharacter()
		&& !HasFreePeopleAboard(nullptr);
}

bool ABRCar::HasFreePeopleAboard(const APVVCharacter* Except) const
{
	for (const APVVCharacter* Occupant : Occupants)
	{
		if (Occupant && Occupant != Except && Occupant->GetCondition() == EPVVCondition::Normal)
		{
			return true;
		}
	}
	return false;
}

bool ABRCar::TakePrisonerOut(APVVCharacter* Officer)
{
	if (!HasAuthority())
	{
		return false;
	}
	if (APVVCharacter* Prisoner = FindPrisonerToUnload(Officer))
	{
		Officer->StartCarrying(Prisoner); // out of the seat, onto your shoulder
		return true;
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
	if (bForSale)
	{
		return WantsBuy(Interactor);
	}
	if (WantsRefuel(Interactor) && Interactor->GetCondition() == EPVVCondition::Normal && !Interactor->GetCarriedCharacter())
	{
		return true; // holding a gas can next to a thirsty car: pour it in (hold E)
	}
	if (WantsDrive(Interactor))
	{
		return true; // (prisoners in the back come along for the ride - O takes one out)
	}
	if (FindPrisonerToUnload(Interactor))
	{
		return true;
	}
	if (WantsSearch(Interactor) || WantsCargoSearch(Interactor))
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
	if (WantsBuy(Interactor)) return 1.f;
	if (WantsRefuel(Interactor)) return 2.f;
	if (WantsDrive(Interactor)) return 0.f;
	if (FindPrisonerToUnload(Interactor)) return 0.6f;
	if (WantsSearch(Interactor)) return 2.5f;
	if (WantsCargoSearch(Interactor)) return 3.f;
	if (WantsRefuel(Interactor)) return 2.f;
	return 0.f;
}

FText ABRCar::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	if (WantsBuy(Interactor))
	{
		return FText::Format(LOCTEXT("BuyCar", "BUY this {0} - ${1} (hold)"), FText::FromString(GetModelName()), SalePrice);
	}
	if (WantsRefuel(Interactor))
	{
		return LOCTEXT("Refuel", "Pour in gas (hold)");
	}
	if (WantsDrive(Interactor) && FindPrisonerToUnload(Interactor))
	{
		return GetDriveScale() > 0.f ? LOCTEXT("DriveWithPrisoner", "Drive (prisoner in the back) - O = take them out")
			: LOCTEXT("DriveWithPrisonerNoGas", "Get in (TANK EMPTY) - O = take the prisoner out");
	}
	if (const APVVCharacter* Prisoner = FindPrisonerToUnload(Interactor))
	{
		return FText::Format(LOCTEXT("Unload", "Take {0} out (carry them to the pen or the catapult)"), FText::FromString(Prisoner->GetPlayerDisplayName()));
	}
	if (WantsSearch(Interactor))
	{
		return LOCTEXT("Search", "Order the suspects OUT (press O, or hold E)");
	}
	if (WantsCargoSearch(Interactor))
	{
		return LOCTEXT("SearchCargo", "SEARCH the vehicle for hidden contraband (hold)");
	}
	if (WantsRefuel(Interactor))
	{
		return LOCTEXT("RefuelB", "Pour in gas (hold)");
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

bool ABRCar::OrderOut(APVVCharacter* Interactor)
{
	if (!HasAuthority())
	{
		return false;
	}
	// Only zip-tied prisoners aboard (no free suspects): O pulls one out onto your shoulder.
	if (!HasFreePeopleAboard(Interactor) && TakePrisonerOut(Interactor))
	{
		return true;
	}
	if (!WantsSearch(Interactor))
	{
		return false;
	}
		// Pull out the suspects: Runners, people with no papers, people with contraband.
		const ABRTruck* AsTruck = Cast<ABRTruck>(this);
		const bool bNPCTruck = AsTruck && AsTruck->IsNPCTruck();
		int32 Found = 0;
		int32 Seized = 0;
		TArray<APVVCharacter*> Out;
		TSet<APVVCharacter*> Minor; // only had contraband (papers fine)
		bool bDriverDirty = false;
		for (APVVCharacter* Occupant : Occupants)
		{
			ABRCharacter* BRC = Cast<ABRCharacter>(Occupant);
			if (!BRC || BRC->GetCondition() != EPVVCondition::Normal)
			{
				continue;
			}
			bool bDirty = false;
			if (BRC->bCarriesContraband)
			{
				BRC->bCarriesContraband = false;
				++Seized;
				bDirty = true;
				if (!BRC->bIllegal)
				{
					Minor.Add(BRC);
				}
			}
			if (BRC->IsSuspect())
			{
				++Found;
				bDirty = true;
			}
			if (bDirty)
			{
				Out.Add(BRC); // the clean, legal ones just stay in their seats
				bDriverDirty |= Occupant == GetDriver();
			}
		}
		// A dirty driver might just FLOOR IT instead of getting out.
		ABRTruck* NPCTruck = bNPCTruck ? const_cast<ABRTruck*>(AsTruck) : nullptr;
		if (NPCTruck && bDriverDirty && FMath::FRand() < 0.1f)
		{
			NPCTruck->FloorIt();
			Out.Reset();
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
				Suspect->LaunchCharacter(FVector(FMath::FRandRange(-150.f, 150.f), FMath::FRandRange(-150.f, 150.f), 300.f), true, true);
				if (ABRCharacter* NPC = Cast<ABRCharacter>(Suspect); NPC && NPC->IsNPC())
				{
					// Fight, knife, run or give up - random, worse for people with no papers.
					NPC->ReactToBust(Cast<ABRCharacter>(Interactor), Minor.Contains(Suspect));
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
		return true;
}

void ABRCar::OnInteract_Implementation(APVVCharacter* Interactor)
{
	if (!HasAuthority() || !Interactor)
	{
		return;
	}
	if (WantsBuy(Interactor))
	{
		ABRPlayerState* PS = Cast<ABRPlayerState>(Interactor->GetPVVPlayerState());
		ABRCharacter* Buyer = Cast<ABRCharacter>(Interactor);
		if (!PS || PS->Money < SalePrice)
		{
			if (Buyer)
			{
				Buyer->Notify(FText::Format(LOCTEXT("CarBroke", "You need ${0} for this one."), SalePrice));
			}
			return;
		}
		PS->AddMoney(-SalePrice);
		bForSale = false;
		OwnerPlayer = PS;
		Fuel = 1.f;
		OnRep_ForSale();
		ForceNetUpdate();
		ABRGameState::Sfx(this, EBRSfx::Buy, GetActorLocation());
		if (Buyer)
		{
			Buyer->Notify(FText::Format(LOCTEXT("CarBought", "SOLD! The {0} is yours (your team can drive it too). Full tank."), FText::FromString(GetModelName())));
		}
		if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
		{
			GM->Feed(FString::Printf(TEXT("%s bought a %s"), *Interactor->GetPlayerDisplayName(), *GetModelName()));
		}
		return;
	}
	if (WantsRefuel(Interactor))
	{
		if (ABRPlayerState* PS = Cast<ABRPlayerState>(Interactor->GetPVVPlayerState()); PS && PS->RemoveItem(EBRItem::GasCan))
		{
			Fuel = FMath::Min(1.f, Fuel + 0.5f);
			ABRGameState::FX(this, EBRFX::PointsText, GetActorLocation() + FVector(0.f, 0.f, 200.f), TEXT("+GAS"));
		}
		return;
	}
	if (WantsDrive(Interactor))
	{
		Super::OnInteract_Implementation(Interactor); // hop in the driver's seat (prisoners stay in the back)
		return;
	}
	if (APVVCharacter* Prisoner = FindPrisonerToUnload(Interactor))
	{
		Interactor->StartCarrying(Prisoner); // pulls them out of the seat onto your shoulder
		return;
	}
	if (WantsSearch(Interactor))
	{
		OrderOut(Interactor);
		return;
	}
	if (WantsCargoSearch(Interactor))
	{
		bCargoSearched = true;
		ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>();
		ABRCharacter* Cop = Cast<ABRCharacter>(Interactor);
		if (bHiddenCargo)
		{
			bHiddenCargo = false;
			const int32 Found = FMath::RandRange(1, 2);
			ABRGameState::FX(this, EBRFX::PointsText, GetActorLocation() + FVector(0.f, 0.f, 220.f), TEXT("STASH FOUND!"));
			ABRGameState::Sfx(this, EBRSfx::ScanAlarm, GetActorLocation());
			if (Cop)
			{
				Cop->Notify(FText::Format(LOCTEXT("StashFound", "Found {0} hidden package(s) in the vehicle! Seized."), Found));
			}
			if (GM)
			{
				GM->OnContrabandSeized(Interactor, Found);
				GM->Feed(FString::Printf(TEXT("%s found a hidden stash in a vehicle"), *Interactor->GetPlayerDisplayName()));
			}
		}
		else
		{
			if (Cop)
			{
				Cop->Notify(LOCTEXT("StashNone", "Nothing hidden in this one."));
			}
			// (an empty vehicle search is routine - only ordering clean people OUT counts as harassment)
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
	// Shooting out of the window while you drive: hold right mouse to aim, left click to fire.
	DriveAimAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	DriveFireAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	PVVInput::Map(BoatMappingContext, DriveAimAction, EKeys::RightMouseButton);
	PVVInput::Map(BoatMappingContext, DriveAimAction, EKeys::Gamepad_LeftTrigger);
	PVVInput::Map(BoatMappingContext, DriveFireAction, EKeys::LeftMouseButton);
	PVVInput::Map(BoatMappingContext, DriveFireAction, EKeys::Gamepad_RightTrigger);
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
	if (DriveAimAction)
	{
		Input->BindAction(DriveAimAction, ETriggerEvent::Started, this, &ABRCar::Input_DriveAimStart);
		Input->BindAction(DriveAimAction, ETriggerEvent::Completed, this, &ABRCar::Input_DriveAimStop);
	}
	if (DriveFireAction) Input->BindAction(DriveFireAction, ETriggerEvent::Started, this, &ABRCar::Input_DriveFire);
}

void ABRCar::Input_DriveAimStart()
{
	const ABRCharacter* Driver = Cast<ABRCharacter>(GetDriver());
	bDriveAiming = Driver && Driver->CanShootFromVehicle();
}

void ABRCar::Input_DriveAimStop()
{
	bDriveAiming = false;
}

void ABRCar::Input_DriveFire()
{
	if (!bDriveAiming || !Camera)
	{
		return;
	}
	// Whatever's under the crosshair (not your own car).
	const FVector From = Camera->GetComponentLocation();
	const FVector To = From + Camera->GetForwardVector() * 9000.f;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BRDriveAim), false, this);
	if (APVVCharacter* Driver = GetDriver())
	{
		Params.AddIgnoredActor(Driver);
	}
	const FVector AimPoint = GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params) ? FVector(Hit.ImpactPoint) : To;
	Server_DriverFire(AimPoint);
}

void ABRCar::Server_DriverFire_Implementation(FVector_NetQuantize AimPoint)
{
	if (ABRCharacter* Driver = Cast<ABRCharacter>(GetDriver()))
	{
		Driver->FireFromVehicle(AimPoint);
	}
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
	RadioAudio->SetVolumeMultiplier(RadioVolume * FPVVPlayerOptions::MusicVolume);
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
		BRAudio::PlayAt(this, HornSound, GetActorLocation(), BRAudio::Loud);
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

	// Extra body parts for the other car models (hidden on the pickup - see ApplyModel).
	// Van back: lower walls + a roof on pillars, with big open windows so Border Patrol can see who's inside.
	Part(TEXT("Body_VanBox"), CubeMesh, FVector(-140.f, 0.f, 85.f), FVector(250.f, 218.f, 100.f));
	Part(TEXT("Body_VanTop"), CubeMesh, FVector(-140.f, 0.f, 240.f), FVector(250.f, 218.f, 12.f));
	Part(TEXT("Body_VanPillarBL"), CubeMesh, FVector(-260.f, -104.f, 188.f), FVector(10.f, 10.f, 106.f));
	Part(TEXT("Body_VanPillarBR"), CubeMesh, FVector(-260.f, 104.f, 188.f), FVector(10.f, 10.f, 106.f));
	Part(TEXT("Body_VanPillarML"), CubeMesh, FVector(-140.f, -104.f, 188.f), FVector(10.f, 10.f, 106.f));
	Part(TEXT("Body_VanPillarMR"), CubeMesh, FVector(-140.f, 104.f, 188.f), FVector(10.f, 10.f, 106.f));
	Part(TEXT("Body_VanRoof"), CubeMesh, FVector(65.f, 0.f, 240.f), FVector(185.f, 218.f, 12.f));
	Part(TEXT("Body_Trunk"), CubeMesh, FVector(-185.f, 0.f, 70.f), FVector(150.f, 212.f, 50.f));
	Part(TEXT("Body_RearRoof"), CubeMesh, FVector(-60.f, 0.f, 238.f), FVector(90.f, 215.f, 8.f));
	Part(TEXT("Body_RearPillarL"), CubeMesh, FVector(-105.f, -102.f, 170.f), FVector(8.f, 8.f, 130.f));
	Part(TEXT("Body_RearPillarR"), CubeMesh, FVector(-105.f, 102.f, 170.f), FVector(8.f, 8.f, 130.f));
	Part(TEXT("Trim_Spoiler"), CubeMesh, FVector(-245.f, 0.f, 130.f), FVector(30.f, 210.f, 6.f));
	Part(TEXT("Trim_SpoilerPostL"), CubeMesh, FVector(-245.f, -80.f, 108.f), FVector(6.f, 6.f, 40.f));
	Part(TEXT("Trim_SpoilerPostR"), CubeMesh, FVector(-245.f, 80.f, 108.f), FVector(6.f, 6.f, 40.f));
	Part(TEXT("Trim_HoodScoop"), CubeMesh, FVector(200.f, 0.f, 85.f), FVector(60.f, 50.f, 14.f));

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

void ABRTruck::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRTruck, Model);
	DOREPLIFETIME(ABRTruck, BribeOffer);
}

FString ABRTruck::GetModelName() const
{
	switch (Model)
	{
	case EBRCarModel::Sedan: return TEXT("Sedan");
	case EBRCarModel::Lowrider: return TEXT("Lowrider");
	case EBRCarModel::Van: return TEXT("Van");
	default: return TEXT("Pickup Truck");
	}
}

void ABRTruck::BeginPlay()
{
	ApplyModel(); // seats must be set before the boat code sizes the seat list
	Super::BeginPlay();
}

void ABRTruck::ApplyModel()
{
	const bool bVan = Model == EBRCarModel::Van;
	const bool bSedan = Model == EBRCarModel::Sedan;
	const bool bLow = Model == EBRCarModel::Lowrider;
	auto Show = [this](const TCHAR* Name, bool bOn)
	{
		TArray<UStaticMeshComponent*> Meshes;
		GetComponents<UStaticMeshComponent>(Meshes);
		for (UStaticMeshComponent* M : Meshes)
		{
			if (M->GetName() == Name)
			{
				M->SetVisibility(bOn);
			}
		}
	};
	Show(TEXT("Body_VanBox"), bVan);
	for (const TCHAR* VanPart : { TEXT("Body_VanTop"), TEXT("Body_VanPillarBL"), TEXT("Body_VanPillarBR"), TEXT("Body_VanPillarML"), TEXT("Body_VanPillarMR") })
	{
		Show(VanPart, bVan);
	}
	Show(TEXT("Body_VanRoof"), bVan);
	Show(TEXT("Body_Roof"), !bVan);
	Show(TEXT("Body_Trunk"), bSedan || bLow);
	Show(TEXT("Body_RearRoof"), bSedan);
	Show(TEXT("Body_RearPillarL"), bSedan);
	Show(TEXT("Body_RearPillarR"), bSedan);
	Show(TEXT("Body_CabBack"), !bSedan);
	Show(TEXT("Trim_Spoiler"), bLow);
	Show(TEXT("Trim_SpoilerPostL"), bLow);
	Show(TEXT("Trim_SpoilerPostR"), bLow);
	Show(TEXT("Trim_HoodScoop"), bLow);
	for (const TCHAR* Bed : { TEXT("Body_BedL"), TEXT("Body_BedR"), TEXT("Body_Tailgate") })
	{
		Show(Bed, Model == EBRCarModel::Pickup);
	}
	if (!HasAuthority())
	{
		return;
	}
	// Seats + handling.
	const ABRTruck* Defaults = GetClass()->GetDefaultObject<ABRTruck>();
	ForwardAcceleration = Defaults->ForwardAcceleration;
	MassKg = Defaults->MassKg;
	RideHeight = Defaults->RideHeight;
	switch (Model)
	{
	case EBRCarModel::Sedan:
		SeatOffsets = { FVector(80.f, -50.f, 125.f), FVector(80.f, 50.f, 125.f), FVector(-70.f, -50.f, 125.f), FVector(-70.f, 50.f, 125.f) };
		ForwardAcceleration *= 1.15f;
		MassKg *= 0.85f;
		break;
	case EBRCarModel::Lowrider:
		SeatOffsets = { FVector(80.f, -50.f, 115.f), FVector(80.f, 50.f, 115.f) };
		ForwardAcceleration *= 1.5f; // FAST
		MassKg *= 0.8f;
		RideHeight *= 0.75f;
		break;
	case EBRCarModel::Van:
		// Six seats: two up front, four in the back (visible through the windows).
		SeatOffsets = { FVector(80.f, -50.f, 125.f), FVector(80.f, 50.f, 125.f), FVector(-70.f, -50.f, 125.f), FVector(-70.f, 50.f, 125.f),
			FVector(-190.f, -50.f, 125.f), FVector(-190.f, 50.f, 125.f) };
		ForwardAcceleration *= 0.8f;
		MassKg *= 1.3f;
		break;
	default:
		break; // pickup: the constructor's 4 seats
	}
}

void ABRTruck::StartNPCRoute(const TArray<FVector>& InRoute, int32 InCheckpointIndex, AActor* InGate)
{
	bNPCDriven = true;
	// This driver's personality (speed and how wobbly).
	DriverSpeedTaste = FMath::FRandRange(0.72f, 1.25f);
	DriverWobble = FMath::FRand() < 0.25f ? FMath::FRandRange(0.06f, 0.14f) : FMath::FRandRange(0.f, 0.05f);
	DriverWobbleRate = FMath::FRandRange(0.4f, 1.1f);
	DriverPhase = FMath::FRandRange(0.f, 6.28f);
	Route = InRoute;
	RouteIndex = 0;
	CheckpointIndex = InCheckpointIndex;
	Gate = InGate;
	Lane = -1.f;
	// Start in whichever lane is emptier near the start.
	if (Route.Num() > 1 && IsLaneBlocked(-1.f, 1500.f, 600.f) && !IsLaneBlocked(1.f, 1500.f, 600.f))
	{
		Lane = 1.f;
	}
}

FVector ABRTruck::LaneTarget(int32 Index, float InLane) const
{
	if (!Route.IsValidIndex(Index))
	{
		return GetActorLocation();
	}
	// Direction of the road at this point (from the previous point, or toward the next).
	FVector Dir = Index > 0 ? Route[Index] - Route[Index - 1] : (Route.IsValidIndex(Index + 1) ? Route[Index + 1] - Route[Index] : GetActorForwardVector());
	Dir = (Dir * FVector(1.f, 1.f, 0.f)).GetSafeNormal();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Dir);
	return Route[Index] + Right * InLane * LaneOffset;
}

bool ABRTruck::IsLaneBlocked(float InLane, float Range, float BehindToo) const
{
	const FVector Fwd = GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Fwd);
	// Where my lane runs, relative to me right now.
	const FVector Me = GetActorLocation();
	for (TActorIterator<APVVBoat> It(GetWorld()); It; ++It)
	{
		if (*It == this)
		{
			continue;
		}
		const FVector To = (It->GetActorLocation() - Me) * FVector(1.f, 1.f, 0.f);
		const float Ahead = FVector::DotProduct(To, Fwd);
		if (Ahead < -BehindToo || Ahead > Range)
		{
			continue;
		}
		// Sideways distance from the middle of the lane we're asking about.
		const float MyLateral = 0.f;
		const float LaneShift = (InLane - Lane) * LaneOffset;
		const float Side = FVector::DotProduct(To, Right) - (MyLateral + LaneShift);
		if (FMath::Abs(Side) < 210.f && (Ahead > 0.f || BehindToo > 0.f))
		{
			return true;
		}
	}
	return false;
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
		if (ReactAt > 0.f && !bReacted && GetWorld()->GetTimeSeconds() >= ReactAt)
		{
			bReacted = true;
			SuspectsReact(); // somebody aboard got scanned as illegal: they panic
		}
		DriveNPC(DeltaSeconds);
		TickDriverTakeover(GetWorld()->GetTimeSeconds());
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

	const FVector ToTarget = FVector(LaneTarget(RouteIndex, Lane) - GetActorLocation()) * FVector(1.f, 1.f, 0.f);
	const float Distance = ToTarget.Size();
	const float Now = GetWorld()->GetTimeSeconds();

	// Running the checkpoint: pedal to the metal, nothing stops us.
	if (bRunningGate)
	{
		ABRCheckpointGate* RamGate = Cast<ABRCheckpointGate>(Gate.Get());
		if (RamGate && !RamGate->IsOpen() && FVector::Dist2D(RamGate->GetActorLocation(), GetActorLocation()) < 650.f)
		{
			RamGate->Smash(this); // CRUNCH
		}
		if (Distance < 400.f)
		{
			++RouteIndex;
			if (!Route.IsValidIndex(RouteIndex))
			{
				bReachedFarSide = true;
				Destroy();
			}
			return;
		}
		SteerInput = FMath::Clamp(FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, ToTarget.Rotation().Yaw) / 30.f, -1.f, 1.f);
		ThrottleInput = 1.f;
		return;
	}

	if (RouteIndex == CheckpointIndex && Distance < 450.f)
	{
		// Pull up and STOP - stand on the brakes, don't coast into the barrier.
		ThrottleInput = GetVelocity().Size2D() > 40.f ? -0.7f : 0.f;
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
		// A dirty driver might try to buy their way through (not always).
		if (!bBribeRolled && Now - CheckpointArrivalTime > 1.5f)
		{
			bBribeRolled = true;
			bool bDirtyAboard = false;
			for (APVVCharacter* Occupant : Occupants)
			{
				const ABRCharacter* BRC = Cast<ABRCharacter>(Occupant);
				bDirtyAboard |= BRC && BRC->IsNPC() && BRC->IsSuspect();
			}
			if (bDirtyAboard && FMath::FRand() < BribeChance)
			{
				BribeOffer = FMath::RandRange(4, 12) * 50;
				ForceNetUpdate();
				ABRGameState::FX(this, EBRFX::PointsText, GetActorLocation() + FVector(150.f, 0.f, 260.f), FString::Printf(TEXT("psst... $%d?"), BribeOffer));
			}
		}
		if (BribeOffer > 0 && (bSteppedOut || bReacted || bRunningGate))
		{
			BribeOffer = 0; // too late for that now
		}
		// Everybody scanned: they ALL step out and wait by the van to be searched (hold E on each).
		// Clean people get back in once searched; anyone with no papers reacts (fight, run or hands up).
		if (bAllScanned && !bSteppedOut && !bRunningGate)
		{
			bSteppedOut = true;
			bReacted = true; // suspects react on foot now, not from inside the truck
			StepOutForSearch(Inspector);
			return;
		}
		if (WaitingOutside.Num() > 0 || HoldsAtCheckpoint())
		{
			FedUpFrom = Now; // nobody gets fed up while they're being searched (or the back is open)
			return;
		}
		const bool bCleared = bAllScanned && !bAnySuspect;
		const bool bFedUp = Now - FMath::Max(CheckpointArrivalTime, FedUpFrom) > MaxCheckpointWait;
		// Scanned and somebody's dirty: give Border Patrol a few seconds to order them out, then they react.
		if (bAllScanned && bAnySuspect && !bReacted)
		{
			if (ReactAt < 0.f)
			{
				ReactAt = Now + FMath::FRandRange(5.f, 9.f);
			}
			else if (Now >= ReactAt)
			{
				bReacted = true;
				SuspectsReact();
				return;
			}
		}
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
			BribeOffer = 0;
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
			bReachedFarSide = true;
			Destroy(); // reached the far side
		}
		return;
	}

	const float Delta = FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, ToTarget.Rotation().Yaw);
	// Every driver is a little different: some crawl, some floor it, and they wander about in the lane a bit
	// (like a person driving - so a player behind the wheel blends right in).
	const float Wander = DriverWobble * FMath::Sin(Now * DriverWobbleRate + DriverPhase) + DriverWobble * 0.5f * FMath::Sin(Now * DriverWobbleRate * 2.3f);
	SteerInput = FMath::Clamp(Delta / 35.f + Wander, -1.f, 1.f);
	const bool bNearCheckpoint = RouteIndex == CheckpointIndex && Distance < 2000.f;
	const float Cruise = 0.75f * DriverSpeedTaste * (1.f + 0.12f * FMath::Sin(Now * 0.23f + DriverPhase)); // eases on and off the gas
	ThrottleInput = FMath::Clamp(Cruise * (1.f - FMath::Abs(Delta) / 120.f), 0.25f, bNearCheckpoint ? 0.35f : FMath::Min(1.f, Cruise));
	// Rolling up to the checkpoint: ease off and brake so we arrive at walking pace (respect the barrier).
	if (RouteIndex == CheckpointIndex && Distance < 2500.f)
	{
		const float Speed = GetVelocity().Size2D();
		const float WantSpeed = FMath::Clamp((Distance - 300.f) * 0.45f, 120.f, 900.f);
		if (Speed > WantSpeed * 1.15f)
		{
			ThrottleInput = -FMath::Clamp((Speed - WantSpeed) / 400.f, 0.2f, 0.8f);
		}
		else if (Speed > WantSpeed)
		{
			ThrottleInput = 0.f;
		}
	}

	// Somebody walking in the road ahead? Slow down, stop and lean on the horn - don't plough through them
	// (only a gate runner or a Runner at the wheel runs people over).
	{
		const float Speed = GetVelocity().Size2D();
		const float LookAhead = 500.f + Speed * 1.2f;
		bool bPedestrian = false;
		bool bClose = false;
		for (TActorIterator<APVVCharacter> It(GetWorld()); It; ++It)
		{
			const APVVCharacter* P = *It;
			if (P->GetCurrentBoat() || P->GetCarrier())
			{
				continue;
			}
			const FVector Rel = GetActorTransform().InverseTransformPositionNoScale(P->GetActorLocation());
			if (Rel.X > 150.f && Rel.X < LookAhead && FMath::Abs(Rel.Y) < 330.f && FMath::Abs(Rel.Z) < 300.f)
			{
				bPedestrian = true;
				bClose |= Rel.X < 650.f;
			}
		}
		if (bPedestrian)
		{
			ThrottleInput = bClose ? (Speed > 80.f ? -0.6f : 0.f) : FMath::Min(ThrottleInput, 0.15f);
			if (Now >= NextHonk)
			{
				NextHonk = Now + FMath::FRandRange(2.5f, 5.f);
				HonkHorn();
			}
			return;
		}
	}
	// Keep a big gap to whatever's in front in my lane (spaced-out queue at the border, no pile-ups).
	if (IsLaneBlocked(Lane, bNearCheckpoint ? 1250.f : 1500.f, 0.f))
	{
		ThrottleInput = 0.f;
		if (BlockedSince < 0.f)
		{
			BlockedSince = Now;
		}
		// Stuck in traffic: lean on the horn now and then.
		if (Now - BlockedSince > 3.f && Now >= NextHonk)
		{
			NextHonk = Now + FMath::FRandRange(4.f, 10.f);
			if (FMath::FRand() < 0.6f)
			{
				HonkHorn();
			}
		}
		// Stuck behind someone for a bit and the other lane's clear: swerve over.
		if (Now - BlockedSince > 2.5f && !IsLaneBlocked(-Lane, 1300.f, 450.f))
		{
			Lane = -Lane;
			BlockedSince = -1.f;
		}
	}
	else
	{
		BlockedSince = -1.f;
	}
}

void ABRTruck::SuspectsReact()
{
	ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>();
	// Who's dirty aboard, and is the nearest Border Patrol agent close?
	TArray<ABRCharacter*> Suspects;
	for (APVVCharacter* Occupant : Occupants)
	{
		if (ABRCharacter* BRC = Cast<ABRCharacter>(Occupant); BRC && BRC->IsNPC() && BRC->IsSuspect())
		{
			Suspects.Add(BRC);
		}
	}
	ABRCharacter* Cop = BRNearestCop(GetWorld(), GetActorLocation(), 1500.f);
	const bool bDriverIsSuspect = Suspects.Contains(Cast<ABRCharacter>(GetDriver()));
	// FLOOR IT (more likely when the driver is the dirty one): smash the barrier and race off.
	if (GetDriver() && bDriverIsSuspect && FMath::FRand() < 0.2f) // emergency only: the driver is the one who's busted
	{
		FloorIt();
		return;
	}
	for (ABRCharacter* Suspect : Suspects)
	{
		const bool bMinor = !Suspect->bIllegal;
		Unseat(Suspect);
		Suspect->LaunchCharacter(GetActorRightVector() * FMath::FRandRange(-200.f, 200.f) + FVector(0.f, 0.f, 300.f), true, true);
		Suspect->ReactToBust(Cop, bMinor); // fight, knife, run or give up
	}
	if (GM && Suspects.Num() > 0)
	{
		GM->Feed(TEXT("The suspects bailed out of the truck!"));
	}
}

void ABRTruck::OnSuspectScanned()
{
	if (HasAuthority() && bNPCDriven && !bReacted && ReactAt < 0.f)
	{
		ReactAt = GetWorld()->GetTimeSeconds() + FMath::FRandRange(1.5f, 3.5f);
	}
}

void ABRTruck::FloorIt()
{
	if (!HasAuthority() || bRunningGate || !GetDriver())
	{
		return;
	}
	bRunningGate = true;
	bReacted = true;
	if (RouteIndex <= CheckpointIndex)
	{
		bCheckpointDone = true;
		RouteIndex = FMath::Min(CheckpointIndex + 1, Route.Num() - 1);
	}
	// (the barrier smashes when the truck reaches it - see DriveNPC)
	if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
	{
		GM->Feed(TEXT("A truck is FLOORING IT at the checkpoint! Chase it!"));
		GM->OnTruckForcedThrough();
	}
}

void ABRTruck::TickDriverTakeover(float Now)
{
	WaitingOutside.RemoveAll([this](const TWeakObjectPtr<APVVCharacter>& W)
	{
		const ABRCharacter* C = Cast<ABRCharacter>(W.Get());
		return !C || C->SearchWaitTruck.Get() != this;
	});
	if (GetDriver() || WaitingOutside.Num() > 0)
	{
		NoDriverSince = -1.f; // (people out being searched: nobody takes the wheel, nobody tows it)
		return;
	}
	if (NoDriverSince < 0.f)
	{
		NoDriverSince = Now;
		return;
	}
	bool bAnyoneAboard = false;
	for (const APVVCharacter* Occupant : Occupants)
	{
		bAnyoneAboard |= Occupant != nullptr;
	}
	// Nobody left aboard: towed after a while (only the empty truck - people who got out stay).
	if (!bAnyoneAboard)
	{
		if (Now - NoDriverSince > TowAfterSeconds)
		{
			Destroy();
		}
		return;
	}
	if (Now - NoDriverSince < 1.5f)
	{
		return; // a moment of panic first
	}
	for (int32 i = 1; i < Occupants.Num(); ++i)
	{
		ABRCharacter* NPC = Cast<ABRCharacter>(Occupants[i].Get());
		if (!NPC || !NPC->IsNPC() || NPC->GetCondition() != EPVVCondition::Normal)
		{
			continue; // zip-tied prisoners in the back don't drive
		}
		const bool bDirty = NPC->IsSuspect();
		if (bDirty && bReacted && FMath::FRand() < 0.4f)
		{
			// Busted: this one makes a run for it instead.
			Unseat(NPC);
			NPC->ReactToBust(BRNearestCop(GetWorld(), GetActorLocation(), 2000.f), !NPC->bIllegal);
			NoDriverSince = Now - 1.f; // next person thinks about it in a sec
			return;
		}
		// Slide over into the driver's seat.
		ClearSeat(i);
		SeatCharacter(NPC, 0);
		NoDriverSince = -1.f;
		if (bDirty && bReacted && FMath::FRand() < 0.25f)
		{
			FloorIt(); // busted and behind the wheel: panic and GO
		}
		return;
	}
}

void ABRTruck::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Only the people still riding along leave with the truck. Anyone who got out (running, fighting,
	// zip-tied, being carried, in the pen) stays in the world.
	TArray<APVVCharacter*> Riders;
	if (HasAuthority() && EndPlayReason == EEndPlayReason::Destroyed)
	{
		for (const TWeakObjectPtr<APVVCharacter>& NPC : SpawnedNPCs)
		{
			if (NPC.IsValid() && NPC->GetCurrentBoat() == this && NPC->GetCondition() != EPVVCondition::Restrained)
			{
				Riders.Add(NPC.Get());
			}
		}
	}
	// Made it all the way across with people / contraband nobody caught: points for the Runners.
	if (bReachedFarSide && Riders.Num() > 0)
	{
		int32 Illegal = 0;
		int32 Contraband = 0;
		for (APVVCharacter* Rider : Riders)
		{
			if (const ABRCharacter* BRC = Cast<ABRCharacter>(Rider))
			{
				Illegal += BRC->bIllegal ? 1 : 0;
				Contraband += BRC->bCarriesContraband ? 1 : 0;
			}
		}
		if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
		{
			GM->OnSmuggledThrough(Illegal, Contraband, GetActorLocation());
		}
	}
	Super::EndPlay(EndPlayReason); // ejects everyone first
	for (APVVCharacter* Rider : Riders)
	{
		if (IsValid(Rider))
		{
			Rider->Destroy();
		}
	}
}

void ABRTruck::StepOutForSearch(ABRCharacter* Inspector)
{
	TArray<TPair<ABRCharacter*, int32>> Out;
	for (int32 i = 0; i < Occupants.Num(); ++i)
	{
		ABRCharacter* C = Cast<ABRCharacter>(Occupants[i]);
		if (C && C->IsNPC() && C->GetCondition() == EPVVCondition::Normal)
		{
			Out.Emplace(C, i);
		}
	}
	for (const TPair<ABRCharacter*, int32>& P : Out)
	{
		Unseat(P.Key);
		P.Key->WaitForSearch(this, P.Value, P.Key->bIllegal ? Inspector : nullptr);
		WaitingOutside.Add(P.Key);
	}
	if (Out.Num() > 0 && Inspector)
	{
		Inspector->Notify(NSLOCTEXT("BRCar", "StepOut", "Everybody out! SEARCH each of them for weapons (hold E). They get back in once they're clean."));
	}
	ForceNetUpdate();
}

void ABRTruck::ReturnFromSearch(APVVCharacter* Passenger, int32 Seat)
{
	if (!HasAuthority() || !Passenger || Passenger->GetCurrentBoat())
	{
		return;
	}
	WaitingOutside.Remove(Passenger);
	if (Occupants.IsValidIndex(Seat) && !Occupants[Seat])
	{
		SeatCharacter(Passenger, Seat);
	}
	else if (const int32 Free = FindFreePassengerSeat(); Free != INDEX_NONE)
	{
		SeatCharacter(Passenger, Free);
	}
	ForceNetUpdate();
}

// ===========================================================================================
// Bribes at the checkpoint

bool ABRTruck::WantsBribe(const APVVCharacter* Interactor) const
{
	return BribeOffer > 0 && bNPCDriven && !bCheckpointDone && Interactor && Interactor->GetFaction() == BR::Wardens
		&& Interactor->GetCondition() == EPVVCondition::Normal && !Interactor->GetCurrentBoat() && !Interactor->GetCarriedCharacter()
		&& FVector::Dist(Interactor->GetActorLocation(), GetActorLocation()) < 520.f;
}

bool ABRTruck::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	return WantsBribe(Interactor) || Super::CanInteract_Implementation(Interactor);
}

float ABRTruck::GetInteractDuration_Implementation(APVVCharacter* Interactor) const
{
	return WantsBribe(Interactor) ? 1.5f : Super::GetInteractDuration_Implementation(Interactor);
}

FText ABRTruck::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	if (WantsBribe(Interactor))
	{
		return FText::Format(LOCTEXT("Bribe", "TAKE the ${0} bribe and wave them through (hold) - watch the cameras! | O = order them out"), BribeOffer);
	}
	return Super::GetInteractPrompt_Implementation(Interactor);
}

void ABRTruck::OnInteract_Implementation(APVVCharacter* Interactor)
{
	if (!WantsBribe(Interactor))
	{
		Super::OnInteract_Implementation(Interactor);
		return;
	}
	if (!HasAuthority())
	{
		return;
	}
	const int32 Amount = BribeOffer;
	BribeOffer = 0;
	ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>();
	// Caught on tape? Cameras look at your head (and ignore this truck).
	const bool bFilmed = ABRSurveillanceCamera::AnyCameraSees(GetWorld(), Interactor->GetActorLocation() + FVector(0.f, 0.f, 60.f), this);
	if (GM)
	{
		if (bFilmed)
		{
			GM->OnBribeFilmed(Interactor, Amount);
		}
		else
		{
			GM->AddBudget(Amount);
			ABRGameState::FX(this, EBRFX::MoneyText, Interactor->GetActorLocation() + FVector(0.f, 0.f, 80.f), FString::Printf(TEXT("+$%d (shh)"), Amount));
		}
	}
	// Waved through: no catch, no approval - whoever is aboard rides on into the US.
	bCheckpointDone = true;
	bReacted = true;
	ForceNetUpdate();
}

#undef LOCTEXT_NAMESPACE
