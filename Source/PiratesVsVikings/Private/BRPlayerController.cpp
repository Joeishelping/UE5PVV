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
#include "EngineUtils.h"
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
	Look.HatChoice = FMath::Clamp(Look.HatChoice, -1, static_cast<int32>(EBRHat::LuchaMask));
	bool bHatChanged = false;
	if (ABRPlayerState* PS = GetPlayerState<ABRPlayerState>())
	{
		bHatChanged = !PS->bHasLook || PS->Look.HatChoice != Look.HatChoice;
		PS->Look = Look;
		PS->bHasLook = true;
	}
	// Changing hats in the menu only works before the Line opens (no mid-fight swaps).
	const ABRGameState* HatGS = GetWorld()->GetGameState<ABRGameState>();
	if (bHatChanged && (!HatGS || HatGS->Phase == EBRPhase::Prep))
	{
		if (ABRCharacter* Hatter = Cast<ABRCharacter>(GetPawn()))
		{
			Hatter->SetHat(Look.HatChoice >= 0 ? static_cast<EBRHat>(Look.HatChoice) : Hatter->GetDefaultHat());
		}
	}
	if (ABRCharacter* Me = Cast<ABRCharacter>(GetPawn()))
	{
		Me->SetLook(Look);
	}
}

void ABRPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	const ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>();
	if (!IsLocalController() || !GS)
	{
		return;
	}
	// Holding Pen: watch someone else until you're out (N = next).
	const APVVCharacter* MyBody = Cast<APVVCharacter>(GetPawn());
	const bool bInPen = MyBody && MyBody->GetCondition() == EPVVCondition::Detained;
	if (!bInPen && SpectateTarget.IsValid())
	{
		SpectateTarget = nullptr;
		if (GetPawn())
		{
			SetViewTargetWithBlend(GetPawn(), 0.4f);
		}
	}
	else if (bInPen && SpectateTarget.IsValid() && GetViewTarget() != SpectateTarget.Get())
	{
		SetViewTargetWithBlend(SpectateTarget.Get(), 0.4f);
	}
	if (GS->Phase != EBRPhase::Ended)
	{
		bBankedRound = false;
	}
	else if (!bBankedRound)
	{
		bBankedRound = true;
		if (UBRGameInstance* GI = GetGameInstance<UBRGameInstance>())
		{
			GI->BankRoundStats(GetPlayerState<ABRPlayerState>()); // hat unlock progress
		}
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
		InputComponent->BindKey(EKeys::Y, IE_Pressed, this, &ABRPlayerController::ToggleReady);
		InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ABRPlayerController::ScoreboardOn);
		InputComponent->BindKey(EKeys::Tab, IE_Released, this, &ABRPlayerController::ScoreboardOff);
		InputComponent->BindKey(EKeys::N, IE_Pressed, this, &ABRPlayerController::SpectateNext);
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

void ABRPlayerController::AddBots(int32 Count) { Server_AddBots(FMath::Clamp(Count, 1, 12), 0); }

void ABRPlayerController::AddBot(const FString& Team)
{
	const bool bWarden = Team.Contains(TEXT("ward"), ESearchCase::IgnoreCase) || Team.Contains(TEXT("patrol"), ESearchCase::IgnoreCase)
		|| Team.Contains(TEXT("bp"), ESearchCase::IgnoreCase) || Team.Contains(TEXT("cop"), ESearchCase::IgnoreCase);
	Server_AddBots(1, bWarden ? 2 : 1);
}

void ABRPlayerController::RemoveBots() { Server_RemoveBots(); }

void ABRPlayerController::Server_AddBots_Implementation(int32 Count, uint8 Team)
{
	ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>();
	if (!GM || !IsLocalController())
	{
		return; // only the host can add bots
	}
	const EPVVFaction Faction = Team == 1 ? BR::Runners : (Team == 2 ? BR::Wardens : EPVVFaction::None);
	for (int32 i = 0; i < FMath::Clamp(Count, 1, 12); ++i)
	{
		GM->AddBot(Faction);
	}
}

void ABRPlayerController::Server_RemoveBots_Implementation()
{
	if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>(); GM && IsLocalController())
	{
		GM->RemoveBots();
	}
}

void ABRPlayerController::ToggleReady()
{
	const ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>();
	if (GS && GS->bLobby)
	{
		Server_ToggleReady();
	}
}

void ABRPlayerController::Server_ToggleReady_Implementation()
{
	if (ABRPlayerState* PS = GetPlayerState<ABRPlayerState>())
	{
		PS->bReady = !PS->bReady;
		PS->ForceNetUpdate();
	}
}

void ABRPlayerController::SpectateNext()
{
	const APVVCharacter* MyBody = Cast<APVVCharacter>(GetPawn());
	if (!MyBody || MyBody->GetCondition() != EPVVCondition::Detained)
	{
		return;
	}
	TArray<APVVCharacter*> People;
	for (TActorIterator<APVVCharacter> It(GetWorld()); It; ++It)
	{
		if (*It != MyBody && It->GetPVVPlayerState() && It->GetCondition() != EPVVCondition::Detained)
		{
			People.Add(*It);
		}
	}
	if (People.Num() == 0)
	{
		SpectateTarget = nullptr;
		SetViewTargetWithBlend(GetPawn(), 0.3f);
		return;
	}
	SpectateIndex = (SpectateIndex + 1) % People.Num();
	SpectateTarget = People[SpectateIndex];
	SetViewTargetWithBlend(People[SpectateIndex], 0.4f);
	LocalMessage = FText::FromString(FString::Printf(TEXT("Watching %s  (N = next)"), *People[SpectateIndex]->GetPlayerDisplayName()));
	LocalMessageTime = GetWorld()->GetTimeSeconds();
}

void ABRPlayerController::SetHostSettings(const FBRHostSettings& Settings)
{
	if (GetNetMode() == NM_Client)
	{
		return; // only the host
	}
	if (UBRGameInstance* GI = GetGameInstance<UBRGameInstance>())
	{
		GI->Host = Settings;
		GI->Host.bSet = true;
		GI->SaveHost();
	}
	if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
	{
		GM->ApplyHostSettings(Settings);
	}
}
