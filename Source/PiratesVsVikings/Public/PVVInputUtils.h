#pragma once

#include "CoreMinimal.h"
#include "InputActionValue.h"

class UInputAction;
class UInputMappingContext;
struct FKey;

/**
 * Builds Enhanced Input actions/contexts at runtime so the prototype is playable
 * without authoring any input assets. Assign real assets on the Blueprint to override.
 */
namespace PVVInput
{
	UInputAction* MakeAction(UObject* Outer, EInputActionValueType ValueType);

	/** bSwizzle maps a 1D key onto the Y axis (forward/back on a 2D move action). */
	void Map(UInputMappingContext* Context, const UInputAction* Action, const FKey& Key,
		bool bNegate = false, bool bSwizzle = false, float Scale = 1.f);
}
