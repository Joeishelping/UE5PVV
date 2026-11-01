#pragma once

#include "CoreMinimal.h"
#include "PVVTypes.generated.h"

UENUM(BlueprintType)
enum class EPVVFaction : uint8
{
	None		UMETA(DisplayName = "Unassigned"),
	Pirates		UMETA(DisplayName = "Runners"),	// attackers (Pirates in the original mode)
	Vikings		UMETA(DisplayName = "Wardens")	// defenders (Vikings in the original mode)
};

/** What a character is currently able to do. Carrying / seating are tracked separately. */
UENUM(BlueprintType)
enum class EPVVCondition : uint8
{
	Normal,
	KnockedDown,	// Out of balance; ragdolls, recovers on a timer unless restrained first.
	Restrained,		// Zip-tied. Can shuffle slowly and struggle (Jump) to break free.
	Detained		// Locked in a detention cell until a jailbreak.
};

UENUM(BlueprintType)
enum class EPVVMatchPhase : uint8
{
	InProgress,
	Ended
};

namespace PVV
{
	/** Only enforcement can restrain and detain. */
	inline bool CanCapture(EPVVFaction Faction) { return Faction == EPVVFaction::Vikings; }

	inline bool IsHostile(EPVVFaction A, EPVVFaction B)
	{
		return A != EPVVFaction::None && B != EPVVFaction::None && A != B;
	}

	inline FLinearColor FactionColor(EPVVFaction Faction)
	{
		switch (Faction)
		{
		case EPVVFaction::Pirates: return FLinearColor(0.1f, 0.85f, 0.75f);
		case EPVVFaction::Vikings: return FLinearColor(1.0f, 0.5f, 0.1f);
		default: return FLinearColor::White;
		}
	}
}
