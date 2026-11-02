#include "BRPlayerState.h"
#include "Net/UnrealNetwork.h"

void ABRPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABRPlayerState, Money);
	DOREPLIFETIME(ABRPlayerState, Inventory);
	DOREPLIFETIME(ABRPlayerState, bHasCrossedLine);
	DOREPLIFETIME(ABRPlayerState, bIsAggressor);
	DOREPLIFETIME(ABRPlayerState, bReachedSafety);
	DOREPLIFETIME(ABRPlayerState, WireCutterCharges);
	DOREPLIFETIME(ABRPlayerState, SentenceSeconds);
	DOREPLIFETIME(ABRPlayerState, AssignedJob);
	DOREPLIFETIME(ABRPlayerState, JobReadyAt);
	DOREPLIFETIME(ABRPlayerState, Points);
	DOREPLIFETIME(ABRPlayerState, StatCrossings);
	DOREPLIFETIME(ABRPlayerState, StatSmuggled);
	DOREPLIFETIME(ABRPlayerState, StatContraband);
	DOREPLIFETIME(ABRPlayerState, StatCatches);
	DOREPLIFETIME(ABRPlayerState, StatKnockdowns);
	DOREPLIFETIME(ABRPlayerState, StatTackles);
	DOREPLIFETIME(ABRPlayerState, StatThrows);
	DOREPLIFETIME(ABRPlayerState, StatCatapulted);
	DOREPLIFETIME(ABRPlayerState, StatCannonShots);
	DOREPLIFETIME(ABRPlayerState, StatTacos);
	DOREPLIFETIME(ABRPlayerState, StatWrongful);
	DOREPLIFETIME(ABRPlayerState, StatDances);
	DOREPLIFETIME(ABRPlayerState, StatClearedTrucks);
}

bool ABRPlayerState::AddItem(EBRItem Item)
{
	if (!HasAuthority() || Item == EBRItem::None || !HasFreeSlot())
	{
		return false;
	}
	Inventory.Add(Item);
	return true;
}

bool ABRPlayerState::RemoveItem(EBRItem Item)
{
	if (!HasAuthority())
	{
		return false;
	}
	const int32 Index = Inventory.Find(Item);
	if (Index == INDEX_NONE)
	{
		return false;
	}
	Inventory.RemoveAt(Index);
	return true;
}

bool ABRPlayerState::RemoveItemAt(int32 Index, EBRItem& OutItem)
{
	if (!HasAuthority() || !Inventory.IsValidIndex(Index))
	{
		return false;
	}
	OutItem = Inventory[Index];
	Inventory.RemoveAt(Index);
	if (OutItem == EBRItem::WireCutters && !HasItem(EBRItem::WireCutters))
	{
		WireCutterCharges = 0;
	}
	return true;
}

void ABRPlayerState::RemoveIllegalItems()
{
	if (HasAuthority())
	{
		Inventory.RemoveAll([](EBRItem Item) { return BR::IsIllegal(Item); });
	}
}
