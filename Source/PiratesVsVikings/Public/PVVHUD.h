#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "PVVHUD.generated.h"

/** Debug-style canvas HUD for the prototype: scores, prompts, meters, kill feed, controls. Replace with UMG later. */
UCLASS()
class PIRATESVSVIKINGS_API APVVHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawCentered(const FString& Text, float Y, const FLinearColor& Color, UFont* Font, float Scale = 1.f);
	void DrawBar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Color);
};
