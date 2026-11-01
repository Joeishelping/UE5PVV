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

void ABRPlayerState::RemoveIllegalItems()
{
	if (HasAuthority())
	{
		Inventory.RemoveAll([](EBRItem Item) { return BR::IsIllegal(Item); });
	}
}
