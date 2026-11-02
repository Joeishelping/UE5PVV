#include "BRPlayerController.h"
#include "BRCharacter.h"
#include "BRDrone.h"
#include "BRGameInstance.h"
#include "BRGameMode.h"
#include "BRMenuWidget.h"
#include "Blueprint/UserWidget.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"
#include "BRGameState.h"
#include "BRPlayerState.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

void ABRPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}
	UBRGameInstance* GI = GetGameInstance<UBRGameInstance>();
	if (GetNetMode() != NM_Standalone)
	{
		ToggleSpeaking(bMicOn); // open mic for proximity voice chat
	}
	// Wait a moment so the automatic team balancing has run, then switch to the chosen side + send your look.
	GetWorldTimerManager().SetTimer(PreferredTeamTimer, this, &ABRPlayerController::ApplyPreferredTeam, 1.f, false);
	if (GI && !GI->bMenuDone && GetNetMode() == NM_Standalone)
	{
		ShowMenu();
	}
}

void ABRPlayerController::ApplyPreferredTeam()
{
	const UBRGameInstance* GI = GetGameInstance<UBRGameInstance>();
	const ABRPlayerState* PS = GetPlayerState<ABRPlayerState>();
	if (GI && GI->PreferredTeam != EPVVFaction::None && (!PS || PS->GetFaction() != GI->PreferredTeam))
	{
		RequestFaction(GI->PreferredTeam);
	}
	if (GI)
	{
		SendLook(GI->Look);
	}
}

void ABRPlayerController::SendLook(const FBRLook& Look)
{
	if (ABRCharacter* Me = Cast<ABRCharacter>(GetPawn()))
	{
		Me->PreviewLook(Look); // instant on your own screen
	}
	Server_SetLook(Look);
}

void ABRPlayerController::Server_SetLook_Implementation(const FBRLook& NewLook)
{
	FBRLook Look = NewLook;
	Look.SkinTone = FMath::Clamp(Look.SkinTone, 0.f, 1.f);
	Look.ShirtColor = FMath::Clamp(Look.ShirtColor, 0.f, 1.f);
	Look.PantsColor = FMath::Clamp(Look.PantsColor, 0.f, 1.f);
	Look.HeadSize = FMath::Clamp(Look.HeadSize, 0.f, 1.f);
	Look.NoseSize = FMath::Clamp(Look.NoseSize, 0.f, 1.f);
	Look.LipSize = FMath::Clamp(Look.LipSize, 0.f, 1.f);
	Look.EarSize = FMath::Clamp(Look.EarSize, 0.f, 1.f);
	Look.HairColor = FMath::Clamp(Look.HairColor, 0.f, 1.f);
	Look.HairStyle = FMath::Clamp(Look.HairStyle, 0, 64);
	Look.FacialHair = FMath::Clamp(Look.FacialHair, 0, 64);
	if (ABRPlayerState* PS = GetPlayerState<ABRPlayerState>())
	{
		PS->Look = Look;
		PS->bHasLook = true;
	}
	if (ABRCharacter* Me = Cast<ABRCharacter>(GetPawn()))
	{
		Me->SetLook(Look);
	}
}

void ABRPlayerController::ToggleMic()
{
	bMicOn = !bMicOn;
	ToggleSpeaking(bMicOn);
	LocalMessage = bMicOn ? NSLOCTEXT("BR", "MicOn", "Mic ON (people near you can hear you) - K to mute") : NSLOCTEXT("BR", "MicOff", "Mic MUTED - K to unmute");
	LocalMessageTime = GetWorld()->GetTimeSeconds();
}

void ABRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::M, IE_Pressed, this, &ABRPlayerController::ToggleMenu);
		InputComponent->BindKey(EKeys::K, IE_Pressed, this, &ABRPlayerController::ToggleMic);
	}
}

void ABRPlayerController::ShowMenu()
{
	if (Menu || !IsLocalController())
	{
		return;
	}
	Menu = CreateWidget<UBRMenuWidget>(this, UBRMenuWidget::StaticClass());
	if (!Menu)
	{
		return;
	}
	Menu->AddToViewport(50);
	SetShowMouseCursor(true);
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
}

void ABRPlayerController::HideMenu()
{
	if (Menu)
	{
		Menu->RemoveFromParent();
		Menu = nullptr;
	}
	SetShowMouseCursor(false);
	SetInputMode(FInputModeGameOnly());
	ResetIgnoreMoveInput();
	ResetIgnoreLookInput();
	if (UBRGameInstance* GI = GetGameInstance<UBRGameInstance>())
	{
		GI->bMenuDone = true;
	}
}

void ABRPlayerController::ToggleMenu()
{
	if (Menu)
	{
		HideMenu();
	}
	else
	{
		ShowMenu();
	}
}

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
