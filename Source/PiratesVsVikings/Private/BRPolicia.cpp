#include "BRPolicia.h"
#include "BRCharacter.h"
#include "BRDesertArena.h"
#include "BRGameMode.h"
#include "BRGameState.h"
#include "BRPlayerState.h"
#include "PVVBoat.h"
#include "Net/UnrealNetwork.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"

#define LOCTEXT_NAMESPACE "BRPolicia"

// ===========================================================================================
// Cárcel

ABRMexJail::ABRMexJail()
{
	const FLinearColor Stone(0.55f, 0.5f, 0.45f);
	const FLinearColor Iron(0.12f, 0.12f, 0.13f);
	const float X = CellHalfX, Y = CellHalfY, H = CellHeight;
	AddMesh(TEXT("Floor"), CubeMesh, FVector(0.f, 0.f, 3.f), FVector(X * 2.f, Y * 2.f, 6.f), FLinearColor(0.35f, 0.33f, 0.3f));
	AddMesh(TEXT("Back"), CubeMesh, FVector(-X, 0.f, H * 0.5f), FVector(30.f, Y * 2.f + 30.f, H), Stone);
	AddMesh(TEXT("Left"), CubeMesh, FVector(0.f, -Y, H * 0.5f), FVector(X * 2.f, 30.f, H), Stone);
	AddMesh(TEXT("Right"), CubeMesh, FVector(0.f, Y, H * 0.5f), FVector(X * 2.f, 30.f, H), Stone);
	AddMesh(TEXT("Roof"), CubeMesh, FVector(0.f, 0.f, H + 10.f), FVector(X * 2.f + 40.f, Y * 2.f + 40.f, 20.f), Stone);
	// The front: iron bars you can see through (an invisible wall behind them does the stopping).
	for (int32 i = 0; i < 11; ++i)
	{
		AddMesh(*FString::Printf(TEXT("Bar%d"), i), CylinderMesh, FVector(X, -Y + 25.f + i * (Y * 2.f - 50.f) / 10.f, H * 0.5f), FVector(7.f, 7.f, H), Iron, FRotator::ZeroRotator, false);
	}
	UStaticMeshComponent* Front = AddMesh(TEXT("FrontBlock"), CubeMesh, FVector(X, 0.f, H * 0.5f), FVector(12.f, Y * 2.f, H), Iron);
	Front->SetVisibility(false);
	Front->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Front->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	UTextRenderComponent* Sign = AddLabel(TEXT("Sign"), FVector(X + 20.f, 0.f, H + 60.f), 90.f, FColor(255, 230, 120));
	Sign->SetText(FText::FromString(TEXT("CARCEL - POLICIA")));
	Tags.Add(TEXT("NoGrab")); // no climbing out over the roof
	// The station's evidence table, outside the cell (everything taken off arrested people ends up here).
	const FVector Table(X + 260.f, -Y - 60.f, 0.f);
	AddMesh(TEXT("EvTable"), CubeMesh, Table + FVector(0.f, 0.f, 45.f), FVector(160.f, 90.f, 10.f), FLinearColor(0.4f, 0.27f, 0.14f));
	for (int32 i = 0; i < 4; ++i)
	{
		AddMesh(*FString::Printf(TEXT("EvLeg%d"), i), CubeMesh, Table + FVector((i % 2 ? 70.f : -70.f), (i / 2 ? 38.f : -38.f), 22.f), FVector(8.f, 8.f, 44.f), FLinearColor(0.3f, 0.2f, 0.1f), FRotator::ZeroRotator, false);
	}
	EvidenceLabel = AddLabel(TEXT("EvSign"), Table + FVector(0.f, 0.f, 120.f), 34.f, FColor(255, 255, 255));
	EvidenceLabel->SetRelativeRotation(FRotator(0.f, 0.f, 0.f));
	TablePos = Table;
	bReplicates = true;
	AddPickupZone(CellHalfX + 400.f);
}

void ABRMexJail::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRMexJail, Evidence);
}

void ABRMexJail::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Evidence();
}

void ABRMexJail::OnRep_Evidence()
{
	if (EvidenceLabel)
	{
		EvidenceLabel->SetText(FText::FromString(FString::Printf(TEXT("EVIDENCE (%d)"), Evidence.Num())));
	}
}

void ABRMexJail::AddEvidence(EBRItem Item)
{
	if (HasAuthority() && Item != EBRItem::None)
	{
		Evidence.Add(Item);
		if (Evidence.Num() > 24)
		{
			Evidence.RemoveAt(0); // (old stuff gets thrown out)
		}
		OnRep_Evidence();
		ForceNetUpdate();
	}
}

bool ABRMexJail::NearTable(const APVVCharacter* Who) const
{
	return Who && FVector::Dist2D(Who->GetActorLocation(), GetActorTransform().TransformPosition(TablePos)) < 230.f;
}

bool ABRMexJail::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	const ABRCharacter* C = Cast<ABRCharacter>(Interactor);
	return C && !C->IsNPC() && C->GetCondition() == EPVVCondition::Normal && C->MexJailEnd < 0.f && NearTable(C) && Evidence.Num() > 0;
}

FText ABRMexJail::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	return FText::Format(LOCTEXT("StealEvidence", "STEAL from the evidence table ({0} things) - a crime if a cop sees you"), Evidence.Num());
}

void ABRMexJail::OnInteract_Implementation(APVVCharacter* Interactor)
{
	ABRCharacter* C = Cast<ABRCharacter>(Interactor);
	ABRPlayerState* PS = C ? C->GetBRPlayerState() : nullptr;
	if (!HasAuthority() || !PS || Evidence.Num() == 0)
	{
		return;
	}
	C->CommitCrimeInTown(35.f); // stealing from the Policía, right under their noses
	int32 Took = 0;
	while (Evidence.Num() > 0 && PS->HasFreeSlot())
	{
		PS->AddItem(Evidence.Pop());
		++Took;
	}
	OnRep_Evidence();
	ForceNetUpdate();
	C->Notify(Took > 0 ? FText::Format(LOCTEXT("TookEvidence", "You grabbed {0} thing(s) off the evidence table."), Took)
		: LOCTEXT("PocketsFull", "Your pockets are full."));
}

FVector ABRMexJail::GetCellSpot() const
{
	return GetActorTransform().TransformPosition(FVector(FMath::FRandRange(-150.f, 150.f), FMath::FRandRange(-130.f, 130.f), 110.f));
}

FVector ABRMexJail::GetReleaseSpot() const
{
	return GetActorTransform().TransformPosition(FVector(CellHalfX + 220.f, 0.f, 110.f));
}

bool ABRMexJail::IsInCell(const FVector& WorldPoint) const
{
	const FVector L = GetActorTransform().InverseTransformPosition(WorldPoint);
	return FMath::Abs(L.X) < CellHalfX && FMath::Abs(L.Y) < CellHalfY && L.Z > -50.f && L.Z < CellHeight;
}

ABRMexJail* ABRMexJail::Find(const UWorld* World)
{
	for (TActorIterator<ABRMexJail> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

// ===========================================================================================
// Character side

namespace
{
	const ABRDesertArena* TownArena(const UWorld* World)
	{
		for (TActorIterator<ABRDesertArena> It(World); It; ++It)
		{
			if (It->bBuildTestMap)
			{
				return *It;
			}
		}
		return nullptr;
	}
}

int32 ABRCharacter::PoliciaStars() const
{
	return PoliciaHeat >= 160.f ? 3 : (PoliciaHeat >= 100.f ? 2 : (PoliciaHeat >= PoliciaWantedHeat ? 1 : 0));
}

bool ABRCharacter::IsMexicanCivilianVictim() const
{
	// Townspeople and the Policía - not Border Patrol, not their officers or cadets, not border rushers.
	if (!bIsNPC || bOfficer || bSparring || bHordeRunner || IsWarden())
	{
		return false;
	}
	const ABRDesertArena* Arena = TownArena(GetWorld());
	return Arena && Arena->IsInTown(GetActorLocation());
}

bool ABRCharacter::IsCrimeWitnessed() const
{
	// A Policía officer has to SEE it (in range, nothing in the way) - otherwise nobody ever knows.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BRWitness), false, this);
	const FVector Me = GetActorLocation() + FVector(0.f, 0.f, 50.f);
	for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
	{
		const ABRCharacter* W = *It;
		if (W == this || !W->IsNPC() || W->GetCondition() != EPVVCondition::Normal || !W->bPolicia)
		{
			continue;
		}
		const float Range = 3500.f;
		const FVector Eye = W->GetActorLocation() + FVector(0.f, 0.f, 60.f);
		if (FVector::DistSquared(Eye, Me) > Range * Range)
		{
			continue;
		}
		FCollisionQueryParams Sight = Params;
		Sight.AddIgnoredActor(W);
		FHitResult Block;
		if (!GetWorld()->LineTraceSingleByChannel(Block, Eye, Me, ECC_Visibility, Sight) || Block.GetActor() == this)
		{
			return true;
		}
	}
	return false;
}

void ABRCharacter::ReportCrimeAgainst(ABRCharacter* Victim, float Heat)
{
	// Only crimes against MEXICO count here (Border Patrol wanted levels are a separate thing the Policía ignore):
	// a townsperson / Policía in town - or, for a troublemaking local, anybody (except Border Patrol) they jump in town.
	if (!HasAuthority() || !Victim || Victim == this || bPolicia)
	{
		return;
	}
	const ABRDesertArena* Arena = TownArena(GetWorld());
	const bool bPlayerVictim = bIsNPC && !Victim->IsNPC() && !Victim->IsWarden() && Arena && Arena->IsInTown(Victim->GetActorLocation());
	if (!Victim->IsMexicanCivilianVictim() && !bPlayerVictim)
	{
		return;
	}
	// Getting your stuff back off the mugger who robbed you (or sorting out someone the Policía already want) isn't a crime.
	if ((Victim->StolenItem != EBRItem::None && Victim->StolenFrom.Get() == GetPlayerState()) || Victim->MugTarget.Get() == this || Victim->bWantedByPolicia)
	{
		return;
	}
	// NPCs: only the troublemakers (or a mugger) - people defending themselves aren't criminals.
	if (bIsNPC && !bTroublemaker && !MugTarget.IsValid())
	{
		return;
	}
	if (Victim->bPolicia)
	{
		Heat *= 1.6f; // hitting a cop: big trouble
	}
	CommitCrimeInTown(Heat);
}

void ABRCharacter::CommitCrimeInTown(float Heat)
{
	if (!HasAuthority() || bPolicia || !IsCrimeWitnessed())
	{
		return; // no cop saw a thing...
	}
	const bool bWasWanted = PoliciaHeat >= PoliciaWantedHeat;
	PoliciaHeat = FMath::Min(200.f, PoliciaHeat + Heat);
	LastCrimeTime = GetWorld()->GetTimeSeconds();
	bWantedByPolicia = PoliciaHeat >= PoliciaWantedHeat;
	PoliciaStarsRep = static_cast<uint8>(PoliciaStars());
	if (bWantedByPolicia && !bWasWanted)
	{
		Notify(LOCTEXT("PoliciaWanted", "POLICIA! A cop saw what you did - the Mexican police are coming for you. Get out of town, or hide, until they forget."));
		if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
		{
			GM->Feed(FString::Printf(TEXT("The Policia want %s for crimes in town"), *GetPlayerDisplayName()));
		}
	}
	ForceNetUpdate();
}

void ABRCharacter::TickPoliciaHeat(float DeltaSeconds)
{
	if (!HasAuthority())
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	// Laying low (30 s with no new crimes) - the heat slowly dies down. Faster outside of town.
	if (PoliciaHeat > 0.f && Now - LastCrimeTime > 30.f)
	{
		const ABRDesertArena* Arena = TownArena(GetWorld());
		const bool bOutOfTown = !Arena || !Arena->IsInTown(GetActorLocation());
		PoliciaHeat = FMath::Max(0.f, PoliciaHeat - DeltaSeconds * (bOutOfTown ? 6.f : 2.f));
		if (PoliciaStarsRep != PoliciaStars())
		{
			PoliciaStarsRep = static_cast<uint8>(PoliciaStars());
			ForceNetUpdate();
		}
		const bool bWanted = PoliciaHeat >= PoliciaWantedHeat;
		if (bWanted != bWantedByPolicia)
		{
			bWantedByPolicia = bWanted;
			if (!bWanted)
			{
				Notify(LOCTEXT("PoliciaForgot", "The Policia have stopped looking for you."));
			}
			ForceNetUpdate();
		}
	}
	// In the cárcel: stay in the cell until your time's up, then out the door.
	if (MexJailEnd > 0.f)
	{
		const ABRGameState* GS = GetBRGameState();
		const float ServerNow = GS ? GS->GetServerWorldTimeSeconds() : Now;
		ABRMexJail* Jail = ABRMexJail::Find(GetWorld());
		if (ServerNow >= MexJailEnd || !Jail)
		{
			MexJailEnd = -1.f;
			if (Jail)
			{
				TeleportTo(Jail->GetReleaseSpot(), GetActorRotation(), false, true);
			}
			Notify(LOCTEXT("JailOut", "You're out of the carcel. Behave yourself in town."));
			ForceNetUpdate();
		}
		else if (!Jail->IsInCell(GetActorLocation()) && GetCondition() == EPVVCondition::Normal && !GetCarrier())
		{
			TeleportTo(Jail->GetCellSpot(), GetActorRotation(), false, true); // nice try
		}
	}
}

bool ABRCharacter::TickPoliciaThink()
{
	// Policía: patrol town; anyone wanted for crimes against Mexico gets chased down, beaten, then DRAGGED to the cárcel.
	const float Now = GetWorld()->GetTimeSeconds();
	ABRMexJail* Jail = ABRMexJail::Find(GetWorld());
	// Dragging someone to the cells: walk them there (keep them down), then in they go.
	if (ABRCharacter* Prisoner = PoliciaPrisoner.Get())
	{
		const bool bStillHeld = HandGrabTarget.Get() == Prisoner && Prisoner->GetCondition() == EPVVCondition::KnockedDown;
		if (!Jail || !bStillHeld || Prisoner->IsDead())
		{
			PoliciaPrisoner = nullptr; // they got away (or something went wrong): back to chasing
			ArmsReaching = 0;
			return false;
		}
		if (Now >= NextPoliciaThink)
		{
			NextPoliciaThink = Now + 0.5f;
			Prisoner->ExtendKnockdown(0.6f); // (a cop's knee in your back)
		}
		const FVector ToJail = (Jail->GetReleaseSpot() - GetActorLocation()) * FVector(1.f, 1.f, 0.f);
		if (ToJail.Size() < 300.f)
		{
			ReleaseHandGrab();
			ArmsReaching = 0;
			PoliciaPrisoner = nullptr;
			Prisoner->SendToMexJail(this);
			CalmDown();
			return false;
		}
		GetCharacterMovement()->MaxWalkSpeed = WanderSpeed * 1.3f;
		AddMovementInput(NPCSteer(ToJail.GetSafeNormal()), 1.f);
		return true;
	}
	if (Now < NextPoliciaThink)
	{
		return false;
	}
	NextPoliciaThink = Now + 0.5f;
	ABRCharacter* Target = nullptr;
	float Best = 6000.f;
	for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
	{
		ABRCharacter* C = *It;
		if (C == this || !C->bWantedByPolicia || C->MexJailEnd > 0.f || C->IsDead() || C->GetCurrentBoat() || C->IsHidingInSpot() || C->GetCarrier()
			|| C->GetCondition() == EPVVCondition::Restrained || C->GetCondition() == EPVVCondition::Detained)
		{
			continue;
		}
		const float D = FVector::Dist(C->GetActorLocation(), GetActorLocation());
		if (D < Best)
		{
			Best = D;
			Target = C;
		}
	}
	if (!Target)
	{
		return false;
	}
	// Down? Grab them and drag them off to the cells.
	if (Target->GetCondition() == EPVVCondition::KnockedDown && Best < 260.f)
	{
		CalmDown();
		ArmsReaching = 3;
		if (TryHandGrab(3) && HandGrabTarget.Get() == Target)
		{
			PoliciaPrisoner = Target;
			Target->ExtendKnockdown(1.f);
			Target->Notify(LOCTEXT("Dragged", "The Policia are DRAGGING you to the carcel! (mash Space when you come round)"));
			ABRGameState::FX(this, EBRFX::PointsText, GetActorLocation() + FVector(0.f, 0.f, 140.f), TEXT("A LA CARCEL!"));
			return true;
		}
		ArmsReaching = 0;
		Target->SendToMexJail(this); // (couldn't get a grip: they're carted off anyway)
		return false;
	}
	if (!IsBrawling() || GetBrawlTarget() != Target)
	{
		StartBrawling(Target, 30.f, 0.9f);
		if (!Target->IsNPC() && Now - Target->LastPoliciaShout > 8.f)
		{
			Target->LastPoliciaShout = Now;
			ABRGameState::FX(this, EBRFX::PointsText, GetActorLocation() + FVector(0.f, 0.f, 140.f), TEXT("ALTO! POLICIA!"));
			ABRGameState::Sfx(this, EBRSfx::Whistle, GetActorLocation());
		}
	}
	return false;
}

void ABRCharacter::SendToMexJail(ABRCharacter* Cop)
{
	ABRMexJail* Jail = ABRMexJail::Find(GetWorld());
	if (!HasAuthority() || !Jail)
	{
		return;
	}
	if (APVVCharacter* Carrier = GetCarrier())
	{
		Carrier->DropCarried();
	}
	if (GetCurrentBoat())
	{
		GetCurrentBoat()->Unseat(this);
	}
	ReleaseHandGrab();
	if (GetCondition() == EPVVCondition::KnockedDown)
	{
		Recover(0.6f);
	}
	const ABRGameState* GS = GetBRGameState();
	const float ServerNow = GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	const int32 Stars = PoliciaStars();
	const float Seconds = Stars >= 3 ? 90.f : (Stars == 2 ? 60.f : 30.f); // 1 star 30 s, 2 stars 1 min, 3 stars 1.5 min
	MexJailEnd = ServerNow + Seconds;
	PoliciaHeat = 0.f;
	PoliciaStarsRep = 0;
	bWantedByPolicia = false;
	TeleportTo(Jail->GetCellSpot(), GetActorRotation(), false, true);
	if (bIsNPC)
	{
		CalmDown();
		MugTarget = nullptr;
	}
	// Everything in your pockets goes on the evidence table at the station.
	int32 Seized = 0;
	if (ABRPlayerState* PS = GetBRPlayerState())
	{
		for (const EBRItem Item : PS->Inventory)
		{
			if (Item != EBRItem::None)
			{
				Jail->AddEvidence(Item);
				++Seized;
			}
		}
		PS->Inventory.Reset();
		PS->ForceNetUpdate();
	}
	if (StolenItem != EBRItem::None)
	{
		Jail->AddEvidence(StolenItem);
		StolenItem = EBRItem::None;
		++Seized;
	}
	if (bIsNPC && HeldItem != EBRItem::None)
	{
		SetNPCHeldItem(EBRItem::None);
	}
	int32 Fine = 0;
	if (ABRPlayerState* PS = GetBRPlayerState())
	{
		Fine = FMath::Min(PS->Money, 300);
		PS->AddMoney(-Fine);
	}
	if (Seized > 0)
	{
		Notify(FText::Format(LOCTEXT("EvidenceTaken", "The Policia took {0} thing(s) off you - they're on the evidence table at the station."), Seized));
	}
	Notify(FText::Format(LOCTEXT("Jailed", "ARRESTED by the Policia: {0} s in the carcel and a ${1} fine."), FMath::RoundToInt(Seconds), Fine));
	if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
	{
		GM->Feed(FString::Printf(TEXT("The Policia threw %s in the carcel"), *GetPlayerDisplayName()));
	}
	ForceNetUpdate();
}

void ABRCharacter::MakePolicia()
{
	if (!HasAuthority() || !bIsNPC)
	{
		return;
	}
	bPolicia = true;
	bVillager = false;
	bIllegal = false;
	bCarriesContraband = false;
	bPeaceful = false;
	bNoFightBack = false;
	FightStyle = FMath::RandRange(0, 1);
	FightToughness *= 1.7f;
	SetNPCPoncho(false);
	SetHat(EBRHat::PatrolCap);
	UniformOverride = 1; // the Policia uniform (only they wear it)
	FBRLook Copy = Look;
	Copy.Glasses = 1;
	SetLook(Copy); // (re-applies the look with the uniform)
}

// ===========================================================================================
// House burglary

ABRHouseStash::ABRHouseStash()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	// A little brass lock plate on the door (the door itself belongs to the house).
	AddMesh(TEXT("Lock"), CubeMesh, FVector(0.f, 0.f, 110.f), FVector(6.f, 14.f, 20.f), FLinearColor(0.8f, 0.65f, 0.25f), FRotator::ZeroRotator, false);
	AddPickupZone(130.f);
}

void ABRHouseStash::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRHouseStash, RestockAt);
}

bool ABRHouseStash::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	const ABRCharacter* C = Cast<ABRCharacter>(Interactor);
	return C && !C->IsNPC() && C->GetCondition() == EPVVCondition::Normal && !C->GetCurrentBoat() && !C->GetCarriedCharacter();
}

bool ABRHouseStash::HasLockpick(const APVVCharacter* Who)
{
	const ABRCharacter* C = Cast<ABRCharacter>(Who);
	const ABRPlayerState* PS = C ? C->GetBRPlayerState() : nullptr;
	return PS && PS->HasItem(EBRItem::Lockpick);
}

float ABRHouseStash::GetInteractDuration_Implementation(APVVCharacter* Interactor) const
{
	return HasLockpick(Interactor) && IsStocked() ? PickSeconds : 0.3f;
}

bool ABRHouseStash::IsStocked() const
{
	const ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>();
	return !GS || GS->GetServerWorldTimeSeconds() >= RestockAt;
}

FText ABRHouseStash::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	if (!IsStocked())
	{
		return LOCTEXT("Robbed", "Already cleaned out (come back later)");
	}
	if (!HasLockpick(Interactor))
	{
		return LOCTEXT("Locked", "Locked. You need a LOCKPICK to get in.");
	}
	return LOCTEXT("PickLock", "PICK THE LOCK and rob the house (hold) - don't let the Policia see you");
}

void ABRHouseStash::OnInteract_Implementation(APVVCharacter* Interactor)
{
	ABRCharacter* C = Cast<ABRCharacter>(Interactor);
	ABRPlayerState* PS = C ? C->GetBRPlayerState() : nullptr;
	ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>();
	if (!HasAuthority() || !PS || !GS || !IsStocked())
	{
		return;
	}
	if (!PS->HasItem(EBRItem::Lockpick))
	{
		C->Notify(LOCTEXT("NeedPick", "It's locked. Buy a Lockpick (Black market / gun shop)."));
		return;
	}
	RestockAt = GS->GetServerWorldTimeSeconds() + RestockSeconds;
	ForceNetUpdate();
	// Burglary is a crime against Mexico - if a cop sees it.
	C->CommitCrimeInTown(45.f);
	// What's inside?
	FString Got;
	const float Roll = FMath::FRand();
	const EBRItem Items[] = { EBRItem::Taco, EBRItem::Knife, EBRItem::FakePassport, EBRItem::Poncho, EBRItem::Scrap, EBRItem::Trumpet, EBRItem::GasCan };
	if (Roll < 0.08f && PS->HasFreeSlot())
	{
		PS->AddItem(EBRItem::IllegalGun);
		Got = TEXT("a REVOLVER under the bed");
	}
	else if (Roll < 0.45f && PS->HasFreeSlot())
	{
		const EBRItem Item = Items[FMath::RandRange(0, static_cast<int32>(UE_ARRAY_COUNT(Items)) - 1)];
		PS->AddItem(Item);
		Got = BR::ItemName(Item);
	}
	else
	{
		const int32 Cash = FMath::RandRange(8, 26) * 10;
		PS->AddMoney(Cash);
		Got = FString::Printf(TEXT("$%d stashed in a jar"), Cash);
		ABRGameState::FX(this, EBRFX::MoneyText, GetActorLocation() + FVector(0.f, 0.f, 150.f), FString::Printf(TEXT("+$%d"), Cash));
	}
	// Lockpicks don't last forever.
	const bool bSnapped = FMath::FRand() < 0.3f;
	if (bSnapped)
	{
		PS->RemoveItem(EBRItem::Lockpick);
	}
	C->Notify(FText::Format(LOCTEXT("Burgled", "You slipped in and found {0}.{1}"), FText::FromString(Got),
		bSnapped ? LOCTEXT("PickSnapped", " (your lockpick snapped)") : FText::GetEmpty()));
	ABRGameState::Sfx(this, EBRSfx::CashPickup, GetActorLocation());
}

#undef LOCTEXT_NAMESPACE
