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
	PIRATESVSVIKINGS_API FString ItemName(EBRItem Item);
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
	inline bool IsIllegal(EBRItem Item) { return Item == EBRItem::IllegalGun; }
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

	/** False until someone picked a look (then the character keeps its Blueprint colours). */
	UPROPERTY() bool bSet = false;
};
