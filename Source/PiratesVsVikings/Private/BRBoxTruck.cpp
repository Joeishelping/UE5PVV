#include "BRBoxTruck.h"
#include "BRCharacter.h"
#include "BRGameMode.h"
#include "BRGameState.h"
#include "BRMapKit.h"
#include "BRPlayerState.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "BRBoxTruck"

namespace
{
	// The hold's walls are their own (non-welded) bodies on this channel so they never fight the truck's own hull.
	constexpr ECollisionChannel HoldChannel = ECC_Destructible;

	ABRCharacter* NearestCop(UWorld* World, const FVector& Where, float Range)
	{
		ABRCharacter* Best = nullptr;
		for (TActorIterator<ABRCharacter> It(World); It; ++It)
		{
			const float D = FVector::Dist(It->GetActorLocation(), Where);
			if (It->IsWarden() && It->GetCondition() == EPVVCondition::Normal && D < Range)
			{
				Range = D;
				Best = *It;
			}
		}
		return Best;
	}
}

ABRBoxTruck::ABRBoxTruck()
{
	// Taller vehicle: camera further back and higher, so you see over the box.
	DriveArmLength = 1350.f;
	DriveCamHeight = 480.f;
	PaintColor = FLinearColor(0.85f, 0.85f, 0.82f);
	MassKg = 1800.f;
	// Two seats up front only: the back is cargo.
	SeatOffsets = { FVector(80.f, -50.f, 125.f), FVector(80.f, 50.f, 125.f) };
	Hull->SetCollisionResponseToChannel(HoldChannel, ECR_Ignore);

	const float Len = HoldFrontX - HoldBackX;
	const float MidX = (HoldFrontX + HoldBackX) * 0.5f;
	const float Height = HoldTopZ - HoldFloorZ;
	const float MidZ = (HoldTopZ + HoldFloorZ) * 0.5f;
	HoldFloor = HoldPart(TEXT("Body_HoldFloor"), FVector(MidX, 0.f, HoldFloorZ - 5.f), FVector(Len, HoldHalfY * 2.f + 10.f, 10.f), true);
	HoldPart(TEXT("Body_HoldWallL"), FVector(MidX, -HoldHalfY - 6.f, MidZ), FVector(Len, 10.f, Height), true);
	HoldPart(TEXT("Body_HoldWallR"), FVector(MidX, HoldHalfY + 6.f, MidZ), FVector(Len, 10.f, Height), true);
	HoldPart(TEXT("Body_HoldRoof"), FVector(MidX, 0.f, HoldTopZ + 5.f), FVector(Len, HoldHalfY * 2.f + 22.f, 10.f), true);
	HoldPart(TEXT("Body_HoldFront"), FVector(HoldFrontX + 5.f, 0.f, MidZ), FVector(10.f, HoldHalfY * 2.f + 10.f, Height), true);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	for (const float WheelY : { -112.f, 112.f })
	{
		UStaticMeshComponent* Wheel = HoldPart(WheelY < 0.f ? TEXT("Wheel_RearXL") : TEXT("Wheel_RearXR"), FVector(-360.f, WheelY, -35.f), FVector(95.f, 95.f, 40.f), false);
		Wheel->SetRelativeRotation(FRotator(0.f, 0.f, 90.f));
		if (Cylinder.Succeeded())
		{
			Wheel->SetStaticMesh(Cylinder.Object);
		}
	}
	HoldPart(TEXT("Trim_RearBumper"), FVector(HoldBackX - 12.f, 0.f, 15.f), FVector(14.f, 225.f, 18.f), false);

	// Two back doors on hinges at the rear corners (they swing out backwards).
	HingeL = CreateDefaultSubobject<USceneComponent>(TEXT("HingeL"));
	HingeL->SetupAttachment(SeatRoot);
	HingeL->SetRelativeLocation(FVector(HoldBackX - 4.f, -HoldHalfY - 6.f, HoldFloorZ));
	HingeR = CreateDefaultSubobject<USceneComponent>(TEXT("HingeR"));
	HingeR->SetupAttachment(SeatRoot);
	HingeR->SetRelativeLocation(FVector(HoldBackX - 4.f, HoldHalfY + 6.f, HoldFloorZ));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	auto Door = [this](const TCHAR* Name, USceneComponent* Hinge, float Side)
	{
		UStaticMeshComponent* D = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		D->SetupAttachment(Hinge);
		D->SetStaticMesh(Cube.Object);
		D->SetRelativeLocation(FVector(0.f, Side * (HoldHalfY + 6.f) * 0.5f, (HoldTopZ - HoldFloorZ) * 0.5f));
		D->SetRelativeScale3D(FVector(6.f, HoldHalfY + 4.f, HoldTopZ - HoldFloorZ) / 100.f);
		// Blocks sight (scanners, cameras) but not bodies: the latch blocker below does that, so nothing desyncs.
		D->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		D->SetCollisionResponseToAllChannels(ECR_Ignore);
		D->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		return D;
	};
	DoorL = Door(TEXT("Body_DoorBackL"), HingeL, 1.f);
	DoorR = Door(TEXT("Body_DoorBackR"), HingeR, -1.f);

	// Loading ramp from the ground up to the hold floor (shown while the back is open and the truck is parked).
	{
		const float Run = 240.f;
		const float Rise = HoldFloorZ + 90.f;
		const float Slope = FMath::RadiansToDegrees(FMath::Atan2(Rise, Run));
		Ramp = HoldPart(TEXT("Trim_Ramp"), FVector(HoldBackX - Run * 0.5f - 4.f, 0.f, HoldFloorZ - Rise * 0.5f - 4.f), FVector(FMath::Sqrt(Run * Run + Rise * Rise), 160.f, 8.f), true);
		Ramp->SetRelativeRotation(FRotator(Slope, 0.f, 0.f));
		Ramp->SetVisibility(false);
		Ramp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	DoorBlocker = CreateDefaultSubobject<UBoxComponent>(TEXT("DoorBlocker"));
	DoorBlocker->SetupAttachment(SeatRoot);
	DoorBlocker->SetRelativeLocation(FVector(HoldBackX - 4.f, 0.f, MidZ));
	DoorBlocker->SetBoxExtent(FVector(6.f, HoldHalfY + 6.f, Height * 0.5f));
	DoorBlocker->SetCollisionProfileName(TEXT("BlockAll"));
	DoorBlocker->SetCollisionObjectType(HoldChannel);
	DoorBlocker->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Ignore);
	DoorBlocker->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	DoorBlocker->BodyInstance.bAutoWeld = false;
}

UStaticMeshComponent* ABRBoxTruck::HoldPart(const TCHAR* Name, const FVector& At, const FVector& SizeCm, bool bSolid)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	C->SetupAttachment(SeatRoot);
	C->SetStaticMesh(Cube.Object);
	C->SetRelativeLocation(At);
	C->SetRelativeScale3D(SizeCm / 100.f);
	if (bSolid)
	{
		// Its own body (not welded into the truck's physics), on a channel the hull ignores.
		C->SetCollisionProfileName(TEXT("BlockAll"));
		C->SetCollisionObjectType(HoldChannel);
		C->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Ignore);
		C->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		C->BodyInstance.bAutoWeld = false;
	}
	else
	{
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	return C;
}

void ABRBoxTruck::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRBoxTruck, bDoorsOpen);
}

void ABRBoxTruck::BeginPlay()
{
	Super::BeginPlay();
	// Nothing in or on the truck (crates, packages, people in the back) ever pulls the chase camera in.
	if (CameraBoom)
	{
		CameraBoom->bDoCollisionTest = false;
	}
	ApplyDoorCollision();
}

bool ABRBoxTruck::IsInHold(const FVector& WorldPoint, float Margin) const
{
	const FVector L = SeatRoot->GetComponentTransform().InverseTransformPositionNoScale(WorldPoint);
	return L.X < HoldFrontX + Margin && L.X > HoldBackX - Margin && FMath::Abs(L.Y) < HoldHalfY + Margin && L.Z > HoldFloorZ - 60.f - Margin && L.Z < HoldTopZ + Margin;
}

FVector ABRBoxTruck::GetHoldCenter() const
{
	return SeatRoot->GetComponentTransform().TransformPositionNoScale(FVector((HoldFrontX + HoldBackX) * 0.5f, 0.f, HoldFloorZ + 100.f));
}

FVector ABRBoxTruck::GetDoorStep() const
{
	return SeatRoot->GetComponentTransform().TransformPositionNoScale(FVector(HoldBackX - 320.f, 0.f, HoldFloorZ + 60.f));
}

// ---------------------------------------------------------------------------------------------
// Doors

void ABRBoxTruck::OnRep_Doors()
{
	ApplyDoorCollision();
	if (bDoorsOpen)
	{
		DoorSpeed[0] = FMath::Max(DoorSpeed[0], 140.f); // the latch pops and they swing out
		DoorSpeed[1] = FMath::Max(DoorSpeed[1], 120.f);
	}
}

void ABRBoxTruck::ApplyDoorCollision()
{
	if (DoorBlocker)
	{
		DoorBlocker->SetCollisionEnabled(bDoorsOpen ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	}
}

static void SetRampState(UStaticMeshComponent* Ramp, bool bDown)
{
	if (Ramp)
	{
		Ramp->SetVisibility(bDown);
		Ramp->SetCollisionEnabled(bDown ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
}

void ABRBoxTruck::SetDoorsOpen(bool bOpen, APVVCharacter* By)
{
	if (!HasAuthority() || bDoorsOpen == bOpen)
	{
		return;
	}
	bDoorsOpen = bOpen;
	OnRep_Doors();
	ForceNetUpdate();
	if (bOpen)
	{
		// Anyone hiding back there is found.
		ReleaseStowed(Cast<ABRCharacter>(By));
	}
	else
	{
		StowPeople();
	}
}

void ABRBoxTruck::TickDoors(float DeltaSeconds)
{
	// Swing physics on every screen: inertia from the truck braking / turning, a little friction on the hinges,
	// bounce off the stops. Latched shut = locked at 0.
	const FVector Vel = GetVelocity();
	const FVector Accel = DeltaSeconds > 0.f ? (Vel - LastVelocity) / DeltaSeconds : FVector::ZeroVector;
	LastVelocity = Vel;
	const FVector LocalAccel = GetActorTransform().InverseTransformVectorNoScale(Accel);
	for (int32 i = 0; i < 2; ++i)
	{
		if (!bDoorsOpen)
		{
			// Closing: swing back to the frame and latch.
			DoorSpeed[i] = 0.f;
			DoorAngle[i] = FMath::FInterpTo(DoorAngle[i], 0.f, DeltaSeconds, 9.f);
			continue;
		}
		const float Side = i == 0 ? 1.f : -1.f;
		// Speeding up throws the doors open (they lag behind), braking slams them toward shut; turning swings one each way.
		float Torque = LocalAccel.X * 0.09f + Side * LocalAccel.Y * 0.07f;
		// People walking into a door push it.
		for (TActorIterator<APVVCharacter> It(GetWorld()); It; ++It)
		{
			const FVector Local = GetActorTransform().InverseTransformPositionNoScale(It->GetActorLocation());
			if (Local.X < HoldBackX + 40.f && Local.X > HoldBackX - 240.f && Local.Y * Side < 0.f && FMath::Abs(Local.Y) < HoldHalfY + 140.f)
			{
				const float Push = FVector::DotProduct(It->GetVelocity(), -GetActorForwardVector());
				Torque += Local.X > HoldBackX ? -FMath::Abs(Push) * 0.6f : FMath::Abs(Push) * 0.6f;
			}
		}
		DoorSpeed[i] += Torque * DeltaSeconds * 60.f;
		DoorSpeed[i] *= FMath::Exp(-1.6f * DeltaSeconds); // hinge friction
		DoorAngle[i] += DoorSpeed[i] * DeltaSeconds;
		if (DoorAngle[i] > 175.f) { DoorAngle[i] = 175.f; DoorSpeed[i] = -DoorSpeed[i] * 0.35f; }	// bangs into the side
		if (DoorAngle[i] < 4.f) { DoorAngle[i] = 4.f; DoorSpeed[i] = -DoorSpeed[i] * 0.3f; }		// bangs on the frame (still unlatched)
	}
	// Ramp: down while the back's open and we're parked; up (and out of the way) when we drive.
	const bool bWantRamp = bDoorsOpen && Vel.Size2D() < 150.f;
	if (bWantRamp != bRampDown)
	{
		bRampDown = bWantRamp;
		SetRampState(Ramp, bRampDown);
	}
	HingeL->SetRelativeRotation(FRotator(0.f, DoorAngle[0], 0.f));
	HingeR->SetRelativeRotation(FRotator(0.f, -DoorAngle[1], 0.f));
}

// ---------------------------------------------------------------------------------------------
// Cargo + stowaways

ABRPhysicsProp* ABRBoxTruck::SpawnCargo(const FVector& At, bool bPackage, ABRPlayerState* CargoOwner)
{
	const FTransform T(FRotator(0.f, FMath::FRandRange(-20.f, 20.f), 0.f), At);
	ABRPhysicsProp* Prop = GetWorld()->SpawnActorDeferred<ABRPhysicsProp>(ABRPhysicsProp::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Prop)
	{
		return nullptr;
	}
	Prop->PropShape = bPackage ? 5 : 0;
	Prop->bContraband = bPackage;
	Prop->ContrabandOwner = CargoOwner;
	Prop->Color = bPackage ? FLinearColor(0.55f, 0.4f, 0.2f) : FLinearColor(0.6f, 0.48f, 0.3f);
	Prop->MassKg = bPackage ? 12.f : 25.f;
	Prop->FinishSpawning(T);
	Cargo.Add(Prop);
	return Prop;
}

void ABRBoxTruck::LoadNPCCargo()
{
	if (!HasAuthority())
	{
		return;
	}
	const FTransform Root = SeatRoot->GetComponentTransform();
	auto Spot = [&Root](float X, float Y, float Z) { return Root.TransformPositionNoScale(FVector(X, Y, Z)); };
	// Packages on the floor at the front, crates piled in front of them.
	const int32 NumPackages = FMath::FRand() < 0.45f ? FMath::RandRange(1, 3) : 0;
	for (int32 i = 0; i < NumPackages; ++i)
	{
		SpawnCargo(Spot(HoldFrontX - 30.f - i * 35.f, FMath::FRandRange(-60.f, 60.f), HoldFloorZ + 20.f), true, nullptr);
	}
	const int32 NumCrates = FMath::RandRange(4, 7);
	for (int32 i = 0; i < NumCrates; ++i)
	{
		const int32 Row = i % 3;
		const int32 Layer = i / 3;
		SpawnCargo(Spot(-190.f - Layer * 20.f, -65.f + Row * 65.f, HoldFloorZ + 35.f + Layer * 62.f), false, nullptr);
	}
	// Somebody hiding back there?
	if (FMath::FRand() < 0.3f)
	{
		if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
		{
			const int32 People = FMath::RandRange(1, 2);
			for (int32 i = 0; i < People; ++i)
			{
				if (ABRCharacter* NPC = GM->SpawnNPC(FTransform(GetActorRotation(), Spot(-110.f, i == 0 ? -45.f : 45.f, HoldFloorZ + 100.f))))
				{
					NPC->bIllegal = true;
					Stowed.Add(NPC);
				}
			}
		}
	}
	StowPeople();
}

void ABRBoxTruck::StowPeople()
{
	if (!HasAuthority())
	{
		return;
	}
	// NPCs in the hold (stowaways, a coyote's clients) hunker down and ride attached to the truck.
	for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
	{
		ABRCharacter* C = *It;
		if (!C->IsNPC() || C->GetCurrentBoat() || C->GetCarrier() || C->GetCondition() != EPVVCondition::Normal || !IsInHold(C->GetActorLocation(), 30.f))
		{
			continue;
		}
		Stowed.AddUnique(C);
	}
	for (const TWeakObjectPtr<ABRCharacter>& Weak : Stowed)
	{
		if (ABRCharacter* C = Weak.Get(); C && C->GetAttachParentActor() != this)
		{
			C->GetCharacterMovement()->DisableMovement();
			C->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			C->AttachToComponent(HoldFloor, FAttachmentTransformRules::KeepWorldTransform);
			C->Crouch();
		}
	}
}

void ABRBoxTruck::ReleaseStowed(ABRCharacter* Finder)
{
	if (!HasAuthority())
	{
		return;
	}
	ABRCharacter* Cop = (Finder && Finder->IsWarden()) ? Finder : NearestCop(GetWorld(), GetActorLocation(), 1500.f);
	int32 Found = 0;
	for (const TWeakObjectPtr<ABRCharacter>& Weak : Stowed)
	{
		ABRCharacter* C = Weak.Get();
		if (!C)
		{
			continue;
		}
		C->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		C->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		C->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		C->SetActorRotation(FRotator(0.f, C->GetActorRotation().Yaw, 0.f));
		C->UnCrouch();
		if (Cop && !C->Leader.IsValid())
		{
			++Found;
			C->ReactToBust(Cop, !C->bIllegal); // fight, run or hands up
		}
	}
	Stowed.Reset();
	if (Found > 0)
	{
		ABRGameState::FX(this, EBRFX::PointsText, GetDoorStep() + FVector(0.f, 0.f, 160.f), FString::Printf(TEXT("%d HIDING IN THE BACK!"), Found));
		if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
		{
			GM->Feed(FString::Printf(TEXT("Border Patrol found %d people hiding in a box truck!"), Found));
		}
	}
}

void ABRBoxTruck::TickCargo(float DeltaSeconds)
{
	// Moving: everything in the back is locked to the floor (no flying through walls at 60 mph).
	// Slowed down / stopped: it all tumbles loose again (the brake shoves it forward) and you can dig through it.
	const float Speed = GetVelocity().Size2D();
	const float Now = GetWorld()->GetTimeSeconds();
	Cargo.RemoveAll([](const TObjectPtr<ABRPhysicsProp>& P) { return !IsValid(P); });
	// Anything somebody threw in the back counts as cargo too.
	for (TActorIterator<ABRPhysicsProp> It(GetWorld()); It; ++It)
	{
		if (!It->IsHeld() && !Cargo.Contains(*It) && IsInHold(It->GetActorLocation()))
		{
			Cargo.Add(*It);
		}
	}
	if (!bCargoFrozen && Speed > 250.f)
	{
		bCargoFrozen = true;
		for (ABRPhysicsProp* P : Cargo)
		{
			if (!P->IsHeld() && IsInHold(P->GetActorLocation(), 20.f))
			{
				P->SetCargoFrozen(true, HoldFloor);
			}
		}
	}
	else if (bCargoFrozen && Speed < 60.f)
	{
		if (SlowSince < 0.f)
		{
			SlowSince = Now;
		}
		if (Now - SlowSince > 0.6f)
		{
			bCargoFrozen = false;
			SlowSince = -1.f;
			for (ABRPhysicsProp* P : Cargo)
			{
				P->SetCargoFrozen(false, nullptr);
			}
		}
	}
	else
	{
		SlowSince = -1.f;
	}
}

void ABRBoxTruck::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Hide the pickup bed parts (the box sits there instead).
	if (!bBedHidden)
	{
		bBedHidden = true;
		TArray<UStaticMeshComponent*> Meshes;
		GetComponents<UStaticMeshComponent>(Meshes);
		for (UStaticMeshComponent* M : Meshes)
		{
			const FString N = M->GetName();
			if (N == TEXT("Body_BedL") || N == TEXT("Body_BedR") || N == TEXT("Body_Tailgate"))
			{
				M->SetVisibility(false);
			}
		}
	}
	TickDoors(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	TickCargo(DeltaSeconds);
	const float Now = GetWorld()->GetTimeSeconds();
	// NPC drivers shut the back themselves once Border Patrol have walked off.
	if (IsNPCTruck() && bDoorsOpen)
	{
		if (NearestCop(GetWorld(), GetDoorStep(), 900.f))
		{
			NoCopNearSince = -1.f;
		}
		else if (NoCopNearSince < 0.f)
		{
			NoCopNearSince = Now;
		}
		else if (Now - NoCopNearSince > 5.f)
		{
			SetDoorsOpen(false, nullptr);
			NoCopNearSince = -1.f;
		}
	}
	// Delivered: packages in the back of a Runner's truck in the Safe Zone pay out.
	if (BRMap::IsInSafeZone(GetWorld(), GetActorLocation()))
	{
		int32 Paid = 0;
		for (ABRPhysicsProp* P : Cargo)
		{
			if (IsValid(P) && P->bContraband && !P->IsHeld() && IsInHold(P->GetActorLocation(), 30.f))
			{
				if (ABRPlayerState* PS = P->ContrabandOwner.Get())
				{
					PS->AddMoney(400);
					Paid += 400;
				}
				P->Destroy();
			}
		}
		if (Paid > 0)
		{
			ABRGameState::FX(this, EBRFX::MoneyText, GetActorLocation() + FVector(0.f, 0.f, 320.f), FString::Printf(TEXT("+$%d DELIVERED"), Paid));
			ABRGameState::Sfx(this, EBRSfx::SafeZone, GetActorLocation());
			if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
			{
				GM->Feed(FString::Printf(TEXT("A box truck full of contraband made it! +$%d"), Paid));
			}
		}
		// Stowed clients climb out (TickClients pays their coyote).
		if (Stowed.Num() > 0)
		{
			ReleaseStowed(nullptr);
		}
		// A Runner's job truck made it: unloaded, then it's taken away (cargo and all).
		if (!IsNPCTruck())
		{
			if (DeliveredAt < 0.f)
			{
				DeliveredAt = Now;
				if (APVVCharacter* Driver = GetDriver())
				{
					if (ABRCharacter* BRDriver = Cast<ABRCharacter>(Driver))
					{
						BRDriver->Notify(LOCTEXT("JobDone", "Delivered! The truck gets unloaded and taken away."));
					}
				}
			}
			else if (Now - DeliveredAt > 5.f)
			{
				for (int32 i = Occupants.Num() - 1; i >= 0; --i)
				{
					if (Occupants[i])
					{
						Unseat(Occupants[i]);
					}
				}
				for (ABRPhysicsProp* P : Cargo)
				{
					if (IsValid(P) && (P->IsCargoFrozen() || IsInHold(P->GetActorLocation(), 40.f)))
					{
						P->Destroy();
					}
				}
				Cargo.Reset();
				ABRGameState::FX(this, EBRFX::Dust, GetActorLocation());
				Destroy();
				return;
			}
		}
	}
}

void ABRBoxTruck::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority() && EndPlayReason == EEndPlayReason::Destroyed)
	{
		// NPC box truck got all the way across: whatever nobody found scores for the Runners.
		int32 People = 0;
		int32 Packages = 0;
		for (const TWeakObjectPtr<ABRCharacter>& Weak : Stowed)
		{
			if (ABRCharacter* C = Weak.Get())
			{
				People += C->bIllegal ? 1 : 0;
				C->Destroy();
			}
		}
		Stowed.Reset();
		for (ABRPhysicsProp* P : Cargo)
		{
			if (IsValid(P) && (P->IsCargoFrozen() || IsInHold(P->GetActorLocation(), 30.f)))
			{
				Packages += P->bContraband ? 1 : 0;
				P->Destroy();
			}
		}
		if (HasReachedFarSide() && (People > 0 || Packages > 0))
		{
			if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
			{
				GM->OnSmuggledThrough(People, Packages, GetActorLocation());
			}
		}
	}
	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------------------------
// E at the back: open / close the doors

bool ABRBoxTruck::WantsDoor(const APVVCharacter* Interactor) const
{
	if (!Interactor || Interactor->GetCurrentBoat() || Interactor->GetCondition() != EPVVCondition::Normal || GetVelocity().Size2D() > 120.f)
	{
		return false;
	}
	const FVector L = SeatRoot->GetComponentTransform().InverseTransformPositionNoScale(Interactor->GetActorLocation());
	return L.X < HoldBackX + 90.f && L.X > HoldBackX - 340.f && FMath::Abs(L.Y) < HoldHalfY + 120.f;
}

bool ABRBoxTruck::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	return WantsDoor(Interactor) || Super::CanInteract_Implementation(Interactor);
}

float ABRBoxTruck::GetInteractDuration_Implementation(APVVCharacter* Interactor) const
{
	return WantsDoor(Interactor) ? (bDoorsOpen ? 0.3f : 0.8f) : Super::GetInteractDuration_Implementation(Interactor);
}

FText ABRBoxTruck::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	if (WantsDoor(Interactor))
	{
		if (bDoorsOpen)
		{
			return LOCTEXT("CloseDoors", "Close the back doors");
		}
		return Interactor->GetFaction() == BR::Wardens ? LOCTEXT("OpenDoorsBP", "Open the back - SEARCH the cargo (grab packages = seize them)")
			: LOCTEXT("OpenDoors", "Open the back doors");
	}
	return Super::GetInteractPrompt_Implementation(Interactor);
}

void ABRBoxTruck::OnInteract_Implementation(APVVCharacter* Interactor)
{
	if (WantsDoor(Interactor))
	{
		SetDoorsOpen(!bDoorsOpen, Interactor);
		return;
	}
	Super::OnInteract_Implementation(Interactor);
}

ABRBoxTruck* ABRBoxTruck::FindHoldForClient(const ABRCharacter* Client)
{
	const ABRCharacter* Coyote = Client ? Client->Leader.Get() : nullptr;
	const APlayerState* CoyotePS = Coyote ? Coyote->GetPlayerState() : nullptr;
	if (!CoyotePS)
	{
		return nullptr;
	}
	for (TActorIterator<ABRBoxTruck> It(Client->GetWorld()); It; ++It)
	{
		if (It->OwnerPlayer == CoyotePS && It->AreDoorsOpen() && FVector::Dist2D(It->GetActorLocation(), Client->GetActorLocation()) < 2500.f)
		{
			return *It;
		}
	}
	return nullptr;
}

// ===========================================================================================
// Smuggling truck job board

ABRTruckJobBoard::ABRTruckJobBoard()
{
	bReplicates = true;
	AddMesh(TEXT("Post"), CubeMesh, FVector(0.f, 0.f, 120.f), FVector(20.f, 20.f, 240.f), FLinearColor(0.35f, 0.25f, 0.15f));
	AddMesh(TEXT("Board"), CubeMesh, FVector(0.f, 0.f, 230.f), FVector(10.f, 220.f, 130.f), FLinearColor(0.7f, 0.6f, 0.4f));
	UTextRenderComponent* Sign = AddLabel(TEXT("Sign"), FVector(-8.f, 0.f, 255.f), 30.f, FColor(40, 20, 10));
	Sign->SetText(LOCTEXT("JobSign", "SMUGGLING TRUCK JOB\nbox truck + 4 packages + 2 clients\n$400 per package delivered"));
	Sign->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	AddPickupZone(220.f);
}

bool ABRTruckJobBoard::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	const ABRCharacter* C = Cast<ABRCharacter>(Interactor);
	return C && C->IsRunner() && !C->IsNPC() && C->GetCondition() == EPVVCondition::Normal && !C->GetCurrentBoat();
}

FText ABRTruckJobBoard::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	if (ActiveTruck.IsValid())
	{
		return LOCTEXT("JobTaken", "Somebody's already on a truck job - wait for it to finish");
	}
	const float Wait = NextJob - GetWorld()->GetTimeSeconds();
	if (Wait > 0.f)
	{
		return FText::Format(LOCTEXT("JobWait", "Next truck job in {0} s"), FMath::CeilToInt(Wait));
	}
	return FText::Format(LOCTEXT("JobTake", "Take a SMUGGLING TRUCK JOB (load the packages + clients, cover them, drive to the Safe Zone: ${0} a package)"), PayPerPackage);
}

void ABRTruckJobBoard::OnInteract_Implementation(APVVCharacter* Interactor)
{
	ABRCharacter* Runner = Cast<ABRCharacter>(Interactor);
	ABRPlayerState* PS = Runner ? Runner->GetBRPlayerState() : nullptr;
	ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>();
	const float Now = GetWorld()->GetTimeSeconds();
	if (!HasAuthority() || !PS || !GM || ActiveTruck.IsValid() || Now < NextJob)
	{
		return;
	}
	const FVector At = GetActorTransform().TransformPosition(TruckSpot) + FVector(0.f, 0.f, 90.f);
	const FTransform T(GetActorRotation(), At);
	ABRBoxTruck* Truck = GetWorld()->SpawnActorDeferred<ABRBoxTruck>(ABRBoxTruck::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Truck)
	{
		return;
	}
	Truck->bRequiresKey = false;
	Truck->OwnerPlayer = PS;
	Truck->Fuel = 1.f;
	Truck->FinishSpawning(T);
	Truck->SetDoorsOpen(true, nullptr);
	ActiveTruck = Truck;
	NextJob = Now + CooldownSeconds;
	// Packages + some crates to hide them under, stacked behind the truck.
	const FVector Behind = Truck->GetDoorStep() - Truck->GetActorForwardVector() * 150.f;
	const FVector Right = Truck->GetActorRightVector();
	for (int32 i = 0; i < Packages; ++i)
	{
		Truck->SpawnCargo(Behind + Right * (i * 60.f - 90.f) + FVector(0.f, 0.f, 30.f), true, PS);
	}
	for (int32 i = 0; i < 5; ++i)
	{
		Truck->SpawnCargo(Behind - Truck->GetActorForwardVector() * 110.f + Right * (i * 70.f - 140.f) + FVector(0.f, 0.f, 40.f), false, nullptr);
	}
	for (int32 i = 0; i < Clients; ++i)
	{
		GM->HireClient(Runner, Behind + Right * (i == 0 ? -250.f : 250.f), 300);
	}
	Runner->Notify(LOCTEXT("JobStart", "Truck job! Grab the packages (Z) and throw them in the back, cover them with crates. Your clients climb in by themselves. Shut the doors (E) and drive to the Safe Zone."));
	GM->Feed(FString::Printf(TEXT("%s took a smuggling truck job"), *Runner->GetPlayerDisplayName()));
}

#undef LOCTEXT_NAMESPACE
