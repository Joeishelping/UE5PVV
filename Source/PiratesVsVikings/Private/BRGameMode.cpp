#include "BRGameMode.h"
#include "BRGameInstance.h"
#include "BRBotController.h"
#include "BRCar.h"
#include "BRCharacter.h"
#include "BRDesertArena.h"
#include "BRMapKit.h"
#include "BRDog.h"
#include "BRGameState.h"
#include "BRHUD.h"
#include "BRPlayerController.h"
#include "BRPlayerState.h"
#include "BRWorldActors.h"
#include "PVVPlayerStart.h"
#include "PVVLake.h"
#include "PVVBoat.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "BRGameMode"

ABRGameMode::ABRGameMode()
{
	DefaultPawnClass = ABRCharacter::StaticClass();
	PlayerControllerClass = ABRPlayerController::StaticClass();
	PlayerStateClass = ABRPlayerState::StaticClass();
	GameStateClass = ABRGameState::StaticClass();
	HUDClass = ABRHUD::StaticClass();
	NPCPassengerClass = ABRCharacter::StaticClass();
	ArenaClass = ABRDesertArena::StaticClass();
	RunnerTruckClass = ABRTruck::StaticClass();
	WardenTruckClass = ABRTruck::StaticClass();
	NPCTruckClass = ABRTruck::StaticClass();
	DogClass = ABRDog::StaticClass();
	bBuildTestArenaIfNoLake = false;
	bVikingsWinWhenAllPiratesDetained = false;
}

ABRGameState* ABRGameMode::GetBRGameState() const
{
	return GetGameState<ABRGameState>();
}

FString ABRGameMode::NameOf(const APVVCharacter* Character) const
{
	return Character ? Character->GetPlayerDisplayName() : TEXT("Someone");
}

// -------------------------------------------------------------------------------------------
// Setup

void ABRGameMode::StartMatchFlow()
{
	UWorld* World = GetWorld();
	for (TActorIterator<ABRDesertArena> It(World); It; ++It)
	{
		Arena = *It;
		break;
	}
	// Your own map (Map Kit pieces placed)? Then the built-in desert is only used for sky, day/night, weather and music.
	const bool bCustomMap = !bUseBuiltInTestMap || BRMap::IsCustomMap(World);
	if (!Arena && (bCustomMap || !TActorIterator<APVVLake>(World)))
	{
		UClass* MapClass = ArenaClass ? ArenaClass.Get() : ABRDesertArena::StaticClass();
		const FTransform At = bCustomMap ? FTransform::Identity : FTransform(ArenaLocation);
		Arena = World->SpawnActorDeferred<ABRDesertArena>(MapClass, At, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Arena)
		{
			Arena->bBuildTestMap = !bCustomMap;
			Arena->FinishSpawning(At);
		}
	}
	if (Arena && Arena->bBuildTestMap)
	{
		Arena->SpawnGameplayActors(RunnerTruckClass, WardenTruckClass, DogClass);
		RelocateExistingPlayers();
	}
	SpawnVillagers();

	UBRGameInstance* GI = GetGameInstance<UBRGameInstance>();
	if (GI && GI->Host.bSet)
	{
		ApplyHostSettings(GI->Host);
	}
	if (ABRGameState* GS = GetBRGameState())
	{
		GS->bLobby = bUseLobby && GetNetMode() != NM_Standalone;
		GS->LobbyCountdown = 0;
		GS->bBestOf3 = bBestOf3;
		if (GI && bBestOf3)
		{
			GS->SeriesRunnerWins = GI->SeriesRunnerWins;
			GS->SeriesWardenWins = GI->SeriesWardenWins;
		}
		GS->Phase = EBRPhase::Prep;
		GS->PhaseSecondsLeft = PrepSeconds;
		GS->TimeOfDay = StartHour;
		GS->Approval = 100.f;
		GS->WardenBudget = StartingBudget;
		GS->BonusTraps = 0;
		GS->BountyTarget = nullptr;
		GS->SetMatchPhase(EPVVMatchPhase::InProgress, EPVVFaction::None);
	}
	NextWeatherChange = FMath::FRandRange(WeatherChangeSeconds.X, WeatherChangeSeconds.Y);
	GetWorldTimerManager().SetTimer(TickTimer, this, &ABRGameMode::TickBR, 1.f, true);
}

EPVVFaction ABRGameMode::PickBalancedFaction() const
{
	const APVVGameState* GS = GetGameState<APVVGameState>();
	if (!GS)
	{
		return BR::Runners;
	}
	const int32 Runners = GS->CountPlayers(BR::Runners);
	const int32 Wardens = GS->CountPlayers(BR::Wardens);
	const int32 Total = Runners + Wardens + 1;
	const int32 WantWardens = FMath::Max(1, FMath::RoundToInt(Total * WardenRatio));
	// The first player is a Runner (so solo testing works); after that keep about a third as Wardens.
	return (Runners > 0 && Wardens < WantWardens) ? BR::Wardens : BR::Runners;
}

// -------------------------------------------------------------------------------------------
// Clock

void ABRGameMode::TickBR()
{
	ABRGameState* GS = GetBRGameState();
	if (!GS)
	{
		return;
	}

	GS->TimeOfDay = FMath::Fmod(GS->TimeOfDay + 24.f / (DayLengthMinutes * 60.f), 24.f);

	NextWeatherChange -= 1.f;
	if (NextWeatherChange <= 0.f)
	{
		NextWeatherChange = FMath::FRandRange(WeatherChangeSeconds.X, WeatherChangeSeconds.Y);
		const float Roll = FMath::FRand();
		const EBRWeather NewWeather = Roll < 0.55f ? EBRWeather::Clear : (Roll < 0.8f ? EBRWeather::Rain : EBRWeather::Sandstorm);
		if (NewWeather != GS->Weather)
		{
			GS->Weather = NewWeather;
			Announce(NewWeather == EBRWeather::Rain ? LOCTEXT("Rain", "Rain rolls in. The river is running fast.")
				: NewWeather == EBRWeather::Sandstorm ? LOCTEXT("Sand", "SANDSTORM! Visibility is terrible - perfect for sneaking.")
				: LOCTEXT("Clear", "The skies clear up."));
		}
	}

	if (GS->Phase == EBRPhase::Ended)
	{
		return;
	}
	if (!bCarKeyGiven)
	{
		GiveCarKey();
	}

	if (GS->bLobby)
	{
		TickBots();
		TickLobby(GS);
		return; // the Prep clock waits until everyone's ready
	}
	GS->PhaseSecondsLeft = FMath::Max(0, GS->PhaseSecondsLeft - 1);
	if (GS->PhaseSecondsLeft > 0 && GS->PhaseSecondsLeft <= 5)
	{
		ABRGameState::Sfx(this, EBRSfx::Countdown, FVector::ZeroVector, true); // 5, 4, 3, 2, 1
	}
	if (GS->PhaseSecondsLeft == 60 && GS->Phase == EBRPhase::Prep)
	{
		Announce(LOCTEXT("OneMinute", "One minute until the Line opens!"));
	}
	TickRunners(GS);
	TickClients();
	TickBots();
	if (GS->Phase == EBRPhase::Invasion)
	{
		TickBounty(GS);
		NextNPCTruck -= 1.f;
		if (NextNPCTruck <= 0.f)
		{
			NextNPCTruck = FMath::FRandRange(NPCTruckEverySeconds.X, NPCTruckEverySeconds.Y);
			SpawnNPCTruck();
		}
	}
	if (GS->PhaseSecondsLeft == 0)
	{
		AdvancePhase();
	}
}

void ABRGameMode::AdvancePhase()
{
	ABRGameState* GS = GetBRGameState();
	if (!GS)
	{
		return;
	}
	if (GS->bLobby)
	{
		GS->bLobby = false; // host skipped the lobby
		GS->PhaseSecondsLeft = PrepSeconds;
		Announce(LOCTEXT("LobbySkipped", "Let's go! Prep has started."));
		return;
	}
	if (GS->Phase == EBRPhase::Prep)
	{
		GS->Phase = EBRPhase::Invasion;
		GS->PhaseSecondsLeft = InvasionSeconds;
		GS->RunnersNeeded = FMath::Max(1, FMath::DivideAndRoundUp(GS->CountPlayers(BR::Runners), 2));
		NextNPCTruck = 3.f;
		ABRGameState::Sfx(this, EBRSfx::RoundStart, FVector::ZeroVector, true);
		Announce(FText::Format(LOCTEXT("Invasion", "THE LINE IS OPEN! {0}: cross, smuggle, score. {1}: catch, scan, score. Most points wins!"),
			FText::FromString(BR::RunnerName()), FText::FromString(BR::WardenName())));
	}
	else if (GS->Phase == EBRPhase::Invasion)
	{
		GS->Phase = EBRPhase::Ended;
		ABRGameState::Sfx(this, EBRSfx::RoundEnd, FVector::ZeroVector, true);
		const EPVVFaction Winner = GS->RunnerScore > GS->WardenScore ? BR::Runners : (GS->WardenScore > GS->RunnerScore ? BR::Wardens : EPVVFaction::None);
		Announce(FText::Format(LOCTEXT("FinalScore", "TIME! {0} {1} - {2} {3}"), FText::FromString(BR::RunnerName()), GS->RunnerScore, GS->WardenScore, FText::FromString(BR::WardenName())));
		if (bBestOf3)
		{
			GS->SeriesRunnerWins += Winner == BR::Runners ? 1 : 0;
			GS->SeriesWardenWins += Winner == BR::Wardens ? 1 : 0;
			UBRGameInstance* SeriesGI = GetGameInstance<UBRGameInstance>();
			if (GS->SeriesRunnerWins >= 2 || GS->SeriesWardenWins >= 2)
			{
				const bool bRunnersWon = GS->SeriesRunnerWins >= 2;
				Announce(FText::Format(LOCTEXT("SeriesWon", "{0} WIN THE SERIES {1} - {2}!"), FText::FromString((bRunnersWon ? BR::RunnerName() : BR::WardenName()).ToUpper()),
					FMath::Max(GS->SeriesRunnerWins, GS->SeriesWardenWins), FMath::Min(GS->SeriesRunnerWins, GS->SeriesWardenWins)));
				if (SeriesGI)
				{
					SeriesGI->SeriesRunnerWins = 0; // a new series next time
					SeriesGI->SeriesWardenWins = 0;
				}
			}
			else
			{
				Announce(FText::Format(LOCTEXT("SeriesScore", "Series: {0} {1} - {2} {3}. Next round!"), FText::FromString(BR::RunnerName()), GS->SeriesRunnerWins, GS->SeriesWardenWins, FText::FromString(BR::WardenName())));
				if (SeriesGI)
				{
					SeriesGI->SeriesRunnerWins = GS->SeriesRunnerWins;
					SeriesGI->SeriesWardenWins = GS->SeriesWardenWins;
				}
			}
		}
		EndMatch(Winner);
	}
}

void ABRGameMode::CheckWinConditions()
{
	// Points race: nothing ends the round early. The team with more points when the Invasion timer runs out wins.
}

void ABRGameMode::TickRunners(ABRGameState* GS)
{
	TArray<APVVCharacter*> ToLaunch;
	for (TActorIterator<APVVCharacter> It(GetWorld()); It; ++It)
	{
		APVVCharacter* C = *It;
		ABRPlayerState* PS = Cast<ABRPlayerState>(C->GetPVVPlayerState());
		if (!PS || C->GetFaction() != BR::Runners)
		{
			continue;
		}
		if (PS->AssignedJob.IsEmpty())
		{
			AssignJob(PS);
		}

		if (C->GetCondition() == EPVVCondition::Detained)
		{
			PS->SentenceSeconds = FMath::Max(0, PS->SentenceSeconds - 1);
			if (PS->SentenceSeconds == 0)
			{
				ToLaunch.Add(C);
			}
			continue;
		}
		if (GS->Phase != EBRPhase::Invasion)
		{
			continue;
		}

		const FVector Where = C->GetActorLocation();
		if (!PS->bHasCrossedLine && BRMap::HasCrossedLine(GetWorld(), Where))
		{
			PS->bHasCrossedLine = true;
			if (ABRCharacter* BRC = Cast<ABRCharacter>(C))
			{
				BRC->Notify(FText::Format(LOCTEXT("Crossed", "You crossed the Line. {0} can now arrest you."), FText::FromString(BR::WardenName())));
			}
		}
		const bool bFree = C->GetCondition() == EPVVCondition::Normal && !C->GetCarrier();
		if (!PS->bReachedSafety && bFree && BRMap::IsInSafeZone(GetWorld(), Where))
		{
			// Made it! Get paid (more for contraband), score, then get sent back to Mexico to go again.
			PS->bReachedSafety = true;
			++GS->RunnersSafe;
			++PS->StatCrossings;
			int32 Contraband = 0;
			for (const EBRItem Item : PS->Inventory)
			{
				Contraband += BR::IsIllegal(Item) ? 1 : 0;
			}
			PS->RemoveIllegalItems();
			PS->StatContraband += Contraband;
			const int32 Pay = CrossingPay + Contraband * ContrabandPay;
			PS->AddMoney(Pay);
			ABRGameState::Sfx(this, EBRSfx::SafeZone, Where);
			ABRGameState::FX(this, EBRFX::MoneyText, Where + FVector(0.f, 0.f, 60.f), FString::Printf(TEXT("+$%d"), Pay));
			AwardPoints(C, PointsPerCrossing + Contraband * PointsPerContraband);
			Announce(Contraband > 0
				? FText::Format(LOCTEXT("SafeContraband", "{0} MADE IT with {1} contraband! +${2}"), FText::FromString(NameOf(C)), Contraband, Pay)
				: FText::Format(LOCTEXT("Safe", "{0} MADE IT to the Safe Zone! +${1}"), FText::FromString(NameOf(C)), Pay));
			if (ABRCharacter* BRC = Cast<ABRCharacter>(C))
			{
				BRC->Notify(LOCTEXT("SentBack", "You made it! Paid. Heading back to Mexico for another run..."));
			}
			SendHomeLater(C, 4.f);
		}
	}
	for (APVVCharacter* C : ToLaunch)
	{
		LaunchFromPen(C);
	}
}

void ABRGameMode::GiveCarKey()
{
	TArray<ABRPlayerState*> Runners;
	for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
	{
		if (It->IsRunner())
		{
			if (ABRPlayerState* PS = It->GetBRPlayerState())
			{
				Runners.Add(PS);
			}
		}
	}
	if (Runners.Num() == 0)
	{
		return;
	}
	ABRPlayerState* Lucky = Runners[FMath::RandRange(0, Runners.Num() - 1)];
	if (Lucky->AddItem(EBRItem::CarKey))
	{
		bCarKeyGiven = true;
		Announce(FText::Format(LOCTEXT("Key", "{0} has the keys to the team car. It's out of gas, of course."), FText::FromString(Lucky->GetPlayerName())));
	}
}

// -------------------------------------------------------------------------------------------
// Catching, pen and catapult

void ABRGameMode::NotifyDetained(APVVCharacter* Victim, APVVCharacter* Captor)
{
	if (const ABRCharacter* NPC = Cast<ABRCharacter>(Victim); NPC && NPC->IsNPC() && NPC->bIllegal)
	{
		ChangeApproval(GoodCatchBonus);
		AwardPoints(Captor, PointsPerIllegalNPC);
		Feed(FString::Printf(TEXT("%s caught someone with no papers"), *NameOf(Captor)));
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Victim, [Victim]() { Victim->Destroy(); }), 15.f, false);
		return;
	}
	if (const ABRCharacter* NPC = Cast<ABRCharacter>(Victim); NPC && NPC->IsNPC())
	{
		ChangeApproval(-WrongfulArrestPenalty);
		Announce(FText::Format(LOCTEXT("Tourist", "{0} locked up a random citizen! Approval -{1}"), FText::FromString(NameOf(Captor)), FMath::RoundToInt(WrongfulArrestPenalty)));
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Victim, [Victim]() { Victim->Destroy(); }), 15.f, false);
		return;
	}
	Super::NotifyDetained(Victim, Captor);

	ABRPlayerState* PS = Victim ? Cast<ABRPlayerState>(Victim->GetPVVPlayerState()) : nullptr;
	if (!PS)
	{
		return;
	}
	PS->SentenceSeconds = SentenceSeconds + RepeatOffenderSeconds * FMath::Max(0, PS->TimesDetained - 1);

	int32 Contraband = 0;
	for (const EBRItem Item : PS->Inventory)
	{
		Contraband += BR::IsIllegal(Item) ? 1 : 0;
	}
	if (Contraband > 0)
	{
		PS->RemoveIllegalItems();
		OnContrabandSeized(Captor, Contraband);
	}
	if (PS->bHasCrossedLine)
	{
		ChangeApproval(GoodCatchBonus);
		AwardPoints(Captor, PointsPerCatch);
		if (ABRPlayerState* CaptorPS = Captor ? Cast<ABRPlayerState>(Captor->GetPVVPlayerState()) : nullptr)
		{
			++CaptorPS->StatCatches;
		}
		OnGoodCatch(Captor, PS);
		Feed(FString::Printf(TEXT("%s locked up %s"), *NameOf(Captor), *NameOf(Victim)));
	}
	else
	{
		ChangeApproval(-WrongfulArrestPenalty);
		AwardPoints(Captor, PointsPerWrongfulArrest);
		if (ABRPlayerState* CaptorPS = Captor ? Cast<ABRPlayerState>(Captor->GetPVVPlayerState()) : nullptr)
		{
			++CaptorPS->StatWrongful;
		}
		Announce(FText::Format(LOCTEXT("Wrongful", "Wrongful arrest! {0} never crossed the Line. Approval -{1}"), FText::FromString(NameOf(Victim)), FMath::RoundToInt(WrongfulArrestPenalty)));
	}
	if (ABRCharacter* BRC = Cast<ABRCharacter>(Victim))
	{
		BRC->Notify(FText::Format(LOCTEXT("Sentence", "Detained for {0}:{1}. Your crew can break you out!"),
			PS->SentenceSeconds / 60, FText::FromString(FString::Printf(TEXT("%02d"), PS->SentenceSeconds % 60))));
	}
}

void ABRGameMode::LaunchFromPen(APVVCharacter* Prisoner)
{
	ABRCatapult* Catapult = (Arena && Arena->bBuildTestMap) ? Arena->GetCatapult() : BRMap::FindCatapult(GetWorld());
	CatapultRunner(Prisoner, Catapult, nullptr);
}

void ABRGameMode::CatapultRunner(APVVCharacter* Prisoner, ABRCatapult* Catapult, APVVCharacter* Warden)
{
	ABRPlayerState* PS = Prisoner ? Cast<ABRPlayerState>(Prisoner->GetPVVPlayerState()) : nullptr;
	const ABRCharacter* NPC = Cast<ABRCharacter>(Prisoner);
	if (!Prisoner || (!PS && !(NPC && NPC->IsNPC())))
	{
		return;
	}
	FVector Landing;
	if (!BRMap::GetCatapultLanding(GetWorld(), Landing))
	{
		if (Arena && Arena->bBuildTestMap)
		{
			Landing = Arena->GetCatapultLanding();
		}
		else
		{
			// No landing marker: throw them back toward the Runner spawn.
			Landing = Prisoner->GetActorLocation();
			for (TActorIterator<APVVPlayerStart> It(GetWorld()); It; ++It)
			{
				if (It->Faction == BR::Runners)
				{
					Landing = It->GetActorLocation();
					break;
				}
			}
		}
	}

	if (NPC && NPC->IsNPC() && NPC->bIllegal)
	{
		ChangeApproval(GoodCatchBonus);
		AwardPoints(Warden, PointsPerIllegalNPC);
		if (ABRCharacter* Client = Cast<ABRCharacter>(Prisoner))
		{
			Client->Leader = nullptr; // their coyote lost them
			Client->ClientPay = 0;
			Client->StartWandering(Landing, 900.f);
		}
	}
	else if (NPC && NPC->IsNPC())
	{
		ChangeApproval(-HarassmentPenalty);
		Announce(FText::Format(LOCTEXT("TouristYeet", "{0} catapulted an innocent citizen! Approval -{1}"),
			FText::FromString(NameOf(Warden)), FMath::RoundToInt(HarassmentPenalty)));
	}
	else
	{
		++PS->StatCatapulted;
		int32 Contraband = 0;
		for (const EBRItem Item : PS->Inventory)
		{
			Contraband += BR::IsIllegal(Item) ? 1 : 0;
		}
		if (Warden && Contraband > 0)
		{
			OnContrabandSeized(Warden, Contraband);
		}
		if (Warden)
		{
			if (PS->bHasCrossedLine)
			{
				ChangeApproval(GoodCatchBonus);
				AwardPoints(Warden, PointsPerCatch);
				if (ABRPlayerState* WPS = Cast<ABRPlayerState>(Warden->GetPVVPlayerState()))
				{
					++WPS->StatCatches;
				}
				OnGoodCatch(Warden, PS);
			}
			else
			{
				ChangeApproval(-WrongfulArrestPenalty);
				AwardPoints(Warden, PointsPerWrongfulArrest);
				if (ABRPlayerState* WPS = Cast<ABRPlayerState>(Warden->GetPVVPlayerState()))
				{
					++WPS->StatWrongful;
				}
				Announce(FText::Format(LOCTEXT("WrongfulYeet", "{0} never even crossed the Line! Approval -{1}"),
					FText::FromString(NameOf(Prisoner)), FMath::RoundToInt(WrongfulArrestPenalty)));
			}
		}
		PS->RemoveIllegalItems();
		PS->bHasCrossedLine = false;
		PS->bIsAggressor = false;
		PS->SentenceSeconds = 0;
	}

	if (Catapult)
	{
		Catapult->Launch(Prisoner, Landing);
	}
	else
	{
		if (Prisoner->GetCarrier())
		{
			Prisoner->GetCarrier()->TakeCarried();
		}
		Prisoner->ReleaseFromDetention(Landing); // no catapult placed: just send them home
		Prisoner->TeleportTo(Landing, Prisoner->GetActorRotation());
		Prisoner->ClearRestraint();
	}
	Feed(Warden ? FString::Printf(TEXT("%s YEETED %s back to Mexico"), *NameOf(Warden), *NameOf(Prisoner))
		: FString::Printf(TEXT("%s was RETURNED TO SENDER"), *NameOf(Prisoner)));
}

void ABRGameMode::HandleLethalShot(APVVCharacter* Victim, APVVCharacter* Shooter)
{
	if (const ABRCharacter* NPC = Cast<ABRCharacter>(Victim); NPC && NPC->IsNPC())
	{
		ChangeApproval(-ScandalPenalty);
		Announce(FText::Format(LOCTEXT("CivilianShot", "SCANDAL! {0} shot an innocent citizen! Approval -{1}"), FText::FromString(NameOf(Shooter)), FMath::RoundToInt(ScandalPenalty)));
		Victim->ApplyKnockdown(Shooter, FVector(0.f, 0.f, 200.f));
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Victim, [Victim]() { Victim->Destroy(); }), 10.f, false);
		return;
	}
	ABRPlayerState* PS = Victim ? Cast<ABRPlayerState>(Victim->GetPVVPlayerState()) : nullptr;
	if (!PS || Victim->GetFaction() != BR::Runners || Victim->GetCondition() == EPVVCondition::Detained)
	{
		return;
	}

	const bool bJustified = PS->bHasCrossedLine || PS->bIsAggressor;
	if (bJustified)
	{
		Announce(FText::Format(LOCTEXT("Shot", "{0} shot {1}."), FText::FromString(NameOf(Shooter)), FText::FromString(NameOf(Victim))));
	}
	else
	{
		ChangeApproval(-ScandalPenalty);
		Announce(FText::Format(LOCTEXT("Scandal", "SCANDAL! {0} shot {1}, who never crossed the Line and was unarmed! Approval -{2}"),
			FText::FromString(NameOf(Shooter)), FText::FromString(NameOf(Victim)), FMath::RoundToInt(ScandalPenalty)));
	}

	if (Victim->GetCondition() == EPVVCondition::Normal)
	{
		Victim->ApplyKnockdown(Shooter, (Victim->GetActorLocation() - Shooter->GetActorLocation()).GetSafeNormal2D() * 400.f);
	}
	if (ABRCharacter* BRC = Cast<ABRCharacter>(Victim))
	{
		BRC->Notify(LOCTEXT("YouDied", "You were shot. Respawning in town..."));
	}
	SendHomeLater(Victim, RunnerRespawnSeconds);
}

void ABRGameMode::SendHomeLater(APVVCharacter* Character, float Seconds)
{
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateUObject(this, &ABRGameMode::RespawnRunner, TWeakObjectPtr<APVVCharacter>(Character)), Seconds, false);
}

void ABRGameMode::HandleDrowned(APVVCharacter* Victim)
{
	if (!Victim || Victim->GetCondition() == EPVVCondition::Detained)
	{
		return;
	}
	Announce(FText::Format(LOCTEXT("Drowned", "{0} tried to swim the river and DROWNED. Should've bought a boat."), FText::FromString(NameOf(Victim))));
	if (APVVCharacter* Carrier = Victim->GetCarrier())
	{
		Carrier->DropCarried();
	}
	if (Victim->GetCondition() == EPVVCondition::Normal)
	{
		Victim->ApplyKnockdown(nullptr, FVector::ZeroVector);
	}
	SendHomeLater(Victim, 6.f);
}

void ABRGameMode::OnVehicleSearched(APVVCharacter* Warden, int32 RunnersFound)
{
	if (RunnersFound > 0)
	{
		Announce(FText::Format(LOCTEXT("SearchHit", "{0} searched a truck and found {1} Runner(s)!"), FText::FromString(NameOf(Warden)), RunnersFound));
		ChangeApproval(GoodCatchBonus);
	}
	else
	{
		ChangeApproval(-HarassmentPenalty);
		Announce(FText::Format(LOCTEXT("Harass", "{0} dragged innocent citizens out of their truck. Approval -{1}"), FText::FromString(NameOf(Warden)), FMath::RoundToInt(HarassmentPenalty)));
	}
}

void ABRGameMode::SpawnNPCTruck()
{
	// Route: your Road Points, or the built-in map's road.
	TArray<FVector> Route;
	int32 CheckpointIndex = INDEX_NONE;
	FTransform Spawn;
	ABRCheckpointGate* RouteGate = nullptr;
	if (!BRMap::GetRoadRoute(GetWorld(), Route, CheckpointIndex, Spawn, RouteGate))
	{
		if (!Arena || !Arena->bBuildTestMap)
		{
			if (!bWarnedNoRoad && GEngine)
			{
				bWarnedNoRoad = true;
				GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Orange, TEXT("No NPC trucks: this map needs at least 2 BR Road Points (Order 0, 1, 2...)."));
			}
			return; // custom map without Road Points = no NPC traffic
		}
		Arena->GetNPCRoute(Route, CheckpointIndex);
		Spawn = Arena->GetNPCSpawn();
		RouteGate = Arena->GetGate();
	}
	int32 Alive = 0;
	for (TActorIterator<ABRTruck> It(GetWorld()); It; ++It)
	{
		Alive += It->IsNPCTruck() ? 1 : 0;
	}
	if (Alive >= MaxNPCTrucks)
	{
		if (MaxNPCTrucks <= 0 && !bWarnedNoRoad && GEngine)
		{
			bWarnedNoRoad = true;
			GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Orange, TEXT("No NPC trucks: BP_BRGameMode -> Max NPC Trucks is 0."));
		}
		return;
	}

	UClass* TruckClass = NPCTruckClass ? NPCTruckClass.Get() : ABRTruck::StaticClass();
	ABRTruck* Truck = GetWorld()->SpawnActorDeferred<ABRTruck>(TruckClass, Spawn, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Truck)
	{
		return;
	}
	const FLinearColor Paints[] = { {0.8f, 0.8f, 0.75f}, {0.2f, 0.35f, 0.6f}, {0.35f, 0.5f, 0.2f}, {0.6f, 0.55f, 0.1f}, {0.3f, 0.3f, 0.3f} };
	if (TruckClass == ABRTruck::StaticClass())
	{
		Truck->PaintColor = Paints[FMath::RandRange(0, static_cast<int32>(UE_ARRAY_COUNT(Paints)) - 1)];
	}
	Truck->bRequiresKey = true;	// players can hide in the back but can't steal it
	Truck->bInfiniteFuel = true;
	Truck->FinishSpawning(Spawn);

	Truck->StartNPCRoute(Route, CheckpointIndex, RouteGate);

	// Look-alike NPCs (driver first). Empty seats are where Runners hide.
	const int32 Count = FMath::Clamp(FMath::RandRange(NPCsPerTruck.X, NPCsPerTruck.Y), 1, 4);
	for (int32 i = 0; i < Count; ++i)
	{
		if (ABRCharacter* NPC = SpawnNPC(FTransform(Spawn.GetLocation() + FVector(0.f, 0.f, 300.f + i * 220.f))))
		{
			NPC->bIllegal = FMath::FRand() < IllegalNPCChance;
			NPC->bCarriesContraband = FMath::FRand() < ContrabandNPCChance;
			Truck->AddNPC(NPC);
		}
	}
	// Only ONE NPC truck plays music at a time (the others are quiet).
	bool bSomeoneBlasting = false;
	for (TActorIterator<ABRTruck> It(GetWorld()); It; ++It)
	{
		bSomeoneBlasting |= (*It != Truck && It->IsNPCTruck() && It->IsRadioOn());
	}
	if (!bSomeoneBlasting && Truck->GetStationCount() > 0)
	{
		Truck->SetRadioStation(FMath::RandRange(0, Truck->GetStationCount() - 1));
	}
}

UClass* ABRGameMode::GetNPCClass() const
{
	UClass* NPCClass = NPCPassengerClass.Get();
	// Default setting: use your Runner Blueprint so NPCs have your mesh, outfits and animations.
	if ((!NPCClass || NPCClass == ABRCharacter::StaticClass()) && PirateCharacterClass && PirateCharacterClass->IsChildOf(ABRCharacter::StaticClass()))
	{
		NPCClass = PirateCharacterClass.Get();
	}
	return NPCClass && NPCClass->IsChildOf(ABRCharacter::StaticClass()) ? NPCClass : ABRCharacter::StaticClass();
}

ABRCharacter* ABRGameMode::SpawnNPC(const FTransform& Where)
{
	ABRCharacter* NPC = GetWorld()->SpawnActorDeferred<ABRCharacter>(GetNPCClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!NPC)
	{
		return nullptr;
	}
	NPC->AutoPossessAI = EAutoPossessAI::Disabled;
	NPC->InitAsNPC(NPCSkinToneMin, NPCSkinToneMax);
	NPC->FinishSpawning(Where);
	if (FMath::FRand() < NPCPonchoChance)
	{
		NPC->SetNPCPoncho(true); // locals wear ponchos too, so a poncho'd Runner blends in
	}
	return NPC;
}

void ABRGameMode::SpawnVillagers()
{
	if (VillagerCount <= 0)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Orange, TEXT("No villagers: BP_BRGameMode -> Villager Count is 0."));
		}
		return;
	}
	// Your Villager Spots, or the built-in town.
	TArray<FVector> Spots;
	BRMap::GetVillagerSpots(GetWorld(), Spots);
	if (Spots.Num() == 0 && Arena && Arena->bBuildTestMap)
	{
		Arena->GetVillageSpots(Spots);
	}
	if (Spots.Num() == 0 && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Orange, TEXT("No villagers: this map has no BR Villager Spot. Place some in your town (Place Actors -> search 'BR Villager Spot')."));
	}
	if (Spots.Num() == 0)
	{
		return;
	}
	for (int32 i = 0; i < VillagerCount; ++i)
	{
		const FVector Home = Spots[i % Spots.Num()];
		const FVector At = Home + FVector(FMath::FRandRange(-200.f, 200.f), FMath::FRandRange(-200.f, 200.f), 0.f);
		if (ABRCharacter* NPC = SpawnNPC(FTransform(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), At)))
		{
			NPC->StartWandering(Home, VillagerWanderRadius);
		}
	}
}

void ABRGameMode::AwardPoints(APVVCharacter* Who, int32 Amount)
{
	ABRGameState* GS = GetBRGameState();
	ABRPlayerState* PS = Who ? Cast<ABRPlayerState>(Who->GetPVVPlayerState()) : nullptr;
	if (!GS || !PS || Amount == 0)
	{
		return;
	}
	PS->Points += Amount;
	if (Amount > 0)
	{
		ABRGameState::FX(this, EBRFX::PointsText, Who->GetActorLocation() + FVector(0.f, 0.f, 120.f), FString::Printf(TEXT("+%d"), Amount));
	}
	if (PS->GetFaction() == BR::Runners)
	{
		GS->RunnerScore = FMath::Max(0, GS->RunnerScore + Amount);
	}
	else if (PS->GetFaction() == BR::Wardens)
	{
		GS->WardenScore = FMath::Max(0, GS->WardenScore + Amount);
	}
}

void ABRGameMode::NotifyKnockedDown(APVVCharacter* Victim, AActor* Cause)
{
	Super::NotifyKnockedDown(Victim, Cause);
	if (ABRCharacter* Hatted = Cast<ABRCharacter>(Victim))
	{
		if (Hatted->GetHat() != EBRHat::None && FMath::FRand() < Hatted->GetHatKnockOffChance())
		{
			Hatted->KnockOffHat(); // it flies off; anyone can grab it
		}
	}
	APVVCharacter* By = Cast<APVVCharacter>(Cause);
	if (!By || By == Victim)
	{
		return;
	}
	if (ABRPlayerState* PS = Cast<ABRPlayerState>(By->GetPVVPlayerState()))
	{
		++PS->StatKnockdowns;
	}
	static const TCHAR* Verbs[] = { TEXT("FLATTENED"), TEXT("BONKED"), TEXT("BODY-SLAMMED"), TEXT("SENT FLYING"), TEXT("DROPPED"), TEXT("CLOTHESLINED") };
	Feed(FString::Printf(TEXT("%s %s %s"), *NameOf(By), Verbs[FMath::RandRange(0, static_cast<int32>(UE_ARRAY_COUNT(Verbs)) - 1)], *NameOf(Victim)));
}

void ABRGameMode::Feed(const FString& Line)
{
	if (ABRGameState* GS = GetBRGameState())
	{
		GS->Multicast_Feed(Line);
	}
}

void ABRGameMode::HireClient(ABRCharacter* Runner, const FVector& At, int32 Pay)
{
	if (!Runner || !Runner->IsRunner() || Runner->IsNPC())
	{
		return;
	}
	int32 Current = 0;
	for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
	{
		Current += It->Leader.Get() == Runner ? 1 : 0;
	}
	if (Current >= MaxClientsPerRunner)
	{
		Runner->Notify(FText::Format(LOCTEXT("TooManyClients", "You can only lead {0} clients at once."), MaxClientsPerRunner));
		return;
	}
	if (ABRCharacter* Client = SpawnNPC(FTransform(At + FVector(0.f, 0.f, 100.f))))
	{
		Client->StartFollowing(Runner, Pay > 0 ? Pay : 250);
		Runner->Notify(FText::Format(LOCTEXT("ClientHired", "New client! They follow you. Get them to the Safe Zone for ${0}. ({1}/{2})"),
			Client->ClientPay, Current + 1, MaxClientsPerRunner));
	}
}

void ABRGameMode::TickClients()
{
	// Smuggling clients who reach the Safe Zone pay their Runner.
	TArray<ABRCharacter*> Delivered;
	for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
	{
		ABRCharacter* Client = *It;
		if (Client->IsNPC() && Client->Leader.IsValid() && Client->GetCondition() == EPVVCondition::Normal && !Client->GetCarrier()
			&& BRMap::IsInSafeZone(GetWorld(), Client->GetActorLocation()))
		{
			Delivered.Add(Client);
		}
	}
	for (ABRCharacter* Client : Delivered)
	{
		ABRCharacter* Runner = Client->Leader.Get();
		ABRPlayerState* PS = Runner ? Runner->GetBRPlayerState() : nullptr;
		if (PS)
		{
			PS->AddMoney(Client->ClientPay);
			ABRGameState::Sfx(this, EBRSfx::Payday, Runner->GetActorLocation());
			ABRGameState::FX(this, EBRFX::MoneyText, Runner->GetActorLocation() + FVector(0.f, 0.f, 60.f), FString::Printf(TEXT("+$%d"), Client->ClientPay));
			++PS->StatSmuggled;
			AwardPoints(Runner, PointsPerClient);
			Runner->Notify(FText::Format(LOCTEXT("ClientPaid", "Client delivered! +${0}"), Client->ClientPay));
			Feed(FString::Printf(TEXT("%s smuggled a client across (+$%d)"), *NameOf(Runner), Client->ClientPay));
		}
		if (Client->GetCurrentBoat())
		{
			Client->GetCurrentBoat()->Unseat(Client);
		}
		Client->Destroy();
	}
}

void ABRGameMode::OnContrabandSeized(APVVCharacter* Warden, int32 Count)
{
	if (Count <= 0)
	{
		return;
	}
	AwardPoints(Warden, PointsPerSeizure * Count);
	ChangeApproval(GoodCatchBonus);
	Feed(FString::Printf(TEXT("%s seized %d contraband"), *NameOf(Warden), Count));
}

void ABRGameMode::OnTruckCleared(APVVCharacter* Warden)
{
	AwardPoints(Warden, PointsPerClearedTruck);
	if (ABRPlayerState* PS = Warden ? Cast<ABRPlayerState>(Warden->GetPVVPlayerState()) : nullptr)
	{
		++PS->StatClearedTrucks;
	}
	AddBudget(BudgetPerClearedTruck);
	Feed(FString::Printf(TEXT("%s cleared a truck at the checkpoint (budget +$%d)"), *NameOf(Warden), BudgetPerClearedTruck));
}

ABRCharacter* ABRGameMode::SpawnLuchador(const FTransform& Where, UClass* LuchadorClass)
{
	UClass* Type = LuchadorClass ? LuchadorClass : GetNPCClass();
	ABRCharacter* Fighter = GetWorld()->SpawnActorDeferred<ABRCharacter>(Type, Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Fighter)
	{
		return nullptr;
	}
	Fighter->AutoPossessAI = EAutoPossessAI::Disabled;
	Fighter->InitAsNPC(NPCSkinToneMin, NPCSkinToneMax);
	Fighter->FinishSpawning(Where);
	return Fighter;
}

void ABRGameMode::TickLobby(ABRGameState* GS)
{
	int32 People = 0, Ready = 0;
	for (APlayerState* Each : GS->PlayerArray)
	{
		const ABRPlayerState* PS = Cast<ABRPlayerState>(Each);
		if (PS && !PS->IsABot() && PS->GetFaction() != EPVVFaction::None)
		{
			++People;
			Ready += PS->bReady ? 1 : 0;
		}
	}
	if (People == 0 || Ready < People)
	{
		GS->LobbyCountdown = 0;
		return;
	}
	if (GS->LobbyCountdown == 0)
	{
		GS->LobbyCountdown = 5;
		Announce(LOCTEXT("AllReady", "Everyone's ready! Starting in 5..."));
		return;
	}
	ABRGameState::Sfx(this, EBRSfx::Countdown, FVector::ZeroVector, true);
	if (--GS->LobbyCountdown <= 0)
	{
		GS->bLobby = false;
		GS->PhaseSecondsLeft = PrepSeconds;
		Announce(LOCTEXT("PrepStart", "PREP! Work, shop and plan. The Line opens when the timer runs out."));
	}
}

void ABRGameMode::ApplyHostSettings(const FBRHostSettings& Settings)
{
	// Changing a time during that phase changes the clock right away (by the difference).
	if (ABRGameState* ClockGS = GetBRGameState(); ClockGS && !ClockGS->bLobby && ClockGS->Phase != EBRPhase::Ended)
	{
		const int32 OldLength = ClockGS->Phase == EBRPhase::Prep ? PrepSeconds : InvasionSeconds;
		const int32 NewLength = FMath::Clamp(ClockGS->Phase == EBRPhase::Prep ? Settings.PrepSeconds : Settings.InvasionSeconds, 30, 3600);
		ClockGS->PhaseSecondsLeft = FMath::Max(10, ClockGS->PhaseSecondsLeft + NewLength - OldLength);
	}
	PrepSeconds = FMath::Clamp(Settings.PrepSeconds, 30, 1800);
	InvasionSeconds = FMath::Clamp(Settings.InvasionSeconds, 60, 3600);
	bUseLobby = Settings.bLobby;
	bBestOf3 = Settings.bBestOf3;
	const bool bHadBots = bFillWithBots;
	bFillWithBots = Settings.bFillBots;
	if (bHadBots && !bFillWithBots)
	{
		RemoveBots();
	}
	FillRunnersTo = FMath::Clamp(Settings.BotRunners, 0, 12);
	FillWardensTo = FMath::Clamp(Settings.BotWardens, 0, 12);
	if (ABRGameState* GS = GetBRGameState())
	{
		GS->bBestOf3 = bBestOf3;
		if (GS->bLobby && !bUseLobby)
		{
			GS->bLobby = false;
			GS->PhaseSecondsLeft = PrepSeconds;
		}
	}
}

void ABRGameMode::GetHostSettings(FBRHostSettings& Out) const
{
	Out.PrepSeconds = PrepSeconds;
	Out.InvasionSeconds = InvasionSeconds;
	Out.bLobby = bUseLobby;
	Out.bBestOf3 = bBestOf3;
	Out.bFillBots = bFillWithBots;
	Out.BotRunners = FillRunnersTo;
	Out.BotWardens = FillWardensTo;
}

void ABRGameMode::AddBot(EPVVFaction Team)
{
	ABRGameState* GS = GetBRGameState();
	if (!GS)
	{
		return;
	}
	if (Team == EPVVFaction::None)
	{
		Team = GS->CountPlayers(BR::Wardens) * 2 < GS->CountPlayers(BR::Runners) ? BR::Wardens : BR::Runners;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABRBotController* Bot = GetWorld()->SpawnActor<ABRBotController>(ABRBotController::StaticClass(), FTransform::Identity, Params);
	ABRPlayerState* PS = Bot ? Bot->GetPlayerState<ABRPlayerState>() : nullptr;
	if (!PS)
	{
		if (Bot)
		{
			Bot->Destroy();
		}
		return;
	}
	static const TCHAR* RunnerNames[] = { TEXT("Pancho"), TEXT("Lupe"), TEXT("Chuy"), TEXT("Memo"), TEXT("Rosa"), TEXT("Nacho"), TEXT("Paco"), TEXT("Beto") };
	static const TCHAR* WardenNames[] = { TEXT("Officer Dale"), TEXT("Officer Randy"), TEXT("Officer Earl"), TEXT("Officer Becky"), TEXT("Officer Hank"), TEXT("Officer Tammy") };
	const int32 Index = BotNameIndex++;
	const FString Name = Team == BR::Wardens
		? WardenNames[Index % static_cast<int32>(UE_ARRAY_COUNT(WardenNames))]
		: RunnerNames[Index % static_cast<int32>(UE_ARRAY_COUNT(RunnerNames))];
	PS->SetPlayerName(FString::Printf(TEXT("[BOT] %s"), *Name));
	PS->SetIsABot(true);
	PS->SetFaction(Team);
	// Random look from the full range (same options as people get).
	PS->Look.SkinTone = FMath::FRand();
	PS->Look.ShirtColor = FMath::FRand();
	PS->Look.PantsColor = FMath::FRand();
	PS->Look.bHat = true;
	PS->Look.HeadSize = FMath::FRandRange(.4f, .6f);
	PS->Look.NoseSize = FMath::FRandRange(.35f, .65f);
	PS->Look.LipSize = FMath::FRandRange(.4f, .6f);
	PS->Look.EarSize = FMath::FRandRange(.4f, .6f);
	PS->Look.HairStyle = FMath::RandRange(0, 8);
	PS->Look.HairColor = FMath::RandBool() ? 0.f : FMath::FRandRange(.05f, .35f);
	PS->Look.FacialHair = FMath::RandRange(0, 4);
	// Spawn the body ourselves (step by step, logged) instead of the engine's RestartPlayer.
	UE_LOG(LogTemp, Warning, TEXT("[Bots] %s: spawning body"), *PS->GetPlayerName());
	const AActor* Start = ChooseStartForFaction(Team);
	UClass* BodyClass = GetDefaultPawnClassForController(Bot);
	if (!Start || !BodyClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Bots] no player start or character class for that team - bot removed"));
		Bot->Destroy();
		return;
	}
	const FVector Spread(FMath::FRandRange(-250.f, 250.f), FMath::FRandRange(-250.f, 250.f), 0.f);
	const FTransform SpawnAt(FRotator(0.f, Start->GetActorRotation().Yaw, 0.f), Start->GetActorLocation() + Spread);
	FActorSpawnParameters BodyParams;
	BodyParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	APawn* Body = GetWorld()->SpawnActor<APawn>(BodyClass, SpawnAt, BodyParams);
	if (!Body)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Bots] body failed to spawn - bot removed"));
		Bot->Destroy();
		return;
	}
	if (AController* Old = Body->GetController())
	{
		Old->UnPossess(); // the Blueprint's own auto-AI, if it has one
		Old->Destroy();
	}
	Bot->Possess(Body);
	UE_LOG(LogTemp, Warning, TEXT("[Bots] %s possessed"), *PS->GetPlayerName());
	// Apply the look a moment later, the same way a real player's look arrives.
	TWeakObjectPtr<ABRPlayerState> WeakPS = PS;
	TWeakObjectPtr<APawn> WeakBody = Body;
	FTimerHandle LookTimer;
	GetWorldTimerManager().SetTimer(LookTimer, FTimerDelegate::CreateWeakLambda(this, [WeakPS, WeakBody]()
	{
		ABRPlayerState* LookPS = WeakPS.Get();
		ABRCharacter* LookBody = Cast<ABRCharacter>(WeakBody.Get());
		if (LookPS && LookBody)
		{
			LookPS->bHasLook = true;
			LookBody->SetLook(LookPS->Look);
		}
	}), 2.f, false);
	Feed(FString::Printf(TEXT("%s joined the %s"), *PS->GetPlayerName(), *(Team == BR::Wardens ? BR::WardenName() : BR::RunnerName())));
}

void ABRGameMode::RemoveBots()
{
	TArray<ABRBotController*> Bots;
	for (TActorIterator<ABRBotController> It(GetWorld()); It; ++It)
	{
		Bots.Add(*It);
	}
	for (ABRBotController* Bot : Bots)
	{
		if (APawn* P = Bot->GetPawn())
		{
			Bot->UnPossess();
			P->Destroy();
		}
		Bot->Destroy();
	}
}

void ABRGameMode::TickBots()
{
	BotClock += 1.f;
	if (!bFillWithBots || BotClock < 5.f)
	{
		return;
	}
	BotClock = 0.f;
	ABRGameState* GS = GetBRGameState();
	if (!GS)
	{
		return;
	}
	for (const EPVVFaction Team : { BR::Runners, BR::Wardens })
	{
		const int32 Want = Team == BR::Runners ? FillRunnersTo : FillWardensTo;
		const int32 Have = GS->CountPlayers(Team);
		if (Have < Want)
		{
			AddBot(Team); // one at a time
		}
		else if (Have > Want)
		{
			// Too many: a bot on this team makes room for the people.
			for (TActorIterator<ABRBotController> It(GetWorld()); It; ++It)
			{
				const ABRPlayerState* PS = It->GetPlayerState<ABRPlayerState>();
				if (PS && PS->GetFaction() == Team)
				{
					if (APawn* P = It->GetPawn())
					{
						It->UnPossess();
						P->Destroy();
					}
					It->Destroy();
					break;
				}
			}
		}
	}
}

UClass* ABRGameMode::GetDogClass() const
{
	return DogClass.Get();
}

bool ABRGameMode::SpendBudget(int32 Amount)
{
	ABRGameState* GS = GetBRGameState();
	if (!GS || Amount < 0 || GS->WardenBudget < Amount)
	{
		return false;
	}
	GS->WardenBudget -= Amount;
	return true;
}

void ABRGameMode::AddBudget(int32 Amount)
{
	if (ABRGameState* GS = GetBRGameState())
	{
		GS->WardenBudget = FMath::Max(0, GS->WardenBudget + Amount);
	}
}

void ABRGameMode::OnGoodCatch(APVVCharacter* Warden, ABRPlayerState* Caught)
{
	AddBudget(BudgetPerCatch);
	if (Warden)
	{
		ABRGameState::Sfx(this, EBRSfx::Whistle, Warden->GetActorLocation());
	}
	ABRGameState* GS = GetBRGameState();
	if (!GS || !Caught || GS->BountyTarget != Caught)
	{
		return;
	}
	AwardPoints(Warden, BountyPoints);
	AddBudget(BountyBudget);
	GS->BountyTarget = nullptr;
	BountyClock = BountyCheckSeconds - 5.f; // a new bounty soon
	Feed(FString::Printf(TEXT("%s CLAIMED THE BOUNTY on %s (+%d, budget +$%d)"), *NameOf(Warden), *Caught->GetPlayerName(), BountyPoints, BountyBudget));
}

void ABRGameMode::TickBounty(ABRGameState* GS)
{
	BountyClock += 1.f;
	if (BountyClock < BountyCheckSeconds)
	{
		return;
	}
	BountyClock = 0.f;
	ABRPlayerState* Top = nullptr;
	for (APlayerState* Each : GS->PlayerArray)
	{
		ABRPlayerState* PS = Cast<ABRPlayerState>(Each);
		if (PS && PS->GetFaction() == BR::Runners && PS->Points >= BountyMinPoints && (!Top || PS->Points > Top->Points))
		{
			Top = PS;
		}
	}
	if (Top && Top != GS->BountyTarget)
	{
		GS->BountyTarget = Top;
		ABRGameState::Sfx(this, EBRSfx::Bounty, FVector::ZeroVector, true);
		Announce(FText::Format(LOCTEXT("Bounty", "WANTED: {0}! Border Patrol gets +{1} for catching them."), FText::FromString(Top->GetPlayerName()), BountyPoints));
		Feed(FString::Printf(TEXT("BOUNTY on %s ($ above their head)"), *Top->GetPlayerName()));
	}
}

void ABRGameMode::OnTruckForcedThrough()
{
	ChangeApproval(-HarassmentPenalty);
	Announce(LOCTEXT("TrafficJam", "Nobody checked the trucks! Traffic pushed through the checkpoint. Approval down."));
}

void ABRGameMode::AssignJob(ABRPlayerState* PS)
{
	TArray<FString> Jobs;
	for (TActorIterator<ABRStation> It(GetWorld()); It; ++It)
	{
		if (It->IsAssignableJob() && !It->SignText.IsEmpty())
		{
			Jobs.AddUnique(It->SignText);
		}
	}
	if (Jobs.Num() == 0 || !PS)
	{
		return; // no paid jobs on this map (yet)
	}
	PS->AssignedJob = FMath::FRand() < UnemployedChance ? FString(TEXT("-")) : Jobs[FMath::RandRange(0, Jobs.Num() - 1)];
	if (ABRCharacter* BRC = Cast<ABRCharacter>(PS->GetPawn()))
	{
		BRC->Notify(PS->AssignedJob == TEXT("-")
			? LOCTEXT("JobNone", "You're unemployed. Rummage through trash for scrap, sell it, hustle.")
			: FText::Format(LOCTEXT("JobGot", "Your job in town: {0}"), FText::FromString(PS->AssignedJob)));
	}
}

void ABRGameMode::RespawnRunner(TWeakObjectPtr<APVVCharacter> Runner)
{
	APVVCharacter* C = Runner.Get();
	ABRPlayerState* PS = C ? Cast<ABRPlayerState>(C->GetPVVPlayerState()) : nullptr;
	if (!PS || C->GetCondition() == EPVVCondition::Detained)
	{
		return;
	}
	if (C->GetFaction() == BR::Runners)
	{
		if (!PS->bReachedSafety)
		{
			PS->RemoveIllegalItems();
		}
		PS->bHasCrossedLine = false;
		PS->bIsAggressor = false;
		PS->bReachedSafety = false;
	}
	RespawnCharacter(C);
}

void ABRGameMode::ChangeApproval(float Delta)
{
	ABRGameState* GS = GetBRGameState();
	if (!GS)
	{
		return;
	}
	GS->Approval = FMath::Clamp(GS->Approval + Delta, 0.f, 100.f);
	if (!GS->DronesAllowed() && !bAnnouncedDroneLoss)
	{
		bAnnouncedDroneLoss = true;
		Announce(LOCTEXT("Defund", "BREAKING: Warden drone program DEFUNDED after public outrage!"));
	}
	if (!GS->LethalAllowed() && !bAnnouncedGunLoss)
	{
		bAnnouncedGunLoss = true;
		Announce(LOCTEXT("Guns", "BREAKING: Wardens' guns CONFISCATED. Tasers only!"));
	}
}

#undef LOCTEXT_NAMESPACE
