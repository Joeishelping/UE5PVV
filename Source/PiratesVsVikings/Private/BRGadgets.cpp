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
	FBRLook Uniform = Look;
	Uniform.ShirtStyle = 0;
	Uniform.PantsStyle = 0;
	Uniform.ShirtColor = 0.36f;
	Uniform.PantsColor = 0.62f;
	SetLook(Uniform);
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

#undef LOCTEXT_NAMESPACE
