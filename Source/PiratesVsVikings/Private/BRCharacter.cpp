#include "BRCharacter.h"
#include "BRDesertArena.h"
#include "BRDrone.h"
#include "BRGameMode.h"
#include "BRGameState.h"
#include "BRPlayerController.h"
#include "BRPlayerState.h"
#include "PVVBoat.h"
#include "PVVInputUtils.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DrawDebugHelpers.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "BRCharacter"

ABRCharacter::ABRCharacter()
{
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
	GetCharacterMovement()->MaxWalkSpeedCrouched = 220.f;
}

void ABRCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABRCharacter, Hunger);
	DOREPLIFETIME(ABRCharacter, Visibility);
	DOREPLIFETIME(ABRCharacter, SpottedUntil);
	DOREPLIFETIME(ABRCharacter, bLethalMode);
}

ABRPlayerState* ABRCharacter::GetBRPlayerState() const
{
	return Cast<ABRPlayerState>(GetPVVPlayerState());
}

ABRGameState* ABRCharacter::GetBRGameState() const
{
	return GetWorld() ? GetWorld()->GetGameState<ABRGameState>() : nullptr;
}

ABRDesertArena* ABRCharacter::FindArena()
{
	if (!Arena.IsValid())
	{
		for (TActorIterator<ABRDesertArena> It(GetWorld()); It; ++It)
		{
			Arena = *It;
			break;
		}
	}
	return Arena.Get();
}

bool ABRCharacter::IsSpotted() const
{
	const ABRGameState* GS = GetBRGameState();
	return GS && GS->GetServerWorldTimeSeconds() < SpottedUntil;
}

void ABRCharacter::Spot(float Seconds)
{
	if (const ABRGameState* GS = GetBRGameState())
	{
		SpottedUntil = GS->GetServerWorldTimeSeconds() + Seconds;
	}
}

void ABRCharacter::Notify(const FText& Message) const
{
	if (ABRPlayerController* PC = Cast<ABRPlayerController>(GetController()))
	{
		PC->Client_Notify(Message);
	}
}

// -------------------------------------------------------------------------------------------
// Input

void ABRCharacter::EnsureDefaultInput()
{
	const bool bBuildingDefaults = DefaultMappingContext == nullptr;
	Super::EnsureDefaultInput();
	if (!bBuildingDefaults)
	{
		return;
	}

	FireAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	CrouchAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	EatAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	DroneAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	WeaponModeAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);

	PVVInput::Map(DefaultMappingContext, FireAction, EKeys::RightMouseButton);
	PVVInput::Map(DefaultMappingContext, FireAction, EKeys::Gamepad_RightTrigger);
	PVVInput::Map(DefaultMappingContext, CrouchAction, EKeys::C);
	PVVInput::Map(DefaultMappingContext, CrouchAction, EKeys::LeftControl);
	PVVInput::Map(DefaultMappingContext, CrouchAction, EKeys::Gamepad_RightThumbstick);
	PVVInput::Map(DefaultMappingContext, EatAction, EKeys::F);
	PVVInput::Map(DefaultMappingContext, EatAction, EKeys::Gamepad_DPad_Down);
	PVVInput::Map(DefaultMappingContext, DroneAction, EKeys::Q);
	PVVInput::Map(DefaultMappingContext, DroneAction, EKeys::Gamepad_DPad_Up);
	PVVInput::Map(DefaultMappingContext, WeaponModeAction, EKeys::X);
	PVVInput::Map(DefaultMappingContext, WeaponModeAction, EKeys::Gamepad_DPad_Right);
}

void ABRCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		return;
	}
	if (FireAction) Input->BindAction(FireAction, ETriggerEvent::Started, this, &ABRCharacter::Input_Fire);
	if (CrouchAction) Input->BindAction(CrouchAction, ETriggerEvent::Started, this, &ABRCharacter::Input_Crouch);
	if (EatAction) Input->BindAction(EatAction, ETriggerEvent::Started, this, &ABRCharacter::Input_Eat);
	if (DroneAction) Input->BindAction(DroneAction, ETriggerEvent::Started, this, &ABRCharacter::Input_Drone);
	if (WeaponModeAction) Input->BindAction(WeaponModeAction, ETriggerEvent::Started, this, &ABRCharacter::Input_WeaponMode);
}

void ABRCharacter::Input_Crouch()
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else if (CanMove())
	{
		Crouch();
	}
}

void ABRCharacter::Input_Eat()
{
	Server_Eat();
}

void ABRCharacter::Input_Drone()
{
	Server_LaunchDrone();
}

void ABRCharacter::Input_WeaponMode()
{
	Server_ToggleWeaponMode();
}

void ABRCharacter::Input_Fire()
{
	if (!CanAttack() || !FollowCamera)
	{
		return;
	}
	Server_Fire(FollowCamera->GetComponentLocation(), FollowCamera->GetForwardVector());
}

// -------------------------------------------------------------------------------------------
// Server actions

void ABRCharacter::Server_Eat_Implementation()
{
	ABRPlayerState* PS = GetBRPlayerState();
	if (PS && Hunger < 95.f && PS->RemoveItem(EBRItem::Taco))
	{
		Eat(TacoFood);
		Notify(LOCTEXT("Ate", "Delicious. (+food)"));
	}
	else if (PS && !PS->HasItem(EBRItem::Taco))
	{
		Notify(LOCTEXT("NoFood", "No tacos! Buy some at the taco stand."));
	}
}

void ABRCharacter::Server_ToggleWeaponMode_Implementation()
{
	if (!IsWarden())
	{
		return;
	}
	const ABRGameState* GS = GetBRGameState();
	if (!bLethalMode && GS && !GS->LethalAllowed())
	{
		Notify(LOCTEXT("GunsRevoked", "Approval too low - your guns were taken away. Taser only."));
		return;
	}
	bLethalMode = !bLethalMode;
	Notify(bLethalMode ? LOCTEXT("Lethal", "GUN (lethal). Shooting someone who hasn't crossed = SCANDAL.")
		: LOCTEXT("Taser", "TASER (knocks down)."));
}

void ABRCharacter::Server_Fire_Implementation(FVector_NetQuantize Start, FVector_NetQuantizeNormal Direction)
{
	ABRPlayerState* PS = GetBRPlayerState();
	ABRGameState* GS = GetBRGameState();
	if (!CanAttack() || !PS || !GS || FVector::Dist(Start, GetActorLocation()) > 900.f)
	{
		return;
	}

	const bool bRunnerGun = IsRunner();
	if (bRunnerGun && !PS->HasItem(EBRItem::IllegalGun))
	{
		Notify(LOCTEXT("NoGun", "You don't have a gun. (Black market, at night.)"));
		return;
	}
	const bool bLethal = IsWarden() && bLethalMode && GS->LethalAllowed();
	const float Cooldown = (bLethal || bRunnerGun) ? GunCooldown : TaserCooldown;
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastFireTime < Cooldown)
	{
		return;
	}
	LastFireTime = Now;

	if (bRunnerGun)
	{
		PS->bIsAggressor = true; // drawing a gun makes force against you justified
	}

	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_Vehicle);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BRFire), false, this);
	Params.AddIgnoredActor(this);

	const FVector End = Start + FVector(Direction) * WeaponRange;
	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, Objects, Params);
	const FVector HitPoint = bHit ? Hit.ImpactPoint : End;
	Multicast_Shot(GetActorLocation() + FVector(0.f, 0.f, 50.f), HitPoint, bLethal || bRunnerGun);
	if (bLethal || bRunnerGun)
	{
		GS->Multicast_NoisePing(GetActorLocation());
	}

	APVVCharacter* Victim = bHit ? Cast<APVVCharacter>(Hit.GetActor()) : nullptr;
	if (!Victim || !PVV::IsHostile(GetFaction(), Victim->GetFaction()))
	{
		return;
	}

	const FVector Push = FVector(Direction).GetSafeNormal2D() * 500.f + FVector(0.f, 0.f, 250.f);
	if (bLethal)
	{
		if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
		{
			GM->HandleLethalShot(Victim, this);
		}
	}
	else
	{
		Victim->ApplyKnockdown(this, Push);
	}
}

void ABRCharacter::Multicast_Shot_Implementation(FVector_NetQuantize Start, FVector_NetQuantize End, bool bLethal)
{
	// Prototype tracer. Replace with Niagara + sound.
	DrawDebugLine(GetWorld(), Start, End, bLethal ? FColor::Red : FColor::Cyan, false, 0.15f, 0, bLethal ? 2.f : 4.f);
}

void ABRCharacter::Server_LaunchDrone_Implementation()
{
	const ABRGameState* GS = GetBRGameState();
	AController* MyController = GetController();
	if (!IsWarden() || !GS || !MyController || bInDrone || GetCondition() != EPVVCondition::Normal || GetCurrentBoat() || GetCarriedCharacter())
	{
		return;
	}
	if (!GS->DronesAllowed())
	{
		Notify(LOCTEXT("NoDrones", "Approval too low - drone program defunded."));
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastDroneTime < DroneCooldown)
	{
		Notify(FText::Format(LOCTEXT("DroneCharging", "Drone recharging ({0}s)"), FText::AsNumber(FMath::CeilToInt(DroneCooldown - (Now - LastDroneTime)))));
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ABRDrone* Drone = GetWorld()->SpawnActor<ABRDrone>(ABRDrone::StaticClass(), GetActorLocation() + FVector(0.f, 0.f, 300.f), FRotator(0.f, GetActorRotation().Yaw, 0.f), SpawnParams);
	if (!Drone)
	{
		return;
	}
	LastDroneTime = Now;
	bInDrone = true;
	if (bIsCrouched) UnCrouch();
	Drone->Pilot = this;
	MyController->Possess(Drone);
}

void ABRCharacter::OnDroneFinished()
{
	bInDrone = false;
	LastDroneTime = GetWorld()->GetTimeSeconds();
}

// -------------------------------------------------------------------------------------------
// Tick: hunger, visibility, river, stealth rendering

void ABRCharacter::UpdateMovementSpeed()
{
	Super::UpdateMovementSpeed();
	if (Hunger < WeakHunger && IsSprinting())
	{
		GetCharacterMovement()->MaxWalkSpeed = FMath::Min(GetCharacterMovement()->MaxWalkSpeed, WalkSpeed);
	}
}

void ABRCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ABRGameState* GS = GetBRGameState();
	ABRDesertArena* MyArena = FindArena();
	const bool bInRiver = MyArena && !GetCurrentBoat() && !GetCarrier() && MyArena->IsInRiver(GetActorLocation());

	if (HasAuthority() && GS)
	{
		// Hunger (Runners only - Wardens are government-fed).
		if (IsRunner() && GetCondition() != EPVVCondition::Detained && GS->Phase != EBRPhase::Ended)
		{
			const float Drain = HungerPerSecond * (IsSprinting() ? 1.8f : 1.f) * (bInRiver ? 2.f : 1.f) * (GS->IsNight() ? 0.8f : 1.f);
			const float Before = Hunger;
			Hunger = FMath::Max(0.f, Hunger - Drain * DeltaSeconds);
			if ((Before >= WeakHunger) != (Hunger >= WeakHunger))
			{
				UpdateMovementSpeed();
			}
			const float Now = GetWorld()->GetTimeSeconds();
			if (Hunger <= 0.f && GetCondition() == EPVVCondition::Normal && Now - LastFaintTime > 45.f)
			{
				LastFaintTime = Now;
				Notify(LOCTEXT("Fainted", "You fainted from hunger! Eat something (F)."));
				ApplyKnockdown(nullptr, FVector(0.f, 0.f, 50.f));
			}
		}

		// Visibility (how far away Wardens can see you), updated a few times a second.
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now >= NextVisibilityUpdate)
		{
			NextVisibilityUpdate = Now + 0.25f;
			float V = GS->IsNight() ? 0.3f : 1.f;
			if (bIsCrouched) V *= 0.5f;
			if (IsSprinting()) V *= 1.5f;
			if (GS->Weather == EBRWeather::Sandstorm) V *= 0.5f;
			else if (GS->Weather == EBRWeather::Rain) V *= 0.75f;
			if (const ABRPlayerState* PS = GetBRPlayerState())
			{
				if (PS->HasItem(EBRItem::Ladder)) V *= 1.3f; // hard to hide a ladder
			}
			Visibility = FMath::Clamp(V, 0.05f, 1.5f);
		}
	}

	// The current pushes swimmers downstream. Applied on the owning client so movement stays predicted.
	if (bInRiver && IsLocallyControlled() && CanMove())
	{
		const float Strength = RiverCurrent * (GS && GS->Weather == EBRWeather::Rain ? 1.5f : 1.f);
		AddMovementInput(MyArena->GetRiverFlowDirection(), Strength);
	}

	UpdateStealthRendering();
}

void ABRCharacter::UpdateStealthRendering()
{
	// Each Warden's screen hides Runners that are too far away to see (night / crouching / storms).
	if (GetNetMode() == NM_DedicatedServer || !IsRunner())
	{
		return;
	}
	const APlayerController* LocalPC = GetWorld()->GetFirstPlayerController();
	const ABRGameState* GS = GetBRGameState();
	const ABRPlayerState* LocalPS = LocalPC ? LocalPC->GetPlayerState<ABRPlayerState>() : nullptr;
	bool bHide = false;
	if (GS && LocalPS && LocalPS->GetFaction() == BR::Wardens && GS->Phase == EBRPhase::Invasion && !IsSpotted() && LocalPC->PlayerCameraManager)
	{
		const float VisibleRange = 800.f + Visibility * 7000.f;
		bHide = FVector::Dist(LocalPC->PlayerCameraManager->GetCameraLocation(), GetActorLocation()) > VisibleRange;
	}
	// Component visibility is local (not replicated), unlike actor hidden, so this only affects this screen.
	if (bStealthHidden != bHide)
	{
		bStealthHidden = bHide;
		GetRootComponent()->SetVisibility(!bHide, true);
	}
}

#undef LOCTEXT_NAMESPACE
