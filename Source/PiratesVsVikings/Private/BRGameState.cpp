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
