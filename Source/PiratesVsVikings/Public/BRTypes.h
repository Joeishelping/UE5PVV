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
	CarKey			UMETA(DisplayName = "Car Key")
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
	Bet			// bet Price (0 = ALL IN) on a rooster at the cockfight pit
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
	inline FString ItemName(EBRItem Item) { return UEnum::GetDisplayValueAsText(Item).ToString(); }
	inline bool IsIllegal(EBRItem Item) { return Item == EBRItem::IllegalGun; }
}
