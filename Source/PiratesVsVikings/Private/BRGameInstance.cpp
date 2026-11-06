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
#include "Interfaces/OnlineIdentityInterface.h"
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
	GConfig->SetInt(LookSection, TEXT("ShirtStyle"), Look.ShirtStyle, GGameUserSettingsIni);
	GConfig->SetInt(LookSection, TEXT("PantsStyle"), Look.PantsStyle, GGameUserSettingsIni);
	GConfig->SetInt(LookSection, TEXT("Glasses"), Look.Glasses, GGameUserSettingsIni);
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
	GConfig->GetInt(LookSection, TEXT("ShirtStyle"), Look.ShirtStyle, GGameUserSettingsIni);
	GConfig->GetInt(LookSection, TEXT("PantsStyle"), Look.PantsStyle, GGameUserSettingsIni);
	GConfig->GetInt(LookSection, TEXT("Glasses"), Look.Glasses, GGameUserSettingsIni);
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
	const IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	if (IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr)
	{
		LoginHandle = Identity->AddOnLoginCompleteDelegate_Handle(0, FOnLoginCompleteDelegate::CreateUObject(this, &UBRGameInstance::OnLoginComplete));
	}
	Status(FString::Printf(TEXT("Online: %s"), *GetOnlineServiceName()));
	if (NeedsLogin())
	{
		// Epic: quietly sign back in with last time's login (no window). If that fails, the Epic window opens when you click Host / Find.
		bTriedPersistentLogin = true;
		StartLogin(TEXT("persistentauth"));
	}
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
	const IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	if (IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr)
	{
		Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginHandle);
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
		Status(TEXT("Online isn't available (Online: ") + GetOnlineServiceName() + TEXT("). Use a packaged / Standalone game, not the editor window."));
		return;
	}

	if (bIsHosting || (GetWorld() && GetWorld()->GetNetMode() == NM_ListenServer))
	{
		Status(TEXT("Already hosting this match."));
		return;
	}

	if (NeedsLogin())
	{
		LoginThen(EAfterLogin::Host);
		return;
	}

	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		bHostAfterDestroy = true;
		Sessions->DestroySession(NAME_GameSession);
		return;
	}

	bIsHosting = true;
	CreateSessionNow();
}

void UBRGameInstance::CreateSessionNow()
{
	IOnlineSessionPtr Sessions = GetSessions();
	const ULocalPlayer* Player = GetFirstGamePlayer();
	if (!Sessions || !Player)
	{
		bIsHosting = false;
		return;
	}
	const bool bLan = GetOnlineServiceName() == TEXT("NULL");

	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = 12;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bIsLANMatch = bLan;
	Settings.bIsDedicated = false;
	Settings.bUsesPresence = true;
	Settings.bAllowJoinViaPresence = true;
	Settings.bAllowInvites = true;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.Set(BRSessionKey, FString(TEXT("1")), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	Status(TEXT("Creating session..."));
	if (!Sessions->CreateSession(0, NAME_GameSession, Settings))
	{
		bIsHosting = false;
		Status(TEXT("Could not create a session."));
	}
}
void UBRGameInstance::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		bIsHosting = false;
		Status(TEXT("Creating the online game failed. Signed in? (Epic: check the Product IDs in Project Settings -> Epic Online Services.)"));
		return;
	}

	bIsHosting = true;
	Status(GetOnlineServiceName() == TEXT("EOS") ? TEXT("Hosting! Friends can find your game in FIND GAMES, or press INVITE.")
		: TEXT("Hosting! Press Shift+Tab -> right-click a friend -> Invite to Game (or type Invite)."));
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
			if (!Sessions->JoinSession(0, NAME_GameSession, InviteToJoin))
			{
				bJoinInProgress = false;
				Status(TEXT("Steam rejected the invite join request."));
			}
		}
		return;
	}

	if (bHostAfterDestroy)
	{
		bHostAfterDestroy = false;
		CreateSessionNow();
		return;
	}

	bIsHosting = false;
}

void UBRGameInstance::LeaveGame()
{
	bIsHosting = false;
	bJoinInProgress = false;

	if (IOnlineSessionPtr Sessions = GetSessions())
	{
		Sessions->DestroySession(NAME_GameSession);
	}
	const FString Map = UGameplayStatics::GetCurrentLevelName(GetWorld(), true);
	UGameplayStatics::OpenLevel(GetWorld(), FName(*Map), true);
}
void UBRGameInstance::Invite()
{
	if (NeedsLogin())
	{
		LoginThen(EAfterLogin::Invite);
		return;
	}
	const IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	IOnlineExternalUIPtr UI = Subsystem ? Subsystem->GetExternalUIInterface() : nullptr;
	if (!UI || !IsInSession() || !UI->ShowInviteUI(0, NAME_GameSession))
	{
		if (GetOnlineServiceName() != TEXT("STEAM") && GetOnlineServiceName() != TEXT("EOS"))
		{
			Status(TEXT("Not connected online (Online: ") + GetOnlineServiceName() + TEXT("). Use the packaged game, not the editor."));
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
	StartSearch(false);
}

void UBRGameInstance::BrowseGames()
{
	StartSearch(true);
}

void UBRGameInstance::StartSearch(bool bBrowse)
{
	IOnlineSessionPtr Sessions = GetSessions();
	const ULocalPlayer* Player = GetFirstGamePlayer();
	if (!Sessions || !Player)
	{
		Status(TEXT("Online isn't available (Online: ") + GetOnlineServiceName() + TEXT("). Use a packaged / Standalone game."));
		return;
	}
	if (NeedsLogin())
	{
		LoginThen(bBrowse ? EAfterLogin::Browse : EAfterLogin::Find);
		return;
	}
	if (bSearching)
	{
		return;
	}
	bBrowseSearch = bBrowse;
	bSearching = true;
	Search = MakeShared<FOnlineSessionSearch>();
	Search->MaxSearchResults = GetOnlineServiceName() == TEXT("EOS") ? 50 : 5000; // (Steam app 480 is shared with every other test game)
	Search->bIsLanQuery = GetOnlineServiceName() == TEXT("NULL");
	Search->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	Search->QuerySettings.Set(BRSessionKey, FString(TEXT("1")), EOnlineComparisonOp::Equals); // only Border Run games
	Status(bBrowse ? TEXT("Looking for games...") : TEXT("Searching for a Border Run game..."));
	Sessions->FindSessions(0, Search.ToSharedRef());
}

void UBRGameInstance::JoinFoundGame(int32 Index)
{
	IOnlineSessionPtr Sessions = GetSessions();
	if (!Sessions || !FoundResults.IsValidIndex(Index))
	{
		return;
	}
	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		PendingInvite = FoundResults[Index]; // leave our own game first, then join (OnDestroySessionComplete)
		bHostAfterDestroy = false;
		Sessions->DestroySession(NAME_GameSession);
		return;
	}
	Status(FString::Printf(TEXT("Joining %s's game..."), *FoundGames[Index].HostName));
	Sessions->JoinSession(0, NAME_GameSession, FoundResults[Index]);
}

void UBRGameInstance::OnFindSessionsComplete(bool bWasSuccessful)
{
	bSearching = false;
	IOnlineSessionPtr Sessions = GetSessions();
	if (!Sessions || !Search.IsValid())
	{
		return;
	}
	if (bBrowseSearch)
	{
		// Server browser: list every Border Run game (name, players, ping).
		FoundGames.Reset();
		FoundResults.Reset();
		for (const FOnlineSessionSearchResult& Result : Search->SearchResults)
		{
			FString Value;
			if (!Result.Session.SessionSettings.Get(BRSessionKey, Value) || Value != TEXT("1"))
			{
				continue;
			}
			FFoundGame Game;
			Game.HostName = Result.Session.OwningUserName.IsEmpty() ? FString(TEXT("Border Run game")) : Result.Session.OwningUserName;
			Game.MaxPlayers = Result.Session.SessionSettings.NumPublicConnections;
			Game.Players = FMath::Max(1, Game.MaxPlayers - Result.Session.NumOpenPublicConnections);
			Game.PingMs = Result.PingInMs;
			FoundGames.Add(Game);
			FoundResults.Add(Result);
		}
		++FoundGamesVersion;
		Status(FoundGames.Num() > 0 ? FString::Printf(TEXT("Found %d game(s) - click one to join."), FoundGames.Num())
			: FString(TEXT("No open games right now. Host one, or try again in a moment.")));
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
	Status(FString::Printf(TEXT("No Border Run game found (%d checked). Try FIND GAMES, or ask the host for an invite."), Search->SearchResults.Num()));
}

void UBRGameInstance::OnInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult)
{
	if (bJoinInProgress)
	{
		return;
	}

	IOnlineSessionPtr Sessions = GetSessions();
	if (!bWasSuccessful || !Sessions || !InviteResult.IsValid())
	{
		Status(TEXT("That invite didn't work - ask for a new one."));
		return;
	}

	bJoinInProgress = true;

	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		Status(TEXT("Invite accepted - closing your own game first..."));
		PendingInvite = InviteResult;
		bHostAfterDestroy = false;
		if (!Sessions->DestroySession(NAME_GameSession))
		{
			PendingInvite.Reset();
			bJoinInProgress = false;
			Status(TEXT("Couldn't close the current session to join the invite."));
		}
		return;
	}

	Status(TEXT("Invite accepted - joining..."));
	if (!Sessions->JoinSession(ControllerId, NAME_GameSession, InviteResult))
	{
		bJoinInProgress = false;
		Status(TEXT("Steam rejected the join request. Accept a fresh invite and try again."));
	}
}
void UBRGameInstance::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	IOnlineSessionPtr Sessions = GetSessions();
	FString Address;

	if (Result != EOnJoinSessionCompleteResult::Success || !Sessions || !Sessions->GetResolvedConnectString(SessionName, Address))
	{
		bJoinInProgress = false;
		Status(FString::Printf(TEXT("Couldn't join that game (error %d). Is the host still in the game? Same version?"), (int32)Result));
		return;
	}

	bJoinInProgress = false;

	if (APlayerController* PC = GetFirstLocalPlayerController())
	{
		PC->ClientTravel(Address, TRAVEL_Absolute);
	}
}

// -------------------------------------------------------------------------------------------
// Epic login (EOS needs you signed in before hosting / joining). Steam / LAN don't need this.

bool UBRGameInstance::IsLoggedIn() const
{
	const IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
	return Identity.IsValid() && Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn;
}

bool UBRGameInstance::NeedsLogin() const
{
	return GetOnlineServiceName() == TEXT("EOS") && !IsLoggedIn();
}

void UBRGameInstance::LoginThen(EAfterLogin Then)
{
	AfterLogin = Then;
	if (bLoginInProgress)
	{
		Status(TEXT("Signing in to Epic..."));
		return;
	}
	// The remembered login already failed: open the Epic sign-in window (free account; only needed once).
	StartLogin(bTriedPersistentLogin ? TEXT("accountportal") : TEXT("persistentauth"));
	bTriedPersistentLogin = true;
}

void UBRGameInstance::StartLogin(const FString& Type)
{
	const IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
	if (!Identity.IsValid())
	{
		return;
	}
	bLoginInProgress = true;
	LastLoginType = Type;
	Status(Type == TEXT("accountportal") ? TEXT("Sign in with your Epic account in the window that opened (only needed once)...")
		: TEXT("Signing in to Epic..."));
	FOnlineAccountCredentials Credentials;
	Credentials.Type = Type;
	Credentials.Id = TEXT("");
	Credentials.Token = TEXT("");
	Identity->Login(0, Credentials);
}

void UBRGameInstance::OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
	bLoginInProgress = false;
	if (bWasSuccessful)
	{
		const IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
		IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
		Status(FString::Printf(TEXT("Signed in to Epic as %s."), Identity.IsValid() ? *Identity->GetPlayerNickname(0) : TEXT("you")));
		RunAfterLogin();
		return;
	}
	UE_LOG(LogPVV, Warning, TEXT("[Online] Epic login failed: %s"), *Error);
	if (AfterLogin != EAfterLogin::None && LastLoginType == TEXT("persistentauth"))
	{
		// The quiet login didn't work but you clicked something: open the Epic sign-in window now.
		StartLogin(TEXT("accountportal"));
		return;
	}
	AfterLogin = EAfterLogin::None;
	Status(TEXT("Epic sign-in didn't work: ") + Error + TEXT(" (Check the IDs in Project Settings -> Epic Online Services.)"));
}

void UBRGameInstance::RunAfterLogin()
{
	const EAfterLogin Then = AfterLogin;
	AfterLogin = EAfterLogin::None;
	switch (Then)
	{
	case EAfterLogin::Host: HostSteam(); break;
	case EAfterLogin::Find: FindGame(); break;
	case EAfterLogin::Browse: BrowseGames(); break;
	case EAfterLogin::Invite: Invite(); break;
	default: break;
	}
}
