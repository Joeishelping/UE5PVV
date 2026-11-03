#include "BRBotController.h"
#include "BRCharacter.h"
#include "BRGameState.h"
#include "BRMapKit.h"
#include "BRPlayerState.h"
#include "BRWorldActors.h"
#include "PVVDetentionZone.h"
#include "PVVInteractable.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	bool IsClimbable(const AActor* A) { return A->IsA<ABRLadder>() || A->IsA<ABRWall>() || A->IsA<ABRWallSegment>() || A->IsA<ABRSpikeFence>() || A->IsA<ABRTunnel>(); }
	bool IsLadder(const AActor* A) { return A->IsA<ABRLadder>(); }
	bool IsPen(const AActor* A) { return A->IsA<APVVDetentionZone>(); }

	bool CanUse(AActor* Thing, APVVCharacter* C)
	{
		return Thing && C && Thing->Implements<UPVVInteractable>() && IPVVInteractable::Execute_CanInteract(Thing, C);
	}
}

ABRBotController::ABRBotController()
{
	bWantsPlayerState = true;
	PrimaryActorTick.bCanEverTick = true;
}

void ABRBotController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	LastPos = InPawn ? InPawn->GetActorLocation() : FVector::ZeroVector;
	StuckTime = 0.f;
	Target = nullptr;
	PatrolUntil = 0.f;
}

ABRCharacter* ABRBotController::Me() const
{
	return Cast<ABRCharacter>(GetPawn());
}

ABRPlayerState* ABRBotController::MyState() const
{
	return GetPlayerState<ABRPlayerState>();
}

void ABRBotController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ABRCharacter* C = Me();
	ABRPlayerState* PS = MyState();
	if (!C || !PS || !HasAuthority())
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();

	// Struggle / wake up like a person mashing a key (about 5 presses a second, not every frame).
	if (C->GetGrabbedBy() && C->GetCondition() == EPVVCondition::Normal && Now >= NextMash)
	{
		NextMash = Now + FMath::FRandRange(0.15f, 0.25f);
		C->BotStruggleGrab(); // wriggle out of their hands
	}
	if (C->GetCondition() == EPVVCondition::Restrained || C->GetCondition() == EPVVCondition::KnockedDown)
	{
		if (Now >= NextMash)
		{
			NextMash = Now + FMath::FRandRange(0.15f, 0.25f);
			if (C->GetCondition() == EPVVCondition::Restrained)
			{
				C->BotStruggle();
			}
			else
			{
				C->BotMash();
			}
		}
		return;
	}

	if (Now >= NextThink)
	{
		NextThink = Now + 0.2f;
		MoveDir = FVector::ZeroVector;
		bWantSprint = false;

		// Stuck? jump, then sidestep.
		const float Moved = FVector::Dist2D(C->GetActorLocation(), LastPos);
		LastPos = C->GetActorLocation();
		const ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>();
		if (!GS || GS->Phase == EBRPhase::Ended || C->GetCondition() != EPVVCondition::Normal || C->GetCarrier() || C->GetCurrentBoat())
		{
			C->BotSetSprinting(false);
			return;
		}
		if (C->IsRunner())
		{
			ThinkRunner(C, PS);
		}
		else
		{
			ThinkWarden(C, PS);
		}
		if (!MoveDir.IsNearlyZero() && !C->IsBusyInteracting())
		{
			if (Moved < 12.f)
			{
				StuckTime += 0.2f;
				if (StuckTime > 0.6f && StuckTime < 0.85f)
				{
					C->Jump();
				}
				if (StuckTime > 1.6f)
				{
					SidestepDir = FVector::CrossProduct(MoveDir, FVector::UpVector) * (FMath::RandBool() ? 1.f : -1.f);
					SidestepUntil = Now + 1.2f;
					StuckTime = 0.f;
				}
			}
			else
			{
				StuckTime = 0.f;
			}
		}
		C->BotSetSprinting(bWantSprint && !C->IsBusyInteracting());
	}

	if (!C->IsBusyInteracting())
	{
		FVector Dir = Now < SidestepUntil ? (SidestepDir + MoveDir * 0.3f) : MoveDir;
		Dir.Z = 0.f;
		if (!Dir.IsNearlyZero())
		{
			C->AddMovementInput(Dir.GetSafeNormal(), 1.f);
			FaceTowards(C->GetActorLocation() + Dir);
		}
	}
}

void ABRBotController::FaceTowards(const FVector& Where)
{
	if (const APawn* P = GetPawn())
	{
		const FVector To = Where - P->GetActorLocation();
		if (!To.IsNearlyZero())
		{
			SetControlRotation(FRotator(0.f, To.Rotation().Yaw, 0.f));
		}
	}
}

bool ABRBotController::SteerTo(ABRCharacter* C, const FVector& Where, float Radius, bool bSprint)
{
	const FVector To = (Where - C->GetActorLocation()) * FVector(1.f, 1.f, 0.f);
	if (To.Size() <= Radius)
	{
		return true;
	}
	MoveDir = To.GetSafeNormal();
	bWantSprint = bSprint;
	return false;
}

bool ABRBotController::GoUse(ABRCharacter* C, AActor* Thing, float UseRange)
{
	if (!Thing)
	{
		return false;
	}
	if (C->IsBusyInteracting())
	{
		return true; // keep holding E
	}
	FVector Closest = Thing->GetActorLocation();
	if (const UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Thing->GetRootComponent()))
	{
		FVector OnShape;
		if (Prim->GetClosestPointOnCollision(C->GetActorLocation(), OnShape) > 0.f)
		{
			Closest = OnShape;
		}
	}
	if (SteerTo(C, Closest, UseRange, false))
	{
		FaceTowards(Thing->GetActorLocation());
		return C->BotInteractWith(Thing);
	}
	return false;
}

AActor* ABRBotController::FindNearestUsable(ABRCharacter* C, float Radius, bool (*Filter)(const AActor*)) const
{
	AActor* Best = nullptr;
	float BestDist = Radius;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* A = *It;
		if (!Filter(A) || !CanUse(A, C))
		{
			continue;
		}
		const float D = FVector::Dist2D(A->GetActorLocation(), C->GetActorLocation());
		if (D < BestDist)
		{
			BestDist = D;
			Best = A;
		}
	}
	return Best;
}

AActor* ABRBotController::FindStation(ABRCharacter* C, ABRPlayerState* PS, bool bJob) const
{
	ABRStation* Best = nullptr;
	float BestDist = MAX_flt;
	for (TActorIterator<ABRStation> It(GetWorld()); It; ++It)
	{
		ABRStation* S = *It;
		bool bWanted = false;
		if (bJob)
		{
			const bool bJunk = S->Kind == EBRStationKind::Job && (S->Price == 0 || S->bAnyoneCanWork);
			bWanted = PS->AssignedJob == TEXT("-") ? bJunk : (S->Kind == EBRStationKind::Job && (S->SignText == PS->AssignedJob || bJunk));
		}
		if (!bWanted)
		{
			continue;
		}
		const float D = FVector::DistSquared(S->GetActorLocation(), C->GetActorLocation());
		if (D < BestDist)
		{
			BestDist = D;
			Best = S;
		}
	}
	return Best;
}

FVector ABRBotController::SafeZoneGoal(const ABRCharacter* C) const
{
	return BRMap::GetSafeZoneGoal(GetWorld(), C->GetActorLocation());
}

// -------------------------------------------------------------------------------------------
// Runner brain

void ABRBotController::ThinkRunner(ABRCharacter* C, ABRPlayerState* PS)
{
	const ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>();
	if (C->GetCondition() == EPVVCondition::Detained || BRMap::IsInSafeZone(GetWorld(), C->GetActorLocation()))
	{
		return;
	}
	if (C->GetHunger() < 35.f && PS->HasItem(EBRItem::Taco))
	{
		C->BotEat();
	}

	if (GS->Phase == EBRPhase::Prep)
	{
		// Shopping list: a ladder, then a taco. Otherwise go to work / sell scrap.
		ABRStation* Shop = nullptr;
		for (TActorIterator<ABRStation> It(GetWorld()); It; ++It)
		{
			const bool bWant = (It->Kind == EBRStationKind::BuyItem && It->Price <= PS->Money && PS->HasFreeSlot()
				&& ((It->Item == EBRItem::Ladder && !PS->HasItem(EBRItem::Ladder)) || (It->Item == EBRItem::Taco && !PS->HasItem(EBRItem::Taco) && PS->HasItem(EBRItem::Ladder))))
				|| (It->Kind == EBRStationKind::SellItem && PS->HasItem(It->Item));
			if (bWant && !It->bNightOnly && (!Shop || FVector::DistSquared(It->GetActorLocation(), C->GetActorLocation()) < FVector::DistSquared(Shop->GetActorLocation(), C->GetActorLocation())))
			{
				Shop = *It;
			}
		}
		if (Shop)
		{
			GoUse(C, Shop);
			return;
		}
		if (AActor* Job = FindStation(C, PS, true))
		{
			GoUse(C, Job); // on a break it just waits next to the counter
		}
		return;
	}

	// --- Invasion: run!
	// Dodge Border Patrol who are close.
	ABRCharacter* Threat = nullptr;
	float ThreatDist = 900.f;
	for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
	{
		if (It->IsWarden() && !It->IsNPC() && It->GetCondition() == EPVVCondition::Normal)
		{
			const float D = FVector::Dist2D(It->GetActorLocation(), C->GetActorLocation());
			if (D < ThreatDist)
			{
				ThreatDist = D;
				Threat = *It;
			}
		}
	}
	const FVector Goal = SafeZoneGoal(C);
	FVector ToGoal = (Goal - C->GetActorLocation()) * FVector(1.f, 1.f, 0.f);
	ToGoal.Normalize();

	// A ladder already leaning on the Wall? Climb it. Got a ladder / cutters / tunnel nearby? Use it.
	if (AActor* Ladder = FindNearestUsable(C, 2000.f, &IsLadder))
	{
		GoUse(C, Ladder, 180.f);
		bWantSprint = true;
		return;
	}
	if (StuckTime > 0.3f || C->IsBusyInteracting())
	{
		if (AActor* Obstacle = FindNearestUsable(C, 420.f, &IsClimbable))
		{
			GoUse(C, Obstacle, 260.f);
			return;
		}
	}
	if (C->IsBusyInteracting())
	{
		return;
	}
	MoveDir = ToGoal;
	if (Threat)
	{
		const FVector Away = ((C->GetActorLocation() - Threat->GetActorLocation()) * FVector(1.f, 1.f, 0.f)).GetSafeNormal();
		MoveDir = (Away * 1.2f + ToGoal).GetSafeNormal();
		if (ThreatDist < 260.f && FMath::FRand() < 0.15f)
		{
			C->BotDive(); // last-second tackle / dodge
		}
	}
	bWantSprint = Threat != nullptr || C->GetHunger() > 30.f;
}

// -------------------------------------------------------------------------------------------
// Border Patrol brain

void ABRBotController::ThinkWarden(ABRCharacter* C, ABRPlayerState* /*PS*/)
{
	const ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>();
	const float Now = GetWorld()->GetTimeSeconds();

	// Carrying a prisoner: to the pen.
	if (C->GetCarriedCharacter())
	{
		if (AActor* Pen = FindNearestUsable(C, 1000000.f, &IsPen))
		{
			GoUse(C, Pen, 280.f);
			bWantSprint = true;
		}
		return;
	}
	if (C->IsBusyInteracting())
	{
		return; // zip-tying
	}

	if (GS->Phase == EBRPhase::Invasion)
	{
		// Pick the closest Runner who crossed that we can see (or who is spotted).
		ABRCharacter* Current = Target.Get();
		const bool bKeep = Current && Current->GetCondition() != EPVVCondition::Detained && !Current->GetCarrier()
			&& FVector::Dist2D(Current->GetActorLocation(), C->GetActorLocation()) < 6000.f;
		if (!bKeep)
		{
			Target = nullptr;
			float Best = 4500.f;
			for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
			{
				ABRCharacter* R = *It;
				const ABRPlayerState* RPS = Cast<ABRPlayerState>(R->GetPVVPlayerState());
				if (!R->IsRunner() || R->IsNPC() || !RPS || !RPS->bHasCrossedLine || R->GetCondition() == EPVVCondition::Detained || R->GetCarrier() || R->IsDisguised())
				{
					continue;
				}
				const float D = FVector::Dist2D(R->GetActorLocation(), C->GetActorLocation());
				FHitResult Hit;
				FCollisionQueryParams Params(SCENE_QUERY_STAT(BRBotSight), false, C);
				Params.AddIgnoredActor(R);
				const bool bSeen = R->IsSpotted() || (D < 3000.f && !GetWorld()->LineTraceSingleByChannel(Hit, C->GetActorLocation() + FVector(0.f, 0.f, 60.f), R->GetActorLocation(), ECC_Visibility, Params));
				if (bSeen && D < Best)
				{
					Best = D;
					Target = R;
				}
			}
		}
		if (ABRCharacter* T = Target.Get())
		{
			const float D = FVector::Dist2D(T->GetActorLocation(), C->GetActorLocation());
			if (T->GetCondition() == EPVVCondition::Normal)
			{
				SteerTo(C, T->GetActorLocation(), 120.f, true);
				if (D < 190.f && Now >= NextAttack)
				{
					FaceTowards(T->GetActorLocation());
					NextAttack = Now + 0.55f;
					if (D > 130.f && FMath::FRand() < 0.2f)
					{
						C->BotDive(); // flying tackle
					}
					else
					{
						C->BotAttack();
					}
				}
			}
			else
			{
				// Down or zip-tied: hold E on them (zip-tie, then pick up).
				GoUse(C, T, 170.f);
			}
			return;
		}
	}

	// Patrol along the Wall / around the base.
	if (Now >= PatrolUntil || SteerTo(C, PatrolPoint, 200.f, false))
	{
		FVector Center = C->GetActorLocation();
		for (TActorIterator<ABRWall> It(GetWorld()); It; ++It)
		{
			Center = It->GetActorLocation() + FVector(600.f, 0.f, 0.f);
			break;
		}
		for (TActorIterator<ABRWallSegment> It(GetWorld()); It; ++It)
		{
			if (FMath::FRand() < 0.5f)
			{
				Center = It->GetActorLocation();
			}
		}
		PatrolPoint = Center + FVector(FMath::FRandRange(-500.f, 1200.f), FMath::FRandRange(-3500.f, 3500.f), 0.f);
		PatrolUntil = Now + 12.f;
	}
}
