#include "BRGameMode.h"
#include "BRCar.h"
#include "BRCharacter.h"
#include "BRDesertArena.h"
#include "BRDog.h"
#include "BRGameState.h"
#include "BRHUD.h"
#include "BRPlayerController.h"
#include "BRPlayerState.h"
#include "BRWorldActors.h"
#include "PVVLake.h"
#include "Engine/World.h"
#include "EngineUtils.h"
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
	if (!TActorIterator<APVVLake>(GetWorld()))
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		UClass* MapClass = ArenaClass ? ArenaClass.Get() : ABRDesertArena::StaticClass();
		Arena = GetWorld()->SpawnActor<ABRDesertArena>(MapClass, FTransform(ArenaLocation), Params);
		if (Arena)
		{
			Arena->SpawnGameplayActors(RunnerTruckClass, WardenTruckClass, DogClass);
		}
		RelocateExistingPlayers();
	}
	else
	{
		for (TActorIterator<ABRDesertArena> It(GetWorld()); It; ++It)
		{
			Arena = *It;
			break;
		}
	}

	if (ABRGameState* GS = GetBRGameState())
	{
		GS->Phase = EBRPhase::Prep;
		GS->PhaseSecondsLeft = PrepSeconds;
		GS->TimeOfDay = StartHour;
		GS->Approval = 100.f;
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

	GS->PhaseSecondsLeft = FMath::Max(0, GS->PhaseSecondsLeft - 1);
	if (GS->PhaseSecondsLeft == 60 && GS->Phase == EBRPhase::Prep)
	{
		Announce(LOCTEXT("OneMinute", "One minute until the Line opens!"));
	}
	TickRunners(GS);
	if (GS->Phase == EBRPhase::Invasion)
	{
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
	if (GS->Phase == EBRPhase::Prep)
	{
		GS->Phase = EBRPhase::Invasion;
		GS->PhaseSecondsLeft = InvasionSeconds;
		GS->RunnersNeeded = FMath::Max(1, FMath::DivideAndRoundUp(GS->CountPlayers(BR::Runners), 2));
		NextNPCTruck = 8.f;
		Announce(FText::Format(LOCTEXT("Invasion", "THE LINE IS OPEN! Runners: get {0} of you to the Safe Zone. Wardens: catch, don't kill."), GS->RunnersNeeded));
	}
	else if (GS->Phase == EBRPhase::Invasion)
	{
		GS->Phase = EBRPhase::Ended;
		EndMatch(BR::Wardens);
	}
}

void ABRGameMode::CheckWinConditions()
{
	ABRGameState* GS = GetBRGameState();
	if (GS && GS->Phase == EBRPhase::Invasion && GS->RunnersSafe >= GS->RunnersNeeded)
	{
		GS->Phase = EBRPhase::Ended;
		EndMatch(BR::Runners);
	}
}

void ABRGameMode::TickRunners(ABRGameState* GS)
{
	if (!Arena)
	{
		return;
	}
	TArray<APVVCharacter*> ToLaunch;
	for (TActorIterator<APVVCharacter> It(GetWorld()); It; ++It)
	{
		APVVCharacter* C = *It;
		ABRPlayerState* PS = Cast<ABRPlayerState>(C->GetPVVPlayerState());
		if (!PS || C->GetFaction() != BR::Runners)
		{
			continue;
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

		const float X = C->GetActorLocation().X;
		if (!PS->bHasCrossedLine && X > Arena->GetLineX())
		{
			PS->bHasCrossedLine = true;
			if (ABRCharacter* BRC = Cast<ABRCharacter>(C))
			{
				BRC->Notify(LOCTEXT("Crossed", "You crossed the Line. Wardens can now arrest you."));
			}
		}
		const bool bFree = C->GetCondition() == EPVVCondition::Normal && !C->GetCarrier();
		if (!PS->bReachedSafety && bFree && X > Arena->GetSafeX())
		{
			PS->bReachedSafety = true;
			++GS->RunnersSafe;
			Announce(FText::Format(LOCTEXT("Safe", "{0} MADE IT to the Safe Zone! ({1}/{2})"), FText::FromString(NameOf(C)), GS->RunnersSafe, GS->RunnersNeeded));
			CheckWinConditions();
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

	if (PS->bHasCrossedLine)
	{
		ChangeApproval(GoodCatchBonus);
	}
	else
	{
		ChangeApproval(-WrongfulArrestPenalty);
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
	ABRPlayerState* PS = Prisoner ? Cast<ABRPlayerState>(Prisoner->GetPVVPlayerState()) : nullptr;
	if (!PS || !Arena || !Arena->GetCatapult())
	{
		return;
	}
	PS->RemoveIllegalItems();
	PS->bHasCrossedLine = false;
	PS->bIsAggressor = false;
	PS->SentenceSeconds = 0;
	Arena->GetCatapult()->Launch(Prisoner, Arena->GetCatapultLanding());
	Announce(FText::Format(LOCTEXT("Catapult", "{0} has been RETURNED TO SENDER via catapult!"), FText::FromString(NameOf(Prisoner))));
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
	if (!Arena)
	{
		return;
	}
	int32 Alive = 0;
	for (TActorIterator<ABRTruck> It(GetWorld()); It; ++It)
	{
		Alive += It->IsNPCTruck() ? 1 : 0;
	}
	if (Alive >= MaxNPCTrucks)
	{
		return;
	}

	const FTransform Spawn = Arena->GetNPCSpawn();
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

	TArray<FVector> Route;
	int32 CheckpointIndex = INDEX_NONE;
	Arena->GetNPCRoute(Route, CheckpointIndex);
	Truck->StartNPCRoute(Route, CheckpointIndex, Arena->GetGate());

	// 1-4 look-alike NPCs (driver first). Empty seats are where Runners hide.
	const int32 Count = FMath::RandRange(1, 3);
	UClass* NPCClass = NPCPassengerClass ? NPCPassengerClass.Get() : ABRCharacter::StaticClass();
	for (int32 i = 0; i < Count; ++i)
	{
		const FTransform NPCSpawn(Spawn.GetLocation() + FVector(0.f, 0.f, 300.f + i * 220.f));
		ABRCharacter* NPC = GetWorld()->SpawnActorDeferred<ABRCharacter>(NPCClass, NPCSpawn, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!NPC)
		{
			continue;
		}
		NPC->InitAsNPC();
		NPC->FinishSpawning(NPCSpawn);
		Truck->AddNPC(NPC);
	}
	if (Truck->GetStationCount() > 0)
	{
		Truck->SetRadioStation(FMath::RandRange(0, Truck->GetStationCount() - 1));
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
		PS->RemoveIllegalItems();
		PS->bHasCrossedLine = false;
		PS->bIsAggressor = false;
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
