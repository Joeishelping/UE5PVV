#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BRHUD.generated.h"

/** Prototype canvas HUD for Border Run. Replace with UMG later. */
UCLASS()
class PIRATESVSVIKINGS_API ABRHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
	virtual void BeginPlay() override;

private:
	void Centered(const FString& Text, float Y, const FLinearColor& Color, UFont* Font, float Scale = 1.f);
	void Bar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Color);
	void Marker(const FVector& World, const FString& Text, const FLinearColor& Color);
	UPROPERTY() TObjectPtr<class ABRReplayDirector> Replay;
};
