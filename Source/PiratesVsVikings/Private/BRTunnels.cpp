// Real dig-and-walk tunnels under the border. Nothing teleports: ramps, corridor sections, dirt.

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
	const FLinearColor TunnelFillDirt(0.4f, 0.28f, 0.17f);
	const FLinearColor TunnelWood(0.4f, 0.28f, 0.15f);
	constexpr float InnerHalfWidth = 170.f;	// corridor / shaft inside half width
	constexpr float InnerHeight = 300.f;	// corridor inside height
	constexpr float WallThick = 40.f;
	constexpr int32 PartsPerSection = 5;	// floor, ceiling, wall, wall, fill

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
	const float PS = FMath::Sin(FMath::DegreesToRadians(Pitch));
	const float PC = FMath::Cos(FMath::DegreesToRadians(Pitch));
	const float ShaftH = Depth + WallThick;
	const float HoleLen = RampRun + WallThick;

	// --- Start pit (Mexican side): a ramp from the ground down to the corridor floor, walled in.
	AddMesh(TEXT("StartRamp"), CubeMesh, FVector(-RampRun * 0.5f - 20.f * PS, 0.f, -Depth * 0.5f - 20.f * PC), FVector(RampLen(), InnerHalfWidth * 2.f, 40.f), TunnelDirt, FRotator(-Pitch, 0.f, 0.f));
	AddMesh(TEXT("StartWallL"), CubeMesh, FVector(-HoleLen * 0.5f, -InnerHalfWidth - WallThick * 0.5f, -ShaftH * 0.5f), FVector(HoleLen, WallThick, ShaftH), TunnelDarkDirt);
	AddMesh(TEXT("StartWallR"), CubeMesh, FVector(-HoleLen * 0.5f, InnerHalfWidth + WallThick * 0.5f, -ShaftH * 0.5f), FVector(HoleLen, WallThick, ShaftH), TunnelDarkDirt);
	AddMesh(TEXT("StartBack"), CubeMesh, FVector(-RampRun - WallThick * 0.5f, 0.f, -ShaftH * 0.5f), FVector(WallThick, HoleHalfWidth * 2.f, ShaftH), TunnelDarkDirt);
	// The lid: plain ground until somebody breaks ground (and again once the first section is filled in).
	StartLid = AddMesh(TEXT("StartLid"), CubeMesh, FVector(-HoleLen * 0.5f, 0.f, -6.f), FVector(HoleLen, HoleHalfWidth * 2.f, 12.f), TunnelDirt);
	// The spot: a wooden frame + sign beside the pit.
	AddMesh(TEXT("Frame"), CubeMesh, FVector(-HoleLen * 0.5f, HoleHalfWidth + 90.f, 60.f), FVector(160.f, 30.f, 120.f), TunnelWood);
	AddLabel(TEXT("Sign"), FVector(-HoleLen * 0.5f, HoleHalfWidth + 90.f, 200.f), 34.f, FColor(255, 210, 120))->SetText(FText::FromString(TEXT("TUNNEL\n(bring a shovel)")));
	AddMesh(TEXT("Spoil"), SphereMesh, FVector(-HoleLen * 0.5f, -HoleHalfWidth - 200.f, 0.f), FVector(380.f, 260.f, 110.f), TunnelDirt, FRotator::ZeroRotator, false);

	// --- Exit pit (USA side): built at X = 0 here, moved out to the end of the tunnel in OnRep_Tunnel.
	ExitPieces.Add(AddMesh(TEXT("ExitRamp"), CubeMesh, FVector(RampRun * 0.5f + 20.f * PS, 0.f, -Depth * 0.5f - 20.f * PC), FVector(RampLen(), InnerHalfWidth * 2.f, 40.f), TunnelDirt, FRotator(Pitch, 0.f, 0.f)));
	ExitPieces.Add(AddMesh(TEXT("ExitWallL"), CubeMesh, FVector::ZeroVector, FVector(HoleLen, WallThick, ShaftH), TunnelDarkDirt));
	ExitPieces.Add(AddMesh(TEXT("ExitWallR"), CubeMesh, FVector::ZeroVector, FVector(HoleLen, WallThick, ShaftH), TunnelDarkDirt));
	ExitPieces.Add(AddMesh(TEXT("ExitBack"), CubeMesh, FVector::ZeroVector, FVector(WallThick, HoleHalfWidth * 2.f, ShaftH), TunnelDarkDirt));
	ExitLid = AddMesh(TEXT("ExitLid"), CubeMesh, FVector::ZeroVector, FVector(HoleLen, HoleHalfWidth * 2.f, 12.f), FLinearColor(0.62f, 0.48f, 0.3f));
	ExitPieces.Add(ExitLid);
	ExitPile = AddMesh(TEXT("ExitPile"), SphereMesh, FVector::ZeroVector, FVector(380.f, 260.f, 110.f), TunnelDirt, FRotator::ZeroRotator, false);
	ExitPieces.Add(ExitPile);
	ExitSign = AddLabel(TEXT("ExitSign"), FVector::ZeroVector, 30.f, FColor(255, 140, 40));
	ExitSign->SetText(FText::FromString(TEXT("SEISMIC ALERT: TUNNEL COMING\nBorder Patrol: hold E here to dig and meet them")));

	// Fresh dirt on the surface above the Runners' digging (shows once the sensors pick the tunnel up).
	DirtMarker = AddMesh(TEXT("FreshDirt"), SphereMesh, FVector::ZeroVector, FVector(260.f, 260.f, 60.f), TunnelDarkDirt, FRotator::ZeroRotator, false);
	// The dirt faces at the ends of the two diggings.
	MexFace = AddMesh(TEXT("MexFace"), CubeMesh, FVector::ZeroVector, FVector(WallThick, InnerHalfWidth * 2.f + 10.f, InnerHeight + 10.f), TunnelDarkDirt);
	BPFace = AddMesh(TEXT("BPFace"), CubeMesh, FVector::ZeroVector, FVector(WallThick, InnerHalfWidth * 2.f + 10.f, InnerHeight + 10.f), TunnelDarkDirt);
}

void ABRDigTunnel::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRDigTunnel, MexDug);
	DOREPLIFETIME(ABRDigTunnel, BPDug);
	DOREPLIFETIME(ABRDigTunnel, FilledMask);
	DOREPLIFETIME(ABRDigTunnel, bDetected);
	DOREPLIFETIME(ABRDigTunnel, ExitDistance);
}

void ABRDigTunnel::BeginPlay()
{
	ExitDistance = SnapLength(ExitDistance);
	Super::BeginPlay();
	OnRep_Tunnel();
}

void ABRDigTunnel::SetPiece(UStaticMeshComponent* M, bool bOn)
{
	if (M)
	{
		M->SetVisibility(bOn);
		M->SetCollisionEnabled(bOn ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
}

bool ABRDigTunnel::AllFilled() const
{
	for (int32 i = 0; i < NumSections(); ++i)
	{
		if (!IsFilled(i))
		{
			return false;
		}
	}
	return true;
}

int32 ABRDigTunnel::SectionAt(float LocalX) const
{
	if (LocalX < 0.f)
	{
		return -1;
	}
	return FMath::Min(FMath::FloorToInt(LocalX / SectionLength), NumSections());
}

bool ABRDigTunnel::IsInside(const FVector& L) const
{
	return IsStarted() && L.Z < -150.f && FMath::Abs(L.Y) < InnerHalfWidth + 60.f
		&& L.X > -RampRun - WallThick && L.X < NumSections() * SectionLength + RampRun + WallThick;
}

void ABRDigTunnel::OnRep_Tunnel()
{
	const int32 N = NumSections();
	if (SectionMeshes.Num() != N * PartsPerSection)
	{
		SectionMeshes.SetNum(N * PartsPerSection);
		SectionLamps.SetNum(N);
	}
	const float MidZ = -Depth + InnerHeight * 0.5f;
	for (int32 i = 0; i < N; ++i)
	{
		const int32 Base = i * PartsPerSection;
		if (!IsDug(i))
		{
			// Not dug (or the tunnel was reset): remove this section.
			for (int32 k = 0; k < PartsPerSection; ++k)
			{
				if (UStaticMeshComponent* M = SectionMeshes[Base + k])
				{
					M->DestroyComponent();
				}
				SectionMeshes[Base + k] = nullptr;
			}
			if (UPointLightComponent* Lamp = SectionLamps[i])
			{
				Lamp->DestroyComponent();
			}
			SectionLamps[i] = nullptr;
			continue;
		}
		const float X = (i + 0.5f) * SectionLength;
		if (!SectionMeshes[Base])
		{
			auto Piece = [&](int32 Slot, const FVector& At, const FVector& Size, const FLinearColor& Color)
			{
				UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(this);
				M->SetupAttachment(Root);
				M->SetStaticMesh(CubeMesh);
				M->SetRelativeLocation(At);
				M->SetRelativeScale3D(Size / 100.f);
				M->SetCollisionProfileName(TEXT("BlockAll"));
				M->RegisterComponent();
				Paint(M, Color);
				SectionMeshes[Base + Slot] = M;
			};
			const float Len = SectionLength + 4.f;
			Piece(0, FVector(X, 0.f, -Depth - WallThick * 0.5f), FVector(Len, InnerHalfWidth * 2.f + WallThick * 2.f, WallThick), TunnelDirt);			// floor
			Piece(1, FVector(X, 0.f, -Depth + InnerHeight + WallThick * 0.5f), FVector(Len, InnerHalfWidth * 2.f + WallThick * 2.f, WallThick), TunnelDarkDirt); // ceiling
			Piece(2, FVector(X, -InnerHalfWidth - WallThick * 0.5f, MidZ), FVector(Len, WallThick, InnerHeight + WallThick * 2.f), TunnelDarkDirt);	// walls
			Piece(3, FVector(X, InnerHalfWidth + WallThick * 0.5f, MidZ), FVector(Len, WallThick, InnerHeight + WallThick * 2.f), TunnelDarkDirt);
			Piece(4, FVector(X, 0.f, MidZ), FVector(SectionLength + 2.f, InnerHalfWidth * 2.f + 6.f, InnerHeight + 6.f), TunnelFillDirt);			// the dirt Border Patrol fill it with
			if (GetNetMode() != NM_DedicatedServer)
			{
				UPointLightComponent* Lamp = NewObject<UPointLightComponent>(this);
				Lamp->SetupAttachment(Root);
				Lamp->SetRelativeLocation(FVector(X, 0.f, -Depth + InnerHeight - 30.f));
				Lamp->SetIntensity(1800.f);
				Lamp->SetAttenuationRadius(700.f);
				Lamp->SetLightColor(FLinearColor(1.f, 0.75f, 0.4f));
				Lamp->SetCastShadows(false);
				Lamp->RegisterComponent();
				SectionLamps[i] = Lamp;
			}
		}
		SetPiece(SectionMeshes[Base + 4], IsFilled(i));
	}

	// The exit pit sits at the far end of the corridor.
	const float E = N * SectionLength;
	const float HoleLen = RampRun + WallThick;
	const float ShaftH = Depth + WallThick;
	const float Pitch = RampPitch();
	const float PS = FMath::Sin(FMath::DegreesToRadians(Pitch));
	const float PC = FMath::Cos(FMath::DegreesToRadians(Pitch));
	auto Place = [&](const TCHAR* PieceName, const FVector& At)
	{
		for (UStaticMeshComponent* M : ExitPieces)
		{
			if (M && M->GetFName() == FName(PieceName))
			{
				M->SetRelativeLocation(At);
			}
		}
	};
	Place(TEXT("ExitRamp"), FVector(E + RampRun * 0.5f + 20.f * PS, 0.f, -Depth * 0.5f - 20.f * PC));
	Place(TEXT("ExitWallL"), FVector(E + HoleLen * 0.5f, -InnerHalfWidth - WallThick * 0.5f, -ShaftH * 0.5f));
	Place(TEXT("ExitWallR"), FVector(E + HoleLen * 0.5f, InnerHalfWidth + WallThick * 0.5f, -ShaftH * 0.5f));
	Place(TEXT("ExitBack"), FVector(E + RampRun + WallThick * 0.5f, 0.f, -ShaftH * 0.5f));
	Place(TEXT("ExitLid"), FVector(E + HoleLen * 0.5f, 0.f, -6.f));
	Place(TEXT("ExitPile"), FVector(E + HoleLen * 0.5f, -HoleHalfWidth - 200.f, 0.f));
	ExitSign->SetRelativeLocation(FVector(E + HoleLen * 0.5f, HoleHalfWidth + 120.f, 220.f));
	ExitSign->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));

	// Pits open while the section next to them is dug and not filled in.
	SetPiece(StartLid, !IsOpen(0) || MexDug == 0);
	SetPiece(ExitLid, !IsOpen(N - 1));
	ExitPile->SetVisibility(IsDug(N - 1));
	ExitSign->SetVisibility(bDetected && !IsDug(N - 1));

	// Dirt faces at the ends of the two diggings (gone once they meet).
	MexFace->SetRelativeLocation(FVector(MexDug * SectionLength + WallThick * 0.5f, 0.f, MidZ));
	SetPiece(MexFace, MexDug > 0 && !IsConnected());
	BPFace->SetRelativeLocation(FVector((N - BPDug) * SectionLength - WallThick * 0.5f, 0.f, MidZ));
	SetPiece(BPFace, BPDug > 0 && !IsConnected());

	DirtMarker->SetRelativeLocation(FVector(MexDug * SectionLength, 0.f, 0.f));
	DirtMarker->SetVisibility(bDetected && MexDug > 0 && !IsConnected());
}

bool ABRDigTunnel::IsUnderground(const UWorld* World, const FVector& Location)
{
	if (!World)
	{
		return false;
	}
	for (TActorIterator<ABRDigTunnel> It(World); It; ++It)
	{
		const FVector L = It->GetActorTransform().InverseTransformPositionNoScale(Location);
		if (L.Z < -250.f && It->IsInside(L))
		{
			return true;
		}
	}
	return false;
}

ABRDigTunnel::EJob ABRDigTunnel::JobFor(const APVVCharacter* C, int32* OutSection) const
{
	if (!C || C->GetCondition() != EPVVCondition::Normal || C->GetCurrentBoat() || C->GetCarrier())
	{
		return EJob::None;
	}
	const int32 N = NumSections();
	const FVector L = GetActorTransform().InverseTransformPositionNoScale(C->GetActorLocation());
	const bool bInside = IsInside(L);
	const int32 Here = SectionAt(L.X);
	// Which way you're facing along the tunnel (+1 = towards the USA, -1 = towards Mexico, 0 = sideways).
	const FVector Fwd = GetActorTransform().InverseTransformVectorNoScale(C->GetActorForwardVector());
	const int32 Dir = Fwd.X > 0.35f ? 1 : (Fwd.X < -0.35f ? -1 : 0);
	const int32 Next = Here + Dir;
	const float Boundary = Dir > 0 ? Next * SectionLength : (Next + 1) * SectionLength;
	const bool bNearNext = Dir != 0 && Next >= 0 && Next < N && FMath::Abs(L.X - Boundary) < 350.f;

	if (C->GetFaction() == BR::Runners)
	{
		const ABRPlayerState* PS = Cast<ABRPlayerState>(C->GetPVVPlayerState());
		if (!PS || !PS->HasItem(EBRItem::Shovel))
		{
			return EJob::None;
		}
		const ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>();
		const bool bPlaying = GS && GS->Phase == EBRPhase::Invasion;
		if (!IsStarted() && bPlaying && L.Z > -150.f && L.X > -RampRun - 300.f && L.X < 300.f && FMath::Abs(L.Y) < HoleHalfWidth + 300.f)
		{
			return EJob::BreakGround;
		}
		const float FaceX = MexDug * SectionLength;
		if (bInside && MexDug > 0 && !IsConnected() && L.X > FaceX - 400.f && L.X < FaceX + 60.f)
		{
			return EJob::Dig;
		}
		if (bInside && bNearNext && IsDug(Next) && IsFilled(Next))
		{
			if (OutSection)
			{
				*OutSection = Next;
			}
			return EJob::DigOut;
		}
		return EJob::None;
	}

	if (C->GetFaction() == BR::Wardens)
	{
		if (!IsConnected())
		{
			// Dig from the USA side to meet them.
			if (BPDug == 0 && MexDug > 0 && L.Z > -150.f && L.X > N * SectionLength - 300.f && L.X < N * SectionLength + RampRun + 300.f
				&& FMath::Abs(L.Y) < HoleHalfWidth + 300.f)
			{
				return EJob::BPBreakGround;
			}
			const float FaceX = (N - BPDug) * SectionLength;
			if (bInside && BPDug > 0 && L.X < FaceX + 400.f && L.X > FaceX - 60.f)
			{
				return EJob::BPDig;
			}
			return EJob::None;
		}
		// Complete tunnel: fill it with dirt - from inside, or standing at either opening (no need to go down there).
		const bool bAtMexMouth = L.X > -RampRun - 400.f && L.X < 300.f && FMath::Abs(L.Y) < HoleHalfWidth + 350.f;
		const bool bAtUSAMouth = L.X > N * SectionLength - 300.f && L.X < N * SectionLength + RampRun + 400.f && FMath::Abs(L.Y) < HoleHalfWidth + 350.f;
		if ((bInside || bAtMexMouth || bAtUSAMouth) && !AllFilled())
		{
			if (OutSection)
			{
				*OutSection = Here;
			}
			return EJob::Fill;
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
	case EJob::BreakGround:
	case EJob::BPBreakGround: return BreakGroundSeconds;
	case EJob::Dig:
	case EJob::BPDig:
	case EJob::DigOut: return DigSeconds;
	case EJob::Fill: return FillSeconds;
	default: return 0.5f;
	}
}

FText ABRDigTunnel::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	const int32 MetresLeft = FMath::Max(0, NumSections() - MexDug - BPDug) * FMath::RoundToInt(SectionLength / 100.f);
	switch (JobFor(Interactor))
	{
	case EJob::BreakGround: return LOCTEXT("TStart", "Break ground - start a tunnel (hold)");
	case EJob::Dig: return FText::Format(LOCTEXT("TDig", "DIG (hold) - {0} m to go"), MetresLeft);
	case EJob::DigOut: return LOCTEXT("TDigOut", "Dig out the dirt Border Patrol filled in (hold)");
	case EJob::BPBreakGround: return LOCTEXT("TBPStart", "Break ground - dig down to meet the smugglers' tunnel (hold)");
	case EJob::BPDig: return FText::Format(LOCTEXT("TBPDig", "DIG towards the smugglers (hold) - {0} m to go"), MetresLeft);
	case EJob::Fill:
	{
		int32 Open = 0;
		for (int32 i = 0; i < NumSections(); ++i)
		{
			Open += IsOpen(i) ? 1 : 0;
		}
		return FText::Format(LOCTEXT("TFill", "FILL WITH DIRT (hold) - fills from the middle out toward both ends, {0} sections left"), Open);
	}
	default: return FText::GetEmpty();
	}
}

void ABRDigTunnel::OnRunnerDug(APVVCharacter* Digger)
{
	ABRPlayerState* PS = Digger ? Cast<ABRPlayerState>(Digger->GetPVVPlayerState()) : nullptr;
	if (PS && PS->WearShovel())
	{
		if (ABRCharacter* BRC = Cast<ABRCharacter>(Digger))
		{
			BRC->Notify(LOCTEXT("TShovelBroke", "Your shovel BROKE! Buy a new one at the shop."));
		}
		ABRGameState::Sfx(this, EBRSfx::HatPop, Digger->GetActorLocation());
	}
}

void ABRDigTunnel::OnInteract_Implementation(APVVCharacter* Interactor)
{
	if (!HasAuthority())
	{
		return;
	}
	int32 Section = INDEX_NONE;
	const EJob Job = JobFor(Interactor, &Section);
	ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>();
	ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>();
	ABRCharacter* BRC = Cast<ABRCharacter>(Interactor);
	const bool bWasConnected = IsConnected();
	switch (Job)
	{
	case EJob::BreakGround:
		MexDug = 1;
		FilledMask = 0;
		OnRunnerDug(Interactor);
		ABRGameState::FX(this, EBRFX::Dust, GetActorTransform().TransformPositionNoScale(FVector(-RampRun * 0.5f, 0.f, 30.f)));
		if (BRC)
		{
			BRC->Notify(LOCTEXT("TBroke", "Walk down the ramp. Hold E at the dirt at the end to keep digging."));
		}
		break;

	case EJob::Dig:
		++MexDug;
		OnRunnerDug(Interactor);
		// Past the river the seismic sensors hear it: Border Patrol get an alarm and a ping where the digging is.
		if (MexDug * SectionLength >= DetectAfter)
		{
			const FVector Head = GetActorTransform().TransformPositionNoScale(FVector(MexDug * SectionLength, 0.f, 0.f));
			if (!bDetected)
			{
				bDetected = true;
				if (GM)
				{
					GM->TriggerBorderAlarm(Head, TEXT("Seismic sensors picked up DIGGING - dig down at the marked spot to meet them"));
				}
			}
			if (GS)
			{
				GS->Multicast_NoisePing(Head);
			}
		}
		break;

	case EJob::DigOut:
		if (Section != INDEX_NONE)
		{
			FilledMask &= ~(1 << Section);
			OnRunnerDug(Interactor);
		}
		break;

	case EJob::BPBreakGround:
		BPDug = 1;
		ABRGameState::FX(this, EBRFX::Dust, GetActorTransform().TransformPositionNoScale(FVector(NumSections() * SectionLength + RampRun * 0.5f, 0.f, 30.f)));
		if (BRC)
		{
			BRC->Notify(LOCTEXT("TBPBroke", "Walk down the ramp and dig towards Mexico (hold E at the dirt) to meet their tunnel."));
		}
		break;

	case EJob::BPDig:
		++BPDug;
		break;

	case EJob::Fill:
		{
			const FVector L = GetActorTransform().InverseTransformPositionNoScale(Interactor->GetActorLocation());
			FillStep(Interactor, L.X < NumSections() * SectionLength * 0.5f ? -1 : 1);
		}
		break;

	default:
		return;
	}

	if (Job == EJob::Dig || Job == EJob::BPDig || Job == EJob::DigOut || Job == EJob::BreakGround || Job == EJob::BPBreakGround)
	{
		ABRGameState::Sfx(this, EBRSfx::Landing, Interactor->GetActorLocation());
	}
	if (!bWasConnected && IsConnected())
	{
		// The two diggings met (or the Runners got all the way through).
		const FVector Where = GetActorTransform().TransformPositionNoScale(FVector(MexDug * SectionLength, 0.f, 0.f));
		ABRGameState::FX(this, EBRFX::Dust, Where);
		if (GM)
		{
			GM->Feed(BPDug > 0 ? TEXT("The tunnels MET underground! Border Patrol: fill it in.") : TEXT("A tunnel broke through past the Wall! Border Patrol: get in and fill it."));
			GM->TriggerBorderAlarm(Where, TEXT("A TUNNEL is through - go down and FILL IT WITH DIRT"));
		}
		if (GS)
		{
			GS->Multicast_NoisePing(Where);
		}
		if (BRC && BRC->IsRunner() && BPDug == 0)
		{
			BRC->Notify(LOCTEXT("TDone", "You broke through past the Wall! Walk up the ramp."));
		}
	}
	OnRep_Tunnel();
	ForceNetUpdate();
}

void ABRDigTunnel::ShoveOutOf(int32 Section, int32 Dir, APVVCharacter* Except)
{
	// Whoever is standing in the dirt being shovelled in gets pushed out of it and knocked over.
	const int32 N = NumSections();
	const bool bPit = Section < 0 || Section >= N;
	const float X0 = Section < 0 ? -RampRun - WallThick : Section * SectionLength;
	const float X1 = Section < 0 ? 0.f : (Section >= N ? N * SectionLength + RampRun + WallThick : (Section + 1) * SectionLength);
	for (TActorIterator<APVVCharacter> It(GetWorld()); It; ++It)
	{
		APVVCharacter* C = *It;
		if ((!bPit && C == Except) || C->GetCarrier() || C->GetCurrentBoat())
		{
			continue;
		}
		const FVector L = GetActorTransform().InverseTransformPositionNoScale(C->GetActorLocation());
		if (L.Z > (bPit ? 60.f : -60.f) || FMath::Abs(L.Y) > HoleHalfWidth + 30.f || L.X < X0 - 20.f || L.X > X1 + 20.f)
		{
			continue;
		}
		const float Half = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		FVector To((X0 + X1) * 0.5f, HoleHalfWidth + 160.f, Half + 10.f); // default: dug out onto the ground above
		const int32 Beyond = Section + Dir;
		if (!bPit && IsOpen(Beyond))
		{
			// Pushed out the far side, into the next open section.
			To = FVector(Dir > 0 ? Beyond * SectionLength + 120.f : (Beyond + 1) * SectionLength - 120.f, 0.f, -Depth + Half + 10.f);
		}
		C->TeleportTo(GetActorTransform().TransformPositionNoScale(To), C->GetActorRotation(), false, true);
		if (C != Except)
		{
			C->ApplyKnockdown(Except, FVector(0.f, 0.f, 150.f));
			if (ABRCharacter* BRC = Cast<ABRCharacter>(C))
			{
				BRC->Notify(LOCTEXT("TBuried", "Border Patrol are filling the tunnel in on you!"));
			}
		}
	}
}

void ABRDigTunnel::GetPlug(int32& OutLo, int32& OutHi) const
{
	OutLo = -1;
	OutHi = -1;
	for (int32 i = 0; i < NumSections(); ++i)
	{
		if (IsFilled(i))
		{
			OutLo = OutLo < 0 ? i : OutLo;
			OutHi = i;
		}
	}
}

void ABRDigTunnel::FillStep(APVVCharacter* By, int32 FillerSide)
{
	const int32 N = NumSections();
	if (N <= 0)
	{
		return;
	}
	int32 Lo, Hi;
	GetPlug(Lo, Hi);
	if (Lo < 0)
	{
		// First load goes in the middle; anyone standing there is shoved toward the filler's end (an exit).
		FillSection(N / 2, -FillerSide, By);
		return;
	}
	// Grow the plug one section toward EACH end. People in a section being filled are pushed outward, toward the exits.
	const int32 Down = Lo - 1;
	const int32 Up = Hi + 1;
	if (Down >= 0)
	{
		FillSection(Down, 1, By); // shove toward Mexico
		if (FilledMask == 0)
		{
			return; // that finished it (tunnel reset)
		}
	}
	if (Up < N)
	{
		FillSection(Up, -1, By); // shove toward the USA
	}
}

void ABRDigTunnel::FillSection(int32 Section, int32 FromSide, APVVCharacter* By)
{
	// FromSide: which side the filler is on (-1 = Mexican side). People inside get pushed the other way.
	const int32 N = NumSections();
	ShoveOutOf(Section, -FromSide, By);
	FilledMask |= (1 << Section);
	ABRGameState::FX(this, EBRFX::Dust, GetActorTransform().TransformPositionNoScale(FVector((Section + 0.5f) * SectionLength, 0.f, -Depth + 100.f)));
	if (By)
	{
		ABRGameState::Sfx(this, EBRSfx::Landing, By->GetActorLocation());
	}
	// An end section filled in seals its pit: anyone down there climbs out.
	if (Section == 0)
	{
		ShoveOutOf(-1, -1, By);
	}
	if (Section == N - 1)
	{
		ShoveOutOf(N, 1, By);
	}
	if (AllFilled())
	{
		ResetTunnel();
		if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
		{
			GM->Feed(FString::Printf(TEXT("%s filled in a smuggling tunnel!"), By ? *By->GetPlayerDisplayName() : TEXT("Border Patrol")));
			GM->OnContrabandSeized(By, 2); // big bust
		}
	}
}

void ABRDigTunnel::ResetTunnel()
{
	MexDug = 0;
	BPDug = 0;
	FilledMask = 0;
	bDetected = false;
}

#undef LOCTEXT_NAMESPACE
