#include "BRHUD.h"
#include "BRCar.h"
#include "BRCharacter.h"
#include "BRDrone.h"
#include "BRGameState.h"
#include "BRPlayerController.h"
#include "BRPlayerState.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
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
	const FVector Screen = Project(World);
	if (Screen.Z <= 0.f)
	{
		return;
	}
	float W = 0.f, H = 0.f;
	GetTextSize(Text, W, H, GEngine->GetMediumFont());
	DrawText(Text, Color, Screen.X - W * 0.5f, Screen.Y - H, GEngine->GetMediumFont());
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
	case EBRPhase::Invasion: PhaseText = FString::Printf(TEXT("INVASION  -  %s left   |   Safe: %d / %d"), *Clock(GS->PhaseSecondsLeft), GS->RunnersSafe, GS->RunnersNeeded); break;
	default: PhaseText = GS->Winner == BR::Runners ? TEXT("RUNNERS WIN!") : TEXT("WARDENS WIN!"); break;
	}
	Centered(PhaseText, 10.f, GS->Phase == EBRPhase::Invasion ? Red : White, Big, 1.4f);
	const TCHAR* WeatherText = GS->Weather == EBRWeather::Rain ? TEXT("Rain") : GS->Weather == EBRWeather::Sandstorm ? TEXT("SANDSTORM") : TEXT("Clear");
	Centered(FString::Printf(TEXT("%s  %s   |   %s   |   Warden approval %d%%"), *TimeOfDayText(GS->TimeOfDay), GS->IsNight() ? TEXT("(night)") : TEXT(""),
		WeatherText, FMath::RoundToInt(GS->Approval)), 46.f, White, Small, 1.2f);
	Bar(SX * 0.5f - 150.f, 66.f, 300.f, 10.f, GS->Approval / 100.f, GS->Approval >= ABRGameState::DroneApproval ? Green : (GS->Approval >= ABRGameState::LethalApproval ? Yellow : Red));

	if (GS->Phase == EBRPhase::Ended)
	{
		Centered(GS->Winner == BR::Runners ? TEXT("RUNNERS WIN!") : TEXT("WARDENS WIN!"), SY * 0.3f, GS->Winner == BR::Runners ? RunnerCol : WardenCol, Big, 4.f);
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
		DrawText(TEXT("SOLO - press ~ and type  Host  (you host)  or  Join 26.x.x.x  (your friend's Radmin IP)"), Yellow, 20.f, SY - 210.f, Small, 1.2f);
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
		Centered(TEXT("DRIVING  -  W/S throttle  A/D steer  E get out"), SY - 90.f, White, Big, 1.2f);
		FString Info = FString::Printf(TEXT("%.0f km/h"), FMath::Abs(Vehicle->GetForwardSpeed()) * 0.036f);
		if (const ABRCar* Car = Cast<ABRCar>(Vehicle))
		{
			Info += Car->bInfiniteFuel ? TEXT("   |   Fuel: government-funded") : FString::Printf(TEXT("   |   Fuel %d%%"), FMath::RoundToInt(Car->Fuel * 100.f));
			if (!Car->bInfiniteFuel && Car->Fuel <= 0.f)
			{
				Centered(TEXT("OUT OF GAS! Bring a Gas Can."), SY * 0.4f, Red, Big, 1.6f);
			}
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
	DrawText(bWarden ? TEXT("YOU ARE A WARDEN") : TEXT("YOU ARE A RUNNER"), bWarden ? WardenCol : RunnerCol, 20.f, SY - 175.f, Big, 1.4f);
	const TCHAR* Goal = bWarden
		? (GS->Phase == EBRPhase::Prep ? TEXT("Prep: grab a truck, scout with your drone (Q). The Line opens soon.")
			: TEXT("Spot Runners, taser them, zip-tie, haul them to the Holding Pen. Don't shoot anyone who hasn't crossed!"))
		: (GS->Phase == EBRPhase::Prep ? TEXT("Prep: work jobs, buy a ladder / wire cutters / gas / tacos. Plan your route.")
			: TEXT("Cross the Line, get past the wire, river and Wall, reach the SAFE ZONE (far east)."));
	DrawText(Goal, White, 20.f, SY - 143.f, Small, 1.2f);

	if (!bWarden)
	{
		DrawText(FString::Printf(TEXT("$%d"), PS->Money), Green, 20.f, SY - 118.f, Big, 1.3f);
		DrawText(TEXT("FOOD"), White, 120.f, SY - 112.f, Small);
		Bar(170.f, SY - 114.f, 180.f, 14.f, Me->GetHunger() / 100.f, Me->GetHunger() > 25.f ? FLinearColor(0.9f, 0.6f, 0.2f) : Red);
		DrawText(TEXT("SEEN"), White, 370.f, SY - 112.f, Small);
		Bar(420.f, SY - 114.f, 120.f, 14.f, Me->GetVisibility() / 1.5f, Me->GetVisibility() < 0.4f ? Green : (Me->GetVisibility() < 0.9f ? Yellow : Red));

		// Inventory slots
		for (int32 i = 0; i < ABRPlayerState::MaxSlots; ++i)
		{
			const float X = 20.f + i * 130.f;
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), X, SY - 92.f, 124.f, 26.f);
			if (PS->Inventory.IsValidIndex(i))
			{
				const EBRItem Item = PS->Inventory[i];
				DrawText(BR::ItemName(Item), BR::IsIllegal(Item) ? Red : White, X + 6.f, SY - 88.f, Small, 1.1f);
			}
		}
		if (PS->SentenceSeconds > 0)
		{
			Centered(FString::Printf(TEXT("HOLDING PEN - catapult in %s (or wait for a jailbreak)"), *Clock(PS->SentenceSeconds)), SY * 0.4f, Red, Big, 1.5f);
		}
		if (PS->bHasCrossedLine)
		{
			DrawText(TEXT("CROSSED"), Red, 560.f, SY - 116.f, Small, 1.2f);
		}
	}
	else
	{
		DrawText(Me->IsLethalMode() ? TEXT("WEAPON: GUN (lethal)  [X to switch]") : TEXT("WEAPON: TASER  [X to switch]"), Me->IsLethalMode() ? Red : FLinearColor(0.4f, 0.8f, 1.f), 20.f, SY - 116.f, Big);
	}

	DrawText(bWarden
		? TEXT("WASD move | Shift sprint | C crouch | LMB punch | RMB fire | X taser/gun | Q drone | E interact")
		: TEXT("WASD move | Shift sprint | C crouch | LMB punch | RMB fire (if armed) | F eat | E interact (tap / hold)"),
		FLinearColor(0.8f, 0.8f, 0.8f), 20.f, SY - 40.f, Small, 1.1f);

	// --- Status
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
