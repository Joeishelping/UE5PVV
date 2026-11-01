#include "BRPlayerController.h"
#include "BRCharacter.h"
#include "BRDrone.h"
#include "BRGameMode.h"
#include "BRGameState.h"
#include "BRPlayerState.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

void ABRPlayerController::Client_Notify_Implementation(const FText& Message)
{
	LocalMessage = Message;
	LocalMessageTime = GetWorld()->GetTimeSeconds();
}

void ABRPlayerController::Client_HitMarker_Implementation(bool bLethal)
{
	HitMarkerTime = GetWorld()->GetTimeSeconds();
	bHitMarkerLethal = bLethal;
}

void ABRPlayerController::Host()
{
	const FString Map = UGameplayStatics::GetCurrentLevelName(this, true);
	UGameplayStatics::OpenLevel(this, FName(*Map), true, TEXT("listen"));
}

void ABRPlayerController::Join(const FString& Address)
{
	if (!Address.IsEmpty())
	{
		ClientTravel(Address.TrimStartAndEnd(), TRAVEL_Absolute);
	}
}

void ABRPlayerController::JoinRunners() { RequestFaction(BR::Runners); }
void ABRPlayerController::JoinWardens() { RequestFaction(BR::Wardens); }

void ABRPlayerController::SkipPhase() { Server_SkipPhase(); }
void ABRPlayerController::GiveMoney(int32 Amount) { Server_GiveMoney(Amount); }
void ABRPlayerController::SetTime(float Hours) { Server_SetTime(Hours); }
void ABRPlayerController::SetWeather(int32 Weather) { Server_SetWeather(Weather); }

void ABRPlayerController::Server_SkipPhase_Implementation()
{
	if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
	{
		GM->AdvancePhase();
	}
}

void ABRPlayerController::Server_GiveMoney_Implementation(int32 Amount)
{
	if (ABRPlayerState* PS = GetPlayerState<ABRPlayerState>())
	{
		PS->AddMoney(FMath::Clamp(Amount, -100000, 100000));
	}
}

void ABRPlayerController::Server_SetTime_Implementation(float Hours)
{
	if (ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>())
	{
		GS->TimeOfDay = FMath::Fmod(FMath::Max(0.f, Hours), 24.f);
	}
}

void ABRPlayerController::Server_SetWeather_Implementation(int32 Weather)
{
	if (ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>())
	{
		GS->Weather = static_cast<EBRWeather>(FMath::Clamp(Weather, 0, 2));
	}
}

void ABRPlayerController::PawnLeavingGame()
{
	// Leaving while flying a drone: return to the body first so the drone is what gets cleaned up.
	if (ABRDrone* Drone = Cast<ABRDrone>(GetPawn()))
	{
		APawn* Body = Drone->Pilot;
		UnPossess();
		Drone->Destroy();
		if (Body)
		{
			Possess(Body);
		}
	}
	Super::PawnLeavingGame();
}
