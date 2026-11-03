#include "BRGameState.h"
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
	if (UNiagaraSystem* System = GetDefault<UBRSettings>()->GetEffect(Kind))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, System, Location, FRotator::ZeroRotator, FVector(Scale));
		return;
	}
	// Built-in look (punch hits = small dust puff).
	const EBRFX Shown = Kind == EBRFX::PunchHit || Kind == EBRFX::Sparks ? EBRFX::Dust : (Kind == EBRFX::CigaretteSmoke ? EBRFX::Smoke : Kind);
	const float Size = Kind == EBRFX::PunchHit ? 0.35f : (Kind == EBRFX::CigaretteSmoke ? 0.2f : (Kind == EBRFX::Sparks ? 0.25f : 1.f));
	const FTransform At(Location);
	if (ABRFX* Effect = World->SpawnActorDeferred<ABRFX>(ABRFX::StaticClass(), At, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Effect->Kind = Shown;
		Effect->Text = Text;
		Effect->FinishSpawning(At);
		Effect->SetActorScale3D(FVector(Size * Scale));
	}
}
