// Real dig-and-walk tunnels under the border. Nothing teleports: ramps, corridor, dirt.

#include "BRTunnels.h"
#include "BRCharacter.h"
#include "BRGameMode.h"
#include "BRGameState.h"
#include "BRPlayerState.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "BRTunnels"

namespace
{
	const FLinearColor TunnelDirt(0.32f, 0.22f, 0.14f);
	const FLinearColor TunnelDarkDirt(0.22f, 0.15f, 0.1f);
	const FLinearColor TunnelWood(0.4f, 0.28f, 0.15f);
	constexpr float InnerHalfWidth = 170.f;	// corridor / shaft inside half width
	constexpr float InnerHeight = 300.f;	// corridor inside height
	constexpr float WallThick = 40.f;

	float RampPitch() { return FMath::RadiansToDegrees(FMath::Atan2(ABRDigTunnel::Depth, ABRDigTunnel::RampRun)); }
	float RampLen() { return FMath::Sqrt(FMath::Square(ABRDigTunnel::Depth) + FMath::Square(ABRDigTunnel::RampRun)) + 30.f; }
}

void ABRDigTunnel::GetHoleRects(float InExitDistance, FBox2D& OutStart, FBox2D& OutExit)
{
	const float E = SnapLength(InExitDistance);
	OutStart = FBox2D(FVector2D(-RampRun - WallThick, -HoleHalfWidth), FVector2D(0.f, HoleHalfWidth));
	OutExit = FBox2D(FVector2D(E, -HoleHalfWidth), FVector2D(E + RampRun + WallThick, HoleHalfWidth));
}

ABRDigTunnel::ABRDigTunnel()
{
	const float Pitch = RampPitch();
	const float S = FMath::Sin(FMath::DegreesToRadians(Pitch));
	const float C = FMath::Cos(FMath::DegreesToRadians(Pitch));
	const float ShaftH = Depth + WallThick;
	const float HoleLen = RampRun + WallThick;

	// --- Start pit (Mexican side): a ramp from the ground down to the corridor floor, walled in.
	AddMesh(TEXT("StartRamp"), CubeMesh, FVector(-RampRun * 0.5f - 20.f * S, 0.f, -Depth * 0.5f - 20.f * C), FVector(RampLen(), InnerHalfWidth * 2.f, 40.f), TunnelDirt, FRotator(-Pitch, 0.f, 0.f));
	AddMesh(TEXT("StartWallL"), CubeMesh, FVector(-HoleLen * 0.5f, -InnerHalfWidth - WallThick * 0.5f, -ShaftH * 0.5f), FVector(HoleLen, WallThick, ShaftH), TunnelDarkDirt);
	AddMesh(TEXT("StartWallR"), CubeMesh, FVector(-HoleLen * 0.5f, InnerHalfWidth + WallThick * 0.5f, -ShaftH * 0.5f), FVector(HoleLen, WallThick, ShaftH), TunnelDarkDirt);
	AddMesh(TEXT("StartBack"), CubeMesh, FVector(-RampRun - WallThick * 0.5f, 0.f, -ShaftH * 0.5f), FVector(WallThick, HoleHalfWidth * 2.f, ShaftH), TunnelDarkDirt);
	// The lid: plain ground until somebody breaks ground (and again once Border Patrol fill the pit back in).
	StartLid = AddMesh(TEXT("StartLid"), CubeMesh, FVector(-HoleLen * 0.5f, 0.f, -6.f), FVector(HoleLen, HoleHalfWidth * 2.f, 12.f), TunnelDirt);
	// The spot: a wooden frame + sign beside the pit.
	AddMesh(TEXT("Frame"), CubeMesh, FVector(-HoleLen * 0.5f, HoleHalfWidth + 90.f, 60.f), FVector(160.f, 30.f, 120.f), TunnelWood);
	AddLabel(TEXT("Sign"), FVector(-HoleLen * 0.5f, HoleHalfWidth + 90.f, 200.f), 34.f, FColor(255, 210, 120))->SetText(FText::FromString(TEXT("TUNNEL\n(bring a shovel)")));
	AddMesh(TEXT("Spoil"), SphereMesh, FVector(-HoleLen * 0.5f, -HoleHalfWidth - 200.f, 0.f), FVector(380.f, 260.f, 110.f), TunnelDirt, FRotator::ZeroRotator, false);

	// --- Exit pit (USA side): built at X = 0 here, moved out to the end of the tunnel in OnRep_Sections.
	ExitPieces.Add(AddMesh(TEXT("ExitRamp"), CubeMesh, FVector(RampRun * 0.5f + 20.f * S, 0.f, -Depth * 0.5f - 20.f * C), FVector(RampLen(), InnerHalfWidth * 2.f, 40.f), TunnelDirt, FRotator(Pitch, 0.f, 0.f)));
	ExitPieces.Add(AddMesh(TEXT("ExitWallL"), CubeMesh, FVector(HoleLen * 0.5f, -InnerHalfWidth - WallThick * 0.5f, -ShaftH * 0.5f), FVector(HoleLen, WallThick, ShaftH), TunnelDarkDirt));
	ExitPieces.Add(AddMesh(TEXT("ExitWallR"), CubeMesh, FVector(HoleLen * 0.5f, InnerHalfWidth + WallThick * 0.5f, -ShaftH * 0.5f), FVector(HoleLen, WallThick, ShaftH), TunnelDarkDirt));
	ExitPieces.Add(AddMesh(TEXT("ExitBack"), CubeMesh, FVector(RampRun + WallThick * 0.5f, 0.f, -ShaftH * 0.5f), FVector(WallThick, HoleHalfWidth * 2.f, ShaftH), TunnelDarkDirt));
	ExitLid = AddMesh(TEXT("ExitLid"), CubeMesh, FVector(HoleLen * 0.5f, 0.f, -6.f), FVector(HoleLen, HoleHalfWidth * 2.f, 12.f), FLinearColor(0.62f, 0.48f, 0.3f));
	ExitPieces.Add(ExitLid);
	ExitPile = AddMesh(TEXT("ExitPile"), SphereMesh, FVector(HoleLen * 0.5f, -HoleHalfWidth - 200.f, 0.f), FVector(380.f, 260.f, 110.f), TunnelDirt, FRotator::ZeroRotator, false);
	ExitPieces.Add(ExitPile);

	// Fresh dirt on the surface above the digging (Border Patrol see it once the sensors pick the tunnel up).
	DirtMarker = AddMesh(TEXT("FreshDirt"), SphereMesh, FVector::ZeroVector, FVector(260.f, 260.f, 60.f), TunnelDarkDirt, FRotator::ZeroRotator, false);
	// The dirt face at the end of the corridor (what the Runners dig).
	Face = AddMesh(TEXT("Face"), CubeMesh, FVector::ZeroVector, FVector(WallThick, InnerHalfWidth * 2.f + 10.f, InnerHeight + 10.f), TunnelDarkDirt);
	// Border Patrol's backfill, growing from the Mexican end.
	Backfill = AddMesh(TEXT("Backfill"), CubeMesh, FVector::ZeroVector, FVector(100.f, InnerHalfWidth * 2.f + 6.f, InnerHeight + 6.f), TunnelDirt);
}

void ABRDigTunnel::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRDigTunnel, Sections);
	DOREPLIFETIME(ABRDigTunnel, Filled);
	DOREPLIFETIME(ABRDigTunnel, bDetected);
	DOREPLIFETIME(ABRDigTunnel, ExitDistance);
}

void ABRDigTunnel::BeginPlay()
{
	ExitDistance = SnapLength(ExitDistance);
	Super::BeginPlay();
	OnRep_Sections();
}

float ABRDigTunnel::UnitStartX(int32 U) const
{
	if (U <= 0)
	{
		return -RampRun - WallThick;
	}
	return (FMath::Min(U, Sections + 1) - 1) * SectionLength;
}

float ABRDigTunnel::UnitEndX(int32 U) const
{
	if (U <= 0)
	{
		return 0.f;
	}
	if (U > Sections)
	{
		return Length() + RampRun + WallThick;
	}
	return U * SectionLength;
}

void ABRDigTunnel::SetPiece(UStaticMeshComponent* M, bool bOn)
{
	if (M)
	{
		M->SetVisibility(bOn);
		M->SetCollisionEnabled(bOn ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
}

void ABRDigTunnel::OnRep_Sections()
{
	// Corridor: floor, ceiling and two walls for every dug section, plus a lamp. Removed again when it gets reset.
	while (Built.Num() > Sections * 4)
	{
		if (UStaticMeshComponent* M = Built.Pop())
		{
			M->DestroyComponent();
		}
	}
	TArray<UPointLightComponent*> Lamps;
	GetComponents<UPointLightComponent>(Lamps);
	for (UPointLightComponent* Lamp : Lamps)
	{
		if (Lamp->ComponentHasTag(TEXT("TunnelLamp")) && Lamp->GetRelativeLocation().X > Length())
		{
			Lamp->DestroyComponent();
		}
	}
	for (int32 i = Built.Num() / 4; i < Sections; ++i)
	{
		const float X = (i + 0.5f) * SectionLength;
		auto Piece = [&](const FVector& At, const FVector& Size, const FLinearColor& Color)
		{
			UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(this);
			M->SetupAttachment(Root);
			M->SetStaticMesh(CubeMesh);
			M->SetRelativeLocation(At);
			M->SetRelativeScale3D(Size / 100.f);
			M->SetCollisionProfileName(TEXT("BlockAll"));
			M->RegisterComponent();
			Paint(M, Color);
			Built.Add(M);
		};
		const float Len = SectionLength + 4.f;
		const float MidZ = -Depth + InnerHeight * 0.5f;
		Piece(FVector(X, 0.f, -Depth - WallThick * 0.5f), FVector(Len, InnerHalfWidth * 2.f + WallThick * 2.f, WallThick), TunnelDirt);			// floor
		Piece(FVector(X, 0.f, -Depth + InnerHeight + WallThick * 0.5f), FVector(Len, InnerHalfWidth * 2.f + WallThick * 2.f, WallThick), TunnelDarkDirt); // ceiling
		Piece(FVector(X, -InnerHalfWidth - WallThick * 0.5f, MidZ), FVector(Len, WallThick, InnerHeight + WallThick * 2.f), TunnelDarkDirt);		// walls
		Piece(FVector(X, InnerHalfWidth + WallThick * 0.5f, MidZ), FVector(Len, WallThick, InnerHeight + WallThick * 2.f), TunnelDarkDirt);
		if (GetNetMode() != NM_DedicatedServer)
		{
			UPointLightComponent* Lamp = NewObject<UPointLightComponent>(this);
			Lamp->SetupAttachment(Root);
			Lamp->SetRelativeLocation(FVector(X, 0.f, -Depth + InnerHeight - 30.f));
			Lamp->SetIntensity(1800.f);
			Lamp->SetAttenuationRadius(700.f);
			Lamp->SetLightColor(FLinearColor(1.f, 0.75f, 0.4f));
			Lamp->SetCastShadows(false);
			Lamp->ComponentTags.Add(TEXT("TunnelLamp"));
			Lamp->RegisterComponent();
		}
	}

	// The exit pit sits at the far end of the corridor.
	const float E = ExitDistance;
	const float HoleLen = RampRun + WallThick;
	{
		const float Pitch = RampPitch();
		const float S = FMath::Sin(FMath::DegreesToRadians(Pitch));
		const float C = FMath::Cos(FMath::DegreesToRadians(Pitch));
		const float ShaftH = Depth + WallThick;
		auto Place = [&](const TCHAR* Name, const FVector& At)
		{
			for (UStaticMeshComponent* M : ExitPieces)
			{
				if (M && M->GetFName() == FName(Name))
				{
					M->SetRelativeLocation(At);
				}
			}
		};
		Place(TEXT("ExitRamp"), FVector(E + RampRun * 0.5f + 20.f * S, 0.f, -Depth * 0.5f - 20.f * C));
		Place(TEXT("ExitWallL"), FVector(E + HoleLen * 0.5f, -InnerHalfWidth - WallThick * 0.5f, -ShaftH * 0.5f));
		Place(TEXT("ExitWallR"), FVector(E + HoleLen * 0.5f, InnerHalfWidth + WallThick * 0.5f, -ShaftH * 0.5f));
		Place(TEXT("ExitBack"), FVector(E + RampRun + WallThick * 0.5f, 0.f, -ShaftH * 0.5f));
		Place(TEXT("ExitLid"), FVector(E + HoleLen * 0.5f, 0.f, -6.f));
		Place(TEXT("ExitPile"), FVector(E + HoleLen * 0.5f, -HoleHalfWidth - 200.f, 0.f));
	}

	// Lids: closed until dug open; the start closes again as soon as Border Patrol fill the pit.
	SetPiece(StartLid, Sections == 0 || Filled >= 1);
	SetPiece(ExitLid, !IsComplete());
	ExitPile->SetVisibility(IsComplete());

	// The dirt face at the end while digging.
	Face->SetRelativeLocation(FVector(Length() + WallThick * 0.5f, 0.f, -Depth + InnerHeight * 0.5f));
	SetPiece(Face, Sections > 0 && !IsComplete());

	// Backfill: from the back of the start pit up to the end of the last filled unit.
	if (Filled >= 1)
	{
		const float From = -RampRun;
		const float To = UnitEndX(Filled - 1);
		Backfill->SetRelativeLocation(FVector((From + To) * 0.5f, 0.f, -Depth + InnerHeight * 0.5f));
		Backfill->SetRelativeScale3D(FVector(FMath::Max(To - From, 10.f), InnerHalfWidth * 2.f + 6.f, InnerHeight + 6.f) / 100.f);
	}
	SetPiece(Backfill, Filled >= 1);

	DirtMarker->SetRelativeLocation(FVector(Length(), 0.f, 0.f));
	DirtMarker->SetVisibility(bDetected && Sections > 0 && !IsComplete());
}

bool ABRDigTunnel::IsUnderground(const UWorld* World, const FVector& Location)
{
	if (!World)
	{
		return false;
	}
	for (TActorIterator<ABRDigTunnel> It(World); It; ++It)
	{
		if (It->Sections <= 0)
		{
			continue;
		}
		const FVector L = It->GetActorTransform().InverseTransformPositionNoScale(Location);
		if (L.Z < -250.f && FMath::Abs(L.Y) < HoleHalfWidth + 50.f && L.X > It->UnitStartX(0) && L.X < It->UnitEndX(It->Sections + 1))
		{
			return true;
		}
	}
	return false;
}

ABRDigTunnel::EJob ABRDigTunnel::JobFor(const APVVCharacter* C) const
{
	if (!C || C->GetCondition() != EPVVCondition::Normal || C->GetCurrentBoat() || C->GetCarrier())
	{
		return EJob::None;
	}
	const ABRPlayerState* PS = Cast<ABRPlayerState>(C->GetPVVPlayerState());
	const ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>();
	const FVector L = GetActorTransform().InverseTransformPositionNoScale(C->GetActorLocation());
	const bool bInside = Sections > 0 && L.Z < -150.f && FMath::Abs(L.Y) < InnerHalfWidth + 60.f
		&& L.X > UnitStartX(0) && L.X < UnitEndX(Sections + 1);

	if (C->GetFaction() == BR::Runners)
	{
		const bool bShovel = PS && PS->HasItem(EBRItem::Shovel);
		if (!bShovel)
		{
			return EJob::None;
		}
		const bool bPlaying = GS && GS->Phase == EBRPhase::Invasion;
		if (Sections == 0 && Filled == 0 && bPlaying && L.Z > -150.f && L.X > -RampRun - 300.f && L.X < 300.f && FMath::Abs(L.Y) < HoleHalfWidth + 300.f)
		{
			return EJob::BreakGround;
		}
		if (bInside && !IsComplete() && Filled == 0 && L.X > Length() - 400.f)
		{
			return EJob::Dig;
		}
		return EJob::None;
	}

	// Border Patrol: only a finished tunnel can be filled in, and only from inside it.
	if (C->GetFaction() == BR::Wardens && IsComplete())
	{
		if (Filled <= Sections)
		{
			// Stand just past the next unit to fill (on the USA side of it) and shovel the dirt back behind you.
			const float B = UnitEndX(Filled);
			if (bInside && L.X > B - 60.f && L.X < B + 450.f)
			{
				return EJob::Fill;
			}
		}
		else if (L.X > Length() - 100.f && L.X < Length() + RampRun + 300.f && FMath::Abs(L.Y) < HoleHalfWidth + 300.f && L.Z > -450.f)
		{
			return EJob::Fill; // last one: seal the exit pit from the ramp / the edge of the hole
		}
	}
	return EJob::None;
}

bool ABRDigTunnel::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	return JobFor(Interactor) != EJob::None;
}

float ABRDigTunnel::GetInteractDuration_Implementation(APVVCharacter* Interactor) const
{
	switch (JobFor(Interactor))
	{
	case EJob::BreakGround: return BreakGroundSeconds;
	case EJob::Dig: return DigSeconds;
	case EJob::Fill: return FillSeconds;
	default: return 0.5f;
	}
}

FText ABRDigTunnel::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	switch (JobFor(Interactor))
	{
	case EJob::BreakGround: return LOCTEXT("TStart", "Break ground - start a tunnel (hold)");
	case EJob::Dig: return FText::Format(LOCTEXT("TDig", "DIG (hold) - {0} m to the far side"), FMath::Max(0, FMath::RoundToInt((ExitDistance - Length()) / 100.f)));
	case EJob::Fill:
		if (Filled == 0)
		{
			return LOCTEXT("TFill0", "FILL IN the tunnel entrance behind you (hold)");
		}
		if (Filled > Sections)
		{
			return LOCTEXT("TFillLast", "SEAL the tunnel exit (hold)");
		}
		return FText::Format(LOCTEXT("TFill", "Fill in the section behind you (hold) - {0} left"), Sections + 2 - Filled);
	default: return FText::GetEmpty();
	}
}

void ABRDigTunnel::OnInteract_Implementation(APVVCharacter* Interactor)
{
	if (!HasAuthority())
	{
		return;
	}
	const EJob Job = JobFor(Interactor);
	ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>();
	ABRCharacter* BRC = Cast<ABRCharacter>(Interactor);
	switch (Job)
	{
	case EJob::BreakGround:
		Sections = 1;
		OnRep_Sections();
		ForceNetUpdate();
		ABRGameState::Sfx(this, EBRSfx::Landing, Interactor->GetActorLocation());
		ABRGameState::FX(this, EBRFX::Dust, GetActorTransform().TransformPositionNoScale(FVector(-RampRun * 0.5f, 0.f, 30.f)));
		if (BRC)
		{
			BRC->Notify(LOCTEXT("TBroke", "Walk down the ramp. Hold E at the dirt at the end to keep digging."));
		}
		break;

	case EJob::Dig:
	{
		++Sections;
		OnRep_Sections();
		ForceNetUpdate();
		ABRGameState::Sfx(this, EBRSfx::Landing, Interactor->GetActorLocation());
		// Past the river the seismic sensors hear it: Border Patrol get an alarm and a ping where the digging is.
		if (Length() >= DetectAfter)
		{
			const FVector Head = GetActorTransform().TransformPositionNoScale(FVector(Length(), 0.f, 0.f));
			if (!bDetected)
			{
				bDetected = true;
				OnRep_Sections();
				if (GM)
				{
					GM->TriggerBorderAlarm(Head, TEXT("Seismic sensors picked up DIGGING"));
				}
			}
			if (ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>())
			{
				GS->Multicast_NoisePing(Head);
			}
		}
		if (IsComplete())
		{
			const FVector Exit = GetActorTransform().TransformPositionNoScale(FVector(Length() + RampRun * 0.5f, 0.f, 0.f));
			ABRGameState::FX(this, EBRFX::Dust, Exit);
			if (BRC)
			{
				BRC->Notify(LOCTEXT("TDone", "You broke through past the Wall! Walk up the ramp."));
			}
			if (GM)
			{
				GM->Feed(TEXT("A tunnel broke through past the Wall! Border Patrol: get in and fill it."));
				GM->TriggerBorderAlarm(Exit, TEXT("A TUNNEL broke through - go down and fill it in"));
			}
			if (ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>())
			{
				GS->Multicast_NoisePing(Exit);
			}
		}
		break;
	}

	case EJob::Fill:
		FillNext(Interactor);
		break;

	default:
		break;
	}
}

void ABRDigTunnel::ShoveOutOf(int32 Unit, APVVCharacter* Except)
{
	// Whoever is standing in the dirt being shovelled in gets pushed out of it and knocked over.
	const float X0 = UnitStartX(Unit);
	const float X1 = UnitEndX(Unit);
	for (TActorIterator<APVVCharacter> It(GetWorld()); It; ++It)
	{
		APVVCharacter* C = *It;
		if (C == Except || C->GetCarrier())
		{
			continue;
		}
		const FVector L = GetActorTransform().InverseTransformPositionNoScale(C->GetActorLocation());
		if (L.Z > -60.f || FMath::Abs(L.Y) > HoleHalfWidth + 30.f || L.X < X0 - 30.f || L.X > X1 + 30.f)
		{
			continue;
		}
		const float Half = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		FVector To;
		if (Unit == 0 || Unit > Sections)
		{
			// A pit: out onto the ground beside it.
			To = FVector((X0 + X1) * 0.5f, HoleHalfWidth + 160.f, Half + 10.f);
		}
		else
		{
			// A corridor section: out the USA side of it (onto the exit ramp if that's what's next).
			const float X = X1 + 150.f;
			const float FloorZ = X > Length() ? -Depth + (X - Length()) * Depth / RampRun : -Depth;
			To = FVector(X, 0.f, FloorZ + Half + 10.f);
		}
		C->TeleportTo(GetActorTransform().TransformPositionNoScale(To), C->GetActorRotation(), false, true);
		C->ApplyKnockdown(Except, FVector(0.f, 0.f, 150.f));
		if (ABRCharacter* BRC = Cast<ABRCharacter>(C))
		{
			BRC->Notify(LOCTEXT("TBuried", "Border Patrol are filling the tunnel in on you!"));
		}
	}
}

void ABRDigTunnel::FillNext(APVVCharacter* By)
{
	const int32 Unit = Filled;
	ShoveOutOf(Unit, By);
	ABRGameState::FX(this, EBRFX::Dust, GetActorTransform().TransformPositionNoScale(FVector((UnitStartX(Unit) + UnitEndX(Unit)) * 0.5f, 0.f, Unit == 0 || Unit > Sections ? 30.f : -Depth + 100.f)));
	if (By)
	{
		ABRGameState::Sfx(this, EBRSfx::Landing, By->GetActorLocation());
	}
	if (Unit > Sections)
	{
		// The exit pit is filled: the tunnel is gone.
		Sections = 0;
		Filled = 0;
		bDetected = false;
		if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
		{
			GM->Feed(FString::Printf(TEXT("%s filled in a smuggling tunnel!"), By ? *By->GetPlayerDisplayName() : TEXT("Border Patrol")));
			GM->OnContrabandSeized(By, 2); // big bust
		}
	}
	else
	{
		++Filled;
		if (Unit == 0)
		{
			if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
			{
				GM->Feed(TEXT("Border Patrol sealed a tunnel entrance - they're filling it in!"));
			}
		}
	}
	OnRep_Sections();
	ForceNetUpdate();
}

#undef LOCTEXT_NAMESPACE
