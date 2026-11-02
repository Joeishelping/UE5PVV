#include "BRSettings.h"
#include "BRTypes.h"
#include "PVVTypes.h"

UBRSettings::UBRSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("Border Run");
}

FText PVVFactionName(EPVVFaction Faction)
{
	switch (Faction)
	{
	case EPVVFaction::Pirates: return FText::FromString(BR::RunnerName());
	case EPVVFaction::Vikings: return FText::FromString(BR::WardenName());
	default:                   return NSLOCTEXT("PVV", "Unassigned", "Unassigned");
	}
}

namespace BR
{
	FString RunnerName()
	{
		const UBRSettings* S = GetDefault<UBRSettings>();
		return S && !S->RunnerTeamName.IsEmpty() ? S->RunnerTeamName : FString(TEXT("Runners"));
	}

	FString WardenName()
	{
		const UBRSettings* S = GetDefault<UBRSettings>();
		return S && !S->WardenTeamName.IsEmpty() ? S->WardenTeamName : FString(TEXT("Border Patrol"));
	}

	FString ItemName(EBRItem Item)
	{
		// UMETA display names are stripped from packaged games, so spell them out here.
		switch (Item)
		{
		case EBRItem::Ladder:      return TEXT("Ladder");
		case EBRItem::WireCutters: return TEXT("Wire Cutters");
		case EBRItem::GasCan:      return TEXT("Gas Can");
		case EBRItem::Taco:        return TEXT("Taco");
		case EBRItem::Scrap:       return TEXT("Scrap");
		case EBRItem::IllegalGun:  return TEXT("Illegal Gun");
		case EBRItem::CarKey:      return TEXT("Car Key");
		default:                   return TEXT("");
		}
	}

	FLinearColor SkinColor(float Tone)
	{
		// Real skin tones from very light to very dark (sRGB), blended smoothly.
		static const FColor Stops[] = {
			FColor(255, 224, 206), FColor(241, 194, 160), FColor(224, 172, 128), FColor(198, 140, 98),
			FColor(160, 108, 70), FColor(122, 78, 50), FColor(88, 56, 38), FColor(58, 38, 28)
		};
		constexpr int32 Num = UE_ARRAY_COUNT(Stops);
		const float T = FMath::Clamp(Tone, 0.f, 1.f) * (Num - 1);
		const int32 A = FMath::Min(FMath::FloorToInt(T), Num - 2);
		return FMath::Lerp(FLinearColor::FromSRGBColor(Stops[A]), FLinearColor::FromSRGBColor(Stops[A + 1]), T - A);
	}

	FLinearColor ClothColor(float Slider)
	{
		// 0.05 .. 0.9 = rainbow, 0.9 .. 1 = white -> black.
		const float S = FMath::Clamp(Slider, 0.f, 1.f);
		if (S >= 0.9f)
		{
			const float Grey = 1.f - (S - 0.9f) / 0.1f;
			return FLinearColor(Grey, Grey, Grey) * 0.9f + FLinearColor(0.02f, 0.02f, 0.02f);
		}
		const float Hue = (S - 0.05f) / 0.85f * 360.f;
		return FLinearColor(Hue, 0.75f, 0.85f).HSVToLinearRGB();
	}
}
