#include "BRGameInstance.h"
#include "PiratesVsVikings.h"
#include "BRPlayerState.h"
#include "BRCharacter.h"
#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"

namespace
{
	const FName BRSessionKey(TEXT("BORDERRUN"));
}

static const TCHAR* LookSection = TEXT("BorderRun.Look");

void UBRGameInstance::SaveLook() const
{
	if (!GConfig)
	{
		return;
	}
	GConfig->SetFloat(LookSection, TEXT("SkinTone"), Look.SkinTone, GGameUserSettingsIni);
	GConfig->SetFloat(LookSection, TEXT("ShirtColor"), Look.ShirtColor, GGameUserSettingsIni);
	GConfig->SetFloat(LookSection, TEXT("PantsColor"), Look.PantsColor, GGameUserSettingsIni);
	GConfig->SetBool(LookSection, TEXT("Hat"), Look.bHat, GGameUserSettingsIni);
	GConfig->SetFloat(LookSection, TEXT("HeadSize"), Look.HeadSize, GGameUserSettingsIni);
	GConfig->SetFloat(LookSection, TEXT("NoseSize"), Look.NoseSize, GGameUserSettingsIni);
	GConfig->SetFloat(LookSection, TEXT("LipSize"), Look.LipSize, GGameUserSettingsIni);
	GConfig->SetFloat(LookSection, TEXT("EarSize"), Look.EarSize, GGameUserSettingsIni);
	for (const FBRFaceSlider& S : BR::FaceSliders())
	{
		GConfig->SetFloat(LookSection, S.BigKey, Look.*S.Field, GGameUserSettingsIni);
	}
	GConfig->SetInt(LookSection, TEXT("HairStyle"), Look.HairStyle, GGameUserSettingsIni);
	GConfig->SetFloat(LookSection, TEXT("HairColor"), Look.HairColor, GGameUserSettingsIni);
	GConfig->SetInt(LookSection, TEXT("FacialHair"), Look.FacialHair, GGameUserSettingsIni);
	GConfig->SetInt(LookSection, TEXT("HatChoice"), Look.HatChoice, GGameUserSettingsIni);
	GConfig->SetInt(LookSection, TEXT("LifetimeTacos"), LifetimeTacos, GGameUserSettingsIni);
	GConfig->SetInt(LookSection, TEXT("LifetimeKnockdowns"), LifetimeKnockdowns, GGameUserSettingsIni);
	GConfig->SetInt(LookSection, TEXT("LifetimeCannonShots"), LifetimeCannonShots, GGameUserSettingsIni);
	GConfig->SetInt(LookSection, TEXT("LifetimeCatches"), LifetimeCatches, GGameUserSettingsIni);
	GConfig->SetInt(LookSection, TEXT("LifetimeLuchaWins"), LifetimeLuchaWins, GGameUserSettingsIni);
	GConfig->SetInt(LookSection, TEXT("RoundsPlayed"), RoundsPlayed, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void UBRGameInstance::LoadLook()
{
	if (!GConfig)
	{
		return;
	}
	GConfig->GetFloat(LookSection, TEXT("SkinTone"), Look.SkinTone, GGameUserSettingsIni);
	GConfig->GetFloat(LookSection, TEXT("ShirtColor"), Look.ShirtColor, GGameUserSettingsIni);
	GConfig->GetFloat(LookSection, TEXT("PantsColor"), Look.PantsColor, GGameUserSettingsIni);
	GConfig->GetBool(LookSection, TEXT("Hat"), Look.bHat, GGameUserSettingsIni);
	GConfig->GetFloat(LookSection, TEXT("HeadSize"), Look.HeadSize, GGameUserSettingsIni);
	GConfig->GetFloat(LookSection, TEXT("NoseSize"), Look.NoseSize, GGameUserSettingsIni);
	GConfig->GetFloat(LookSection, TEXT("LipSize"), Look.LipSize, GGameUserSettingsIni);
	GConfig->GetFloat(LookSection, TEXT("EarSize"), Look.EarSize, GGameUserSettingsIni);
	for (const FBRFaceSlider& S : BR::FaceSliders())
	{
		GConfig->GetFloat(LookSection, S.BigKey, Look.*S.Field, GGameUserSettingsIni);
	}
	GConfig->GetInt(LookSection, TEXT("HairStyle"), Look.HairStyle, GGameUserSettingsIni);
	GConfig->GetFloat(LookSection, TEXT("HairColor"), Look.HairColor, GGameUserSettingsIni);
	GConfig->GetInt(LookSection, TEXT("FacialHair"), Look.FacialHair, GGameUserSettingsIni);
	GConfig->GetInt(LookSection, TEXT("HatChoice"), Look.HatChoice, GGameUserSettingsIni);
	GConfig->GetInt(LookSection, TEXT("LifetimeTacos"), LifetimeTacos, GGameUserSettingsIni);
	GConfig->GetInt(LookSection, TEXT("LifetimeKnockdowns"), LifetimeKnockdowns, GGameUserSettingsIni);
	GConfig->GetInt(LookSection, TEXT("LifetimeCannonShots"), LifetimeCannonShots, GGameUserSettingsIni);
	GConfig->GetInt(LookSection, TEXT("LifetimeCatches"), LifetimeCatches, GGameUserSettingsIni);
	GConfig->GetInt(LookSection, TEXT("LifetimeLuchaWins"), LifetimeLuchaWins, GGameUserSettingsIni);
	GConfig->GetInt(LookSection, TEXT("RoundsPlayed"), RoundsPlayed, GGameUserSettingsIni);
	GConfig->GetBool(LookSection, TEXT("HostSet"), Host.bSet, GGameUserSettingsIni);
	GConfig->GetInt(LookSection, TEXT("HostPrep"), Host.PrepSeconds, GGameUserSettingsIni);
	GConfig->GetInt(LookSection, TEXT("HostInvasion"), Host.InvasionSeconds, GGameUserSettingsIni);
	GConfig->GetBool(LookSection, TEXT("HostLobby"), Host.bLobby, GGameUserSettingsIni);
	GConfig->GetBool(LookSection, TEXT("HostBestOf3"), Host.bBestOf3, GGameUserSettingsIni);
	GConfig->GetBool(LookSection, TEXT("HostBots"), Host.bFillBots, GGameUserSettingsIni);
	GConfig->GetInt(LookSection, TEXT("HostBotRunners"), Host.BotRunners, GGameUserSettingsIni);
	GConfig->GetInt(LookSection, TEXT("HostBotWardens"), Host.BotWardens, GGameUserSettingsIni);
	if (Look.HatChoice >= 0 && !IsHatUnlocked(static_cast<EBRHat>(Look.HatChoice)))
	{
		Look.HatChoice = -1;
	}
}

void UBRGameInstance::SaveHost() const
{
	if (!GConfig)
	{
		return;
	}
	GConfig->SetBool(LookSection, TEXT("HostSet"), Host.bSet, GGameUserSettingsIni);
	GConfig->SetInt(LookSection, TEXT("HostPrep"), Host.PrepSeconds, GGameUserSettingsIni);
	GConfig->SetInt(LookSection, TEXT("HostInvasion"), Host.InvasionSeconds, GGameUserSettingsIni);
	GConfig->SetBool(LookSection, TEXT("HostLobby"), Host.bLobby, GGameUserSettingsIni);
	GConfig->SetBool(LookSection, TEXT("HostBestOf3"), Host.bBestOf3, GGameUserSettingsIni);
	GConfig->SetBool(LookSection, TEXT("HostBots"), Host.bFillBots, GGameUserSettingsIni);
	GConfig->SetInt(LookSection, TEXT("HostBotRunners"), Host.BotRunners, GGameUserSettingsIni);
	GConfig->SetInt(LookSection, TEXT("HostBotWardens"), Host.BotWardens, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void UBRGameInstance::BankRoundStats(const ABRPlayerState* PS)
{
	++RoundsPlayed;
	if (!PS)
	{
		return;
	}
	TArray<EBRHat> Before;
	for (int32 i = 1; i <= static_cast<int32>(EBRHat::LuchaMask); ++i)
	{
		if (!IsHatUnlocked(static_cast<EBRHat>(i)))
		{
			Before.Add(static_cast<EBRHat>(i));
		}
	}
	LifetimeTacos += PS->StatTacos;
	LifetimeKnockdowns += PS->StatKnockdowns;
	LifetimeCannonShots += PS->StatCannonShots;
	LifetimeCatches += PS->StatCatches;
	LifetimeLuchaWins += PS->StatLuchaWins;
	SaveLook();
	for (EBRHat Hat : Before)
	{
		if (IsHatUnlocked(Hat) && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 12.f, FColor::Yellow,
				FString::Printf(TEXT("UNLOCKED: %s (%s). Pick it in Customize."), *BR::HatName(Hat), *BR::HatSkill(Hat)));
		}
	}
}

bool UBRGameInstance::IsHatUnlocked(EBRHat Hat) const
{
	switch (Hat)
	{
	case EBRHat::Sombrero:     return LifetimeTacos >= 30;
	case EBRHat::HardHat:      return LifetimeKnockdowns >= 25;
	case EBRHat::CannonHelmet: return LifetimeCannonShots >= 10;
	case EBRHat::LuchaMask:    return LifetimeLuchaWins >= 1;
	default:                   return true;
	}
}

FString UBRGameInstance::HatUnlockText(EBRHat Hat) const
{
	if (IsHatUnlocked(Hat))
	{
		return FString();
	}
	switch (Hat)
	{
	case EBRHat::Sombrero:     return FString::Printf(TEXT("Eat 30 tacos (%d/30)"), LifetimeTacos);
	case EBRHat::HardHat:      return FString::Printf(TEXT("Knock out 25 people (%d/25)"), LifetimeKnockdowns);
	case EBRHat::CannonHelmet: return FString::Printf(TEXT("Get shot from the cannon 10 times (%d/10)"), LifetimeCannonShots);
	case EBRHat::LuchaMask:    return TEXT("Beat the luchador in the ring");
	default:                   return FString();
	}
}

void UBRGameInstance::SaveOptions() const
{
	if (!GConfig)
	{
		return;
	}
	GConfig->SetFloat(LookSection, TEXT("OptSensitivity"), FPVVPlayerOptions::LookSensitivity, GGameUserSettingsIni);
	GConfig->SetBool(LookSection, TEXT("OptInvert"), FPVVPlayerOptions::bInvertLook, GGameUserSettingsIni);
	GConfig->SetFloat(LookSection, TEXT("OptFOV"), FPVVPlayerOptions::FieldOfView, GGameUserSettingsIni);
	GConfig->SetFloat(LookSection, TEXT("OptMaster"), FPVVPlayerOptions::MasterVolume, GGameUserSettingsIni);
	GConfig->SetFloat(LookSection, TEXT("OptMusic"), FPVVPlayerOptions::MusicVolume, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void UBRGameInstance::LoadOptions()
{
	if (!GConfig)
	{
		return;
	}
	GConfig->GetFloat(LookSection, TEXT("OptSensitivity"), FPVVPlayerOptions::LookSensitivity, GGameUserSettingsIni);
	GConfig->GetBool(LookSection, TEXT("OptInvert"), FPVVPlayerOptions::bInvertLook, GGameUserSettingsIni);
	GConfig->GetFloat(LookSection, TEXT("OptFOV"), FPVVPlayerOptions::FieldOfView, GGameUserSettingsIni);
	GConfig->GetFloat(LookSection, TEXT("OptMaster"), FPVVPlayerOptions::MasterVolume, GGameUserSettingsIni);
	GConfig->GetFloat(LookSection, TEXT("OptMusic"), FPVVPlayerOptions::MusicVolume, GGameUserSettingsIni);
	FPVVPlayerOptions::LookSensitivity = FMath::Clamp(FPVVPlayerOptions::LookSensitivity, 0.1f, 4.f);
	FPVVPlayerOptions::FieldOfView = FMath::Clamp(FPVVPlayerOptions::FieldOfView, 60.f, 120.f);
}

void UBRGameInstance::ApplyOptions()
{
	if (GEngine)
	{
		if (FAudioDeviceHandle Device = GEngine->GetMainAudioDevice())
		{
			Device->SetTransientPrimaryVolume(FMath::Clamp(FPVVPlayerOptions::MasterVolume, 0.f, 1.f));
		}
	}
	if (UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (const APlayerController* PC = It->Get(); PC && PC->IsLocalController())
			{
				if (ABRCharacter* Me = Cast<ABRCharacter>(PC->GetPawn()))
				{
					Me->ApplyCameraOptions();
				}
			}
		}
	}
}

void UBRGameInstance::Init()
{
	Super::Init();
	LoadLook();
	LoadOptions();
	ApplyOptions();
	if (IOnlineSessionPtr Sessions = GetSessions())
	{
		CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &UBRGameInstance::OnCreateSessionComplete));
		DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this, &UBRGameInstance::OnDestroySessionComplete));
		FindHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(this, &UBRGameInstance::OnFindSessionsComplete));
		JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &UBRGameInstance::OnJoinSessionComplete));
		// Fires when you accept a Steam invite (or "Join Game" from the friends list) - even from the desktop.
		InviteHandle = Sessions->AddOnSessionUserInviteAcceptedDelegate_Handle(FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &UBRGameInstance::OnInviteAccepted));
	}
	Status(FString::Printf(TEXT("Online: %s"), *GetOnlineServiceName()));
}

void UBRGameInstance::Shutdown()
{
	if (IOnlineSessionPtr Sessions = GetSessions())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
		Sessions->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteHandle);
	}
	Super::Shutdown();
}

IOnlineSessionPtr UBRGameInstance::GetSessions() const
{
	const IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	return Subsystem ? Subsystem->GetSessionInterface() : nullptr;
}

FString UBRGameInstance::GetOnlineServiceName() const
{
	const IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	return Subsystem ? Subsystem->GetSubsystemName().ToString() : TEXT("none");
}

bool UBRGameInstance::IsInSession() const
{
	IOnlineSessionPtr Sessions = GetSessions();
	return Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession) != nullptr;
}

void UBRGameInstance::Status(const FString& Text)
{
	StatusText = Text;
	UE_LOG(LogPVV, Log, TEXT("[Online] %s"), *Text);
	if (APlayerController* PC = GetFirstLocalPlayerController())
	{
		PC->ClientMessage(Text);
	}
}

// -------------------------------------------------------------------------------------------
// Host

void UBRGameInstance::HostSteam()
{
	IOnlineSessionPtr Sessions = GetSessions();
	if (!Sessions)
	{
		Status(TEXT("No online subsystem - is Steam running? Use Host + Radmin instead."));
		return;
	}
	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		bHostAfterDestroy = true;
		Sessions->DestroySession(NAME_GameSession);
		return;
	}
	CreateSessionNow();
}

void UBRGameInstance::CreateSessionNow()
{
	IOnlineSessionPtr Sessions = GetSessions();
	const ULocalPlayer* Player = GetFirstGamePlayer();
	if (!Sessions || !Player)
	{
		return;
	}
	const bool bLan = GetOnlineServiceName() == TEXT("NULL");

	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = 12;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bIsLANMatch = bLan;
	Settings.bUsesPresence = true;
	Settings.bAllowJoinViaPresence = true;
	Settings.bAllowInvites = true;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.Set(BRSessionKey, FString(TEXT("1")), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	Status(TEXT("Creating session..."));
	if (!Sessions->CreateSession(0, NAME_GameSession, Settings))
	{
		Status(TEXT("Could not create a session."));
	}
}

void UBRGameInstance::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		Status(TEXT("Creating the session failed. Is Steam running and logged in?"));
		return;
	}
	Status(TEXT("Hosting! Press Shift+Tab -> right-click a friend -> Invite to Game (or type Invite)."));
	const FString Map = UGameplayStatics::GetCurrentLevelName(GetWorld(), true);
	UGameplayStatics::OpenLevel(GetWorld(), FName(*Map), true, TEXT("listen"));
}

void UBRGameInstance::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (PendingInvite.IsSet())
	{
		const FOnlineSessionSearchResult InviteToJoin = PendingInvite.GetValue();
		PendingInvite.Reset();
		if (IOnlineSessionPtr Sessions = GetSessions())
		{
			Status(TEXT("Invite accepted - joining..."));
			Sessions->JoinSession(0, NAME_GameSession, InviteToJoin);
		}
		return;
	}
	if (bHostAfterDestroy)
	{
		bHostAfterDestroy = false;
		CreateSessionNow();
	}
}

void UBRGameInstance::LeaveGame()
{
	if (IOnlineSessionPtr Sessions = GetSessions())
	{
		Sessions->DestroySession(NAME_GameSession);
	}
	const FString Map = UGameplayStatics::GetCurrentLevelName(GetWorld(), true);
	UGameplayStatics::OpenLevel(GetWorld(), FName(*Map), true);
}

void UBRGameInstance::Invite()
{
	const IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	IOnlineExternalUIPtr UI = Subsystem ? Subsystem->GetExternalUIInterface() : nullptr;
	if (!UI || !IsInSession() || !UI->ShowInviteUI(0, NAME_GameSession))
	{
		if (GetOnlineServiceName() != TEXT("STEAM"))
		{
			Status(TEXT("Steam is NOT connected (Online: ") + GetOnlineServiceName() + TEXT("). Open Steam first, use the packaged Development .exe, not the editor."));
		}
		else if (!IsInSession())
		{
			Status(TEXT("Click HOST GAME (invite Steam friends) first, then press M and Invite."));
		}
		else
		{
			Status(TEXT("Overlay blocked. In the Steam app: Friends list -> right-click your friend -> Invite to Game."));
		}
	}
}

// -------------------------------------------------------------------------------------------
// Join

void UBRGameInstance::FindGame()
{
	IOnlineSessionPtr Sessions = GetSessions();
	const ULocalPlayer* Player = GetFirstGamePlayer();
	if (!Sessions || !Player)
	{
		Status(TEXT("No online subsystem - is Steam running?"));
		return;
	}
	Search = MakeShared<FOnlineSessionSearch>();
	Search->MaxSearchResults = 5000; // app 480 is shared with every other test game
	Search->bIsLanQuery = GetOnlineServiceName() == TEXT("NULL");
	Search->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);
	Status(TEXT("Searching for a Border Run game..."));
	Sessions->FindSessions(0, Search.ToSharedRef());
}

void UBRGameInstance::OnFindSessionsComplete(bool bWasSuccessful)
{
	IOnlineSessionPtr Sessions = GetSessions();
	if (!Sessions || !Search.IsValid())
	{
		return;
	}
	for (const FOnlineSessionSearchResult& Result : Search->SearchResults)
	{
		FString Value;
		if (Result.Session.SessionSettings.Get(BRSessionKey, Value) && Value == TEXT("1"))
		{
			Status(FString::Printf(TEXT("Found %s's game - joining..."), *Result.Session.OwningUserName));
			Sessions->JoinSession(0, NAME_GameSession, Result);
			return;
		}
	}
	Status(FString::Printf(TEXT("No Border Run game found (%d sessions checked). Ask the host for an invite instead."), Search->SearchResults.Num()));
}

void UBRGameInstance::OnInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult)
{
	IOnlineSessionPtr Sessions = GetSessions();
	if (!bWasSuccessful || !Sessions || !InviteResult.IsValid())
	{
		Status(TEXT("That invite didn't work - ask for a new one."));
		return;
	}
	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		// We were hosting our own game: close it first, then join (in OnDestroySessionComplete).
		Status(TEXT("Invite accepted - closing your own game first..."));
		PendingInvite = InviteResult;
		bHostAfterDestroy = false;
		Sessions->DestroySession(NAME_GameSession);
		return;
	}
	Status(TEXT("Invite accepted - joining..."));
	Sessions->JoinSession(ControllerId, NAME_GameSession, InviteResult);
}

void UBRGameInstance::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	IOnlineSessionPtr Sessions = GetSessions();
	FString Address;
	if (Result != EOnJoinSessionCompleteResult::Success || !Sessions || !Sessions->GetResolvedConnectString(SessionName, Address))
	{
		Status(FString::Printf(TEXT("Couldn't join that game (error %d). Is the host still in the game? Same version?"), (int32)Result));
		return;
	}
	if (APlayerController* PC = GetFirstLocalPlayerController())
	{
		PC->ClientTravel(Address, TRAVEL_Absolute);
	}
}
