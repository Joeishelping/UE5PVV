#include "BRCantina.h"
#include "BRDoors.h"
#include "BRAudio.h"
#include "BRCharacter.h"
#include "BRGameMode.h"
#include "BRGameState.h"
#include "BRPlayerState.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "PVVTypes.h"
#include "BRSettings.h"
#include "Sound/SoundBase.h"

#define LOCTEXT_NAMESPACE "BRCantina"

namespace
{
	ABRPlayerState* CantinaPS(const APVVCharacter* C)
	{
		return C ? Cast<ABRPlayerState>(C->GetPVVPlayerState()) : nullptr;
	}
	bool IsFreePlayer(const APVVCharacter* C)
	{
		const ABRCharacter* BRC = Cast<ABRCharacter>(C);
		return BRC && !BRC->IsNPC() && C->GetCondition() == EPVVCondition::Normal && !C->GetCarriedCharacter() && !C->GetCurrentBoat();
	}
	bool IsStanding(const ABRCharacter* C)
	{
		return IsValid(C) && C->GetCondition() == EPVVCondition::Normal && !C->GetCarrier() && !C->GetCurrentBoat();
	}
}

// ===========================================================================================
// Spots

ABRCantinaSpot::ABRCantinaSpot()
{
	bReplicates = true;
	AddPickupZone(170.f);
	AddLabel(TEXT("Sign"), FVector(0.f, 0.f, 200.f), 34.f, FColor(255, 200, 90));
}

void ABRCantinaSpot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRCantinaSpot, Kind);
	DOREPLIFETIME(ABRCantinaSpot, Cantina);
}

void ABRCantinaSpot::BeginPlay()
{
	Super::BeginPlay();
	if (UTextRenderComponent* Label = FindComponentByClass<UTextRenderComponent>())
	{
		const TCHAR* Text = Kind == EBRCantinaSpot::Drink ? TEXT("TEQUILA") : Kind == EBRCantinaSpot::Contest ? TEXT("DRINKING CONTEST\nbeat El Borracho") : TEXT("THE BIG GUY\n(don't push him)");
		Label->SetText(FText::FromString(Text));
	}
}

bool ABRCantinaSpot::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	return Cantina && FVector::Dist(Interactor->GetActorLocation(), GetActorLocation()) < 300.f && Cantina->CanUse(Kind, Interactor);
}

FText ABRCantinaSpot::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	return Cantina ? Cantina->GetPrompt(Kind, Interactor) : FText::GetEmpty();
}

void ABRCantinaSpot::OnInteract_Implementation(APVVCharacter* Interactor)
{
	if (HasAuthority() && Cantina)
	{
		Cantina->Use(Kind, Interactor);
	}
}

// ===========================================================================================
// The cantina

ABRCantina::ABRCantina()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	const FLinearColor Adobe(0.78f, 0.55f, 0.36f), Wood(0.42f, 0.25f, 0.12f), DarkWood(0.25f, 0.14f, 0.07f), Red(0.75f, 0.12f, 0.1f);
	const float H = 420.f, T = 40.f, Door = 320.f;
	// Floor + walls (the door is in the +X wall). No roof, so the camera can see the fight - just beams.
	AddMesh(TEXT("Cantina_Floor"), CubeMesh, FVector(0.f, 0.f, 5.f), FVector(HalfX * 2.f, HalfY * 2.f, 10.f), Wood);
	AddMesh(TEXT("Cantina_WallBack"), CubeMesh, FVector(-HalfX, 0.f, H * 0.5f), FVector(T, HalfY * 2.f, H), Adobe);
	AddMesh(TEXT("Cantina_WallL"), CubeMesh, FVector(0.f, -HalfY, H * 0.5f), FVector(HalfX * 2.f, T, H), Adobe);
	AddMesh(TEXT("Cantina_WallR"), CubeMesh, FVector(0.f, HalfY, H * 0.5f), FVector(HalfX * 2.f, T, H), Adobe);
	const float SideLen = HalfY - Door * 0.5f;
	AddMesh(TEXT("Cantina_FrontL"), CubeMesh, FVector(HalfX, -HalfY + SideLen * 0.5f, H * 0.5f), FVector(T, SideLen, H), Adobe);
	AddMesh(TEXT("Cantina_FrontR"), CubeMesh, FVector(HalfX, HalfY - SideLen * 0.5f, H * 0.5f), FVector(T, SideLen, H), Adobe);
	// Western false front with the name on it.
	AddMesh(TEXT("Cantina_Facade"), CubeMesh, FVector(HalfX + 5.f, 0.f, H + 120.f), FVector(T, HalfY * 2.f + 80.f, 240.f), Red);
	AddMesh(TEXT("Cantina_DoorTop"), CubeMesh, FVector(HalfX, 0.f, H - 40.f), FVector(T, Door, 80.f), Adobe);
	// Swinging doors (decoration).
	AddMesh(TEXT("Cantina_DoorL"), CubeMesh, FVector(HalfX + 10.f, -80.f, 170.f), FVector(6.f, 150.f, 140.f), DarkWood, FRotator(0.f, 25.f, 0.f), false);
	AddMesh(TEXT("Cantina_DoorR"), CubeMesh, FVector(HalfX + 10.f, 80.f, 170.f), FVector(6.f, 150.f, 140.f), DarkWood, FRotator(0.f, -25.f, 0.f), false);
	// Porch + posts.
	AddMesh(TEXT("Cantina_Porch"), CubeMesh, FVector(HalfX + 160.f, 0.f, 8.f), FVector(300.f, HalfY * 2.f, 16.f), Wood);
	AddMesh(TEXT("Cantina_PorchRoof"), CubeMesh, FVector(HalfX + 170.f, 0.f, 330.f), FVector(320.f, HalfY * 2.f, 12.f), DarkWood, FRotator(0.f, 0.f, 0.f), false);
	AddMesh(TEXT("Cantina_PostL"), CylinderMesh, FVector(HalfX + 300.f, -HalfY + 40.f, 165.f), FVector(16.f, 16.f, 330.f), DarkWood);
	AddMesh(TEXT("Cantina_PostR"), CylinderMesh, FVector(HalfX + 300.f, HalfY - 40.f, 165.f), FVector(16.f, 16.f, 330.f), DarkWood);
	for (int32 i = 0; i < 4; ++i)
	{
		AddMesh(*FString::Printf(TEXT("Cantina_Beam%d"), i), CubeMesh, FVector(-HalfX + 320.f * (i + 1), 0.f, H - 10.f), FVector(24.f, HalfY * 2.f, 24.f), DarkWood, FRotator::ZeroRotator, false);
	}
	// The bar along the back wall, shelves of bottles behind it.
	AddMesh(TEXT("Cantina_Bar"), CubeMesh, FVector(-HalfX + 230.f, 0.f, 60.f), FVector(90.f, 800.f, 110.f), DarkWood);
	AddMesh(TEXT("Cantina_BarTop"), CubeMesh, FVector(-HalfX + 230.f, 0.f, 118.f), FVector(110.f, 820.f, 8.f), Wood, FRotator::ZeroRotator, false);
	AddMesh(TEXT("Cantina_Shelf"), CubeMesh, FVector(-HalfX + 40.f, 0.f, 200.f), FVector(30.f, 760.f, 12.f), Wood, FRotator::ZeroRotator, false);
	for (int32 i = 0; i < 12; ++i)
	{
		const FLinearColor Glass = (i % 3 == 0) ? FLinearColor(0.2f, 0.5f, 0.2f) : (i % 3 == 1 ? FLinearColor(0.55f, 0.35f, 0.1f) : FLinearColor(0.85f, 0.85f, 0.8f));
		AddMesh(*FString::Printf(TEXT("Cantina_Bottle%d"), i), CylinderMesh, FVector(-HalfX + 40.f, -340.f + i * 62.f, 235.f), FVector(10.f, 10.f, 40.f), Glass, FRotator::ZeroRotator, false);
	}
	// Stools + tables.
	for (int32 i = 0; i < 5; ++i)
	{
		AddMesh(*FString::Printf(TEXT("Cantina_Stool%d"), i), CylinderMesh, FVector(-HalfX + 330.f, -300.f + i * 150.f, 35.f), FVector(35.f, 35.f, 70.f), Wood, FRotator::ZeroRotator, false);
	}
	const FVector2D Tables[] = { {-50.f, -330.f}, {-50.f, 330.f}, {350.f, -330.f}, {350.f, 330.f} };
	for (int32 i = 0; i < 4; ++i)
	{
		AddMesh(*FString::Printf(TEXT("Cantina_Table%d"), i), CylinderMesh, FVector(Tables[i].X, Tables[i].Y, 40.f), FVector(110.f, 110.f, 80.f), Wood);
	}
	// Warm hanging lights inside (always on - it's dark in a cantina) + one over the door.
	const FVector LightSpots[] = { {-HalfX + 300.f, 0.f, H - 60.f}, {0.f, -300.f, H - 60.f}, {0.f, 300.f, H - 60.f}, {400.f, 0.f, H - 60.f}, {HalfX + 150.f, 0.f, 300.f} };
	for (int32 i = 0; i < static_cast<int32>(UE_ARRAY_COUNT(LightSpots)); ++i)
	{
		UPointLightComponent* Bulb = CreateDefaultSubobject<UPointLightComponent>(*FString::Printf(TEXT("Cantina_Light%d"), i));
		Bulb->SetupAttachment(Root);
		Bulb->SetRelativeLocation(LightSpots[i]);
		Bulb->SetIntensityUnits(ELightUnits::Candelas);
		Bulb->SetIntensity(i == 4 ? 500.f : 350.f);
		Bulb->SetAttenuationRadius(i == 4 ? 1400.f : 1100.f);
		Bulb->SetLightColor(FLinearColor(1.f, 0.72f, 0.42f));
		Bulb->SetCastShadows(false);
		Bulb->SetMobility(EComponentMobility::Movable);
		AddMesh(*FString::Printf(TEXT("Cantina_Lamp%d"), i), SphereMesh, LightSpots[i] + FVector(0.f, 0.f, 20.f), FVector(28.f, 28.f, 28.f), FLinearColor(1.f, 0.85f, 0.5f), FRotator::ZeroRotator, false);
	}
	if (UTextRenderComponent* Sign = AddLabel(TEXT("Cantina_Sign"), FVector(HalfX + 30.f, 0.f, H + 120.f), 80.f, FColor(255, 215, 80)))
	{
		Sign->SetText(FText::FromString(TEXT("LA CANTINA DEL DIABLO")));
	}
}

void ABRCantina::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRCantina, bFight);
	DOREPLIFETIME(ABRCantina, LocalsStanding);
	DOREPLIFETIME(ABRCantina, FightEndsAt);
}

FVector ABRCantina::Local(float X, float Y, float Z) const
{
	return GetActorTransform().TransformPositionNoScale(FVector(X, Y, Z));
}

float ABRCantina::GetFightSecondsLeft() const
{
	const ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>();
	return GS ? FightEndsAt - GS->GetServerWorldTimeSeconds() : 0.f;
}

bool ABRCantina::IsInside(const FVector& WorldLocation) const
{
	const FVector L = GetActorTransform().InverseTransformPositionNoScale(WorldLocation);
	return FMath::Abs(L.X) < HalfX + 20.f && FMath::Abs(L.Y) < HalfY + 20.f && L.Z < 700.f && L.Z > -200.f;
}

bool ABRCantina::IsBanned(const APVVCharacter* Who) const
{
	const ABRPlayerState* PS = CantinaPS(Who);
	return PS && PS->CantinaStrikes >= StrikesBeforeBanned;
}

void ABRCantina::BeginPlay()
{
	Super::BeginPlay();
	// Real saloon doors: they flap open when you walk into them and swing shut behind you (the old ones were painted on).
	TArray<UStaticMeshComponent*> Parts;
	GetComponents<UStaticMeshComponent>(Parts);
	for (UStaticMeshComponent* Part : Parts)
	{
		if (Part->GetName() == TEXT("Cantina_DoorL") || Part->GetName() == TEXT("Cantina_DoorR"))
		{
			Part->SetVisibility(false);
		}
	}
	if (HasAuthority())
	{
		const FTransform DoorAt(GetActorRotation(), GetActorTransform().TransformPosition(FVector(HalfX + 6.f, 0.f, 0.f)));
		if (ABRSwingDoor* Saloon = GetWorld()->SpawnActorDeferred<ABRSwingDoor>(ABRSwingDoor::StaticClass(), DoorAt, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Saloon->bSaloon = true;
			Saloon->DoorWidth = 300.f;
			Saloon->DoorHeight = 260.f;
			Saloon->FinishSpawning(DoorAt);
		}
	}
	if (UTextRenderComponent* Sign = FindComponentByClass<UTextRenderComponent>())
	{
		Sign->SetText(FText::FromString(CantinaName));
	}
	const UBRSettings* Settings = GetDefault<UBRSettings>();
	if (!Music) Music = Settings->CantinaMusic.LoadSynchronous();
	if (!FightSound) FightSound = Settings->BarFightSound.LoadSynchronous();
	if (!DrinkSound) DrinkSound = Settings->DrinkSound.LoadSynchronous();
	if (Music && GetNetMode() != NM_DedicatedServer)
	{
		// Plays "in your head" (not from a point), but only while YOU are inside the cantina - see Tick.
		MusicAudio = UGameplayStatics::CreateSound2D(this, Music, 1.f, 1.f, 0.f, nullptr, false, false);
		if (MusicAudio)
		{
			MusicAudio->SetVolumeMultiplier(0.001f);
			MusicAudio->Play();
		}
	}
	if (!HasAuthority())
	{
		return;
	}
	// The three things to do.
	struct FSpotDef { EBRCantinaSpot Kind; FVector At; };
	const FSpotDef Spots[] = {
		{ EBRCantinaSpot::Drink, FVector(-HalfX + 330.f, -150.f, 0.f) },
		{ EBRCantinaSpot::Contest, FVector(-50.f, 330.f, 0.f) },
		{ EBRCantinaSpot::Brawl, FVector(-HalfX + 330.f, 260.f, 0.f) },
	};
	for (const FSpotDef& Def : Spots)
	{
		const FTransform T(GetActorRotation(), Local(Def.At.X, Def.At.Y, 0.f));
		if (ABRCantinaSpot* Spot = GetWorld()->SpawnActorDeferred<ABRCantinaSpot>(ABRCantinaSpot::StaticClass(), T, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Spot->Kind = Def.Kind;
			Spot->Cantina = this;
			Spot->FinishSpawning(T);
		}
	}
	// Things to grab and swing (Z): chairs and bottles.
	for (int32 i = 0; i < 10; ++i)
	{
		const bool bBottle = i >= 5;
		const FVector At = bBottle ? Local(-HalfX + 230.f, -320.f + (i - 5) * 150.f, 150.f) : Local(FMath::FRandRange(-150.f, HalfX - 200.f), FMath::FRandRange(-HalfY + 150.f, HalfY - 150.f), 60.f);
		const FTransform T(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), At, bBottle ? FVector(0.08f, 0.08f, 0.26f) : FVector(0.42f, 0.42f, 0.8f));
		if (ABRPhysicsProp* Prop = GetWorld()->SpawnActorDeferred<ABRPhysicsProp>(ABRPhysicsProp::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn))
		{
			Prop->PropShape = bBottle ? 1 : 0;
			Prop->MassKg = bBottle ? 1.f : 7.f;
			Prop->KnockoutSpeed = bBottle ? 450.f : 650.f;
			Prop->Color = bBottle ? (i % 2 ? FLinearColor(0.15f, 0.45f, 0.15f) : FLinearColor(0.5f, 0.3f, 0.08f)) : FLinearColor(0.38f, 0.22f, 0.1f);
			Prop->FinishSpawning(T);
		}
	}
	// The bouncer on the porch.
	if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
	{
		const FVector Door = Local(HalfX + 200.f, 260.f, 110.f);
		Bouncer = GM->SpawnLuchador(FTransform(GetActorRotation(), Door), nullptr);
		if (Bouncer)
		{
			Bouncer->LuchaName = TEXT("EL BOUNCER");
			Bouncer->bCantinaRegular = true;
			Bouncer->SetCombatStats(320.f, 40.f);
			Bouncer->bNoFightBack = false;
			Bouncer->SetActorScale3D(FVector(1.25f));
			Bouncer->StartWandering(Door, 40.f);
		}
	}
	SpawnPatrons();
}

void ABRCantina::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearPatrons();
	if (IsValid(Bouncer))
	{
		Bouncer->Destroy();
	}
	if (MusicAudio)
	{
		MusicAudio->Stop();
		MusicAudio->DestroyComponent();
		MusicAudio = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void ABRCantina::SpawnPatrons()
{
	ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>();
	if (!GM)
	{
		return;
	}
	for (int32 i = 0; i < PatronCount; ++i)
	{
		const FVector At = Local(FMath::FRandRange(-HalfX + 420.f, HalfX - 150.f), FMath::FRandRange(-HalfY + 120.f, HalfY - 120.f), 110.f);
		if (ABRCharacter* P = GM->SpawnNPC(FTransform(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), At)))
		{
			P->bLootWad = false; // bar fight pays out the $50 drops instead
			P->bCantinaRegular = true;
			P->bLootDropped = true;
			P->StartWandering(Local(0.f, 0.f, 110.f), 420.f);
			Patrons.Add(P);
		}
	}
}

void ABRCantina::ClearPatrons()
{
	for (ABRCharacter* P : Patrons)
	{
		if (IsValid(P) && !P->GetCarrier() && P->GetCondition() != EPVVCondition::Detained)
		{
			P->Destroy();
		}
	}
	Patrons.Reset();
}

TArray<ABRCharacter*> ABRCantina::PlayersInside() const
{
	TArray<ABRCharacter*> Out;
	for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
	{
		if (!It->IsNPC() && IsInside(It->GetActorLocation()))
		{
			Out.Add(*It);
		}
	}
	return Out;
}

bool ABRCantina::CanUse(EBRCantinaSpot Kind, const APVVCharacter* Who) const
{
	if (!IsFreePlayer(Who) || IsBanned(Who))
	{
		return false;
	}
	const ABRPlayerState* PS = CantinaPS(Who);
	switch (Kind)
	{
	case EBRCantinaSpot::Drink: return PS && !bFight;
	case EBRCantinaSpot::Contest: return PS && !bFight && !ContestDrinker;
	case EBRCantinaSpot::Brawl: return !bFight;
	default: return false;
	}
}

FText ABRCantina::GetPrompt(EBRCantinaSpot Kind, const APVVCharacter* Who) const
{
	switch (Kind)
	{
	case EBRCantinaSpot::Drink:
		return FText::Format(LOCTEXT("Drink", "Buy a tequila (${0}) - wobblier but harder to knock down"), DrinkPrice);
	case EBRCantinaSpot::Contest:
		return FText::Format(LOCTEXT("Contest", "Drinking contest vs El Borracho: ${0} entry, win ${1} (MASH SPACE)"), ContestEntry, ContestPrize);
	default:
		return FText::Format(LOCTEXT("Brawl", "Shove the big guy - start a BAR FIGHT! (last one standing: ${0} per local)"), CashPerLocal);
	}
}

void ABRCantina::Use(EBRCantinaSpot Kind, APVVCharacter* Who)
{
	ABRCharacter* BRC = Cast<ABRCharacter>(Who);
	ABRPlayerState* PS = CantinaPS(Who);
	if (!HasAuthority() || !BRC || !PS || !CanUse(Kind, Who))
	{
		return;
	}
	if (Kind == EBRCantinaSpot::Drink)
	{
		if (PS->Money < DrinkPrice)
		{
			BRC->Notify(LOCTEXT("NoDrinkMoney", "No money, no tequila, amigo."));
			return;
		}
		PS->AddMoney(-DrinkPrice);
		BRC->MakeTipsy(TipsySeconds);
		ABRGameState::Sfx(this, EBRSfx::Buy, Who->GetActorLocation());
		if (DrinkSound)
		{
			BRAudio::PlayAt(this, DrinkSound, Who->GetActorLocation(), BRAudio::Near);
		}
		BRC->Notify(LOCTEXT("Tipsy", "TEQUILA! You're tipsy: wobblier, but you take hits like a champ."));
		return;
	}
	if (Kind == EBRCantinaSpot::Contest)
	{
		if (PS->Money < ContestEntry)
		{
			BRC->Notify(FText::Format(LOCTEXT("NoContestMoney", "The entry is ${0}."), ContestEntry));
			return;
		}
		const float Now = GetWorld()->GetTimeSeconds();
		if (const float* Until = ContestBannedUntil.Find(BRC))
		{
			if (Now < *Until)
			{
				BRC->Notify(FText::Format(LOCTEXT("ContestWait", "El Borracho: \"Again?! Come back in {0} seconds, I need a nap.\""), FMath::CeilToInt(*Until - Now)));
				return;
			}
		}
		if (BRC->IsTipsy())
		{
			BRC->Notify(LOCTEXT("ContestTipsy", "El Borracho: \"You're already drunk, amigo. Sober up first.\""));
			return;
		}
		PS->AddMoney(-ContestEntry);
		ContestDrinker = BRC;
		ContestStart = Now;
		ContestBannedUntil.Add(BRC, Now + ContestCooldown);
		// El Borracho's form tonight (and he gets better every time he loses).
		RivalRate = FMath::FRandRange(0.15f, 0.2f) + RivalRatePerWin * BorrachoLosses;
		BRC->ChugGag = 0.f;
		BRC->ChugGaggedUntil = 0.f;
		BRC->bChugging = true;
		BRC->ChugMine = 0.f;
		BRC->ChugRival = 0.f;
		BRC->ApplySlow(12.f, 0.f);
		BRC->Notify(LOCTEXT("ContestGo", "DRINKING CONTEST! Tap SPACE in a steady rhythm to chug. Spam it and you'll GAG!"));
		return;
	}
	StartFight(BRC);
}

void ABRCantina::StartFight(ABRCharacter* Starter)
{
	if (!HasAuthority() || bFight)
	{
		return;
	}
	bFight = true;
	bToldFinalDuel = false;
	FightStartTime = GetWorld()->GetTimeSeconds();
	if (const ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>())
	{
		FightEndsAt = GS->GetServerWorldTimeSeconds() + FightSeconds;
	}
	PlayerKOs.Reset();
	PlayersDown.Reset();
	Contestants.Reset();
	Losers.Reset();
	KnockedOut.Reset();
	for (ABRCharacter* P : PlayersInside())
	{
		Contestants.AddUnique(P);
	}
	if (Starter)
	{
		Contestants.AddUnique(Starter);
		if (ABRPlayerState* PS = Starter->GetBRPlayerState())
		{
			++PS->CantinaStrikes;
		}
		if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
		{
			GM->AddTrouble(Starter, 2);
		}
	}
	// Everybody fights everybody. They stay down once they're knocked out (until the fight's over).
	for (ABRCharacter* P : Patrons)
	{
		if (IsStanding(P))
		{
			P->SetKnockdownSeconds(120.f);
			if (ABRCharacter* Foe = PickFoe(P))
			{
				P->StartBrawling(Foe, 60.f, LocalAggression);
			}
		}
	}
	if (FightSound)
	{
		BRAudio::PlayAt(this, FightSound, GetActorLocation(), BRAudio::Loud);
	}
	ABRGameState::Sfx(this, EBRSfx::Whistle, GetActorLocation());
	if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
	{
		GM->Feed(FString::Printf(TEXT("BAR FIGHT at %s!%s"), *CantinaName,
			Starter ? *FString::Printf(TEXT(" (%s started it)"), *Starter->GetPlayerDisplayName()) : TEXT("")));
	}
	for (ABRCharacter* C : Contestants)
	{
		C->Client_Banner(TEXT("BAR FIGHT!"), false);
		C->Notify(FText::Format(LOCTEXT("FightOn", "BAR FIGHT! Be the LAST ONE STANDING to win ${0} per local. Go down and you pay ${1}."), CashPerLocal, LoseFine));
	}
	ForceNetUpdate();
}

ABRCharacter* ABRCantina::PickFoe(ABRCharacter* For) const
{
	// Gang Beasts style: everybody brawls, but it's readable - at most MaxOnOnePlayer locals on any one player
	// at a time (the rest pair off and fight each other), and you always go for the nearest free opponent.
	auto CountOn = [this, For](const ABRCharacter* Who)
	{
		int32 N = 0;
		for (const ABRCharacter* P : Patrons)
		{
			if (P != For && IsValid(P) && P->IsBrawling() && P->GetBrawlTarget() == Who && IsStanding(P))
			{
				++N;
			}
		}
		return N;
	};
	ABRCharacter* Best = nullptr;
	float BestScore = MAX_flt;
	auto Consider = [&](ABRCharacter* O, bool bPlayer)
	{
		if (!O || O == For || !IsStanding(O) || !IsInside(O->GetActorLocation()))
		{
			return;
		}
		const int32 Already = CountOn(O);
		if (Already >= (bPlayer ? MaxOnOnePlayer : 1))
		{
			return; // they've got their hands full
		}
		// Nearest first; players are a little more tempting (the outsider started it).
		const float Score = FVector::Dist(O->GetActorLocation(), For->GetActorLocation()) * (bPlayer ? 0.7f : 1.f) + Already * 400.f;
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = O;
		}
	};
	for (ABRCharacter* C : Contestants)
	{
		if (!Losers.Contains(C))
		{
			Consider(C, true);
		}
	}
	for (ABRCharacter* P : Patrons)
	{
		Consider(P, false);
	}
	return Best;
}

void ABRCantina::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (MusicAudio)
	{
		// Inside: full music. Standing in the doorway: faint. Anywhere else: silent.
		FVector Ear;
		float Want = 0.f;
		if (BRAudio::GetListener(this, Ear))
		{
			const FVector L = GetActorTransform().InverseTransformPositionNoScale(Ear);
			const float Out = FMath::Max(FMath::Abs(L.X) - HalfX, FMath::Abs(L.Y) - HalfY); // cm outside the walls (<0 = inside)
			const bool bHeight = L.Z < 700.f && L.Z > -200.f;
			Want = bHeight ? FMath::Clamp(1.f - Out / MusicSpillDistance, 0.f, 1.f) : 0.f;
			Want = Out <= 0.f && bHeight ? 1.f : Want * 0.35f;
		}
		MusicFade = FMath::FInterpTo(MusicFade, Want, DeltaSeconds, 3.f);
		if (!MusicAudio->IsPlaying())
		{
			MusicAudio->Play(); // keep it looping even if the song isn't set to loop
		}
		MusicAudio->SetVolumeMultiplier(FMath::Max(0.001f, MusicFade * FPVVPlayerOptions::MusicVolume * (bFight ? 0.5f : 1.f)));
	}
	if (!HasAuthority())
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	TickContest(Now);
	if (Now < NextThink)
	{
		return;
	}
	NextThink = Now + 0.25f;
	Patrons.RemoveAll([](const TObjectPtr<ABRCharacter>& P) { return !IsValid(P); });
	TickBouncer();
	if (bFight)
	{
		TickFight();
		return;
	}
	if (RefillAt > 0.f && Now >= RefillAt && PlayersInside().Num() == 0)
	{
		RefillAt = -1.f;
		ClearPatrons();
		SpawnPatrons();
	}
	// Somebody picked a fight with a local in here? That's how bar fights start.
	for (ABRCharacter* P : Patrons)
	{
		const ABRCharacter* Target = P->GetBrawlTarget();
		if (P->IsBrawling() && Target && !Target->IsNPC() && IsInside(P->GetActorLocation()))
		{
			StartFight(const_cast<ABRCharacter*>(Target));
			break;
		}
	}
	// Wandering locals stay inside.
	for (ABRCharacter* P : Patrons)
	{
		if (IsStanding(P) && !P->IsBrawling() && !IsInside(P->GetActorLocation()))
		{
			P->CalmDown(); // no running off down the street - home is the cantina
		}
	}
}

void ABRCantina::TickFight()
{
	const float Now = GetWorld()->GetTimeSeconds();
	// Players who walk in during the fight are in it too.
	for (ABRCharacter* P : PlayersInside())
	{
		if (IsStanding(P) && !Losers.Contains(P))
		{
			Contestants.AddUnique(P);
		}
	}
	// Players: knocked down once = you get back up and keep going. Twice = you lose.
	for (ABRCharacter* C : Contestants)
	{
		if (!IsValid(C) || Losers.Contains(C))
		{
			continue;
		}
		const bool bDown = C->GetCondition() == EPVVCondition::KnockedDown;
		const bool bWasDown = PlayersDown.Contains(C);
		if (bDown && !bWasDown)
		{
			PlayersDown.Add(C);
			int32& KOs = PlayerKOs.FindOrAdd(C);
			++KOs;
			if (KOs < 2)
			{
				C->Client_Banner(TEXT("DOWN! Get up - one more and you're out!"), false);
				continue;
			}
		}
		else if (!bDown && bWasDown)
		{
			PlayersDown.Remove(C);
		}
		if (bDown && PlayerKOs.FindRef(C) >= 2)
		{
			Losers.Add(C);
			if (ABRPlayerState* PS = C->GetBRPlayerState())
			{
				const int32 Fine = FMath::Min(PS->Money, LoseFine);
				PS->AddMoney(-Fine);
				C->Client_Banner(FString::Printf(TEXT("YOU LOST THE BAR FIGHT   -$%d"), Fine), false);
			}
		}
	}
	// Locals keep fighting: when their target drops, pick the next one.
	int32 Standing = 0;
	for (ABRCharacter* P : Patrons)
	{
		if (!IsStanding(P))
		{
			if (P->GetCondition() == EPVVCondition::KnockedDown || P->GetCondition() == EPVVCondition::Restrained)
			{
				KnockedOut.AddUnique(P);
			}
			continue;
		}
		if (!IsInside(P->GetActorLocation()))
		{
			continue; // ran off out the door: out of the fight
		}
		++Standing;
		const ABRCharacter* Target = P->GetBrawlTarget();
		if (!P->IsBrawling() || !IsStanding(Target) || !IsInside(Target->GetActorLocation()))
		{
			if (ABRCharacter* Foe = PickFoe(P))
			{
				P->StartBrawling(Foe, 60.f, LocalAggression);
			}
			else
			{
				P->CalmDown(); // nobody free to fight: wait your turn (cheer them on)
			}
		}
	}
	TArray<ABRCharacter*> Winners;
	for (ABRCharacter* C : Contestants)
	{
		if (IsValid(C) && !Losers.Contains(C) && (IsStanding(C) || C->GetCondition() == EPVVCondition::KnockedDown) && IsInside(C->GetActorLocation()))
		{
			Winners.Add(C);
		}
	}
	if (LocalsStanding != Standing)
	{
		LocalsStanding = Standing;
		ForceNetUpdate();
	}
	const ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>();
	const bool bTimeUp = GS && GS->GetServerWorldTimeSeconds() >= FightEndsAt;
	if (Standing == 0 && Winners.Num() == 1)
	{
		EndFight(Winners); // THE last one standing
	}
	else if (Standing == 0 && Winners.Num() > 1 && !bToldFinalDuel)
	{
		bToldFinalDuel = true; // only players left: they settle it between themselves
		for (ABRCharacter* W : Winners)
		{
			W->Client_Banner(TEXT("LOCALS ARE DOWN - NOW FIGHT EACH OTHER!"), true);
		}
	}
	if (bFight && (Winners.Num() == 0 || bTimeUp))
	{
		EndFight(TArray<ABRCharacter*>(), bTimeUp); // every player is out, or the cops showed up
	}
	(void)Now;
}

void ABRCantina::EndFight(const TArray<ABRCharacter*>& Winners, bool bTimeUp)
{
	if (bTimeUp)
	{
		for (ABRCharacter* C : Contestants)
		{
			if (IsValid(C) && !Losers.Contains(C))
			{
				C->Client_Banner(TEXT("TIME! The cops broke it up - nobody wins"), false);
			}
		}
	}
	bFight = false;
	LocalsStanding = 0;
	RefillAt = GetWorld()->GetTimeSeconds() + RefillSeconds;
	ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>();
	if (Winners.Num() > 0 && GM)
	{
		// $50 next to every knocked-out local - only the winners can pick it up.
		ABRCharacter* Winner = Winners[0];
		int32 Total = 0;
		for (ABRCharacter* Out : KnockedOut)
		{
			if (IsValid(Out))
			{
				GM->DropCash(Out->GetActorLocation() - FVector(0.f, 0.f, 80.f), CashPerLocal, Winner->GetBRPlayerState());
				Total += CashPerLocal;
			}
		}
		TArray<FString> Names = { Winner->GetPlayerDisplayName() };
		Winner->Client_Banner(FString::Printf(TEXT("YOU WON THE BAR FIGHT!   $%d on the floor - only you can grab it"), Total), true);
		GM->Feed(FString::Printf(TEXT("%s won the bar fight at %s!"), *FString::Join(Names, TEXT(" & ")), *CantinaName));
		ABRGameState::Sfx(this, EBRSfx::Payday, GetActorLocation());
	}
	else if (GM)
	{
		GM->Feed(FString::Printf(TEXT("The locals won the bar fight at %s."), *CantinaName));
	}
	for (ABRCharacter* P : Patrons)
	{
		if (IsValid(P))
		{
			P->CalmDown();
			P->SetKnockdownSeconds(14.f);
		}
	}
	PlayerKOs.Reset();
	PlayersDown.Reset();
	Contestants.Reset();
	Losers.Reset();
	KnockedOut.Reset();
	ForceNetUpdate();
}

void ABRCantina::TickContest(float Now)
{
	ABRCharacter* D = ContestDrinker;
	if (!D)
	{
		return;
	}
	if (!IsValid(D) || D->GetCondition() != EPVVCondition::Normal)
	{
		if (IsValid(D))
		{
			D->bChugging = false;
		}
		ContestDrinker = nullptr;
		return;
	}
	const float Dt = GetWorld()->GetDeltaSeconds();
	D->ChugGag = FMath::Max(0.f, D->ChugGag - 0.5f * Dt);
	D->ChugRival = FMath::Min(1.f, D->ChugRival + RivalRate * FMath::FRandRange(0.8f, 1.2f) * Dt);
	const bool bDone = D->ChugMine >= 1.f || D->ChugRival >= 1.f || Now - ContestStart > 12.f;
	if (!bDone)
	{
		return;
	}
	const bool bWon = D->ChugMine > D->ChugRival;
	D->bChugging = false;
	D->ApplySlow(0.1f, 1.f);
	ContestDrinker = nullptr;
	ABRPlayerState* PS = D->GetBRPlayerState();
	ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>();
	if (bWon)
	{
		++BorrachoLosses;
		if (PS)
		{
			PS->AddMoney(ContestPrize);
		}
		D->MakeTipsy(TipsySeconds);
		ABRGameState::Sfx(this, EBRSfx::Payday, D->GetActorLocation());
		ABRGameState::FX(this, EBRFX::MoneyText, D->GetActorLocation() + FVector(0.f, 0.f, 100.f), FString::Printf(TEXT("+$%d"), ContestPrize));
		D->Notify(FText::Format(LOCTEXT("ContestWon", "You drank El Borracho under the table! +${0}"), ContestPrize));
		if (GM)
		{
			GM->Feed(FString::Printf(TEXT("%s won the drinking contest!"), *D->GetPlayerDisplayName()));
		}
	}
	else
	{
		D->MakeTipsy(TipsySeconds * 1.5f);
		D->Notify(LOCTEXT("ContestLost", "El Borracho wins. You... need to lie down."));
		D->ApplyKnockdown(nullptr, -D->GetActorForwardVector() * 150.f); // passes out
	}
}

void ABRCantina::TickBouncer()
{
	if (!IsValid(Bouncer))
	{
		return;
	}
	// The bouncer NEVER comes inside: he guards the door.
	const FVector Door = Local(HalfX + 200.f, 260.f, 110.f);
	if (IsInside(Bouncer->GetActorLocation()) && IsStanding(Bouncer))
	{
		Bouncer->CalmDown();
		Bouncer->TeleportTo(Door, GetActorRotation());
		Bouncer->StartWandering(Door, 40.f);
		return;
	}
	if (!IsStanding(Bouncer))
	{
		return;
	}
	if (!Bouncer->IsBrawling() && FVector::Dist2D(Bouncer->GetActorLocation(), Door) > 300.f)
	{
		Bouncer->CalmDown();
		Bouncer->StartWandering(Door, 40.f); // back to the door
	}
	// Troublemakers trying to get in get shoved back out onto the street.
	const float Now = GetWorld()->GetTimeSeconds();
	const FVector Entrance = Local(HalfX + 120.f, 0.f, 110.f);
	for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
	{
		ABRCharacter* P = *It;
		if (P->IsNPC() || !IsBanned(P) || !IsStanding(P) || IsInside(P->GetActorLocation()) || FVector::Dist2D(P->GetActorLocation(), Entrance) > 320.f)
		{
			continue;
		}
		if (Now - LastBounceTime < 1.2f)
		{
			continue;
		}
		LastBounceTime = Now;
		const FVector Out = GetActorForwardVector().GetSafeNormal2D();
		Bouncer->SetActorRotation(FRotator(0.f, (P->GetActorLocation() - Bouncer->GetActorLocation()).Rotation().Yaw, 0.f));
		P->LaunchCharacter(Out * 900.f + FVector(0.f, 0.f, 250.f), true, true);
		P->Notify(LOCTEXT("Bounced", "\"NOT TONIGHT, AMIGO.\" - you started too many fights in here. The bouncer won't let you in."));
	}
}

// ===========================================================================================
// Smuggling job board

ABRMissionBoard::ABRMissionBoard()
{
	bReplicates = true;
	AddMesh(TEXT("Board"), CubeMesh, FVector(0.f, 0.f, 200.f), FVector(10.f, 220.f, 160.f), FLinearColor(0.35f, 0.22f, 0.1f));
	AddMesh(TEXT("LegL"), CubeMesh, FVector(0.f, -90.f, 60.f), FVector(10.f, 12.f, 120.f), FLinearColor(0.25f, 0.15f, 0.07f));
	AddMesh(TEXT("LegR"), CubeMesh, FVector(0.f, 90.f, 60.f), FVector(10.f, 12.f, 120.f), FLinearColor(0.25f, 0.15f, 0.07f));
	for (int32 i = 0; i < 4; ++i)
	{
		AddMesh(*FString::Printf(TEXT("Note%d"), i), CubeMesh, FVector(7.f, -75.f + i * 50.f, 190.f + (i % 2) * 25.f), FVector(2.f, 38.f, 46.f), FLinearColor(0.92f, 0.88f, 0.75f), FRotator::ZeroRotator, false);
	}
	if (UTextRenderComponent* Sign = AddLabel(TEXT("Sign"), FVector(10.f, 0.f, 300.f), 40.f, FColor(255, 215, 80)))
	{
		Sign->SetText(FText::FromString(TEXT("SE BUSCA: SMUGGLING JOBS")));
	}
}

bool ABRMissionBoard::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	const ABRPlayerState* PS = CantinaPS(Interactor);
	return IsFreePlayer(Interactor) && Interactor->GetFaction() == BR::Runners && PS && PS->MissionPay == 0 && PS->HasFreeSlot();
}

FText ABRMissionBoard::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	return FText::Format(LOCTEXT("TakeJob", "Take a smuggling job: get a package to the Safe Zone in {0} min (${1}-${2})"),
		FMath::RoundToInt(TimeLimit / 60.f), MinPay, MaxPay);
}

void ABRMissionBoard::OnInteract_Implementation(APVVCharacter* Interactor)
{
	ABRPlayerState* PS = CantinaPS(Interactor);
	const ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>();
	ABRCharacter* BRC = Cast<ABRCharacter>(Interactor);
	if (!HasAuthority() || !PS || !GS || !BRC || !CanInteract_Implementation(Interactor) || !PS->AddItem(EBRItem::Package))
	{
		return;
	}
	PS->MissionPay = FMath::RandRange(MinPay, MaxPay) / 10 * 10;
	PS->MissionDeadline = GS->GetServerWorldTimeSeconds() + TimeLimit;
	BRC->Client_Banner(FString::Printf(TEXT("SMUGGLING JOB: $%d   -   %d:00 to reach the Safe Zone"), PS->MissionPay, FMath::RoundToInt(TimeLimit / 60.f)), true);
	BRC->Notify(LOCTEXT("JobTaken", "Package in your pocket. Don't get scanned - it shows up as CONTRABAND!"));
	ABRGameState::Sfx(this, EBRSfx::Buy, GetActorLocation());
}

// ===========================================================================================
// Wanted poster

ABRWantedPoster::ABRWantedPoster()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	AddMesh(TEXT("Poster_Board"), CubeMesh, FVector(0.f, 0.f, 220.f), FVector(4.f, 130.f, 170.f), FLinearColor(0.85f, 0.75f, 0.5f), FRotator::ZeroRotator, false);
	AddMesh(TEXT("Poster_Post"), CylinderMesh, FVector(-8.f, 0.f, 110.f), FVector(12.f, 12.f, 220.f), FLinearColor(0.35f, 0.22f, 0.1f));
	Text = AddLabel(TEXT("Poster_Text"), FVector(4.f, 0.f, 280.f), 18.f, FColor(60, 30, 10));
}

void ABRWantedPoster::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = GetWorld()->GetTimeSeconds();
	if (!Text || GetNetMode() == NM_DedicatedServer || Now < NextRefresh)
	{
		return;
	}
	NextRefresh = Now + 2.f;
	const ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>();
	if (!GS)
	{
		return;
	}
	const ABRPlayerState* Target = Cast<ABRPlayerState>(GS->BountyTarget);
	FString Why = TEXT("BOUNTY");
	if (!Target)
	{
		// No bounty yet: the Runner with the most points.
		for (APlayerState* Each : GS->PlayerArray)
		{
			const ABRPlayerState* PS = Cast<ABRPlayerState>(Each);
			if (PS && PS->GetFaction() == BR::Runners && PS->Points > 0 && (!Target || PS->Points > Target->Points))
			{
				Target = PS;
			}
		}
		Why = TEXT("MOST WANTED");
	}
	Text->SetText(FText::FromString(Target
		? FString::Printf(TEXT("WANTED\n- %s -\n\n%s\n%d pts\nDEAD OR ALIVE"), *Why, *Target->GetPlayerName().ToUpper(), Target->Points)
		: FString(TEXT("WANTED\n\n(nobody...\nyet)\n\nREWARD $$$"))));
}

#undef LOCTEXT_NAMESPACE
