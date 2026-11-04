// Active ragdoll for ABRCharacter (Gang Beasts style).
//
// The capsule + CharacterMovement still do the walking (that's what keeps multiplayer solid). The body on top is a
// physics puppet: everything below the pelvis simulates and is driven toward the animation by muscles (see
// EnableWobble / ApplyMuscles), and here we add:
//   * BALANCE: the whole body leans like an inverted pendulum around your feet. Hits, shoves and your own starts,
//     stops and turns tip you; your legs pull you back upright. Lean past FallOverAngle and you fall (server).
//     Getting beaten up, being dazed or drunk makes you wobblier.
//   * PHYSICAL GRABS: each grabbing hand is tied to whoever it holds (or the wall it hangs on) with a physics
//     constraint, so arms stretch, bodies tug and swing.

#include "BRCharacter.h"
#include "BRPlayerState.h"
#include "BRGameState.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "BRGameMode.h"
#include "TimerManager.h"

void ABRCharacter::AddLean(const FVector& WorldPush, ABRCharacter* By)
{
	if (!bActiveRagdoll)
	{
		return;
	}
	const FVector Flat(WorldPush.X, WorldPush.Y, 0.f);
	BodyLeanVel += Flat * HitTip;
	if (By && By != this)
	{
		LastLeanPusher = By;
	}
}

void ABRCharacter::TickActiveRagdoll(float DeltaSeconds)
{
	USkeletalMeshComponent* Body = GetMesh();
	if (!Body || DeltaSeconds <= 0.f)
	{
		return;
	}
	// Diving: the whole body goes flat out in front of you (superman), arms first.
	if (bDiving && GetCondition() == EPVVCondition::Normal && !IsRagdollActive() && !GetCurrentBoat())
	{
		DiveTilt = FMath::FInterpTo(DiveTilt, 72.f, DeltaSeconds, 12.f);
		Body->SetRelativeRotation(FQuat(FVector(0.f, 1.f, 0.f), FMath::DegreesToRadians(DiveTilt)) * GetMeshRelativeTransform().GetRotation());
		bLeanApplied = true;
		UpdateGrabTethers();
		return;
	}
	DiveTilt = 0.f;
	const bool bStanding = bActiveRagdoll && GetCondition() == EPVVCondition::Normal && !GetCurrentBoat() && !GetCarrier()
		&& !bClimbing && !bOnShoulders && !bInCannon && !bHiddenInSpot && !IsRagdollActive();
	if (!bStanding)
	{
		// Not balancing (down, riding, climbing...): straighten the body back to normal.
		BodyLean = FVector::ZeroVector;
		BodyLeanVel = FVector::ZeroVector;
		if (bLeanApplied && !IsRagdollActive())
		{
			bLeanApplied = false;
			Body->SetRelativeRotation(GetMeshRelativeTransform().GetRotation());
		}
		LastLeanVelocity = GetVelocity();
		UpdateGrabTethers();
		return;
	}
	// Your own momentum: start, stop or turn sharply and you tip the other way.
	const FVector Vel = GetVelocity();
	FVector Accel = (Vel - LastLeanVelocity) / DeltaSeconds;
	Accel.Z = 0.f;
	LastLeanVelocity = Vel;
	if (Accel.Size() < 6000.f) // ignore teleports
	{
		BodyLeanVel += -Accel * MoveTip * DeltaSeconds;
	}
	// Pull yourself upright (weaker when you're hurt, dazed or drunk).
	float Steady = FMath::Lerp(0.45f, 1.f, GetBalanceFraction());
	Steady *= IsDazed() ? 0.45f : 1.f;
	Steady *= IsTipsy() ? 0.7f : 1.f;
	Steady *= bBlocking ? 1.4f : 1.f; // feet planted behind your guard
	const float K = BalanceStiffness * Steady;
	// Lean INTO the way you're walking (Gang Beasts: the body leads, the legs catch up).
	const FVector LeanTarget = FVector(Vel.X, Vel.Y, 0.f) * (LeanIntoMove / 100.f);
	BodyLeanVel += (-(BodyLean - LeanTarget) * K - BodyLeanVel * BalanceDamping) * DeltaSeconds;
	BodyLean += BodyLeanVel * DeltaSeconds;
	BodyLean.Z = 0.f;
	const float Angle = BodyLean.Size();
	const float OffBalance = (BodyLean - LeanTarget).Size(); // only being knocked off your lean counts
	// Too far over: you GO DOWN (the server decides).
	if (HasAuthority() && OffBalance > FallOverAngle && GetWorld()->GetTimeSeconds() >= BalanceGraceUntil)
	{
		const FVector Dir = (BodyLean - LeanTarget).GetSafeNormal();
		BodyLean = FVector::ZeroVector;
		BodyLeanVel = FVector::ZeroVector;
		ApplyKnockdown(LastLeanPusher.Get(), Dir * 350.f + FVector(0.f, 0.f, 80.f));
		return;
	}
	// Show it: tilt the whole body around the feet.
	const FVector LocalLean = GetActorRotation().UnrotateVector(FVector(BodyLean.X, BodyLean.Y, 0.f));
	FQuat Tilt = FQuat::Identity;
	if (Angle > 0.05f)
	{
		const FVector Axis = FVector::CrossProduct(FVector::UpVector, LocalLean.GetSafeNormal());
		Tilt = FQuat(Axis.GetSafeNormal(), FMath::DegreesToRadians(FMath::Min(Angle, FallOverAngle)));
	}
	Body->SetRelativeRotation(Tilt * GetMeshRelativeTransform().GetRotation());
	bLeanApplied = true;
	UpdateGrabTethers();
}

namespace
{
	// The parts of a limp body you can drag it by.
	const TCHAR* const DragBoneNames[] = { TEXT("hand_l"), TEXT("hand_r"), TEXT("foot_l"), TEXT("foot_r"), TEXT("spine_03"), TEXT("head") };
	constexpr int32 NumDragBones = UE_ARRAY_COUNT(DragBoneNames);
}

void ABRCharacter::UpdateGrabTethers()
{
	USkeletalMeshComponent* Body = GetMesh();
	if (!Body || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	static const FName HandBones[2] = { FName(TEXT("hand_l")), FName(TEXT("hand_r")) };
	const ABRCharacter* Held = HandGrabTarget.Get();
	for (int32 i = 0; i < 2; ++i)
	{
		// What should this hand be tied to?
		UPrimitiveComponent* WantOther = nullptr;
		FName OtherBone = NAME_None;
		FVector Anchor = FVector::ZeroVector;
		bool bWorld = false;
		const bool bHandUsable = bPhysicalGrabs && bWobbleOn && Body->GetBoneIndex(HandBones[i]) != INDEX_NONE && Body->IsSimulatingPhysics(PhysBone(HandBones[i]));
		if (bHandUsable && bClimbing)
		{
			// Hands on the wall: the hand you moved last is higher.
			const bool bThisHigh = (i == 0) == bLastClimbLeft;
			const FVector Wall = -FVector(ClimbNormal);
			const FVector Side = FVector::CrossProduct(FVector::UpVector, Wall).GetSafeNormal() * (i == 0 ? -22.f : 22.f);
			Anchor = GetActorLocation() + Wall * (GetCapsuleComponent()->GetScaledCapsuleRadius() + 4.f) + Side + FVector(0.f, 0.f, bThisHigh ? 105.f : 72.f);
			bWorld = true;
		}
		else if (bHandUsable && Held && !bLiftOverhead && (HandGrabMask & (1 << i)) && Held->GetMesh() && Held->GetMesh()->GetBoneIndex(TEXT("spine_03")) != INDEX_NONE)
		{
			// Hold on to the part you actually grabbed (head, arm, leg, collar) - standing OR limp: your hand is really on them.
			FName Part = (HandGrabBone != NAME_None && Held->GetMesh()->GetBoneIndex(HandGrabBone) != INDEX_NONE) ? HandGrabBone : FName(TEXT("spine_03"));
			if (Held->IsRagdollActive() && DragBone < NumDragBones)
			{
				Part = DragBoneNames[DragBone];
			}
			WantOther = Held->GetMesh();
			OtherBone = Part;
			Anchor = Held->GetMesh()->GetBoneLocation(Part) + (Body->GetBoneLocation(HandBones[i]) - Held->GetMesh()->GetBoneLocation(Part)).GetClampedToMaxSize(15.f);
		}
		const bool bWant = bWorld || WantOther;
		UPhysicsConstraintComponent* Tether = GrabTether[i];
		const bool bSame = Tether && bWant && bTetherToWorld[i] == bWorld && TetherOther[i].Get() == WantOther
			&& (!bWorld || FVector::DistSquared(TetherAnchor[i], Anchor) < 15.f * 15.f);
		if (bSame)
		{
			continue;
		}
		// Re-tie (or untie) this hand.
		if (Tether)
		{
			Tether->BreakConstraint();
			Tether->DestroyComponent();
			GrabTether[i] = nullptr;
		}
		TetherOther[i] = WantOther;
		bTetherToWorld[i] = bWorld;
		TetherAnchor[i] = Anchor;
		if (!bWant)
		{
			continue;
		}
		Tether = NewObject<UPhysicsConstraintComponent>(this);
		Tether->SetWorldLocation(Anchor);
		Tether->RegisterComponent();
		// Hands stay within a few cm of the grip point; everything can still rotate (floppy wrists).
		Tether->SetLinearXLimit(ELinearConstraintMotion::LCM_Limited, 4.f);
		Tether->SetLinearYLimit(ELinearConstraintMotion::LCM_Limited, 4.f);
		Tether->SetLinearZLimit(ELinearConstraintMotion::LCM_Limited, 4.f);
		Tether->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Free, 0.f);
		Tether->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Free, 0.f);
		Tether->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Free, 0.f);
		Tether->SetDisableCollision(true);
		Tether->SetConstrainedComponents(Body, PhysBone(HandBones[i]), WantOther, Held && WantOther ? Held->PhysBone(OtherBone) : OtherBone); // null other = tied to the world (the wall)
		GrabTether[i] = Tether;
	}
}

// -------------------------------------------------------------------------------------------
// Dragging limp bodies + lifting people over your head

bool ABRCharacter::IsDraggingBody() const
{
	const ABRCharacter* Held = HandGrabTarget.Get();
	return Held && (bLiftOverhead || Held->GetCondition() == EPVVCondition::KnockedDown);
}

void ABRCharacter::PickDragBone()
{
	ABRCharacter* Held = HandGrabTarget.Get();
	USkeletalMeshComponent* Their = Held ? Held->GetMesh() : nullptr;
	if (!Their)
	{
		return;
	}
	// The part you grabbed (if it's one we can haul by), else whatever is closest to your hands.
	for (int32 i = 0; i < NumDragBones; ++i)
	{
		if (HandGrabBone == FName(DragBoneNames[i]) && Their->GetBoneIndex(DragBoneNames[i]) != INDEX_NONE)
		{
			DragBone = static_cast<uint8>(i);
			ForceNetUpdate();
			return;
		}
	}
	const FVector Hands = GetMesh() && GetMesh()->GetBoneIndex(TEXT("hand_r")) != INDEX_NONE ? GetMesh()->GetBoneLocation(TEXT("hand_r"))
		: GetActorLocation() + GetActorForwardVector().GetSafeNormal2D() * 70.f;
	float Best = MAX_flt;
	uint8 Pick = 4; // spine (the collar)
	for (int32 i = 0; i < NumDragBones; ++i)
	{
		if (Their->GetBoneIndex(DragBoneNames[i]) == INDEX_NONE)
		{
			continue;
		}
		const float D = FVector::DistSquared(Their->GetBoneLocation(DragBoneNames[i]), Hands);
		if (D < Best)
		{
			Best = D;
			Pick = static_cast<uint8>(i);
		}
	}
	DragBone = Pick;
	ForceNetUpdate();
}

FVector ABRCharacter::GetHoldPoint() const
{
	const FVector Fwd = GetActorForwardVector().GetSafeNormal2D();
	const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	// Dragging: low in front of you, at hand height.
	const FVector Low = GetActorLocation() + Fwd * 80.f - FVector(0.f, 0.f, Half * 0.35f);
	if (!bLiftOverhead)
	{
		return Low;
	}
	// Lifting: heave them up from your hands to above your head.
	const float Alpha = FMath::Clamp((GetWorld()->GetTimeSeconds() - LiftStartTime) / FMath::Max(0.05f, LiftSeconds), 0.f, 1.f);
	const FVector High = GetActorLocation() + Fwd * 35.f + FVector(0.f, 0.f, Half + 40.f); // a bit in front (out of the camera's way)
	return FMath::Lerp(Low, High, FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f));
}

void ABRCharacter::DragHeldBody(float DeltaSeconds)
{
	// Slow down while hauling (all machines, so movement prediction agrees).
	const bool bSlow = IsDraggingBody();
	if (bSlow != bDragSlowed)
	{
		bDragSlowed = bSlow;
		UpdateMovementSpeed();
	}
	ABRCharacter* Held = HandGrabTarget.Get();
	USkeletalMeshComponent* Their = Held ? Held->GetMesh() : nullptr;
	if (!Their || HandGrabMask == 0 || !Held->IsRagdollActive() || !Their->IsSimulatingPhysics() || DeltaSeconds <= 0.f)
	{
		return;
	}
	// Pull the gripped part toward your hands like a spring: the rest of the body flops along behind it.
	auto Pull = [&](const TCHAR* Bone, const FVector& Target, float Strength)
	{
		if (Their->GetBoneIndex(Bone) == INDEX_NONE)
		{
			return;
		}
		FBodyInstance* BI = Their->GetBodyInstance(Bone);
		if (!BI || !Their->IsSimulatingPhysics(Bone))
		{
			return;
		}
		const FVector Pos = BI->GetUnrealWorldTransform().GetLocation();
		FVector Want = (Target - Pos) * Strength + GetVelocity();
		Want = Want.GetClampedToMaxSize(1500.f);
		const FVector Cur = BI->GetUnrealWorldVelocity();
		BI->SetLinearVelocity(FMath::Lerp(Cur, Want, FMath::Clamp(DeltaSeconds * 14.f, 0.f, 1.f)), false);
	};
	const FVector Target = GetHoldPoint();
	if (bLiftOverhead)
	{
		// Over your head: lying across your shoulders, hips one side, chest the other (arms and legs dangle).
		const FVector Side = FVector::CrossProduct(FVector::UpVector, GetActorForwardVector().GetSafeNormal2D()) * 35.f;
		Pull(TEXT("spine_03"), Target + Side, DragStrength * 1.4f);
		Pull(TEXT("pelvis"), Target - Side, DragStrength * 1.4f);
		Pull(TEXT("head"), Target + Side * 1.8f, DragStrength * 0.5f);
		return;
	}
	const int32 Index = DragBone < NumDragBones ? DragBone : 4;
	// Pull the part you're holding to where your gripping hand actually is.
	FVector HandTarget = Target;
	if (USkeletalMeshComponent* Mine = GetMesh(); Mine && bWobbleOn)
	{
		const FName GripHand = (HandGrabMask & 1) ? FName(TEXT("hand_l")) : FName(TEXT("hand_r"));
		if (Mine->GetBoneIndex(GripHand) != INDEX_NONE)
		{
			HandTarget = Mine->GetBoneLocation(GripHand);
		}
	}
	Pull(DragBoneNames[Index], HandTarget, DragStrength);
}

void ABRCharacter::Server_Lift_Implementation()
{
	ABRCharacter* Held = HandGrabTarget.Get();
	if (!Held || GetCondition() != EPVVCondition::Normal || bClimbing || GetCurrentBoat())
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (bLiftOverhead)
	{
		if (Now - LiftStartTime < LiftSeconds)
		{
			return; // still heaving
		}
		// BODY SLAM: drive them into the ground in front of you.
		const FVector Fwd = GetActorForwardVector().GetSafeNormal2D();
		ReleaseHandGrab();
		Held->LastTosser = this;
		Held->LastTossTime = Now;
		Held->Multicast_FlingBody(Fwd * 300.f - FVector(0.f, 0.f, 1400.f));
		Held->ExtendKnockdown(2.5f);
		AddShake(0.7f);
		Multicast_Sfx(1);
		ABRGameState::Highlight(this, Held, 80.f, FString::Printf(TEXT("%s BODY SLAMMED %s"), *GetPlayerDisplayName(), *Held->GetPlayerDisplayName()));
		if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
		{
			GM->Feed(FString::Printf(TEXT("%s BODY SLAMMED %s!"), *GetPlayerDisplayName(), *Held->GetPlayerDisplayName()));
		}
		return;
	}
	if (HandGrabMask != 3)
	{
		return; // need both hands
	}
	// Somebody still on their feet has to be worn down first (or caught in a chokehold).
	if (Held->GetCondition() == EPVVCondition::Normal)
	{
		if (Held->GetBalanceFraction() > 0.35f && !Held->bChoked && !Held->IsDazed())
		{
			Notify(NSLOCTEXT("BR", "LiftTooStrong", "They're too fresh to lift! Wear them down first (punch them, chokehold, or knock them dizzy)."));
			return;
		}
		Held->bChoked = false;
		Held->ApplyKnockdown(this, FVector(0.f, 0.f, 50.f));
		if (Held->GetCondition() != EPVVCondition::KnockedDown)
		{
			return;
		}
	}
	bLiftOverhead = true;
	LiftStartTime = Now;
	Held->ExtendKnockdown(1.5f);
	Multicast_Sfx(5);
	Notify(NSLOCTEXT("BR", "Lifted", "UP THEY GO! Z = hurl them, SPACE = body slam, let go of the mouse = drop them."));
	ForceNetUpdate();
}

void ABRCharacter::EndLift()
{
	if (bLiftOverhead)
	{
		bLiftOverhead = false;
		ForceNetUpdate();
	}
}

void ABRCharacter::Multicast_FlingBody_Implementation(FVector_NetQuantize Velocity)
{
	const FVector Clamped = FVector(Velocity).GetClampedToMaxSize(MaxFlingSpeed);
	Velocity = Clamped;
	USkeletalMeshComponent* Body = GetMesh();
	if (Body && IsRagdollActive() && Body->IsSimulatingPhysics())
	{
		Body->SetAllPhysicsLinearVelocity(FVector(Velocity), false);
	}
	else
	{
		AddLean(FVector(Velocity), nullptr);
	}
	if (HasAuthority() && !IsRagdollActive())
	{
		LaunchCharacter(FVector(Velocity), true, true);
	}
}

void ABRCharacter::Multicast_IgnoreVehicles_Implementation(uint8 Channel, float Seconds)
{
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!Capsule)
	{
		return;
	}
	const ECollisionChannel Chan = static_cast<ECollisionChannel>(Channel);
	if (IgnoredVehicleChannel == 255)
	{
		Capsule->SetCollisionResponseToChannel(Chan, ECR_Ignore);
		IgnoredVehicleChannel = Channel;
	}
	TWeakObjectPtr<ABRCharacter> WeakThis(this);
	GetWorldTimerManager().SetTimer(IgnoreVehiclesTimer, FTimerDelegate::CreateLambda([WeakThis]()
	{
		ABRCharacter* Self = WeakThis.Get();
		if (Self && Self->IgnoredVehicleChannel != 255 && Self->GetCapsuleComponent())
		{
			Self->GetCapsuleComponent()->SetCollisionResponseToChannel(static_cast<ECollisionChannel>(Self->IgnoredVehicleChannel), ECR_Block);
			Self->IgnoredVehicleChannel = 255;
		}
	}), FMath::Max(0.1f, Seconds), false);
}

// -------------------------------------------------------------------------------------------
// Knockouts (Gang Beasts): head hits fill a KO meter; full = out cold

void ABRCharacter::AddKO(float Amount, ABRCharacter* By)
{
	if (!HasAuthority() || Amount <= 0.f || GetCondition() != EPVVCondition::Normal || bPeaceful)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	// Gym: a strong puncher hits harder (+10% per level), a strong body takes it better (-6% per level).
	float GymScale = 1.f;
	if (const ABRPlayerState* ByPS = By ? By->GetBRPlayerState() : nullptr)
	{
		GymScale *= 1.f + 0.1f * ByPS->GymStrength;
	}
	if (const ABRPlayerState* MyPS = GetBRPlayerState())
	{
		GymScale /= 1.f + 0.06f * MyPS->GymStrength;
	}
	KOMeter = FMath::Clamp(KOMeter + Amount * GymScale * VsComputerScale(By) / FMath::Max(0.25f, FightToughness / 1.9f), 0.f, 1.f);
	LastKOHitTime = Now;
	if (KOMeter >= 1.f)
	{
		KnockOut(By);
		return;
	}
	// Rocked: birds and wobbly legs for a moment (one more good one and you're out).
	if (KOMeter >= 0.6f && !IsDazed())
	{
		if (const ABRGameState* GS = GetBRGameState())
		{
			DazedUntil = GS->GetServerWorldTimeSeconds() + DazeSeconds * 0.6f;
			UpdateMovementSpeed();
			ForceNetUpdate();
		}
	}
}

void ABRCharacter::KnockOut(ABRCharacter* By)
{
	KOMeter = 0.f;
	ReleaseHandGrab();
	bBlocking = false;
	const FVector Push = By ? (GetActorLocation() - By->GetActorLocation()).GetSafeNormal2D() * 250.f + FVector(0.f, 0.f, 60.f) : FVector(0.f, 0.f, 60.f);
	ApplyKnockdown(By, Push);
	if (GetCondition() != EPVVCondition::KnockedDown)
	{
		return;
	}
	bKnockedOut = true;
	bWasKnockedOut = true;
	ExtendKnockdown(KOExtraSeconds);
	ABRGameState::FX(this, EBRFX::PointsText, GetActorLocation() + FVector(0.f, 0.f, 120.f), TEXT("KNOCKED OUT!"));
	if (By)
	{
		ABRGameState::Highlight(this, By, 70.f, FString::Printf(TEXT("%s KNOCKED OUT %s"), *By->GetPlayerDisplayName(), *GetPlayerDisplayName()));
		if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>(); GM && (!bIsNPC || !By->bIsNPC))
		{
			GM->Feed(FString::Printf(TEXT("%s knocked out %s!"), *By->GetPlayerDisplayName(), *GetPlayerDisplayName()));
		}
	}
	ForceNetUpdate();
}

void ABRCharacter::TickKOMeter(float DeltaSeconds)
{
	if (!HasAuthority())
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (KOMeter > 0.f && Now - LastKOHitTime > KORecoverDelay)
	{
		KOMeter = FMath::Max(0.f, KOMeter - KORecoverRate * DeltaSeconds);
	}
	// Came round after a real knockout: up you get, groggy and wobbly for a moment.
	if (bKnockedOut && GetCondition() != EPVVCondition::KnockedDown)
	{
		bKnockedOut = false;
		if (GetCondition() == EPVVCondition::Normal && bWasKnockedOut)
		{
			if (const ABRGameState* GS = GetBRGameState())
			{
				DazedUntil = GS->GetServerWorldTimeSeconds() + GroggySeconds;
			}
			ApplySlow(GroggySeconds, 0.45f);
			UpdateMovementSpeed();
		}
		bWasKnockedOut = false;
		ForceNetUpdate();
	}
}

// -------------------------------------------------------------------------------------------
// Fluid physical attacks: fists and feet are steered through the move (no canned snap-back)

void ABRCharacter::Multicast_AttackSteer_Implementation(uint8 Limb, FVector_NetQuantize TargetLocal, float Seconds)
{
	if (Limb > 2)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	SteerTargetLocal[Limb] = TargetLocal;
	SteerStart[Limb] = Now;
	SteerUntil[Limb] = Now + Seconds;
}

void ABRCharacter::TickAttackSteer()
{
	USkeletalMeshComponent* Body = GetMesh();
	if (!Body || !bWobbleOn || GetCondition() != EPVVCondition::Normal)
	{
		return;
	}
	static const FName Bones[3] = { FName(TEXT("hand_l")), FName(TEXT("hand_r")), FName(TEXT("foot_r")) };
	const float Now = GetWorld()->GetTimeSeconds();
	for (int32 i = 0; i < 3; ++i)
	{
		if (Now < SteerStart[i] || Now > SteerUntil[i] || Body->GetBoneIndex(Bones[i]) == INDEX_NONE || !Body->IsSimulatingPhysics(PhysBone(Bones[i])))
		{
			continue;
		}
		// Fast at first (the snap of the punch / kick), easing off as it reaches full extension.
		const float T = FMath::Clamp((Now - SteerStart[i]) / FMath::Max(0.05f, SteerUntil[i] - SteerStart[i]), 0.f, 1.f);
		const float Gain = FMath::Lerp(22.f, 9.f, T);
		const FVector Target = GetActorTransform().TransformPositionNoScale(SteerTargetLocal[i]);
		const FVector At = Body->GetBoneLocation(Bones[i]);
		const FVector Want = ((Target - At) * Gain).GetClampedToMaxSize(i == 2 ? 1500.f : 1800.f) + GetVelocity();
		Body->SetPhysicsLinearVelocity(Want, false, PhysBone(Bones[i]));
	}
}

void ABRCharacter::Server_DropKick_Implementation()
{
	DoDropKick();
}

void ABRCharacter::DoDropKick()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (!HasAuthority() || GetCondition() != EPVVCondition::Normal || GetCurrentBoat() || GetCarrier() || GetCarriedCharacter() || bInCannon
		|| bOnShoulders || bClimbing || Now - LastDropKickTime < 1.6f)
	{
		return;
	}
	LastDropKickTime = Now;
	BlowDisguise();
	ReleaseHandGrab();
	const FVector Fwd = GetActorForwardVector().GetSafeNormal2D();
	// Launch feet-first: the body goes flying (a ragdoll) and whoever it slams into goes down.
	bDropKicking = true;
	DropKickHitUntil = Now + 0.9f;
	MarkThrown(this);
	// You stay in control: fly feet-first with floppy limbs, then land back on your feet (no knockdown for the kicker).
	LaunchCharacter(Fwd * DropKickSpeed * 0.8f + FVector(0.f, 0.f, GetCharacterMovement()->IsFalling() ? 160.f : 420.f), true, true);
	BalanceGraceUntil = Now + 2.f; // the landing doesn't topple you
	Multicast_FlightFlop(true);
	if (bWobbleOn && GetMesh())
	{
		for (const TCHAR* Foot : { TEXT("foot_l"), TEXT("foot_r") })
		{
			if (GetMesh()->GetBoneIndex(Foot) != INDEX_NONE)
			{
				GetMesh()->AddImpulse(Fwd * 1400.f + FVector(0.f, 0.f, 500.f), PhysBone(Foot), true);
			}
		}
	}
	Multicast_Sfx(2);
	ForceNetUpdate();
}
