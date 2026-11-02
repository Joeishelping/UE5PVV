#include "BRGameMode.h"
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
	const bool bCustomMap = BRMap::IsCustomMap(World);
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
		NextNPCTruck = 3.f;
		Announce(FText::Format(LOCTEXT("Invasion", "THE LINE IS OPEN! {1}: get {0} of you to the Safe Zone. {2}: catch, don't kill."), GS->RunnersNeeded,
			FText::FromString(BR::RunnerName()), FText::FromString(BR::WardenName())));
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

	if (NPC && NPC->IsNPC())
	{
		ChangeApproval(-HarassmentPenalty);
		Announce(FText::Format(LOCTEXT("TouristYeet", "{0} catapulted an innocent citizen! Approval -{1}"),
			FText::FromString(NameOf(Warden)), FMath::RoundToInt(HarassmentPenalty)));
	}
	else
	{
		if (Warden)
		{
			if (PS->bHasCrossedLine)
			{
				ChangeApproval(GoodCatchBonus);
			}
			else
			{
				ChangeApproval(-WrongfulArrestPenalty);
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
	Announce(Warden
		? FText::Format(LOCTEXT("CatapultBy", "{1} sent {0} flying back across the border!"), FText::FromString(NameOf(Prisoner)), FText::FromString(NameOf(Warden)))
		: FText::Format(LOCTEXT("Catapult", "{0} has been RETURNED TO SENDER via catapult!"), FText::FromString(NameOf(Prisoner))));
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
			Truck->AddNPC(NPC);
		}
	}
	if (Truck->GetStationCount() > 0)
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
