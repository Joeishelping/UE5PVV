#pragma once

#include "CoreMinimal.h"
#include "PVVPlayerController.h"
#include "BRPlayerController.generated.h"

UCLASS()
class PIRATESVSVIKINGS_API ABRPlayerController : public APVVPlayerController
{
	GENERATED_BODY()

public:
	/** Short message in the middle of this player's screen. */
	UFUNCTION(Client, Reliable)
	void Client_Notify(const FText& Message);

	FText LocalMessage;
	float LocalMessageTime = -100.f;

	// --- Multiplayer (press ~ to open the console) -------------------------------------
	/** Reopen this map as a listen server so friends can join (e.g. over Radmin VPN). */
	UFUNCTION(Exec) void Host();
	/** Connect to a host, e.g. "Join 26.12.34.56". */
	UFUNCTION(Exec) void Join(const FString& Address);

	/** Switch teams. */
	UFUNCTION(Exec) void JoinRunners();
	UFUNCTION(Exec) void JoinWardens();

	// --- Test commands -----------------------------------------------------------------
	/** Jump to the next phase (Prep -> Invasion -> End). */
	UFUNCTION(Exec) void SkipPhase();
	/** Give yourself money, e.g. "GiveMoney 500". */
	UFUNCTION(Exec) void GiveMoney(int32 Amount);
	/** Set the time of day in hours, e.g. "SetTime 22". */
	UFUNCTION(Exec) void SetTime(float Hours);
	/** 0 = clear, 1 = rain, 2 = sandstorm. */
	UFUNCTION(Exec) void SetWeather(int32 Weather);

	virtual void PawnLeavingGame() override;

protected:
	UFUNCTION(Server, Reliable) void Server_SkipPhase();
	UFUNCTION(Server, Reliable) void Server_GiveMoney(int32 Amount);
	UFUNCTION(Server, Reliable) void Server_SetTime(float Hours);
	UFUNCTION(Server, Reliable) void Server_SetWeather(int32 Weather);
};
