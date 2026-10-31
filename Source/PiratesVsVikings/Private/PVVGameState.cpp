#include "PVVGameState.h"
#include "PVVPlayerState.h"
#include "PiratesVsVikings.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

void APVVGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APVVGameState, PirateScore);
	DOREPLIFETIME(APVVGameState, VikingScore);
	DOREPLIFETIME(APVVGameState, RemainingSeconds);
	DOREPLIFETIME(APVVGameState, Winner);
	DOREPLIFETIME(APVVGameState, MatchPhase);
}

void APVVGameState::AddScore(EPVVFaction Faction, int32 Points)
{
	if (!HasAuthority())
	{
		return;
	}
	if (Faction == EPVVFaction::Pirates)
	{
		PirateScore += Points;
	}
	else if (Faction == EPVVFaction::Vikings)
	{
		VikingScore += Points;
	}
}

void APVVGameState::SetMatchPhase(EPVVMatchPhase NewPhase, EPVVFaction InWinner)
{
	if (!HasAuthority())
	{
		return;
	}
	Winner = InWinner;
	MatchPhase = NewPhase;
	OnRep_MatchPhase();
	ForceNetUpdate();
}

int32 APVVGameState::CountPlayers(EPVVFaction Faction) const
{
	int32 Count = 0;
	for (const APlayerState* PS : PlayerArray)
	{
		const APVVPlayerState* PVVPS = Cast<APVVPlayerState>(PS);
		if (PVVPS && PVVPS->GetFaction() == Faction)
		{
			++Count;
		}
	}
	return Count;
}

void APVVGameState::Multicast_Announce_Implementation(const FText& Message)
{
	UE_LOG(LogPVV, Log, TEXT("%s"), *Message.ToString());
	RecentMessages.Add(Message.ToString());
	RecentMessageTimes.Add(GetWorld()->GetTimeSeconds());
	if (RecentMessages.Num() > 6)
	{
		RecentMessages.RemoveAt(0);
		RecentMessageTimes.RemoveAt(0);
	}
	OnAnnouncement(Message);
}

void APVVGameState::OnRep_MatchPhase()
{
	if (MatchPhase == EPVVMatchPhase::Ended)
	{
		OnMatchEnded(Winner);
	}
}
