#include "PVVPlayerState.h"
#include "Net/UnrealNetwork.h"

void APVVPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APVVPlayerState, Faction);
	DOREPLIFETIME(APVVPlayerState, Captures);
	DOREPLIFETIME(APVVPlayerState, Detentions);
	DOREPLIFETIME(APVVPlayerState, Rescues);
	DOREPLIFETIME(APVVPlayerState, TimesDetained);
}

void APVVPlayerState::SetFaction(EPVVFaction NewFaction)
{
	if (!HasAuthority() || Faction == NewFaction)
	{
		return;
	}
	Faction = NewFaction;
	OnRep_Faction();
	ForceNetUpdate();
}

void APVVPlayerState::OnRep_Faction()
{
	OnFactionChanged(Faction);
}
