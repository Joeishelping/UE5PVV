// Gadgets: smoke bombs, grappling hook, lockpick, fake Border Patrol uniform.

#include "BRGadgets.h"
#include "BRCharacter.h"
#include "BRGameMode.h"
#include "BRGameState.h"
#include "BRMapKit.h"
#include "BRPlayerState.h"
#include "BRWorldActors.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"

#define LOCTEXT_NAMESPACE "BRGadgets"

// ===========================================================================================
// Smoke cloud

ABRSmokeCloud::ABRSmokeCloud()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ABRSmokeCloud::BeginPlay()
{
	Super::BeginPlay();
	SetLifeSpan(Seconds);
}

void ABRSmokeCloud::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (GetNetMode() == NM_DedicatedServer || Age < NextPuff)
	{
		return;
	}
	// Thick puffs all through the cloud (thinning out for the last few seconds).
	NextPuff = Age + (Age > Seconds - 3.f ? 0.25f : 0.07f);
	for (int32 i = 0; i < 3; ++i)
	{
		const FVector P = GetActorLocation() + FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f).GetClampedToMaxSize(1.f) * Radius
			+ FVector(0.f, 0.f, FMath::FRandRange(20.f, 220.f));
		ABRGameState::SpawnLocalFX(this, EBRFX::Smoke, P, FString(), FMath::FRandRange(2.5f, 4.f));
	}
}

bool ABRSmokeCloud::IsInSmoke(const UWorld* World, const FVector& Location)
{
	if (!World)
	{
		return false;
	}
	for (TActorIterator<ABRSmokeCloud> It(World); It; ++It)
	{
		if (FVector::Dist2D(It->GetActorLocation(), Location) < It->Radius && FMath::Abs(Location.Z - It->GetActorLocation().Z) < 400.f)
		{
			return true;
		}
	}
	return false;
}

// ===========================================================================================
// Character side

void ABRCharacter::ThrowSmokeBomb()
{
	ABRPlayerState* PS = GetBRPlayerState();
	if (!HasAuthority() || !PS || !PS->HasItem(EBRItem::SmokeBomb) || GetCurrentBoat() || GetCondition() != EPVVCondition::Normal)
	{
		return;
	}
	// Lands about 5 m in front of you (on the ground).
	const FVector Fwd = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f).Vector();
	FVector Land = GetActorLocation() + Fwd * 500.f;
	FHitResult Ground;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BRSmoke), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Ground, Land + FVector(0.f, 0.f, 200.f), Land - FVector(0.f, 0.f, 800.f), ECC_Visibility, Params))
	{
		Land = Ground.ImpactPoint;
	}
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (GetWorld()->SpawnActor<ABRSmokeCloud>(ABRSmokeCloud::StaticClass(), Land, FRotator::ZeroRotator, SP))
	{
		PS->RemoveItem(EBRItem::SmokeBomb);
		ABRGameState::Sfx(this, EBRSfx::HatPop, Land);
		Notify(LOCTEXT("SmokeOut", "SMOKE! Anyone inside the cloud can't be scanned, spotted or seen by cameras."));
	}
}

void ABRCharacter::UseGrapple()
{
	ABRPlayerState* PS = GetBRPlayerState();
	const float Now = GetWorld()->GetTimeSeconds();
	if (!HasAuthority() || !PS || !PS->HasItem(EBRItem::GrappleHook) || GetCurrentBoat() || GetCondition() != EPVVCondition::Normal || bClimbing
		|| Now - LastGrappleTime < 6.f)
	{
		return;
	}
	// Fire the hook where you're looking (up to 25 m): it has to catch a wall / fence / roof edge.
	const FVector Eye = GetActorLocation() + FVector(0.f, 0.f, 60.f);
	const FVector Dir = GetBaseAimRotation().Vector();
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BRGrapple), false, this);
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + Dir * 2500.f, ECC_Visibility, Params) || Cast<APawn>(Hit.GetActor())
		|| FMath::Abs(Hit.ImpactNormal.Z) > 0.5f)
	{
		Notify(LOCTEXT("GrappleMiss", "The hook found nothing to catch - aim at a wall or fence."));
		return;
	}
	const FVector Normal = Hit.ImpactNormal.GetSafeNormal2D();
	// Find the top edge above the hit.
	FHitResult Top;
	const FVector Over = Hit.ImpactPoint - Normal * 40.f;
	if (!GetWorld()->LineTraceSingleByChannel(Top, Over + FVector(0.f, 0.f, 1500.f), Over - FVector(0.f, 0.f, 200.f), ECC_Visibility, Params) || Top.bStartPenetrating)
	{
		Notify(LOCTEXT("GrappleNoTop", "Too tall - the hook can't reach the top."));
		return;
	}
	LastGrappleTime = Now;
	const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float R = GetCapsuleComponent()->GetScaledCapsuleRadius();
	// Zip up the rope and hang off the top (SPACE pulls you over).
	SetActorRotation(FRotator(0.f, (-Normal).Rotation().Yaw, 0.f));
	SetActorLocation(FVector(Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Top.ImpactPoint.Z - 90.f - Half * 0.2f) + Normal * (R + 6.f), false, nullptr, ETeleportType::TeleportPhysics);
	if (StartWallClimb())
	{
		bHanging = true;
		Notify(LOCTEXT("GrappleUp", "ZIP! You're at the top - SPACE to pull yourself over."));
	}
	Spot(4.f); // the clank of the hook gives you away
	ABRGameState::Sfx(this, EBRSfx::HatPop, Hit.ImpactPoint);
	if (Hit.GetActor() && (Hit.GetActor()->IsA<ABRWall>() || Hit.GetActor()->IsA<ABRWallSegment>()))
	{
		if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
		{
			GM->TriggerBorderAlarm(Hit.ImpactPoint, TEXT("A grappling hook hit the Wall"));
		}
	}
}

void ABRCharacter::ToggleFakeUniform()
{
	ABRPlayerState* PS = GetBRPlayerState();
	if (!HasAuthority() || !PS || !IsRunner() || bIsNPC)
	{
		return;
	}
	if (bFakeUniform)
	{
		TakeOffFakeUniform(false);
		return;
	}
	if (!PS->HasItem(EBRItem::FakeUniform))
	{
		return;
	}
	SavedLook = Look;
	SavedHat = GetHat();
	bFakeUniform = true;
	UniformOverride = 3; // the real Border Patrol uniform (vest, BORDER PATROL on the back)
	ApplyLook(Look);
	SetHat(EBRHat::PatrolCap);
	ForceNetUpdate();
	Notify(LOCTEXT("UniformOn", "You're in a Border Patrol uniform. Scans say 'fellow agent' and the BP door opens for you. Don't fight or sprint near them!"));
}

void ABRCharacter::TakeOffFakeUniform(bool bBlown)
{
	if (!HasAuthority() || !bFakeUniform)
	{
		return;
	}
	bFakeUniform = false;
	UniformOverride = 0;
	SetLook(SavedLook);
	SetHat(SavedHat);
	if (bBlown)
	{
		if (ABRPlayerState* PS = GetBRPlayerState())
		{
			PS->RemoveItem(EBRItem::FakeUniform);
		}
		Spot(8.f);
		Notify(LOCTEXT("UniformBlown", "Your cover is BLOWN - they know you're not one of them!"));
	}
	ForceNetUpdate();
}

// ===========================================================================================
// TNT

ABRTNTCharge::ABRTNTCharge()
{
	PrimaryActorTick.bCanEverTick = true;
	const FLinearColor Red(0.75f, 0.1f, 0.08f);
	for (int32 i = 0; i < 3; ++i)
	{
		AddMesh(*FString::Printf(TEXT("Stick%d"), i), CylinderMesh, FVector(0.f, -12.f + i * 12.f, 25.f), FVector(11.f, 11.f, 50.f), Red, FRotator::ZeroRotator, false);
	}
	AddMesh(TEXT("Timer"), CubeMesh, FVector(8.f, 0.f, 30.f), FVector(6.f, 22.f, 14.f), FLinearColor(0.1f, 0.1f, 0.1f), FRotator::ZeroRotator, false);
}

void ABRTNTCharge::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (GetNetMode() != NM_DedicatedServer && FMath::Fmod(Age, 0.5f) < DeltaSeconds)
	{
		ABRGameState::SpawnLocalFX(this, EBRFX::Sparks, GetActorLocation() + FVector(0.f, 0.f, 55.f));
	}
	if (!HasAuthority())
	{
		return;
	}
	const int32 Left = FMath::CeilToInt(FuseSeconds - Age);
	if (Left != LastBeep && Left > 0)
	{
		LastBeep = Left;
		ABRGameState::Sfx(this, EBRSfx::Countdown, GetActorLocation());
		ABRGameState::FX(this, EBRFX::PointsText, GetActorLocation() + FVector(0.f, 0.f, 90.f), FString::FromInt(Left));
	}
	if (Age >= FuseSeconds)
	{
		Explode();
	}
}

void ABRTNTCharge::Explode()
{
	const FVector At = GetActorLocation();
	ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>();
	ABRCharacter* By = PlantedBy.Get();
	for (int32 i = 0; i < 4; ++i)
	{
		ABRGameState::FX(this, EBRFX::Smoke, At + FMath::VRand() * 120.f + FVector(0.f, 0.f, 100.f));
	}
	ABRGameState::FX(this, EBRFX::Sparks, At + FVector(0.f, 0.f, 60.f));
	ABRGameState::Sfx(this, EBRSfx::GateSmash, At, false);
	// Everyone nearby goes flying (Gang Beasts ragdolls everywhere).
	for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
	{
		ABRCharacter* C = *It;
		const FVector To = C->GetActorLocation() - At;
		const float D = To.Size();
		if (D > BlastRadius || C->GetCurrentBoat() || C->GetCarrier())
		{
			continue;
		}
		const float K = 1.f - D / BlastRadius;
		C->ApplyKnockdown(By, To.GetSafeNormal2D() * 500.f * K + FVector(0.f, 0.f, 350.f * K + 100.f));
		C->AddKO(0.5f * K, By);
	}
	// The Wall: blow out the panels it was against.
	bool bBreach = false;
	for (TActorIterator<ABRWall> It(GetWorld()); It; ++It)
	{
		bBreach |= It->Blast(At, BlastRadius * 0.6f);
	}
	// Fences nearby are wrecked.
	TArray<AActor*> Wrecked;
	for (TActorIterator<ABRSpikeFence> It(GetWorld()); It; ++It)
	{
		if (FVector::Dist(It->GetActorLocation(), At) < BlastRadius * 0.7f)
		{
			Wrecked.Add(*It);
		}
	}
	for (AActor* F : Wrecked)
	{
		F->Destroy();
	}
	if (GM)
	{
		GM->TriggerBorderAlarm(At, bBreach ? TEXT("EXPLOSION - the Wall is BREACHED") : TEXT("An explosion at the border"));
		if (bBreach)
		{
			GM->Feed(FString::Printf(TEXT("%s blew a hole in the Wall!"), By ? *By->GetPlayerDisplayName() : TEXT("Someone")));
		}
	}
	if (ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>())
	{
		GS->Multicast_NoisePing(At);
	}
	Destroy();
}

void ABRCharacter::PlantTNT()
{
	ABRPlayerState* PS = GetBRPlayerState();
	if (!HasAuthority() || !PS || !PS->HasItem(EBRItem::TNT) || GetCurrentBoat() || GetCondition() != EPVVCondition::Normal || bClimbing)
	{
		return;
	}
	const FVector Feet = GetActorLocation() + GetActorForwardVector() * 70.f - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ABRTNTCharge* Charge = GetWorld()->SpawnActor<ABRTNTCharge>(ABRTNTCharge::StaticClass(), Feet, GetActorRotation(), SP))
	{
		Charge->PlantedBy = this;
		PS->RemoveItem(EBRItem::TNT);
		BlowDisguise();
		Notify(LOCTEXT("TNTPlanted", "TNT planted - 5 seconds - RUN!"));
	}
}

void ABRCharacter::UseFuseKit()
{
	ABRPlayerState* PS = GetBRPlayerState();
	ABRGameState* GS = GetBRGameState();
	if (!HasAuthority() || !PS || !GS || !PS->HasItem(EBRItem::FuseKit))
	{
		return;
	}
	if (GS->GetServerWorldTimeSeconds() >= GS->FencePowerUntil)
	{
		Notify(LOCTEXT("FuseNotLive", "The fences aren't electrified right now - save the fuse kit."));
		return;
	}
	PS->RemoveItem(EBRItem::FuseKit);
	GS->FencePowerCutUntil = GS->GetServerWorldTimeSeconds() + 45.f;
	GS->ForceNetUpdate();
	if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
	{
		GM->Feed(TEXT("Someone blew the fuses - the fence power is OFF for 45 s! (the spikes still hurt)"));
	}
}

#undef LOCTEXT_NAMESPACE
