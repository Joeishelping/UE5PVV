#include "BRDog.h"
#include "BRAudio.h"
#include "AIController.h"
#include "BRCharacter.h"
#include "BRDesertArena.h"
#include "BRMapKit.h"
#include "BRGameState.h"
#include "BRPlayerState.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "BRDog"

ABRDog::ABRDog()
{
	PrimaryActorTick.bCanEverTick = true;
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	SetNetCullDistanceSquared(FMath::Square(50000.f));

	GetCapsuleComponent()->InitCapsuleSize(35.f, 40.f);
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 720.f, 0.f);
	Move->MaxWalkSpeed = PatrolSpeed;
	bUseControllerRotationYaw = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Visual = CreateDefaultSubobject<USceneComponent>(TEXT("Visual"));
	Visual->SetupAttachment(GetCapsuleComponent());
	Visual->SetRelativeLocation(FVector(0.f, 0.f, -40.f));
	auto Part = [this](const TCHAR* Name, UStaticMesh* PartMesh, const FVector& At, const FVector& Size, const FRotator& Rot = FRotator::ZeroRotator)
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Visual);
		C->SetStaticMesh(PartMesh);
		C->SetRelativeLocationAndRotation(At, Rot);
		C->SetRelativeScale3D(Size / 100.f);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	};
	Part(TEXT("Body"), Cube.Object, FVector(0.f, 0.f, 45.f), FVector(90.f, 35.f, 35.f));
	Part(TEXT("Head"), Cube.Object, FVector(55.f, 0.f, 65.f), FVector(35.f, 28.f, 28.f));
	Part(TEXT("Snout"), Cube.Object, FVector(78.f, 0.f, 58.f), FVector(20.f, 16.f, 14.f));
	Part(TEXT("EarL"), Cube.Object, FVector(50.f, -12.f, 84.f), FVector(8.f, 6.f, 16.f));
	Part(TEXT("EarR"), Cube.Object, FVector(50.f, 12.f, 84.f), FVector(8.f, 6.f, 16.f));
	Part(TEXT("Tail"), Cylinder.Object, FVector(-52.f, 0.f, 58.f), FVector(6.f, 6.f, 35.f), FRotator(-45.f, 0.f, 0.f));
	Part(TEXT("LegFL"), Cylinder.Object, FVector(32.f, -12.f, 14.f), FVector(9.f, 9.f, 30.f));
	Part(TEXT("LegFR"), Cylinder.Object, FVector(32.f, 12.f, 14.f), FVector(9.f, 9.f, 30.f));
	Part(TEXT("LegBL"), Cylinder.Object, FVector(-32.f, -12.f, 14.f), FVector(9.f, 9.f, 30.f));
	Part(TEXT("LegBR"), Cylinder.Object, FVector(-32.f, 12.f, 14.f), FVector(9.f, 9.f, 30.f));
}

void ABRDog::BeginPlay()
{
	Super::BeginPlay();
	Home = GetActorLocation();
	PickPatrolPoint();

	TArray<UStaticMeshComponent*> Parts;
	GetComponents<UStaticMeshComponent>(Parts);
	for (UStaticMeshComponent* PartComp : Parts)
	{
		if (UMaterialInstanceDynamic* MID = PartComp->CreateDynamicMaterialInstance(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), PartComp->GetName().StartsWith(TEXT("Snout")) ? FLinearColor(0.05f, 0.04f, 0.03f) : FLinearColor(0.35f, 0.22f, 0.1f));
		}
	}
}

void ABRDog::PickPatrolPoint()
{
	const FVector2D Offset = FMath::RandPointInCircle(PatrolRadius);
	PatrolTarget = Home + FVector(Offset.X, Offset.Y, 0.f);
}

void ABRDog::SetHandler(ABRCharacter* NewHandler)
{
	Handler = NewHandler;
}

void ABRDog::ReleaseLeash()
{
	Handler = nullptr;
	Home = GetActorLocation(); // patrols from here now
	NextThink = 0.f; // sniff around right away
}

void ABRDog::Think()
{
	const ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>();
	const bool bHunting = GS && GS->Phase == EBRPhase::Invasion;
	const float Now = GetWorld()->GetTimeSeconds();
	const float ServerNow = GS ? GS->GetServerWorldTimeSeconds() : Now;

	// Who's worth chasing? Lower score = more urgent.
	ABRCharacter* Best = nullptr;
	float BestScore = MAX_flt;
	bool bBestResisting = false;
	for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
	{
		ABRCharacter* C = *It;
		if ((IgnoreTarget.Get() == C && Now < IgnoreUntil) || C->IsWarden() || C->bSparring || C->bPolicia || C->GetCondition() != EPVVCondition::Normal || C->GetCurrentBoat() || C->GetCarrier() || C->IsHidingInSpot() || C->IsDead())
		{
			continue;
		}
		if (BRMap::IsInRiver(GetWorld(), C->GetActorLocation()))
		{
			continue; // water hides your scent
		}
		const float Dist = FVector::Dist(C->GetActorLocation(), GetActorLocation());
		float Score = MAX_flt;
		bool bResisting = false;

		// 1) Somebody is beating up Border Patrol: GET THEM (any phase).
		const ABRCharacter* Victim = C->LastMeleeVictim.Get();
		const ABRCharacter* BrawlTarget = C->IsNPC() && C->IsBrawling() ? C->GetBrawlTarget() : nullptr;
		const ABRCharacter* Attacked = (Victim && Victim->IsWarden() && Now - C->LastMeleeTime < 8.f) ? Victim
			: ((BrawlTarget && BrawlTarget->IsWarden()) ? BrawlTarget : nullptr);
		if (Attacked && Dist < DefendRange)
		{
			Score = Dist * 0.3f;
			bResisting = true;
			if (Attacked == Handler.Get())
			{
				Score *= 0.5f; // nobody touches MY human
			}
		}
		else if (bHunting)
		{
			// BORDER RUSH crowd: the dogs go straight for them (and take them down).
			if (C->bHordeRunner && Dist < SmellRange * 2.f)
			{
				Score = Dist * 0.5f;
				bResisting = true;
			}
			const bool bScannedBad = !C->bHordeRunner && C->IsSuspect() && C->LastScannedTime > -999.f && ServerNow - C->LastScannedTime < ScanMemorySeconds;
			const bool bRunning = C->IsNPC() ? C->IsFleeing() : C->IsSprinting();
			// 2) Scanned as illegal / contraband and now running or fighting: chase them down.
			if (bScannedBad && (bRunning || C->IsBrawling()) && Dist < SmellRange * 2.f)
			{
				Score = Dist * 0.6f;
				bResisting = true;
			}
			// 3) Scanned dirty and standing there: go stand guard on them (barks so Border Patrol can find them).
			else if (bScannedBad && Dist < SmellRange * 1.5f)
			{
				Score = Dist * 1.2f;
			}
			// 4) A Runner player who crossed the Line, sniffed out the usual way.
			else if (!C->IsNPC() && C->IsRunner())
			{
				const ABRPlayerState* PS = C->GetBRPlayerState();
				const float Range = SmellRange * (C->bIsCrouched ? 0.6f : 1.f) * (C->IsSprinting() ? 1.3f : 1.f);
				if (PS && PS->bHasCrossedLine && Dist < Range)
				{
					Score = Dist;
					bResisting = C->IsSprinting();
				}
			}
		}
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = C;
			bBestResisting = bResisting;
		}
	}
	// Chased the same person for ages without catching them: give up and trot back (they're too quick / too far).
	if (Best && Best == Target.Get())
	{
		if (Now - ChaseSince > GiveUpSeconds)
		{
			IgnoreTarget = Best;
			IgnoreUntil = Now + 15.f;
			Best = nullptr;
		}
	}
	else
	{
		ChaseSince = Now;
	}
	Target = Best;
	bTargetResisting = bBestResisting;
	GetCharacterMovement()->MaxWalkSpeed = Best ? ChaseSpeed : PatrolSpeed;
	if (!Best && FVector::Dist2D(GetActorLocation(), PatrolTarget) < 150.f)
	{
		PickPatrolPoint();
	}
}

void ABRDog::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextThink)
	{
		NextThink = Now + 0.4f;
		Think();
	}

	ABRCharacter* Prey = Target.Get();
	FVector Goal = Prey ? Prey->GetActorLocation() : PatrolTarget;
	// On the leash: heel next to the handler, or strain toward a Runner it smells (only as far as the leash lets it).
	if (ABRCharacter* Boss = Handler.Get())
	{
		if (Boss->GetCondition() != EPVVCondition::Normal || Boss->GetCurrentBoat() || Boss->GetLeashedDog() != this)
		{
			Handler = nullptr; // dropped the leash
		}
		else
		{
			const FVector Heel = Boss->GetActorLocation() - Boss->GetActorForwardVector() * 120.f + Boss->GetActorRightVector() * 80.f;
			Goal = Prey ? Prey->GetActorLocation() : Heel;
			const FVector FromBoss = (Goal - Boss->GetActorLocation()) * FVector(1.f, 1.f, 0.f);
			if (FromBoss.Size() > LeashLength)
			{
				Goal = Boss->GetActorLocation() + FromBoss.GetSafeNormal() * LeashLength;
				if (Prey)
				{
					// Pulling hard: drags the handler along a little.
					Boss->AddMovementInput(FromBoss.GetSafeNormal(), 0.35f);
				}
			}
			GetCharacterMovement()->MaxWalkSpeed = Prey ? ChaseSpeed : FMath::Max(PatrolSpeed, Boss->GetVelocity().Size2D() + 60.f);
		}
	}
	const FVector ToGoal = (Goal - GetActorLocation()) * FVector(1.f, 1.f, 0.f);
	if (ToGoal.Size() > 60.f)
	{
		// Run AROUND things (walls, trucks, crates) instead of into them.
		FVector Dir = ToGoal.GetSafeNormal();
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BRDogSteer), false, this);
		auto Blocked = [&](const FVector& D)
		{
			FHitResult Hit;
			const FVector From = GetActorLocation();
			return GetWorld()->SweepSingleByChannel(Hit, From, From + D * 160.f, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(25.f), Params)
				&& !Cast<APawn>(Hit.GetActor()) && Hit.ImpactNormal.Z < 0.7f;
		};
		if (Blocked(Dir))
		{
			const int32 First = SteerSide != 0 ? SteerSide : (FMath::RandBool() ? 1 : -1);
			bool bFound = false;
			for (const float Angle : { 40.f, 80.f, 120.f })
			{
				for (const int32 Sgn : { First, -First })
				{
					const FVector Try = Dir.RotateAngleAxis(Angle * Sgn, FVector::UpVector);
					if (!bFound && !Blocked(Try))
					{
						Dir = Try;
						SteerSide = Sgn;
						bFound = true;
					}
				}
			}
		}
		else
		{
			SteerSide = 0;
		}
		AddMovementInput(Dir);
	}

	if (Prey)
	{
		if (Now >= NextBark)
		{
			NextBark = Now + 3.f;
			Multicast_Bark();
			if (ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>())
			{
				GS->Multicast_NoisePing(GetActorLocation());
			}
		}
		// Guarding someone who's just standing there busted: circle them, don't bite.
		if (!bTargetResisting && Prey->IsNPC() && ToGoal.Size() < 220.f)
		{
			ConsumeMovementInputVector();
			AddMovementInput(FVector::CrossProduct(ToGoal.GetSafeNormal(), FVector::UpVector), 0.4f);
		}
		else if (ToGoal.Size() < 150.f && Now >= NextBite)
		{
			NextBite = Now + 7.f;
			ChaseSince = Now; // got a mouthful: keep at it
			Prey->Spot(6.f);
			ABRGameState::FX(this, EBRFX::PunchHit, Prey->GetActorLocation());
			if (bTargetResisting && FMath::FRand() < 0.45f)
			{
				// Latches on and drags them down: Border Patrol has a few seconds to zip-tie them.
				const FVector Pull = (GetActorLocation() - Prey->GetActorLocation()).GetSafeNormal2D() * 250.f + FVector(0.f, 0.f, 120.f);
				Prey->ApplyKnockdown(this, Pull);
				Prey->SetKnockdownTimeLeft(Prey->IsNPC() ? 3.f : 1.8f);
				Prey->Notify(LOCTEXT("BitDown", "A DOG TOOK YOU DOWN! Get up and shake it off!"));
			}
			else
			{
				Prey->ApplySlow(3.5f, 0.3f);
				Prey->Notify(LOCTEXT("Bit", "A DOG GOT YOUR LEG! Shake it off!"));
			}
		}
	}
}

void ABRDog::Multicast_Bark_Implementation()
{
	if (BarkSound)
	{
		BRAudio::PlayAt(this, BarkSound, GetActorLocation(), BRAudio::Medium);
	}
}

#undef LOCTEXT_NAMESPACE
