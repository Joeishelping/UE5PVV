#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerStart.h"
#include "PVVTypes.h"
#include "PVVPlayerStart.generated.h"

/** Place several of these per faction. Pirates near the Learning Center, Vikings at their facility. */
UCLASS()
class PIRATESVSVIKINGS_API APVVPlayerStart : public APlayerStart
{
	GENERATED_BODY()

public:
	APVVPlayerStart(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PVV")
	EPVVFaction Faction = EPVVFaction::Pirates;
};
