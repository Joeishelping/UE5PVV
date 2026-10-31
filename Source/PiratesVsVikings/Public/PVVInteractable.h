#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PVVInteractable.generated.h"

class APVVCharacter;

UINTERFACE(MinimalAPI, BlueprintType)
class UPVVInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Anything a character can press Interact on: other characters (capture / rescue / pick up),
 * boats (drive / board / load prisoner), detention zones (deposit / jailbreak).
 * Implementable in Blueprint for new props (supply crates, sabotage points...).
 * All calls are evaluated on the server; clients only use them to show prompts.
 */
class PIRATESVSVIKINGS_API IPVVInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "PVV|Interaction")
	bool CanInteract(APVVCharacter* Interactor) const;

	/** 0 = instant on press. >0 = Interact must be held this many seconds. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "PVV|Interaction")
	float GetInteractDuration(APVVCharacter* Interactor) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "PVV|Interaction")
	FText GetInteractPrompt(APVVCharacter* Interactor) const;

	/** Server only. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "PVV|Interaction")
	void OnInteract(APVVCharacter* Interactor);
};
