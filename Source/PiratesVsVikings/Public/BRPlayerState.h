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

	/** Your job in town (Station sign text). "-" = unemployed: rummage through trash. Empty = not assigned yet. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR")
	FString AssignedJob;

	/** Server time when you can work your next shift. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR")
	float JobReadyAt = 0.f;

	// --- Points race + end-of-round awards ---------------------------------------------
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR|Stats") int32 Points = 0;
	/** Fights started this round (punching locals, bar fights). Enough of it = WANTED. */
	int32 Trouble = 0;
	float LastTroubleTime = -100.f;
	/** 0-3 wanted stars (from Trouble). Your team sees them; Border Patrol only when you're tagged. 3 stars = Border Patrol hears where you are. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR|Stats") int32 WantedStars = 0;
	/** Thrown out of the cantina by the bouncer (too much trouble). */
	int32 CantinaStrikes = 0;
	/** Lobby: pressed Y. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR") bool bReady = false;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR|Stats") int32 StatCrossings = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR|Stats") int32 StatSmuggled = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR|Stats") int32 StatContraband = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR|Stats") int32 StatCatches = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR|Stats") int32 StatKnockdowns = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR|Stats") int32 StatTackles = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR|Stats") int32 StatThrows = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR|Stats") int32 StatCatapulted = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR|Stats") int32 StatCannonShots = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR|Stats") int32 StatTacos = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR|Stats") int32 StatWrongful = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR|Stats") int32 StatDances = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR|Stats") int32 StatLuchaWins = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR|Stats") int32 StatClearedTrucks = 0;

	/** Customization picked in the menu (server copy; the character replicates it to everyone). */
	FBRLook Look;
	bool bHasLook = false;

	bool HasItem(EBRItem Item) const { return Inventory.Contains(Item); }
	bool HasFreeSlot() const { return Inventory.Num() < MaxSlots; }

	// Server only.
	bool AddItem(EBRItem Item);
	bool RemoveItem(EBRItem Item);
	/** Removes the item in that inventory slot. */
	bool RemoveItemAt(int32 Index, EBRItem& OutItem);
	void RemoveIllegalItems();
	void AddMoney(int32 Amount) { Money = FMath::Max(0, Money + Amount); }
};
