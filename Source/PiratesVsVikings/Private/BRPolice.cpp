// Checkpoint & police tools: frisking, papers-only scans, fake passports, car searches, security cameras,
// border alarms and NPC reinforcement officers.

#include "BRPolice.h"
#include "BRAudio.h"
#include "BRCar.h"
#include "BRCharacter.h"
#include "BRGameMode.h"
#include "BRGameState.h"
#include "BRPlayerState.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"

#define LOCTEXT_NAMESPACE "BRPolice"

// ===========================================================================================
// Security camera

ABRSurveillanceCamera::ABRSurveillanceCamera()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	const FLinearColor Grey(0.2f, 0.2f, 0.22f);
	AddMesh(TEXT("Pole"), CylinderMesh, FVector(0.f, 0.f, 160.f), FVector(10.f, 10.f, 320.f), Grey);
	Head = CreateDefaultSubobject<USceneComponent>(TEXT("Head"));
	Head->SetupAttachment(Root);
	Head->SetRelativeLocation(FVector(0.f, 0.f, 320.f));
	UStaticMeshComponent* Box = AddMesh(TEXT("CamBody"), CubeMesh, FVector(18.f, 0.f, 320.f), FVector(45.f, 20.f, 20.f), FLinearColor(0.85f, 0.85f, 0.85f), FRotator::ZeroRotator, false);
	Box->SetupAttachment(Head);
	Box->SetRelativeLocation(FVector(18.f, 0.f, 0.f));
	Lamp = CreateDefaultSubobject<UPointLightComponent>(TEXT("RecLight"));
	Lamp->SetupAttachment(Head);
	Lamp->SetRelativeLocation(FVector(42.f, 0.f, 8.f));
	Lamp->SetIntensity(400.f);
	Lamp->SetAttenuationRadius(120.f);
	Lamp->SetLightColor(FLinearColor(1.f, 0.1f, 0.05f));
	Lamp->SetCastShadows(false);
	AddPickupZone(160.f);
}

void ABRSurveillanceCamera::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Slowly pans left and right (on every screen).
	Sweep += DeltaSeconds;
	Head->SetRelativeRotation(FRotator(-8.f, 25.f * FMath::Sin(Sweep * 0.5f), 0.f));
	Lamp->SetVisibility(FMath::Fmod(Sweep, 1.f) < 0.5f); // blinking REC light
	if (!HasAuthority() || Sweep < NextLook)
	{
		return;
	}
	NextLook = Sweep + 0.5f;
	const FVector Eye = Head->GetComponentLocation();
	const FVector Look = Head->GetForwardVector().GetSafeNormal2D();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BRCamera), false, this);
	for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
	{
		ABRCharacter* C = *It;
		const ABRPlayerState* PS = C->GetBRPlayerState();
		const bool bTrespasser = (PS && C->IsRunner() && PS->bHasCrossedLine) || (C->IsNPC() && C->bIllegal && C->IsSuspect() && C->GetCondition() == EPVVCondition::Normal && !C->GetCurrentBoat() && C->IsBrawling());
		if (!bTrespasser || C->GetCondition() != EPVVCondition::Normal || C->IsHiddenFromPatrol() || C->IsFakeAgent())
		{
			continue;
		}
		const FVector To = C->GetActorLocation() - Eye;
		if (To.Size() > Range || FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(To.GetSafeNormal2D(), Look), -1.f, 1.f))) > HalfAngle)
		{
			continue;
		}
		FHitResult Block;
		Params.AddIgnoredActor(C);
		const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Block, Eye, C->GetActorLocation() + FVector(0.f, 0.f, 50.f), ECC_Visibility, Params);
		Params.ClearIgnoredSourceObjects();
		Params.AddIgnoredActor(this);
		if (bBlocked)
		{
			continue;
		}
		C->CameraSpot(6.f);
		if (Sweep - LastAlarm > 25.f)
		{
			LastAlarm = Sweep;
			if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
			{
				GM->TriggerBorderAlarm(C->GetActorLocation(), TEXT("A camera spotted someone"));
			}
		}
	}
}

bool ABRSurveillanceCamera::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	return Interactor && Interactor->GetFaction() == BR::Runners && Interactor->GetCondition() == EPVVCondition::Normal && !Interactor->GetCurrentBoat();
}

FText ABRSurveillanceCamera::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	return LOCTEXT("SmashCam", "Smash the security camera (hold)");
}

void ABRSurveillanceCamera::OnInteract_Implementation(APVVCharacter* Interactor)
{
	if (!HasAuthority())
	{
		return;
	}
	ABRGameState::FX(this, EBRFX::Sparks, Head->GetComponentLocation());
	ABRGameState::FX(this, EBRFX::PointsText, Head->GetComponentLocation() + FVector(0.f, 0.f, 60.f), TEXT("CAMERA DOWN"));
	if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
	{
		GM->Feed(FString::Printf(TEXT("%s smashed a security camera"), *Interactor->GetPlayerDisplayName()));
	}
	Destroy();
}

// ===========================================================================================
// Character side: frisking, camera placement, officers

void ABRCharacter::CameraSpot(float Seconds)
{
	Spot(Seconds);
}

bool ABRCharacter::WantsFrisk(const APVVCharacter* Interactor) const
{
	const ABRCharacter* Cop = Cast<ABRCharacter>(Interactor);
	if (!Cop || !Cop->IsWarden() || Cop == this || IsWarden() || bOfficer || bFakeUniform || GetCondition() != EPVVCondition::Normal || GetCurrentBoat() || GetCarrier()
		|| Cop->GetCarriedCharacter() || Cop->GetCurrentBoat() || IsHiddenFromPatrol() || IsHeldByHands(Interactor))
	{
		return false;
	}
	return GetWorld()->GetTimeSeconds() - LastFriskedTime > 20.f;
}

bool ABRCharacter::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	return WantsFrisk(Interactor) || Super::CanInteract_Implementation(Interactor);
}

float ABRCharacter::GetInteractDuration_Implementation(APVVCharacter* Interactor) const
{
	return WantsFrisk(Interactor) ? 2.f : Super::GetInteractDuration_Implementation(Interactor);
}

FText ABRCharacter::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	if (WantsFrisk(Interactor))
	{
		return LOCTEXT("Frisk", "SEARCH them for weapons & contraband (hold)");
	}
	return Super::GetInteractPrompt_Implementation(Interactor);
}

void ABRCharacter::OnInteract_Implementation(APVVCharacter* Interactor)
{
	if (WantsFrisk(Interactor))
	{
		Frisk(Cast<ABRCharacter>(Interactor));
		return;
	}
	Super::OnInteract_Implementation(Interactor);
}

int32 ABRCharacter::Confiscate()
{
	int32 Taken = 0;
	if (bCarriesContraband)
	{
		bCarriesContraband = false;
		++Taken;
	}
	if (bIsNPC && (HeldItem == EBRItem::Knife || HeldItem == EBRItem::IllegalGun))
	{
		SetNPCHeldItem(EBRItem::None);
		++Taken;
	}
	if (ABRPlayerState* PS = GetBRPlayerState())
	{
		for (int32 i = PS->Inventory.Num() - 1; i >= 0; --i)
		{
			const EBRItem I = PS->Inventory[i];
			if (BR::IsIllegal(I) || I == EBRItem::Knife || I == EBRItem::FakePassport)
			{
				PS->RemoveItem(I);
				++Taken;
			}
		}
	}
	ForceNetUpdate();
	return Taken;
}

void ABRCharacter::Frisk(ABRCharacter* Cop)
{
	if (!HasAuthority() || !Cop)
	{
		return;
	}
	LastFriskedTime = GetWorld()->GetTimeSeconds();
	const bool bHadPassport = GetBRPlayerState() && GetBRPlayerState()->HasItem(EBRItem::FakePassport);
	const int32 Found = Confiscate();
	ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>();
	if (Found > 0)
	{
		Spot(10.f);
		ABRGameState::Sfx(this, EBRSfx::ScanAlarm, GetActorLocation());
		ABRGameState::FX(this, EBRFX::PointsText, GetActorLocation() + FVector(0.f, 0.f, 130.f), FString::Printf(TEXT("SEIZED x%d"), Found));
		Cop->Notify(FText::Format(LOCTEXT("FriskFound", "SEARCH: found {0} thing(s) - weapons / contraband seized.{1}"), Found,
			bHadPassport ? LOCTEXT("FakePapers", " Their passport is FAKE!") : FText::GetEmpty()));
		Notify(LOCTEXT("FriskedYou", "Border Patrol searched you and took your stuff!"));
		if (GM)
		{
			GM->OnContrabandSeized(Cop, Found);
		}
		if (bIsNPC && !IsBrawling())
		{
			ReactToBust(Cop, !bIllegal); // caught red-handed: fight, run or give up
		}
	}
	else
	{
		ABRGameState::Sfx(this, EBRSfx::ScanOK, GetActorLocation());
		Cop->Notify(LOCTEXT("FriskClean", "SEARCH: nothing on them."));
		if (GM)
		{
			GM->OnVehicleSearched(Cop, IsSuspect() ? 1 : 0); // searching clean citizens is harassment
		}
	}
}

bool ABRCharacter::PlaceSecurityCamera()
{
	ABRPlayerState* PS = GetBRPlayerState();
	if (!HasAuthority() || !IsWarden() || !PS || !PS->HasItem(EBRItem::SecurityCamera) || GetCurrentBoat() || GetCondition() != EPVVCondition::Normal)
	{
		return false;
	}
	const FVector Feet = GetActorLocation() + GetActorForwardVector() * 60.f - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (GetWorld()->SpawnActor<ABRSurveillanceCamera>(ABRSurveillanceCamera::StaticClass(), Feet, FRotator(0.f, GetActorRotation().Yaw, 0.f), Params))
	{
		PS->RemoveItem(EBRItem::SecurityCamera);
		Notify(LOCTEXT("CamPlaced", "Camera up. It watches where you were facing and sounds the alarm when it sees a trespasser."));
		return true;
	}
	return false;
}

void ABRCharacter::MakeOfficer(float LifeSeconds)
{
	bOfficer = true;
	bNoFightBack = true;
	SetNPCFaction(BR::Wardens);
	OfficerUntil = GetWorld()->GetTimeSeconds() + LifeSeconds;
	bIllegal = false;
	bCarriesContraband = false;
	SetHat(EBRHat::PatrolCap);
	FBRLook Uniform = Look;
	Uniform.ShirtStyle = 0;
	Uniform.PantsStyle = 0;
	Uniform.ShirtColor = 0.36f; // olive green
	Uniform.PantsColor = 0.62f; // dark navy
	Uniform.Glasses = 0;
	SetLook(Uniform);
	ForceNetUpdate();
}

void ABRCharacter::TickOfficer(float DeltaSeconds)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now > OfficerUntil)
	{
		Destroy(); // shift's over
		return;
	}
	// Find the nearest Runner who crossed the Line (or whoever just hit us).
	ABRCharacter* Target = nullptr;
	float Best = 4500.f;
	for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
	{
		ABRCharacter* C = *It;
		const ABRPlayerState* PS = C->GetBRPlayerState();
		const bool bWanted = (PS && C->IsRunner() && PS->bHasCrossedLine) || (ProvokedBy.Get() == C && Now - ProvokeTime < 8.f);
		if (!bWanted || C == this || C->GetCurrentBoat() || C->IsHiddenFromPatrol() || (C->IsFakeAgent() && ProvokedBy.Get() != C) || C->GetCondition() == EPVVCondition::Detained || C->GetCondition() == EPVVCondition::Restrained)
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
		return; // nobody to chase: stand guard
	}
	const FVector To = (Target->GetActorLocation() - GetActorLocation()) * FVector(1.f, 1.f, 0.f);
	GetCharacterMovement()->MaxWalkSpeed = 560.f;
	if (Target->GetCondition() == EPVVCondition::KnockedDown)
	{
		// Down: walk over and zip-tie them.
		if (To.Size() > 120.f)
		{
			AddMovementInput(To.GetSafeNormal(), 1.f);
		}
		else
		{
			Target->Restrain(this);
		}
		return;
	}
	AngryAt = Target;
	AngryUntil = Now + 2.f;
	if (To.Size() > 130.f)
	{
		AddMovementInput(To.GetSafeNormal(), 1.f);
	}
	BrawlerAct(Target, 0.85f, DeltaSeconds);
}

#undef LOCTEXT_NAMESPACE
