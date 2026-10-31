#include "PVVGameMode.h"
#include "PVVBoat.h"
#include "PVVCharacter.h"
#include "PVVGameState.h"
#include "PVVHUD.h"
#include "PVVLake.h"
#include "PVVTestArena.h"
#include "PVVPlayerController.h"
#include "PVVPlayerStart.h"
#include "PVVPlayerState.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "PVVGameMode"

APVVGameMode::APVVGameMode()
{
	DefaultPawnClass = APVVCharacter::StaticClass();
	PlayerControllerClass = APVVPlayerController::StaticClass();
	PlayerStateClass = APVVPlayerState::StaticClass();
	GameStateClass = APVVGameState::StaticClass();
	HUDClass = APVVHUD::StaticClass();
	TestArenaBoatClass = APVVBoat::StaticClass();
}

void APVVGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (APVVGameState* GS = GetPVVGameState())
	{
		GS->RemainingSeconds = MatchDurationSeconds;
		GS->SetMatchPhase(EPVVMatchPhase::InProgress, EPVVFaction::None);
	}
	GetWorldTimerManager().SetTimer(MatchClockTimer, this, &APVVGameMode::TickMatchClock, 1.f, true);

	if (bBuildTestArenaIfNoLake && !TActorIterator<APVVLake>(GetWorld()))
	{
		BuildTestArena();
	}
}

void APVVGameMode::BuildTestArena()
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APVVTestArena* Arena = GetWorld()->SpawnActor<APVVTestArena>(APVVTestArena::StaticClass(), FTransform(TestArenaLocation), Params);
	if (!Arena)
	{
		return;
	}
	Arena->SpawnGameplayActors(TestArenaBoatClass, TestArenaBoatsPerSide);

	// Anyone who joined before the arena existed gets moved (or spawned) at their team's base.
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}
		if (APVVCharacter* Character = Cast<APVVCharacter>(PC->GetPawn()))
		{
			RespawnCharacter(Character);
		}
		else if (!PC->GetPawn())
		{
			const APVVPlayerState* PS = PC->GetPlayerState<APVVPlayerState>();
			if (PS && PS->GetFaction() != EPVVFaction::None)
			{
				RestartPlayer(PC);
			}
		}
	}
}

void APVVGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	if (bAutoAssignFactions)
	{
		RequestFaction(NewPlayer, PickBalancedFaction());
		return;
	}

	// Wait (pawnless) until the player picks a side.
	if (APVVPlayerController* PC = Cast<APVVPlayerController>(NewPlayer))
	{
		PC->Client_ShowFactionSelect();
	}
}

void APVVGameMode::RequestFaction(APlayerController* PC, EPVVFaction Faction)
{
	APVVPlayerState* PS = PC ? PC->GetPlayerState<APVVPlayerState>() : nullptr;
	if (!PS || Faction == EPVVFaction::None)
	{
		return;
	}
	if (PS->GetFaction() == Faction && PC->GetPawn())
	{
		return;
	}

	if (MaxTeamImbalance > 0 && PS->GetFaction() != Faction)
	{
		const APVVGameState* GS = GetPVVGameState();
		if (!GS)
		{
			return;
		}
		const EPVVFaction Other = Faction == EPVVFaction::Pirates ? EPVVFaction::Vikings : EPVVFaction::Pirates;
		const int32 OtherCount = GS->CountPlayers(Other) - (PS->GetFaction() == Other ? 1 : 0);
		if (GS->CountPlayers(Faction) + 1 - OtherCount > MaxTeamImbalance)
		{
			return;
		}
	}

	// Get out of any boat, then replace the old body.
	if (APVVBoat* Boat = Cast<APVVBoat>(PC->GetPawn()))
	{
		if (APVVCharacter* Driver = Boat->GetDriver())
		{
			Boat->Unseat(Driver);
		}
	}
	if (APawn* OldPawn = PC->GetPawn())
	{
		PC->UnPossess();
		OldPawn->Destroy();
	}

	PS->SetFaction(Faction);
	RestartPlayer(PC);

	const FText FactionName = UEnum::GetDisplayValueAsText(Faction);
	Announce(FText::Format(LOCTEXT("Joined", "{0} joined the {1}."), FText::FromString(PS->GetPlayerName()), FactionName));
}

EPVVFaction APVVGameMode::PickBalancedFaction() const
{
	const APVVGameState* GS = GetPVVGameState();
	if (!GS)
	{
		return EPVVFaction::Pirates;
	}
	return GS->CountPlayers(EPVVFaction::Vikings) < GS->CountPlayers(EPVVFaction::Pirates)
		? EPVVFaction::Vikings
		: EPVVFaction::Pirates;
}

AActor* APVVGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	const APVVPlayerState* PS = Player ? Player->GetPlayerState<APVVPlayerState>() : nullptr;
	if (AActor* Start = ChooseStartForFaction(PS ? PS->GetFaction() : EPVVFaction::None))
	{
		return Start;
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

AActor* APVVGameMode::ChooseStartForFaction(EPVVFaction Faction) const
{
	TArray<APVVPlayerStart*> Candidates;
	for (TActorIterator<APVVPlayerStart> It(GetWorld()); It; ++It)
	{
		if (It->Faction == Faction)
		{
			Candidates.Add(*It);
		}
	}
	return Candidates.Num() > 0 ? Candidates[FMath::RandRange(0, Candidates.Num() - 1)] : nullptr;
}

UClass* APVVGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	const APVVPlayerState* PS = InController ? InController->GetPlayerState<APVVPlayerState>() : nullptr;
	if (PS)
	{
		if (PS->GetFaction() == EPVVFaction::Pirates && PirateCharacterClass)
		{
			return PirateCharacterClass;
		}
		if (PS->GetFaction() == EPVVFaction::Vikings && VikingCharacterClass)
		{
			return VikingCharacterClass;
		}
	}
	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

void APVVGameMode::RespawnCharacter(APVVCharacter* Character)
{
	if (!Character)
	{
		return;
	}
	const AActor* Start = ChooseStartForFaction(Character->GetFaction());
	Character->ResetForRespawn(Start ? Start->GetActorTransform() : FTransform(FVector(0.f, 0.f, 500.f)));
}

void APVVGameMode::NotifyKnockedDown(APVVCharacter* Victim, AActor* Cause)
{
	// No points for a knockdown; it only opens the door to a capture.
}

void APVVGameMode::NotifyCaptured(APVVCharacter* Victim, APVVCharacter* Captor)
{
	if (APVVGameState* GS = GetPVVGameState())
	{
		GS->AddScore(Captor->GetFaction(), PointsPerCapture);
	}
	if (APVVPlayerState* PS = Captor->GetPVVPlayerState())
	{
		++PS->Captures;
	}
	Announce(FText::Format(LOCTEXT("Captured", "{0} zip-tied {1}!"),
		FText::FromString(Captor->GetPlayerDisplayName()), FText::FromString(Victim->GetPlayerDisplayName())));
}

void APVVGameMode::NotifyFreed(APVVCharacter* Victim, APVVCharacter* Rescuer)
{
	if (Rescuer)
	{
		if (APVVGameState* GS = GetPVVGameState())
		{
			GS->AddScore(Rescuer->GetFaction(), PointsPerRescue);
		}
		if (APVVPlayerState* PS = Rescuer->GetPVVPlayerState())
		{
			++PS->Rescues;
		}
		Announce(FText::Format(LOCTEXT("Rescued", "{0} cut {1} loose!"),
			FText::FromString(Rescuer->GetPlayerDisplayName()), FText::FromString(Victim->GetPlayerDisplayName())));
	}
	else
	{
		Announce(FText::Format(LOCTEXT("Escaped", "{0} wriggled free!"), FText::FromString(Victim->GetPlayerDisplayName())));
	}
}

void APVVGameMode::NotifyDetained(APVVCharacter* Victim, APVVCharacter* Captor)
{
	if (APVVGameState* GS = GetPVVGameState())
	{
		GS->AddScore(Captor->GetFaction(), PointsPerDetention);
	}
	if (APVVPlayerState* PS = Captor->GetPVVPlayerState())
	{
		++PS->Detentions;
	}
	if (APVVPlayerState* PS = Victim->GetPVVPlayerState())
	{
		++PS->TimesDetained;
	}
	Announce(FText::Format(LOCTEXT("Detained", "{0} hauled {1} off to processing!"),
		FText::FromString(Captor->GetPlayerDisplayName()), FText::FromString(Victim->GetPlayerDisplayName())));

	CheckWinConditions();
}

void APVVGameMode::NotifyJailbreak(APVVCharacter* Rescuer, int32 NumFreed)
{
	if (APVVGameState* GS = GetPVVGameState())
	{
		GS->AddScore(Rescuer->GetFaction(), PointsPerJailbreakPrisoner * NumFreed);
	}
	if (APVVPlayerState* PS = Rescuer->GetPVVPlayerState())
	{
		PS->Rescues += NumFreed;
	}
	Announce(FText::Format(LOCTEXT("Jailbreak", "JAILBREAK! {0} sprung {1} prisoner(s)!"),
		FText::FromString(Rescuer->GetPlayerDisplayName()), FText::AsNumber(NumFreed)));
}

void APVVGameMode::CheckWinConditions()
{
	const APVVGameState* GS = GetPVVGameState();
	if (!GS || GS->MatchPhase != EPVVMatchPhase::InProgress || !bVikingsWinWhenAllPiratesDetained)
	{
		return;
	}

	int32 Pirates = 0;
	int32 DetainedPirates = 0;
	for (TActorIterator<APVVCharacter> It(GetWorld()); It; ++It)
	{
		if (It->GetFaction() == EPVVFaction::Pirates && It->IsPlayerCharacter())
		{
			++Pirates;
			DetainedPirates += It->GetCondition() == EPVVCondition::Detained ? 1 : 0;
		}
	}
	if (Pirates > 0 && Pirates == DetainedPirates)
	{
		EndMatch(EPVVFaction::Vikings);
	}
}

void APVVGameMode::TickMatchClock()
{
	APVVGameState* GS = GetPVVGameState();
	if (!GS || GS->MatchPhase != EPVVMatchPhase::InProgress)
	{
		return;
	}

	GS->RemainingSeconds = FMath::Max(0, GS->RemainingSeconds - 1);
	if (GS->RemainingSeconds == 0)
	{
		const EPVVFaction Winner = GS->PirateScore > GS->VikingScore ? EPVVFaction::Pirates
			: GS->VikingScore > GS->PirateScore ? EPVVFaction::Vikings
			: EPVVFaction::None;
		EndMatch(Winner);
	}
}

void APVVGameMode::EndMatch(EPVVFaction Winner)
{
	APVVGameState* GS = GetPVVGameState();
	if (!GS || GS->MatchPhase == EPVVMatchPhase::Ended)
	{
		return;
	}

	GS->SetMatchPhase(EPVVMatchPhase::Ended, Winner);
	GetWorldTimerManager().ClearTimer(MatchClockTimer);

	Announce(Winner == EPVVFaction::None
		? LOCTEXT("Draw", "Time! It's a draw.")
		: FText::Format(LOCTEXT("Win", "The {0} win!"), UEnum::GetDisplayValueAsText(Winner)));

	GetWorldTimerManager().SetTimer(RestartTimer, this, &APVVGameMode::RestartMatch, RestartDelaySeconds, false);
}

void APVVGameMode::RestartMatch()
{
	GetWorld()->ServerTravel(TEXT("?Restart"), false);
}

APVVGameState* APVVGameMode::GetPVVGameState() const
{
	return GetGameState<APVVGameState>();
}

void APVVGameMode::Announce(const FText& Message) const
{
	if (APVVGameState* GS = GetPVVGameState())
	{
		GS->Multicast_Announce(Message);
	}
}

#undef LOCTEXT_NAMESPACE
