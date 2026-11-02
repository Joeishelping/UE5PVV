#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
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
};
