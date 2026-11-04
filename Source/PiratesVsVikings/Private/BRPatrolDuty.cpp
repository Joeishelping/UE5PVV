#include "BRPatrolDuty.h"
#include "BRCharacter.h"
#include "BRGameMode.h"
#include "BRGameState.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "BRPatrolDuty"

namespace
{
	/** Full pay in prep, half once the Line is open (you should be out catching people). */
	int32 DutyPay(const UObject* Context, int32 Amount)
	{
		const ABRGameState* GS = Context->GetWorld()->GetGameState<ABRGameState>();
		return (GS && GS->Phase == EBRPhase::Prep) ? Amount : Amount / 2;
	}

	void PayBudget(const UObject* Context, int32 Amount, const FVector& Where)
	{
		if (Amount <= 0)
		{
			return;
		}
		if (ABRGameMode* GM = Context->GetWorld()->GetAuthGameMode<ABRGameMode>())
		{
			GM->AddBudget(Amount);
		}
		ABRGameState::FX(Context, EBRFX::MoneyText, Where, FString::Printf(TEXT("+$%d BUDGET"), Amount));
	}
}

// ===========================================================================================
// Range target

ABRRangeTarget::ABRRangeTarget()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	const FLinearColor Wood(0.45f, 0.32f, 0.18f);
	AddMesh(TEXT("Stand"), CubeMesh, FVector(0.f, 0.f, 50.f), FVector(30.f, 90.f, 100.f), Wood);
	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	Pivot->SetupAttachment(Root);
	Pivot->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
	// The board faces +X (point the actor's front at the shooters).
	UStaticMeshComponent* Board = AddMesh(TEXT("Board"), CylinderMesh, FVector(0.f, 0.f, 70.f), FVector(110.f, 110.f, 6.f), FLinearColor(0.95f, 0.95f, 0.9f), FRotator(90.f, 0.f, 0.f));
	Board->SetupAttachment(Pivot);
	Board->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
	UStaticMeshComponent* Ring = AddMesh(TEXT("Ring"), CylinderMesh, FVector(4.f, 0.f, 70.f), FVector(70.f, 70.f, 6.f), FLinearColor(0.1f, 0.1f, 0.12f), FRotator(90.f, 0.f, 0.f));
	Ring->SetupAttachment(Pivot);
	Ring->SetRelativeLocation(FVector(4.f, 0.f, 70.f));
	Bull = AddMesh(TEXT("Bull"), CylinderMesh, FVector(8.f, 0.f, 70.f), FVector(28.f, 28.f, 6.f), FLinearColor(0.9f, 0.1f, 0.08f), FRotator(90.f, 0.f, 0.f));
	Bull->SetupAttachment(Pivot);
	Bull->SetRelativeLocation(FVector(8.f, 0.f, 70.f));
}

void ABRRangeTarget::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRRangeTarget, bUp);
}

void ABRRangeTarget::OnShot(ABRCharacter* By, const FVector& Point)
{
	if (!HasAuthority() || !bUp || !By)
	{
		return;
	}
	bUp = false;
	UpAt = GetWorld()->GetTimeSeconds() + FMath::FRandRange(DownSeconds.X, DownSeconds.Y);
	ForceNetUpdate();
	const bool bBullseye = FVector::Dist(Point, Bull->GetComponentLocation()) < 20.f;
	ABRGameState::FX(this, EBRFX::Sparks, Point);
	if (!By->IsWarden())
	{
		return; // only Border Patrol training pays
	}
	const int32 Pay = DutyPay(this, bBullseye ? BullseyeBudget : HitBudget);
	PayBudget(this, Pay, Point + FVector(0.f, 0.f, 50.f));
	if (bBullseye)
	{
		ABRGameState::FX(this, EBRFX::PointsText, Point + FVector(0.f, 0.f, 100.f), TEXT("BULLSEYE!"));
	}
}

void ABRRangeTarget::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority() && !bUp && GetWorld()->GetTimeSeconds() >= UpAt)
	{
		bUp = true;
		ForceNetUpdate();
	}
	// Knocked flat backwards when hit, springs back up.
	Angle = FMath::FInterpTo(Angle, bUp ? 0.f : -85.f, DeltaSeconds, bUp ? 5.f : 14.f);
	Pivot->SetRelativeRotation(FRotator(Angle, 0.f, 0.f));
}

// ===========================================================================================
// Sparring mat

ABRSparringMat::ABRSparringMat()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	const float Half = 380.f;
	AddMesh(TEXT("Mat"), CubeMesh, FVector(0.f, 0.f, 3.f), FVector(Half * 2.f, Half * 2.f, 6.f), FLinearColor(0.15f, 0.3f, 0.6f));
	const FLinearColor PostCol(0.85f, 0.85f, 0.85f);
	const FLinearColor RopeCol(0.8f, 0.1f, 0.1f);
	int32 Index = 0;
	for (const FVector2D Corner : { FVector2D(1, 1), FVector2D(1, -1), FVector2D(-1, 1), FVector2D(-1, -1) })
	{
		AddMesh(*FString::Printf(TEXT("Post%d"), Index++), CylinderMesh, FVector(Corner.X * Half, Corner.Y * Half, 60.f), FVector(14.f, 14.f, 120.f), PostCol);
	}
	// Ropes are just for looks (no collision: you get knocked straight off the mat, Gang Beasts style).
	AddMesh(TEXT("RopeN"), CubeMesh, FVector(0.f, Half, 100.f), FVector(Half * 2.f, 4.f, 4.f), RopeCol, FRotator::ZeroRotator, false);
	AddMesh(TEXT("RopeS"), CubeMesh, FVector(0.f, -Half, 100.f), FVector(Half * 2.f, 4.f, 4.f), RopeCol, FRotator::ZeroRotator, false);
	AddMesh(TEXT("RopeE"), CubeMesh, FVector(Half, 0.f, 100.f), FVector(4.f, Half * 2.f, 4.f), RopeCol, FRotator::ZeroRotator, false);
	AddMesh(TEXT("RopeW"), CubeMesh, FVector(-Half, 0.f, 100.f), FVector(4.f, Half * 2.f, 4.f), RopeCol, FRotator::ZeroRotator, false);
	UTextRenderComponent* Sign = AddLabel(TEXT("Sign"), FVector(-Half - 10.f, 0.f, 230.f), 60.f, FColor(120, 190, 255));
	Sign->SetText(LOCTEXT("SparSign", "SPARRING - E: fight a recruit"));
	Sign->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	AddPickupZone(Half + 80.f);
}

void ABRSparringMat::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRSparringMat, Streak);
	DOREPLIFETIME(ABRSparringMat, bBusy);
}

int32 ABRSparringMat::Reward() const
{
	return FMath::Min(BaseBudget + StepBudget * Streak, MaxBudget);
}

bool ABRSparringMat::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	const ABRCharacter* C = Cast<ABRCharacter>(Interactor);
	return C && C->IsWarden() && !C->IsNPC() && C->GetCondition() == EPVVCondition::Normal && !C->GetCurrentBoat() && !bBusy;
}

FText ABRSparringMat::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	return FText::Format(LOCTEXT("SparPrompt", "Spar with recruit #{0} - knock him out: +${1} budget (half once the Line opens)"), Streak + 1, Reward());
}

void ABRSparringMat::OnInteract_Implementation(APVVCharacter* Interactor)
{
	ABRCharacter* C = Cast<ABRCharacter>(Interactor);
	ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>();
	const float Now = GetWorld()->GetTimeSeconds();
	if (!HasAuthority() || !C || !GM || bBusy || Now < NextBout)
	{
		return;
	}
	// The recruit hops in on the far side of the mat, facing you.
	FVector Side = (GetActorLocation() - C->GetActorLocation()) * FVector(1.f, 1.f, 0.f);
	Side = Side.IsNearlyZero() ? GetActorForwardVector() : Side.GetSafeNormal();
	const FVector Spot = GetActorLocation() + Side * 220.f + FVector(0.f, 0.f, 110.f);
	ABRCharacter* NPC = GM->SpawnNPC(FTransform((-Side).Rotation(), Spot));
	if (!NPC)
	{
		return;
	}
	NPC->MakeSparringPartner(C, Streak);
	Recruit = NPC;
	Fighter = C;
	BoutStart = Now;
	bBusy = true;
	ForceNetUpdate();
	C->Notify(FText::Format(LOCTEXT("SparStart", "FIGHT! Recruit #{0} wants a piece of you. Knock him out for +${1} budget."), Streak + 1, DutyPay(this, Reward())));
	GM->Feed(FString::Printf(TEXT("%s is sparring with a recruit at the base"), *C->GetPlayerDisplayName()));
}

void ABRSparringMat::EndBout(bool bWon, const FText& Why)
{
	ABRCharacter* NPC = Recruit.Get();
	ABRCharacter* C = Fighter.Get();
	if (bWon)
	{
		const int32 Pay = DutyPay(this, Reward());
		PayBudget(this, Pay, (NPC ? NPC->GetActorLocation() : GetActorLocation()) + FVector(0.f, 0.f, 120.f));
		++Streak;
		if (C)
		{
			C->Notify(FText::Format(LOCTEXT("SparWin", "KNOCKOUT! +${0} budget. The next recruit is tougher (+${1})."), Pay, DutyPay(this, Reward())));
		}
	}
	else
	{
		Streak = 0;
		if (C)
		{
			C->Notify(Why);
		}
	}
	if (NPC)
	{
		NPC->CalmDown();
		NPC->bSparring = false;
		NPC->SetLifeSpan(4.f); // limps off the mat
	}
	Recruit = nullptr;
	Fighter = nullptr;
	bBusy = false;
	NextBout = GetWorld()->GetTimeSeconds() + 3.f;
	ForceNetUpdate();
}

void ABRSparringMat::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || !bBusy)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	ABRCharacter* NPC = Recruit.Get();
	ABRCharacter* C = Fighter.Get();
	if (!NPC || NPC->IsDead())
	{
		EndBout(false, LOCTEXT("SparDead", "That's not sparring. No budget for that."));
		return;
	}
	if (NPC->GetCondition() == EPVVCondition::KnockedDown || NPC->IsFleeing())
	{
		EndBout(true, FText::GetEmpty());
		return;
	}
	if (!C || C->GetCondition() == EPVVCondition::KnockedDown)
	{
		EndBout(false, LOCTEXT("SparLose", "The recruit knocked YOU out. Shake it off and try again (the streak resets)."));
		return;
	}
	if (C->GetCondition() != EPVVCondition::Normal || FVector::Dist2D(C->GetActorLocation(), GetActorLocation()) > 1800.f)
	{
		EndBout(false, LOCTEXT("SparLeft", "You walked away from the fight. The recruit wins."));
		return;
	}
	if (Now - BoutStart > BoutSeconds)
	{
		EndBout(false, LOCTEXT("SparTime", "Time! The recruit taps out - no knockout, no budget."));
		return;
	}
	// Keep the recruit mad at you for the whole bout.
	if (!NPC->IsBrawling() || NPC->GetBrawlTarget() != C)
	{
		NPC->StartBrawling(C, BoutSeconds, 0.85f);
	}
}

// ===========================================================================================
// Character side

void ABRCharacter::MakeSparringPartner(ABRCharacter* Opponent, int32 Level)
{
	if (!HasAuthority() || !bIsNPC)
	{
		return;
	}
	bSparring = true;
	bIllegal = false;
	bCarriesContraband = false;
	bPeaceful = false;
	bNoFightBack = false;
	FightStyle = FMath::RandRange(0, 1); // a brawler or a grappler - never a coward
	FightToughness *= 1.f + 0.2f * FMath::Min(Level, 6);
	SetHat(EBRHat::PatrolCap);
	// Dressed like Border Patrol (a cadet in uniform), not a local.
	SetNPCPoncho(false);
	FBRLook Uniform = Look;
	Uniform.ShirtStyle = 0;
	Uniform.PantsStyle = 0;
	Uniform.ShirtColor = 0.36f; // olive green
	Uniform.PantsColor = 0.62f; // dark navy
	Uniform.Glasses = 0;
	SetLook(Uniform);
	StartBrawling(Opponent, 90.f, FMath::Min(0.7f + 0.05f * Level, 0.95f));
}

#undef LOCTEXT_NAMESPACE
