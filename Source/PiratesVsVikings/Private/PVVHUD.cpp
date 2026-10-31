#include "PVVHUD.h"
#include "PVVBoat.h"
#include "PVVCharacter.h"
#include "PVVGameState.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

void APVVHUD::DrawCentered(const FString& Text, float Y, const FLinearColor& Color, UFont* Font, float Scale)
{
	float W = 0.f, H = 0.f;
	GetTextSize(Text, W, H, Font, Scale);
	DrawText(Text, Color, (Canvas->ClipX - W) * 0.5f, Y, Font, Scale);
}

void APVVHUD::DrawBar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Color)
{
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), X, Y, W, H);
	DrawRect(Color, X + 2.f, Y + 2.f, (W - 4.f) * FMath::Clamp(Fraction, 0.f, 1.f), H - 4.f);
}

void APVVHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !GEngine)
	{
		return;
	}

	UFont* Big = GEngine->GetLargeFont();
	UFont* Small = GEngine->GetSmallFont();
	const float SX = Canvas->ClipX;
	const float SY = Canvas->ClipY;
	const FLinearColor White = FLinearColor::White;
	const FLinearColor Pirate = PVV::FactionColor(EPVVFaction::Pirates);
	const FLinearColor Viking = PVV::FactionColor(EPVVFaction::Vikings);
	const FLinearColor Yellow(1.f, 0.85f, 0.2f);

	// Scoreboard + clock.
	if (const APVVGameState* GS = GetWorld()->GetGameState<APVVGameState>())
	{
		DrawCentered(FString::Printf(TEXT("PIRATES %d   |   %d:%02d   |   %d VIKINGS"),
			GS->PirateScore, GS->RemainingSeconds / 60, GS->RemainingSeconds % 60, GS->VikingScore), 12.f, White, Big, 1.3f);

		// Kill feed (last 8 seconds).
		const float Now = GetWorld()->GetTimeSeconds();
		float FeedY = 60.f;
		for (int32 i = 0; i < GS->RecentMessages.Num(); ++i)
		{
			if (Now - GS->RecentMessageTimes[i] < 8.f)
			{
				DrawText(GS->RecentMessages[i], Yellow, 20.f, FeedY, Big);
				FeedY += 24.f;
			}
		}

		if (GS->MatchPhase == EPVVMatchPhase::Ended)
		{
			const FString Result = GS->Winner == EPVVFaction::None ? TEXT("DRAW!")
				: GS->Winner == EPVVFaction::Pirates ? TEXT("PIRATES WIN!") : TEXT("VIKINGS WIN!");
			DrawCentered(Result, SY * 0.3f, GS->Winner == EPVVFaction::Vikings ? Viking : Pirate, Big, 4.f);
		}
	}

	const APlayerController* PC = GetOwningPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;

	if (const APVVBoat* Boat = Cast<APVVBoat>(Pawn))
	{
		DrawCentered(TEXT("DRIVING  -  W/S throttle   A/D steer   E jump out"), SY - 90.f, White, Big, 1.2f);
		DrawCentered(FString::Printf(TEXT("%.0f km/h"), FMath::Abs(Boat->GetForwardSpeed()) * 0.036f), SY - 60.f, Yellow, Big, 1.2f);
		return;
	}

	const APVVCharacter* Character = Cast<APVVCharacter>(Pawn);
	if (!Character)
	{
		DrawCentered(TEXT("Press ~ and type JoinPirates or JoinVikings"), SY * 0.45f, White, Big, 1.5f);
		return;
	}

	// Who am I.
	const bool bPirate = Character->GetFaction() == EPVVFaction::Pirates;
	DrawText(bPirate ? TEXT("YOU ARE A PIRATE") : TEXT("YOU ARE A VIKING"), bPirate ? Pirate : Viking, 20.f, SY - 120.f, Big, 1.4f);
	DrawText(bPirate
		? TEXT("Goal: dodge Vikings, rescue tied-up crew, break friends out of the cell")
		: TEXT("Goal: knock Pirates down, zip-tie them, haul them to the cell"),
		White, 20.f, SY - 90.f, Small, 1.2f);

	// Balance meter.
	DrawText(TEXT("BALANCE"), White, 20.f, SY - 62.f, Small);
	DrawBar(90.f, SY - 64.f, 220.f, 16.f, Character->GetBalanceFraction(), FLinearColor(0.3f, 0.9f, 0.3f));

	// Controls.
	DrawText(TEXT("WASD move | Mouse look | Space jump | Shift sprint | Left click punch | E interact (tap / hold)"),
		FLinearColor(0.8f, 0.8f, 0.8f), 20.f, SY - 36.f, Small, 1.1f);

	// Status messages.
	switch (Character->GetCondition())
	{
	case EPVVCondition::KnockedDown:
		DrawCentered(TEXT("KNOCKED DOWN!  Get up soon... unless a Viking zip-ties you"), SY * 0.4f, FLinearColor::Red, Big, 1.6f);
		break;
	case EPVVCondition::Restrained:
		DrawCentered(TEXT("ZIP-TIED!  MASH SPACE TO BREAK FREE"), SY * 0.4f, FLinearColor::Red, Big, 1.8f);
		DrawBar(SX * 0.5f - 150.f, SY * 0.4f + 45.f, 300.f, 18.f, Character->GetStruggleProgress(), Yellow);
		break;
	case EPVVCondition::Detained:
		DrawCentered(TEXT("DETAINED.  Wait for your crew to break you out!"), SY * 0.4f, FLinearColor::Red, Big, 1.6f);
		break;
	default:
		break;
	}

	if (Character->GetCurrentBoat())
	{
		DrawCentered(TEXT("Riding as passenger  -  Space to jump out"), SY - 160.f, White, Big);
	}

	// Interaction prompt + hold ring.
	const FText& Prompt = Character->GetFocusPrompt();
	if (!Prompt.IsEmpty())
	{
		DrawCentered(FString::Printf(TEXT("[E]  %s"), *Prompt.ToString()), SY * 0.62f, Yellow, Big, 1.5f);
	}
	if (Character->GetHoldProgress() > 0.f)
	{
		DrawBar(SX * 0.5f - 120.f, SY * 0.62f + 40.f, 240.f, 14.f, Character->GetHoldProgress(), Yellow);
	}
}
