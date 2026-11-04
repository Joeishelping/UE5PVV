#include "BRGameState.h"
#include "Materials/MaterialInterface.h"
#include "Components/DecalComponent.h"
#include "BRAudio.h"
#include "BRReplay.h"
#include "BRCharacter.h"
#include "EngineUtils.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "BRWorldActors.h"
#include "BRSettings.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

void ABRGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABRGameState, Phase);
	DOREPLIFETIME(ABRGameState, PhaseSecondsLeft);
	DOREPLIFETIME(ABRGameState, Approval);
	DOREPLIFETIME(ABRGameState, TimeOfDay);
	DOREPLIFETIME(ABRGameState, Weather);
	DOREPLIFETIME(ABRGameState, RunnersSafe);
	DOREPLIFETIME(ABRGameState, RunnersNeeded);
	DOREPLIFETIME(ABRGameState, RunnerScore);
	DOREPLIFETIME(ABRGameState, WardenScore);
	DOREPLIFETIME(ABRGameState, WardenBudget);
	DOREPLIFETIME(ABRGameState, BonusTraps);
	DOREPLIFETIME(ABRGameState, FencePowerUntil);
	DOREPLIFETIME(ABRGameState, FencePowerCutUntil);
	DOREPLIFETIME(ABRGameState, BountyTarget);
	DOREPLIFETIME(ABRGameState, bLobby);
	DOREPLIFETIME(ABRGameState, LobbyCountdown);
	DOREPLIFETIME(ABRGameState, bBestOf3);
	DOREPLIFETIME(ABRGameState, SeriesRunnerWins);
	DOREPLIFETIME(ABRGameState, SeriesWardenWins);
	DOREPLIFETIME(ABRGameState, DayNumber);
	DOREPLIFETIME(ABRGameState, TopMoments);
}

void ABRGameState::Multicast_NoisePing_Implementation(FVector_NetQuantize Location)
{
	NoisePings.Add(Location);
	NoisePingTimes.Add(GetWorld()->GetTimeSeconds());
	if (NoisePings.Num() > 12)
	{
		NoisePings.RemoveAt(0);
		NoisePingTimes.RemoveAt(0);
	}
}

void ABRGameState::Multicast_Feed_Implementation(const FString& Line)
{
	FeedLines.Add(Line);
	FeedTimes.Add(GetWorld()->GetTimeSeconds());
	if (FeedLines.Num() > 6)
	{
		FeedLines.RemoveAt(0);
		FeedTimes.RemoveAt(0);
	}
}

void ABRGameState::Sfx(const UObject* Context, EBRSfx Sound, const FVector& Location, bool b2D)
{
	UWorld* World = Context ? Context->GetWorld() : nullptr;
	ABRGameState* GS = World ? World->GetGameState<ABRGameState>() : nullptr;
	if (GS && GS->HasAuthority())
	{
		GS->Multicast_Sfx(Sound, Location, b2D);
	}
}

void ABRGameState::FX(const UObject* Context, EBRFX Kind, const FVector& Location, const FString& Text)
{
	UWorld* World = Context ? Context->GetWorld() : nullptr;
	ABRGameState* GS = World ? World->GetGameState<ABRGameState>() : nullptr;
	if (GS && GS->HasAuthority())
	{
		GS->Multicast_FX(Kind, Location, Text);
	}
}

void ABRGameState::Multicast_Sfx_Implementation(EBRSfx Sound, FVector_NetQuantize Location, bool b2D)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (USoundBase* Wave = GetDefault<UBRSettings>()->GetSound(Sound))
	{
		if (b2D)
		{
			UGameplayStatics::PlaySound2D(this, Wave);
		}
		else
		{
			BRAudio::PlayAt(this, Wave, Location, BRAudio::Medium);
		}
	}
}

void ABRGameState::Highlight(const UObject* Context, ABRCharacter* Star, float Score, const FString& Caption)
{
	UWorld* World = Context ? Context->GetWorld() : nullptr;
	ABRGameState* GS = World ? World->GetGameState<ABRGameState>() : nullptr;
	if (GS && GS->HasAuthority() && Star && GS->Phase == EBRPhase::Invasion)
	{
		// Keep the 5 best moments of the round for the end screen.
		int32 At = 0;
		while (At < GS->TopMomentScores.Num() && GS->TopMomentScores[At] >= Score)
		{
			++At;
		}
		if (At < 5 && !GS->TopMoments.Contains(Caption))
		{
			GS->TopMoments.Insert(Caption, At);
			GS->TopMomentScores.Insert(Score, At);
			if (GS->TopMoments.Num() > 5)
			{
				GS->TopMoments.SetNum(5);
				GS->TopMomentScores.SetNum(5);
			}
		}
		GS->Multicast_Highlight(Star, Score, Caption);
	}
}

void ABRGameState::Multicast_Highlight_Implementation(ABRCharacter* Star, float Score, const FString& Caption)
{
	for (TActorIterator<ABRReplayDirector> It(GetWorld()); It; ++It)
	{
		It->OnHighlight(Star, Score, Caption);
	}
}

void ABRGameState::Multicast_FX_Implementation(EBRFX Kind, FVector_NetQuantize Location, const FString& Text)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	SpawnLocalFX(this, Kind, Location, Text);
}

void ABRGameState::SpawnLocalFX(UObject* Context, EBRFX Kind, const FVector& Location, const FString& Text, float Scale)
{
	UWorld* World = Context ? Context->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (Kind == EBRFX::BloodPool)
	{
		const UBRSettings* FXSettings = GetDefault<UBRSettings>();
		if (!FXSettings->bBloodPools)
		{
			return; // blood puddles switched off
		}
		if (UMaterialInterface* DecalMat = FXSettings->BloodPoolDecal.LoadSynchronous())
		{
			// Your decal, projected straight down onto the floor, random size and spin, fades out at the end.
			const float Radius = 55.f * Scale * FMath::FRandRange(0.8f, 1.3f);
			if (UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(World, DecalMat, FVector(40.f, Radius, Radius), Location,
				FRotator(-90.f, FMath::FRandRange(0.f, 360.f), 0.f), FXSettings->BloodPoolSeconds))
			{
				Decal->SetFadeScreenSize(0.f);
				Decal->SetFadeOut(FMath::Max(0.f, FXSettings->BloodPoolSeconds - 3.f), 3.f, false);
			}
			return;
		}
	}
	if (Kind == EBRFX::Dust && !GetDefault<UBRSettings>()->bDustEffects)
	{
		return;
	}
	if (UNiagaraSystem* System = GetDefault<UBRSettings>()->GetEffect(Kind))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, System, Location, FRotator::ZeroRotator, FVector(Scale * GetDefault<UBRSettings>()->GetEffectSize(Kind)));
		return;
	}
	// No cigarette effect of its own: use your Smoke Effect, shrunk way down (a cigarette puff, not a cannon blast).
	if (Kind == EBRFX::CigaretteSmoke)
	{
		if (UNiagaraSystem* Smoke = GetDefault<UBRSettings>()->GetEffect(EBRFX::Smoke))
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Smoke, Location, FRotator::ZeroRotator, FVector(Scale * 0.15f));
			return;
		}
	}
	// Built-in look: punch hits = quick white flash, sparks = little orange sparks, cigarette = tiny smoke puff.
	const EBRFX Shown = Kind == EBRFX::CigaretteSmoke ? EBRFX::Smoke : Kind;
	const float Size = Kind == EBRFX::CigaretteSmoke ? 0.2f : 1.f;
	const FTransform At(Location);
	if (ABRFX* Effect = World->SpawnActorDeferred<ABRFX>(ABRFX::StaticClass(), At, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Effect->Kind = Shown;
		Effect->Text = Text;
		Effect->FinishSpawning(At);
		Effect->SetActorScale3D(FVector(Size * Scale));
	}
}
