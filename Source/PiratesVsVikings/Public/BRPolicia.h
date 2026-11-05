#pragma once

// Mexican police (Policía) and the town jail (cárcel).
// They ONLY care about crimes against Mexico: hurting / stabbing / shooting / killing townspeople (or the Policía) in town.
// Anything you do to Border Patrol, or a Border Patrol wanted level, is not their business.
// Crimes raise your "Policía heat"; at 40+ they come for you, beat you down and lock you in the cárcel for a while (plus a fine).

#include "CoreMinimal.h"
#include "BRWorldActors.h"
#include "BRPolicia.generated.h"

UCLASS(meta = (DisplayName = "BR Mexican Jail"))
class PIRATESVSVIKINGS_API ABRMexJail : public ABRGreyboxActor
{
	GENERATED_BODY()

public:
	ABRMexJail();
	/** Inside the cell / just outside the door. */
	FVector GetCellSpot() const;
	FVector GetReleaseSpot() const;
	bool IsInCell(const FVector& WorldPoint) const;
	static ABRMexJail* Find(const UWorld* World);

	static constexpr float CellHalfX = 300.f;
	static constexpr float CellHalfY = 250.f;
	static constexpr float CellHeight = 340.f;
};
