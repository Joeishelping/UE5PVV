#include "PVVPlayerController.h"
#include "PVVBoat.h"
#include "PVVCharacter.h"
#include "PVVGameMode.h"
#include "Engine/World.h"

void APVVPlayerController::RequestFaction(EPVVFaction Faction)
{
	Server_RequestFaction(Faction);
}

void APVVPlayerController::JoinPirates()
{
	RequestFaction(EPVVFaction::Pirates);
}

void APVVPlayerController::JoinVikings()
{
	RequestFaction(EPVVFaction::Vikings);
}

void APVVPlayerController::Server_RequestFaction_Implementation(EPVVFaction Faction)
{
	if (APVVGameMode* GM = GetWorld()->GetAuthGameMode<APVVGameMode>())
	{
		GM->RequestFaction(this, Faction);
	}
}

void APVVPlayerController::Client_ShowFactionSelect_Implementation()
{
	ShowFactionSelect();
}

void APVVPlayerController::PawnLeavingGame()
{
	// Leaving while driving: hop out first so the boat isn't destroyed with us.
	if (APVVBoat* Boat = Cast<APVVBoat>(GetPawn()))
	{
		if (APVVCharacter* Driver = Boat->GetDriver())
		{
			Boat->Unseat(Driver);
		}
	}
	Super::PawnLeavingGame();
}
