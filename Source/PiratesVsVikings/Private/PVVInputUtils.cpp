#include "PVVInputUtils.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputCoreTypes.h"

UInputAction* PVVInput::MakeAction(UObject* Outer, EInputActionValueType ValueType)
{
	UInputAction* Action = NewObject<UInputAction>(Outer);
	Action->ValueType = ValueType;
	return Action;
}

void PVVInput::Map(UInputMappingContext* Context, const UInputAction* Action, const FKey& Key,
	bool bNegate, bool bSwizzle, float Scale)
{
	if (!Context || !Action)
	{
		return;
	}

	FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);
	if (bSwizzle)
	{
		Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(Context));
	}
	if (bNegate)
	{
		Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
	}
	if (!FMath::IsNearlyEqual(Scale, 1.f))
	{
		UInputModifierScalar* Scalar = NewObject<UInputModifierScalar>(Context);
		Scalar->Scalar = FVector(Scale);
		Mapping.Modifiers.Add(Scalar);
	}
}
