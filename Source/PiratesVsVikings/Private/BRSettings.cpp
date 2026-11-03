#include "BRSettings.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "BRTypes.h"
#include "PVVTypes.h"

UBRSettings::UBRSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("Border Run");
}

USoundBase* UBRSettings::GetSound(EBRSfx Sfx) const
{
	const TSoftObjectPtr<USoundBase>* Slot = nullptr;
	switch (Sfx)
	{
	case EBRSfx::Buy:           Slot = &BuySound; break;
	case EBRSfx::Sell:          Slot = &SellSound; break;
	case EBRSfx::CashPickup:    Slot = &CashPickupSound; break;
	case EBRSfx::Payday:        Slot = &PaydaySound; break;
	case EBRSfx::ZipTie:        Slot = &ZipTieSound; break;
	case EBRSfx::JailDoor:      Slot = &JailDoorSound; break;
	case EBRSfx::Splash:        Slot = &SplashSound; break;
	case EBRSfx::ScanOK:        Slot = &ScanOKSound; break;
	case EBRSfx::ScanAlarm:     Slot = &ScanAlarmSound; break;
	case EBRSfx::TrapSnap:      Slot = &TrapSnapSound; break;
	case EBRSfx::TripwireAlarm: Slot = &TripwireAlarmSound; break;
	case EBRSfx::HatPop:        Slot = &HatPopSound; break;
	case EBRSfx::Spotted:       Slot = &SpottedSound; break;
	case EBRSfx::Bounty:        Slot = &BountySound; break;
	case EBRSfx::RoundStart:    Slot = &RoundStartSound; break;
	case EBRSfx::RoundEnd:      Slot = &RoundEndSound; break;
	case EBRSfx::Countdown:     Slot = &CountdownSound; break;
	case EBRSfx::SafeZone:      Slot = &SafeZoneSound; break;
	case EBRSfx::Whistle:       Slot = &WhistleSound; break;
	case EBRSfx::Landing:       Slot = &LandingSound; break;
	case EBRSfx::Requisition:   Slot = &RequisitionSound; break;
	default: break;
	}
	return Slot && !Slot->IsNull() ? Slot->LoadSynchronous() : nullptr;
}

bool UBRSettings::ApplySurface(UStaticMeshComponent* Mesh, const FString& Kind, const FString& Piece) const
{
	if (!Mesh)
	{
		return false;
	}
	auto Has = [&Piece](const TCHAR* Word) { return Piece.Contains(Word); };
	const FBRMapSurface* Surface = nullptr;
	if (Kind == TEXT("Water") || Has(TEXT("Fountain_Water"))) Surface = &Water;
	else if (Kind == TEXT("Wall")) Surface = &BorderWall;
	else if (Kind == TEXT("Fence")) Surface = Has(TEXT("Panel")) ? &Fences : &Metal;
	else if (Kind == TEXT("Station")) Surface = Has(TEXT("Counter")) ? &Wood : nullptr;
	else if (Kind == TEXT("Ring")) Surface = Has(TEXT("Deck")) ? &RingCanvas : nullptr;
	else if (Kind == TEXT("Tower")) Surface = Has(TEXT("Roof")) ? &Roofs : &Wood;
	else if (Has(TEXT("Roof"))) Surface = &Roofs;
	else if (Has(TEXT("Sand")) || Has(TEXT("Edge"))) Surface = &Ground;
	else if (Has(TEXT("Road")) || Has(TEXT("Bridge_Deck"))) Surface = &Road;
	else if (Has(TEXT("Rail")) || Has(TEXT("Pump")) || Has(TEXT("Cell")) || Has(TEXT("Junk")) || Piece == TEXT("Tower")) Surface = &Metal;
	else if (Has(TEXT("Plaza")) || Has(TEXT("Fountain"))) Surface = &Plaza;
	else if (Has(TEXT("_Body")) || Has(TEXT("Church_Tower")) || Has(TEXT("Coop_Box")) || Has(TEXT("ToolShop")) || Has(TEXT("Taco"))
		|| Has(TEXT("BlackMarket")) || Has(TEXT("Marina")) || Has(TEXT("House")) || Has(TEXT("WardenHQ"))) Surface = &Buildings;
	else if (Has(TEXT("Bench")) || Has(TEXT("Table")) || Has(TEXT("Stall")) || Has(TEXT("Door")) || Has(TEXT("Shutter"))) Surface = &Wood;
	if (!Surface)
	{
		return false;
	}
	if (!Surface->Material.IsNull())
	{
		if (UMaterialInterface* Mat = Surface->Material.LoadSynchronous())
		{
			Mesh->SetMaterial(0, Mat);
			Mesh->ComponentTags.AddUnique(TEXT("BRThemed"));
			return true;
		}
	}
	if (!Surface->Texture.IsNull() && !WorldTextureMaterial.IsNull())
	{
		UMaterialInterface* Base = WorldTextureMaterial.LoadSynchronous();
		UTexture* Tex = Surface->Texture.LoadSynchronous();
		if (Base && Tex)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, Mesh);
			MID->SetTextureParameterValue(TEXT("Texture"), Tex);
			MID->SetScalarParameterValue(TEXT("TileSize"), FMath::Max(10.f, Surface->TileSize));
			Mesh->SetMaterial(0, MID);
			Mesh->ComponentTags.AddUnique(TEXT("BRThemed"));
			return true;
		}
	}
	return false;
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
		case EBRItem::CannonSuit:  return TEXT("Cannonball Suit");
		case EBRItem::Shovel:      return TEXT("Shovel");
		case EBRItem::Trumpet:     return TEXT("Mariachi Trumpet");
		case EBRItem::TacoPlatter: return TEXT("Taco Platter");
		case EBRItem::Drugs:       return TEXT("Drug Package");
		case EBRItem::Poncho:      return TEXT("Disguise Poncho");
		case EBRItem::Lasso:       return TEXT("Lasso");
		case EBRItem::CardboardBox: return TEXT("Cardboard Box");
		default:                   return TEXT("");
		}
	}

	FString HatName(EBRHat Hat)
	{
		switch (Hat)
		{
		case EBRHat::CowboyHat:    return TEXT("Cowboy Hat");
		case EBRHat::Sombrero:     return TEXT("Sombrero");
		case EBRHat::PatrolCap:    return TEXT("Patrol Cap");
		case EBRHat::HardHat:      return TEXT("Hard Hat");
		case EBRHat::CannonHelmet: return TEXT("Cannonball Helmet");
		case EBRHat::LuchaMask:    return TEXT("Lucha Mask");
		default:                   return TEXT("No hat");
		}
	}

	FString HatSkill(EBRHat Hat)
	{
		switch (Hat)
		{
		case EBRHat::CowboyHat:    return TEXT("+12% speed");
		case EBRHat::Sombrero:     return TEXT("get hungry 40% slower, harder to spot by day");
		case EBRHat::PatrolCap:    return TEXT("scanner: longer range, twice as fast");
		case EBRHat::HardHat:      return TEXT("take 40% less damage from hits");
		case EBRHat::CannonHelmet: return TEXT("land from the cannon / catapult on your feet");
		case EBRHat::LuchaMask:    return TEXT("dives and tackles fly 40% farther and hit harder");
		default:                   return TEXT("");
		}
	}

	FString WardenGearName(EBRWardenGear Gear, EBRItem Item)
	{
		switch (Gear)
		{
		case EBRWardenGear::ExtraTraps:    return TEXT("+2 Team Traps");
		case EBRWardenGear::HardHat:       return TEXT("Hard Hat");
		case EBRWardenGear::DroneRecharge: return TEXT("Drone Recharge");
		case EBRWardenGear::PatrolDog:     return TEXT("Patrol Dog");
		default:                           return ItemName(Item);
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

	FLinearColor HairColor(float Slider)
	{
		// Natural hair only: black -> browns -> blonde -> auburn/red -> grey -> white.
		static const FColor Stops[] = {
			FColor(14, 11, 10), FColor(30, 22, 17), FColor(55, 36, 24), FColor(90, 60, 36), FColor(130, 95, 60),
			FColor(175, 140, 90), FColor(205, 175, 120), FColor(120, 50, 25), FColor(150, 70, 35), FColor(125, 120, 115), FColor(200, 198, 195)
		};
		constexpr int32 Num = UE_ARRAY_COUNT(Stops);
		const float T = FMath::Clamp((Slider - 0.05f) / 0.95f, 0.f, 1.f) * (Num - 1);
		const int32 A = FMath::Min(FMath::FloorToInt(T), Num - 2);
		return FMath::Lerp(FLinearColor::FromSRGBColor(Stops[A]), FLinearColor::FromSRGBColor(Stops[A + 1]), T - A);
	}

	FLinearColor ClothColor(float Slider)
	{
		// Everyday fabric colours (workwear, denim, khaki...), picked in steps - no neon.
		static const FColor Swatches[] = {
			FColor(235, 232, 222), FColor(214, 200, 170), FColor(176, 156, 112), FColor(140, 110, 75), FColor(96, 70, 48),
			FColor(110, 112, 70), FColor(62, 78, 52), FColor(150, 170, 190), FColor(70, 95, 130), FColor(40, 55, 85),
			FColor(30, 36, 52), FColor(130, 130, 128), FColor(70, 70, 72), FColor(28, 28, 30), FColor(120, 35, 35),
			FColor(80, 25, 30), FColor(185, 150, 60), FColor(170, 95, 55)
		};
		constexpr int32 Num = UE_ARRAY_COUNT(Swatches);
		const int32 Index = FMath::Clamp(FMath::FloorToInt((FMath::Clamp(Slider, 0.05f, 1.f) - 0.05f) / 0.95f * Num), 0, Num - 1);
		return FLinearColor::FromSRGBColor(Swatches[Index]);
	}
}
