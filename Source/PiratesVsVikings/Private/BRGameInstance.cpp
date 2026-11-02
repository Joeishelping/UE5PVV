#include "BRGameInstance.h"
#include "PiratesVsVikings.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"

namespace
{
	const FName BRSessionKey(TEXT("BORDERRUN"));
}

void UBRGameInstance::Init()
{
	Super::Init();
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
