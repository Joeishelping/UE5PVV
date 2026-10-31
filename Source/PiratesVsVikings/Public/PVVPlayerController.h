#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PVVTypes.h"
#include "PVVPlayerController.generated.h"

UCLASS()
class PIRATESVSVIKINGS_API APVVPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** Call from a faction-select widget. */
	UFUNCTION(BlueprintCallable, Category = "PVV")
	void RequestFaction(EPVVFaction Faction);

	/** Console: JoinPirates / JoinVikings */
	UFUNCTION(Exec)
	void JoinPirates();

	UFUNCTION(Exec)
	void JoinVikings();

	UFUNCTION(Client, Reliable)
	void Client_ShowFactionSelect();

	virtual void PawnLeavingGame() override;

protected:
	UFUNCTION(Server, Reliable)
	void Server_RequestFaction(EPVVFaction Faction);

	/** Open your faction-select widget here (only fires when the GameMode doesn't auto-assign). */
	UFUNCTION(BlueprintImplementableEvent, Category = "PVV")
	void ShowFactionSelect();
};
