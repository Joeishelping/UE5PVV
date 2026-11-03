// Real dig-and-walk tunnels under the border.

#include "BRTunnels.h"
#include "BRCharacter.h"
#include "BRGameMode.h"
#include "BRGameState.h"
#include "BRPlayerState.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "BRTunnels"

namespace
{
	const FLinearColor Dirt(0.32f, 0.22f, 0.14f);
	const FLinearColor DarkDirt(0.22f, 0.15f, 0.1f);
	const FLinearColor Beam(0.45f, 0.3f, 0.16f);
}

ABRDigTunnel::ABRDigTunnel()
{
	const FLinearColor Wood(0.4f, 0.28f, 0.15f);
	AddMesh(TEXT("Mound"), SphereMesh, FVector(0.f, 0.f, 0.f), FVector(320.f, 320.f, 90.f), Dirt, FRotator::ZeroRotator, false);
	AddMesh(TEXT("Frame"), CubeMesh, FVector(0.f, 0.f, 60.f), FVector(150.f, 150.f, 20.f), Wood, FRotator::ZeroRotator, false);
	AddLabel(TEXT("Sign"), FVector(0.f, 0.f, 220.f), 34.f, FColor(255, 210, 120))->SetText(FText::FromString(TEXT("TUNNEL\n(bring a shovel)")));
	ExitMound = AddMesh(TEXT("ExitMound"), SphereMesh, FVector(ExitDistance, 0.f, 0.f), FVector(320.f, 320.f, 90.f), Dirt, FRotator::ZeroRotator, false);
	DirtMarker = AddMesh(TEXT("FreshDirt"), SphereMesh, FVector(0.f, 0.f, 0.f), FVector(260.f, 260.f, 60.f), DarkDirt, FRotator::ZeroRotator, false);
	// The dirt face at the end of the corridor (what you dig).
	Face = AddMesh(TEXT("Face"), CubeMesh, FVector(0.f, 0.f, 0.f), FVector(60.f, 340.f, 340.f), DarkDirt);
	AddPickupZone(260.f);
}

void ABRDigTunnel::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRDigTunnel, Sections);
	DOREPLIFETIME(ABRDigTunnel, bDetected);
}

void ABRDigTunnel::BeginPlay()
{
	Super::BeginPlay();
	ExitMound->SetRelativeLocation(FVector(ExitDistance, 0.f, 0.f));
	OnRep_Sections();
}

FVector ABRDigTunnel::CorridorPoint(float Along) const
{
	return FVector(Along, 0.f, -Depth);
}

void ABRDigTunnel::OnRep_Sections()
{
	// (Re)build the corridor: floor, walls, ceiling and a lamp for every dug section.
	while (Built.Num() > Sections * 4)
	{
		if (UStaticMeshComponent* M = Built.Pop())
		{
			M->DestroyComponent();
		}
	}
	TArray<UPointLightComponent*> Lamps;
	GetComponents<UPointLightComponent>(Lamps);
	for (UPointLightComponent* L : Lamps)
	{
		if (L->ComponentHasTag(TEXT("TunnelLamp")) && L->GetRelativeLocation().X > Length() + 10.f)
		{
			L->DestroyComponent();
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
			if (UMaterialInstanceDynamic* MID = M->CreateDynamicMaterialInstance(0))
			{
				MID->SetVectorParameterValue(TEXT("Color"), Color);
			}
			Built.Add(M);
		};
		Piece(FVector(X, 0.f, -Depth - 20.f), FVector(SectionLength + 4.f, 340.f, 40.f), Dirt);			// floor
		Piece(FVector(X, 0.f, -Depth + 320.f), FVector(SectionLength + 4.f, 340.f, 40.f), DarkDirt);		// ceiling
		Piece(FVector(X, -190.f, -Depth + 150.f), FVector(SectionLength + 4.f, 40.f, 340.f), DarkDirt);	// walls
		Piece(FVector(X, 190.f, -Depth + 150.f), FVector(SectionLength + 4.f, 40.f, 340.f), DarkDirt);
		if (GetNetMode() != NM_DedicatedServer)
		{
			UPointLightComponent* Lamp = NewObject<UPointLightComponent>(this);
			Lamp->SetupAttachment(Root);
			Lamp->SetRelativeLocation(FVector(X, 0.f, -Depth + 270.f));
			Lamp->SetIntensity(1800.f);
			Lamp->SetAttenuationRadius(700.f);
			Lamp->SetLightColor(FLinearColor(1.f, 0.75f, 0.4f));
			Lamp->SetCastShadows(false);
			Lamp->ComponentTags.Add(TEXT("TunnelLamp"));
			Lamp->RegisterComponent();
		}
	}
	// The dirt face sits at the end (gone once it breaks through); the starting shaft room is section 0's back wall.
	Face->SetRelativeLocation(FVector(Length() + 30.f, 0.f, -Depth + 150.f));
	Face->SetVisibility(Sections > 0 && !IsComplete());
	Face->SetCollisionEnabled(Sections > 0 && !IsComplete() ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	ExitMound->SetVisibility(IsComplete());
	DirtMarker->SetRelativeLocation(FVector(Length(), 0.f, 0.f));
	DirtMarker->SetVisibility(bDetected && Sections > 0);
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
		if (L.Z < -It->Depth * 0.5f && L.X > -300.f && L.X < It->Length() + 300.f && FMath::Abs(L.Y) < 260.f)
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
	const bool bUnder = L.Z < -Depth * 0.5f;
	const bool bRunner = C->GetFaction() == BR::Runners;
	const bool bWarden = C->GetFaction() == BR::Wardens;
	const bool bShovel = PS && PS->HasItem(EBRItem::Shovel);
	const bool bPlaying = GS && GS->Phase == EBRPhase::Invasion;
	if (!bUnder)
	{
		const float AtStart = FVector2D(L.X, L.Y).Size();
		const float AtEnd = FVector2D(L.X - ExitDistance, L.Y).Size();
		const float AtHead = FVector2D(L.X - Length(), L.Y).Size();
		if (bWarden && Sections > 0 && ((bDetected && AtHead < 300.f) || (IsComplete() && AtEnd < 300.f)))
		{
			return EJob::Collapse;
		}
		if (bRunner && bPlaying && AtStart < 300.f)
		{
			return Sections == 0 ? (bShovel ? EJob::Start : EJob::None) : EJob::Enter;
		}
		if (bRunner && IsComplete() && AtEnd < 300.f)
		{
			return EJob::EnterEnd;
		}
		return EJob::None;
	}
	if (L.X < 250.f)
	{
		return EJob::ClimbOutStart;
	}
	if (IsComplete() && L.X > ExitDistance - 300.f)
	{
		return EJob::ClimbOutEnd;
	}
	if (!IsComplete() && bShovel && L.X > Length() - 300.f)
	{
		return EJob::Dig;
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
	case EJob::Start: return DigSeconds;
	case EJob::Dig: return DigSeconds;
	case EJob::Collapse: return 6.f;
	default: return 0.5f;
	}
}

FText ABRDigTunnel::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	switch (JobFor(Interactor))
	{
	case EJob::Start: return LOCTEXT("TStart", "Start digging a tunnel (hold)");
	case EJob::Enter: return FText::Format(LOCTEXT("TEnter", "Climb down into the tunnel ({0}% dug)"), FMath::Min(100, FMath::RoundToInt(Length() / ExitDistance * 100.f)));
	case EJob::ClimbOutStart: return LOCTEXT("TOutStart", "Climb back out (Mexico side)");
	case EJob::Dig: return FText::Format(LOCTEXT("TDig", "DIG (hold) - {0} m to go"), FMath::Max(0, FMath::RoundToInt((ExitDistance - Length()) / 100.f)));
	case EJob::ClimbOutEnd: return LOCTEXT("TOutEnd", "Climb out - USA side!");
	case EJob::EnterEnd: return LOCTEXT("TEnterEnd", "Climb down into the tunnel");
	case EJob::Collapse: return LOCTEXT("TCollapse", "COLLAPSE the tunnel (hold)");
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
	const float Half = Interactor->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	auto MoveTo = [&](const FVector& Local)
	{
		Interactor->TeleportTo(GetActorTransform().TransformPositionNoScale(Local) + FVector(0.f, 0.f, Half + 5.f), FRotator(0.f, GetActorRotation().Yaw, 0.f), false, true);
	};
	ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>();
	switch (Job)
	{
	case EJob::Start:
	case EJob::Dig:
	{
		++Sections;
		OnRep_Sections();
		ForceNetUpdate();
		ABRGameState::Sfx(this, EBRSfx::Landing, Interactor->GetActorLocation());
		if (Job == EJob::Start)
		{
			MoveTo(CorridorPoint(100.f));
		}
		// Past the river: the seismic sensors hear it and Border Patrol get a ping where the digging is.
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
			if (ABRCharacter* BRC = Cast<ABRCharacter>(Interactor))
			{
				BRC->Notify(LOCTEXT("TDone", "You broke through - the tunnel comes out past the Wall! Climb out (E)."));
			}
			if (GM)
			{
				GM->Feed(TEXT("A tunnel broke through past the Wall!"));
			}
		}
		break;
	}
	case EJob::Enter: MoveTo(CorridorPoint(100.f)); break;
	case EJob::ClimbOutStart: MoveTo(FVector(0.f, 220.f, 0.f)); break;
	case EJob::ClimbOutEnd: MoveTo(FVector(ExitDistance, 220.f, 0.f)); break;
	case EJob::EnterEnd: MoveTo(CorridorPoint(ExitDistance - 100.f)); break;
	case EJob::Collapse: Collapse(Interactor); break;
	default: break;
	}
}

void ABRDigTunnel::Collapse(APVVCharacter* By)
{
	// Everyone inside is buried: dragged back out at the start, knocked out cold.
	for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
	{
		ABRCharacter* C = *It;
		const FVector L = GetActorTransform().InverseTransformPositionNoScale(C->GetActorLocation());
		if (L.Z < -Depth * 0.5f && L.X > -300.f && L.X < Length() + 300.f && FMath::Abs(L.Y) < 300.f)
		{
			C->TeleportTo(GetActorTransform().TransformPositionNoScale(FVector(-150.f, 0.f, 120.f)), C->GetActorRotation(), false, true);
			C->ApplyKnockdown(By, FVector(0.f, 0.f, 100.f));
			C->Notify(LOCTEXT("TBuried", "The tunnel COLLAPSED on you!"));
		}
	}
	ABRGameState::FX(this, EBRFX::Dust, GetActorTransform().TransformPositionNoScale(FVector(Length(), 0.f, 50.f)));
	Sections = 0;
	bDetected = false;
	OnRep_Sections();
	ForceNetUpdate();
	if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
	{
		GM->Feed(FString::Printf(TEXT("%s collapsed a smuggling tunnel!"), By ? *By->GetPlayerDisplayName() : TEXT("Border Patrol")));
		GM->OnContrabandSeized(By, 2); // big bust
	}
}

#undef LOCTEXT_NAMESPACE
