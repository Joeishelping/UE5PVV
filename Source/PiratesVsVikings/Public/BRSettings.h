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
class UStaticMeshComponent;
class UMaterialInterface;
class UTexture;
class UNiagaraSystem;

/** How one kind of surface on the test map looks. Give it a Material, OR a Texture (needs World Texture Material below). */
USTRUCT(BlueprintType)
struct FBRMapSurface
{
	GENERATED_BODY()

	/** A finished material (e.g. from Fab or Starter Content). Wins over Texture. */
	UPROPERTY(EditAnywhere, Category = "Surface") TSoftObjectPtr<UMaterialInterface> Material;
	/** Just a picture (sand, asphalt, adobe...): it gets repeated across the surface so it never looks stretched. */
	UPROPERTY(EditAnywhere, Category = "Surface") TSoftObjectPtr<UTexture> Texture;
	/** How big one copy of the texture is, in cm (400 = 4 metres). */
	UPROPERTY(EditAnywhere, Category = "Surface") float TileSize = 400.f;
};

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
	/** A vehicle smashes through the checkpoint barrier (crunch / wood splintering). */
	UPROPERTY(Config, EditAnywhere, Category = "Sounds") TSoftObjectPtr<USoundBase> GateSmashSound;
	/** Cantina: saloon piano / music that loops inside. */
	UPROPERTY(Config, EditAnywhere, Category = "Sounds") TSoftObjectPtr<USoundBase> CantinaMusic;
	/** Cantina: a bar fight kicks off (bell, crowd roar, glass smash). */
	UPROPERTY(Config, EditAnywhere, Category = "Sounds") TSoftObjectPtr<USoundBase> BarFightSound;
	/** Cantina: buying a drink (glug glug, "salud!"). */
	UPROPERTY(Config, EditAnywhere, Category = "Sounds") TSoftObjectPtr<USoundBase> DrinkSound;
	/** Weather: rain loop (set it to Looping), howling wind loop for sandstorms, thunder. */
	UPROPERTY(Config, EditAnywhere, Category = "Sounds") TSoftObjectPtr<USoundBase> RainLoopSound;
	UPROPERTY(Config, EditAnywhere, Category = "Sounds") TSoftObjectPtr<USoundBase> WindLoopSound;
	UPROPERTY(Config, EditAnywhere, Category = "Sounds") TSoftObjectPtr<USoundBase> ThunderSound;

	USoundBase* GetSound(EBRSfx Sfx) const;

	// --- Body sounds (every character). Add several of each and one is picked at random. -------------------
	/** Footsteps (played in step with walking / running; quieter crouched). */
	UPROPERTY(Config, EditAnywhere, Category = "Body Sounds") TArray<TSoftObjectPtr<USoundBase>> FootstepSounds;
	/** A body hitting the ground (ragdoll tumbles, hard landings). Louder the harder they land. */
	UPROPERTY(Config, EditAnywhere, Category = "Body Sounds") TArray<TSoftObjectPtr<USoundBase>> BodyThumpSounds;
	/** "Oof!" when you get hit. */
	UPROPERTY(Config, EditAnywhere, Category = "Body Sounds") TArray<TSoftObjectPtr<USoundBase>> HitGruntSounds;
	/** Screams when you get thrown, launched or catapulted. */
	UPROPERTY(Config, EditAnywhere, Category = "Body Sounds") TArray<TSoftObjectPtr<USoundBase>> FlyingYellSounds;
	/** Whoosh of a punch, kick, headbutt or dive. */
	UPROPERTY(Config, EditAnywhere, Category = "Body Sounds") TArray<TSoftObjectPtr<USoundBase>> WhooshSounds;

	/** One random sound from a list (nullptr if empty). */
	static USoundBase* PickSound(const TArray<TSoftObjectPtr<USoundBase>>& List);

	// --- Effects: drag in a Niagara particle system (free ones on Fab). Empty = the simple built-in look. ---------
	/** Dust when someone lands hard, belly-flops or falls off a wall. */
	UPROPERTY(Config, EditAnywhere, Category = "Effects") TSoftObjectPtr<UNiagaraSystem> DustEffect;
	/** Splash when someone hits deep water. */
	UPROPERTY(Config, EditAnywhere, Category = "Effects") TSoftObjectPtr<UNiagaraSystem> SplashEffect;
	/** Cannon smoke. */
	UPROPERTY(Config, EditAnywhere, Category = "Effects") TSoftObjectPtr<UNiagaraSystem> SmokeEffect;
	/** Punch / kick / headbutt impact (where the hit lands). */
	UPROPERTY(Config, EditAnywhere, Category = "Effects") TSoftObjectPtr<UNiagaraSystem> PunchHitEffect;
	/** Little puff from the smoking emote. */
	UPROPERTY(Config, EditAnywhere, Category = "Effects") TSoftObjectPtr<UNiagaraSystem> CigaretteSmokeEffect;
	/** Cannon fuse sparks. */
	UPROPERTY(Config, EditAnywhere, Category = "Effects") TSoftObjectPtr<UNiagaraSystem> SparksEffect;
	/** Rain (follows your camera; empty = simple built-in streaks). */
	UPROPERTY(Config, EditAnywhere, Category = "Effects") TSoftObjectPtr<UNiagaraSystem> RainEffect;
	/** Sandstorm (follows your camera; empty = simple built-in blowing sand). */
	UPROPERTY(Config, EditAnywhere, Category = "Effects") TSoftObjectPtr<UNiagaraSystem> SandstormEffect;

	UNiagaraSystem* GetEffect(EBRFX Kind) const;

	// --- Test map materials (Project Settings -> Border Run -> Test Map Look) ---------------------
	/** Your one "world texture" material (see the guide): it has a Texture Object parameter "Texture" and a scalar "TileSize". */
	UPROPERTY(Config, EditAnywhere, Category = "Test Map Look") TSoftObjectPtr<UMaterialInterface> WorldTextureMaterial;
	/** Desert sand / dirt. */
	UPROPERTY(Config, EditAnywhere, Category = "Test Map Look") FBRMapSurface Ground;
	/** The road and the bridge deck. */
	UPROPERTY(Config, EditAnywhere, Category = "Test Map Look") FBRMapSurface Road;
	/** The river (and the fountain). Tip: Starter Content's M_Water_Lake works here. */
	UPROPERTY(Config, EditAnywhere, Category = "Test Map Look") FBRMapSurface Water;
	/** House, shop, church and HQ walls. */
	UPROPERTY(Config, EditAnywhere, Category = "Test Map Look") FBRMapSurface Buildings;
	UPROPERTY(Config, EditAnywhere, Category = "Test Map Look") FBRMapSurface Roofs;
	/** THE WALL (and Map Kit wall segments without their own material). */
	UPROPERTY(Config, EditAnywhere, Category = "Test Map Look") FBRMapSurface BorderWall;
	/** Razor-wire fence panels. */
	UPROPERTY(Config, EditAnywhere, Category = "Test Map Look") FBRMapSurface Fences;
	/** Counters, benches, stalls, doors, watchtowers. */
	UPROPERTY(Config, EditAnywhere, Category = "Test Map Look") FBRMapSurface Wood;
	/** Bridge rails, gas pumps, holding cells, junk. */
	UPROPERTY(Config, EditAnywhere, Category = "Test Map Look") FBRMapSurface Metal;
	/** The town plaza floor and fountain stone. */
	UPROPERTY(Config, EditAnywhere, Category = "Test Map Look") FBRMapSurface Plaza;
	/** The lucha ring's canvas. */
	UPROPERTY(Config, EditAnywhere, Category = "Test Map Look") FBRMapSurface RingCanvas;

	/** Put the right surface on one piece of the test map. Kind = "Map", "Village", "Wall", "Fence", "Station", "Ring", "Tower", "Water". True if it did. */
	bool ApplySurface(UStaticMeshComponent* Mesh, const FString& Kind, const FString& Piece) const;
};
