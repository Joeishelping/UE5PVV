#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BRTypes.h"
#include "Sound/SoundBase.h"
#include "BRSettings.generated.h"

/**
 * Edit -> Project Settings -> (left list, under Game) -> Border Run.
 * Saved into Config/DefaultGame.ini, so it ships with your packaged game.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Border Run"))
class PIRATESVSVIKINGS_API UBRSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBRSettings();

	/** Name of the team that crosses (menu buttons, HUD, announcements). */
	UPROPERTY(Config, EditAnywhere, Category = "Team Names")
	FString RunnerTeamName = TEXT("Runners");

	/** Name of the team that guards the border. */
	UPROPERTY(Config, EditAnywhere, Category = "Team Names")
	FString WardenTeamName = TEXT("Border Patrol");

	/** One player of that team ("YOU ARE A ..."). */
	UPROPERTY(Config, EditAnywhere, Category = "Team Names")
	FString RunnerSingular = TEXT("Runner");

	UPROPERTY(Config, EditAnywhere, Category = "Team Names")
	FString WardenSingular = TEXT("Border Patrol Agent");

	// --- Sounds: drag a sound (Sound Wave or Sound Cue) into any slot. Empty = silent. -------------
	/** Buying something at a counter (cash register "ka-ching"). */
	UPROPERTY(Config, EditAnywhere, Category = "Sounds") TSoftObjectPtr<USoundBase> BuySound;
	/** Selling scrap / getting paid for a job. */
	UPROPERTY(Config, EditAnywhere, Category = "Sounds") TSoftObjectPtr<USoundBase> SellSound;
	/** Picking up dropped cash. */
	UPROPERTY(Config, EditAnywhere, Category = "Sounds") TSoftObjectPtr<USoundBase> CashPickupSound;
	/** Big payout (reaching the Safe Zone, smuggling pay). */
	UPROPERTY(Config, EditAnywhere, Category = "Sounds") TSoftObjectPtr<USoundBase> PaydaySound;
	/** Zip-ties going on. */
	UPROPERTY(Config, EditAnywhere, Category = "Sounds") TSoftObjectPtr<USoundBase> ZipTieSound;
	/** Locked in the Holding Pen (cell door clang). */
	UPROPERTY(Config, EditAnywhere, Category = "Sounds") TSoftObjectPtr<USoundBase> JailDoorSound;
	/** Jumping / falling into deep water. */
	UPROPERTY(Config, EditAnywhere, Category = "Sounds") TSoftObjectPtr<USoundBase> SplashSound;
	/** Scanner: papers in order (friendly beep). */
	UPROPERTY(Config, EditAnywhere, Category = "Sounds") TSoftObjectPtr<USoundBase> ScanOKSound;
	/** Scanner: no papers / contraband / Runner found (alarm buzz). */
	UPROPERTY(Config, EditAnywhere, Category = "Sounds") TSoftObjectPtr<USoundBase> ScanAlarmSound;
	/** A bear trap / glue pad going off. */
	UPROPERTY(Config, EditAnywhere, Category = "Sounds") TSoftObjectPtr<USoundBase> TrapSnapSound;
	/** Tripwire alarm. */
	UPROPERTY(Config, EditAnywhere, Category = "Sounds") TSoftObjectPtr<USoundBase> TripwireAlarmSound;
	/** A hat flying off someone's head (pop!). */
	UPROPERTY(Config, EditAnywhere, Category = "Sounds") TSoftObjectPtr<USoundBase> HatPopSound;
	/** Caught in a watchtower spotlight (dramatic sting). */
	UPROPERTY(Config, EditAnywhere, Category = "Sounds") TSoftObjectPtr<USoundBase> SpottedSound;
	/** A new bounty ("WANTED" sting, everyone hears it). */
	UPROPERTY(Config, EditAnywhere, Category = "Sounds") TSoftObjectPtr<USoundBase> BountySound;
	/** The Line opens (air horn / starter pistol, everyone hears it). */
	UPROPERTY(Config, EditAnywhere, Category = "Sounds") TSoftObjectPtr<USoundBase> RoundStartSound;
	/** Time's up (buzzer, everyone hears it). */
	UPROPERTY(Config, EditAnywhere, Category = "Sounds") TSoftObjectPtr<USoundBase> RoundEndSound;
	/** Lobby / last-seconds countdown beep. */
	UPROPERTY(Config, EditAnywhere, Category = "Sounds") TSoftObjectPtr<USoundBase> CountdownSound;
	/** Runner made it to the Safe Zone (celebration jingle). */
	UPROPERTY(Config, EditAnywhere, Category = "Sounds") TSoftObjectPtr<USoundBase> SafeZoneSound;
	/** Border Patrol whistle (a Warden starts chasing / zip-ties someone). */
	UPROPERTY(Config, EditAnywhere, Category = "Sounds") TSoftObjectPtr<USoundBase> WhistleSound;
	/** Heavy landing after a catapult / cannon flight (thud). */
	UPROPERTY(Config, EditAnywhere, Category = "Sounds") TSoftObjectPtr<USoundBase> LandingSound;
	/** Border Patrol buying gear with the budget (stamp / "approved"). */
	UPROPERTY(Config, EditAnywhere, Category = "Sounds") TSoftObjectPtr<USoundBase> RequisitionSound;

	USoundBase* GetSound(EBRSfx Sfx) const;
};
