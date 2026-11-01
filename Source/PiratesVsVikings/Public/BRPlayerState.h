#pragma once

#include "CoreMinimal.h"
#include "PVVPlayerState.h"
#include "BRTypes.h"
#include "BRPlayerState.generated.h"

UCLASS()
class PIRATESVSVIKINGS_API ABRPlayerState : public APVVPlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static constexpr int32 MaxSlots = 6;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR")
	int32 Money = 40;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR")
	TArray<EBRItem> Inventory;

	/** Set the first time a Runner passes the Line during the Invasion. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR")
	bool bHasCrossedLine = false;

	/** Set when a Runner draws/fires a gun or hits a Warden. Makes force "justified". */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR")
	bool bIsAggressor = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR")
	bool bReachedSafety = false;

	/** Cuts left before the wire cutters snap (each pair = 2 cuts). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR")
	int32 WireCutterCharges = 0;

	/** Seconds left in the holding pen (0 = free). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR")
	int32 SentenceSeconds = 0;

	bool HasItem(EBRItem Item) const { return Inventory.Contains(Item); }
	bool HasFreeSlot() const { return Inventory.Num() < MaxSlots; }

	// Server only.
	bool AddItem(EBRItem Item);
	bool RemoveItem(EBRItem Item);
	void RemoveIllegalItems();
	void AddMoney(int32 Amount) { Money = FMath::Max(0, Money + Amount); }
};
