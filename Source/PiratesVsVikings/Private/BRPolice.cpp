// Checkpoint & police tools: frisking, papers-only scans, fake passports, car searches, security cameras,
// border alarms and NPC reinforcement officers.

#include "BRPolice.h"
#include "BRAudio.h"
#include "BRCar.h"
#include "BRCharacter.h"
#include "BRGameMode.h"
#include "BRGameState.h"
#include "BRPlayerState.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
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
	ViewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ViewCamera"));
	ViewCamera->SetupAttachment(Head);
	ViewCamera->SetRelativeLocation(FVector(60.f, 0.f, -4.f));
	ViewCamera->SetFieldOfView(95.f);
	AddPickupZone(160.f);
}

void ABRSurveillanceCamera::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Slowly pans left and right - in step on every screen (server clock), so you can time things like bribes.
	Sweep += DeltaSeconds;
	const ABRGameState* ClockGS = GetWorld()->GetGameState<ABRGameState>();
	const float Clock = ClockGS ? ClockGS->GetServerWorldTimeSeconds() : Sweep;
	const float Phase = FMath::Fmod(FMath::Abs(GetActorLocation().X * 0.013f + GetActorLocation().Y * 0.007f), 6.283f);
	Head->SetRelativeRotation(FRotator(-8.f, SweepAngle * FMath::Sin(Clock * SweepSpeed + Phase), 0.f));
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

bool ABRSurveillanceCamera::SeesPoint(const FVector& Point, const AActor* IgnoreActor) const
{
	const FVector Eye = Head->GetComponentLocation();
	const FVector To = Point - Eye;
	if (To.Size() > Range)
	{
		return false;
	}
	const FVector Look = Head->GetForwardVector().GetSafeNormal2D();
	if (FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(To.GetSafeNormal2D(), Look), -1.f, 1.f))) > HalfAngle)
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BRCameraSees), false, this);
	if (IgnoreActor)
	{
		Params.AddIgnoredActor(IgnoreActor);
	}
	FHitResult Block;
	return !GetWorld()->LineTraceSingleByChannel(Block, Eye, Point, ECC_Visibility, Params);
}

bool ABRSurveillanceCamera::AnyCameraSees(const UWorld* World, const FVector& Point, const AActor* IgnoreActor)
{
	for (TActorIterator<ABRSurveillanceCamera> It(World); It; ++It)
	{
		if (It->SeesPoint(Point, IgnoreActor))
		{
			return true;
		}
	}
	return false;
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

void ABRCharacter::Input_CameraView()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !IsLocallyControlled())
	{
		return;
	}
	if (!IsWarden())
	{
		return;
	}
	TArray<ABRSurveillanceCamera*> Cams;
	for (TActorIterator<ABRSurveillanceCamera> It(GetWorld()); It; ++It)
	{
		Cams.Add(*It);
	}
	Cams.Sort([](const ABRSurveillanceCamera& A, const ABRSurveillanceCamera& B) { return A.GetName() < B.GetName(); });
	if (Cams.Num() == 0)
	{
		StopCameraView();
		Notify(LOCTEXT("NoCams", "No security cameras yet. Buy one at the Border Patrol supply counters, scroll to it and press F to place it. Then L looks through it."));
		return;
	}
	++CameraViewIndex;
	if (CameraViewIndex >= Cams.Num())
	{
		StopCameraView();
		return;
	}
	ViewingCamera = Cams[CameraViewIndex];
	CameraViewCount = Cams.Num();
	CameraViewFrom = GetActorLocation();
	PC->SetViewTargetWithBlend(Cams[CameraViewIndex], 0.15f);
}

void ABRCharacter::StopCameraView()
{
	if (CameraViewIndex < 0 && !ViewingCamera.IsValid())
	{
		return;
	}
	CameraViewIndex = -1;
	ViewingCamera = nullptr;
	if (APlayerController* PC = Cast<APlayerController>(GetController()); PC && PC->IsLocalController())
	{
		PC->SetViewTargetWithBlend(this, 0.15f);
	}
}

void ABRCharacter::TickCameraView()
{
	if (CameraViewIndex < 0)
	{
		return;
	}
	// Walk away, get hit, lose the camera or swap teams: back to your own eyes.
	if (!ViewingCamera.IsValid() || !IsWarden() || GetCondition() != EPVVCondition::Normal || GetCurrentBoat()
		|| FVector::Dist2D(GetActorLocation(), CameraViewFrom) > 150.f)
	{
		StopCameraView();
	}
}

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
		int32 Contraband = 0;
		for (int32 i = PS->Inventory.Num() - 1; i >= 0; --i)
		{
			const EBRItem I = PS->Inventory[i];
			if (BR::IsIllegal(I) || I == EBRItem::Knife || I == EBRItem::FakePassport)
			{
				Contraband += BR::IsIllegal(I) ? 1 : 0;
				PS->RemoveItem(I);
				++Taken;
			}
		}
		ChargeForLostContraband(Contraband); // failed to sneak it through
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
	const bool bWasWaiting = IsWaitingForSearch();
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
		SearchWaitTruck = nullptr; // not getting back in that van
		BustReactAt = -1.f;
		if (bIsNPC && !IsBrawling())
		{
			ReactToBust(Cop, !bIllegal); // caught red-handed: fight, run or give up
		}
	}
	else
	{
		ABRGameState::Sfx(this, EBRSfx::ScanOK, GetActorLocation());
		Cop->Notify(LOCTEXT("FriskClean", "SEARCH: nothing on them."));
		if (bWasWaiting && !bIllegal)
		{
			ReturnToTruckAfterSearch(); // clean: back in the van
		}
		if (GM && !bWasWaiting)
		{
			GM->OnVehicleSearched(Cop, IsSuspect() ? 1 : 0); // searching clean citizens on the street is harassment
		}
		// (searching the people who stepped out at the checkpoint is just the routine check - no penalty)
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

// -------------------------------------------------------------------------------------------
// NPCs searched at the checkpoint: out of the van, wait to be searched, back in

void ABRCharacter::WaitForSearch(ABRTruck* Truck, int32 Seat, ABRCharacter* SuspectCop)
{
	const float Now = GetWorld()->GetTimeSeconds();
	SearchWaitTruck = Truck;
	SearchWaitSeat = Seat;
	SearchWaitSince = Now;
	BustCop = SuspectCop;
	// No papers: they won't just stand there for long (fight, run or hands up). Everyone else waits to be searched.
	BustReactAt = bIllegal ? Now + FMath::FRandRange(3.f, 6.f) : -1.f;
	bWandering = false;
	FleeUntil = 0.f;
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->bRunPhysicsWithNoController = true;
}

void ABRCharacter::ReturnToTruckAfterSearch()
{
	ABRTruck* Truck = SearchWaitTruck.Get();
	const int32 Seat = SearchWaitSeat;
	SearchWaitTruck = nullptr;
	SearchWaitSeat = INDEX_NONE;
	BustReactAt = -1.f;
	if (Truck && GetCondition() == EPVVCondition::Normal && !GetCarrier())
	{
		Truck->ReturnFromSearch(this, Seat);
	}
}

void ABRCharacter::TickSearchWait()
{
	ABRTruck* Truck = SearchWaitTruck.Get();
	if (!Truck)
	{
		return;
	}
	if (GetCondition() != EPVVCondition::Normal || GetCarrier() || GetCurrentBoat() || IsBrawling())
	{
		SearchWaitTruck = nullptr; // zip-tied, grabbed, fighting...: not waiting any more
		BustReactAt = -1.f;
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (BustReactAt > 0.f && Now >= BustReactAt)
	{
		ABRCharacter* Cop = BustCop.Get();
		SearchWaitTruck = nullptr;
		BustReactAt = -1.f;
		ReactToBust(Cop, !bIllegal);
		return;
	}
	if (Now - SearchWaitSince > 120.f)
	{
		ReturnToTruckAfterSearch(); // nobody bothered to search them: they get back in
		return;
	}
	// Stand by the van (walk back if they got shoved away).
	const FVector ToTruck = (Truck->GetActorLocation() - GetActorLocation()) * FVector(1.f, 1.f, 0.f);
	if (ToTruck.Size() > 650.f)
	{
		GetCharacterMovement()->MaxWalkSpeed = WanderSpeed;
		AddMovementInput(ToTruck.GetSafeNormal(), 1.f);
	}
}

#undef LOCTEXT_NAMESPACE
