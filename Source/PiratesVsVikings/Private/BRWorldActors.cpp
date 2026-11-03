#include "BRWorldActors.h"
#include "Components/CapsuleComponent.h"
#include "BRSettings.h"
#include "BRCharacter.h"
#include "BRDog.h"
#include "BRGameMode.h"
#include "BRGameState.h"
#include "BRPlayerState.h"
#include "PVVBoat.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/SphereComponent.h"
#include "TimerManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "BRWorld"

namespace
{
	ABRPlayerState* BRState(const APVVCharacter* Character)
	{
		return Character ? Cast<ABRPlayerState>(Character->GetPVVPlayerState()) : nullptr;
	}
	ABRGameState* BRGame(const UObject* Context)
	{
		return Context && Context->GetWorld() ? Context->GetWorld()->GetGameState<ABRGameState>() : nullptr;
	}
	void Tell(const APVVCharacter* Character, const FText& Message)
	{
		if (const ABRCharacter* BRC = Cast<ABRCharacter>(Character))
		{
			BRC->Notify(Message);
		}
	}
	bool IsFree(const APVVCharacter* C, EPVVFaction Team)
	{
		const ABRCharacter* BRC = Cast<ABRCharacter>(C);
		return C && C->GetFaction() == Team && C->GetCondition() == EPVVCondition::Normal && !C->GetCarriedCharacter() && !C->GetCurrentBoat()
			&& !(BRC && BRC->IsNPC());
	}
	bool IsFreeRunner(const APVVCharacter* C) { return IsFree(C, BR::Runners); }
	bool IsFreeWarden(const APVVCharacter* C) { return IsFree(C, BR::Wardens); }
	EPVVFaction LocalViewerTeam(const UWorld* World)
	{
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		const ABRPlayerState* PS = PC ? PC->GetPlayerState<ABRPlayerState>() : nullptr;
		return PS ? PS->GetFaction() : EPVVFaction::None;
	}
	FString Clock(float Seconds)
	{
		const int32 S = FMath::Max(0, FMath::CeilToInt(Seconds));
		return FString::Printf(TEXT("%d:%02d"), S / 60, S % 60);
	}
}

// ===========================================================================================
// Greybox base

ABRGreyboxActor::ABRGreyboxActor()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	CubeMesh = Cube.Object;
	ConeMesh = Cone.Object;
	CylinderMesh = Cylinder.Object;
	SphereMesh = Sphere.Object;
}

UStaticMeshComponent* ABRGreyboxActor::AddMesh(const TCHAR* Name, UStaticMesh* Mesh, const FVector& Location, const FVector& SizeCm,
	const FLinearColor& Color, const FRotator& Rotation, bool bCollide)
{
	UStaticMeshComponent* Comp = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Comp->SetupAttachment(Root);
	Comp->SetStaticMesh(Mesh);
	Comp->SetRelativeLocationAndRotation(Location, Rotation);
	Comp->SetRelativeScale3D(SizeCm / 100.f);
	if (bCollide)
	{
		Comp->SetCollisionProfileName(TEXT("BlockAll"));
	}
	else
	{
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	ComponentColors.Add(FName(Name), Color);
	return Comp;
}

UTextRenderComponent* ABRGreyboxActor::AddLabel(const TCHAR* Name, const FVector& Location, float Size, const FColor& Color)
{
	UTextRenderComponent* Text = CreateDefaultSubobject<UTextRenderComponent>(Name);
	Text->SetupAttachment(Root);
	Text->SetRelativeLocation(Location);
	Text->SetWorldSize(Size);
	Text->SetHorizontalAlignment(EHTA_Center);
	Text->SetTextRenderColor(Color);
	Text->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	return Text;
}

USphereComponent* ABRGreyboxActor::AddPickupZone(float Radius)
{
	USphereComponent* Zone = CreateDefaultSubobject<USphereComponent>(TEXT("PickupZone"));
	Zone->SetupAttachment(Root);
	Zone->SetSphereRadius(Radius);
	Zone->SetRelativeLocation(FVector(0.f, 0.f, Radius * 0.5f));
	Zone->SetCollisionObjectType(ECC_WorldDynamic);
	Zone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Zone->SetCollisionResponseToAllChannels(ECR_Overlap);
	Zone->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Zone->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Zone->SetGenerateOverlapEvents(false);
	Zone->SetCanEverAffectNavigation(false);
	return Zone;
}

void ABRGreyboxActor::StartDespawn(float Seconds)
{
	if (Seconds <= 0.f || IsNetStartupActor())
	{
		return; // placed in the level by you: stays forever
	}
	if (HasAuthority())
	{
		SetLifeSpan(Seconds);
	}
	// Blink for the last 5 seconds so people know it's about to go (each screen does this itself).
	GetWorldTimerManager().SetTimer(BlinkTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		bBlinkVisible = !bBlinkVisible;
		if (Root)
		{
			Root->SetVisibility(bBlinkVisible, true);
		}
	}), 0.2f, true, FMath::Max(0.1f, Seconds - 5.f));
}

void ABRGreyboxActor::Paint(UStaticMeshComponent* Mesh, const FLinearColor& Color)
{
	if (UMaterialInstanceDynamic* MID = Mesh ? Mesh->CreateDynamicMaterialInstance(0) : nullptr)
	{
		MID->SetVectorParameterValue(TEXT("Color"), Color);
	}
}

void ABRGreyboxActor::BeginPlay()
{
	Super::BeginPlay();
	const TCHAR* Kind = IsA<ABRVillage>() ? TEXT("Village") : IsA<ABRWall>() ? TEXT("Wall") : IsA<ABRSpikeFence>() ? TEXT("Fence")
		: IsA<ABRStation>() ? TEXT("Station") : IsA<ABRLuchaRing>() ? TEXT("Ring") : IsA<ABRWatchtower>() ? TEXT("Tower") : TEXT("Other");
	const UBRSettings* Look = GetDefault<UBRSettings>();
	TArray<UStaticMeshComponent*> Meshes;
	GetComponents<UStaticMeshComponent>(Meshes);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (Look->ApplySurface(Mesh, Kind, Mesh->GetName()))
		{
			continue; // your texture (Project Settings -> Border Run -> Test Map Look)
		}
		if (const FLinearColor* Color = ComponentColors.Find(Mesh->GetFName()))
		{
			Paint(Mesh, *Color);
		}
	}
}

// ===========================================================================================
// Station

ABRStation::ABRStation()
{
	Counter = AddMesh(TEXT("Counter"), CubeMesh, FVector(0.f, 0.f, 50.f), FVector(120.f, 70.f, 100.f), FLinearColor(0.5f, 0.5f, 0.5f));
	Label = AddLabel(TEXT("Label"), FVector(0.f, 0.f, 150.f), 32.f, FColor::White);
}

void ABRStation::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRStation, Kind);
	DOREPLIFETIME(ABRStation, Item);
	DOREPLIFETIME(ABRStation, WardenGear);
	DOREPLIFETIME(ABRStation, Price);
	DOREPLIFETIME(ABRStation, WorkSeconds);
	DOREPLIFETIME(ABRStation, CooldownSeconds);
	DOREPLIFETIME(ABRStation, bNightOnly);
	DOREPLIFETIME(ABRStation, Title);
	DOREPLIFETIME(ABRStation, Color);
	DOREPLIFETIME(ABRStation, BetSide);
	DOREPLIFETIME(ABRStation, Pit);
	DOREPLIFETIME(ABRStation, Ring);
	DOREPLIFETIME(ABRStation, NextUseServerTime);
}

void ABRStation::BeginPlay()
{
	ComponentColors.Add(TEXT("Counter"), Color);
	Super::BeginPlay();
	if (HasAuthority())
	{
		if (BoatSpawn.IsNearlyZero())
		{
			BoatSpawn = GetActorTransform().TransformPosition(BoatSpawnOffset);
		}
		if ((Kind == EBRStationKind::LuchaChallenge || Kind == EBRStationKind::LuchaBet) && !Ring)
		{
			float BestRing = MAX_flt;
			for (TActorIterator<ABRLuchaRing> It(GetWorld()); It; ++It)
			{
				const float D = FVector::DistSquared(It->GetActorLocation(), GetActorLocation());
				if (D < BestRing)
				{
					BestRing = D;
					Ring = *It;
				}
			}
		}
		if (Kind == EBRStationKind::Bet && !Pit)
		{
			float Best = MAX_flt;
			for (TActorIterator<ABRCockfightPit> It(GetWorld()); It; ++It)
			{
				const float D = FVector::DistSquared(It->GetActorLocation(), GetActorLocation());
				if (D < Best)
				{
					Best = D;
					Pit = *It;
				}
			}
		}
	}
	FString Text = Title;
	if (Text.IsEmpty())
	{
		// Make a sign from the settings.
		switch (Kind)
		{
		case EBRStationKind::BuyItem: Text = FString::Printf(TEXT("%s $%d"), *BR::ItemName(Item).ToUpper(), Price); break;
		case EBRStationKind::SellItem: Text = FString::Printf(TEXT("WE BUY %s ($%d)"), *BR::ItemName(Item).ToUpper(), Price); break;
		case EBRStationKind::Job: Text = Price > 0 ? FString::Printf(TEXT("WORK HERE ($%d)"), Price) : FString(TEXT("Dig through junk")); break;
		case EBRStationKind::BuyBoat: Text = FString::Printf(TEXT("BOATS $%d"), Price); break;
		case EBRStationKind::Bet: Text = FString::Printf(TEXT("%s %s"), BetSide == 0 ? TEXT("RED") : TEXT("BLUE"), Price > 0 ? *FString::Printf(TEXT("$%d"), Price) : TEXT("ALL IN")); break;
		case EBRStationKind::HireClient: Text = FString::Printf(TEXT("COYOTE: SMUGGLE A CLIENT (+$%d on arrival)"), Price); break;
		case EBRStationKind::LuchaChallenge: Text = FString::Printf(TEXT("FIGHT THE LUCHADOR: $%d (win $%d)"), Price, Price * 2); break;
		case EBRStationKind::LuchaBet: Text = FString::Printf(TEXT("BET $%d ON THE %s"), Price, BetSide == 0 ? TEXT("CHALLENGER") : TEXT("LUCHADOR")); break;
		case EBRStationKind::WardenSupply: Text = FString::Printf(TEXT("GOV. SUPPLY: %s $%d"), *BR::WardenGearName(WardenGear, Item).ToUpper(), Price); break;
		default: break;
		}
	}
	SignText = Text;
	Label->SetText(FText::FromString(Text));
	Label->SetHiddenInGame(!bShowSign);
	Counter->SetHiddenInGame(!bShowCounter);
}

bool ABRStation::IsOpen() const
{
	const ABRGameState* GS = BRGame(this);
	if (!GS || GS->Phase == EBRPhase::Ended || GS->bLobby)
	{
		return false;
	}
	if (bNightOnly && !GS->IsNight())
	{
		return false;
	}
	return GS->GetServerWorldTimeSeconds() >= NextUseServerTime;
}

bool ABRStation::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	if (Kind == EBRStationKind::WardenSupply)
	{
		return IsFreeWarden(Interactor) && IsOpen() && FVector::Dist(Interactor->GetActorLocation(), GetActorLocation()) <= 400.f;
	}
	if (!IsFreeRunner(Interactor) || !IsOpen() || FVector::Dist(Interactor->GetActorLocation(), GetActorLocation()) > 400.f)
	{
		return false;
	}
	const ABRPlayerState* PS = BRState(Interactor);
	if (Kind == EBRStationKind::SellItem)
	{
		return PS && PS->HasItem(Item);
	}
	if (Kind == EBRStationKind::Bet)
	{
		return PS && Pit && Pit->IsTakingBets() && !Pit->HasBet(PS) && PS->Money > 0;
	}
	if (Kind == EBRStationKind::LuchaChallenge)
	{
		return PS && Ring && Ring->IsOpenForChallenge() && PS->Money >= Price;
	}
	if (Kind == EBRStationKind::LuchaBet)
	{
		return PS && Ring && Ring->IsTakingBets() && !Ring->HasBet(PS) && !Ring->IsChallenger(Interactor) && PS->Money > 0;
	}
	return true;
}

namespace
{
	/** Why this Runner can't work this job right now (empty = they can). */
	FText JobRefusal(const ABRStation* Station, const APVVCharacter* Interactor)
	{
		const ABRPlayerState* PS = BRState(Interactor);
		const ABRGameState* GS = BRGame(Station);
		if (!PS || !GS || Station->Kind != EBRStationKind::Job)
		{
			return FText::GetEmpty();
		}
		if (Station->IsAssignableJob())
		{
			if (PS->AssignedJob == TEXT("-"))
			{
				return LOCTEXT("Unemployed", "You're unemployed. Rummage through trash for scrap to sell.");
			}
			if (!PS->AssignedJob.IsEmpty() && PS->AssignedJob != Station->SignText)
			{
				return FText::Format(LOCTEXT("NotYourJob", "Not your job. You work at: {0}"), FText::FromString(PS->AssignedJob));
			}
		}
		const float Wait = PS->JobReadyAt - GS->GetServerWorldTimeSeconds();
		if (Wait > 0.f)
		{
			return FText::Format(LOCTEXT("OnBreak", "On break - next shift in {0} s"), FMath::CeilToInt(Wait));
		}
		return FText::GetEmpty();
	}
}

float ABRStation::GetInteractDuration_Implementation(APVVCharacter* Interactor) const
{
	if (Kind == EBRStationKind::Job)
	{
		return JobRefusal(this, Interactor).IsEmpty() ? WorkSeconds : 0.f;
	}
	return Kind == EBRStationKind::HireClient ? 1.f : 0.f;
}

FText ABRStation::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	const FText ItemText = FText::FromString(BR::ItemName(Item));
	const FText SideText = BetSide == 0 ? LOCTEXT("Red", "RED") : LOCTEXT("Blue", "BLUE");
	switch (Kind)
	{
	case EBRStationKind::BuyItem: return FText::Format(LOCTEXT("Buy", "Buy {0} (${1})"), ItemText, Price);
	case EBRStationKind::SellItem: return FText::Format(LOCTEXT("Sell", "Sell {0} (+${1})"), ItemText, Price);
	case EBRStationKind::BuyBoat: return FText::Format(LOCTEXT("BuyBoat", "Buy a boat (${0}) - delivered to the river"), Price);
	case EBRStationKind::Bet:
		return Price <= 0 ? FText::Format(LOCTEXT("AllIn", "Go ALL IN on the {0} rooster (pays x2.5)"), SideText)
			: FText::Format(LOCTEXT("Bet", "Bet ${0} on the {1} rooster (pays x2)"), Price, SideText);
	case EBRStationKind::LuchaChallenge: return FText::Format(LOCTEXT("LuchaChallenge", "Challenge the luchador for ${0} (win ${1} + his mask)"), Price, Price * 2);
	case EBRStationKind::LuchaBet: return FText::Format(LOCTEXT("LuchaBet", "Bet ${0} on the {1} (pays x2)"), Price,
		BetSide == 0 ? LOCTEXT("TheChallenger", "challenger") : LOCTEXT("TheLuchador", "luchador"));
	case EBRStationKind::HireClient: return FText::Format(LOCTEXT("Hire", "Take a smuggling client (paid ${0} when you get them to the Safe Zone)"), Price);
	case EBRStationKind::WardenSupply:
	{
		const ABRGameState* GS = BRGame(this);
		return FText::Format(LOCTEXT("Supply", "Requisition {0} (${1} of ${2} budget)"), FText::FromString(BR::WardenGearName(WardenGear, Item)), Price, GS ? GS->WardenBudget : 0);
	}
	default:
	{
		const FText Refusal = JobRefusal(this, Interactor);
		if (!Refusal.IsEmpty())
		{
			return Refusal;
		}
		return FText::Format(LOCTEXT("Job", "{0} (hold) +${1}"), FText::FromString(SignText), Price);
	}
	}
}

void ABRStation::OnInteract_Implementation(APVVCharacter* Interactor)
{
	ABRPlayerState* PS = BRState(Interactor);
	ABRGameState* GS = BRGame(this);
	if (!HasAuthority() || !PS || !GS)
	{
		return;
	}

	switch (Kind)
	{
	case EBRStationKind::BuyItem:
		if (PS->Money < Price) { Tell(Interactor, LOCTEXT("Broke", "Not enough money. Go work a job!")); return; }
		if (!PS->HasFreeSlot()) { Tell(Interactor, LOCTEXT("Full", "Your pockets are full (6 slots).")); return; }
		PS->AddMoney(-Price);
		PS->AddItem(Item);
		ABRGameState::Sfx(this, EBRSfx::Buy, GetActorLocation());
		if (Item == EBRItem::WireCutters)
		{
			PS->WireCutterCharges += 2;
			Tell(Interactor, LOCTEXT("BoughtCutters", "Bought: Wire Cutters (good for 2 cuts - and they're LOUD)"));
		}
		else if (Item == EBRItem::Trumpet)
		{
			Tell(Interactor, LOCTEXT("BoughtTrumpet", "Bought: Mariachi Trumpet. Press G near Border Patrol - they can't stop dancing!"));
		}
		else if (Item == EBRItem::Shovel)
		{
			Tell(Interactor, LOCTEXT("BoughtShovel", "Bought: Shovel. Find soft dirt and dig a tunnel under the border (hold E)."));
		}
		else if (Item == EBRItem::CannonSuit)
		{
			if (ABRCharacter* Buyer = Cast<ABRCharacter>(Interactor))
			{
				Buyer->SetHat(EBRHat::CannonHelmet); // you wear it right away
			}
			Tell(Interactor, LOCTEXT("BoughtSuit", "Suited up: Cannonball Helmet (no knockdown when you land). Climb into the Human Cannon once the Line opens!"));
		}
		else
		{
			Tell(Interactor, FText::Format(LOCTEXT("Bought", "Bought: {0}"), FText::FromString(BR::ItemName(Item))));
		}
		break;

	case EBRStationKind::SellItem:
		if (PS->RemoveItem(Item))
		{
			PS->AddMoney(Price);
			ABRGameState::Sfx(this, EBRSfx::Sell, GetActorLocation());
			ABRGameState::FX(this, EBRFX::MoneyText, Interactor->GetActorLocation() + FVector(0.f, 0.f, 60.f), FString::Printf(TEXT("+$%d"), Price));
		}
		break;

	case EBRStationKind::BuyBoat:
	{
		if (PS->Money < Price) { Tell(Interactor, LOCTEXT("BrokeBoat", "Boats aren't free. Pool your money!")); return; }
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		if (GetWorld()->SpawnActor<APVVBoat>(APVVBoat::StaticClass(), BoatSpawn, FRotator::ZeroRotator, Params))
		{
			PS->AddMoney(-Price);
			NextUseServerTime = GS->GetServerWorldTimeSeconds() + 20.f;
			Tell(Interactor, LOCTEXT("BoatReady", "Your boat is waiting on the river bank past the Line."));
		}
		break;
	}

	case EBRStationKind::Bet:
		if (Pit)
		{
			const int32 Amount = Pit->PlaceBet(PS, BetSide, Price);
			if (Amount > 0)
			{
				Tell(Interactor, FText::Format(LOCTEXT("BetPlaced", "You bet ${0} on {1}. Don't look away..."), Amount, BetSide == 0 ? LOCTEXT("RedB", "RED") : LOCTEXT("BlueB", "BLUE")));
			}
		}
		break;

	case EBRStationKind::Job:
	{
		const FText Refusal = JobRefusal(this, Interactor);
		if (!Refusal.IsEmpty())
		{
			Tell(Interactor, Refusal);
			return;
		}
		PS->AddMoney(Price);
		if (Price > 0)
		{
			ABRGameState::Sfx(this, EBRSfx::Sell, GetActorLocation());
			ABRGameState::FX(this, EBRFX::MoneyText, Interactor->GetActorLocation() + FVector(0.f, 0.f, 60.f), FString::Printf(TEXT("+$%d"), Price));
		}
		if (Item != EBRItem::None && !PS->AddItem(Item))
		{
			Tell(Interactor, LOCTEXT("JobFull", "Pockets full - you only got paid."));
		}
		NextUseServerTime = GS->GetServerWorldTimeSeconds() + CooldownSeconds;
		// Each Runner gets a break between shifts, so you can't farm one job forever.
		PS->JobReadyAt = GS->GetServerWorldTimeSeconds() + FMath::Max(CooldownSeconds, Price > 0 ? 20.f : 6.f);
		break;
	}

	case EBRStationKind::HireClient:
		if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
		{
			GM->HireClient(Cast<ABRCharacter>(Interactor), GetActorLocation() + GetActorForwardVector() * 150.f, Price);
		}
		NextUseServerTime = GS->GetServerWorldTimeSeconds() + FMath::Max(CooldownSeconds, 5.f);
		break;

	case EBRStationKind::LuchaChallenge:
		if (Ring)
		{
			Ring->Challenge(Cast<ABRCharacter>(Interactor), Price);
		}
		break;

	case EBRStationKind::LuchaBet:
		if (Ring)
		{
			const int32 Amount = Ring->PlaceBet(PS, BetSide, Price);
			if (Amount > 0)
			{
				Tell(Interactor, FText::Format(LOCTEXT("LuchaBetPlaced", "You bet ${0} on the {1}."), Amount,
					BetSide == 0 ? LOCTEXT("TheChallenger2", "challenger") : LOCTEXT("TheLuchador2", "luchador")));
			}
		}
		break;

	case EBRStationKind::WardenSupply:
	{
		ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>();
		ABRCharacter* Warden = Cast<ABRCharacter>(Interactor);
		if (!GM || !Warden)
		{
			return;
		}
		if (WardenGear == EBRWardenGear::None && (Item == EBRItem::None || !PS->HasFreeSlot()))
		{
			Tell(Interactor, LOCTEXT("SupplyFull", "Your pockets are full (6 slots)."));
			return;
		}
		if (!GM->SpendBudget(Price))
		{
			Tell(Interactor, LOCTEXT("NoBudget", "Budget's spent. Clear legal trucks and make catches to get more funding."));
			return;
		}
		switch (WardenGear)
		{
		case EBRWardenGear::ExtraTraps: GS->BonusTraps += 2; break;
		case EBRWardenGear::HardHat: Warden->SetHat(EBRHat::HardHat); break;
		case EBRWardenGear::DroneRecharge: Warden->RechargeDrone(); break;
		case EBRWardenGear::PatrolDog:
		{
			FActorSpawnParameters DogParams;
			DogParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			UClass* DogType = GM->GetDogClass() ? GM->GetDogClass() : ABRDog::StaticClass();
			GetWorld()->SpawnActor<ABRDog>(DogType, GetActorLocation() + GetActorForwardVector() * 250.f + FVector(0.f, 0.f, 60.f), FRotator::ZeroRotator, DogParams);
			break;
		}
		default: PS->AddItem(Item); break;
		}
		NextUseServerTime = GS->GetServerWorldTimeSeconds() + CooldownSeconds;
		ABRGameState::Sfx(this, EBRSfx::Requisition, GetActorLocation());
		Tell(Interactor, FText::Format(LOCTEXT("Requisitioned", "Requisitioned: {0}. Budget left: ${1}"), FText::FromString(BR::WardenGearName(WardenGear, Item)), GS->WardenBudget));
		break;
	}
	}
}

// ===========================================================================================
// Spike fence

ABRSpikeFence::ABRSpikeFence()
{
	const FLinearColor Steel(0.15f, 0.15f, 0.17f);
	AddMesh(TEXT("Panel"), CubeMesh, FVector(0.f, 0.f, 110.f), FVector(40.f, 400.f, 220.f), Steel);
	for (int32 i = 0; i < 4; ++i)
	{
		const FString Name = FString::Printf(TEXT("Spike%d"), i);
		AddMesh(*Name, ConeMesh, FVector(0.f, -150.f + i * 100.f, 250.f), FVector(30.f, 30.f, 60.f), FLinearColor(0.6f, 0.6f, 0.62f), FRotator::ZeroRotator, false);
	}

	HurtZone = CreateDefaultSubobject<UBoxComponent>(TEXT("HurtZone"));
	HurtZone->SetupAttachment(Root);
	HurtZone->SetRelativeLocation(FVector(0.f, 0.f, 130.f));
	HurtZone->SetBoxExtent(FVector(70.f, 200.f, 150.f));
	HurtZone->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	HurtZone->SetGenerateOverlapEvents(true);
}

void ABRSpikeFence::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		HurtZone->OnComponentBeginOverlap.AddDynamic(this, &ABRSpikeFence::OnHurtOverlap);
	}
}

void ABRSpikeFence::OnHurtOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APVVCharacter* Victim = Cast<APVVCharacter>(OtherActor);
	if (!Victim || Victim->GetCurrentBoat() || Victim->GetCarrier())
	{
		return;
	}
	const FVector Across = GetActorForwardVector().GetSafeNormal2D();
	const float Side = FMath::Sign(FVector::DotProduct(Victim->GetActorLocation() - GetActorLocation(), Across));
	Victim->ReceiveMeleeHit(nullptr, 35.f, Across * Side * 700.f + FVector(0.f, 0.f, 300.f));
	Tell(Victim, LOCTEXT("Ouch", "OUCH! Razor wire. Use wire cutters (or go around)."));
}

bool ABRSpikeFence::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	const ABRPlayerState* PS = BRState(Interactor);
	const ABRGameState* GS = BRGame(this);
	return IsFreeRunner(Interactor) && PS && PS->HasItem(EBRItem::WireCutters) && PS->WireCutterCharges > 0 && GS && GS->Phase == EBRPhase::Invasion;
}

FText ABRSpikeFence::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	const ABRPlayerState* PS = BRState(Interactor);
	return FText::Format(LOCTEXT("Cut", "Cut the fence (hold 8 s - VERY LOUD) - {0} cut(s) left"), PS ? PS->WireCutterCharges : 0);
}

void ABRSpikeFence::OnInteract_Implementation(APVVCharacter* Interactor)
{
	ABRPlayerState* PS = BRState(Interactor);
	if (!HasAuthority() || !PS)
	{
		return;
	}
	if (ABRGameState* GS = BRGame(this))
	{
		GS->Multicast_NoisePing(GetActorLocation());
	}
	if (ABRCharacter* Cutter = Cast<ABRCharacter>(Interactor))
	{
		Cutter->Spot(6.f); // the racket gives you away
	}
	PS->WireCutterCharges = FMath::Max(0, PS->WireCutterCharges - 1);
	if (PS->WireCutterCharges == 0)
	{
		PS->RemoveItem(EBRItem::WireCutters);
		Tell(Interactor, LOCTEXT("Snapped", "SNAP! Your wire cutters broke."));
	}
	Destroy();
}

// ===========================================================================================
// Wall (with a gate gap for the road)

ABRWall::ABRWall()
{
	const FLinearColor Concrete(0.42f, 0.38f, 0.33f);
	const float SouthEnd = GateY - GateHalfWidth;
	const float NorthStart = GateY + GateHalfWidth;
	const float SouthLen = SouthEnd + HalfLength;
	const float NorthLen = HalfLength - NorthStart;
	AddMesh(TEXT("WallSouth"), CubeMesh, FVector(0.f, -HalfLength + SouthLen * 0.5f, Height * 0.5f), FVector(100.f, SouthLen, Height), Concrete);
	AddMesh(TEXT("WallNorth"), CubeMesh, FVector(0.f, NorthStart + NorthLen * 0.5f, Height * 0.5f), FVector(100.f, NorthLen, Height), Concrete);
	AddLabel(TEXT("SignWest"), FVector(-60.f, 2500.f, Height + 60.f), 160.f, FColor(255, 200, 80))->SetText(FText::FromString(TEXT("THE WALL")));
}

bool ABRWall::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	const ABRPlayerState* PS = BRState(Interactor);
	const ABRGameState* GS = BRGame(this);
	const FVector Local = Interactor ? Interactor->GetActorLocation() - GetActorLocation() : FVector::ZeroVector;
	return IsFreeRunner(Interactor) && PS && PS->HasItem(EBRItem::Ladder) && GS && GS->Phase == EBRPhase::Invasion
		&& FMath::Abs(Local.X) < 300.f && FMath::Abs(Local.Y - GateY) > GateHalfWidth + 100.f;
}

FText ABRWall::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	return LOCTEXT("PlaceLadder", "Lean your ladder on the wall (hold)");
}

void ABRWall::OnInteract_Implementation(APVVCharacter* Interactor)
{
	ABRPlayerState* PS = BRState(Interactor);
	if (!HasAuthority() || !PS || !PS->RemoveItem(EBRItem::Ladder))
	{
		return;
	}
	const FVector At(GetActorLocation().X, Interactor->GetActorLocation().Y, GetActorLocation().Z);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	GetWorld()->SpawnActor<ABRLadder>(ABRLadder::StaticClass(), At, FRotator::ZeroRotator, Params);
}

// ===========================================================================================
// Ladder

ABRLadder::ABRLadder()
{
	const FLinearColor Wood(0.55f, 0.35f, 0.15f);
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float S = Side == 0 ? -1.f : 1.f;
		for (int32 Rail = 0; Rail < 2; ++Rail)
		{
			const FString Name = FString::Printf(TEXT("Rail%d%d"), Side, Rail);
			AddMesh(*Name, CubeMesh, FVector(S * 130.f, Rail == 0 ? -30.f : 30.f, 310.f), FVector(8.f, 8.f, 640.f), Wood, FRotator(S * 22.f, 0.f, 0.f), false);
		}
		for (int32 Rung = 0; Rung < 7; ++Rung)
		{
			const FString Name = FString::Printf(TEXT("Rung%d%d"), Side, Rung);
			const float H = 40.f + Rung * 85.f;
			AddMesh(*Name, CubeMesh, FVector(S * (250.f - H * 0.4f), 0.f, H), FVector(6.f, 60.f, 6.f), Wood, FRotator::ZeroRotator, false);
		}
	}

	UseZone = CreateDefaultSubobject<USphereComponent>(TEXT("UseZone"));
	UseZone->SetupAttachment(Root);
	UseZone->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
	UseZone->InitSphereRadius(260.f);
	UseZone->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
}

bool ABRLadder::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	return IsFreeRunner(Interactor) || IsFreeWarden(Interactor);
}

float ABRLadder::GetInteractDuration_Implementation(APVVCharacter* Interactor) const
{
	return Interactor && Interactor->GetFaction() == BR::Wardens ? 1.f : 2.5f;
}

FText ABRLadder::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	return Interactor && Interactor->GetFaction() == BR::Wardens ? LOCTEXT("Kick", "Kick the ladder down (hold)") : LOCTEXT("Climb", "Climb over (hold)");
}

void ABRLadder::OnInteract_Implementation(APVVCharacter* Interactor)
{
	if (!HasAuthority() || !Interactor)
	{
		return;
	}
	if (Interactor->GetFaction() == BR::Wardens)
	{
		Destroy();
		return;
	}
	// Over to the other side of the wall (the ladder faces across it).
	const FVector Across = GetActorForwardVector().GetSafeNormal2D();
	const FVector ToMe = Interactor->GetActorLocation() - GetActorLocation();
	const float Side = FMath::Sign(FVector::DotProduct(ToMe, Across));
	const FVector Along = ToMe - Across * FVector::DotProduct(ToMe, Across);
	const FVector Dest = GetActorLocation() + Along * FVector(1.f, 1.f, 0.f) - Across * Side * 250.f + FVector(0.f, 0.f, ToMe.Z + 30.f);
	Interactor->TeleportTo(Dest, Interactor->GetActorRotation());
}

// ===========================================================================================
// Line barrier

ABRLineBarrier::ABRLineBarrier()
{
	PrimaryActorTick.bCanEverTick = true;
	Barrier = CreateDefaultSubobject<UBoxComponent>(TEXT("Barrier"));
	Barrier->SetupAttachment(Root);
	Barrier->SetBoxExtent(FVector(50.f, 8100.f, 1500.f));
	Barrier->SetRelativeLocation(FVector(0.f, 0.f, 1400.f));
	AddMesh(TEXT("Stripe"), CubeMesh, FVector(0.f, 0.f, 1.f), FVector(200.f, 16200.f, 4.f), FLinearColor(1.f, 0.85f, 0.f), FRotator::ZeroRotator, false);
	AddLabel(TEXT("Sign"), FVector(-120.f, 0.f, 260.f), 120.f, FColor(255, 220, 0))->SetText(FText::FromString(TEXT("THE LINE")));
}

void ABRLineBarrier::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const ABRGameState* GS = BRGame(this);
	const int32 Closed = (GS && GS->Phase == EBRPhase::Prep) ? 1 : 0;
	if (Closed != AppliedState)
	{
		AppliedState = Closed;
		Barrier->SetCollisionProfileName(Closed ? TEXT("BlockAll") : TEXT("NoCollision"));
	}
}

// ===========================================================================================
// Catapult

ABRCatapult::ABRCatapult()
{
	PrimaryActorTick.bCanEverTick = true;
	const FLinearColor Wood(0.45f, 0.28f, 0.12f);
	AddMesh(TEXT("Base"), CubeMesh, FVector(0.f, 0.f, 60.f), FVector(400.f, 260.f, 120.f), Wood);
	AddMesh(TEXT("Post"), CubeMesh, FVector(60.f, 0.f, 220.f), FVector(40.f, 200.f, 200.f), Wood);

	ArmPivot = CreateDefaultSubobject<USceneComponent>(TEXT("ArmPivot"));
	ArmPivot->SetupAttachment(Root);
	ArmPivot->SetRelativeLocation(FVector(60.f, 0.f, 300.f));

	UStaticMeshComponent* Arm = AddMesh(TEXT("Arm"), CubeMesh, FVector(150.f, 0.f, 0.f), FVector(380.f, 30.f, 30.f), Wood, FRotator::ZeroRotator, false);
	Arm->SetupAttachment(ArmPivot);
	UStaticMeshComponent* Cup = AddMesh(TEXT("Cup"), CylinderMesh, FVector(330.f, 0.f, 20.f), FVector(110.f, 110.f, 40.f), FLinearColor(0.3f, 0.2f, 0.1f), FRotator::ZeroRotator, false);
	Cup->SetupAttachment(ArmPivot);

	Basket = CreateDefaultSubobject<USceneComponent>(TEXT("Basket"));
	Basket->SetupAttachment(Root);
	Basket->SetRelativeLocation(FVector(-80.f, 0.f, 620.f)); // where the cup releases (arm swings toward -X)

	AddLabel(TEXT("Sign"), FVector(0.f, 0.f, 520.f), 70.f, FColor(255, 150, 60))->SetText(FText::FromString(TEXT("RETURN TO SENDER")));
}

void ABRCatapult::Launch(APVVCharacter* Passenger, const FVector& Target)
{
	if (!HasAuthority() || !Passenger)
	{
		return;
	}
	// Launch from just above the basket (inside it, the capsule snags on the catapult and freezes).
	const float Half = Passenger->GetCapsuleComponent() ? Passenger->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
	const FVector Start = Basket->GetComponentLocation() + FVector(0.f, 0.f, Half + 60.f);
	if (Passenger->GetCarrier())
	{
		Passenger->GetCarrier()->TakeCarried();
	}
	if (Passenger->GetCondition() == EPVVCondition::Detained)
	{
		Passenger->ReleaseFromDetention(Start);
	}
	else
	{
		Passenger->TeleportTo(Start, Passenger->GetActorRotation(), false, true);
	}

	// Land somewhere around the target, not all in one pile.
	const FVector2D Spread = FVector2D(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f)) * LandingSpread;
	const FVector Delta = Target + FVector(Spread.X, Spread.Y, 0.f) - Start;
	const float Flat = FVector(Delta.X, Delta.Y, 0.f).Size();
	const float Gravity = 980.f;
	const float T = FMath::Max(2.5f, FMath::Sqrt(2.f * Flat / Gravity));
	FVector Velocity = FVector(Delta.X, Delta.Y, 0.f) / T;
	Velocity.Z = Delta.Z / T + 0.5f * Gravity * T;
	if (ABRCharacter* Flyer = Cast<ABRCharacter>(Passenger))
	{
		Flyer->IgnoreDuringFlight(this); // fly straight out of the swinging arm
		Flyer->StartFlight(Velocity);
	}
	else
	{
		Passenger->LaunchCharacter(Velocity, true, true);
	}
	Multicast_Fling();
}

void ABRCatapult::Multicast_Fling_Implementation()
{
	FlingTime = GetWorld()->GetTimeSeconds();
	if (LaunchSound && GetNetMode() != NM_DedicatedServer)
	{
		UGameplayStatics::PlaySoundAtLocation(this, LaunchSound, GetActorLocation());
	}
}

bool ABRCatapult::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	const APVVCharacter* Prisoner = Interactor ? Interactor->GetCarriedCharacter() : nullptr;
	return Interactor && Interactor->GetFaction() == BR::Wardens && Prisoner && Prisoner->GetFaction() == BR::Runners;
}

FText ABRCatapult::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	const APVVCharacter* Prisoner = Interactor ? Interactor->GetCarriedCharacter() : nullptr;
	return FText::Format(LOCTEXT("Fling", "LAUNCH {0} back across the border! (hold)"),
		FText::FromString(Prisoner ? Prisoner->GetPlayerDisplayName() : FString(TEXT("them"))));
}

void ABRCatapult::OnInteract_Implementation(APVVCharacter* Interactor)
{
	if (!HasAuthority() || !Interactor)
	{
		return;
	}
	APVVCharacter* Prisoner = Interactor->TakeCarried();
	ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>();
	if (Prisoner && GM)
	{
		GM->CatapultRunner(Prisoner, this, Interactor);
	}
}

void ABRCatapult::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Age = GetWorld()->GetTimeSeconds() - FlingTime;
	const float Up = Age < 0.15f ? Age / 0.15f : FMath::Clamp(1.f - (Age - 0.15f) / 1.5f, 0.f, 1.f);
	ArmPivot->SetRelativeRotation(FRotator(Up * 110.f, 0.f, 0.f));
}

// ===========================================================================================
// Checkpoint gate

ABRCheckpointGate::ABRCheckpointGate()
{
	PrimaryActorTick.bCanEverTick = true;
	const FLinearColor Post(0.3f, 0.3f, 0.32f);
	const float Half = ABRWall::GateHalfWidth;
	AddMesh(TEXT("PostL"), CubeMesh, FVector(0.f, -Half - 30.f, 350.f), FVector(110.f, 60.f, 700.f), Post);
	AddMesh(TEXT("PostR"), CubeMesh, FVector(0.f, Half + 30.f, 350.f), FVector(110.f, 60.f, 700.f), Post);
	AddMesh(TEXT("Header"), CubeMesh, FVector(0.f, 0.f, 690.f), FVector(110.f, Half * 2.f + 120.f, 40.f), Post);
	AddMesh(TEXT("Booth"), CubeMesh, FVector(400.f, -Half - 250.f, 150.f), FVector(250.f, 250.f, 300.f), FLinearColor(0.85f, 0.85f, 0.8f));
	AddLabel(TEXT("Sign"), FVector(-70.f, 0.f, 720.f), 90.f, FColor(255, 220, 60))->SetText(FText::FromString(TEXT("CHECKPOINT")));

	ArmPivot = CreateDefaultSubobject<USceneComponent>(TEXT("ArmPivot"));
	ArmPivot->SetupAttachment(Root);
	ArmPivot->SetRelativeLocation(FVector(-80.f, -Half, 110.f));
	UStaticMeshComponent* Arm = AddMesh(TEXT("Arm"), CubeMesh, FVector(0.f, Half, 0.f), FVector(20.f, Half * 2.f, 20.f), FLinearColor(0.9f, 0.1f, 0.1f), FRotator::ZeroRotator, false);
	Arm->SetupAttachment(ArmPivot);
	ArmMesh = Arm;

	Barrier = CreateDefaultSubobject<UBoxComponent>(TEXT("Barrier"));
	Barrier->SetupAttachment(Root);
	Barrier->SetBoxExtent(FVector(60.f, Half, 400.f));
	Barrier->SetRelativeLocation(FVector(0.f, 0.f, 400.f));
}

void ABRCheckpointGate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRCheckpointGate, bOpen);
	DOREPLIFETIME(ABRCheckpointGate, bSmashed);
}

void ABRCheckpointGate::OpenFor(float Seconds)
{
	if (HasAuthority())
	{
		bOpen = true;
		CloseAt = GetWorld()->GetTimeSeconds() + Seconds;
	}
}

void ABRCheckpointGate::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = GetWorld()->GetTimeSeconds();
	if (HasAuthority() && bOpen && Now > CloseAt)
	{
		bOpen = false;
	}
	if (HasAuthority() && bSmashed && Now > RepairAt)
	{
		bSmashed = false; // fixed
		OnRep_Smashed();
	}
	// Anything flooring it at the closed barrier smashes straight through.
	if (HasAuthority() && !bOpen && !bSmashed)
	{
		for (TActorIterator<APVVBoat> It(GetWorld()); It; ++It)
		{
			const FVector Local = GetActorTransform().InverseTransformPosition(It->GetActorLocation());
			const float Speed = It->GetVelocity().Size2D();
			const float Toward = FVector::DotProduct(It->GetVelocity().GetSafeNormal2D(), -Local.GetSafeNormal2D());
			if (FMath::Abs(Local.X) < 550.f && FMath::Abs(Local.Y) < ABRWall::GateHalfWidth + 100.f && Speed > 450.f && Toward > 0.3f)
			{
				Smash(*It);
				break;
			}
		}
	}
	ArmAngle = FMath::FInterpTo(ArmAngle, (bOpen && !bSmashed) ? 80.f : 0.f, DeltaSeconds, 4.f);
	ArmPivot->SetRelativeRotation(FRotator(0.f, 0.f, ArmAngle));
	const int32 State = IsOpen() ? 1 : 0;
	if (State != AppliedState)
	{
		AppliedState = State;
		Barrier->SetCollisionProfileName(bOpen ? TEXT("NoCollision") : TEXT("BlockAll"));
	}
}

void ABRCheckpointGate::Smash(AActor* Smasher)
{
	if (!HasAuthority() || bSmashed)
	{
		return;
	}
	bSmashed = true;
	RepairAt = GetWorld()->GetTimeSeconds() + RepairSeconds;
	OnRep_Smashed();
	ForceNetUpdate();
	ABRGameState::Sfx(this, EBRSfx::GateSmash, GetActorLocation());
	ABRGameState::FX(this, EBRFX::Sparks, GetActorLocation() + FVector(0.f, 0.f, 110.f));
	ABRGameState::FX(this, EBRFX::Dust, GetActorLocation() + FVector(0.f, 0.f, 20.f));
	if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
	{
		const APVVBoat* Ride = Cast<APVVBoat>(Smasher);
		const APVVCharacter* Driver = Ride ? Ride->GetDriver() : nullptr;
		const ABRCharacter* BRDriver = Cast<ABRCharacter>(Driver);
		if (Driver && !(BRDriver && BRDriver->IsNPC()))
		{
			GM->Feed(FString::Printf(TEXT("%s SMASHED THROUGH the checkpoint!"), *Driver->GetPlayerDisplayName()));
		}
		else
		{
			GM->Feed(TEXT("The checkpoint barrier got SMASHED!"));
		}
	}
}

void ABRCheckpointGate::OnRep_Smashed()
{
	if (ArmMesh)
	{
		ArmMesh->SetHiddenInGame(bSmashed);
	}
	for (UStaticMeshComponent* Piece : Debris)
	{
		if (Piece)
		{
			Piece->DestroyComponent();
		}
	}
	Debris.Reset();
	if (!bSmashed || GetNetMode() == NM_DedicatedServer || !ArmMesh)
	{
		return;
	}
	// The arm snaps into chunks that go flying down the road (just for looks; they vanish when it's fixed).
	const FVector ArmStart = ArmPivot->GetComponentLocation();
	const FVector Along = GetActorRightVector();
	const float GateHalf = ABRWall::GateHalfWidth;
	for (int32 i = 0; i < 5; ++i)
	{
		UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(this);
		Piece->SetStaticMesh(ArmMesh->GetStaticMesh());
		Piece->SetWorldLocation(ArmStart + Along * (GateHalf * 2.f) * ((i + 0.5f) / 5.f));
		Piece->SetWorldRotation(GetActorRotation());
		Piece->SetWorldScale3D(FVector(0.2f, GateHalf * 2.f / 100.f / 5.f, 0.2f));
		Piece->SetCollisionProfileName(TEXT("PhysicsActor"));
		Piece->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Piece->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Ignore);
		Piece->RegisterComponent();
		for (int32 m = 0; m < ArmMesh->GetNumMaterials(); ++m)
		{
			Piece->SetMaterial(m, ArmMesh->GetMaterial(m));
		}
		Piece->SetSimulatePhysics(true);
		Piece->AddImpulse(GetActorForwardVector() * FMath::FRandRange(400.f, 900.f) + FVector(FMath::FRandRange(-200.f, 200.f), FMath::FRandRange(-200.f, 200.f), FMath::FRandRange(300.f, 700.f)), NAME_None, true);
		Piece->AddAngularImpulseInDegrees(FVector(FMath::FRandRange(-600.f, 600.f), FMath::FRandRange(-600.f, 600.f), FMath::FRandRange(-600.f, 600.f)), NAME_None, true);
		Debris.Add(Piece);
	}
}

bool ABRCheckpointGate::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	return IsFreeWarden(Interactor);
}

FText ABRCheckpointGate::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	if (bSmashed)
	{
		return LOCTEXT("GateSmashed", "The barrier's SMASHED (fixed soon)");
	}
	return bOpen ? LOCTEXT("CloseGate", "Close the gate") : LOCTEXT("OpenGate", "Wave them through (open gate 12 s)");
}

void ABRCheckpointGate::OnInteract_Implementation(APVVCharacter* Interactor)
{
	if (!HasAuthority() || bSmashed)
	{
		return;
	}
	if (bOpen)
	{
		bOpen = false;
	}
	else
	{
		OpenFor(12.f);
	}
}

// ===========================================================================================
// Traps

ABRTrap::ABRTrap()
{
	PrimaryActorTick.bCanEverTick = true;
	const FLinearColor Metal(0.35f, 0.35f, 0.38f);
	// Bear trap: two jaws with teeth.
	AddMesh(TEXT("Bear_Base"), CylinderMesh, FVector(0.f, 0.f, 3.f), FVector(80.f, 80.f, 6.f), Metal, FRotator::ZeroRotator, false);
	AddMesh(TEXT("Bear_JawA"), CylinderMesh, FVector(0.f, -22.f, 14.f), FVector(70.f, 10.f, 16.f), Metal, FRotator(0.f, 0.f, -35.f), false);
	AddMesh(TEXT("Bear_JawB"), CylinderMesh, FVector(0.f, 22.f, 14.f), FVector(70.f, 10.f, 16.f), Metal, FRotator(0.f, 0.f, 35.f), false);
	// Glue pad.
	AddMesh(TEXT("Glue_Pad"), CylinderMesh, FVector(0.f, 0.f, 1.5f), FVector(140.f, 140.f, 3.f), FLinearColor(0.2f, 0.45f, 0.1f), FRotator::ZeroRotator, false);
	// Alarm tripwire.
	AddMesh(TEXT("Alarm_Box"), CubeMesh, FVector(0.f, -80.f, 12.f), FVector(20.f, 20.f, 24.f), FLinearColor(0.7f, 0.1f, 0.1f), FRotator::ZeroRotator, false);
	AddMesh(TEXT("Alarm_Wire"), CubeMesh, FVector(0.f, 0.f, 10.f), FVector(2.f, 160.f, 2.f), FLinearColor(0.8f, 0.8f, 0.8f), FRotator::ZeroRotator, false);

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(Root);
	Trigger->InitSphereRadius(80.f);
	Trigger->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
	Trigger->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Trigger->SetGenerateOverlapEvents(true);
}

void ABRTrap::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRTrap, Type);
}

void ABRTrap::BeginPlay()
{
	Super::BeginPlay();
	const FString Prefix = Type == EBRTrap::Bear ? TEXT("Bear") : (Type == EBRTrap::Glue ? TEXT("Glue") : TEXT("Alarm"));
	TArray<UStaticMeshComponent*> Meshes;
	GetComponents<UStaticMeshComponent>(Meshes);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		Mesh->SetVisibility(Mesh->GetName().StartsWith(Prefix));
	}
	if (HasAuthority())
	{
		Trigger->OnComponentBeginOverlap.AddDynamic(this, &ABRTrap::OnTriggered);
	}
}

void ABRTrap::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Runners only spot traps up close; Wardens always see their own.
	const UWorld* World = GetWorld();
	const APlayerController* PC = World->GetFirstPlayerController();
	bool bHide = false;
	if (LocalViewerTeam(World) == BR::Runners && PC && PC->PlayerCameraManager)
	{
		const APawn* Pawn = PC->GetPawn();
		bHide = !Pawn || FVector::Dist(Pawn->GetActorLocation(), GetActorLocation()) > 600.f;
	}
	if (bHide != bHiddenLocally)
	{
		bHiddenLocally = bHide;
		TArray<UStaticMeshComponent*> Meshes;
		GetComponents<UStaticMeshComponent>(Meshes);
		const FString Prefix = Type == EBRTrap::Bear ? TEXT("Bear") : (Type == EBRTrap::Glue ? TEXT("Glue") : TEXT("Alarm"));
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			Mesh->SetVisibility(!bHide && Mesh->GetName().StartsWith(Prefix));
		}
	}
}

void ABRTrap::OnTriggered(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ABRCharacter* Victim = Cast<ABRCharacter>(OtherActor);
	if (!Victim || !Victim->IsRunner() || Victim->IsNPC() || Victim->GetCurrentBoat() || Victim->GetCarrier() || Victim->GetCondition() != EPVVCondition::Normal)
	{
		return;
	}
	ABRGameState* GS = BRGame(this);
	switch (Type)
	{
	case EBRTrap::Bear:
		Victim->ApplySlow(5.f, 0.f);
		Victim->Spot(5.f);
		ABRGameState::Sfx(this, EBRSfx::TrapSnap, GetActorLocation());
		Victim->Notify(LOCTEXT("BearTrap", "CLANG! Bear trap - you're stuck!"));
		break;
	case EBRTrap::Glue:
		Victim->ApplySlow(8.f, 0.3f);
		ABRGameState::Sfx(this, EBRSfx::TrapSnap, GetActorLocation());
		Victim->Notify(LOCTEXT("Glue", "Ugh, glue! You can barely move."));
		break;
	case EBRTrap::Alarm:
		Victim->Spot(8.f);
		ABRGameState::Sfx(this, EBRSfx::TripwireAlarm, GetActorLocation());
		Victim->Notify(LOCTEXT("Alarm", "You tripped an alarm wire! Wardens can see you."));
		break;
	}
	if (GS)
	{
		GS->Multicast_NoisePing(GetActorLocation());
	}
	Destroy();
}

bool ABRTrap::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	return IsFreeRunner(Interactor) && FVector::Dist(Interactor->GetActorLocation(), GetActorLocation()) < 300.f;
}

FText ABRTrap::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	return LOCTEXT("Disarm", "Disarm the trap (hold)");
}

void ABRTrap::OnInteract_Implementation(APVVCharacter* Interactor)
{
	if (HasAuthority())
	{
		Tell(Interactor, LOCTEXT("Disarmed", "Trap disarmed. Nice."));
		Destroy();
	}
}

// ===========================================================================================
// Cockfight pit

ABRCockfightPit::ABRCockfightPit()
{
	PrimaryActorTick.bCanEverTick = true;
	const FLinearColor Plank(0.5f, 0.32f, 0.15f);
	AddMesh(TEXT("Floor"), CylinderMesh, FVector(0.f, 0.f, 2.f), FVector(600.f, 600.f, 4.f), FLinearColor(0.75f, 0.6f, 0.35f), FRotator::ZeroRotator, false);
	AddMesh(TEXT("WallN"), CubeMesh, FVector(0.f, 310.f, 40.f), FVector(640.f, 20.f, 80.f), Plank);
	AddMesh(TEXT("WallS"), CubeMesh, FVector(0.f, -310.f, 40.f), FVector(640.f, 20.f, 80.f), Plank);
	AddMesh(TEXT("WallE"), CubeMesh, FVector(310.f, 0.f, 40.f), FVector(20.f, 640.f, 80.f), Plank);
	AddMesh(TEXT("WallW"), CubeMesh, FVector(-310.f, 0.f, 40.f), FVector(20.f, 640.f, 80.f), Plank);
	AddMesh(TEXT("BoardPost"), CubeMesh, FVector(0.f, 360.f, 200.f), FVector(20.f, 20.f, 400.f), Plank, FRotator::ZeroRotator, false);

	Board = AddLabel(TEXT("Board"), FVector(0.f, 340.f, 420.f), 55.f, FColor(255, 230, 120));
	Board->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));

	RoosterRed = CreateDefaultSubobject<USceneComponent>(TEXT("RoosterRed"));
	RoosterRed->SetupAttachment(Root);
	RoosterBlue = CreateDefaultSubobject<USceneComponent>(TEXT("RoosterBlue"));
	RoosterBlue->SetupAttachment(Root);
	BuildRooster(RoosterRed, TEXT("Red"), FLinearColor(0.85f, 0.05f, 0.03f));
	BuildRooster(RoosterBlue, TEXT("Blue"), FLinearColor(0.05f, 0.2f, 0.95f));

	// Team corners: red half / blue half of the floor + a flag on each side.
	AddMesh(TEXT("Team_FloorRed"), CubeMesh, FVector(-150.f, 0.f, 5.f), FVector(290.f, 580.f, 2.f), FLinearColor(0.55f, 0.08f, 0.05f), FRotator::ZeroRotator, false);
	AddMesh(TEXT("Team_FloorBlue"), CubeMesh, FVector(150.f, 0.f, 5.f), FVector(290.f, 580.f, 2.f), FLinearColor(0.06f, 0.12f, 0.5f), FRotator::ZeroRotator, false);
	AddMesh(TEXT("Team_PostRed"), CylinderMesh, FVector(-345.f, -300.f, 160.f), FVector(10.f, 10.f, 320.f), Plank, FRotator::ZeroRotator, false);
	AddMesh(TEXT("Team_FlagRed"), CubeMesh, FVector(-345.f, -255.f, 285.f), FVector(4.f, 90.f, 60.f), FLinearColor(0.85f, 0.05f, 0.03f), FRotator::ZeroRotator, false);
	AddMesh(TEXT("Team_PostBlue"), CylinderMesh, FVector(345.f, -300.f, 160.f), FVector(10.f, 10.f, 320.f), Plank, FRotator::ZeroRotator, false);
	AddMesh(TEXT("Team_FlagBlue"), CubeMesh, FVector(345.f, -255.f, 285.f), FVector(4.f, 90.f, 60.f), FLinearColor(0.05f, 0.2f, 0.95f), FRotator::ZeroRotator, false);

	// Big RED / BLUE signs floating over each bird (always turned to face you).
	RedTag = AddLabel(TEXT("RedTag"), FVector(-200.f, 0.f, 190.f), 60.f, FColor(255, 40, 30));
	RedTag->SetText(FText::FromString(TEXT("RED")));
	BlueTag = AddLabel(TEXT("BlueTag"), FVector(200.f, 0.f, 190.f), 60.f, FColor(60, 120, 255));
	BlueTag->SetText(FText::FromString(TEXT("BLUE")));
}

void ABRCockfightPit::SetupRoosterLooks()
{
	for (int32 Side = 0; Side < 2; ++Side)
	{
		USceneComponent* Bird = Side == 0 ? RoosterRed : RoosterBlue;
		const FLinearColor Team = Side == 0 ? RedTeamColor : BlueTeamColor;
		if (!Bird)
		{
			continue;
		}
		TArray<USceneComponent*> Parts;
		Bird->GetChildrenComponents(false, Parts);
		for (USceneComponent* Child : Parts)
		{
			UStaticMeshComponent* Part = Cast<UStaticMeshComponent>(Child);
			if (!Part)
			{
				continue;
			}
			const FString Name = Part->GetName();
			const bool bTeamPart = Name.EndsWith(TEXT("_Collar")) || Name.EndsWith(TEXT("_Tag"));
			if (Name.EndsWith(TEXT("_Collar")))
			{
				Part->SetRelativeLocation(CollarLocation);
				Part->SetRelativeScale3D(CollarSize / 100.f);
			}
			if (Name.EndsWith(TEXT("_Tag")))
			{
				Part->SetRelativeLocation(CollarLocation + FVector(CollarSize.X * 0.5f, 0.f, -CollarSize.Z));
			}
			if (bTeamPart || Name.EndsWith(TEXT("_Tail")) || Name.EndsWith(TEXT("_Sash")) || Name.Contains(TEXT("_Wing")))
			{
				Paint(Part, Team);
			}
			if (RoosterModel && !bTeamPart)
			{
				Part->SetVisibility(false); // your model replaces the greybox bird
			}
		}
		if (RoosterModel && GetNetMode() != NM_DedicatedServer)
		{
			UStaticMeshComponent* Model = NewObject<UStaticMeshComponent>(this, Side == 0 ? TEXT("RedRoosterModel") : TEXT("BlueRoosterModel"));
			Model->SetStaticMesh(RoosterModel);
			Model->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Model->SetupAttachment(Bird);
			Model->RegisterComponent();
			Model->SetRelativeTransform(FTransform(RoosterModelRotation, RoosterModelOffset, RoosterModelScale));
		}
		if (UTextRenderComponent* Tag = Side == 0 ? RedTag.Get() : BlueTag.Get())
		{
			Tag->SetTextRenderColor(Team.ToFColor(true));
		}
	}
	TArray<UStaticMeshComponent*> Meshes;
	GetComponents<UStaticMeshComponent>(Meshes);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		const FString Name = Mesh->GetName();
		if (Name == TEXT("Team_FlagRed"))
		{
			Paint(Mesh, RedTeamColor);
		}
		else if (Name == TEXT("Team_FlagBlue"))
		{
			Paint(Mesh, BlueTeamColor);
		}
	}
}

void ABRCockfightPit::BuildRooster(USceneComponent* Parent, const TCHAR* Prefix, const FLinearColor& Accent)
{
	auto Piece = [&](const TCHAR* Part, UStaticMesh* Mesh, const FVector& At, const FVector& Size, const FLinearColor& Color, const FRotator& Rot = FRotator::ZeroRotator)
	{
		const FString Name = FString::Printf(TEXT("%s_%s"), Prefix, Part);
		UStaticMeshComponent* C = AddMesh(*Name, Mesh, At, Size, Color, Rot, false);
		C->SetupAttachment(Parent);
	};
	const FLinearColor Feathers = FLinearColor(0.85f, 0.8f, 0.7f);
	Piece(TEXT("Body"), SphereMesh, FVector(0.f, 0.f, 45.f), FVector(70.f, 50.f, 55.f), Feathers);
	Piece(TEXT("Head"), SphereMesh, FVector(30.f, 0.f, 85.f), FVector(30.f, 28.f, 30.f), Feathers);
	Piece(TEXT("Comb"), ConeMesh, FVector(30.f, 0.f, 105.f), FVector(10.f, 16.f, 22.f), FLinearColor(0.9f, 0.05f, 0.05f));
	Piece(TEXT("Beak"), ConeMesh, FVector(48.f, 0.f, 84.f), FVector(10.f, 10.f, 16.f), FLinearColor(1.f, 0.75f, 0.1f), FRotator(-90.f, 0.f, 0.f));
	Piece(TEXT("Tail"), ConeMesh, FVector(-38.f, 0.f, 70.f), FVector(25.f, 15.f, 50.f), Accent, FRotator(35.f, 0.f, 0.f));
	Piece(TEXT("Sash"), CylinderMesh, FVector(0.f, 0.f, 50.f), FVector(55.f, 55.f, 8.f), Accent);
	// Team collar around the neck + a hanging tag, and wings in the team colour.
	Piece(TEXT("Collar"), CylinderMesh, FVector(28.f, 0.f, 68.f), FVector(34.f, 34.f, 9.f), Accent);
	Piece(TEXT("Tag"), SphereMesh, FVector(45.f, 0.f, 59.f), FVector(12.f, 12.f, 12.f), Accent);
	Piece(TEXT("WingL"), SphereMesh, FVector(-5.f, -27.f, 50.f), FVector(50.f, 12.f, 32.f), Accent, FRotator(-15.f, 0.f, 0.f));
	Piece(TEXT("WingR"), SphereMesh, FVector(-5.f, 27.f, 50.f), FVector(50.f, 12.f, 32.f), Accent, FRotator(-15.f, 0.f, 0.f));
	Piece(TEXT("LegL"), CylinderMesh, FVector(0.f, -12.f, 12.f), FVector(5.f, 5.f, 24.f), FLinearColor(1.f, 0.75f, 0.1f));
	Piece(TEXT("LegR"), CylinderMesh, FVector(0.f, 12.f, 12.f), FVector(5.f, 5.f, 24.f), FLinearColor(1.f, 0.75f, 0.1f));
}

void ABRCockfightPit::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRCockfightPit, State);
	DOREPLIFETIME(ABRCockfightPit, StateEndsAt);
	DOREPLIFETIME(ABRCockfightPit, Winner);
	DOREPLIFETIME(ABRCockfightPit, PotRed);
	DOREPLIFETIME(ABRCockfightPit, PotBlue);
}

void ABRCockfightPit::BeginPlay()
{
	Super::BeginPlay();
	SetupRoosterLooks();
	if (HasAuthority() && bSpawnBetCounters)
	{
		struct FBetSpot { FVector Offset; int32 Side; int32 Amount; };
		const FBetSpot Spots[] = { {FVector(-550.f, -250.f, 0.f), 0, 20}, {FVector(-550.f, 250.f, 0.f), 0, 0},
			{FVector(550.f, -250.f, 0.f), 1, 20}, {FVector(550.f, 250.f, 0.f), 1, 0} };
		for (const FBetSpot& Spot : Spots)
		{
			const FTransform T(GetActorRotation(), GetActorTransform().TransformPositionNoScale(Spot.Offset));
			if (ABRStation* S = GetWorld()->SpawnActorDeferred<ABRStation>(ABRStation::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				S->Kind = EBRStationKind::Bet;
				S->BetSide = Spot.Side;
				S->Price = Spot.Amount;
				S->Pit = this;
				S->Color = Spot.Side == 0 ? RedTeamColor : BlueTeamColor;
				S->FinishSpawning(T);
			}
		}
	}
	if (HasAuthority())
	{
		const ABRGameState* GS = BRGame(this);
		State = BRPit::Betting;
		StateEndsAt = (GS ? GS->GetServerWorldTimeSeconds() : 0.f) + BettingSeconds;
	}
}

bool ABRCockfightPit::HasBet(const ABRPlayerState* Player) const
{
	for (const FBRPitBet& Bet : Bets)
	{
		if (Bet.Player.Get() == Player)
		{
			return true;
		}
	}
	return false;
}

int32 ABRCockfightPit::PlaceBet(ABRPlayerState* Player, int32 Side, int32 Amount)
{
	if (!HasAuthority() || !Player || State != BRPit::Betting || HasBet(Player) || Player->Money <= 0)
	{
		return 0;
	}
	FBRPitBet Bet;
	Bet.Player = Player;
	Bet.Side = Side == 0 ? 0 : 1;
	Bet.bAllIn = Amount <= 0;
	Bet.Amount = Bet.bAllIn ? Player->Money : FMath::Min(Amount, Player->Money);
	Player->AddMoney(-Bet.Amount);
	(Bet.Side == 0 ? PotRed : PotBlue) += Bet.Amount;
	Bets.Add(Bet);
	return Bet.Amount;
}

void ABRCockfightPit::AdvanceState(float ServerNow)
{
	switch (State)
	{
	case BRPit::Betting:
		State = BRPit::Fighting;
		Winner = FMath::RandBool() ? 0 : 1;
		StateEndsAt = ServerNow + FightSeconds;
		break;
	case BRPit::Fighting:
		State = BRPit::Result;
		StateEndsAt = ServerNow + ResultSeconds;
		PayOut();
		break;
	default:
		State = BRPit::Betting;
		StateEndsAt = ServerNow + BettingSeconds;
		Bets.Reset();
		PotRed = 0;
		PotBlue = 0;
		break;
	}
}

void ABRCockfightPit::PayOut()
{
	const FText WinnerText = Winner == 0 ? LOCTEXT("RedWins", "RED") : LOCTEXT("BlueWins", "BLUE");
	for (const FBRPitBet& Bet : Bets)
	{
		ABRPlayerState* PS = Bet.Player.Get();
		if (!PS)
		{
			continue;
		}
		const ABRCharacter* Gambler = Cast<ABRCharacter>(PS->GetPawn());
		if (Bet.Side == Winner)
		{
			const int32 Payout = Bet.bAllIn ? (Bet.Amount * 5) / 2 : Bet.Amount * 2;
			PS->AddMoney(Payout);
			if (Gambler) Gambler->Notify(FText::Format(LOCTEXT("Won", "{0} WINS! You collect ${1}!"), WinnerText, Payout));
		}
		else if (Gambler)
		{
			Gambler->Notify(FText::Format(LOCTEXT("Lost", "{0} wins... you lost ${1}."), WinnerText, Bet.Amount));
		}
	}
	if (ABRGameState* GS = BRGame(this))
	{
		if (Bets.Num() > 0)
		{
			GS->Multicast_Announce(FText::Format(LOCTEXT("PitResult", "COCKFIGHT: the {0} rooster wins! (pot ${1})"), WinnerText, PotRed + PotBlue));
		}
	}
}

void ABRCockfightPit::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const ABRGameState* GS = BRGame(this);
	const float ServerNow = GS ? GS->GetServerWorldTimeSeconds() : 0.f;
	if (HasAuthority() && GS && ServerNow >= StateEndsAt)
	{
		AdvanceState(ServerNow);
	}

	// Board.
	const float Left = StateEndsAt - ServerNow;
	FString Text;
	switch (State)
	{
	case BRPit::Betting: Text = FString::Printf(TEXT("COCKFIGHT - BETS OPEN %s\nRED $%d   vs   BLUE $%d"), *Clock(Left), PotRed, PotBlue); break;
	case BRPit::Fighting: Text = TEXT("FIGHT!!!"); break;
	default: Text = Winner == 0 ? TEXT("RED WINS!") : TEXT("BLUE WINS!"); break;
	}
	Board->SetText(FText::FromString(Text));

	// Cartoon fight animation.
	const float T = GetWorld()->GetTimeSeconds();
	for (int32 Side = 0; Side < 2; ++Side)
	{
		USceneComponent* Bird = Side == 0 ? RoosterRed : RoosterBlue;
		const float Dir = Side == 0 ? -1.f : 1.f;
		const float Phase = Side * 1.7f;
		FVector Pos;
		FRotator Rot(0.f, Side == 0 ? 0.f : 180.f, 0.f);
		if (State == BRPit::Betting)
		{
			Pos = FVector(Dir * 200.f, 0.f, FMath::Abs(FMath::Sin(T * 2.f + Phase)) * 6.f);
			Rot.Yaw += FMath::Sin(T * 1.3f + Phase) * 20.f;
		}
		else if (State == BRPit::Fighting)
		{
			Pos = FVector(Dir * (55.f + 45.f * FMath::Sin(T * 2.3f + Phase)), 25.f * FMath::Sin(T * 3.1f + Phase), FMath::Abs(FMath::Sin(T * 9.f + Phase)) * 50.f);
			Rot.Pitch = FMath::Sin(T * 14.f + Phase) * 25.f;
			Rot.Yaw += FMath::Sin(T * 5.f + Phase) * 25.f;
		}
		else if (Side == Winner)
		{
			Pos = FVector(0.f, 0.f, FMath::Abs(FMath::Sin(T * 6.f)) * 70.f);
			Rot.Yaw = T * 200.f;
		}
		else
		{
			Pos = FVector(Dir * 150.f, 0.f, 0.f);
			Rot.Roll = 90.f; // KO'd
		}
		Bird->SetRelativeLocationAndRotation(Pos, Rot);

		// RED / BLUE sign follows its bird and turns to face this player's camera.
		if (UTextRenderComponent* Tag = Side == 0 ? RedTag.Get() : BlueTag.Get())
		{
			Tag->SetRelativeLocation(FVector(Pos.X, Pos.Y, 170.f + Pos.Z * 0.3f));
			const APlayerController* PC = GetWorld()->GetFirstPlayerController();
			if (PC && PC->PlayerCameraManager)
			{
				const FVector ToCamera = PC->PlayerCameraManager->GetCameraLocation() - Tag->GetComponentLocation();
				Tag->SetWorldRotation(FRotator(0.f, ToCamera.Rotation().Yaw, 0.f));
			}
		}
	}
}

// ===========================================================================================
// Tracer FX

ABRTracerFX::ABRTracerFX()
{
	InitialLifeSpan = 0.09f;
	USceneComponent* FXRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(FXRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Beam = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beam"));
	Beam->SetupAttachment(FXRoot);
	Beam->SetStaticMesh(Cylinder.Object);
	Beam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Beam->SetCastShadow(false);
	Impact = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Impact"));
	Impact->SetupAttachment(FXRoot);
	Impact->SetStaticMesh(Sphere.Object);
	Impact->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Impact->SetCastShadow(false);

	Flash = CreateDefaultSubobject<UPointLightComponent>(TEXT("Flash"));
	Flash->SetupAttachment(FXRoot);
	Flash->SetIntensity(15000.f);
	Flash->SetAttenuationRadius(500.f);
	Flash->SetCastShadows(false);
}

void ABRTracerFX::Init(const FVector& Start, const FVector& End, bool bLethal, bool bHitSomeone)
{
	const FVector Dir = End - Start;
	const float Length = FMath::Max(Dir.Size(), 1.f);
	const FLinearColor Color = bLethal ? FLinearColor(1.f, 0.7f, 0.2f) : FLinearColor(0.3f, 0.8f, 1.f);

	Beam->SetWorldLocationAndRotation(Start + Dir * 0.5f, FRotationMatrix::MakeFromZ(Dir).Rotator());
	Beam->SetWorldScale3D(FVector(bLethal ? 0.02f : 0.04f, bLethal ? 0.02f : 0.04f, Length / 100.f));
	Impact->SetWorldLocation(End);
	Impact->SetWorldScale3D(FVector(bHitSomeone ? 0.35f : 0.15f));
	Flash->SetWorldLocation(Start);
	Flash->SetLightColor(Color);

	for (UStaticMeshComponent* Mesh : { Beam.Get(), Impact.Get() })
	{
		if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), bHitSomeone ? FLinearColor(1.f, 0.1f, 0.1f) : Color);
		}
	}
}

// ===========================================================================================
// Human cannon

ABRCannon::ABRCannon()
{
	PrimaryActorTick.bCanEverTick = true;
	const FLinearColor Iron(0.12f, 0.12f, 0.14f);
	const FLinearColor Red(0.7f, 0.1f, 0.08f);
	const FLinearColor Gold(0.9f, 0.7f, 0.2f);
	AddMesh(TEXT("Carriage"), CubeMesh, FVector(-40.f, 0.f, 60.f), FVector(260.f, 160.f, 70.f), Red);
	AddMesh(TEXT("WheelL"), CylinderMesh, FVector(-30.f, -95.f, 60.f), FVector(120.f, 120.f, 20.f), Gold, FRotator(0.f, 0.f, 90.f));
	AddMesh(TEXT("WheelR"), CylinderMesh, FVector(-30.f, 95.f, 60.f), FVector(120.f, 120.f, 20.f), Gold, FRotator(0.f, 0.f, 90.f));

	BarrelPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BarrelPivot"));
	BarrelPivot->SetupAttachment(Root);
	BarrelPivot->SetRelativeLocation(FVector(0.f, 0.f, 130.f));
	UStaticMeshComponent* Barrel = AddMesh(TEXT("Barrel"), CylinderMesh, FVector(150.f, 0.f, 0.f), FVector(90.f, 90.f, 360.f), Iron, FRotator(-90.f, 0.f, 0.f));
	Barrel->SetupAttachment(BarrelPivot);
	UStaticMeshComponent* Rim = AddMesh(TEXT("Rim"), CylinderMesh, FVector(330.f, 0.f, 0.f), FVector(110.f, 110.f, 20.f), Gold, FRotator(-90.f, 0.f, 0.f), false);
	Rim->SetupAttachment(BarrelPivot);
	UStaticMeshComponent* Stripe = AddMesh(TEXT("Stripe"), CylinderMesh, FVector(120.f, 0.f, 0.f), FVector(96.f, 96.f, 30.f), Red, FRotator(-90.f, 0.f, 0.f), false);
	Stripe->SetupAttachment(BarrelPivot);

	Muzzle = CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));
	Muzzle->SetupAttachment(BarrelPivot);
	Muzzle->SetRelativeLocation(FVector(380.f, 0.f, 0.f));

	Sign = AddLabel(TEXT("Sign"), FVector(-40.f, 0.f, 330.f), 55.f, FColor(255, 200, 60));
}

void ABRCannon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRCannon, Loaded);
	DOREPLIFETIME(ABRCannon, Aim);
	DOREPLIFETIME(ABRCannon, PluggedUntil);
	DOREPLIFETIME(ABRCannon, FuseLitAt);
}

bool ABRCannon::PhaseAllowsFiring() const
{
	const ABRGameState* GS = BRGame(this);
	return GS && !GS->bLobby && (GS->Phase == EBRPhase::Invasion || (bUsableDuringPrep && GS->Phase == EBRPhase::Prep));
}

void ABRCannon::FindRiders(TArray<ABRCharacter*>& Out) const
{
	// Anyone whose body is along the top of the barrel (between the back end and the muzzle).
	const FVector Back = BarrelPivot->GetComponentLocation() - BarrelPivot->GetForwardVector() * 40.f;
	const FVector Front = Muzzle->GetComponentLocation() + BarrelPivot->GetForwardVector() * 40.f;
	for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
	{
		ABRCharacter* C = *It;
		if (C->GetCarrier() || C->GetCurrentBoat() || C->IsInCannon() || C->GetCondition() == EPVVCondition::Detained || C->GetCondition() == EPVVCondition::KnockedDown)
		{
			continue;
		}
		const FVector P = C->GetActorLocation();
		if (FMath::PointDistToSegment(P, Back, Front) < 175.f && P.Z > BarrelPivot->GetComponentLocation().Z - 10.f)
		{
			Out.Add(C);
		}
	}
}

bool ABRCannon::IsRider(const APVVCharacter* Who) const
{
	TArray<ABRCharacter*> Riders;
	FindRiders(Riders);
	return Riders.ContainsByPredicate([Who](const ABRCharacter* R) { return R == Who; });
}

FVector ABRCannon::FusePoint() const
{
	return BarrelPivot->GetComponentLocation() - BarrelPivot->GetForwardVector() * 30.f + FVector(0.f, 0.f, 50.f);
}

void ABRCannon::FireRiders()
{
	FuseLitAt = 0.f;
	TArray<ABRCharacter*> Riders;
	FindRiders(Riders);
	const FVector Dir = BarrelPivot->GetForwardVector();
	ABRGameState::FX(this, EBRFX::Smoke, Muzzle->GetComponentLocation() + Dir * 80.f);
	Multicast_Boom();
	if (Loaded)
	{
		Fire(); // whoever is inside goes too
	}
	int32 Index = 0;
	for (ABRCharacter* Rider : Riders)
	{
		Rider->TeleportTo(Muzzle->GetComponentLocation() + Dir * (60.f + Index * 90.f) + FVector(0.f, 0.f, 40.f), FRotator(0.f, Dir.Rotation().Yaw, 0.f), false, true);
		Rider->StartFlight(Dir * LaunchSpeed * FMath::FRandRange(0.85f, 1.f) + FVector(FMath::FRandRange(-150.f, 150.f), FMath::FRandRange(-150.f, 150.f), 0.f));
		Rider->Spot(8.f);
		ABRGameState::Highlight(this, Rider, 100.f + Index, FString::Printf(TEXT("%s got blasted off the top of the cannon"), *Rider->GetPlayerDisplayName()));
		if (ABRPlayerState* PS = Rider->GetBRPlayerState())
		{
			++PS->StatCannonShots;
		}
		if (ABRGameState* GS = BRGame(this))
		{
			GS->Multicast_Announce(FText::Format(LOCTEXT("CannonRider", "BOOM! {0} got blasted off the top of the cannon!"), FText::FromString(Rider->GetPlayerDisplayName())));
		}
		++Index;
	}
	if (ABRGameState* GS = BRGame(this))
	{
		GS->Multicast_NoisePing(GetActorLocation());
	}
	ForceNetUpdate();
}

bool ABRCannon::IsPlugged() const
{
	const ABRGameState* GS = BRGame(this);
	return GS && GS->GetServerWorldTimeSeconds() < PluggedUntil;
}

bool ABRCannon::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	if (!Interactor)
	{
		return false;
	}
	if (Loaded)
	{
		return Interactor == Loaded; // only the one inside can fire it
	}
	if (IsPlugged() || FuseLitAt > 0.f)
	{
		return false;
	}
	if (IsFreeWarden(Interactor))
	{
		return true; // plug it
	}
	// Someone's sitting on the barrel: any Runner can light the fuse (even the one sitting there).
	TArray<ABRCharacter*> Riders;
	FindRiders(Riders);
	if (IsFreeRunner(Interactor) && Riders.Num() > 0 && PhaseAllowsFiring())
	{
		return true;
	}
	if (IsFreeRunner(Interactor) && !IsRider(Interactor))
	{
		return true; // climb up on the barrel (or into it with a suit)
	}
	const ABRPlayerState* PS = BRState(Interactor);
	const ABRGameState* GS = BRGame(this);
	const bool bPhaseOK = GS && (GS->Phase == EBRPhase::Invasion || (bUsableDuringPrep && GS->Phase == EBRPhase::Prep));
	const ABRCharacter* BRC = Cast<ABRCharacter>(Interactor);
	const bool bSuited = (PS && PS->HasItem(EBRItem::CannonSuit)) || (BRC && BRC->GetHat() == EBRHat::CannonHelmet);
	return IsFreeRunner(Interactor) && bSuited && bPhaseOK;
}

float ABRCannon::GetInteractDuration_Implementation(APVVCharacter* Interactor) const
{
	if (Loaded && Interactor == Loaded)
	{
		return 0.f; // fire instantly
	}
	if (Interactor && Interactor->GetFaction() == BR::Wardens)
	{
		return 4.f;
	}
	TArray<ABRCharacter*> Riders;
	FindRiders(Riders);
	if (Riders.Num() > 0 && PhaseAllowsFiring())
	{
		return 0.5f; // strike a match
	}
	return 1.f;
}

FText ABRCannon::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	if (Loaded && Interactor == Loaded)
	{
		return LOCTEXT("CannonFire", "AIM with the mouse - press E to FIRE!");
	}
	if (Interactor && Interactor->GetFaction() == BR::Wardens)
	{
		return LOCTEXT("CannonPlug", "Plug the cannon (hold)");
	}
	TArray<ABRCharacter*> Riders;
	FindRiders(Riders);
	if (Riders.Num() > 0 && PhaseAllowsFiring())
	{
		return IsRider(Interactor) ? LOCTEXT("LightOwn", "LIGHT THE FUSE (you're sitting on it...)") : LOCTEXT("LightFuse", "LIGHT THE FUSE!");
	}
	const ABRPlayerState* PS = BRState(Interactor);
	const ABRCharacter* BRC = Cast<ABRCharacter>(Interactor);
	const bool bSuited = (PS && PS->HasItem(EBRItem::CannonSuit)) || (BRC && BRC->GetHat() == EBRHat::CannonHelmet);
	if (bSuited && PhaseAllowsFiring())
	{
		return LOCTEXT("CannonLoad", "Climb INTO the cannon (aim it yourself)");
	}
	return LOCTEXT("CannonClimb", "Climb up on the barrel (get someone to light the fuse!)");
}

void ABRCannon::OnInteract_Implementation(APVVCharacter* Interactor)
{
	if (!HasAuthority() || !Interactor)
	{
		return;
	}
	ABRGameState* GS = BRGame(this);
	if (Loaded && Interactor == Loaded)
	{
		Fire();
		return;
	}
	if (Interactor->GetFaction() == BR::Wardens)
	{
		FuseLitAt = 0.f;
		PluggedUntil = (GS ? GS->GetServerWorldTimeSeconds() : 0.f) + PluggedSeconds;
		Tell(Interactor, LOCTEXT("Plugged", "Cannon plugged. Nobody's flying anywhere for a while."));
		return;
	}
	ABRPlayerState* PS = BRState(Interactor);
	ABRCharacter* Runner = Cast<ABRCharacter>(Interactor);
	if (!PS || !Runner)
	{
		return;
	}
	// Someone on the barrel: light the fuse.
	TArray<ABRCharacter*> Riders;
	FindRiders(Riders);
	if (Riders.Num() > 0 && PhaseAllowsFiring())
	{
		FuseLitAt = GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
		LastFuseCount = -1;
		if (FuseSound)
		{
			UGameplayStatics::SpawnSoundAtLocation(this, FuseSound, FusePoint());
		}
		if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
		{
			GM->Feed(FString::Printf(TEXT("%s lit the cannon fuse..."), *Runner->GetPlayerDisplayName()));
		}
		ForceNetUpdate();
		return;
	}
	const bool bSuited = PS->HasItem(EBRItem::CannonSuit) || Runner->GetHat() == EBRHat::CannonHelmet;
	if (!bSuited || !PhaseAllowsFiring())
	{
		// No suit: just climb up on top of the barrel. Someone else lights the fuse.
		const FVector Top = BarrelPivot->GetComponentLocation() + BarrelPivot->GetForwardVector() * 230.f + FVector(0.f, 0.f, 140.f);
		Runner->TeleportTo(Top, FRotator(0.f, GetActorRotation().Yaw + 180.f, 0.f), false, true);
		return;
	}
	if (Runner->GetHat() != EBRHat::CannonHelmet && !PS->RemoveItem(EBRItem::CannonSuit))
	{
		return;
	}
	if (Runner->GetHat() != EBRHat::CannonHelmet)
	{
		Runner->SetHat(EBRHat::CannonHelmet); // suit on
	}
	Loaded = Runner;
	Aim = FRotator(45.f, 0.f, 0.f);
	Runner->SetInCannon(true);
	Runner->TeleportTo(BarrelPivot->GetComponentLocation(), Runner->GetActorRotation(), false, true);
	Tell(Interactor, LOCTEXT("InCannon", "You're in the cannon! AIM with the mouse, press E to FIRE."));
	ForceNetUpdate();
}

void ABRCannon::Fire()
{
	ABRCharacter* Runner = Cast<ABRCharacter>(Loaded.Get());
	Loaded = nullptr;
	if (!Runner)
	{
		return;
	}
	const FVector Dir = BarrelPivot->GetForwardVector();
	Runner->SetInCannon(false);
	Runner->TeleportTo(Muzzle->GetComponentLocation() + Dir * 60.f, FRotator(0.f, Dir.Rotation().Yaw, 0.f), false, true);
	Runner->StartFlight(Dir * LaunchSpeed);
	ABRGameState::Highlight(this, Runner, 100.f, FString::Printf(TEXT("%s fired themselves out of a cannon"), *Runner->GetPlayerDisplayName()));
	ABRGameState::FX(this, EBRFX::Smoke, Muzzle->GetComponentLocation() + Dir * 80.f);
	Runner->Spot(8.f); // everyone saw that
	if (ABRPlayerState* PS = Runner->GetBRPlayerState())
	{
		++PS->StatCannonShots;
	}
	if (ABRGameState* GS = BRGame(this))
	{
		GS->Multicast_NoisePing(GetActorLocation());
		GS->Multicast_Announce(FText::Format(LOCTEXT("CannonFired", "BOOM! {0} just fired themselves out of a cannon!"),
			FText::FromString(Runner->GetPlayerDisplayName())));
	}
	Multicast_Boom();
	ForceNetUpdate();
}

void ABRCannon::Unload()
{
	if (ABRCharacter* Runner = Cast<ABRCharacter>(Loaded.Get()))
	{
		Runner->SetInCannon(false);
		Runner->TeleportTo(GetActorLocation() - GetActorForwardVector() * 250.f + FVector(0.f, 0.f, 100.f), Runner->GetActorRotation(), false, true);
	}
	Loaded = nullptr;
}

void ABRCannon::Multicast_Boom_Implementation()
{
	BoomTime = GetWorld()->GetTimeSeconds();
	if (FireSound && GetNetMode() != NM_DedicatedServer)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, Muzzle->GetComponentLocation());
	}
}

void ABRCannon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const ABRGameState* GS = BRGame(this);

	if (HasAuthority() && Loaded)
	{
		// Whoever is inside aims with their mouse.
		const ABRCharacter* Runner = Cast<ABRCharacter>(Loaded.Get());
		if (!Runner || Runner->GetCondition() != EPVVCondition::Normal || !Runner->IsInCannon() || (GS && GS->Phase == EBRPhase::Ended))
		{
			Unload();
		}
		else
		{
			const AController* C = Runner->GetController();
			const FRotator View = C ? C->GetControlRotation() : FRotator::ZeroRotator;
			const float RelYaw = FMath::Clamp(FRotator::NormalizeAxis(View.Yaw - GetActorRotation().Yaw), -YawRange, YawRange);
			const float Pitch = FMath::Clamp(FRotator::NormalizeAxis(View.Pitch) + 45.f, MinPitch, MaxPitch);
			Aim = FRotator(Pitch, RelYaw, 0.f);
			Loaded->SetActorLocation(BarrelPivot->GetComponentLocation());
		}
	}

	// Fuse burning: sparks on every screen, BOOM on the server.
	if (FuseLitAt > 0.f && GS)
	{
		const float Burnt = GS->GetServerWorldTimeSeconds() - FuseLitAt;
		const float Now = GetWorld()->GetTimeSeconds();
		if (GetNetMode() != NM_DedicatedServer && Now >= NextSpark)
		{
			NextSpark = Now + 0.15f;
			ABRGameState::SpawnLocalFX(this, EBRFX::Sparks, FusePoint() + FMath::VRand() * 10.f);
		}
		if (HasAuthority())
		{
			const int32 Count = FMath::CeilToInt(FuseSeconds - Burnt);
			if (Count != LastFuseCount && Count > 0)
			{
				LastFuseCount = Count;
				ABRGameState::Sfx(this, EBRSfx::Countdown, FusePoint());
			}
			if (Burnt >= FuseSeconds)
			{
				FireRiders();
			}
		}
	}

	// Barrel follows the aim (smoothly on every screen) and kicks back when it fires.
	const FRotator Current = BarrelPivot->GetRelativeRotation();
	const FRotator Target = Loaded ? Aim : FRotator(30.f, 0.f, 0.f);
	BarrelPivot->SetRelativeRotation(FMath::RInterpTo(Current, Target, DeltaSeconds, 6.f));
	const float Kick = FMath::Clamp(1.f - (GetWorld()->GetTimeSeconds() - BoomTime) / 0.4f, 0.f, 1.f);
	BarrelPivot->SetRelativeLocation(FVector(-60.f * Kick, 0.f, 130.f));

	if (Sign)
	{
		FString Text = TEXT("HUMAN CANNON\nclimb on the barrel - get a friend to light the fuse");
		if (FuseLitAt > 0.f && GS)
		{
			Text = FString::Printf(TEXT("FUSE LIT!\n%d..."), FMath::Max(1, FMath::CeilToInt(FuseSeconds - (GS->GetServerWorldTimeSeconds() - FuseLitAt))));
		}
		if (GS && GS->GetServerWorldTimeSeconds() < PluggedUntil)
		{
			const int32 Left = FMath::CeilToInt(PluggedUntil - GS->GetServerWorldTimeSeconds());
			Text = FString::Printf(TEXT("PLUGGED BY BORDER PATROL\n%d:%02d"), Left / 60, Left % 60);
		}
		else if (Loaded)
		{
			Text = TEXT("LOADED!");
		}
		Sign->SetText(FText::FromString(Text));
	}
}

// ===========================================================================================
// Physics prop

ABRPhysicsProp::ABRPhysicsProp()
{
	bReplicates = true;
	SetReplicateMovement(true);
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Mesh->SetStaticMesh(Cube.Object);
	Mesh->SetRelativeScale3D(FVector(0.6f));
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetSimulatePhysics(true);
	Mesh->SetNotifyRigidBodyCollision(true);
}

void ABRPhysicsProp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRPhysicsProp, Holder);
}

void ABRPhysicsProp::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (PropMesh)
	{
		Mesh->SetStaticMesh(PropMesh);
	}
}

void ABRPhysicsProp::BeginPlay()
{
	Super::BeginPlay();
	Mesh->SetMassOverrideInKg(NAME_None, MassKg, true);
	if (!PropMesh)
	{
		if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
		}
	}
	if (HasAuthority())
	{
		Mesh->OnComponentHit.AddDynamic(this, &ABRPhysicsProp::OnPropHit);
	}
}

void ABRPhysicsProp::ApplyHeldState()
{
	if (Holder)
	{
		Mesh->SetSimulatePhysics(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (USkeletalMeshComponent* Hand = Holder->GetMesh())
		{
			const FName Socket = Hand->DoesSocketExist(TEXT("hand_r")) ? FName(TEXT("hand_r")) : NAME_None;
			AttachToComponent(Hand, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
			SetActorRelativeLocation(FVector(0.f, 10.f, 0.f));
		}
	}
	else
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
		Mesh->SetSimulatePhysics(true);
	}
}

void ABRPhysicsProp::OnRep_Holder()
{
	ApplyHeldState();
}

void ABRPhysicsProp::PickUp(ABRCharacter* By)
{
	if (!HasAuthority() || !By || Holder)
	{
		return;
	}
	Holder = By;
	ApplyHeldState();
	ForceNetUpdate();
}

void ABRPhysicsProp::Throw(const FVector& Velocity)
{
	if (!HasAuthority() || !Holder)
	{
		return;
	}
	LastThrower = Holder;
	ThrownTime = GetWorld()->GetTimeSeconds();
	const FVector From = Holder->GetActorLocation() + Holder->GetActorForwardVector() * 90.f + FVector(0.f, 0.f, 40.f);
	Holder = nullptr;
	ApplyHeldState();
	SetActorLocation(From, false, nullptr, ETeleportType::TeleportPhysics);
	Mesh->SetPhysicsLinearVelocity(Velocity);
	Mesh->SetPhysicsAngularVelocityInDegrees(FVector(FMath::FRandRange(-400.f, 400.f), FMath::FRandRange(-400.f, 400.f), 0.f));
	ForceNetUpdate();
}

void ABRPhysicsProp::Drop()
{
	if (!HasAuthority() || !Holder)
	{
		return;
	}
	Holder = nullptr;
	ApplyHeldState();
	ForceNetUpdate();
}

void ABRPhysicsProp::OnPropHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	APVVCharacter* Victim = Cast<APVVCharacter>(OtherActor);
	const FVector Vel = Mesh->GetPhysicsLinearVelocity();
	if (!Victim || Holder || Vel.Size() < KnockoutSpeed || Victim == LastThrower.Get() || Victim->GetCondition() != EPVVCondition::Normal)
	{
		return;
	}
	// Bonk.
	Victim->ApplyKnockdown(LastThrower.Get(), Vel.GetSafeNormal2D() * 500.f + FVector(0.f, 0.f, 250.f));
	if (ABRCharacter* Thrower = LastThrower.Get())
	{
		if (ABRPlayerState* PS = Thrower->GetBRPlayerState())
		{
			++PS->StatThrows;
		}
	}
}

// ===========================================================================================
// Chicken

ABRChicken::ABRChicken()
{
	PrimaryActorTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Mesh->SetStaticMesh(Sphere.Object);
	const float BodyScale = 0.32f;
	Mesh->SetRelativeScale3D(FVector(BodyScale));
	MassKg = 2.5f;
	KnockoutSpeed = 700.f;
	Color = FLinearColor(0.92f, 0.9f, 0.85f);
	auto Part = [&](const TCHAR* Name, UStaticMesh* PartMesh, const FVector& AtCm, const FVector& SizeCm, const FLinearColor& PartColor)
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Mesh);
		C->SetStaticMesh(PartMesh);
		C->SetUsingAbsoluteScale(true);
		C->SetRelativeLocation(AtCm / BodyScale); // parent is scaled
		C->SetWorldScale3D(SizeCm / 100.f);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->ComponentTags.Add(PartColor == Color ? FName(TEXT("Feather")) : FName(*FString::Printf(TEXT("Color_%.2f_%.2f_%.2f"), PartColor.R, PartColor.G, PartColor.B)));
	};
	Part(TEXT("Head"), Sphere.Object, FVector(17.f, 0.f, 16.f), FVector(16.f, 15.f, 16.f), Color);
	Part(TEXT("Comb"), Cone.Object, FVector(17.f, 0.f, 27.f), FVector(6.f, 8.f, 10.f), FLinearColor(0.9f, 0.05f, 0.05f));
	Part(TEXT("Beak"), Cone.Object, FVector(27.f, 0.f, 15.f), FVector(6.f, 6.f, 8.f), FLinearColor(1.f, 0.75f, 0.1f));
	Part(TEXT("Tail"), Sphere.Object, FVector(-17.f, 0.f, 8.f), FVector(12.f, 18.f, 16.f), Color);
	Part(TEXT("Legs"), Cylinder.Object, FVector(0.f, 0.f, -18.f), FVector(4.f, 14.f, 12.f), FLinearColor(1.f, 0.75f, 0.1f));
}

void ABRChicken::BeginPlay()
{
	Super::BeginPlay();
	Home = GetActorLocation();
	NextHop = FMath::FRandRange(0.5f, 3.f);
	// Paint the head, comb, beak... (each part carries its colour in a tag).
	TArray<UStaticMeshComponent*> Parts;
	GetComponents<UStaticMeshComponent>(Parts);
	for (UStaticMeshComponent* C : Parts)
	{
		if (C == Mesh || C->ComponentTags.Num() == 0)
		{
			continue;
		}
		if (C->ComponentTags[0] == TEXT("Feather"))
		{
			if (UMaterialInstanceDynamic* MID = C->CreateDynamicMaterialInstance(0))
			{
				MID->SetVectorParameterValue(TEXT("Color"), Color); // same feathers as the body
			}
			continue;
		}
		TArray<FString> Bits;
		C->ComponentTags[0].ToString().ParseIntoArray(Bits, TEXT("_"));
		if (Bits.Num() == 4)
		{
			if (UMaterialInstanceDynamic* MID = C->CreateDynamicMaterialInstance(0))
			{
				MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(FCString::Atof(*Bits[1]), FCString::Atof(*Bits[2]), FCString::Atof(*Bits[3])));
			}
		}
	}
}

void ABRChicken::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || IsHeld() || !Mesh->IsSimulatingPhysics())
	{
		return;
	}
	// Keep it on its feet: no rolling around like a ball (thrown chickens still fly).
	if (FBodyInstance* Body = Mesh->GetBodyInstance(); Body && !Body->bLockXRotation)
	{
		Body->bLockXRotation = true;
		Body->bLockYRotation = true;
		Body->SetDOFLock(EDOFMode::SixDOF);
	}
	const FVector Vel = Mesh->GetPhysicsLinearVelocity();
	const FRotator Rot = GetActorRotation();
	if (!bPecking && (FMath::Abs(Rot.Pitch) > 10.f || FMath::Abs(Rot.Roll) > 10.f))
	{
		SetActorRotation(FRotator(0.f, Rot.Yaw, 0.f), ETeleportType::TeleportPhysics);
		Mesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
	// In the air (thrown / hopping): let physics do its thing.
	if (FMath::Abs(Vel.Z) > 120.f)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	// Somebody close? RUN (and flap).
	bool bScared = false;
	for (TActorIterator<APVVCharacter> It(GetWorld()); It; ++It)
	{
		const FVector Away = (GetActorLocation() - It->GetActorLocation()) * FVector(1.f, 1.f, 0.f);
		if (Away.Size() < 320.f)
		{
			Heading = Away.GetSafeNormal();
			bScared = true;
			break;
		}
	}
	NextHop -= DeltaSeconds;
	if (!bScared && NextHop <= 0.f)
	{
		// Pick something new to do: wander somewhere, or stop and peck.
		NextHop = FMath::FRandRange(1.f, 3.f);
		bPecking = FMath::FRand() < 0.35f;
		FVector Dir = FMath::VRand() * FVector(1.f, 1.f, 0.f);
		const FVector ToHome = (Home - GetActorLocation()) * FVector(1.f, 1.f, 0.f);
		if (ToHome.Size() > WanderRadius)
		{
			Dir = ToHome; // head back to the yard
		}
		Heading = Dir.GetSafeNormal();
		if (FMath::FRand() < 0.3f)
		{
			Multicast_Cluck();
		}
	}
	const float Speed = bScared ? 380.f : (bPecking ? 0.f : 110.f);
	if (!Heading.IsNearlyZero())
	{
		const float Yaw = FMath::FixedTurn(Rot.Yaw, Heading.Rotation().Yaw, 540.f * DeltaSeconds);
		SetActorRotation(FRotator(0.f, Yaw, 0.f), ETeleportType::TeleportPhysics);
	}
	FVector Walk = Heading * Speed;
	// Little steps: a tiny bob up and down as it walks (and a flappy hop when scared).
	Walk.Z = Speed > 0.f && FMath::Fmod(Now * (bScared ? 7.f : 4.f), 1.f) < 0.12f ? (bScared ? 220.f : 80.f) : Vel.Z;
	Mesh->SetPhysicsLinearVelocity(Walk);
	if (bPecking)
	{
		// Peck the ground.
		const float Peck = FMath::Max(0.f, FMath::Sin(Now * 9.f)) * 25.f;
		SetActorRotation(FRotator(-Peck, GetActorRotation().Yaw, 0.f), ETeleportType::TeleportPhysics);
	}
}

void ABRChicken::Multicast_Cluck_Implementation()
{
	if (CluckSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, CluckSound, GetActorLocation());
	}
}

// ===========================================================================================
// Watchtower + spotlight

ABRWatchtower::ABRWatchtower()
{
	PrimaryActorTick.bCanEverTick = true;
	const FLinearColor Wood(0.35f, 0.25f, 0.15f), Metal(0.2f, 0.2f, 0.22f);
	const float H = 650.f;
	for (int32 i = 0; i < 4; ++i)
	{
		const float X = (i % 2) ? 160.f : -160.f, Y = (i / 2) ? 160.f : -160.f;
		AddMesh(*FString::Printf(TEXT("Leg%d"), i), CylinderMesh, FVector(X, Y, H * 0.5f), FVector(30.f, 30.f, H), Wood);
	}
	AddMesh(TEXT("Platform"), CubeMesh, FVector(0.f, 0.f, H), FVector(420.f, 420.f, 20.f), Wood);
	AddMesh(TEXT("RailN"), CubeMesh, FVector(0.f, 205.f, H + 55.f), FVector(420.f, 10.f, 90.f), Wood);
	AddMesh(TEXT("RailS"), CubeMesh, FVector(0.f, -205.f, H + 55.f), FVector(420.f, 10.f, 90.f), Wood);
	AddMesh(TEXT("RailW"), CubeMesh, FVector(-205.f, 0.f, H + 55.f), FVector(10.f, 420.f, 90.f), Wood);
	AddMesh(TEXT("Roof"), ConeMesh, FVector(0.f, 0.f, H + 330.f), FVector(520.f, 520.f, 120.f), FLinearColor(0.5f, 0.2f, 0.12f));
	for (int32 i = 0; i < 4; ++i)
	{
		const float X = (i % 2) ? 195.f : -195.f, Y = (i / 2) ? 195.f : -195.f;
		AddMesh(*FString::Printf(TEXT("Post%d"), i), CylinderMesh, FVector(X, Y, H + 140.f), FVector(16.f, 16.f, 280.f), Wood, FRotator::ZeroRotator, false);
	}
	AddMesh(TEXT("Ladder"), CubeMesh, FVector(240.f, 0.f, H * 0.5f), FVector(10.f, 80.f, H), Metal, FRotator(-8.f, 0.f, 0.f), false);
	Lamp = AddMesh(TEXT("Lamp"), CylinderMesh, FVector(170.f, 0.f, H + 150.f), FVector(60.f, 60.f, 70.f), Metal, FRotator(-90.f, 0.f, 0.f), false);
	Light = CreateDefaultSubobject<USpotLightComponent>(TEXT("Spotlight"));
	Light->SetupAttachment(Root);
	Light->SetRelativeLocation(FVector(170.f, 0.f, H + 150.f));
	Light->SetIntensity(400000.f);
	Light->SetAttenuationRadius(7000.f);
	Light->SetInnerConeAngle(6.f);
	Light->SetOuterConeAngle(10.f);
	Light->SetLightColor(FLinearColor(1.f, 0.97f, 0.85f));
	Light->SetCastShadows(true);
}

ABRCharacter* ABRWatchtower::FindOperator() const
{
	const FVector Top = GetActorLocation() + FVector(0.f, 0.f, Height);
	for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
	{
		const FVector D = It->GetActorLocation() - Top;
		if (It->IsWarden() && !It->IsNPC() && FVector2D(D.X, D.Y).Size() < 260.f && D.Z > -20.f && D.Z < 250.f)
		{
			return *It;
		}
	}
	return nullptr;
}

FRotator ABRWatchtower::GetBeamRotation() const
{
	if (const ABRCharacter* Operator = FindOperator())
	{
		FRotator Aim = Operator->GetBaseAimRotation();
		Aim.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Aim.Pitch), -60.f, 5.f);
		return Aim;
	}
	const ABRGameState* GS = BRGame(this);
	const float T = GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	return FRotator(-14.f, GetActorRotation().Yaw + FMath::Sin(T * 0.35f) * SweepAngle, 0.f);
}

void ABRWatchtower::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const FRotator Beam = GetBeamRotation();
	if (Light)
	{
		Light->SetWorldRotation(Beam);
		Light->SetRelativeLocation(FVector(170.f, 0.f, Height + 150.f));
	}
	if (Lamp)
	{
		Lamp->SetWorldRotation(Beam + FRotator(-90.f, 0.f, 0.f));
	}
	if (!HasAuthority())
	{
		return;
	}
	NextScan -= DeltaSeconds;
	if (NextScan > 0.f)
	{
		return;
	}
	NextScan = 0.25f;
	const FVector From = Light ? Light->GetComponentLocation() : GetActorLocation() + FVector(0.f, 0.f, Height + 150.f);
	const FVector Dir = Beam.Vector();
	const float CosLimit = FMath::Cos(FMath::DegreesToRadians(BeamAngle));
	for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
	{
		ABRCharacter* C = *It;
		if (!C->IsRunner() || C->IsNPC() || C->IsDisguised() || C->IsHiddenFromPatrol() || C->GetCondition() == EPVVCondition::Restrained)
		{
			continue;
		}
		const FVector To = C->GetActorLocation() - From;
		const float Dist = To.Size();
		if (Dist > Range || FVector::DotProduct(To / FMath::Max(Dist, 1.f), Dir) < CosLimit)
		{
			continue;
		}
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BRSpotlight), false, this);
		Params.AddIgnoredActor(C);
		if (GetWorld()->LineTraceSingleByChannel(Hit, From, C->GetActorLocation(), ECC_Visibility, Params))
		{
			continue; // behind cover
		}
		const bool bWasSpotted = C->IsSpotted();
		C->Spot(SpotSeconds);
		if (!bWasSpotted)
		{
			ABRGameState::Sfx(this, EBRSfx::Spotted, C->GetActorLocation());
			C->Notify(LOCTEXT("InSpotlight", "SPOTLIGHT! Border Patrol can see you - get to cover!"));
		}
	}
}

bool ABRWatchtower::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	return IsFreeWarden(Interactor);
}

FText ABRWatchtower::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	const bool bUp = Interactor && Interactor->GetActorLocation().Z > GetActorLocation().Z + Height * 0.5f;
	return bUp ? LOCTEXT("TowerDown", "Climb down") : LOCTEXT("TowerUp", "Climb the watchtower (aim the spotlight with your view)");
}

void ABRWatchtower::OnInteract_Implementation(APVVCharacter* Interactor)
{
	if (!HasAuthority() || !Interactor)
	{
		return;
	}
	const bool bUp = Interactor->GetActorLocation().Z > GetActorLocation().Z + Height * 0.5f;
	const FVector To = bUp ? GetActorTransform().TransformPosition(FVector(380.f, 0.f, 100.f)) : GetActorLocation() + FVector(0.f, 0.f, Height + 110.f);
	Interactor->TeleportTo(To, Interactor->GetActorRotation(), false, true);
}

// ===========================================================================================
// Item pickup (dropped items)

ABRItemPickup::ABRItemPickup()
{
	AddMesh(TEXT("Box"), CubeMesh, FVector(0.f, 0.f, 18.f), FVector(36.f, 36.f, 36.f), FLinearColor(0.85f, 0.7f, 0.3f), FRotator(0.f, 25.f, 0.f), false);
	Tag = AddLabel(TEXT("Tag"), FVector(0.f, 0.f, 70.f), 22.f, FColor::White);
	AddPickupZone(60.f);
	Tags.Add(TEXT("BRPickup"));
}

void ABRItemPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRItemPickup, Item);
	DOREPLIFETIME(ABRItemPickup, Charges);
	DOREPLIFETIME(ABRItemPickup, ReservedFor);
}

void ABRItemPickup::BeginPlay()
{
	ComponentColors.Add(TEXT("Box"), BR::IsIllegal(Item) ? FLinearColor(0.7f, 0.1f, 0.1f) : FLinearColor(0.85f, 0.7f, 0.3f));
	Super::BeginPlay();
	StartDespawn(DespawnSeconds);
	if (Tag)
	{
		Tag->SetText(FText::FromString(BR::ItemName(Item)));
		Tag->SetTextRenderColor(BR::IsIllegal(Item) ? FColor(255, 80, 80) : FColor::White);
	}
}

bool ABRItemPickup::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	if (ReservedFor)
	{
		const ABRPlayerState* Claimer = BRState(Interactor);
		return Claimer && Claimer == ReservedFor && Interactor->GetCondition() == EPVVCondition::Normal && Claimer->HasFreeSlot();
	}
	if (IsFreeWarden(Interactor))
	{
		return BR::IsIllegal(Item); // confiscate
	}
	const ABRPlayerState* PS = BRState(Interactor);
	return IsFreeRunner(Interactor) && PS && PS->HasFreeSlot();
}

FText ABRItemPickup::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	if (ReservedFor)
	{
		return FText::Format(LOCTEXT("GetBack", "Get your {0} back!"), FText::FromString(BR::ItemName(Item)));
	}
	if (Interactor && Interactor->GetFaction() == BR::Wardens)
	{
		return FText::Format(LOCTEXT("Confiscate", "Confiscate the {0}"), FText::FromString(BR::ItemName(Item)));
	}
	return FText::Format(LOCTEXT("PickUp", "Pick up {0}"), FText::FromString(BR::ItemName(Item)));
}

void ABRItemPickup::OnInteract_Implementation(APVVCharacter* Interactor)
{
	if (!HasAuthority() || !Interactor || IsActorBeingDestroyed())
	{
		return;
	}
	if (Interactor->GetFaction() == BR::Wardens && !ReservedFor)
	{
		if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
		{
			GM->OnContrabandSeized(Interactor, 1);
		}
		Destroy();
		return;
	}
	ABRPlayerState* PS = BRState(Interactor);
	if (PS && PS->AddItem(Item))
	{
		if (Item == EBRItem::WireCutters)
		{
			PS->WireCutterCharges += FMath::Max(1, Charges);
		}
		Destroy();
	}
}

// ===========================================================================================
// Hat pickup (knocked-off hats)

ABRHatPickup::ABRHatPickup()
{
	bReplicates = true;
	Visual = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Root);
	Visual->SetRelativeLocation(FVector(0.f, 0.f, -165.f)); // hat meshes sit at head height; drop them to the ground
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AddMesh(TEXT("Marker"), SphereMesh, FVector(0.f, 0.f, 12.f), FVector(22.f, 22.f, 22.f), FLinearColor(0.9f, 0.75f, 0.2f), FRotator::ZeroRotator, false);
	Tag = AddLabel(TEXT("Tag"), FVector(0.f, 0.f, 60.f), 18.f, FColor(255, 230, 120));
	AddPickupZone(60.f);
	Tags.Add(TEXT("BRPickup"));
}

void ABRHatPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRHatPickup, Hat);
	DOREPLIFETIME(ABRHatPickup, HatMesh);
}

void ABRHatPickup::BeginPlay()
{
	Super::BeginPlay();
	StartDespawn(DespawnSeconds);
	OnRep_HatInfo();
}

void ABRHatPickup::OnRep_HatInfo()
{
	if (Visual)
	{
		Visual->SetSkeletalMeshAsset(HatMesh);
	}
	if (UStaticMeshComponent* Marker = FindComponentByClass<UStaticMeshComponent>())
	{
		Marker->SetHiddenInGame(HatMesh != nullptr); // little ball only when there's no hat mesh to show
	}
	if (Tag)
	{
		Tag->SetText(FText::FromString(FString::Printf(TEXT("%s: %s"), *BR::HatName(Hat), *BR::HatSkill(Hat))));
	}
}

bool ABRHatPickup::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	const ABRCharacter* BRC = Cast<ABRCharacter>(Interactor);
	return BRC && !BRC->IsNPC() && Interactor->GetCondition() == EPVVCondition::Normal && Hat != EBRHat::None;
}

FText ABRHatPickup::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	return FText::Format(LOCTEXT("WearHat", "Wear the {0} ({1})"), FText::FromString(BR::HatName(Hat)), FText::FromString(BR::HatSkill(Hat)));
}

void ABRHatPickup::OnInteract_Implementation(APVVCharacter* Interactor)
{
	ABRCharacter* BRC = Cast<ABRCharacter>(Interactor);
	if (!HasAuthority() || !BRC || IsActorBeingDestroyed())
	{
		return;
	}
	const EBRHat NewHat = Hat;
	Hat = EBRHat::None; // so a swap can't loop
	BRC->KnockOffHat(); // drop the one you're wearing
	BRC->SetHat(NewHat);
	BRC->Notify(FText::Format(LOCTEXT("NowWearing", "{0}: {1}"), FText::FromString(BR::HatName(NewHat)), FText::FromString(BR::HatSkill(NewHat))));
	Destroy();
}

// ===========================================================================================
// Dropped cash

ABRCashPickup::ABRCashPickup()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	AddMesh(TEXT("Stack"), CubeMesh, FVector(0.f, 0.f, 6.f), FVector(32.f, 16.f, 10.f), FLinearColor(0.25f, 0.6f, 0.25f), FRotator(0.f, 15.f, 0.f), false);
	AddMesh(TEXT("Band"), CubeMesh, FVector(0.f, 0.f, 6.f), FVector(8.f, 17.f, 11.f), FLinearColor(0.9f, 0.85f, 0.7f), FRotator(0.f, 15.f, 0.f), false);
	Tag = AddLabel(TEXT("Tag"), FVector(0.f, 0.f, 45.f), 24.f, FColor(90, 255, 90));
	AddPickupZone(60.f);
	Tags.Add(TEXT("BRPickup"));
}

void ABRCashPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRCashPickup, Amount);
	DOREPLIFETIME(ABRCashPickup, DropperName);
	DOREPLIFETIME(ABRCashPickup, DropperTeam);
	DOREPLIFETIME(ABRCashPickup, ReservedFor);
}

void ABRCashPickup::BeginPlay()
{
	Super::BeginPlay();
	StartDespawn(DespawnSeconds);
	if (Tag)
	{
		Tag->SetText(FText::FromString(FString::Printf(TEXT("$%d"), Amount)));
	}
}

void ABRCashPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Tag && GetNetMode() != NM_DedicatedServer)
	{
		Tag->SetText(FText::FromString(FString::Printf(TEXT("$%d"), Amount)));
		Tag->SetRelativeRotation(FRotator(0.f, GetWorld()->GetTimeSeconds() * 90.f, 0.f));
	}
}

bool ABRCashPickup::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	const ABRCharacter* BRC = Cast<ABRCharacter>(Interactor);
	if (ReservedFor && BRState(Interactor) != ReservedFor)
	{
		return false; // somebody else's winnings
	}
	return BRC && !BRC->IsNPC() && Interactor->GetCondition() == EPVVCondition::Normal && Amount > 0;
}

FText ABRCashPickup::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	const ABRPlayerState* PS = BRState(Interactor);
	if (ReservedFor)
	{
		return FText::Format(LOCTEXT("Winnings", "Grab your ${0}!"), Amount);
	}
	if (DropperName.IsEmpty())
	{
		return FText::Format(LOCTEXT("FoundCash", "Pick up ${0}"), Amount);
	}
	if (PS && PS->GetPlayerName() == DropperName)
	{
		return FText::Format(LOCTEXT("TakeBack", "Pick your ${0} back up"), Amount);
	}
	if (Interactor && Interactor->GetFaction() == BR::Wardens)
	{
		return FText::Format(LOCTEXT("TakeCashW", "Accept ${0} from {1} (goes to the budget)"), Amount, FText::FromString(DropperName));
	}
	return FText::Format(LOCTEXT("TakeCash", "Take ${0} from {1}"), Amount, FText::FromString(DropperName));
}

void ABRCashPickup::OnInteract_Implementation(APVVCharacter* Interactor)
{
	ABRPlayerState* PS = BRState(Interactor);
	ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>();
	if (!HasAuthority() || !PS || !GM || IsActorBeingDestroyed() || Amount <= 0)
	{
		return;
	}
	const int32 Taken = Amount;
	Amount = 0;
	if (Interactor->GetFaction() == BR::Wardens)
	{
		GM->AddBudget(Taken);
	}
	else
	{
		PS->AddMoney(Taken);
	}
	if (PS->GetPlayerName() != DropperName && DropperTeam != Interactor->GetFaction())
	{
		GM->Feed(FString::Printf(TEXT("%s accepted $%d from %s. Totally normal."), *PS->GetPlayerName(), Taken, *DropperName));
	}
	Tell(Interactor, FText::Format(LOCTEXT("GotCash", "+${0}"), Taken));
	ABRGameState::Sfx(this, EBRSfx::CashPickup, GetActorLocation());
	ABRGameState::FX(this, EBRFX::MoneyText, GetActorLocation() + FVector(0.f, 0.f, 40.f), FString::Printf(TEXT("+$%d"), Taken));
	Destroy();
}

// ===========================================================================================
// Taco platter

ABRTacoPlatter::ABRTacoPlatter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	AddMesh(TEXT("Table"), CylinderMesh, FVector(0.f, 0.f, 35.f), FVector(110.f, 110.f, 70.f), FLinearColor(0.55f, 0.35f, 0.18f));
	AddMesh(TEXT("Plate"), CylinderMesh, FVector(0.f, 0.f, 73.f), FVector(95.f, 95.f, 4.f), FLinearColor(0.95f, 0.95f, 0.9f), FRotator::ZeroRotator, false);
	for (int32 i = 0; i < 8; ++i)
	{
		const float A = i * (2.f * UE_PI / 8.f);
		const FString Name = FString::Printf(TEXT("Taco%d"), i);
		AddMesh(*Name, SphereMesh, FVector(FMath::Cos(A) * 30.f, FMath::Sin(A) * 30.f, 82.f), FVector(26.f, 12.f, 16.f), FLinearColor(0.95f, 0.75f, 0.3f),
			FRotator(0.f, FMath::RadiansToDegrees(A), 0.f), false);
	}
	AddLabel(TEXT("Sign"), FVector(0.f, 0.f, 140.f), 30.f, FColor(255, 200, 80))->SetText(FText::FromString(TEXT("FREE TACOS")));
}

void ABRTacoPlatter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRTacoPlatter, Servings);
}

bool ABRTacoPlatter::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	return Servings > 0 && Interactor && Interactor->GetCondition() == EPVVCondition::Normal && !Interactor->GetCarrier();
}

FText ABRTacoPlatter::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	return FText::Format(LOCTEXT("GrabTaco", "Grab a taco ({0} left)"), Servings);
}

void ABRTacoPlatter::OnInteract_Implementation(APVVCharacter* Interactor)
{
	ABRCharacter* Eater = Cast<ABRCharacter>(Interactor);
	if (!HasAuthority() || !Eater || Servings <= 0)
	{
		return;
	}
	ABRPlayerState* PS = BRState(Interactor);
	if (Eater->GetHunger() < 85.f)
	{
		Eater->Eat(45.f);
		if (PS)
		{
			++PS->StatTacos;
		}
		Tell(Interactor, LOCTEXT("PlatterAte", "Free taco. Delicious."));
	}
	else if (!PS || !PS->AddItem(EBRItem::Taco))
	{
		Tell(Interactor, LOCTEXT("PlatterFull", "You're full and your pockets are too."));
		return;
	}
	if (--Servings <= 0)
	{
		Destroy();
	}
}

void ABRTacoPlatter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (ShownServings == Servings)
	{
		return;
	}
	ShownServings = Servings;
	TArray<UStaticMeshComponent*> Meshes;
	GetComponents<UStaticMeshComponent>(Meshes);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		const FString Name = Mesh->GetName();
		if (Name.StartsWith(TEXT("Taco")))
		{
			Mesh->SetVisibility(FCString::Atoi(*Name.RightChop(4)) < Servings);
		}
	}
}

// ===========================================================================================
// Pinata

ABRPinata::ABRPinata()
{
	PrimaryActorTick.bCanEverTick = true;
	const FLinearColor Wood(0.45f, 0.28f, 0.12f);
	AddMesh(TEXT("PostL"), CubeMesh, FVector(0.f, -220.f, 200.f), FVector(20.f, 20.f, 400.f), Wood);
	AddMesh(TEXT("PostR"), CubeMesh, FVector(0.f, 220.f, 200.f), FVector(20.f, 20.f, 400.f), Wood);
	AddMesh(TEXT("Bar"), CubeMesh, FVector(0.f, 0.f, 400.f), FVector(20.f, 460.f, 20.f), Wood);

	Swing = CreateDefaultSubobject<USceneComponent>(TEXT("Swing"));
	Swing->SetupAttachment(Root);
	Swing->SetRelativeLocation(FVector(0.f, 0.f, 390.f));
	auto Piece = [&](const TCHAR* Name, UStaticMesh* Mesh, const FVector& At, const FVector& Size, const FLinearColor& Color)
	{
		UStaticMeshComponent* C = AddMesh(Name, Mesh, At, Size, Color, FRotator::ZeroRotator, false);
		C->SetupAttachment(Swing);
	};
	// A paper donkey in loud colours, hanging from a rope.
	Piece(TEXT("Rope"), CubeMesh, FVector(0.f, 0.f, -60.f), FVector(3.f, 3.f, 120.f), FLinearColor(0.85f, 0.8f, 0.6f));
	Piece(TEXT("Body"), CubeMesh, FVector(0.f, 0.f, -150.f), FVector(110.f, 60.f, 60.f), FLinearColor(1.f, 0.2f, 0.6f));
	Piece(TEXT("Stripe1"), CubeMesh, FVector(-30.f, 0.f, -150.f), FVector(15.f, 64.f, 64.f), FLinearColor(1.f, 0.85f, 0.1f));
	Piece(TEXT("Stripe2"), CubeMesh, FVector(30.f, 0.f, -150.f), FVector(15.f, 64.f, 64.f), FLinearColor(0.1f, 0.75f, 0.95f));
	Piece(TEXT("Neck"), CubeMesh, FVector(60.f, 0.f, -110.f), FVector(30.f, 35.f, 60.f), FLinearColor(0.2f, 0.85f, 0.3f));
	Piece(TEXT("Head"), CubeMesh, FVector(80.f, 0.f, -80.f), FVector(55.f, 35.f, 35.f), FLinearColor(1.f, 0.5f, 0.1f));
	Piece(TEXT("EarL"), ConeMesh, FVector(70.f, -12.f, -50.f), FVector(10.f, 10.f, 35.f), FLinearColor(1.f, 0.2f, 0.6f));
	Piece(TEXT("EarR"), ConeMesh, FVector(70.f, 12.f, -50.f), FVector(10.f, 10.f, 35.f), FLinearColor(1.f, 0.2f, 0.6f));
	for (int32 i = 0; i < 4; ++i)
	{
		const FString Name = FString::Printf(TEXT("Leg%d"), i);
		Piece(*Name, CubeMesh, FVector(i < 2 ? 35.f : -35.f, i % 2 ? 18.f : -18.f, -205.f), FVector(15.f, 15.f, 50.f), FLinearColor(0.6f, 0.2f, 0.9f));
	}
	Piece(TEXT("Tail"), CubeMesh, FVector(-65.f, 0.f, -140.f), FVector(30.f, 8.f, 8.f), FLinearColor(1.f, 0.85f, 0.1f));
}

void ABRPinata::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRPinata, Whacks);
	DOREPLIFETIME(ABRPinata, BackAt);
}

bool ABRPinata::IsBroken() const
{
	const ABRGameState* GS = BRGame(this);
	return GS && GS->GetServerWorldTimeSeconds() < BackAt;
}

bool ABRPinata::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	return IsFreeRunner(Interactor) && !IsBroken();
}

FText ABRPinata::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	return FText::Format(LOCTEXT("Whack", "WHACK the pinata! ({0}/{1})"), Whacks, WhacksToBreak);
}

void ABRPinata::OnInteract_Implementation(APVVCharacter* Interactor)
{
	ABRPlayerState* PS = BRState(Interactor);
	ABRGameState* GS = BRGame(this);
	if (!HasAuthority() || !PS || !GS || IsBroken())
	{
		return;
	}
	++Whacks;
	if (Whacks < WhacksToBreak)
	{
		Multicast_Whack(false);
		return;
	}

	// BURST! Whoever lands the last hit gets the prize.
	Whacks = 0;
	BackAt = GS->GetServerWorldTimeSeconds() + RespawnSeconds;
	FString Prize;
	const EBRItem Items[] = { EBRItem::Taco, EBRItem::Taco, EBRItem::GasCan, EBRItem::WireCutters, EBRItem::Ladder,
		EBRItem::Shovel, EBRItem::Trumpet, EBRItem::CannonSuit };
	const EBRItem Item = Items[FMath::RandRange(0, static_cast<int32>(UE_ARRAY_COUNT(Items)) - 1)];
	if (FMath::FRand() < 0.4f && PS->HasFreeSlot() && (Item != EBRItem::CannonSuit || FMath::FRand() < 0.3f))
	{
		PS->AddItem(Item);
		if (Item == EBRItem::WireCutters)
		{
			PS->WireCutterCharges += 2;
		}
		Prize = BR::ItemName(Item);
	}
	else
	{
		const int32 Cash = FMath::RandRange(MinCash, MaxCash);
		PS->AddMoney(Cash);
		Prize = FString::Printf(TEXT("$%d"), Cash);
	}
	GS->Multicast_Announce(FText::Format(LOCTEXT("PinataBurst", "{0} smashed the pinata and got {1}!"),
		FText::FromString(Interactor->GetPlayerDisplayName()), FText::FromString(Prize)));
	Multicast_Whack(true);
}

void ABRPinata::Multicast_Whack_Implementation(bool bBurst)
{
	WhackTime = GetWorld()->GetTimeSeconds();
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (USoundBase* Sound = bBurst ? BurstSound.Get() : WhackSound.Get())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Swing->GetComponentLocation());
	}
}

void ABRPinata::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const bool bBroken = IsBroken();
	if (Swing->IsVisible() == bBroken)
	{
		Swing->SetVisibility(!bBroken, true);
	}
	// Gentle sway, big swing right after a whack.
	const float T = GetWorld()->GetTimeSeconds();
	const float Hit = FMath::Clamp(1.f - (T - WhackTime) / 1.5f, 0.f, 1.f);
	Swing->SetRelativeRotation(FRotator(0.f, T * 15.f, FMath::Sin(T * 1.3f) * 5.f + FMath::Sin(T * 9.f) * 35.f * Hit));
}

// ===========================================================================================
// Tunnel

ABRTunnel::ABRTunnel()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	const FLinearColor Dirt(0.45f, 0.32f, 0.2f);
	const FLinearColor Dark(0.03f, 0.02f, 0.02f);
	const FLinearColor Wood(0.4f, 0.26f, 0.12f);
	AddMesh(TEXT("Mound"), SphereMesh, FVector(0.f, 0.f, 0.f), FVector(260.f, 260.f, 90.f), Dirt);
	AddMesh(TEXT("Hole"), CylinderMesh, FVector(0.f, 0.f, 3.f), FVector(170.f, 170.f, 6.f), Dark, FRotator::ZeroRotator, false);
	AddMesh(TEXT("FrameL"), CubeMesh, FVector(0.f, -95.f, 60.f), FVector(15.f, 15.f, 120.f), Wood, FRotator::ZeroRotator, false);
	AddMesh(TEXT("FrameR"), CubeMesh, FVector(0.f, 95.f, 60.f), FVector(15.f, 15.f, 120.f), Wood, FRotator::ZeroRotator, false);
	AddMesh(TEXT("FrameTop"), CubeMesh, FVector(0.f, 0.f, 120.f), FVector(15.f, 205.f, 15.f), Wood, FRotator::ZeroRotator, false);

	ExitPoint = CreateDefaultSubobject<USceneComponent>(TEXT("ExitPoint"));
	ExitPoint->SetupAttachment(Root);
	ExitPoint->SetRelativeLocation(ExitOffset);
	UStaticMeshComponent* ExitHole = AddMesh(TEXT("ExitHole"), CylinderMesh, FVector(0.f, 0.f, 3.f), FVector(150.f, 150.f, 6.f), Dark, FRotator::ZeroRotator, false);
	ExitHole->SetupAttachment(ExitPoint);
	UStaticMeshComponent* ExitDirt = AddMesh(TEXT("ExitDirt"), SphereMesh, FVector(0.f, 120.f, 0.f), FVector(140.f, 100.f, 50.f), Dirt, FRotator::ZeroRotator, false);
	ExitDirt->SetupAttachment(ExitPoint);

	Sign = AddLabel(TEXT("Sign"), FVector(0.f, 0.f, 200.f), 40.f, FColor(230, 200, 150));
}

void ABRTunnel::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRTunnel, bOpen);
}

void ABRTunnel::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ExitPoint->SetRelativeLocation(ExitOffset);
}

bool ABRTunnel::IsNearExit(const APVVCharacter* C) const
{
	return C && FVector::Dist2D(C->GetActorLocation(), ExitPoint->GetComponentLocation()) < 450.f;
}

bool ABRTunnel::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	if (IsFreeWarden(Interactor))
	{
		return bOpen; // cave it in from either end
	}
	if (!IsFreeRunner(Interactor) || IsNearExit(Interactor))
	{
		return false;
	}
	if (bOpen)
	{
		const ABRGameState* GS = BRGame(this);
		return GS && GS->Phase == EBRPhase::Invasion;
	}
	const ABRPlayerState* PS = BRState(Interactor);
	return PS && PS->HasItem(EBRItem::Shovel);
}

float ABRTunnel::GetInteractDuration_Implementation(APVVCharacter* Interactor) const
{
	if (Interactor && Interactor->GetFaction() == BR::Wardens)
	{
		return CaveInSeconds;
	}
	return bOpen ? CrawlSeconds : DigSeconds;
}

FText ABRTunnel::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	if (Interactor && Interactor->GetFaction() == BR::Wardens)
	{
		return LOCTEXT("CaveIn", "Cave in the tunnel (hold)");
	}
	return bOpen ? LOCTEXT("Crawl", "Crawl through the tunnel (hold)") : LOCTEXT("Dig", "Dig a tunnel with your shovel (hold - takes a while)");
}

void ABRTunnel::OnInteract_Implementation(APVVCharacter* Interactor)
{
	if (!HasAuthority() || !Interactor)
	{
		return;
	}
	ABRGameState* GS = BRGame(this);
	if (Interactor->GetFaction() == BR::Wardens)
	{
		bOpen = false;
		if (GS)
		{
			GS->Multicast_Announce(FText::Format(LOCTEXT("CavedIn", "{0} found a tunnel and caved it in!"), FText::FromString(Interactor->GetPlayerDisplayName())));
		}
		return;
	}
	if (!bOpen)
	{
		ABRPlayerState* PS = BRState(Interactor);
		if (!PS || !PS->HasItem(EBRItem::Shovel))
		{
			return;
		}
		if (bShovelBreaks)
		{
			PS->RemoveItem(EBRItem::Shovel);
		}
		bOpen = true;
		Tell(Interactor, LOCTEXT("Dug", "Tunnel dug! When the Line is open, hold E here to crawl through."));
		return;
	}
	const FVector Out = ExitPoint->GetComponentLocation() + FVector(0.f, 0.f, 120.f);
	Interactor->TeleportTo(Out, FRotator(0.f, GetActorRotation().Yaw, 0.f));
	Tell(Interactor, LOCTEXT("Crawled", "You crawled out the other side. Keep moving!"));
}

void ABRTunnel::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const int32 State = bOpen ? 1 : 0;
	if (State == AppliedState)
	{
		return;
	}
	AppliedState = State;
	TArray<UStaticMeshComponent*> Meshes;
	GetComponents<UStaticMeshComponent>(Meshes);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		const FString Name = Mesh->GetName();
		if (Name == TEXT("Hole") || Name.StartsWith(TEXT("Frame")) || Name == TEXT("ExitHole") || Name == TEXT("ExitDirt"))
		{
			Mesh->SetVisibility(bOpen);
		}
		else if (Name == TEXT("Mound"))
		{
			Mesh->SetRelativeScale3D(bOpen ? FVector(2.6f, 2.6f, 0.4f) : FVector(2.6f, 2.6f, 0.9f));
		}
	}
	if (Sign)
	{
		Sign->SetText(bOpen ? LOCTEXT("SignOpen", "TUNNEL") : LOCTEXT("SignDirt", "soft dirt...\n(dig with a shovel)"));
	}
}

// ===========================================================================================
// Village (decoration for the Runner town). Positions are relative to the Desert Arena origin.

namespace
{
	constexpr float VillageFloorZ = 100.f; // desert floor height (ABRDesertArena::Ground)
	const FLinearColor VillageWood(0.42f, 0.27f, 0.13f);
	const FLinearColor VillageDarkWood(0.25f, 0.15f, 0.08f);
	const FLinearColor VillageGlass(0.25f, 0.45f, 0.6f);
	const FLinearColor VillageLeaf(0.15f, 0.45f, 0.12f);
	const FLinearColor VillageCactus(0.2f, 0.5f, 0.2f);
	const FLinearColor VillageFestive[] = {
		{0.95f, 0.1f, 0.4f}, {0.1f, 0.7f, 0.9f}, {1.f, 0.8f, 0.05f}, {0.2f, 0.85f, 0.25f}, {1.f, 0.4f, 0.05f}, {0.6f, 0.2f, 0.9f}
	};
}

ABRVillage::ABRVillage()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;

	// --- Colourful adobe houses (solid; doors + windows face the road).
	House(TEXT("House1"), -7600.f, -3300.f, 700.f, 600.f, 330.f, FLinearColor(0.95f, 0.55f, 0.6f), FLinearColor(0.95f, 0.95f, 0.9f));
	House(TEXT("House2"), -8600.f, -4600.f, 600.f, 700.f, 300.f, FLinearColor(0.25f, 0.7f, 0.65f), FLinearColor(0.95f, 0.9f, 0.6f));
	House(TEXT("House3"), -7400.f, 1600.f, 700.f, 600.f, 340.f, FLinearColor(0.98f, 0.8f, 0.3f), FLinearColor(0.2f, 0.4f, 0.8f));
	House(TEXT("House4"), -12900.f, 5200.f, 800.f, 700.f, 320.f, FLinearColor(0.35f, 0.5f, 0.9f), FLinearColor(0.95f, 0.95f, 0.95f));
	House(TEXT("House5"), -11200.f, 5600.f, 700.f, 700.f, 360.f, FLinearColor(1.f, 0.55f, 0.2f), FLinearColor(0.4f, 0.2f, 0.1f));
	House(TEXT("House6"), -15200.f, 3400.f, 600.f, 800.f, 300.f, FLinearColor(0.45f, 0.75f, 0.35f), FLinearColor(0.95f, 0.9f, 0.8f));
	House(TEXT("House7"), -13200.f, -4800.f, 700.f, 600.f, 310.f, FLinearColor(0.7f, 0.55f, 0.9f), FLinearColor(1.f, 0.9f, 0.5f));
	House(TEXT("House8"), -9800.f, 4300.f, 700.f, 700.f, 330.f, FLinearColor(0.8f, 0.4f, 0.25f), FLinearColor(0.95f, 0.85f, 0.6f));
	House(TEXT("House9"), -6800.f, -2000.f, 700.f, 600.f, 300.f, FLinearColor(0.7f, 0.9f, 0.3f), FLinearColor(0.3f, 0.2f, 0.15f));

	// --- Church with a bell tower.
	const FLinearColor Chalk(0.95f, 0.92f, 0.85f);
	AddMesh(TEXT("Church_Body"), CubeMesh, FVector(-14200.f, -3200.f, VillageFloorZ + 300.f), FVector(900.f, 700.f, 600.f), Chalk);
	AddMesh(TEXT("Church_Roof"), CubeMesh, FVector(-14200.f, -3200.f, VillageFloorZ + 615.f), FVector(940.f, 740.f, 30.f), FLinearColor(0.7f, 0.35f, 0.2f), FRotator::ZeroRotator, false);
	AddMesh(TEXT("Church_Tower"), CubeMesh, FVector(-13850.f, -3200.f, VillageFloorZ + 550.f), FVector(260.f, 260.f, 1100.f), Chalk);
	AddMesh(TEXT("Church_Dome"), SphereMesh, FVector(-13850.f, -3200.f, VillageFloorZ + 1120.f), FVector(240.f, 240.f, 200.f), FLinearColor(0.85f, 0.6f, 0.2f), FRotator::ZeroRotator, false);
	AddMesh(TEXT("Church_CrossV"), CubeMesh, FVector(-13850.f, -3200.f, VillageFloorZ + 1300.f), FVector(15.f, 15.f, 160.f), FLinearColor(0.2f, 0.2f, 0.2f), FRotator::ZeroRotator, false);
	AddMesh(TEXT("Church_CrossH"), CubeMesh, FVector(-13850.f, -3200.f, VillageFloorZ + 1330.f), FVector(15.f, 90.f, 15.f), FLinearColor(0.2f, 0.2f, 0.2f), FRotator::ZeroRotator, false);
	AddMesh(TEXT("Church_Bell"), ConeMesh, FVector(-13715.f, -3200.f, VillageFloorZ + 900.f), FVector(70.f, 70.f, 80.f), FLinearColor(0.75f, 0.6f, 0.15f), FRotator::ZeroRotator, false);
	AddMesh(TEXT("Church_Door"), CubeMesh, FVector(-13748.f, -3200.f, VillageFloorZ + 140.f), FVector(10.f, 160.f, 280.f), VillageDarkWood, FRotator::ZeroRotator, false);
	AddMesh(TEXT("Church_Window"), SphereMesh, FVector(-13745.f, -3200.f, VillageFloorZ + 450.f), FVector(10.f, 120.f, 120.f), FLinearColor(0.9f, 0.3f, 0.5f), FRotator::ZeroRotator, false);

	// --- Plaza: stone floor, fountain, benches, palms, flags strung across.
	AddMesh(TEXT("Plaza_Floor"), CylinderMesh, FVector(-10000.f, 700.f, VillageFloorZ + 2.f), FVector(1500.f, 1500.f, 4.f), FLinearColor(0.75f, 0.68f, 0.58f), FRotator::ZeroRotator, false);
	AddMesh(TEXT("Fountain_Base"), CylinderMesh, FVector(-10000.f, 700.f, VillageFloorZ + 30.f), FVector(420.f, 420.f, 60.f), FLinearColor(0.6f, 0.58f, 0.55f));
	AddMesh(TEXT("Fountain_Water"), CylinderMesh, FVector(-10000.f, 700.f, VillageFloorZ + 58.f), FVector(380.f, 380.f, 6.f), FLinearColor(0.15f, 0.5f, 0.75f), FRotator::ZeroRotator, false);
	AddMesh(TEXT("Fountain_Pillar"), CylinderMesh, FVector(-10000.f, 700.f, VillageFloorZ + 140.f), FVector(50.f, 50.f, 200.f), FLinearColor(0.6f, 0.58f, 0.55f));
	AddMesh(TEXT("Fountain_Bowl"), CylinderMesh, FVector(-10000.f, 700.f, VillageFloorZ + 240.f), FVector(160.f, 160.f, 20.f), FLinearColor(0.6f, 0.58f, 0.55f), FRotator::ZeroRotator, false);
	Bench(-10000.f, 230.f, 0.f);
	Bench(-10000.f, 1170.f, 0.f);
	Bench(-10470.f, 700.f, 90.f);
	Bench(-9530.f, 700.f, 90.f);
	Palm(-10850.f, -150.f);
	Palm(-9150.f, 1550.f);
	Palm(-9150.f, -150.f);
	Palm(-10850.f, 1550.f);
	Picado(FVector2D(-10750.f, -50.f), FVector2D(-9250.f, 1450.f), VillageFloorZ + 430.f, 16);
	Picado(FVector2D(-10750.f, 1450.f), FVector2D(-9250.f, -50.f), VillageFloorZ + 430.f, 16);
	Lamp(-10750.f, -50.f);
	Lamp(-9250.f, 1450.f);
	Lamp(-10750.f, 1450.f);
	Lamp(-9250.f, -50.f);

	// --- Market stalls along the road.
	Stall(-10600.f, -620.f, VillageFestive[0]);
	Stall(-10000.f, -620.f, VillageFestive[1]);
	Stall(-9400.f, -620.f, VillageFestive[2]);
	Stall(-12800.f, -620.f, VillageFestive[4]);

	// --- Street lamps along the road.
	Lamp(-13600.f, -780.f);
	Lamp(-11400.f, -780.f);
	Lamp(-8600.f, -780.f);
	Lamp(-6600.f, -780.f);

	// --- Clutter: barrels, crates, a well, a wrecked car, laundry.
	Barrel(-10650.f, -2300.f, FLinearColor(0.35f, 0.2f, 0.1f));
	Barrel(-10560.f, -2180.f, FLinearColor(0.2f, 0.35f, 0.6f));
	Barrel(-11000.f, 2400.f, FLinearColor(0.35f, 0.2f, 0.1f));
	Barrel(-7120.f, -2880.f, FLinearColor(0.6f, 0.15f, 0.1f));
	Barrel(-12500.f, 4750.f, FLinearColor(0.35f, 0.2f, 0.1f));
	Crate(-10900.f, -650.f, 10.f);
	Crate(-10880.f, -560.f, -20.f);
	Crate(-7900.f, 1200.f, 30.f);
	Crate(-9300.f, 4760.f, 5.f);
	Crate(-14700.f, -2600.f, 40.f);

	AddMesh(TEXT("Well_Base"), CylinderMesh, FVector(-12300.f, 1100.f, VillageFloorZ + 50.f), FVector(180.f, 180.f, 100.f), FLinearColor(0.55f, 0.5f, 0.45f));
	AddMesh(TEXT("Well_PostL"), CubeMesh, FVector(-12300.f, 1020.f, VillageFloorZ + 170.f), FVector(12.f, 12.f, 160.f), VillageWood, FRotator::ZeroRotator, false);
	AddMesh(TEXT("Well_PostR"), CubeMesh, FVector(-12300.f, 1180.f, VillageFloorZ + 170.f), FVector(12.f, 12.f, 160.f), VillageWood, FRotator::ZeroRotator, false);
	AddMesh(TEXT("Well_Roof"), CubeMesh, FVector(-12300.f, 1100.f, VillageFloorZ + 255.f), FVector(150.f, 220.f, 12.f), FLinearColor(0.6f, 0.25f, 0.15f), FRotator(0.f, 0.f, 0.f), false);

	const FLinearColor Rust(0.45f, 0.22f, 0.1f);
	AddMesh(TEXT("Wreck_Body"), CubeMesh, FVector(-8400.f, -2500.f, VillageFloorZ + 60.f), FVector(420.f, 190.f, 80.f), Rust, FRotator(0.f, 15.f, 3.f));
	AddMesh(TEXT("Wreck_Cab"), CubeMesh, FVector(-8450.f, -2510.f, VillageFloorZ + 140.f), FVector(180.f, 170.f, 80.f), FLinearColor(0.2f, 0.5f, 0.55f), FRotator(0.f, 15.f, 3.f));
	AddMesh(TEXT("Wreck_Tire"), CylinderMesh, FVector(-8150.f, -2250.f, VillageFloorZ + 15.f), FVector(80.f, 80.f, 30.f), FLinearColor(0.05f, 0.05f, 0.05f), FRotator::ZeroRotator, false);

	AddMesh(TEXT("Laundry_PostA"), CylinderMesh, FVector(-12400.f, 4700.f, VillageFloorZ + 110.f), FVector(10.f, 10.f, 220.f), VillageWood, FRotator::ZeroRotator, false);
	AddMesh(TEXT("Laundry_PostB"), CylinderMesh, FVector(-11650.f, 4700.f, VillageFloorZ + 110.f), FVector(10.f, 10.f, 220.f), VillageWood, FRotator::ZeroRotator, false);
	AddMesh(TEXT("Laundry_Line"), CubeMesh, FVector(-12025.f, 4700.f, VillageFloorZ + 210.f), FVector(750.f, 2.f, 2.f), FLinearColor(0.9f, 0.9f, 0.9f), FRotator::ZeroRotator, false);
	for (int32 i = 0; i < 5; ++i)
	{
		const FString Name = FString::Printf(TEXT("Laundry_Cloth%d"), i);
		AddMesh(*Name, CubeMesh, FVector(-12300.f + i * 140.f, 4700.f, VillageFloorZ + 160.f), FVector(80.f, 3.f, 90.f - (i % 2) * 30.f), VillageFestive[(i + 2) % 6], FRotator::ZeroRotator, false);
	}

	// --- Desert plants.
	Cactus(-15500.f, -7000.f, 320.f);
	Cactus(-12500.f, -7300.f, 260.f);
	Cactus(-7800.f, 7300.f, 340.f);
	Cactus(-15600.f, 7200.f, 280.f);
	Cactus(-6200.f, -4500.f, 300.f);
	Cactus(-10500.f, 7300.f, 250.f);
	Cactus(-14000.f, 1300.f, 230.f);
	Cactus(-5800.f, 5200.f, 310.f);
	Palm(-12800.f, -2000.f);
	Palm(-7900.f, 4800.f);

	// --- Chicken yard with free-roaming chickens.
	ChickenYard = FVector(-14000.f, 4300.f, VillageFloorZ);
	AddMesh(TEXT("Coop_Box"), CubeMesh, FVector(-14350.f, 4150.f, VillageFloorZ + 70.f), FVector(200.f, 160.f, 140.f), VillageWood);
	AddMesh(TEXT("Coop_Roof"), CubeMesh, FVector(-14350.f, 4150.f, VillageFloorZ + 150.f), FVector(230.f, 190.f, 15.f), FLinearColor(0.6f, 0.25f, 0.15f), FRotator(0.f, 0.f, 8.f), false);
	// (The chickens themselves are real BR Chickens spawned at BeginPlay, so you can grab and throw them.)
}

FString ABRVillage::NextName(const TCHAR* Prefix)
{
	return FString::Printf(TEXT("%s%d"), Prefix, ++PieceCount);
}

void ABRVillage::House(const TCHAR* Name, float X, float Y, float W, float D, float H, const FLinearColor& Wall, const FLinearColor& Trim)
{
	auto N = [Name](const TCHAR* Part) { return FString::Printf(TEXT("%s_%s"), Name, Part); };
	AddMesh(*N(TEXT("Body")), CubeMesh, FVector(X, Y, VillageFloorZ + H * 0.5f), FVector(W, D, H), Wall);
	AddMesh(*N(TEXT("Roof")), CubeMesh, FVector(X, Y, VillageFloorZ + H + 10.f), FVector(W + 60.f, D + 60.f, 20.f), Trim, FRotator::ZeroRotator, false);
	AddMesh(*N(TEXT("Stripe")), CubeMesh, FVector(X, Y, VillageFloorZ + 20.f), FVector(W + 6.f, D + 6.f, 40.f), Trim, FRotator::ZeroRotator, false);
	// Door, windows and a little awning on the side facing the road.
	const float Face = Y > -1200.f ? -1.f : 1.f;
	const float FaceY = Y + Face * (D * 0.5f + 4.f);
	AddMesh(*N(TEXT("Door")), CubeMesh, FVector(X, FaceY, VillageFloorZ + 110.f), FVector(110.f, 8.f, 220.f), VillageDarkWood, FRotator::ZeroRotator, false);
	AddMesh(*N(TEXT("WindowL")), CubeMesh, FVector(X - W * 0.3f, FaceY, VillageFloorZ + H * 0.6f), FVector(90.f, 8.f, 80.f), VillageGlass, FRotator::ZeroRotator, false);
	AddMesh(*N(TEXT("WindowR")), CubeMesh, FVector(X + W * 0.3f, FaceY, VillageFloorZ + H * 0.6f), FVector(90.f, 8.f, 80.f), VillageGlass, FRotator::ZeroRotator, false);
	AddMesh(*N(TEXT("ShutterL")), CubeMesh, FVector(X - W * 0.3f - 60.f, FaceY, VillageFloorZ + H * 0.6f), FVector(25.f, 10.f, 90.f), Trim, FRotator::ZeroRotator, false);
	AddMesh(*N(TEXT("ShutterR")), CubeMesh, FVector(X + W * 0.3f + 60.f, FaceY, VillageFloorZ + H * 0.6f), FVector(25.f, 10.f, 90.f), Trim, FRotator::ZeroRotator, false);
	AddMesh(*N(TEXT("Awning")), CubeMesh, FVector(X, FaceY + Face * 60.f, VillageFloorZ + 250.f), FVector(200.f, 130.f, 8.f), Trim, FRotator(0.f, 0.f, Face * -15.f), false);
	// Flower pot by the door.
	AddMesh(*N(TEXT("Pot")), CylinderMesh, FVector(X + 110.f, FaceY + Face * 40.f, VillageFloorZ + 25.f), FVector(45.f, 45.f, 50.f), FLinearColor(0.7f, 0.35f, 0.2f), FRotator::ZeroRotator, false);
	AddMesh(*N(TEXT("Flowers")), SphereMesh, FVector(X + 110.f, FaceY + Face * 40.f, VillageFloorZ + 65.f), FVector(55.f, 55.f, 40.f), VillageFestive[FMath::Abs(static_cast<int32>(X + Y)) % 6], FRotator::ZeroRotator, false);
}

void ABRVillage::Stall(float X, float Y, const FLinearColor& Awning)
{
	const FString Id = NextName(TEXT("Stall"));
	auto N = [&Id](const TCHAR* Part) { return FString::Printf(TEXT("%s_%s"), *Id, Part); };
	AddMesh(*N(TEXT("Table")), CubeMesh, FVector(X, Y, VillageFloorZ + 45.f), FVector(320.f, 140.f, 90.f), VillageWood);
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector Corner(X + (i % 2 ? 170.f : -170.f), Y + (i / 2 ? 80.f : -80.f), VillageFloorZ + 130.f);
		AddMesh(*FString::Printf(TEXT("%s_Post%d"), *Id, i), CylinderMesh, Corner, FVector(8.f, 8.f, 260.f), VillageDarkWood, FRotator::ZeroRotator, false);
	}
	AddMesh(*N(TEXT("Awning")), CubeMesh, FVector(X, Y, VillageFloorZ + 265.f), FVector(380.f, 210.f, 10.f), Awning, FRotator(0.f, 0.f, -8.f), false);
	for (int32 i = 0; i < 6; ++i)
	{
		AddMesh(*FString::Printf(TEXT("%s_Goods%d"), *Id, i), SphereMesh, FVector(X - 120.f + i * 48.f, Y + ((i % 2) ? 25.f : -25.f), VillageFloorZ + 102.f),
			FVector(34.f, 34.f, 30.f), VillageFestive[(i + static_cast<int32>(PieceCount)) % 6], FRotator::ZeroRotator, false);
	}
}

void ABRVillage::Picado(const FVector2D& A, const FVector2D& B, float Z, int32 Count)
{
	const FString Id = NextName(TEXT("Picado"));
	const FVector2D Dir = (B - A);
	const float Len = Dir.Size();
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
	const FVector2D Mid = (A + B) * 0.5f;
	AddMesh(*FString::Printf(TEXT("%s_String"), *Id), CubeMesh, FVector(Mid.X, Mid.Y, Z), FVector(Len, 2.f, 2.f), FLinearColor(0.9f, 0.9f, 0.9f), FRotator(0.f, Yaw, 0.f), false);
	for (int32 i = 1; i <= Count; ++i)
	{
		const float T = static_cast<float>(i) / (Count + 1);
		const FVector2D P = A + Dir * T;
		const float Sag = FMath::Sin(T * UE_PI) * 60.f; // the string droops in the middle
		AddMesh(*FString::Printf(TEXT("%s_Flag%d"), *Id, i), CubeMesh, FVector(P.X, P.Y, Z - Sag - 22.f), FVector(40.f, 2.f, 44.f), VillageFestive[i % 6], FRotator(0.f, Yaw, 0.f), false);
	}
}

void ABRVillage::Cactus(float X, float Y, float H)
{
	const FString Id = NextName(TEXT("Cactus"));
	AddMesh(*FString::Printf(TEXT("%s_Trunk"), *Id), CylinderMesh, FVector(X, Y, VillageFloorZ + H * 0.5f), FVector(55.f, 55.f, H), VillageCactus);
	AddMesh(*FString::Printf(TEXT("%s_ArmL"), *Id), CylinderMesh, FVector(X, Y - 55.f, VillageFloorZ + H * 0.55f), FVector(35.f, 35.f, H * 0.35f), VillageCactus, FRotator::ZeroRotator, false);
	AddMesh(*FString::Printf(TEXT("%s_ArmLLink"), *Id), CylinderMesh, FVector(X, Y - 35.f, VillageFloorZ + H * 0.4f), FVector(30.f, 30.f, 50.f), VillageCactus, FRotator(0.f, 0.f, 90.f), false);
	AddMesh(*FString::Printf(TEXT("%s_ArmR"), *Id), CylinderMesh, FVector(X, Y + 55.f, VillageFloorZ + H * 0.7f), FVector(35.f, 35.f, H * 0.3f), VillageCactus, FRotator::ZeroRotator, false);
	AddMesh(*FString::Printf(TEXT("%s_ArmRLink"), *Id), CylinderMesh, FVector(X, Y + 35.f, VillageFloorZ + H * 0.58f), FVector(30.f, 30.f, 50.f), VillageCactus, FRotator(0.f, 0.f, 90.f), false);
}

void ABRVillage::Palm(float X, float Y)
{
	const FString Id = NextName(TEXT("Palm"));
	AddMesh(*FString::Printf(TEXT("%s_Trunk"), *Id), CylinderMesh, FVector(X, Y, VillageFloorZ + 250.f), FVector(35.f, 35.f, 500.f), FLinearColor(0.5f, 0.38f, 0.22f), FRotator(0.f, 0.f, 4.f));
	for (int32 i = 0; i < 6; ++i)
	{
		const float Yaw = i * 60.f;
		const FVector Out = FRotator(0.f, Yaw, 0.f).Vector() * 110.f;
		AddMesh(*FString::Printf(TEXT("%s_Frond%d"), *Id, i), CubeMesh, FVector(X + Out.X, Y + Out.Y, VillageFloorZ + 490.f), FVector(230.f, 50.f, 6.f), VillageLeaf, FRotator(-20.f, Yaw, 0.f), false);
	}
}

void ABRVillage::Lamp(float X, float Y)
{
	const FString Id = NextName(TEXT("Lamp"));
	AddMesh(*FString::Printf(TEXT("%s_Post"), *Id), CylinderMesh, FVector(X, Y, VillageFloorZ + 230.f), FVector(14.f, 14.f, 460.f), FLinearColor(0.1f, 0.1f, 0.1f), FRotator::ZeroRotator, false);
	AddMesh(*FString::Printf(TEXT("%s_Bulb"), *Id), SphereMesh, FVector(X, Y, VillageFloorZ + 470.f), FVector(40.f, 40.f, 40.f), FLinearColor(1.f, 0.85f, 0.5f), FRotator::ZeroRotator, false);
	UPointLightComponent* Light = CreateDefaultSubobject<UPointLightComponent>(*FString::Printf(TEXT("%s_Light"), *Id));
	Light->SetupAttachment(Root);
	Light->SetRelativeLocation(FVector(X, Y, VillageFloorZ + 440.f));
	Light->SetIntensity(0.f);
	Light->SetAttenuationRadius(1100.f);
	Light->SetLightColor(FLinearColor(1.f, 0.75f, 0.45f));
	Light->SetCastShadows(false);
	Light->SetMobility(EComponentMobility::Movable);
}

void ABRVillage::Barrel(float X, float Y, const FLinearColor& Color)
{
	AddMesh(*NextName(TEXT("Barrel")), CylinderMesh, FVector(X, Y, VillageFloorZ + 50.f), FVector(70.f, 70.f, 100.f), Color);
}

void ABRVillage::Crate(float X, float Y, float Yaw)
{
	AddMesh(*NextName(TEXT("Crate")), CubeMesh, FVector(X, Y, VillageFloorZ + 40.f), FVector(80.f, 80.f, 80.f), FLinearColor(0.6f, 0.45f, 0.25f), FRotator(0.f, Yaw, 0.f));
}

void ABRVillage::Bench(float X, float Y, float Yaw)
{
	const FString Id = NextName(TEXT("Bench"));
	AddMesh(*FString::Printf(TEXT("%s_Seat"), *Id), CubeMesh, FVector(X, Y, VillageFloorZ + 45.f), FVector(220.f, 50.f, 12.f), VillageWood, FRotator(0.f, Yaw, 0.f));
	AddMesh(*FString::Printf(TEXT("%s_Legs"), *Id), CubeMesh, FVector(X, Y, VillageFloorZ + 20.f), FVector(180.f, 30.f, 40.f), VillageDarkWood, FRotator(0.f, Yaw, 0.f), false);
}

void ABRVillage::Chicken(float X, float Y)
{
	const FString Id = NextName(TEXT("Chicken"));
	USceneComponent* Bird = CreateDefaultSubobject<USceneComponent>(*Id);
	Bird->SetupAttachment(Root);
	Bird->SetRelativeLocation(FVector(X, Y, VillageFloorZ));
	const bool bBrown = (PieceCount % 2) == 0;
	const FLinearColor Feathers = bBrown ? FLinearColor(0.55f, 0.32f, 0.15f) : FLinearColor(0.92f, 0.9f, 0.85f);
	auto Piece = [&](const TCHAR* Part, UStaticMesh* Mesh, const FVector& At, const FVector& Size, const FLinearColor& Color)
	{
		UStaticMeshComponent* C = AddMesh(*FString::Printf(TEXT("%s_%s"), *Id, Part), Mesh, At, Size, Color, FRotator::ZeroRotator, false);
		C->SetupAttachment(Bird);
		C->SetCastShadow(false);
	};
	Piece(TEXT("Body"), SphereMesh, FVector(0.f, 0.f, 22.f), FVector(36.f, 26.f, 28.f), Feathers);
	Piece(TEXT("Head"), SphereMesh, FVector(16.f, 0.f, 40.f), FVector(16.f, 15.f, 16.f), Feathers);
	Piece(TEXT("Comb"), ConeMesh, FVector(16.f, 0.f, 51.f), FVector(6.f, 8.f, 10.f), FLinearColor(0.9f, 0.05f, 0.05f));
	Piece(TEXT("Beak"), ConeMesh, FVector(26.f, 0.f, 40.f), FVector(6.f, 6.f, 8.f), FLinearColor(1.f, 0.75f, 0.1f));
	Piece(TEXT("Legs"), CylinderMesh, FVector(0.f, 0.f, 6.f), FVector(4.f, 14.f, 12.f), FLinearColor(1.f, 0.75f, 0.1f));
}

void ABRVillage::BeginPlay()
{
	Super::BeginPlay();
	TArray<UStaticMeshComponent*> Meshes;
	GetComponents<UStaticMeshComponent>(Meshes);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (const FLinearColor* Color = ColorOverrides.Find(Mesh->GetFName()); Color && !Mesh->ComponentTags.Contains(TEXT("BRThemed")))
		{
			Paint(Mesh, *Color);
		}
	}
	// Components made in the constructor aren't in the Transient arrays on spawned copies - collect them again.
	Chickens.Reset();
	ChickenTargets.Reset();
	Lamps.Reset();
	TArray<USceneComponent*> All;
	GetComponents<USceneComponent>(All);
	for (USceneComponent* Comp : All)
	{
		if (UPointLightComponent* Light = Cast<UPointLightComponent>(Comp))
		{
			Lamps.Add(Light);
		}
		else if (Comp->GetName().StartsWith(TEXT("Chicken")) && Comp->GetAttachParent() == Root)
		{
			Chickens.Add(Comp);
			ChickenTargets.Add(Comp->GetRelativeLocation());
		}
	}
	if (HasAuthority())
	{
		for (int32 i = 0; i < 6; ++i)
		{
			const FVector Local(ChickenYard.X + FMath::Cos(i * 1.3f) * 200.f, ChickenYard.Y + FMath::Sin(i * 1.3f) * 200.f, ChickenYard.Z + 30.f);
			const FTransform T(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), GetActorTransform().TransformPosition(Local));
			if (ABRChicken* Bird = GetWorld()->SpawnActorDeferred<ABRChicken>(ABRChicken::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn))
			{
				Bird->Color = (i % 2) ? FLinearColor(0.55f, 0.32f, 0.15f) : FLinearColor(0.92f, 0.9f, 0.85f);
				Bird->FinishSpawning(T);
			}
		}
	}
	if (GetNetMode() == NM_DedicatedServer)
	{
		SetActorTickEnabled(false);
	}
}

void ABRVillage::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = GetWorld()->GetTimeSeconds();

	// Lamps on at night.
	if (Now >= NextLampCheck)
	{
		NextLampCheck = Now + 1.f;
		const ABRGameState* GS = BRGame(this);
		const bool bNight = GS && GS->IsNight();
		if (bNight != bLampsOn)
		{
			bLampsOn = bNight;
			for (UPointLightComponent* Light : Lamps)
			{
				if (Light)
				{
					Light->SetIntensity(bNight ? LampBrightness : 0.f);
				}
			}
		}
	}

	// Chickens strut, peck and wander around their yard (cosmetic, local only).
	for (int32 i = 0; i < Chickens.Num(); ++i)
	{
		USceneComponent* Bird = Chickens[i];
		if (!Bird || !ChickenTargets.IsValidIndex(i))
		{
			continue;
		}
		FVector Pos = Bird->GetRelativeLocation();
		const FVector To = (ChickenTargets[i] - Pos) * FVector(1.f, 1.f, 0.f);
		FRotator Rot = Bird->GetRelativeRotation();
		if (To.Size() < 15.f)
		{
			if (FMath::FRand() < 0.01f)
			{
				const FVector2D Offset = FVector2D(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f)) * 450.f;
				ChickenTargets[i] = ChickenYard + FVector(Offset.X, Offset.Y, 0.f);
			}
			Rot.Pitch = FMath::Max(0.f, FMath::Sin(Now * 9.f + i)) * 35.f; // peck peck
		}
		else
		{
			Pos += To.GetSafeNormal() * 90.f * DeltaSeconds;
			Pos.Z = ChickenYard.Z + FMath::Abs(FMath::Sin(Now * 14.f + i)) * 4.f;
			Rot = FRotator(0.f, To.Rotation().Yaw, 0.f);
		}
		Bird->SetRelativeLocationAndRotation(Pos, Rot);
	}
}

// ===========================================================================================
// Lucha Libre ring

ABRLuchaRing::ABRLuchaRing()
{
	PrimaryActorTick.bCanEverTick = true;
	const float Half = RingSize * 0.5f;
	const FLinearColor Canvas(0.2f, 0.35f, 0.75f), Apron(0.75f, 0.1f, 0.15f), Rope(0.95f, 0.95f, 0.95f), Gold(1.f, 0.78f, 0.15f);
	AddMesh(TEXT("Deck"), CubeMesh, FVector(0.f, 0.f, DeckHeight * 0.5f), FVector(RingSize, RingSize, DeckHeight), Canvas);
	AddMesh(TEXT("ApronN"), CubeMesh, FVector(0.f, Half + 2.f, DeckHeight * 0.45f), FVector(RingSize, 4.f, DeckHeight * 0.9f), Apron, FRotator::ZeroRotator, false);
	AddMesh(TEXT("ApronS"), CubeMesh, FVector(0.f, -Half - 2.f, DeckHeight * 0.45f), FVector(RingSize, 4.f, DeckHeight * 0.9f), Apron, FRotator::ZeroRotator, false);
	AddMesh(TEXT("ApronE"), CubeMesh, FVector(Half + 2.f, 0.f, DeckHeight * 0.45f), FVector(4.f, RingSize, DeckHeight * 0.9f), Apron, FRotator::ZeroRotator, false);
	AddMesh(TEXT("ApronW"), CubeMesh, FVector(-Half - 2.f, 0.f, DeckHeight * 0.45f), FVector(4.f, RingSize, DeckHeight * 0.9f), Apron, FRotator::ZeroRotator, false);
	// Steps up to the canvas on two sides.
	// Two small steps (20 cm each) so you can walk right up onto the canvas.
	const FLinearColor StepColor(0.3f, 0.3f, 0.32f);
	AddMesh(TEXT("StepW1"), CubeMesh, FVector(-Half - 120.f, 0.f, 10.f), FVector(80.f, 220.f, 20.f), StepColor);
	AddMesh(TEXT("StepW2"), CubeMesh, FVector(-Half - 40.f, 0.f, 20.f), FVector(80.f, 220.f, 40.f), StepColor);
	AddMesh(TEXT("StepE1"), CubeMesh, FVector(Half + 120.f, 0.f, 10.f), FVector(80.f, 220.f, 20.f), StepColor);
	AddMesh(TEXT("StepE2"), CubeMesh, FVector(Half + 40.f, 0.f, 20.f), FVector(80.f, 220.f, 40.f), StepColor);
	for (int32 i = 0; i < 4; ++i)
	{
		const float X = (i % 2) ? Half - 15.f : -Half + 15.f, Y = (i / 2) ? Half - 15.f : -Half + 15.f;
		AddMesh(*FString::Printf(TEXT("Post%d"), i), CylinderMesh, FVector(X, Y, DeckHeight + 75.f), FVector(22.f, 22.f, 150.f), i % 3 == 0 ? Apron : Canvas);
		AddMesh(*FString::Printf(TEXT("Pad%d"), i), CubeMesh, FVector(X, Y, DeckHeight + 140.f), FVector(28.f, 28.f, 22.f), Gold, FRotator::ZeroRotator, false);
	}
	for (int32 Level = 0; Level < 3; ++Level)
	{
		const float Z = DeckHeight + 40.f + Level * 40.f;
		const float In = Half - 15.f;
		AddMesh(*FString::Printf(TEXT("RopeN%d"), Level), CylinderMesh, FVector(0.f, In, Z), FVector(4.f, 4.f, RingSize - 30.f), Rope, FRotator(90.f, 0.f, 0.f), false);
		AddMesh(*FString::Printf(TEXT("RopeS%d"), Level), CylinderMesh, FVector(0.f, -In, Z), FVector(4.f, 4.f, RingSize - 30.f), Rope, FRotator(90.f, 0.f, 0.f), false);
		AddMesh(*FString::Printf(TEXT("RopeE%d"), Level), CylinderMesh, FVector(In, 0.f, Z), FVector(4.f, 4.f, RingSize - 30.f), Rope, FRotator(0.f, 0.f, 90.f), false);
		AddMesh(*FString::Printf(TEXT("RopeW%d"), Level), CylinderMesh, FVector(-In, 0.f, Z), FVector(4.f, 4.f, RingSize - 30.f), Rope, FRotator(0.f, 0.f, 90.f), false);
	}
	Board = AddLabel(TEXT("Board"), FVector(0.f, 0.f, 430.f), 34.f, FColor(255, 210, 60));
	BoardBack = AddLabel(TEXT("BoardBack"), FVector(0.f, 0.f, 430.f), 34.f, FColor(255, 210, 60));
	BoardBack->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	LuchadorNames = { TEXT("EL TORO"), TEXT("EL BIGOTE"), TEXT("LA MOMIA"), TEXT("SR. TACO"), TEXT("EL DIABLO AZUL"), TEXT("CHUPACABRA"), TEXT("EL CACTUS") };
}

void ABRLuchaRing::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRLuchaRing, State);
	DOREPLIFETIME(ABRLuchaRing, StateEndsAt);
	DOREPLIFETIME(ABRLuchaRing, ChallengerName);
	DOREPLIFETIME(ABRLuchaRing, FighterName);
	DOREPLIFETIME(ABRLuchaRing, ChallengerKOs);
	DOREPLIFETIME(ABRLuchaRing, LuchadorKOs);
	DOREPLIFETIME(ABRLuchaRing, Wager);
	DOREPLIFETIME(ABRLuchaRing, PotChallenger);
	DOREPLIFETIME(ABRLuchaRing, PotLuchador);
	DOREPLIFETIME(ABRLuchaRing, ResultText);
}

void ABRLuchaRing::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && bSpawnCounters)
	{
		SpawnCounters();
	}
}

void ABRLuchaRing::SpawnCounters()
{
	const float Out = RingSize * 0.5f + 300.f;
	struct FSpot { FVector Offset; EBRStationKind Kind; int32 Side; int32 Amount; };
	const FSpot Spots[] = {
		{ FVector(-Out, -300.f, 0.f), EBRStationKind::LuchaChallenge, 0, 50 },
		{ FVector(-Out, 300.f, 0.f), EBRStationKind::LuchaChallenge, 0, 100 },
		{ FVector(0.f, -Out, 0.f), EBRStationKind::LuchaChallenge, 0, 250 },
		{ FVector(Out, -300.f, 0.f), EBRStationKind::LuchaBet, 0, 20 },
		{ FVector(Out, 300.f, 0.f), EBRStationKind::LuchaBet, 0, 100 },
		{ FVector(-300.f, Out, 0.f), EBRStationKind::LuchaBet, 1, 20 },
		{ FVector(300.f, Out, 0.f), EBRStationKind::LuchaBet, 1, 100 } };
	for (const FSpot& Spot : Spots)
	{
		const FVector Where = GetActorTransform().TransformPositionNoScale(Spot.Offset);
		const FRotator Facing = (GetActorLocation() - Where).Rotation() + FRotator(0.f, 180.f, 0.f);
		const FTransform T(FRotator(0.f, Facing.Yaw, 0.f), Where);
		if (ABRStation* S = GetWorld()->SpawnActorDeferred<ABRStation>(ABRStation::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			S->Kind = Spot.Kind;
			S->BetSide = Spot.Side;
			S->Price = Spot.Amount;
			S->Ring = this;
			S->Color = Spot.Kind == EBRStationKind::LuchaChallenge ? FLinearColor(0.75f, 0.1f, 0.15f)
				: (Spot.Side == 0 ? FLinearColor(0.3f, 0.6f, 0.3f) : FLinearColor(0.2f, 0.35f, 0.75f));
			S->FinishSpawning(T);
		}
	}
}

FVector ABRLuchaRing::Corner(int32 Which) const
{
	const float C = RingSize * 0.5f - 110.f;
	const FVector Local((Which == 0) ? -C : C, (Which == 0) ? -C : C, DeckHeight + 110.f);
	return GetActorTransform().TransformPositionNoScale(Local);
}

bool ABRLuchaRing::IsOpenForChallenge() const { return State == 0; }
bool ABRLuchaRing::IsTakingBets() const { return State == 1; }
bool ABRLuchaRing::IsChallenger(const APVVCharacter* Who) const { return Who && Who == Challenger.Get(); }

bool ABRLuchaRing::HasBet(const ABRPlayerState* Player) const
{
	return Bets.ContainsByPredicate([Player](const FLuchaBet& B) { return B.Player.Get() == Player; });
}

bool ABRLuchaRing::Challenge(ABRCharacter* Runner, int32 Amount)
{
	ABRPlayerState* PS = Runner ? Runner->GetBRPlayerState() : nullptr;
	ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>();
	const ABRGameState* GS = BRGame(this);
	if (!HasAuthority() || State != 0 || !PS || !GM || !GS || PS->Money < Amount)
	{
		return false;
	}
	const FVector LuchaSpot = Corner(1);
	ABRCharacter* Fighter = GM->SpawnLuchador(FTransform((Corner(0) - LuchaSpot).Rotation(), LuchaSpot), LuchadorClass.Get());
	if (!Fighter)
	{
		return false;
	}
	PS->AddMoney(-Amount);
	Wager = Amount;
	Challenger = Runner;
	Luchador = Fighter;
	ChallengerName = PS->GetPlayerName();
	FighterName = LuchadorNames.Num() > 0 ? LuchadorNames[FMath::RandRange(0, LuchadorNames.Num() - 1)] : FString(TEXT("EL LUCHADOR"));
	Fighter->MakeLuchador(FighterName, 1.f + Amount / 100.f * ToughnessPer100);
	Runner->TeleportTo(Corner(0), (LuchaSpot - Corner(0)).Rotation(), false, true);
	ChallengerKOs = LuchadorKOs = 0;
	PotChallenger = PotLuchador = 0;
	Bets.Reset();
	bChallengerWasDown = bLuchadorWasDown = false;
	ResultText.Reset();
	State = 1;
	StateEndsAt = GS->GetServerWorldTimeSeconds() + BettingSeconds;
	GM->Feed(FString::Printf(TEXT("%s challenged %s for $%d! Place your bets at the ring!"), *ChallengerName, *FighterName, Amount));
	ABRGameState::Sfx(this, EBRSfx::Bounty, GetActorLocation());
	Runner->Notify(FText::Format(LOCTEXT("LuchaReady", "Get ready! The bell rings in {0} seconds. Knock {1} down twice. Don't leave the ring!"),
		FMath::RoundToInt(BettingSeconds), FText::FromString(FighterName)));
	ForceNetUpdate();
	return true;
}

int32 ABRLuchaRing::PlaceBet(ABRPlayerState* Player, int32 Side, int32 Amount)
{
	if (!HasAuthority() || !Player || State != 1 || HasBet(Player) || Player->Money <= 0)
	{
		return 0;
	}
	FLuchaBet Bet;
	Bet.Player = Player;
	Bet.Side = Side == 0 ? 0 : 1;
	Bet.Amount = FMath::Min(Amount > 0 ? Amount : Player->Money, Player->Money);
	Player->AddMoney(-Bet.Amount);
	(Bet.Side == 0 ? PotChallenger : PotLuchador) += Bet.Amount;
	Bets.Add(Bet);
	return Bet.Amount;
}

void ABRLuchaRing::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const ABRGameState* GS = BRGame(this);
	const float ServerNow = GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	if (GetNetMode() != NM_DedicatedServer)
	{
		UpdateBoard(ServerNow);
	}
	if (!HasAuthority())
	{
		return;
	}
	switch (State)
	{
	case 1:
	case 2:
		TickFight(ServerNow);
		break;
	case 3:
		if (ServerNow >= StateEndsAt)
		{
			if (ABRCharacter* Fighter = Luchador.Get())
			{
				Fighter->Destroy();
			}
			Luchador = nullptr;
			Challenger = nullptr;
			State = 0;
			ForceNetUpdate();
		}
		break;
	default:
		break;
	}
}

void ABRLuchaRing::TickFight(float ServerNow)
{
	ABRCharacter* C = Challenger.Get();
	ABRCharacter* L = Luchador.Get();
	if (!L)
	{
		Finish(0, TEXT("the luchador ran off"));
		return;
	}
	const FVector Center = GetActorLocation();
	const float Limit = RingSize * 0.5f + 250.f;
	if (!C || C->GetCondition() == EPVVCondition::Restrained || C->GetCondition() == EPVVCondition::Detained || C->GetCarrier()
		|| FVector::Dist2D(C->GetActorLocation(), Center) > Limit)
	{
		Finish(1, TEXT("the challenger left the ring"));
		return;
	}
	if (State == 1)
	{
		// Waiting for the bell: stare each other down.
		L->SetActorRotation(FRotator(0.f, (C->GetActorLocation() - L->GetActorLocation()).Rotation().Yaw, 0.f));
		if (ServerNow >= StateEndsAt)
		{
			State = 2;
			StateEndsAt = ServerNow + FightSeconds;
			ABRGameState::Sfx(this, EBRSfx::RoundStart, Center);
			C->Notify(LOCTEXT("LuchaBell", "DING DING! FIGHT!"));
			ForceNetUpdate();
		}
		return;
	}

	// Knockdown counting.
	const bool bCDown = C->GetCondition() == EPVVCondition::KnockedDown;
	const bool bLDown = L->GetCondition() == EPVVCondition::KnockedDown;
	if (bCDown && !bChallengerWasDown) { ++LuchadorKOs; ForceNetUpdate(); }
	if (bLDown && !bLuchadorWasDown) { ++ChallengerKOs; ForceNetUpdate(); }
	bChallengerWasDown = bCDown;
	bLuchadorWasDown = bLDown;
	// Thrown out of the ring = counts as a knockdown, then back to his corner.
	if (FVector::Dist2D(L->GetActorLocation(), Center) > Limit && L->GetCondition() == EPVVCondition::Normal && !L->GetCarrier())
	{
		++ChallengerKOs;
		L->TeleportTo(Corner(1), L->GetActorRotation(), false, true);
		C->Notify(LOCTEXT("RingOut", "RING OUT! That counts!"));
		ForceNetUpdate();
	}
	if (ChallengerKOs >= KnockdownsToWin)
	{
		Finish(0, TEXT("by knockout"));
		return;
	}
	if (LuchadorKOs >= KnockdownsToWin)
	{
		Finish(1, TEXT("by knockout"));
		return;
	}
	if (ServerNow >= StateEndsAt)
	{
		Finish(ChallengerKOs > LuchadorKOs ? 0 : 1, TEXT("on points"));
		return;
	}

	// The luchador's brain (Gang Beasts style): charge in, circle, then punch combos, grabs, headbutts, kicks, windmills, dives.
	if (L->GetCondition() != EPVVCondition::Normal)
	{
		return;
	}
	const float Delta = GetWorld()->GetDeltaSeconds();
	const FVector To = (C->GetActorLocation() - L->GetActorLocation()) * FVector(1.f, 1.f, 0.f);
	const float Dist = To.Size();
	const float Toughness = FMath::Clamp(1.f + Wager / 100.f * ToughnessPer100, 1.f, 3.f);
	if (bCDown)
	{
		// Showboat while the challenger's down: back off and flex.
		if (Dist < 260.f)
		{
			L->AddMovementInput(-To.GetSafeNormal(), 0.6f);
		}
		return;
	}
	if (!L->GetHandGrabTarget() && !L->GetGrabbedBy())
	{
		L->GetCharacterMovement()->MaxWalkSpeed = 360.f + 60.f * Toughness;
		if (Dist > 140.f)
		{
			L->AddMovementInput(To.GetSafeNormal(), 1.f); // charge!
		}
		else
		{
			// Circle around you a little, like a wrestler sizing you up.
			const FVector Around = FVector::CrossProduct(To.GetSafeNormal(), FVector::UpVector) * FMath::Sin(GetWorld()->GetTimeSeconds() * 1.7f);
			L->AddMovementInput(Around, 0.5f);
		}
	}
	L->BrawlerAct(C, FMath::Clamp(0.45f + 0.15f * Toughness, 0.f, 1.f), Delta);
}

void ABRLuchaRing::Finish(int32 WinnerSide, const FString& Why)
{
	ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>();
	const ABRGameState* GS = BRGame(this);
	ABRCharacter* C = Challenger.Get();
	ABRPlayerState* PS = C ? C->GetBRPlayerState() : nullptr;
	if (WinnerSide == 0)
	{
		ResultText = FString::Printf(TEXT("%s WINS %s! (+$%d)"), *ChallengerName.ToUpper(), *Why, Wager * 2);
		if (PS)
		{
			PS->AddMoney(Wager * 2);
			++PS->StatLuchaWins;
		}
		if (C)
		{
			C->SetHat(EBRHat::LuchaMask); // won his mask!
			ABRGameState::Highlight(this, C, 90.f, FString::Printf(TEXT("%s beat %s in the ring"), *ChallengerName, *FighterName));
			C->Notify(FText::Format(LOCTEXT("LuchaWin", "YOU BEAT {0}! +${1} and his MASK (dives and tackles hit harder)."), FText::FromString(FighterName), Wager * 2));
			ABRGameState::FX(this, EBRFX::MoneyText, C->GetActorLocation() + FVector(0.f, 0.f, 80.f), FString::Printf(TEXT("+$%d"), Wager * 2));
		}
		ABRGameState::Sfx(this, EBRSfx::Payday, GetActorLocation());
	}
	else
	{
		ResultText = FString::Printf(TEXT("%s WINS %s!"), *FighterName, *Why);
		if (C)
		{
			C->Notify(FText::Format(LOCTEXT("LuchaLose", "{0} beat you. There goes ${1}."), FText::FromString(FighterName), Wager));
		}
		ABRGameState::Sfx(this, EBRSfx::RoundEnd, GetActorLocation());
	}
	// Bets: right side gets double.
	for (const FLuchaBet& Bet : Bets)
	{
		if (ABRPlayerState* Better = Bet.Player.Get(); Better && Bet.Side == WinnerSide)
		{
			Better->AddMoney(Bet.Amount * 2);
		}
	}
	if (GM)
	{
		GM->Feed(WinnerSide == 0 ? FString::Printf(TEXT("%s BEAT %s in the ring (+$%d)"), *ChallengerName, *FighterName, Wager * 2)
			: FString::Printf(TEXT("%s destroyed %s in the ring"), *FighterName, *ChallengerName));
	}
	Bets.Reset();
	State = 3;
	StateEndsAt = (GS ? GS->GetServerWorldTimeSeconds() : 0.f) + 6.f;
	ForceNetUpdate();
}

void ABRLuchaRing::UpdateBoard(float ServerNow)
{
	FString Text;
	const int32 Left = FMath::Max(0, FMath::CeilToInt(StateEndsAt - ServerNow));
	switch (State)
	{
	case 1: Text = FString::Printf(TEXT("%s vs %s  ($%d)\nBETS CLOSE IN %d  -  $%d vs $%d"), *ChallengerName, *FighterName, Wager, Left, PotChallenger, PotLuchador); break;
	case 2: Text = FString::Printf(TEXT("%s  %d - %d  %s\n%d:%02d"), *ChallengerName, ChallengerKOs, LuchadorKOs, *FighterName, Left / 60, Left % 60); break;
	case 3: Text = ResultText; break;
	default: Text = TEXT("LUCHA LIBRE!\nBeat the luchador: DOUBLE your money + his mask"); break;
	}
	for (UTextRenderComponent* B : { Board.Get(), BoardBack.Get() })
	{
		if (B)
		{
			B->SetText(FText::FromString(Text));
		}
	}
}

// ===========================================================================================
// Hiding spots

ABRHidingSpot::ABRHidingSpot()
{
	// All three looks are built; BeginPlay shows the chosen one.
	const FLinearColor Green(0.15f, 0.35f, 0.2f), Blue(0.2f, 0.4f, 0.75f), Straw(0.85f, 0.72f, 0.35f);
	AddMesh(TEXT("Dumpster_Body"), CubeMesh, FVector(0.f, 0.f, 70.f), FVector(220.f, 130.f, 140.f), Green);
	AddMesh(TEXT("Dumpster_Lid"), CubeMesh, FVector(-10.f, 0.f, 145.f), FVector(230.f, 140.f, 10.f), FLinearColor(0.1f, 0.1f, 0.1f), FRotator(0.f, 0.f, 0.f), false);
	AddMesh(TEXT("Potty_Body"), CubeMesh, FVector(0.f, 0.f, 120.f), FVector(120.f, 120.f, 240.f), Blue);
	AddMesh(TEXT("Potty_Roof"), CylinderMesh, FVector(0.f, 0.f, 245.f), FVector(130.f, 130.f, 15.f), FLinearColor(0.9f, 0.9f, 0.9f), FRotator::ZeroRotator, false);
	AddMesh(TEXT("Potty_Door"), CubeMesh, FVector(61.f, 0.f, 105.f), FVector(4.f, 80.f, 190.f), FLinearColor(0.15f, 0.3f, 0.6f), FRotator::ZeroRotator, false);
	AddMesh(TEXT("Hay_A"), CubeMesh, FVector(0.f, -60.f, 50.f), FVector(180.f, 110.f, 100.f), Straw);
	AddMesh(TEXT("Hay_B"), CubeMesh, FVector(0.f, 60.f, 50.f), FVector(180.f, 110.f, 100.f), Straw);
	AddMesh(TEXT("Hay_C"), CubeMesh, FVector(0.f, 0.f, 150.f), FVector(180.f, 110.f, 100.f), Straw, FRotator(0.f, 90.f, 0.f));
	AddPickupZone(130.f);
}

void ABRHidingSpot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRHidingSpot, Style);
	DOREPLIFETIME(ABRHidingSpot, Hider);
}

void ABRHidingSpot::BeginPlay()
{
	Super::BeginPlay();
	const TCHAR* Prefix = Style == EBRHideStyle::PortaPotty ? TEXT("Potty") : (Style == EBRHideStyle::HayBale ? TEXT("Hay") : TEXT("Dumpster"));
	TArray<UStaticMeshComponent*> Meshes;
	GetComponents<UStaticMeshComponent>(Meshes);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		const bool bMine = Mesh->GetName().StartsWith(Prefix);
		Mesh->SetVisibility(bMine);
		Mesh->SetCollisionEnabled(bMine ? Mesh->GetCollisionEnabled() : ECollisionEnabled::NoCollision);
	}
}

FString ABRHidingSpot::StyleName() const
{
	return Style == EBRHideStyle::PortaPotty ? TEXT("porta-potty") : (Style == EBRHideStyle::HayBale ? TEXT("hay bales") : TEXT("dumpster"));
}

bool ABRHidingSpot::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	const ABRCharacter* BRC = Cast<ABRCharacter>(Interactor);
	if (!BRC || BRC->IsNPC())
	{
		return false;
	}
	if (Hider.Get() == BRC)
	{
		return true; // climb out
	}
	if (BRC->IsHidingInSpot() || Interactor->GetCondition() != EPVVCondition::Normal || Interactor->GetCarriedCharacter())
	{
		return false;
	}
	if (BRC->IsWarden())
	{
		return true; // search it
	}
	return BRC->IsRunner() && !Hider;
}

float ABRHidingSpot::GetInteractDuration_Implementation(APVVCharacter* Interactor) const
{
	if (Hider.Get() == Interactor)
	{
		return 0.f;
	}
	return Interactor && Interactor->GetFaction() == BR::Wardens ? 1.5f : 0.8f;
}

FText ABRHidingSpot::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	const FText Where = FText::FromString(StyleName());
	if (Hider.Get() == Interactor)
	{
		return LOCTEXT("HideOut", "Climb out");
	}
	if (Interactor && Interactor->GetFaction() == BR::Wardens)
	{
		return FText::Format(LOCTEXT("HideSearch", "Search the {0} (hold)"), Where);
	}
	return FText::Format(LOCTEXT("HideIn", "Hide in the {0}"), Where);
}

void ABRHidingSpot::OnInteract_Implementation(APVVCharacter* Interactor)
{
	ABRCharacter* BRC = Cast<ABRCharacter>(Interactor);
	if (!HasAuthority() || !BRC)
	{
		return;
	}
	const FVector Out = GetActorLocation() + GetActorForwardVector() * 170.f + FVector(0.f, 0.f, 100.f);
	if (Hider.Get() == BRC)
	{
		BRC->SetHiddenInSpot(false, Out);
		Hider = nullptr;
		ForceNetUpdate();
		return;
	}
	if (BRC->IsWarden())
	{
		if (ABRCharacter* Found = Hider.Get())
		{
			// Dragged out by the collar.
			Hider = nullptr;
			Found->SetHiddenInSpot(false, Out);
			Found->ApplyKnockdown(BRC, GetActorForwardVector() * 300.f + FVector(0.f, 0.f, 200.f));
			Found->Spot(8.f);
			if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
			{
				GM->Feed(FString::Printf(TEXT("%s found %s hiding in a %s"), *BRC->GetPlayerDisplayName(), *Found->GetPlayerDisplayName(), *StyleName()));
			}
		}
		else
		{
			Tell(Interactor, Style == EBRHideStyle::PortaPotty ? LOCTEXT("SearchPotty", "Empty. You'll never get that smell out of your uniform.")
				: LOCTEXT("SearchEmpty", "Nobody in there."));
		}
		ForceNetUpdate();
		return;
	}
	if (!Hider)
	{
		Hider = BRC;
		BRC->SetHiddenInSpot(true, Out);
		BRC->TeleportTo(GetActorLocation() + FVector(0.f, 0.f, 100.f), BRC->GetActorRotation(), false, true);
		Tell(Interactor, LOCTEXT("Hidden", "Hidden! Border Patrol can't see you. Press E to climb out."));
		ForceNetUpdate();
	}
}

// ===========================================================================================
// Mariachi band

ABRMariachiBand::ABRMariachiBand()
{
	PrimaryActorTick.bCanEverTick = true;
	const FLinearColor Wood(0.5f, 0.32f, 0.18f), Red(0.8f, 0.1f, 0.12f), Green(0.1f, 0.55f, 0.25f), White(0.95f, 0.95f, 0.92f);
	AddMesh(TEXT("Stage"), CubeMesh, FVector(0.f, 0.f, 20.f), FVector(600.f, 400.f, 40.f), Wood);
	AddMesh(TEXT("StepFront"), CubeMesh, FVector(240.f, 0.f, 10.f), FVector(80.f, 200.f, 20.f), Wood);
	for (int32 i = 0; i < 3; ++i)
	{
		const FLinearColor Stripe = i == 0 ? Green : (i == 1 ? White : Red);
		AddMesh(*FString::Printf(TEXT("Banner%d"), i), CubeMesh, FVector(-195.f, -200.f + i * 200.f, 230.f), FVector(10.f, 200.f, 120.f), Stripe, FRotator::ZeroRotator, false);
	}
	AddMesh(TEXT("PostL"), CylinderMesh, FVector(-195.f, -305.f, 160.f), FVector(15.f, 15.f, 320.f), Wood, FRotator::ZeroRotator, false);
	AddMesh(TEXT("PostR"), CylinderMesh, FVector(-195.f, 305.f, 160.f), FVector(15.f, 15.f, 320.f), Wood, FRotator::ZeroRotator, false);
	AddLabel(TEXT("Sign"), FVector(-190.f, 0.f, 330.f), 50.f, FColor(255, 210, 60))->SetText(FText::FromString(TEXT("MARIACHI\nbring a trumpet, join the band (E)")));
	AddPickupZone(320.f);
}

void ABRMariachiBand::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRMariachiBand, BeatStartTime);
	DOREPLIFETIME(ABRMariachiBand, Performers);
	DOREPLIFETIME(ABRMariachiBand, Streaks);
}

void ABRMariachiBand::BeginPlay()
{
	Super::BeginPlay();
	if (Music && GetNetMode() != NM_DedicatedServer)
	{
		MusicAudio = UGameplayStatics::SpawnSoundAttached(Music, Root, NAME_None, FVector(0.f, 0.f, 150.f), EAttachLocation::KeepRelativeOffset, true);
	}
	if (!HasAuthority())
	{
		return;
	}
	const ABRGameState* GS = BRGame(this);
	BeatStartTime = GS ? GS->GetServerWorldTimeSeconds() : 0.f;
	// Three musicians in sombreros on the back of the stage.
	if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
	{
		for (int32 i = 0; i < 3; ++i)
		{
			const FVector At = GetActorTransform().TransformPositionNoScale(FVector(-110.f, -150.f + i * 150.f, 140.f));
			if (ABRCharacter* Player = GM->SpawnLuchador(FTransform(GetActorRotation(), At), nullptr))
			{
				Player->SetHat(EBRHat::Sombrero);
				Player->SetNPCHeldItem(EBRItem::Trumpet);
				Player->bPeaceful = true;	// flailing arms never knock bandmates over
				Player->bNoFightBack = true;
				Player->ForceDance(100000.f);
				Musicians.Add(Player);
			}
		}
	}
}

void ABRMariachiBand::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (ABRCharacter* Player : Musicians)
	{
		if (IsValid(Player))
		{
			Player->Destroy();
		}
	}
	Super::EndPlay(EndPlayReason);
}

float ABRMariachiBand::GetBeatPhase() const
{
	const ABRGameState* GS = BRGame(this);
	const float T = (GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds()) - BeatStartTime;
	const float Beats = T * FMath::Max(30.f, BeatsPerMinute) / 60.f;
	return Beats - FMath::FloorToFloat(Beats);
}

bool ABRMariachiBand::IsPerformer(const ABRCharacter* Who) const
{
	return Who && Performers.ContainsByPredicate([Who](const TObjectPtr<ABRCharacter>& P) { return P.Get() == Who; });
}

int32 ABRMariachiBand::GetStreak(const ABRCharacter* Who) const
{
	for (int32 i = 0; i < Performers.Num(); ++i)
	{
		if (Performers[i].Get() == Who && Streaks.IsValidIndex(i))
		{
			return Streaks[i];
		}
	}
	return 0;
}

bool ABRMariachiBand::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	const ABRCharacter* BRC = Cast<ABRCharacter>(Interactor);
	const ABRPlayerState* PS = BRState(Interactor);
	if (!BRC || BRC->IsNPC() || !PS || Interactor->GetCondition() != EPVVCondition::Normal)
	{
		return false;
	}
	return IsPerformer(BRC) || PS->HasItem(EBRItem::Trumpet);
}

FText ABRMariachiBand::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	return IsPerformer(Cast<ABRCharacter>(Interactor)) ? LOCTEXT("BandLeave", "Leave the band")
		: LOCTEXT("BandJoin", "Join the band! (pick your trumpet with the mouse wheel, press F on the beat)");
}

void ABRMariachiBand::OnInteract_Implementation(APVVCharacter* Interactor)
{
	ABRCharacter* BRC = Cast<ABRCharacter>(Interactor);
	if (!HasAuthority() || !BRC)
	{
		return;
	}
	const int32 Index = Performers.IndexOfByKey(BRC);
	if (Index != INDEX_NONE)
	{
		RemovePerformer(Index);
		return;
	}
	Performers.Add(BRC);
	Streaks.Add(0);
	TipsThisMinute.Add(0);
	TipWindowStart.Add(GetWorld()->GetTimeSeconds());
	BRC->TeleportTo(GetActorTransform().TransformPositionNoScale(FVector(60.f, FMath::FRandRange(-120.f, 120.f), 140.f)), GetActorRotation(), false, true);
	Tell(Interactor, LOCTEXT("BandJoined", "You're in the band! Press F ON THE BEAT (watch the circle). Hit notes in a row = tips!"));
	ForceNetUpdate();
}

void ABRMariachiBand::RemovePerformer(int32 Index)
{
	if (!Performers.IsValidIndex(Index))
	{
		return;
	}
	Performers.RemoveAt(Index);
	if (Streaks.IsValidIndex(Index)) Streaks.RemoveAt(Index);
	if (TipsThisMinute.IsValidIndex(Index)) TipsThisMinute.RemoveAt(Index);
	if (TipWindowStart.IsValidIndex(Index)) TipWindowStart.RemoveAt(Index);
	ForceNetUpdate();
}

void ABRMariachiBand::PlayNote(ABRCharacter* Who)
{
	const int32 Index = Performers.IndexOfByKey(Who);
	if (!HasAuthority() || Index == INDEX_NONE || !Streaks.IsValidIndex(Index))
	{
		return;
	}
	// How far from the nearest beat was it? (Allow for the player's ping.)
	const ABRPlayerState* PS = Who->GetBRPlayerState();
	const float BeatLen = 60.f / FMath::Max(30.f, BeatsPerMinute);
	const ABRGameState* GS = BRGame(this);
	const float Lag = PS ? PS->GetPingInMilliseconds() * 0.0005f : 0.f;
	const float T = (GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds()) - BeatStartTime - Lag;
	const float Into = FMath::Fmod(FMath::Max(0.f, T), BeatLen);
	const float Off = FMath::Min(Into, BeatLen - Into);
	const bool bGood = Off <= BeatWindow;
	// ONE trumpet sound per note: the band's note sound if set, otherwise your trumpet.
	if (NoteSounds.Num() > 0 || !bGood)
	{
		Who->PlayBandNoteSilent();
	}
	else
	{
		Who->PlayBandNote();
	}
	Multicast_Note(Who->GetActorLocation() + FVector(0.f, 0.f, 120.f), bGood);
	if (!bGood)
	{
		Streaks[Index] = 0;
		ForceNetUpdate();
		return;
	}
	++Streaks[Index];
	// Every 4 notes in a row the crowd throws money (more for longer streaks).
	if (Streaks[Index] % 4 == 0 && PS)
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now - TipWindowStart[Index] > 60.f)
		{
			TipWindowStart[Index] = Now;
			TipsThisMinute[Index] = 0;
		}
		const int32 Tip = FMath::Min(5 + 3 * (Streaks[Index] / 4), MaxTipsPerMinute - TipsThisMinute[Index]);
		if (Tip > 0)
		{
			TipsThisMinute[Index] += Tip;
			if (ABRPlayerState* Earner = Who->GetBRPlayerState())
			{
				Earner->AddMoney(Tip);
			}
			ABRGameState::FX(this, EBRFX::MoneyText, Who->GetActorLocation() + FVector(0.f, 0.f, 90.f), FString::Printf(TEXT("TIP +$%d"), Tip));
			ABRGameState::Sfx(this, EBRSfx::CashPickup, Who->GetActorLocation());
		}
		else
		{
			Who->Notify(LOCTEXT("CrowdBroke", "The crowd's out of change. Play again in a minute!"));
		}
	}
	ForceNetUpdate();
}

void ABRMariachiBand::Multicast_Note_Implementation(FVector_NetQuantize Where, bool bGood)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	USoundBase* Sound = bGood ? (NoteSounds.Num() > 0 ? NoteSounds[FMath::RandRange(0, NoteSounds.Num() - 1)].Get() : nullptr) : MissSound.Get();
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Where);
	}
}

void ABRMariachiBand::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (MusicAudio)
	{
		MusicAudio->SetVolumeMultiplier(FPVVPlayerOptions::MusicVolume);
	}
	if (!HasAuthority())
	{
		return;
	}
	// Performers who walk off the stage leave the band.
	for (int32 i = Performers.Num() - 1; i >= 0; --i)
	{
		const ABRCharacter* P = Performers[i].Get();
		if (!P || P->GetCondition() != EPVVCondition::Normal || FVector::Dist2D(P->GetActorLocation(), GetActorLocation()) > 450.f)
		{
			RemovePerformer(i);
		}
	}
	// The band toots along.
	const float Now = GetWorld()->GetTimeSeconds();
	// The band toots along - but only ONE trumpet at a time, and it goes quiet while a player is soloing.
	if (Now >= NextMusicianNote && Musicians.Num() > 0)
	{
		NextMusicianNote = Now + 60.f / FMath::Max(30.f, BeatsPerMinute) * 2.f;
		const int32 Lead = FMath::RandRange(0, Musicians.Num() - 1);
		for (int32 i = 0; i < Musicians.Num(); ++i)
		{
			if (ABRCharacter* Player = Musicians[i].Get())
			{
				if (i == Lead && Performers.Num() == 0)
				{
					Player->PlayBandNote();
				}
				else
				{
					Player->PlayBandNoteSilent(); // the others just mime along
				}
			}
		}
	}
}

// ===========================================================================================
// Little effects

ABRFX::ABRFX()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	USceneComponent* FXRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(FXRoot);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	for (int32 i = 0; i < 7; ++i)
	{
		UStaticMeshComponent* B = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Blob%d"), i));
		B->SetupAttachment(FXRoot);
		B->SetStaticMesh(Sphere.Object);
		B->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		B->SetCastShadow(false);
		B->SetRelativeScale3D(FVector(0.01f));
		Blobs.Add(B);
	}
	Popup = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Popup"));
	Popup->SetupAttachment(FXRoot);
	Popup->SetHorizontalAlignment(EHTA_Center);
	Popup->SetWorldSize(46.f);
	Popup->SetHiddenInGame(true);
}

void ABRFX::BeginPlay()
{
	Super::BeginPlay();
	const bool bText = Kind == EBRFX::MoneyText || Kind == EBRFX::PointsText;
	FLinearColor Color(0.75f, 0.62f, 0.42f); // dust
	Life = 0.9f;
	BlobSize = 45.f;
	if (Kind == EBRFX::Splash) { Color = FLinearColor(0.75f, 0.88f, 1.f); BlobSize = 35.f; Life = 0.8f; }
	if (Kind == EBRFX::Smoke) { Color = FLinearColor(0.35f, 0.35f, 0.37f); BlobSize = 80.f; Life = 1.6f; }
	for (UStaticMeshComponent* B : Blobs)
	{
		B->SetHiddenInGame(bText);
		if (!bText)
		{
			if (UMaterialInstanceDynamic* MID = B->CreateDynamicMaterialInstance(0))
			{
				MID->SetVectorParameterValue(TEXT("Color"), Color);
			}
		}
		FVector D = FMath::VRand();
		D.Z = Kind == EBRFX::Splash ? FMath::Abs(D.Z) * 2.5f : FMath::Abs(D.Z) * 0.6f;
		Drift.Add(D * (Kind == EBRFX::Smoke ? 120.f : 160.f));
	}
	if (bText && Popup)
	{
		Life = 1.6f;
		Popup->SetHiddenInGame(false);
		Popup->SetText(FText::FromString(Text));
		Popup->SetTextRenderColor(Kind == EBRFX::MoneyText ? FColor(90, 255, 90) : FColor(255, 220, 60));
	}
	SetLifeSpan(Life + 0.1f);
}

void ABRFX::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float T = FMath::Clamp(Age / Life, 0.f, 1.f);
	if (Popup && (Kind == EBRFX::MoneyText || Kind == EBRFX::PointsText))
	{
		Popup->SetRelativeLocation(FVector(0.f, 0.f, 40.f + T * 120.f));
		if (const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
		{
			const FVector ToCam = Cam->GetCameraLocation() - Popup->GetComponentLocation();
			Popup->SetWorldRotation(FRotator(0.f, ToCam.Rotation().Yaw, 0.f));
		}
		return;
	}
	// Puffs swell, drift, then shrink away.
	const float Size = BlobSize / 100.f * FMath::Sin(T * UE_PI) * (0.6f + T);
	for (int32 i = 0; i < Blobs.Num(); ++i)
	{
		if (UStaticMeshComponent* B = Blobs[i].Get())
		{
			B->SetRelativeLocation(Drift[i] * T + (Kind == EBRFX::Splash ? FVector(0.f, 0.f, -300.f * T * T) : FVector::ZeroVector));
			B->SetRelativeScale3D(FVector(FMath::Max(0.01f, Size * (0.7f + 0.1f * i))));
		}
	}
}

#undef LOCTEXT_NAMESPACE
