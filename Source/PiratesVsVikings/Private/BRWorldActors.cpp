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
	Label->SetText(FText::FromString(Title));
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
	const float Side = FMath::Sign(Victim->GetActorLocation().X - GetActorLocation().X);
	Victim->ReceiveMeleeHit(nullptr, 35.f, FVector(Side * 700.f, 0.f, 300.f));
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
	const float Side = FMath::Sign(Interactor->GetActorLocation().X - GetActorLocation().X);
	const FVector Dest(GetActorLocation().X - Side * 250.f, Interactor->GetActorLocation().Y, Interactor->GetActorLocation().Z + 30.f);
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
	BuildRooster(RoosterRed, TEXT("Red"), FLinearColor(0.75f, 0.1f, 0.05f));
	BuildRooster(RoosterBlue, TEXT("Blue"), FLinearColor(0.1f, 0.25f, 0.85f));
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

#undef LOCTEXT_NAMESPACE
