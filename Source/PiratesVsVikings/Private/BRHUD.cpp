#include "BRHUD.h"
#include "BRCantina.h"
#include "BRDesertArena.h"
#include "BRCar.h"
#include "BRCharacter.h"
#include "BRDrone.h"
#include "BRGameInstance.h"
#include "BRGameState.h"
#include "BRPlayerController.h"
#include "BRPlayerState.h"
#include "BRSettings.h"
#include "BRReplay.h"
#include "BRWorldActors.h"
#include "PVVDetentionZone.h"
#include "BRMapKit.h"
#include "BRPolice.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	/** What F does with the selected item (shown under the inventory so nobody has to guess). */
	FString ItemUseHint(EBRItem Item)
	{
		switch (Item)
		{
		case EBRItem::Taco: return TEXT("F: eat (fills FOOD)");
		case EBRItem::TacoPlatter: return TEXT("F: put the taco platter down (NPCs stop to eat)");
		case EBRItem::Trumpet: return TEXT("F: play the trumpet (tips / serenade)");
		case EBRItem::SmokeBomb: return TEXT("F: throw SMOKE BOMB (blocks sight: cameras, spotlights, agents)");
		case EBRItem::SecurityCamera: return TEXT("F: place SECURITY CAMERA here (spots trespassers). L: look through your cameras");
		case EBRItem::TNT: return TEXT("F: plant TNT (blows a hole in the Wall)");
		case EBRItem::FuseKit: return TEXT("F: use fuse kit (near electric fence box)");
		case EBRItem::WallReinforcement: return TEXT("Walk to the Wall and hold E to reinforce it");
		case EBRItem::GrappleHook: return TEXT("F: fire grappling hook where you look");
		case EBRItem::FakeUniform: return TEXT("F: put on / take off the fake Border Patrol uniform");
		case EBRItem::CardboardBox: return TEXT("F: hide in the cardboard box");
		case EBRItem::Lasso: return TEXT("F: throw lasso at who you're looking at");
		case EBRItem::Ladder: return TEXT("E at the Wall: lean the ladder on it");
		case EBRItem::WireCutters: return TEXT("Hold E at the razor wire: cut it");
		case EBRItem::GasCan: return TEXT("E at a truck: fill it up");
		case EBRItem::Shovel: return TEXT("Hold E at a TUNNEL sign / tunnel end: dig");
		case EBRItem::CannonSuit: return TEXT("E at the Human Cannon: climb in");
		case EBRItem::Lockpick: return TEXT("Hold E on a locked door / holding pen");
		case EBRItem::FakePassport: return TEXT("Passive: scans say you're legal (unless carrying contraband)");
		case EBRItem::Knife: return TEXT("Left click: stab");
		case EBRItem::Drugs: case EBRItem::IllegalGun: case EBRItem::Package: return TEXT("Contraband: get it to the SAFE ZONE");
		default: return FString();
		}
	}

	FString Clock(int32 Seconds) { return FString::Printf(TEXT("%d:%02d"), Seconds / 60, Seconds % 60); }
	FString TimeOfDayText(float Hours)
	{
		const int32 H = FMath::FloorToInt(Hours) % 24;
		const int32 M = FMath::FloorToInt(FMath::Frac(Hours) * 60.f);
		return FString::Printf(TEXT("%02d:%02d"), H, M);
	}
}

void ABRHUD::Centered(const FString& Text, float Y, const FLinearColor& Color, UFont* Font, float Scale)
{
	float W = 0.f, H = 0.f;
	GetTextSize(Text, W, H, Font, Scale);
	DrawText(Text, Color, (Canvas->ClipX - W) * 0.5f, Y, Font, Scale);
}

void ABRHUD::Bar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Color)
{
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), X, Y, W, H);
	DrawRect(Color, X + 2.f, Y + 2.f, (W - 4.f) * FMath::Clamp(Fraction, 0.f, 1.f), H - 4.f);
}

void ABRHUD::Marker(const FVector& World, const FString& Text, const FLinearColor& Color)
{
	FVector Screen = Project(World);
	float W = 0.f, H = 0.f;
	GetTextSize(Text, W, H, GEngine->GetMediumFont());
	if (Screen.Z <= 0.f)
	{
		// Behind you: pin it to the bottom edge on the side it's on.
		Screen.X = Canvas->ClipX - Screen.X;
		Screen.Y = Canvas->ClipY - 60.f;
	}
	// Keep it on screen so you can always see which way to go.
	Screen.X = FMath::Clamp(Screen.X, W * 0.5f + 10.f, Canvas->ClipX - W * 0.5f - 10.f);
	Screen.Y = FMath::Clamp(Screen.Y, H + 90.f, Canvas->ClipY - 50.f);
	DrawText(Text, Color, Screen.X - W * 0.5f, Screen.Y - H, GEngine->GetMediumFont());
}

void ABRHUD::BeginPlay()
{
	Super::BeginPlay();
	// Play of the Round recorder (this screen only).
	if (GetNetMode() != NM_DedicatedServer)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Replay = GetWorld()->SpawnActor<ABRReplayDirector>(ABRReplayDirector::StaticClass(), FTransform::Identity, Params);
	}
}

void ABRHUD::DrawHUD()
{
	Super::DrawHUD();
	ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>();
	ABRPlayerController* PC = Cast<ABRPlayerController>(GetOwningPlayerController());
	if (!Canvas || !GEngine || !GS || !PC)
	{
		return;
	}

	UFont* Big = GEngine->GetLargeFont();
	UFont* Small = GEngine->GetSmallFont();
	const float SX = Canvas->ClipX, SY = Canvas->ClipY;
	const FLinearColor White = FLinearColor::White, Yellow(1.f, 0.85f, 0.2f), Red(1.f, 0.25f, 0.2f), Green(0.3f, 1.f, 0.4f);
	const FLinearColor RunnerCol = PVV::FactionColor(BR::Runners), WardenCol = PVV::FactionColor(BR::Wardens);
	const float Now = GetWorld()->GetTimeSeconds();

	ABRPlayerState* PS = PC->GetPlayerState<ABRPlayerState>();
	const bool bWarden = PS && PS->GetFaction() == BR::Wardens;

	// --- Top: phase, clock, world
	FString PhaseText;
	switch (GS->Phase)
	{
	case EBRPhase::Prep: PhaseText = FString::Printf(TEXT("PREP  -  Line opens in %s"), *Clock(GS->PhaseSecondsLeft)); break;
	case EBRPhase::Invasion: PhaseText = FString::Printf(TEXT("INVASION  -  %s left   |   %s %d  -  %d %s"), *Clock(GS->PhaseSecondsLeft),
		*BR::RunnerName().ToUpper(), GS->RunnerScore, GS->WardenScore, *BR::WardenName().ToUpper()); break;
	default: PhaseText = FString::Printf(TEXT("FINAL   %s %d  -  %d %s"), *BR::RunnerName().ToUpper(), GS->RunnerScore, GS->WardenScore, *BR::WardenName().ToUpper()); break;
	}
	if (GS->bLobby)
	{
		PhaseText = GS->LobbyCountdown > 0 ? FString::Printf(TEXT("LOBBY  -  starting in %d..."), GS->LobbyCountdown)
			: FString(TEXT("LOBBY  -  press Y when you're ready"));
	}
	if (GS->bBestOf3)
	{
		PhaseText += FString::Printf(TEXT("    [BEST OF 3: %d - %d]"), GS->SeriesRunnerWins, GS->SeriesWardenWins);
	}
	Centered(PhaseText, 10.f, GS->Phase == EBRPhase::Invasion ? Red : White, Big, 1.4f);

	// --- Lobby: who's ready
	if (GS->bLobby)
	{
		float LY = SY * 0.3f;
		Centered(TEXT("WHO'S READY  (Y = ready,  M = menu: switch team / customize)"), LY, Yellow, Small, 1.3f);
		LY += 26.f;
		for (APlayerState* Each : GS->PlayerArray)
		{
			const ABRPlayerState* Other = Cast<ABRPlayerState>(Each);
			if (!Other || Other->GetFaction() == EPVVFaction::None)
			{
				continue;
			}
			const bool bOtherWarden = Other->GetFaction() == BR::Wardens;
			const FString Line = FString::Printf(TEXT("%s  -  %s  -  %s"), *Other->GetPlayerName(),
				*(bOtherWarden ? GetDefault<UBRSettings>()->WardenSingular : GetDefault<UBRSettings>()->RunnerSingular), (Other->IsABot() || Other->bReady) ? TEXT("READY") : TEXT("not ready"));
			Centered(Line, LY, (Other->IsABot() || Other->bReady) ? Green : (bOtherWarden ? WardenCol : RunnerCol), Small, 1.2f);
			LY += 20.f;
		}
	}
	const TCHAR* WeatherText = GS->Weather == EBRWeather::Rain ? TEXT("Rain") : GS->Weather == EBRWeather::Sandstorm ? TEXT("SANDSTORM") : TEXT("Clear");
	Centered(FString::Printf(TEXT("DAY %d   |   %s  %s   |   %s   |   Warden approval %d%%"), GS->DayNumber, *TimeOfDayText(GS->TimeOfDay), GS->IsNight() ? TEXT("(night)") : TEXT(""),
		WeatherText, FMath::RoundToInt(GS->Approval)), 46.f, White, Small, 1.2f);
	Bar(SX * 0.5f - 150.f, 66.f, 300.f, 10.f, GS->Approval / 100.f, GS->Approval >= ABRGameState::DroneApproval ? Green : (GS->Approval >= ABRGameState::LethalApproval ? Yellow : Red));

	// --- Play of the Round (cinema bars + caption, nothing else)
	if (Replay && Replay->IsPlaying())
	{
		DrawRect(FLinearColor::Black, 0.f, 0.f, SX, SY * 0.12f);
		DrawRect(FLinearColor::Black, 0.f, SY * 0.88f, SX, SY * 0.12f);
		Centered(TEXT("PLAY OF THE ROUND"), SY * 0.03f, Yellow, Big, 2.2f);
		Centered(Replay->GetCaption(), SY * 0.91f, White, Big, 1.5f);
		return;
	}
	if (GS->Phase == EBRPhase::Ended)
	{
		const FString WinText = GS->Winner == BR::Runners ? BR::RunnerName().ToUpper() + TEXT(" WIN!")
			: (GS->Winner == BR::Wardens ? BR::WardenName().ToUpper() + TEXT(" WIN!") : FString(TEXT("IT'S A DRAW!")));
		Centered(WinText, SY * 0.2f, GS->Winner == BR::Runners ? RunnerCol : WardenCol, Big, 4.f);

		// --- Award screen
		struct FAward { const TCHAR* Title; int32 ABRPlayerState::* Stat; const TCHAR* Unit; };
		static const FAward Awards[] = {
			{ TEXT("MVP"), &ABRPlayerState::Points, TEXT("points") },
			{ TEXT("Border Hopper"), &ABRPlayerState::StatCrossings, TEXT("crossings") },
			{ TEXT("Coyote of the Year"), &ABRPlayerState::StatSmuggled, TEXT("clients smuggled") },
			{ TEXT("Narco Legend"), &ABRPlayerState::StatContraband, TEXT("contraband delivered") },
			{ TEXT("Top Cop"), &ABRPlayerState::StatCatches, TEXT("catches") },
			{ TEXT("Paper Pusher"), &ABRPlayerState::StatClearedTrucks, TEXT("trucks cleared") },
			{ TEXT("Wrongful Arrest King"), &ABRPlayerState::StatWrongful, TEXT("wrongful arrests") },
			{ TEXT("Most Catapulted"), &ABRPlayerState::StatCatapulted, TEXT("flights home") },
			{ TEXT("Human Cannonball"), &ABRPlayerState::StatCannonShots, TEXT("cannon shots") },
			{ TEXT("Body Slammer"), &ABRPlayerState::StatKnockdowns, TEXT("knockdowns") },
			{ TEXT("Linebacker"), &ABRPlayerState::StatTackles, TEXT("tackles") },
			{ TEXT("Yeet Master"), &ABRPlayerState::StatThrows, TEXT("throws") },
			{ TEXT("Mariachi Menace"), &ABRPlayerState::StatDances, TEXT("people made to dance") },
			{ TEXT("Taco Addict"), &ABRPlayerState::StatTacos, TEXT("tacos") },
			{ TEXT("Luchador Slayer"), &ABRPlayerState::StatLuchaWins, TEXT("ring wins") },
		};
		float AwardY = SY * 0.2f + 90.f;
		for (const FAward& Award : Awards)
		{
			const ABRPlayerState* Best = nullptr;
			for (APlayerState* Each : GS->PlayerArray)
			{
				const ABRPlayerState* BPS = Cast<ABRPlayerState>(Each);
				if (BPS && BPS->*Award.Stat > 0 && (!Best || BPS->*Award.Stat > Best->*Award.Stat))
				{
					Best = BPS;
				}
			}
			if (Best)
			{
				Centered(FString::Printf(TEXT("%s:  %s  (%d %s)"), Award.Title, *Best->GetPlayerName(), Best->*Award.Stat, Award.Unit), AwardY, Yellow, Big, 1.3f);
				AwardY += 32.f;
			}
		}
		// --- Best moments of the round
		if (GS->TopMoments.Num() > 0)
		{
			AwardY += 20.f;
			Centered(FString::Printf(TEXT("DAY %d - BEST MOMENTS"), GS->DayNumber), AwardY, White, Big, 1.4f);
			AwardY += 36.f;
			for (int32 i = 0; i < GS->TopMoments.Num(); ++i)
			{
				Centered(FString::Printf(TEXT("%d.  %s"), i + 1, *GS->TopMoments[i]), AwardY, FLinearColor(1.f, 0.85f, 0.5f), Small, 1.3f);
				AwardY += 26.f;
			}
		}
	}

	// --- Kill feed (top right)
	{
		float KY = 90.f;
		for (int32 i = 0; i < GS->FeedLines.Num(); ++i)
		{
			if (Now - GS->FeedTimes[i] < 7.f)
			{
				float W = 0.f, H = 0.f;
				GetTextSize(GS->FeedLines[i], W, H, Small, 1.2f);
				DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), SX - W - 30.f, KY - 2.f, W + 16.f, H + 4.f);
				DrawText(GS->FeedLines[i], White, SX - W - 22.f, KY, Small, 1.2f);
				KY += H + 8.f;
			}
		}
	}

	// --- Announcements
	float FeedY = 90.f;
	for (int32 i = 0; i < GS->RecentMessages.Num(); ++i)
	{
		if (Now - GS->RecentMessageTimes[i] < 9.f)
		{
			DrawText(GS->RecentMessages[i], Yellow, 20.f, FeedY, Big);
			FeedY += 24.f;
		}
	}

	// --- Solo session: how to play with friends
	if (GetNetMode() == NM_Standalone)
	{
		DrawText(TEXT("SOLO - press M for the menu (host / join / pick your side)"), Yellow, 20.f, SY - 210.f, Small, 1.2f);
	}
	if (const UBRGameInstance* GI = GetGameInstance<UBRGameInstance>())
	{
		DrawText(GI->StatusText, FLinearColor(0.7f, 0.7f, 0.7f), 20.f, SY - 230.f, Small, 1.0f);
	}

	// --- Scoreboard (hold Tab)
	if (PC->bShowScoreboard)
	{
		const float W = FMath::Min(1100.f, SX - 80.f);
		const float Left = (SX - W) * 0.5f, Top = SY * 0.14f;
		TArray<ABRPlayerState*> Teams[2];
		for (APlayerState* Each : GS->PlayerArray)
		{
			if (ABRPlayerState* Other = Cast<ABRPlayerState>(Each); Other && Other->GetFaction() != EPVVFaction::None)
			{
				Teams[Other->GetFaction() == BR::Wardens ? 1 : 0].Add(Other);
			}
		}
		int32 Rows = 1;
		for (TArray<ABRPlayerState*>& Team : Teams)
		{
			Team.Sort([](const ABRPlayerState& A, const ABRPlayerState& B) { return A.Points > B.Points; });
			Rows = FMath::Max(Rows, Team.Num());
		}
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.75f), Left, Top, W, 90.f + Rows * 26.f);
		const float ColW = W * 0.5f;
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const float X = Left + Side * ColW + 16.f;
			const bool bW = Side == 1;
			DrawText(FString::Printf(TEXT("%s   %d"), *(bW ? BR::WardenName() : BR::RunnerName()).ToUpper(), bW ? GS->WardenScore : GS->RunnerScore),
				bW ? WardenCol : RunnerCol, X, Top + 10.f, Big, 1.2f);
			DrawText(bW ? TEXT("NAME                      PTS   CATCH  CLEARED  KO   PING") : TEXT("NAME                      PTS   CROSS  SMUGGLED  KO   PING"),
				FLinearColor(0.7f, 0.7f, 0.7f), X, Top + 50.f, Small, 1.f);
			for (int32 i = 0; i < Teams[Side].Num(); ++i)
			{
				const ABRPlayerState* R = Teams[Side][i];
				const float Y = Top + 72.f + i * 26.f;
				const FLinearColor RowCol = R == PS ? Yellow : White;
				DrawText(R->GetPlayerName().Left(24), RowCol, X, Y, Small, 1.15f);
				DrawText(FString::Printf(TEXT("%d"), R->Points), RowCol, X + ColW * 0.48f, Y, Small, 1.15f);
				DrawText(FString::Printf(TEXT("%d"), bW ? R->StatCatches : R->StatCrossings), RowCol, X + ColW * 0.58f, Y, Small, 1.15f);
				DrawText(FString::Printf(TEXT("%d"), bW ? R->StatClearedTrucks : R->StatSmuggled + R->StatContraband), RowCol, X + ColW * 0.69f, Y, Small, 1.15f);
				DrawText(FString::Printf(TEXT("%d"), R->StatKnockdowns), RowCol, X + ColW * 0.81f, Y, Small, 1.15f);
				DrawText(R->IsABot() ? FString(TEXT("BOT")) : FString::Printf(TEXT("%d"), FMath::RoundToInt(R->GetPingInMilliseconds())), RowCol, X + ColW * 0.88f, Y, Small, 1.15f);
			}
		}
	}

	// --- Tips for your first few rounds
	if (const UBRGameInstance* TipGI = GetGameInstance<UBRGameInstance>(); TipGI && TipGI->RoundsPlayed < 3 && GS->Phase != EBRPhase::Ended && PS)
	{
		static const TCHAR* RunnerTips[] = {
			TEXT("TIP: Work your job (shown at the bottom) to earn cash, then buy a LADDER at the hardware store."),
			TEXT("TIP: Lean a ladder on the Wall (hold E) - your whole crew can climb it."),
			TEXT("TIP: Contraband (??? Package) pays $500 if you get it to the Safe Zone."),
			TEXT("TIP: A PONCHO makes Border Patrol scanners think you're a local. Don't sprint!"),
			TEXT("TIP: Z grabs people and things. Z again throws them. Grab a teammate to ride their shoulders."),
			TEXT("TIP: Mash SPACE when you're knocked down or zip-tied."),
			TEXT("TIP: Hold T for emotes. H drops $50 - anyone can pick it up..."),
			TEXT("TIP: Watch out for the watchtower spotlights at night.") };
		static const TCHAR* WardenTips[] = {
			TEXT("TIP: Only arrest Runners who CROSSED the Line - wrongful arrests cost points and approval."),
			TEXT("TIP: Punch a Runner down, hold E to zip-tie, tap E to pick them up, carry them to the Holding Pen."),
			TEXT("TIP: Scan people in trucks with V. Clearing a legal truck raises your budget."),
			TEXT("TIP: Spend the government budget at the GOV. SUPPLY counters (traps, dogs, drone recharge)."),
			TEXT("TIP: Climb a watchtower (E) and aim the spotlight with your view."),
			TEXT("TIP: The $ WANTED $ Runner is worth bonus points."),
			TEXT("TIP: B places traps, Q launches the drone.") };
		const bool bTipWarden = PS->GetFaction() == BR::Wardens;
		const int32 Count = bTipWarden ? static_cast<int32>(UE_ARRAY_COUNT(WardenTips)) : static_cast<int32>(UE_ARRAY_COUNT(RunnerTips));
		const int32 Tip = static_cast<int32>(Now / 9.f) % Count;
		Centered(bTipWarden ? WardenTips[Tip] : RunnerTips[Tip], SY - 262.f, FLinearColor(0.6f, 0.9f, 1.f), Small, 1.2f);
	}

	// --- Personal message
	if (Now - PC->LocalMessageTime < 4.f)
	{
		Centered(PC->LocalMessage.ToString(), SY * 0.25f, White, Big, 1.3f);
	}

	// --- Warden radar: noise pings + tagged Runners
	if (bWarden)
	{
		for (int32 i = 0; i < GS->NoisePings.Num(); ++i)
		{
			if (Now - GS->NoisePingTimes[i] < 6.f)
			{
				Marker(GS->NoisePings[i] + FVector(0.f, 0.f, 200.f), TEXT("((( NOISE )))"), Red);
			}
		}
		for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
		{
			if (It->IsRunner() && It->IsSpotted())
			{
				Marker(It->GetActorLocation() + FVector(0.f, 0.f, 160.f), TEXT("v TAGGED v"), Red);
			}
		}
	}

	APawn* Pawn = PC->GetPawn();

	// --- Drone
	if (const ABRDrone* Drone = Cast<ABRDrone>(Pawn))
	{
		Centered(TEXT("[ DRONE ]  WASD fly  |  Space/Ctrl up/down  |  Left click TAG a Runner  |  Q return"), SY - 70.f, Green, Big, 1.1f);
		Bar(SX * 0.5f - 150.f, SY - 40.f, 300.f, 16.f, Drone->Battery, Drone->Battery > 0.25f ? Green : Red);
		Centered(TEXT("+"), SY * 0.5f - 12.f, Green, Big, 1.5f);
		return;
	}

	// --- Driving
	if (const APVVBoat* Vehicle = Cast<APVVBoat>(Pawn))
	{
		Centered(TEXT("DRIVING  -  W/S throttle  A/D steer  E get out  |  R/T radio  Backspace off  H horn"), SY - 90.f, White, Big, 1.1f);
		FString Info = FString::Printf(TEXT("%.0f km/h"), FMath::Abs(Vehicle->GetForwardSpeed()) * 0.036f);
		if (const ABRCar* Car = Cast<ABRCar>(Vehicle))
		{
			Info += Car->bInfiniteFuel ? TEXT("   |   Fuel: government-funded") : FString::Printf(TEXT("   |   Fuel %d%%"), FMath::RoundToInt(Car->Fuel * 100.f));
			if (Car->IsFlooded())
			{
				Centered(TEXT("ENGINE FLOODED! Trucks don't swim."), SY * 0.4f, Red, Big, 1.6f);
			}
			else if (!Car->bInfiniteFuel && Car->Fuel <= 0.f)
			{
				Centered(TEXT("OUT OF GAS! Bring a Gas Can."), SY * 0.4f, Red, Big, 1.6f);
			}
			Centered(FString::Printf(TEXT("RADIO: %s"), *Car->GetStationName()), SY - 125.f, FLinearColor(0.6f, 0.9f, 1.f), Big, 1.1f);
		}
		Centered(Info, SY - 60.f, Yellow, Big, 1.2f);
		return;
	}

	ABRCharacter* Me = Cast<ABRCharacter>(Pawn);
	if (!Me || !PS)
	{
		Centered(TEXT("Waiting to spawn..."), SY * 0.45f, White, Big, 1.5f);
		return;
	}

	// --- Team + goal
	{
		const UBRSettings* Names = GetDefault<UBRSettings>();
		const FString Singular = bWarden ? Names->WardenSingular : Names->RunnerSingular;
		DrawText(FString::Printf(TEXT("YOU ARE: %s"), *Singular.ToUpper()), bWarden ? WardenCol : RunnerCol, 20.f, SY - 175.f, Big, 1.4f);
	}
	const TCHAR* Goal = bWarden
		? (GS->Phase == EBRPhase::Prep ? TEXT("Prep: grab a truck, scout with your drone (Q). The Line opens soon.")
			: TEXT("Spot Runners, taser them, zip-tie, haul them to the Holding Pen. Don't shoot anyone who hasn't crossed!"))
		: (GS->Phase == EBRPhase::Prep ? TEXT("Prep: work jobs, buy a ladder / wire cutters / gas / tacos. Plan your route.")
			: TEXT("Cross the Line, get past the wire, river and Wall, reach the SAFE ZONE (far east)."));
	DrawText(Goal, White, 20.f, SY - 143.f, Small, 1.2f);

	if (bWarden)
	{
		DrawText(FString::Printf(TEXT("GOV. BUDGET $%d"), GS->WardenBudget), Green, 20.f, SY - 118.f, Big, 1.3f);
	}
	if (GS->BountyTarget && GS->Phase == EBRPhase::Invasion)
	{
		const bool bMe = GS->BountyTarget == PS;
		Centered(bMe ? FString(TEXT("THERE'S A BOUNTY ON YOU! Border Patrol can see the $ over your head"))
			: FString::Printf(TEXT("BOUNTY: %s"), *GS->BountyTarget->GetPlayerName()), 92.f, bMe ? Red : Green, Small, 1.3f);
	}
	if (!bWarden)
	{
		DrawText(FString::Printf(TEXT("$%d"), PS->Money), Green, 20.f, SY - 118.f, Big, 1.3f);
		const FString JobText = PS->AssignedJob.IsEmpty() ? FString() : (PS->AssignedJob == TEXT("-") ? FString(TEXT("JOB: none - rummage for scrap")) : TEXT("JOB: ") + PS->AssignedJob);
		DrawText(JobText, FLinearColor(0.8f, 0.8f, 0.8f), 560.f, SY - 135.f, Small, 1.1f);
		DrawText(TEXT("FOOD"), White, 120.f, SY - 112.f, Small);
		Bar(170.f, SY - 114.f, 180.f, 14.f, Me->GetHunger() / 100.f, Me->GetHunger() > 25.f ? FLinearColor(0.9f, 0.6f, 0.2f) : Red);
		DrawText(TEXT("SEEN"), White, 370.f, SY - 112.f, Small);
		Bar(420.f, SY - 114.f, 120.f, 14.f, Me->GetVisibility() / 1.5f, Me->GetVisibility() < 0.4f ? Green : (Me->GetVisibility() < 0.9f ? Yellow : Red));

		if (PS->SentenceSeconds > 0)
		{
			Centered(FString::Printf(TEXT("HOLDING PEN - catapult in %s (or wait for a jailbreak)"), *Clock(PS->SentenceSeconds)), SY * 0.4f, Red, Big, 1.5f);
		}
		if (PS->bHasCrossedLine)
		{
			DrawText(TEXT("CROSSED"), Red, 560.f, SY - 116.f, Small, 1.2f);
		}
	}
	// Inventory slots (both teams) + what the selected item does.
	for (int32 i = 0; i < ABRPlayerState::MaxSlots; ++i)
	{
		const float X = 20.f + i * 130.f;
		const bool bSelected = i == Me->GetSelectedSlot() && PS->Inventory.IsValidIndex(i);
		if (bSelected)
		{
			DrawRect(FLinearColor(1.f, 0.85f, 0.3f, 0.9f), X - 3.f, SY - 95.f, 130.f, 32.f);
		}
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), X, SY - 92.f, 124.f, 26.f);
		if (PS->Inventory.IsValidIndex(i))
		{
			const EBRItem Item = PS->Inventory[i];
			DrawText(BR::ItemName(Item), BR::IsIllegal(Item) ? Red : White, X + 6.f, SY - 88.f, Small, 1.1f);
		}
	}
	{
		FString Hint;
		if (PS->Inventory.IsValidIndex(Me->GetSelectedSlot()))
		{
			Hint = ItemUseHint(PS->Inventory[Me->GetSelectedSlot()]);
		}
		if (PS->Inventory.Num() > 1)
		{
			Hint += Hint.IsEmpty() ? TEXT("Mouse wheel: switch item") : TEXT("   |   Wheel: switch item   G: drop");
		}
		DrawText(Hint, Yellow, 20.f, SY - 60.f, Small, 1.1f);
	}
	if (Me->GetCameraViewIndex() >= 0)
	{
		const float Blink = FMath::Fmod(Now, 1.f) < 0.5f ? 1.f : 0.3f;
		DrawText(TEXT("[REC]"), FLinearColor(1.f, 0.1f, 0.1f, Blink), 40.f, 40.f, Big, 1.5f);
		Centered(FString::Printf(TEXT("SECURITY CAMERA %d / %d     L: next camera     move: exit"), Me->GetCameraViewIndex() + 1, Me->GetCameraViewCount()), 70.f, White, Big, 1.3f);
	}
	if (Me->HasWeapon())
	{
		const FLinearColor WeaponCol = (bWarden && !Me->IsLethalMode()) ? FLinearColor(0.4f, 0.8f, 1.f) : Red;
		const FString AmmoText = Me->IsReloading() ? TEXT("RELOADING...") : FString::Printf(TEXT("%d / %d"), Me->GetAmmo(), Me->GetMagSize());
		DrawText(FString::Printf(TEXT("%s   %s"), *Me->GetWeaponName(), *AmmoText), WeaponCol, SX - 520.f, SY - 70.f, Big, 1.2f);
	}

	// Crosshair + hit marker.
	if (Me->IsAiming())
	{
		const float CX = SX * 0.5f, CY = SY * 0.5f;
		const FLinearColor CrossCol(1.f, 1.f, 1.f, 0.9f);
		DrawRect(CrossCol, CX - 12.f, CY - 1.f, 8.f, 2.f);
		DrawRect(CrossCol, CX + 4.f, CY - 1.f, 8.f, 2.f);
		DrawRect(CrossCol, CX - 1.f, CY - 12.f, 2.f, 8.f);
		DrawRect(CrossCol, CX - 1.f, CY + 4.f, 2.f, 8.f);
		if (Now - PC->HitMarkerTime < 0.25f)
		{
			Centered(TEXT("X"), CY - 14.f, PC->bHitMarkerLethal ? Red : FLinearColor(0.4f, 0.8f, 1.f), Big, 1.4f);
		}
	}
	else
	{
		DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.5f), SX * 0.5f - 2.f, SY * 0.5f - 2.f, 4.f, 4.f);
	}

	// Breath (deep water) and grabbed/stuck warnings.
	if (Me->GetBreath() < 0.999f)
	{
		Centered(TEXT("BREATH"), SY * 0.33f - 22.f, FLinearColor(0.5f, 0.8f, 1.f), Small, 1.2f);
		Bar(SX * 0.5f - 150.f, SY * 0.33f, 300.f, 16.f, Me->GetBreath(), Me->GetBreath() > 0.35f ? FLinearColor(0.4f, 0.75f, 1.f) : Red);
	}
	if (Me->IsSlowed())
	{
		Centered(TEXT("STUCK!"), SY * 0.36f + 30.f, Red, Big, 1.4f);
	}

	if (bWarden)
	{
		static const TCHAR* TrapNames[] = { TEXT("Bear Trap"), TEXT("Glue"), TEXT("Alarm Wire"), TEXT("Spike Strip") };
		int32 CamCount = 0;
		for (TActorIterator<ABRSurveillanceCamera> It(GetWorld()); It; ++It)
		{
			++CamCount;
		}
		DrawText(FString::Printf(TEXT("B: place trap [%s]   N: change trap   V: scan   X: fists/taser/gun   Q: drone   L: view cameras (%d)   O: order out   E: interact / search"),
			TrapNames[Me->GetSelectedTrap() % 4], CamCount), FLinearColor(0.6f, 0.8f, 1.f), 20.f, SY - 40.f, Small, 1.1f);
	}
	else
	{
		DrawText(TEXT("F: use item   Wheel: switch item   G: drop   E: interact (hold for jobs, digging, cutting)   H: drop $   Q: whistle for your clients"),
			FLinearColor(0.8f, 0.8f, 0.8f), 20.f, SY - 40.f, Small, 1.1f);
	}
	DrawText(TEXT("Click = punch (tap) / grab + stick (hold) with that hand | Z grab/throw | Left Alt block | Left Ctrl dive | Middle mouse headbutt | Space jump/climb | T emotes | M menu"),
		FLinearColor(0.8f, 0.8f, 0.8f), 20.f, SY - 22.f, Small, 1.1f);

	// --- Emote wheel (hold T, move the mouse toward one, let go)
	if (Me->IsEmoteWheelOpen())
	{
		const float CX = SX * 0.5f, CY = SY * 0.5f, R = 150.f;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.35f), CX - R - 70.f, CY - R - 40.f, (R + 70.f) * 2.f, (R + 40.f) * 2.f);
		for (int32 i = 0; i < ABRCharacter::NumEmotes; ++i)
		{
			const float A = FMath::DegreesToRadians(i * 360.f / ABRCharacter::NumEmotes);
			const FString Name = ABRCharacter::EmoteName(i);
			const bool bPicked = Me->GetEmoteChoice() == i;
			float W = 0.f, H = 0.f;
			GetTextSize(Name, W, H, Big, bPicked ? 1.5f : 1.1f);
			const float X = CX + FMath::Sin(A) * R - W * 0.5f, Y = CY - FMath::Cos(A) * R - H * 0.5f;
			if (bPicked)
			{
				DrawRect(FLinearColor(1.f, 0.85f, 0.3f, 0.8f), X - 8.f, Y - 4.f, W + 16.f, H + 8.f);
			}
			DrawText(Name, bPicked ? FLinearColor::Black : White, X, Y, Big, bPicked ? 1.5f : 1.1f);
		}
		Centered(TEXT("let go of T"), CY - 8.f, FLinearColor(0.8f, 0.8f, 0.8f), Small, 1.1f);
	}

	// --- Objective markers
	if (GS->Phase == EBRPhase::Invasion)
	{
		if (!bWarden && !PS->bReachedSafety && Me->GetCondition() == EPVVCondition::Normal)
		{
			const FVector SafeGoal = BRMap::GetSafeZoneGoal(GetWorld(), Me->GetActorLocation());
			Marker(SafeGoal + FVector(0.f, 0.f, 300.f), FString::Printf(TEXT("SAFE ZONE  %dm"), FMath::RoundToInt(FVector::Dist2D(SafeGoal, Me->GetActorLocation()) / 100.f)), Green);
		}
		if (bWarden && Me->GetCarriedCharacter())
		{
			for (TActorIterator<APVVDetentionZone> It(GetWorld()); It; ++It)
			{
				Marker(It->GetActorLocation() + FVector(0.f, 0.f, 300.f), TEXT("HOLDING PEN"), Yellow);
				break;
			}
		}
		if (GS->BountyTarget && GS->BountyTarget != PS)
		{
			for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
			{
				if (It->GetPlayerState() == GS->BountyTarget && (!bWarden || It->IsSpotted() || FVector::Dist(It->GetActorLocation(), Me->GetActorLocation()) < 4000.f))
				{
					Marker(It->GetActorLocation() + FVector(0.f, 0.f, 160.f), TEXT("$ BOUNTY $"), Green);
				}
			}
		}
	}

	// --- Mariachi: beat circle
	for (TActorIterator<ABRMariachiBand> It(GetWorld()); It; ++It)
	{
		if (!It->IsPerformer(Me))
		{
			continue;
		}
		const float Phase = It->GetBeatPhase();
		const bool bOnBeat = Phase < 0.15f || Phase > 0.85f;
		const float Size = FMath::Lerp(110.f, 40.f, Phase);
		const float CX = SX * 0.5f, CY = SY * 0.78f;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.4f), CX - 60.f, CY - 60.f, 120.f, 120.f);
		DrawRect(bOnBeat ? FLinearColor(0.2f, 1.f, 0.3f, 0.9f) : FLinearColor(1.f, 0.85f, 0.2f, 0.7f), CX - Size * 0.5f, CY - Size * 0.5f, Size, Size);
		Centered(FString::Printf(TEXT("PRESS F ON THE BEAT  -  streak %d  (every 4 = tip)"), It->GetStreak(Me)), CY + 70.f, White, Small, 1.3f);
		break;
	}

	// --- Hiding
	if (Me->IsHidingInSpot())
	{
		Centered(TEXT("HIDING  -  nobody can see you.  E = climb out"), SY * 0.7f, Green, Big, 1.2f);
	}
	else if (Me->IsInBox())
	{
		Centered(Me->IsHiddenFromPatrol() ? TEXT("In the box: invisible while you stand still") : TEXT("In the box: MOVING (they can see you)"), SY * 0.7f, Me->IsHiddenFromPatrol() ? Green : Yellow, Small, 1.3f);
	}

	// --- Smuggling job timer
	if (const ABRPlayerState* JobPS = Me->GetBRPlayerState(); JobPS && JobPS->MissionPay > 0)
	{
		const int32 Left = FMath::Max(0, FMath::CeilToInt(JobPS->MissionDeadline - GS->GetServerWorldTimeSeconds()));
		Centered(FString::Printf(TEXT("SMUGGLING JOB  $%d   -   %d:%02d to reach the Safe Zone"), JobPS->MissionPay, Left / 60, Left % 60), 155.f, Left < 30 ? Red : Yellow, Small, 1.3f);
	}

	// --- Map (hold Tab)
	if (PC->bShowScoreboard)
	{
		DrawMap(Me, GS);
	}

	// --- Wanted stars (your own)
	if (const ABRPlayerState* MyPS = Me->GetBRPlayerState(); MyPS && MyPS->WantedStars > 0)
	{
		const FString Stars = MyPS->WantedStars >= 3 ? TEXT("* * *") : (MyPS->WantedStars == 2 ? TEXT("* *") : TEXT("*"));
		Centered(FString::Printf(TEXT("WANTED  %s%s"), *Stars, MyPS->WantedStars >= 3 ? TEXT("   (Border Patrol can hear where you are!)") : TEXT("   (lay low to cool off)")), 130.f, Yellow, Small, 1.3f);
	}

	if (Me->IsDazed())
	{
		Centered(TEXT("DAZED!  (one more hit and you're out - get away or block!)"), SY * 0.42f, Yellow, Big, 1.4f);
	}

	// --- Blocking
	if (Me->IsBlocking())
	{
		Centered(TEXT("BLOCKING  (hits from the front barely hurt - grabs and kicks from behind still get you)"), SY * 0.78f, Green, Small, 1.1f);
		Bar(SX * 0.5f - 120.f, SY * 0.78f + 26.f, 240.f, 10.f, Me->GuardMeter, Me->GuardMeter > 0.3f ? Green : Red);
	}

	// --- Sprint stamina (only shows while it's not full)
	if (Me->SprintStamina < 0.999f && !Me->GetCurrentBoat())
	{
		const FLinearColor StamColor = Me->SprintStamina < 0.3f ? FLinearColor(1.f, 0.35f, 0.2f) : FLinearColor(0.95f, 0.85f, 0.3f);
		Bar(SX * 0.5f - 140.f, SY - 46.f, 280.f, 9.f, Me->SprintStamina, StamColor);
	}

	// --- Health (only knives and bullets hurt it - shows once you're wounded)
	if (Me->bDead)
	{
		Centered(TEXT("YOU'RE DEAD"), SY * 0.4f, Red, Big, 2.f);
	}
	else if (Me->Health < 99.5f)
	{
		Centered(Me->Health < 35.f ? TEXT("HEALTH  - BLEEDING BADLY!") : TEXT("HEALTH"), SY - 128.f, Me->Health < 35.f ? Red : White, Small, 1.f);
		Bar(SX * 0.5f - 110.f, SY - 106.f, 220.f, 10.f, Me->Health / 100.f, Me->Health < 35.f ? FLinearColor(0.9f, 0.1f, 0.1f) : FLinearColor(0.75f, 0.15f, 0.15f));
	}

	// --- Shovel wear (breaks after a few digs - buy a new one)
	if (const ABRPlayerState* ShovelPS = Me->GetBRPlayerState(); ShovelPS && ShovelPS->HasItem(EBRItem::Shovel))
	{
		const float Left = FMath::Clamp(1.f - ShovelPS->ShovelWear, 0.f, 1.f);
		const int32 DigsLeft = FMath::Max(1, FMath::RoundToInt(Left * ABRPlayerState::ShovelDigs));
		Centered(FString::Printf(TEXT("SHOVEL  (%d dig%s left)"), DigsLeft, DigsLeft == 1 ? TEXT("") : TEXT("s")), SY - 88.f, Left < 0.34f ? Red : Yellow, Small, 1.f);
		Bar(SX * 0.5f - 90.f, SY - 66.f, 180.f, 8.f, Left, Left < 0.34f ? FLinearColor(1.f, 0.35f, 0.2f) : FLinearColor(0.75f, 0.55f, 0.3f));
	}

	// --- Cantina: drinking contest + tipsy
	if (Me->bChugging)
	{
		Centered(TEXT("DRINKING CONTEST!  TAP SPACE IN RHYTHM - DON'T SPAM!"), SY * 0.36f, Yellow, Big, 1.6f);
		Centered(TEXT("YOU"), SY * 0.36f + 40.f, Green, Small, 1.2f);
		Bar(SX * 0.5f - 200.f, SY * 0.36f + 62.f, 400.f, 18.f, Me->ChugMine, Green);
		Centered(TEXT("EL BORRACHO"), SY * 0.36f + 90.f, Red, Small, 1.2f);
		Bar(SX * 0.5f - 200.f, SY * 0.36f + 112.f, 400.f, 18.f, Me->ChugRival, Red);
		Centered(TEXT("GAG"), SY * 0.36f + 140.f, Me->ChugGag > 0.6f ? Red : Yellow, Small, 1.f);
		Bar(SX * 0.5f - 100.f, SY * 0.36f + 160.f, 200.f, 10.f, Me->ChugGag, Me->ChugGag > 0.6f ? Red : Yellow);
	}
	else if (Me->IsTipsy())
	{
		Centered(FString::Printf(TEXT("~ tipsy ~  (wobbly, but tough)  %ds"), FMath::CeilToInt(Me->GetTipsySecondsLeft())), SY - 120.f, Yellow, Small, 1.1f);
	}
	// Bar fight scoreboard (when you're in the cantina during a fight).
	for (TActorIterator<ABRCantina> It(GetWorld()); It; ++It)
	{
		if (It->IsFightOn() && It->IsInside(Me->GetActorLocation()))
		{
			const int32 Secs = FMath::Max(0, FMath::CeilToInt(It->GetFightSecondsLeft()));
			Centered(FString::Printf(TEXT("BAR FIGHT!   locals still standing: %d   %d:%02d   (last one standing wins - go down and you pay)"), It->GetLocalsStanding(), Secs / 60, Secs % 60), 60.f, Red, Big, 1.1f);
		}
	}
	// Big result banner.
	if (Me->BannerUntil > GetWorld()->GetTimeSeconds())
	{
		Centered(Me->BannerText, SY * 0.3f, Me->bBannerGood ? Green : Red, Big, 2.2f);
	}

	// --- Climbing
	if (Me->IsHanging())
	{
		Centered(TEXT("AT THE TOP!  SPACE = pull yourself up"), SY * 0.7f, Yellow, Big, 1.3f);
	}
	else if (Me->IsClimbing())
	{
		Centered(TEXT("CLIMBING:  LEFT click, RIGHT click, LEFT, RIGHT...   (same hand twice = slip)   SPACE = let go"), SY * 0.7f, Yellow, Small, 1.3f);
	}

	// --- Hand grabs
	if (Me->GetGrabbedBy() && Me->GetCondition() == EPVVCondition::Normal)
	{
		Centered(TEXT("GRABBED!  MASH SPACE TO WRIGGLE FREE"), SY * 0.45f, Red, Big, 1.6f);
		Bar(SX * 0.5f - 150.f, SY * 0.45f + 40.f, 300.f, 16.f, Me->GetGrabStruggle(), Yellow);
	}
	else if (const ABRCharacter* Held = Me->GetHandGrabTarget())
	{
		Centered(FString::Printf(TEXT("HOLDING %s  -  tap the other click to punch | Z throw%s"), *Held->GetPlayerDisplayName(),
			bWarden ? TEXT(" | hold E zip-tie") : TEXT("")), SY * 0.7f, Yellow, Small, 1.3f);
	}
	if (!Me->HasWeapon() && bWarden)
	{
		DrawText(TEXT("FISTS   (X = taser)"), FLinearColor(0.8f, 0.8f, 0.8f), SX - 520.f, SY - 70.f, Big, 1.2f);
	}

	// --- Status
	if (Me->GetCondition() == EPVVCondition::Detained)
	{
		Centered(TEXT("N = watch other players"), SY * 0.4f + 40.f, White, Small, 1.2f);
		Centered(Me->bGuardWatching ? TEXT("A GUARD IS WATCHING - wait for them to leave...") : TEXT("Nobody's watching: MASH SPACE to pick your cuffs!"),
			SY * 0.4f + 75.f, Me->bGuardWatching ? Red : Green, Small, 1.3f);
		Bar(SX * 0.5f - 150.f, SY * 0.4f + 100.f, 300.f, 12.f, Me->EscapeProgress, Me->bGuardWatching ? Red : Green);
	}
	switch (Me->GetCondition())
	{
	case EPVVCondition::KnockedDown: Centered(TEXT("KNOCKED DOWN!"), SY * 0.45f, Red, Big, 1.8f); break;
	case EPVVCondition::Restrained:
		Centered(TEXT("ZIP-TIED!  MASH SPACE TO BREAK FREE"), SY * 0.45f, Red, Big, 1.8f);
		Bar(SX * 0.5f - 150.f, SY * 0.45f + 45.f, 300.f, 18.f, Me->GetStruggleProgress(), Yellow);
		break;
	default: break;
	}

	// --- Prompt
	if (!Me->GetFocusPrompt().IsEmpty())
	{
		Centered(FString::Printf(TEXT("[E]  %s"), *Me->GetFocusPrompt().ToString()), SY * 0.62f, Yellow, Big, 1.4f);
	}
	if (Me->GetHoldProgress() > 0.f)
	{
		Bar(SX * 0.5f - 120.f, SY * 0.62f + 38.f, 240.f, 14.f, Me->GetHoldProgress(), Yellow);
	}
}

void ABRHUD::DrawMap(const ABRCharacter* Me, const ABRGameState* GS)
{
	const ABRDesertArena* Arena = nullptr;
	for (TActorIterator<ABRDesertArena> It(GetWorld()); It; ++It)
	{
		Arena = *It;
	}
	(void)GS;
	if (!Arena || !Me)
	{
		return; // the map only knows the built-in test map
	}
	const float MinX = -ABRDesertArena::HalfX, MaxX = ABRDesertArena::EastX, MinY = -ABRDesertArena::HalfY, MaxY = ABRDesertArena::HalfY;
	const float W = FMath::Min(560.f, Canvas->SizeX * 0.42f);
	const float H = W * (MaxY - MinY) / (MaxX - MinX);
	const float Left = Canvas->SizeX - W - 20.f, Top = Canvas->SizeY - H - 110.f;
	const FVector O = Arena->GetActorLocation();
	auto ToMap = [&](const FVector& World) { return FVector2D(Left + (World.X - O.X - MinX) / (MaxX - MinX) * W, Top + (World.Y - O.Y - MinY) / (MaxY - MinY) * H); };
	auto Band = [&](float X0, float X1, const FLinearColor& C) { const FVector2D A = ToMap(O + FVector(X0, MinY, 0.f)), B = ToMap(O + FVector(X1, MaxY, 0.f)); DrawRect(C, A.X, A.Y, B.X - A.X, B.Y - A.Y); };
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), Left - 6.f, Top - 24.f, W + 12.f, H + 30.f);
	DrawRect(FLinearColor(0.55f, 0.42f, 0.26f, 0.9f), Left, Top, W, H);
	Band(-ABRDesertArena::RiverHalfWidth, ABRDesertArena::RiverHalfWidth, FLinearColor(0.15f, 0.35f, 0.7f, 0.95f));
	Band(ABRDesertArena::LineX - 25.f, ABRDesertArena::LineX + 25.f, FLinearColor(1.f, 0.85f, 0.f));
	Band(ABRDesertArena::WallX - 40.f, ABRDesertArena::WallX + 40.f, FLinearColor(0.3f, 0.3f, 0.3f));
	Band(ABRDesertArena::SafeX, ABRDesertArena::EastX, FLinearColor(0.1f, 0.7f, 0.2f, 0.6f));
	{
		const FVector2D A = ToMap(O + FVector(MinX, ABRDesertArena::RoadY - 350.f, 0.f)), B = ToMap(O + FVector(MaxX, ABRDesertArena::RoadY + 350.f, 0.f));
		DrawRect(FLinearColor(0.12f, 0.12f, 0.13f), A.X, A.Y, B.X - A.X, FMath::Max(2.f, B.Y - A.Y));
	}
	struct FSpot { float X, Y; const TCHAR* Name; };
	static const FSpot Spots[] = { {-11000.f, 0.f, TEXT("TOWN")}, {-8000.f, -6300.f, TEXT("CANTINA")}, {-7550.f, 3000.f, TEXT("CAR LOT")},
		{-11000.f, -6200.f, TEXT("RING")}, {-9000.f, 2200.f, TEXT("PIT")}, {-9800.f, 1900.f, TEXT("JOBS")}, {6100.f, 4000.f, TEXT("PEN")},
		{8500.f, 4000.f, TEXT("HQ")}, {3500.f, -1200.f, TEXT("GATE")}, {23500.f, 0.f, TEXT("SAFE ZONE")} };
	for (const FSpot& Spot : Spots)
	{
		const FVector2D P = ToMap(O + FVector(Spot.X, Spot.Y, 0.f));
		DrawText(Spot.Name, FLinearColor::White, P.X - 12.f, P.Y - 6.f, GEngine->GetSmallFont(), 0.75f);
	}
	// People: you, your teammates, and tagged enemies (never untagged Runners for Border Patrol).
	for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
	{
		const ABRCharacter* C = *It;
		if (C->IsNPC())
		{
			continue;
		}
		const bool bMe = C == Me, bMate = C->GetFaction() == Me->GetFaction();
		if (!bMe && !bMate && !C->IsSpotted())
		{
			continue;
		}
		const FVector2D P = ToMap(C->GetActorLocation());
		const FLinearColor Col = bMe ? FLinearColor::White : (bMate ? FLinearColor(0.2f, 1.f, 0.3f) : FLinearColor(1.f, 0.2f, 0.2f));
		const float Sz = bMe ? 9.f : 7.f;
		DrawRect(Col, P.X - Sz * 0.5f, P.Y - Sz * 0.5f, Sz, Sz);
	}
	DrawText(TEXT("MAP  (white = you, green = team, red = tagged)"), FLinearColor(1.f, 0.9f, 0.6f), Left, Top - 20.f, GEngine->GetSmallFont(), 0.9f);
}
