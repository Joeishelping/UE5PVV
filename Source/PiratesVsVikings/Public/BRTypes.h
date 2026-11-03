#pragma once

#include "CoreMinimal.h"
#include "PVVTypes.h"
#include "BRTypes.generated.h"

/** Border Run: Runners = EPVVFaction::Pirates (attackers), Wardens = EPVVFaction::Vikings (defenders). */
namespace BR
{
	constexpr EPVVFaction Runners = EPVVFaction::Pirates;
	constexpr EPVVFaction Wardens = EPVVFaction::Vikings;
}

UENUM(BlueprintType)
enum class EBRPhase : uint8
{
	Prep,		// Runners earn and shop; the Line is closed
	Invasion,	// The Line opens
	Ended
};

UENUM(BlueprintType)
enum class EBRItem : uint8
{
	None,
	Ladder			UMETA(DisplayName = "Ladder"),
	WireCutters		UMETA(DisplayName = "Wire Cutters"),
	GasCan			UMETA(DisplayName = "Gas Can"),
	Taco			UMETA(DisplayName = "Taco"),
	Scrap			UMETA(DisplayName = "Scrap"),
	IllegalGun		UMETA(DisplayName = "Illegal Gun"),
	CarKey			UMETA(DisplayName = "Car Key"),
	CannonSuit		UMETA(DisplayName = "Cannonball Suit"),
	Shovel			UMETA(DisplayName = "Shovel"),
	Trumpet			UMETA(DisplayName = "Mariachi Trumpet"),
	TacoPlatter		UMETA(DisplayName = "Taco Platter"),
	Drugs			UMETA(DisplayName = "Drug Package"),
	Poncho			UMETA(DisplayName = "Disguise Poncho"),
	Lasso			UMETA(DisplayName = "Lasso"),
	CardboardBox	UMETA(DisplayName = "Cardboard Box"),
	Knife			UMETA(DisplayName = "Knife")
};

/** Hats: each one gives a skill. Get knocked out and it may fly off - anyone can pick it up. */
UENUM(BlueprintType)
enum class EBRHat : uint8
{
	None,
	CowboyHat		UMETA(DisplayName = "Cowboy Hat"),
	Sombrero		UMETA(DisplayName = "Sombrero"),
	PatrolCap		UMETA(DisplayName = "Patrol Cap"),
	HardHat			UMETA(DisplayName = "Hard Hat"),
	CannonHelmet	UMETA(DisplayName = "Cannonball Helmet"),
	LuchaMask		UMETA(DisplayName = "Lucha Mask")
};

UENUM(BlueprintType)
enum class EBRWeather : uint8
{
	Clear,
	Rain,
	Sandstorm
};

UENUM(BlueprintType)
enum class EBRStationKind : uint8
{
	BuyItem,	// pay Price, get Item
	SellItem,	// give Item, get Price
	BuyBoat,	// pay Price, a boat appears at BoatSpawn
	Job,		// hold to work, get Price (and Item if set)
	Bet,		// bet Price (0 = ALL IN) on a rooster at the cockfight pit
	HireClient,	// a smuggling client pays you to get them across: they follow you (paid on arrival)
	LuchaChallenge,	// fight the luchador in the ring for Price (win = double)
	LuchaBet,		// bet Price on the fight (BetSide 0 = the challenger, 1 = the luchador)
	WardenSupply	// Border Patrol only: buy gear with the government budget (pick Warden Gear, or an Item)
};

/** Body style of a car (test-map greybox trucks). */
UENUM(BlueprintType)
enum class EBRCarModel : uint8
{
	Pickup		UMETA(DisplayName = "Pickup Truck"),
	Sedan,
	Lowrider	UMETA(DisplayName = "Lowrider (fast, 2 seats)"),
	Van			UMETA(DisplayName = "Van (6 seats, hidden back)")
};

/** Look of a hiding spot. */
UENUM(BlueprintType)
enum class EBRHideStyle : uint8
{
	Dumpster,
	PortaPotty	UMETA(DisplayName = "Porta-Potty"),
	HayBale		UMETA(DisplayName = "Hay Bales")
};

/** Game-wide sound effects (set them all in Project Settings -> Border Run -> Sounds). */
UENUM(BlueprintType)
enum class EBRSfx : uint8
{
	Buy, Sell, CashPickup, Payday, ZipTie, JailDoor, Splash, ScanOK, ScanAlarm, TrapSnap, TripwireAlarm,
	HatPop, Spotted, Bounty, RoundStart, RoundEnd, Countdown, SafeZone, Whistle, Landing, Requisition, GateSmash
};

/** Simple world effects. */
UENUM(BlueprintType)
enum class EBRFX : uint8
{
	Dust, Splash, Smoke, MoneyText, PointsText, PunchHit, CigaretteSmoke, Sparks
};

/** Match settings the host picks in the M menu (kept on the host's PC between rounds). */
USTRUCT(BlueprintType)
struct FBRHostSettings
{
	GENERATED_BODY()

	UPROPERTY() int32 PrepSeconds = 180;
	UPROPERTY() int32 InvasionSeconds = 600;
	/** Wait in a lobby until everyone presses Y (ready). */
	UPROPERTY() bool bLobby = true;
	UPROPERTY() bool bBestOf3 = false;
	UPROPERTY() bool bFillBots = false;
	UPROPERTY() int32 BotRunners = 3;
	UPROPERTY() int32 BotWardens = 1;
	/** False until the host changed something (then these replace the Blueprint values). */
	UPROPERTY() bool bSet = false;
};

/** What a Border Patrol supply counter sells (paid from the team's government budget). */
UENUM(BlueprintType)
enum class EBRWardenGear : uint8
{
	None,			// sells the station's Item instead (taco, ladder...)
	ExtraTraps,		// +2 traps for the whole team
	HardHat,		// wear a Hard Hat
	DroneRecharge,	// your drone is ready again right now
	PatrolDog		// a new patrol dog appears next to the counter
};

UENUM(BlueprintType)
enum class EBRTrap : uint8
{
	Bear,	// roots the Runner for a few seconds
	Glue,	// slows them a lot
	Alarm	// pings and tags them for the Wardens
};

namespace BR
{
	PIRATESVSVIKINGS_API FString ItemName(EBRItem Item);
	PIRATESVSVIKINGS_API FString HatName(EBRHat Hat);
	/** What the hat does, e.g. "+12% speed". */
	PIRATESVSVIKINGS_API FString HatSkill(EBRHat Hat);
	/** Name of a Border Patrol supply (uses the Item name when Gear is None). */
	PIRATESVSVIKINGS_API FString WardenGearName(EBRWardenGear Gear, EBRItem Item);
	/** Your team names from Project Settings -> Game -> Border Run. */
	PIRATESVSVIKINGS_API FString RunnerName();
	PIRATESVSVIKINGS_API FString WardenName();
	/** 0 = lightest .. 1 = darkest. */
	PIRATESVSVIKINGS_API FLinearColor SkinColor(float Tone);
	/** Slider 0..1 -> clothing colour. Below 0.05 = keep the outfit's own colour. */
	PIRATESVSVIKINGS_API FLinearColor ClothColor(float Slider);
	inline bool IsOriginalCloth(float Slider) { return Slider < 0.05f; }
	/** Slider 0..1 -> natural hair colours, then dyed ones. Below 0.05 = keep the hair's own colour. */
	PIRATESVSVIKINGS_API FLinearColor HairColor(float Slider);
	/** Contraband: worth big money if you get it to the Safe Zone; confiscated if you're caught. */
	inline bool IsIllegal(EBRItem Item) { return Item == EBRItem::IllegalGun || Item == EBRItem::Drugs; }
}

/** Character customization (menu sliders). Same options for every player on both teams. */
USTRUCT(BlueprintType)
struct FBRLook
{
	GENERATED_BODY()

	/** 0 = lightest skin, 1 = darkest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BR|Look") float SkinTone = 0.45f;
	/** 0 = outfit's own colour, otherwise a colour from the rainbow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BR|Look") float ShirtColor = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BR|Look") float PantsColor = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BR|Look") bool bHat = true;

	// Face shape (your Blender shape keys / Unreal morph targets). 0 = small, 0.5 = normal, 1 = big.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BR|Look") float HeadSize = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BR|Look") float NoseSize = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BR|Look") float LipSize = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BR|Look") float EarSize = 0.5f;

	/** 0 = no hair, 1.. = Hair Styles list on the character Blueprint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BR|Look") int32 HairStyle = 1;
	/** 0 = the hair's own colour, otherwise black -> brown -> blonde -> red -> grey -> dyed colours. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BR|Look") float HairColor = 0.f;
	/** 0 = clean shaven, 1.. = Facial Hair Styles list on the character Blueprint (mustaches, beards). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BR|Look") int32 FacialHair = 0;
	/** Hat you start with: -1 = your team's normal hat, otherwise an EBRHat you've unlocked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BR|Look") int32 HatChoice = -1;

	/** False until someone picked a look (then the character keeps its Blueprint colours). */
	UPROPERTY() bool bSet = false;
};
