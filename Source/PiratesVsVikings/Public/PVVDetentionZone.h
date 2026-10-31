#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PVVInteractable.h"
#include "PVVDetentionZone.generated.h"

class APVVCharacter;
class UBoxComponent;

/**
 * Viking processing facility.
 *  - Vikings carrying a prisoner into DepositArea press Interact -> prisoner teleported into the cell.
 *  - Pirates standing in DoorArea hold Interact -> jailbreak, every detainee walks out at ReleasePoint.
 * Build the actual cell walls in the level around CellPoint.
 */
UCLASS()
class PIRATESVSVIKINGS_API APVVDetentionZone : public AActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	APVVDetentionZone();

	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

	UFUNCTION(BlueprintPure, Category = "PVV|Detention")
	int32 GetNumDetained() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PVV|Components")
	TObjectPtr<UBoxComponent> DepositArea;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PVV|Components")
	TObjectPtr<UBoxComponent> DoorArea;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PVV|Components")
	TObjectPtr<USceneComponent> CellPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PVV|Components")
	TObjectPtr<USceneComponent> ReleasePoint;

	UPROPERTY(EditAnywhere, Category = "PVV|Detention")
	float JailbreakDuration = 5.f;

	/** Detainees are scattered this far around CellPoint / ReleasePoint. */
	UPROPERTY(EditAnywhere, Category = "PVV|Detention")
	float ScatterRadius = 150.f;

	UFUNCTION(BlueprintImplementableEvent, Category = "PVV|Detention")
	void OnPrisonerProcessed(APVVCharacter* Prisoner);

	UFUNCTION(BlueprintImplementableEvent, Category = "PVV|Detention")
	void OnJailbreak(APVVCharacter* Rescuer);

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PVV|Detention")
	TArray<TObjectPtr<APVVCharacter>> Detainees;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	FVector Scatter(const FVector& Center) const;
};
