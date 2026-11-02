#include "BRWorldActors.h"
#include "BRCharacter.h"
#include "BRGameMode.h"
#include "BRGameState.h"
#include "BRPlayerState.h"
#include "PVVBoat.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
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
	TArray<UStaticMeshComponent*> Meshes;
	GetComponents<UStaticMeshComponent>(Meshes);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
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
	DOREPLIFETIME(ABRStation, Price);
	DOREPLIFETIME(ABRStation, WorkSeconds);
	DOREPLIFETIME(ABRStation, CooldownSeconds);
	DOREPLIFETIME(ABRStation, bNightOnly);
	DOREPLIFETIME(ABRStation, Title);
	DOREPLIFETIME(ABRStation, Color);
	DOREPLIFETIME(ABRStation, BetSide);
	DOREPLIFETIME(ABRStation, Pit);
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
		default: break;
		}
	}
	Label->SetText(FText::FromString(Text));
}

bool ABRStation::IsOpen() const
{
	const ABRGameState* GS = BRGame(this);
	if (!GS || GS->Phase == EBRPhase::Ended)
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
	return true;
}

float ABRStation::GetInteractDuration_Implementation(APVVCharacter* Interactor) const
{
	return Kind == EBRStationKind::Job ? WorkSeconds : 0.f;
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
	default: return FText::Format(LOCTEXT("Job", "{0} (hold) +${1}"), FText::FromString(Title), Price);
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
		if (Item == EBRItem::WireCutters)
		{
			PS->WireCutterCharges += 2;
			Tell(Interactor, LOCTEXT("BoughtCutters", "Bought: Wire Cutters (good for 2 cuts - and they're LOUD)"));
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
		PS->AddMoney(Price);
		if (Item != EBRItem::None && !PS->AddItem(Item))
		{
			Tell(Interactor, LOCTEXT("JobFull", "Pockets full - you only got paid."));
		}
		NextUseServerTime = GS->GetServerWorldTimeSeconds() + CooldownSeconds;
		break;
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
	const FVector Start = Basket->GetComponentLocation();
	Passenger->ReleaseFromDetention(Start);

	// Flight time that minimises launch speed (needs Flat < ~160 m to stay under terminal velocity).
	const FVector Delta = Target - Start;
	const float Flat = FVector(Delta.X, Delta.Y, 0.f).Size();
	const float Gravity = 980.f;
	const float T = FMath::Max(2.5f, FMath::Sqrt(2.f * Flat / Gravity));
	FVector Velocity = FVector(Delta.X, Delta.Y, 0.f) / T;
	Velocity.Z = Delta.Z / T + 0.5f * Gravity * T;
	Passenger->LaunchCharacter(Velocity, true, true);
	Multicast_Fling();
}

void ABRCatapult::Multicast_Fling_Implementation()
{
	FlingTime = GetWorld()->GetTimeSeconds();
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

	Barrier = CreateDefaultSubobject<UBoxComponent>(TEXT("Barrier"));
	Barrier->SetupAttachment(Root);
	Barrier->SetBoxExtent(FVector(60.f, Half, 400.f));
	Barrier->SetRelativeLocation(FVector(0.f, 0.f, 400.f));
}

void ABRCheckpointGate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRCheckpointGate, bOpen);
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
	if (HasAuthority() && bOpen && GetWorld()->GetTimeSeconds() > CloseAt)
	{
		bOpen = false;
	}
	ArmAngle = FMath::FInterpTo(ArmAngle, bOpen ? 80.f : 0.f, DeltaSeconds, 4.f);
	ArmPivot->SetRelativeRotation(FRotator(0.f, 0.f, ArmAngle));
	const int32 State = bOpen ? 1 : 0;
	if (State != AppliedState)
	{
		AppliedState = State;
		Barrier->SetCollisionProfileName(bOpen ? TEXT("NoCollision") : TEXT("BlockAll"));
	}
}

bool ABRCheckpointGate::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	return IsFreeWarden(Interactor);
}

FText ABRCheckpointGate::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	return bOpen ? LOCTEXT("CloseGate", "Close the gate") : LOCTEXT("OpenGate", "Wave them through (open gate 12 s)");
}

void ABRCheckpointGate::OnInteract_Implementation(APVVCharacter* Interactor)
{
	if (!HasAuthority())
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
		Victim->Notify(LOCTEXT("BearTrap", "CLANG! Bear trap - you're stuck!"));
		break;
	case EBRTrap::Glue:
		Victim->ApplySlow(8.f, 0.3f);
		Victim->Notify(LOCTEXT("Glue", "Ugh, glue! You can barely move."));
		break;
	case EBRTrap::Alarm:
		Victim->Spot(8.f);
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
	for (int32 i = 0; i < 5; ++i)
	{
		Chicken(ChickenYard.X + FMath::Cos(i * 1.3f) * 200.f, ChickenYard.Y + FMath::Sin(i * 1.3f) * 200.f);
	}
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
		if (const FLinearColor* Color = ColorOverrides.Find(Mesh->GetFName()))
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

#undef LOCTEXT_NAMESPACE
