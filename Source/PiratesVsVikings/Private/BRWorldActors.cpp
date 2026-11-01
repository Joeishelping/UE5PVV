#include "BRWorldActors.h"
#include "BRCharacter.h"
#include "BRGameMode.h"
#include "BRGameState.h"
#include "BRPlayerState.h"
#include "PVVBoat.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
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
	void Tell(APVVCharacter* Character, const FText& Message)
	{
		if (ABRCharacter* BRC = Cast<ABRCharacter>(Character))
		{
			BRC->Notify(Message);
		}
	}
	bool IsFreeRunner(const APVVCharacter* C)
	{
		return C && C->GetFaction() == BR::Runners && C->GetCondition() == EPVVCondition::Normal && !C->GetCarriedCharacter() && !C->GetCurrentBoat();
	}
	bool IsFreeWarden(const APVVCharacter* C)
	{
		return C && C->GetFaction() == BR::Wardens && C->GetCondition() == EPVVCondition::Normal && !C->GetCarriedCharacter() && !C->GetCurrentBoat();
	}
}

// ===========================================================================================
// Greybox base

ABRGreyboxActor::ABRGreyboxActor()
{
	bReplicates = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	CubeMesh = Cube.Object;
	ConeMesh = Cone.Object;
	CylinderMesh = Cylinder.Object;
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
	if (Kind == EBRStationKind::SellItem)
	{
		const ABRPlayerState* PS = BRState(Interactor);
		return PS && PS->HasItem(Item);
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
	switch (Kind)
	{
	case EBRStationKind::BuyItem: return FText::Format(LOCTEXT("Buy", "Buy {0} (${1})"), ItemText, Price);
	case EBRStationKind::SellItem: return FText::Format(LOCTEXT("Sell", "Sell {0} (+${1})"), ItemText, Price);
	case EBRStationKind::BuyBoat: return FText::Format(LOCTEXT("BuyBoat", "Buy a boat (${0}) - delivered to the river"), Price);
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
		Tell(Interactor, FText::Format(LOCTEXT("Bought", "Bought: {0}"), FText::FromString(BR::ItemName(Item))));
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
	return IsFreeRunner(Interactor) && PS && PS->HasItem(EBRItem::WireCutters) && GS && GS->Phase == EBRPhase::Invasion;
}

FText ABRSpikeFence::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	return LOCTEXT("Cut", "Cut the fence (hold - LOUD)");
}

void ABRSpikeFence::OnInteract_Implementation(APVVCharacter* Interactor)
{
	if (!HasAuthority())
	{
		return;
	}
	if (ABRGameState* GS = BRGame(this))
	{
		GS->Multicast_NoisePing(GetActorLocation());
	}
	Destroy();
}

// ===========================================================================================
// Wall

ABRWall::ABRWall()
{
	AddMesh(TEXT("WallMesh"), CubeMesh, FVector(0.f, 0.f, Height * 0.5f), FVector(100.f, HalfLength * 2.f, Height), FLinearColor(0.42f, 0.38f, 0.33f));
	AddLabel(TEXT("SignWest"), FVector(-60.f, 0.f, Height + 60.f), 160.f, FColor(255, 200, 80))->SetText(FText::FromString(TEXT("THE WALL")));
}

bool ABRWall::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	const ABRPlayerState* PS = BRState(Interactor);
	const ABRGameState* GS = BRGame(this);
	return IsFreeRunner(Interactor) && PS && PS->HasItem(EBRItem::Ladder) && GS && GS->Phase == EBRPhase::Invasion
		&& FMath::Abs(Interactor->GetActorLocation().X - GetActorLocation().X) < 300.f;
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
	// One leaning ladder on each side of the wall, meeting at the top.
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

	// Ballistic arc that stays under the character's terminal velocity (4000 cm/s).
	const FVector Delta = Target - Start;
	const float Flat = FVector(Delta.X, Delta.Y, 0.f).Size();
	// Flight time that minimises launch speed (needs Flat < ~160 m to stay under terminal velocity).
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
	// Snap the arm up, then ease it back down.
	const float Age = GetWorld()->GetTimeSeconds() - FlingTime;
	const float Up = Age < 0.15f ? Age / 0.15f : FMath::Clamp(1.f - (Age - 0.15f) / 1.5f, 0.f, 1.f);
	ArmPivot->SetRelativeRotation(FRotator(Up * 110.f, 0.f, 0.f));
}

#undef LOCTEXT_NAMESPACE
