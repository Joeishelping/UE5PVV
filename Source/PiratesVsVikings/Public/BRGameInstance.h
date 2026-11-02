#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "PVVTypes.h"
#include "BRGameInstance.generated.h"

class FOnlineSessionSearch;

/**
 * Steam friends multiplayer (Unreal's built-in Online Subsystem Steam + sessions).
 *  1. Host:   press ~ and type  HostSteam
 *  2. Invite: Shift+Tab (Steam overlay) -> friend -> "Invite to Game", or type  Invite
 *  3. Friend: accepts the Steam invite (or right-clicks you -> "Join Game") and drops into the match.
 *  Fallback:  FindGame  joins the first Border Run session it finds.
 * Needs Steam running on both PCs and a Standalone/packaged build (Steam doesn't run in Play-In-Editor).
 */
UCLASS()
class PIRATESVSVIKINGS_API UBRGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	UFUNCTION(Exec, BlueprintCallable, Category = "BR|Online") void HostSteam();
	UFUNCTION(Exec, BlueprintCallable, Category = "BR|Online") void Invite();
	UFUNCTION(Exec, BlueprintCallable, Category = "BR|Online") void FindGame();
	UFUNCTION(Exec, BlueprintCallable, Category = "BR|Online") void LeaveGame();

	/** "Steam" when friends invites are available, "NULL" otherwise (LAN/IP only). */
	UFUNCTION(BlueprintPure, Category = "BR|Online") FString GetOnlineServiceName() const;
	bool IsInSession() const;

	/** Short status line for the HUD. */
	FString StatusText;

	/** Team picked in the main menu (kept across map reloads / joining). None = let the game balance teams. */
	EPVVFaction PreferredTeam = EPVVFaction::None;
	/** The startup menu only shows once per launch. */
	bool bMenuDone = false;

private:
	IOnlineSessionPtr GetSessions() const;
	void Status(const FString& Text);
	void CreateSessionNow();

	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void OnFindSessionsComplete(bool bWasSuccessful);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);

	TSharedPtr<FOnlineSessionSearch> Search;
	FDelegateHandle CreateHandle, DestroyHandle, FindHandle, JoinHandle, InviteHandle;
	bool bHostAfterDestroy = false;
	/** Invite accepted while we still had our own session: join this once that session is gone. */
	TOptional<FOnlineSessionSearchResult> PendingInvite;
};
