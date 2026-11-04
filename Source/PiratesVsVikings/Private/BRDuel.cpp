#include "BRDuel.h"
#include "BRCharacter.h"
#include "BRGameMode.h"
#include "BRGameState.h"
#include "BRPlayerState.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

#define LOCTEXT_NAMESPACE "BRDuel"

ABRDuelSpot::ABRDuelSpot()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	// A hitching post with a sign, and the two marks scratched in the dirt either side.
	AddMesh(TEXT("Post"), CubeMesh, FVector(0.f, 0.f, 110.f), FVector(22.f, 22.f, 220.f), FLinearColor(0.35f, 0.22f, 0.12f));
	AddMesh(TEXT("Board"), CubeMesh, FVector(0.f, 0.f, 200.f), FVector(12.f, 200.f, 90.f), FLinearColor(0.6f, 0.45f, 0.25f));
	AddMesh(TEXT("MarkA"), CubeMesh, FVector(-750.f, 0.f, 2.f), FVector(20.f, 160.f, 4.f), FLinearColor(0.9f, 0.85f, 0.7f), FRotator::ZeroRotator, false);
	AddMesh(TEXT("MarkB"), CubeMesh, FVector(750.f, 0.f, 2.f), FVector(20.f, 160.f, 4.f), FLinearColor(0.9f, 0.85f, 0.7f), FRotator::ZeroRotator, false);
	UTextRenderComponent* Sign = AddLabel(TEXT("Sign"), FVector(-8.f, 0.f, 230.f), 26.f, FColor(40, 15, 5));
	Sign->SetText(LOCTEXT("DuelSign", "HIGH NOON DUELS\nwin = double your money + $1500\nlose = DEAD + he takes EVERYTHING"));
	Sign->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	AddPickupZone(200.f);
}

bool ABRDuelSpot::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	const ABRCharacter* C = Cast<ABRCharacter>(Interactor);
	return C && C->IsRunner() && !C->IsNPC() && C->GetCondition() == EPVVCondition::Normal && !C->GetCurrentBoat() && State == EState::Idle;
}

FText ABRDuelSpot::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	const ABRCharacter* C = Cast<ABRCharacter>(Interactor);
	const ABRPlayerState* PS = C ? C->GetBRPlayerState() : nullptr;
	if (PS && PS->Money < MinMoney)
	{
		return FText::Format(LOCTEXT("DuelBroke", "The pistolero won't draw on you for less than ${0}."), MinMoney);
	}
	return FText::Format(LOCTEXT("DuelPrompt", "DUEL the pistolero (hold): win = ${0}  |  lose = DEAD and he takes ALL your money + items"),
		PS ? PS->Money * 2 + Prize : Prize);
}

void ABRDuelSpot::Say(const FString& Text, float Height)
{
	ABRGameState::FX(this, EBRFX::PointsText, GetActorLocation() + FVector(0.f, 0.f, Height), Text);
}

void ABRDuelSpot::OnInteract_Implementation(APVVCharacter* Interactor)
{
	ABRCharacter* C = Cast<ABRCharacter>(Interactor);
	ABRPlayerState* PS = C ? C->GetBRPlayerState() : nullptr;
	ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>();
	if (!HasAuthority() || !PS || !GM || State != EState::Idle)
	{
		return;
	}
	if (PS->Money < MinMoney)
	{
		C->Notify(FText::Format(LOCTEXT("DuelBrokeN", "Come back with at least ${0}, amigo."), MinMoney));
		return;
	}
	MarkA = GetActorLocation() + GetActorForwardVector() * -Paces * 0.5f;
	MarkB = GetActorLocation() + GetActorForwardVector() * Paces * 0.5f;
	ABRCharacter* NPC = GM->SpawnNPC(FTransform((MarkA - MarkB).Rotation(), MarkB + FVector(0.f, 0.f, 100.f)));
	if (!NPC)
	{
		return;
	}
	NPC->bSparring = true; // (no trouble, dogs leave him alone)
	NPC->bNoFightBack = true;
	NPC->SetHat(EBRHat::CowboyHat);
	NPC->SetNPCHeldItem(EBRItem::IllegalGun);
	Pistolero = NPC;
	Challenger = C;
	C->Duel = this;
	C->bDuelGun = true;
	C->SetWeaponDrawn(false);
	C->FillRunnerAmmo();
	C->SetActorLocationAndRotation(MarkA + FVector(0.f, 0.f, 100.f), (MarkB - MarkA).Rotation(), false, nullptr, ETeleportType::TeleportPhysics);
	if (AController* PC = C->GetController())
	{
		PC->SetControlRotation((MarkB - MarkA).Rotation());
	}
	State = EState::Countdown;
	StateTime = GetWorld()->GetTimeSeconds();
	CountShown = 0;
	C->Notify(LOCTEXT("DuelStart", "The pistolero lends you a revolver for this duel only. Stand on your mark, hands off it. When it says DRAW: right mouse to aim, left click to shoot. Walk off your mark and you lose."));
	GM->Feed(FString::Printf(TEXT("%s called out the pistolero! HIGH NOON..."), *C->GetPlayerDisplayName()));
}

void ABRDuelSpot::OnShotFired(ABRCharacter* Shooter, APVVCharacter* Victim)
{
	if (!HasAuthority() || Shooter != Challenger.Get())
	{
		return;
	}
	if (State == EState::Countdown || State == EState::Waiting)
	{
		Finish(false, LOCTEXT("DuelFoul", "You drew early! The pistolero shot you down. No honour, no money."));
		return;
	}
	if (State == EState::Draw && Victim && Victim == Pistolero.Get())
	{
		Finish(true, FText::GetEmpty());
	}
}

void ABRDuelSpot::Finish(bool bChallengerWon, const FText& Why)
{
	ABRCharacter* C = Challenger.Get();
	ABRCharacter* NPC = Pistolero.Get();
	ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>();
	State = EState::Over;
	StateTime = GetWorld()->GetTimeSeconds();
	if (C)
	{
		C->Duel = nullptr;
		C->bDuelGun = false;
		C->SetWeaponDrawn(false);
		C->ForceNetUpdate();
	}
	ABRPlayerState* PS = C ? C->GetBRPlayerState() : nullptr;
	if (bChallengerWon)
	{
		++Wins;
		if (NPC && !NPC->IsDead())
		{
			NPC->Die(C, true);
		}
		const int32 Payout = (PS ? PS->Money : 0) + Prize;
		if (PS)
		{
			PS->AddMoney(Payout);
		}
		Say(FString::Printf(TEXT("FASTEST GUN IN TOWN! +$%d"), Payout), 320.f);
		if (C)
		{
			C->Notify(FText::Format(LOCTEXT("DuelWin", "You won the duel! +${0}. The next pistolero will be faster..."), Payout));
		}
		if (GM && C)
		{
			GM->Feed(FString::Printf(TEXT("%s WON a duel and walked off with $%d!"), *C->GetPlayerDisplayName(), Payout));
		}
		return;
	}
	// Lost: everything goes.
	if (PS)
	{
		PS->AddMoney(-PS->Money);
		PS->Inventory.Reset();
		PS->ForceNetUpdate();
	}
	if (C)
	{
		C->Notify(Why);
		if (NPC)
		{
			NPC->DuelShoot(C, true);
		}
		if (!C->IsDead())
		{
			C->Die(NPC, true);
		}
		if (GM)
		{
			GM->Feed(FString::Printf(TEXT("%s lost a duel - and everything they had."), *C->GetPlayerDisplayName()));
		}
	}
	if (NPC)
	{
		NPC->SetLifeSpan(6.f); // tips his hat and walks off
	}
}

void ABRDuelSpot::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || State == EState::Idle)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (State == EState::Over)
	{
		if (Now - StateTime > 6.f)
		{
			State = EState::Idle;
		}
		return;
	}
	ABRCharacter* C = Challenger.Get();
	ABRCharacter* NPC = Pistolero.Get();
	if (!C || C->IsDead() || C->GetCondition() != EPVVCondition::Normal || !NPC || NPC->IsDead())
	{
		if (C && !C->IsDead() && NPC && !NPC->IsDead())
		{
			Finish(false, LOCTEXT("DuelDown", "You went down mid-duel. He doesn't miss twice."));
		}
		else if (NPC && NPC->IsDead() && C)
		{
			Finish(true, FText::GetEmpty());
		}
		else
		{
			State = EState::Over;
			StateTime = Now;
			if (C)
			{
				C->Duel = nullptr;
				C->bDuelGun = false;
			}
		}
		return;
	}
	// Keep the pistolero on his mark, facing you.
	NPC->SetActorRotation((C->GetActorLocation() - NPC->GetActorLocation()).GetSafeNormal2D().Rotation());
	// Walk away before the shooting starts = coward.
	if (State != EState::Draw && FVector::Dist2D(C->GetActorLocation(), MarkA) > 260.f)
	{
		ABRPlayerState* PS = C->GetBRPlayerState();
		if (PS)
		{
			PS->AddMoney(-PS->Money);
		}
		C->Duel = nullptr;
		C->bDuelGun = false;
		C->SetWeaponDrawn(false);
		C->Notify(LOCTEXT("DuelCoward", "COWARD! You walked off your mark. The pistolero takes your money and laughs."));
		Say(TEXT("COBARDE!"), 300.f);
		NPC->SetLifeSpan(4.f);
		State = EState::Over;
		StateTime = Now;
		return;
	}
	const float T = Now - StateTime;
	if (State == EState::Countdown)
	{
		const int32 Count = 3 - FMath::FloorToInt(T);
		if (Count >= 1 && Count != CountShown)
		{
			CountShown = Count;
			Say(FString::Printf(TEXT("%d..."), Count));
		}
		if (T >= 3.f)
		{
			State = EState::Waiting;
			StateTime = Now;
			DrawAt = Now + FMath::FRandRange(0.8f, 3.5f); // the tumbleweed moment
		}
		return;
	}
	if (State == EState::Waiting)
	{
		if (Now >= DrawAt)
		{
			State = EState::Draw;
			StateTime = Now;
			Say(TEXT("DRAW!"), 300.f);
			ABRGameState::Sfx(this, EBRSfx::Whistle, GetActorLocation());
			C->SetWeaponDrawn(true); // the loaner revolver comes out - aim and fire!
			C->ForceNetUpdate();
			C->Notify(LOCTEXT("DuelDraw", "DRAW!!! Right mouse to aim, LEFT CLICK to fire!"));
			const float Faster = FMath::Pow(0.9f, static_cast<float>(FMath::Min(Wins, 5)));
			NextNPCShot = Now + FMath::FRandRange(DrawTime.X, DrawTime.Y) * Faster;
		}
		return;
	}
	// DRAW: the pistolero fires when his hand gets there.
	if (Now >= NextNPCShot)
	{
		const bool bHit = FMath::FRand() < HitChance;
		NPC->DuelShoot(C, bHit);
		if (bHit)
		{
			Finish(false, LOCTEXT("DuelLose", "Too slow. The pistolero shot you dead - and took ALL your money and everything in your pockets."));
			return;
		}
		Say(TEXT("*MISS*"), 220.f);
		NextNPCShot = Now + 0.6f;
	}
}

// ===========================================================================================
// Character side

void ABRCharacter::DuelShoot(ABRCharacter* Target, bool bHit)
{
	if (!HasAuthority() || !Target)
	{
		return;
	}
	const FVector Muzzle = GetActorLocation() + GetActorForwardVector() * 50.f + FVector(0.f, 0.f, 40.f);
	const FVector Aim = Target->GetActorLocation() + (bHit ? FVector(0.f, 0.f, 30.f) : FVector(FMath::FRandRange(-120.f, 120.f), FMath::FRandRange(-120.f, 120.f), 90.f));
	Multicast_Shot(Muzzle, Aim, true, bHit);
}

#undef LOCTEXT_NAMESPACE
