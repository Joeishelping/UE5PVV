#include "BRGameState.h"
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
