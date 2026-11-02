#include "BRGameState.h"
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
			UGameplayStatics::PlaySoundAtLocation(this, Wave, Location);
		}
	}
}

void ABRGameState::Multicast_FX_Implementation(EBRFX Kind, FVector_NetQuantize Location, const FString& Text)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const FTransform At(Location);
	if (ABRFX* Effect = GetWorld()->SpawnActorDeferred<ABRFX>(ABRFX::StaticClass(), At, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Effect->Kind = Kind;
		Effect->Text = Text;
		Effect->FinishSpawning(At);
	}
}
