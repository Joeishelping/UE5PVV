#include "PVVDetentionZone.h"
#include "PVVCharacter.h"
#include "PVVGameMode.h"
#include "Components/BoxComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "PVVDetentionZone"

APVVDetentionZone::APVVDetentionZone()
{
	bReplicates = true;

	DepositArea = CreateDefaultSubobject<UBoxComponent>(TEXT("DepositArea"));
	SetRootComponent(DepositArea);
	DepositArea->SetBoxExtent(FVector(300.f, 300.f, 150.f));
	DepositArea->SetCollisionProfileName(TEXT("Trigger"));
	DepositArea->SetGenerateOverlapEvents(true);
	DepositArea->SetHiddenInGame(false);
	DepositArea->ShapeColor = FColor::Orange;

	DoorArea = CreateDefaultSubobject<UBoxComponent>(TEXT("DoorArea"));
	DoorArea->SetupAttachment(DepositArea);
	DoorArea->SetRelativeLocation(FVector(-500.f, 0.f, 0.f));
	DoorArea->SetBoxExtent(FVector(100.f, 150.f, 150.f));
	DoorArea->SetCollisionProfileName(TEXT("Trigger"));
	DoorArea->SetGenerateOverlapEvents(true);
	DoorArea->SetHiddenInGame(false);
	DoorArea->ShapeColor = FColor::Cyan;

	CellPoint = CreateDefaultSubobject<USceneComponent>(TEXT("CellPoint"));
	CellPoint->SetupAttachment(DepositArea);
	CellPoint->SetRelativeLocation(FVector(-900.f, 0.f, 0.f));

	ReleasePoint = CreateDefaultSubobject<USceneComponent>(TEXT("ReleasePoint"));
	ReleasePoint->SetupAttachment(DepositArea);
	ReleasePoint->SetRelativeLocation(FVector(-500.f, 400.f, 0.f));
}

void APVVDetentionZone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APVVDetentionZone, Detainees);
}

int32 APVVDetentionZone::GetNumDetained() const
{
	int32 Count = 0;
	for (const APVVCharacter* Detainee : Detainees)
	{
		if (Detainee && Detainee->GetCondition() == EPVVCondition::Detained)
		{
			++Count;
		}
	}
	return Count;
}

bool APVVDetentionZone::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	if (!Interactor || Interactor->GetCondition() != EPVVCondition::Normal)
	{
		return false;
	}
	if (Interactor->GetCarriedCharacter())
	{
		return PVV::CanCapture(Interactor->GetFaction()) && DepositArea->IsOverlappingActor(Interactor);
	}
	return !PVV::CanCapture(Interactor->GetFaction()) && GetNumDetained() > 0 && DoorArea->IsOverlappingActor(Interactor);
}

float APVVDetentionZone::GetInteractDuration_Implementation(APVVCharacter* Interactor) const
{
	return (Interactor && Interactor->GetCarriedCharacter()) ? 0.f : JailbreakDuration;
}

FText APVVDetentionZone::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	return (Interactor && Interactor->GetCarriedCharacter())
		? LOCTEXT("Process", "Process detainee")
		: FText::Format(LOCTEXT("Jailbreak", "Break them out! ({0} inside, hold)"), FText::AsNumber(GetNumDetained()));
}

void APVVDetentionZone::OnInteract_Implementation(APVVCharacter* Interactor)
{
	if (!HasAuthority() || !Interactor)
	{
		return;
	}
	APVVGameMode* GM = GetWorld()->GetAuthGameMode<APVVGameMode>();

	if (Interactor->GetCarriedCharacter())
	{
		APVVCharacter* Prisoner = Interactor->TakeCarried();
		if (!Prisoner)
		{
			return;
		}
		Prisoner->Detain(this, Scatter(CellPoint->GetComponentLocation()));
		Detainees.AddUnique(Prisoner);
		OnPrisonerProcessed(Prisoner);
		if (GM)
		{
			GM->NotifyDetained(Prisoner, Interactor);
		}
		return;
	}

	int32 NumFreed = 0;
	for (APVVCharacter* Detainee : Detainees)
	{
		if (Detainee && Detainee->GetCondition() == EPVVCondition::Detained && Detainee->GetDetentionZone() == this)
		{
			Detainee->ReleaseFromDetention(Scatter(ReleasePoint->GetComponentLocation()));
			++NumFreed;
		}
	}
	Detainees.Reset();
	if (NumFreed > 0)
	{
		OnJailbreak(Interactor);
		if (GM)
		{
			GM->NotifyJailbreak(Interactor, NumFreed);
		}
	}
}

FVector APVVDetentionZone::Scatter(const FVector& Center) const
{
	const FVector2D Offset = FMath::RandPointInCircle(ScatterRadius);
	return Center + FVector(Offset.X, Offset.Y, 0.f);
}

#undef LOCTEXT_NAMESPACE
