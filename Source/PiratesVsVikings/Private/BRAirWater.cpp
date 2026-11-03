// Air & water: inflatable raft, hot air balloon, anti-air guns, wheelbarrow.

#include "BRAirWater.h"
#include "BRCharacter.h"
#include "BRGameMode.h"
#include "BRGameState.h"
#include "BRPlayerState.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "BRAirWater"

bool BRAir::TryPop(AActor* Hit, ABRCharacter* By)
{
	if (ABRRaft* Raft = Cast<ABRRaft>(Hit))
	{
		Raft->Pop(By);
		return true;
	}
	if (ABRBalloon* Balloon = Cast<ABRBalloon>(Hit); Balloon && Balloon->IsFlying())
	{
		Balloon->Pop(By);
		return true;
	}
	return false;
}

// ===========================================================================================
// Character: riding a fixture (balloon basket, wheelbarrow) - visible, no walking, no collision

void ABRCharacter::SetOnRide(bool bOn)
{
	if (!HasAuthority() || bOnRide == bOn)
	{
		return;
	}
	bOnRide = bOn;
	OnRep_OnRide();
	ForceNetUpdate();
}

void ABRCharacter::OnRep_OnRide()
{
	SetActorEnableCollision(!bOnRide);
	if (bOnRide)
	{
		DisableWobble();
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
	}
	else
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		EnableWobble();
	}
}

// ===========================================================================================
// Raft

ABRRaft::ABRRaft()
{
	if (UStaticMeshComponent* H = GetHull())
	{
		H->SetRelativeScale3D(FVector(3.f, 1.6f, 0.45f));
	}
	SeatOffsets = { FVector(40.f, 0.f, 70.f), FVector(-50.f, 0.f, 70.f) };
}

void ABRRaft::BeginPlay()
{
	Super::BeginPlay();
	if (UStaticMeshComponent* H = GetHull())
	{
		if (UMaterialInstanceDynamic* MID = H->CreateDynamicMaterialInstance(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.95f, 0.45f, 0.08f)); // inflatable orange
		}
	}
}

void ABRRaft::Pop(ABRCharacter* By)
{
	if (!HasAuthority() || IsActorBeingDestroyed())
	{
		return;
	}
	EjectAll();
	ABRGameState::FX(this, EBRFX::Splash, GetActorLocation());
	ABRGameState::FX(this, EBRFX::PointsText, GetActorLocation() + FVector(0.f, 0.f, 150.f), TEXT("POP!"));
	ABRGameState::Sfx(this, EBRSfx::HatPop, GetActorLocation());
	if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
	{
		GM->Feed(FString::Printf(TEXT("%s popped a raft!"), By ? *By->GetPlayerDisplayName() : TEXT("Border Patrol")));
	}
	Destroy();
}

// ===========================================================================================
// Balloon

ABRBalloon::ABRBalloon()
{
	PrimaryActorTick.bCanEverTick = true;
	SetReplicateMovement(true);
	const FLinearColor Wicker(0.55f, 0.38f, 0.2f), Red(0.85f, 0.15f, 0.1f), Yellow(0.95f, 0.8f, 0.15f), Green(0.1f, 0.55f, 0.25f), Rope(0.3f, 0.25f, 0.2f);
	AddMesh(TEXT("Basket"), CubeMesh, FVector(0.f, 0.f, 55.f), FVector(170.f, 170.f, 110.f), Wicker);
	AddMesh(TEXT("Envelope"), SphereMesh, FVector(0.f, 0.f, 900.f), FVector(620.f, 620.f, 720.f), Red, FRotator::ZeroRotator, true);
	AddMesh(TEXT("BandA"), SphereMesh, FVector(0.f, 0.f, 900.f), FVector(630.f, 630.f, 160.f), Yellow, FRotator::ZeroRotator, false);
	AddMesh(TEXT("BandB"), SphereMesh, FVector(0.f, 0.f, 1080.f), FVector(560.f, 560.f, 90.f), Green, FRotator::ZeroRotator, false);
	for (int32 i = 0; i < 4; ++i)
	{
		const float X = (i % 2 == 0 ? 1.f : -1.f) * 75.f;
		const float Y = (i < 2 ? 1.f : -1.f) * 75.f;
		AddMesh(*FString::Printf(TEXT("Rope%d"), i), CylinderMesh, FVector(X * 1.4f, Y * 1.4f, 380.f), FVector(4.f, 4.f, 520.f), Rope,
			FRotator(Y * -0.12f, 0.f, X * 0.12f), false);
	}
	AddLabel(TEXT("Sign"), FVector(0.f, 0.f, 260.f), 40.f, FColor(255, 220, 80))->SetText(FText::FromString(TEXT("GLOBOS PANCHO\nfly over the border")));
	AddPickupZone(260.f);
}

void ABRBalloon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRBalloon, State);
	DOREPLIFETIME(ABRBalloon, Riders);
}

void ABRBalloon::BeginPlay()
{
	Super::BeginPlay();
	PadLocation = GetActorLocation();
}

void ABRBalloon::PlaceRiders()
{
	int32 i = 0;
	for (ABRCharacter* R : Riders)
	{
		if (IsValid(R))
		{
			R->SetActorLocation(GetActorLocation() + FVector(0.f, i == 0 ? -40.f : 40.f, 150.f), false, nullptr, ETeleportType::TeleportPhysics);
		}
		++i;
	}
}

void ABRBalloon::DropRiders(bool bFall)
{
	for (ABRCharacter* R : Riders)
	{
		if (!IsValid(R))
		{
			continue;
		}
		R->SetOnRide(false);
		if (bFall)
		{
			R->MarkFallFrom(R->GetActorLocation().Z); // a long way down...
			R->LaunchCharacter(FVector(FMath::FRandRange(-200.f, 200.f), FMath::FRandRange(-200.f, 200.f), 100.f), true, true);
		}
		else
		{
			R->TeleportTo(GetActorLocation() + FVector(0.f, 180.f, 120.f), R->GetActorRotation(), false, true);
		}
	}
	Riders.Reset();
	ForceNetUpdate();
}

void ABRBalloon::Pop(ABRCharacter* By)
{
	if (!HasAuthority() || !IsFlying())
	{
		return;
	}
	ABRGameState::FX(this, EBRFX::Smoke, GetActorLocation() + FVector(0.f, 0.f, 900.f));
	ABRGameState::FX(this, EBRFX::PointsText, GetActorLocation() + FVector(0.f, 0.f, 1300.f), TEXT("POP!"));
	ABRGameState::Sfx(this, EBRSfx::GateSmash, GetActorLocation());
	if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
	{
		GM->Feed(FString::Printf(TEXT("%s shot down a hot air balloon!"), By ? *By->GetPlayerDisplayName() : TEXT("Border Patrol")));
	}
	DropRiders(true);
	State = 4;
	StateTime = 0.f;
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	ForceNetUpdate();
}

void ABRBalloon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	StateTime += DeltaSeconds;
	Riders.RemoveAll([](const TObjectPtr<ABRCharacter>& R) { return !IsValid(R) || R->GetCondition() != EPVVCondition::Normal; });
	if ((State == 1 || State == 2) && Riders.Num() == 0)
	{
		State = 3; // nobody aboard: it drifts down
	}
	switch (State)
	{
	case 1: // boarding
		PlaceRiders();
		if (StateTime > 8.f || Riders.Num() >= 2)
		{
			State = 2;
			StateTime = 0.f;
			FlyYaw = 0.f;
			if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
			{
				GM->Feed(TEXT("A hot air balloon just took off from town!"));
			}
			ForceNetUpdate();
		}
		break;
	case 2: // flying across
	{
		// The first rider steers a little with the mouse (up to 40 degrees off course).
		if (Riders.Num() > 0 && Riders[0]->GetController())
		{
			const float Want = FMath::Clamp(FRotator::NormalizeAxis(Riders[0]->GetController()->GetControlRotation().Yaw), -40.f, 40.f);
			FlyYaw = FMath::FInterpTo(FlyYaw, Want, DeltaSeconds, 0.6f);
		}
		const FVector Dir = FRotator(0.f, FlyYaw, 0.f).Vector();
		const float Rise = GetActorLocation().Z < PadLocation.Z + CruiseHeight ? 260.f : 0.f;
		SetActorLocation(GetActorLocation() + (Dir * DriftSpeed * (Rise > 0.f ? 0.4f : 1.f) + FVector(0.f, 0.f, Rise)) * DeltaSeconds);
		PlaceRiders();
		if (FMath::Fmod(StateTime, 2.f) < DeltaSeconds)
		{
			for (ABRCharacter* R : Riders)
			{
				R->CameraSpot(3.f); // you can see a balloon from miles away
			}
		}
		float WallX = PadLocation.X + 12000.f;
		for (TActorIterator<ABRWall> It(GetWorld()); It; ++It)
		{
			WallX = It->GetActorLocation().X;
			break;
		}
		if (GetActorLocation().X > WallX + LandPastWall)
		{
			State = 3;
			StateTime = 0.f;
			ForceNetUpdate();
		}
		break;
	}
	case 3: // landing
	{
		FHitResult Ground;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BRBalloon), false, this);
		for (ABRCharacter* R : Riders)
		{
			Params.AddIgnoredActor(R);
		}
		const bool bGround = GetWorld()->LineTraceSingleByChannel(Ground, GetActorLocation() + FVector(0.f, 0.f, 50.f), GetActorLocation() - FVector(0.f, 0.f, 5000.f), ECC_Visibility, Params);
		if (bGround && GetActorLocation().Z - Ground.ImpactPoint.Z < 30.f)
		{
			DropRiders(false);
			State = 4;
			StateTime = 0.f;
			ForceNetUpdate();
			break;
		}
		SetActorLocation(GetActorLocation() + FVector(0.f, 0.f, -260.f * DeltaSeconds));
		PlaceRiders();
		break;
	}
	case 4: // deflated: comes back to the pad after a minute
		if (StateTime > 60.f)
		{
			SetActorLocation(PadLocation);
			SetActorHiddenInGame(false);
			SetActorEnableCollision(true);
			State = 0;
			ForceNetUpdate();
		}
		break;
	default:
		break;
	}
}

bool ABRBalloon::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	const ABRCharacter* R = Cast<ABRCharacter>(Interactor);
	const ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>();
	return R && R->IsRunner() && !R->IsNPC() && R->GetCondition() == EPVVCondition::Normal && !R->GetCurrentBoat() && GS && GS->Phase == EBRPhase::Invasion
		&& (State == 0 || (State == 1 && Riders.Num() < 2 && !Riders.Contains(R)));
}

FText ABRBalloon::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	return State == 0 ? FText::Format(LOCTEXT("BalloonBuy", "Hot air balloon over the border - ${0} (2 seats, lifts off in 8 s)"), Price)
		: LOCTEXT("BalloonJoin", "Hop in the balloon (free - your friend paid)");
}

void ABRBalloon::OnInteract_Implementation(APVVCharacter* Interactor)
{
	ABRCharacter* R = Cast<ABRCharacter>(Interactor);
	ABRPlayerState* PS = R ? R->GetBRPlayerState() : nullptr;
	if (!HasAuthority() || !PS || !CanInteract_Implementation(Interactor))
	{
		return;
	}
	if (State == 0)
	{
		if (PS->Money < Price)
		{
			R->Notify(FText::Format(LOCTEXT("BalloonBroke", "A balloon ride is ${0}."), Price));
			return;
		}
		PS->AddMoney(-Price);
		State = 1;
		StateTime = 0.f;
	}
	Riders.Add(R);
	R->SetOnRide(true);
	PlaceRiders();
	R->Notify(LOCTEXT("BalloonIn", "In the basket! It lifts off soon. Steer a little with the mouse. Border Patrol can SHOOT it down..."));
	ForceNetUpdate();
}

// ===========================================================================================
// Anti-air gun

ABRAntiAir::ABRAntiAir()
{
	const FLinearColor Olive(0.25f, 0.3f, 0.18f), Steel(0.2f, 0.2f, 0.22f);
	AddMesh(TEXT("Base"), CylinderMesh, FVector(0.f, 0.f, 40.f), FVector(160.f, 160.f, 80.f), Olive);
	AddMesh(TEXT("BarrelA"), CylinderMesh, FVector(60.f, -20.f, 150.f), FVector(16.f, 16.f, 220.f), Steel, FRotator(-50.f, 0.f, 0.f), false);
	AddMesh(TEXT("BarrelB"), CylinderMesh, FVector(60.f, 20.f, 150.f), FVector(16.f, 16.f, 220.f), Steel, FRotator(-50.f, 0.f, 0.f), false);
	AddLabel(TEXT("Sign"), FVector(0.f, 0.f, 290.f), 30.f, FColor(255, 220, 80))->SetText(FText::FromString(TEXT("ANTI-AIR")));
	AddPickupZone(220.f);
}

AActor* ABRAntiAir::FindTarget() const
{
	AActor* Best = nullptr;
	float BestD = Range;
	for (TActorIterator<ABRBalloon> It(GetWorld()); It; ++It)
	{
		const float D = FVector::Dist(It->GetActorLocation(), GetActorLocation());
		if (It->IsFlying() && D < BestD)
		{
			BestD = D;
			Best = *It;
		}
	}
	for (TActorIterator<ABRRaft> It(GetWorld()); It; ++It)
	{
		const float D = FVector::Dist(It->GetActorLocation(), GetActorLocation());
		if (D < BestD)
		{
			BestD = D;
			Best = *It;
		}
	}
	return Best;
}

bool ABRAntiAir::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	return Interactor && Interactor->GetFaction() == BR::Wardens && Interactor->GetCondition() == EPVVCondition::Normal && FindTarget() != nullptr;
}

FText ABRAntiAir::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	return GetWorld()->GetTimeSeconds() < NextShot ? LOCTEXT("AAReload", "Anti-air reloading...") : LOCTEXT("AAFire", "FIRE the anti-air gun at the balloon / raft");
}

void ABRAntiAir::OnInteract_Implementation(APVVCharacter* Interactor)
{
	const float Now = GetWorld()->GetTimeSeconds();
	AActor* Target = FindTarget();
	if (!HasAuthority() || !Target || Now < NextShot)
	{
		return;
	}
	NextShot = Now + Cooldown;
	ABRGameState::Sfx(this, EBRSfx::GateSmash, GetActorLocation());
	ABRGameState::FX(this, EBRFX::Smoke, GetActorLocation() + FVector(80.f, 0.f, 250.f));
	if (ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>())
	{
		GS->Multicast_NoisePing(GetActorLocation());
	}
	const bool bHit = FMath::FRand() < HitChance;
	TWeakObjectPtr<AActor> WeakTarget(Target);
	TWeakObjectPtr<ABRCharacter> WeakBy(Cast<ABRCharacter>(Interactor));
	FTimerHandle Flight;
	GetWorldTimerManager().SetTimer(Flight, FTimerDelegate::CreateWeakLambda(this, [this, WeakTarget, WeakBy, bHit]()
	{
		AActor* T = WeakTarget.Get();
		if (!T)
		{
			return;
		}
		ABRGameState::FX(this, EBRFX::Smoke, T->GetActorLocation() + FVector(FMath::FRandRange(-300.f, 300.f), FMath::FRandRange(-300.f, 300.f), 500.f));
		if (bHit)
		{
			BRAir::TryPop(T, WeakBy.Get());
		}
		else if (ABRCharacter* By = WeakBy.Get())
		{
			By->Notify(LOCTEXT("AAMiss", "MISSED! Reloading..."));
		}
	}), 1.2f, false);
}

// ===========================================================================================
// Wheelbarrow

ABRWheelbarrow::ABRWheelbarrow()
{
	PrimaryActorTick.bCanEverTick = true;
	SetReplicateMovement(true);
	const FLinearColor Green(0.2f, 0.45f, 0.25f), Wood(0.45f, 0.3f, 0.18f), Black(0.1f, 0.1f, 0.1f);
	AddMesh(TEXT("Tray"), CubeMesh, FVector(10.f, 0.f, 60.f), FVector(120.f, 80.f, 40.f), Green);
	AddMesh(TEXT("Wheel"), CylinderMesh, FVector(80.f, 0.f, 25.f), FVector(50.f, 50.f, 12.f), Black, FRotator(0.f, 0.f, 90.f), false);
	AddMesh(TEXT("HandleL"), CubeMesh, FVector(-80.f, -30.f, 60.f), FVector(100.f, 6.f, 6.f), Wood, FRotator::ZeroRotator, false);
	AddMesh(TEXT("HandleR"), CubeMesh, FVector(-80.f, 30.f, 60.f), FVector(100.f, 6.f, 6.f), Wood, FRotator::ZeroRotator, false);
	AddPickupZone(170.f);
}

void ABRWheelbarrow::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRWheelbarrow, Pusher);
	DOREPLIFETIME(ABRWheelbarrow, Rider);
}

void ABRWheelbarrow::LetGo()
{
	if (IsValid(Rider))
	{
		Rider->SetOnRide(false);
		Rider->TeleportTo(GetActorLocation() + GetActorRightVector() * 120.f + FVector(0.f, 0.f, 60.f), Rider->GetActorRotation(), false, true);
	}
	Rider = nullptr;
	Pusher = nullptr;
	SetActorEnableCollision(true);
	ForceNetUpdate();
}

void ABRWheelbarrow::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	if (Pusher && (!IsValid(Pusher) || Pusher->GetCondition() != EPVVCondition::Normal || Pusher->GetCurrentBoat()))
	{
		LetGo(); // pusher went down: the barrow tips over
		return;
	}
	if (IsValid(Rider) && Rider->GetCondition() != EPVVCondition::Normal)
	{
		Rider->SetOnRide(false);
		Rider = nullptr;
	}
	if (Pusher)
	{
		// Rolls along in front of whoever's pushing it.
		const FVector Fwd = Pusher->GetActorForwardVector().GetSafeNormal2D();
		const float Half = Pusher->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		SetActorLocationAndRotation(Pusher->GetActorLocation() + Fwd * 130.f - FVector(0.f, 0.f, Half), FRotator(0.f, Pusher->GetActorRotation().Yaw, 0.f));
		Pusher->SlowFor(0.25f, IsValid(Rider) ? 0.8f : 0.95f);
	}
	if (IsValid(Rider))
	{
		Rider->SetActorLocation(GetActorLocation() + FVector(0.f, 0.f, 150.f), false, nullptr, ETeleportType::TeleportPhysics);
	}
}

bool ABRWheelbarrow::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	const ABRCharacter* C = Cast<ABRCharacter>(Interactor);
	if (!C || C->GetCondition() != EPVVCondition::Normal || C->GetCurrentBoat() || C->GetCarriedCharacter())
	{
		return false;
	}
	if (!Pusher)
	{
		return true; // grab the handles
	}
	if (C == Pusher || C == Rider)
	{
		return true; // let go / hop out
	}
	return !Rider && C->GetFaction() == Pusher->GetFaction(); // a teammate hops in
}

FText ABRWheelbarrow::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	if (!Pusher)
	{
		return LOCTEXT("BarrowGrab", "Grab the wheelbarrow");
	}
	if (Interactor == Pusher)
	{
		return LOCTEXT("BarrowLetGo", "Let go of the wheelbarrow");
	}
	if (Interactor == Rider)
	{
		return LOCTEXT("BarrowOut", "Hop out");
	}
	return LOCTEXT("BarrowIn", "Hop in the wheelbarrow");
}

void ABRWheelbarrow::OnInteract_Implementation(APVVCharacter* Interactor)
{
	ABRCharacter* C = Cast<ABRCharacter>(Interactor);
	if (!HasAuthority() || !C || !CanInteract_Implementation(Interactor))
	{
		return;
	}
	if (!Pusher)
	{
		Pusher = C;
		SetActorEnableCollision(false);
	}
	else if (C == Pusher)
	{
		LetGo();
		return;
	}
	else if (C == Rider)
	{
		Rider->SetOnRide(false);
		Rider = nullptr;
	}
	else
	{
		Rider = C;
		C->SetOnRide(true);
	}
	ForceNetUpdate();
}

#undef LOCTEXT_NAMESPACE
