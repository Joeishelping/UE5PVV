#include "PVVCharacter.h"
#include "PVVBoat.h"
#include "PVVDetentionZone.h"
#include "PVVGameMode.h"
#include "PVVInputUtils.h"
#include "PVVLake.h"
#include "PVVPlayerState.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "PVVCharacter"

APVVCharacter::APVVCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	SetNetCullDistanceSquared(FMath::Square(50000.f));

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 600.f, 0.f);
	Move->JumpZVelocity = 620.f;
	Move->AirControl = 0.35f;
	Move->MaxWalkSpeed = WalkSpeed;
	Move->MaxSwimSpeed = SwimSpeed;
	Move->BrakingDecelerationWalking = 2000.f;
	Move->GetNavAgentPropertiesRef().bCanSwim = true;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 450.f;
	CameraBoom->SocketOffset = FVector(0.f, 0.f, 80.f);
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -96.f), FRotator(0.f, -90.f, 0.f));

	StatusLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StatusLabel"));
	StatusLabel->SetupAttachment(RootComponent);
	StatusLabel->SetRelativeLocation(FVector(0.f, 0.f, 125.f));
	StatusLabel->SetHorizontalAlignment(EHTA_Center);
	StatusLabel->SetWorldSize(22.f);
	StatusLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PlaceholderBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderBody"));
	PlaceholderBody->SetupAttachment(RootComponent);
	PlaceholderBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlaceholderBody->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.9f));
	PlaceholderBody->SetHiddenInGame(true);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		PlaceholderBody->SetStaticMesh(CylinderMesh.Object);
	}
}

void APVVCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APVVCharacter, Faction);
	DOREPLIFETIME(APVVCharacter, Condition);
	DOREPLIFETIME(APVVCharacter, CarriedBy);
	DOREPLIFETIME(APVVCharacter, CarriedCharacter);
	DOREPLIFETIME(APVVCharacter, CurrentBoat);
	DOREPLIFETIME(APVVCharacter, Balance);
	DOREPLIFETIME(APVVCharacter, StruggleProgress);
	DOREPLIFETIME(APVVCharacter, LastHitImpulse);
	DOREPLIFETIME_CONDITION(APVVCharacter, HoldProgress, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(APVVCharacter, bIsSprinting, COND_SkipOwner);
}

void APVVCharacter::BeginPlay()
{
	Super::BeginPlay();

	MeshRelativeTransform = GetMesh()->GetRelativeTransform();
	PlaceholderBody->SetHiddenInGame(GetMesh()->GetSkeletalMeshAsset() != nullptr);
	PlaceholderMID = PlaceholderBody->CreateDynamicMaterialInstance(0);

	if (HasAuthority())
	{
		Balance = MaxBalance;
		if (Faction == EPVVFaction::None)
		{
			Faction = DefaultFaction;
		}
	}
	RefreshStateEffects();
}

void APVVCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority() && EndPlayReason == EEndPlayReason::Destroyed)
	{
		ReleaseAllAttachments();
	}
	Super::EndPlay(EndPlayReason);
}

void APVVCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Cache: PlayerState is cleared from the pawn while its controller drives a boat.
	if (APVVPlayerState* PS = GetPlayerState<APVVPlayerState>())
	{
		CachedPlayerState = PS;
		bIsPlayerCharacter = true;
		if (Faction != PS->GetFaction())
		{
			Faction = PS->GetFaction();
			RefreshStateEffects();
		}
	}
}

void APVVCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	EnsureDefaultInput();
	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->ClearAllMappings();
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void APVVCharacter::EnsureDefaultInput()
{
	if (DefaultMappingContext)
	{
		return;
	}

	MoveAction = PVVInput::MakeAction(this, EInputActionValueType::Axis2D);
	LookAction = PVVInput::MakeAction(this, EInputActionValueType::Axis2D);
	JumpAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	SprintAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	AttackAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	InteractAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);

	UInputMappingContext* IMC = NewObject<UInputMappingContext>(this);
	PVVInput::Map(IMC, MoveAction, EKeys::W, false, true);
	PVVInput::Map(IMC, MoveAction, EKeys::S, true, true);
	PVVInput::Map(IMC, MoveAction, EKeys::D);
	PVVInput::Map(IMC, MoveAction, EKeys::A, true);
	PVVInput::Map(IMC, MoveAction, EKeys::Gamepad_Left2D);
	PVVInput::Map(IMC, LookAction, EKeys::Mouse2D);
	PVVInput::Map(IMC, LookAction, EKeys::Gamepad_Right2D, false, false, 3.f);
	PVVInput::Map(IMC, JumpAction, EKeys::SpaceBar);
	PVVInput::Map(IMC, JumpAction, EKeys::Gamepad_FaceButton_Bottom);
	PVVInput::Map(IMC, SprintAction, EKeys::LeftShift);
	PVVInput::Map(IMC, SprintAction, EKeys::Gamepad_LeftThumbstick);
	PVVInput::Map(IMC, AttackAction, EKeys::LeftMouseButton);
	PVVInput::Map(IMC, AttackAction, EKeys::Gamepad_FaceButton_Left);
	PVVInput::Map(IMC, InteractAction, EKeys::E);
	PVVInput::Map(IMC, InteractAction, EKeys::Gamepad_FaceButton_Right);
	DefaultMappingContext = IMC;
}

void APVVCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	EnsureDefaultInput();
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		return;
	}

	if (MoveAction)
	{
		Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APVVCharacter::Input_Move);
	}
	if (LookAction)
	{
		Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &APVVCharacter::Input_Look);
	}
	if (JumpAction)
	{
		Input->BindAction(JumpAction, ETriggerEvent::Started, this, &APVVCharacter::Input_Jump);
		Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &APVVCharacter::Input_StopJump);
	}
	if (SprintAction)
	{
		Input->BindAction(SprintAction, ETriggerEvent::Started, this, &APVVCharacter::Input_SprintStart);
		Input->BindAction(SprintAction, ETriggerEvent::Completed, this, &APVVCharacter::Input_SprintStop);
	}
	if (AttackAction)
	{
		Input->BindAction(AttackAction, ETriggerEvent::Started, this, &APVVCharacter::Input_Attack);
	}
	if (InteractAction)
	{
		Input->BindAction(InteractAction, ETriggerEvent::Started, this, &APVVCharacter::Input_InteractStart);
		Input->BindAction(InteractAction, ETriggerEvent::Completed, this, &APVVCharacter::Input_InteractStop);
	}
}

// -------------------------------------------------------------------------------------------
// Tick

void APVVCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (Condition == EPVVCondition::Normal && Balance < MaxBalance && Now - LastDamageTime > BalanceRegenDelay)
		{
			Balance = FMath::Min(MaxBalance, Balance + BalanceRegenPerSecond * DeltaSeconds);
		}
		if (StruggleProgress > 0.f)
		{
			StruggleProgress = FMath::Max(0.f, StruggleProgress - StruggleDecayPerSecond * DeltaSeconds);
		}
		TickHold(DeltaSeconds);
	}

	UpdateWading();
	if (IsLocallyControlled())
	{
		UpdateFocus();
	}

	// Billboard the label toward the local camera.
	if (StatusLabel && GetNetMode() != NM_DedicatedServer)
	{
		if (const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
		{
			const FVector ToCam = Cam->GetCameraLocation() - StatusLabel->GetComponentLocation();
			StatusLabel->SetWorldRotation(FRotator(0.f, ToCam.Rotation().Yaw, 0.f));
		}
	}
}

// -------------------------------------------------------------------------------------------
// Queries

APVVPlayerState* APVVCharacter::GetPVVPlayerState() const
{
	if (APVVPlayerState* PS = GetPlayerState<APVVPlayerState>())
	{
		return PS;
	}
	return CachedPlayerState.Get();
}

FString APVVCharacter::GetPlayerDisplayName() const
{
	if (const APVVPlayerState* PS = GetPVVPlayerState())
	{
		return PS->GetPlayerName();
	}
	return GetName();
}

bool APVVCharacter::CanAttack() const
{
	return Condition == EPVVCondition::Normal && !CarriedCharacter && !CarriedBy;
}

bool APVVCharacter::CanMove() const
{
	return !CarriedBy && !CurrentBoat && Condition != EPVVCondition::KnockedDown;
}

// -------------------------------------------------------------------------------------------
// Input

void APVVCharacter::Input_Move(const FInputActionValue& Value)
{
	if (!Controller || !CanMove())
	{
		return;
	}
	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Axis.X);
}

void APVVCharacter::Input_Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(-Axis.Y);
}

void APVVCharacter::Input_Jump()
{
	if (HandleJumpPressed())
	{
		return;
	}
	if (Condition == EPVVCondition::Restrained)
	{
		Server_Struggle();
		return;
	}
	if (CurrentBoat)
	{
		Server_LeaveSeat();
		return;
	}
	if (CanMove())
	{
		Jump();
	}
}

void APVVCharacter::Input_StopJump()
{
	StopJumping();
}

void APVVCharacter::Input_SprintStart()
{
	bIsSprinting = true;
	UpdateMovementSpeed();
	Server_SetSprinting(true);
}

void APVVCharacter::Input_SprintStop()
{
	bIsSprinting = false;
	UpdateMovementSpeed();
	Server_SetSprinting(false);
}

void APVVCharacter::Input_Attack()
{
	if (CanAttack() && WantsMeleeOnAttack())
	{
		Server_Attack();
	}
}

void APVVCharacter::Input_InteractStart()
{
	Server_InteractStart();
}

void APVVCharacter::Input_InteractStop()
{
	Server_InteractStop();
}

void APVVCharacter::Server_SetSprinting_Implementation(bool bNewSprinting)
{
	bIsSprinting = bNewSprinting;
	UpdateMovementSpeed();
}

void APVVCharacter::OnRep_Sprinting()
{
	UpdateMovementSpeed();
}

void APVVCharacter::Server_LeaveSeat_Implementation()
{
	if (CurrentBoat && Condition != EPVVCondition::Restrained)
	{
		CurrentBoat->Unseat(this);
	}
}

// -------------------------------------------------------------------------------------------
// Combat

void APVVCharacter::Server_Attack_Implementation()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (!CanAttack() || Now - LastAttackTime < AttackCooldown)
	{
		return;
	}
	LastAttackTime = Now;
	CancelHold();
	Multicast_AttackStarted();
	GetWorldTimerManager().SetTimer(AttackTimer, this, &APVVCharacter::PerformAttackSweep, AttackWindup, false);
}

void APVVCharacter::Multicast_AttackStarted_Implementation()
{
	OnAttackStartedNative();
	OnAttackStarted();
}

void APVVCharacter::PerformAttackSweep()
{
	if (!CanAttack())
	{
		return;
	}

	// Seated players swing where they look; on foot, where they face.
	const FRotator AimYaw(0.f, CurrentBoat ? GetBaseAimRotation().Yaw : GetActorRotation().Yaw, 0.f);
	const FVector Forward = AimYaw.Vector();
	const FVector Center = GetActorLocation() + Forward * AttackReach;

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PVVMelee), false, this);
	GetWorld()->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(AttackRadius), Params);

	const APVVGameMode* GM = GetWorld()->GetAuthGameMode<APVVGameMode>();
	TArray<APVVCharacter*> Victims;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		APVVCharacter* Victim = Cast<APVVCharacter>(Overlap.GetActor());
		if (!Victim || Victim == this || Victims.Contains(Victim))
		{
			continue;
		}
		Victims.Add(Victim);

		FVector Dir = (Victim->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		if (Dir.IsNearlyZero())
		{
			Dir = Forward;
		}
		const bool bHostile = PVV::IsHostile(Faction, Victim->GetFaction());
		const float Damage = (bHostile || (GM && GM->AllowsFriendlyDamage())) ? AttackDamage : 0.f;
		Victim->ReceiveMeleeHit(this, Damage, Dir * KnockbackStrength + FVector(0.f, 0.f, KnockbackLift));
	}
}

void APVVCharacter::ReceiveMeleeHit(APVVCharacter* Attacker, float Damage, const FVector& Impulse)
{
	if (!HasAuthority() || CarriedBy)
	{
		return;
	}

	Multicast_HitReceived(Attacker);
	CancelHold();

	if (Damage > 0.f && Condition == EPVVCondition::Normal)
	{
		Balance = FMath::Max(0.f, Balance - Damage);
		LastDamageTime = GetWorld()->GetTimeSeconds();
		if (Balance <= 0.f)
		{
			ApplyKnockdown(Attacker, Impulse);
			return;
		}
	}

	if (!CurrentBoat)
	{
		LaunchCharacter(Impulse, true, true);
	}
}

void APVVCharacter::Multicast_HitReceived_Implementation(APVVCharacter* Attacker)
{
	OnHitReceivedNative(Attacker);
	OnHitReceived(Attacker);
}

void APVVCharacter::ApplyKnockdown(AActor* Cause, const FVector& Impulse)
{
	if (!HasAuthority() || Condition != EPVVCondition::Normal || CarriedBy)
	{
		return;
	}

	if (CurrentBoat)
	{
		CurrentBoat->Unseat(this);
	}
	if (CarriedCharacter)
	{
		DropCarried();
	}
	CancelHold();

	Balance = 0.f;
	LastHitImpulse = Impulse;
	SetState(EPVVCondition::KnockedDown);
	LaunchCharacter(Impulse, true, true);

	FTimerDelegate RecoverDelegate = FTimerDelegate::CreateUObject(this, &APVVCharacter::Recover, 0.5f);
	GetWorldTimerManager().SetTimer(KnockdownTimer, RecoverDelegate, KnockdownDuration, false);

	if (APVVGameMode* GM = GetWorld()->GetAuthGameMode<APVVGameMode>())
	{
		GM->NotifyKnockedDown(this, Cause);
	}
}

void APVVCharacter::Recover(float BalanceFraction)
{
	if (!HasAuthority() || Condition != EPVVCondition::KnockedDown)
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(KnockdownTimer);
	Balance = MaxBalance * BalanceFraction;
	SetState(EPVVCondition::Normal);
}

// -------------------------------------------------------------------------------------------
// Capture / rescue

void APVVCharacter::Restrain(APVVCharacter* Captor)
{
	if (!HasAuthority() || Condition != EPVVCondition::KnockedDown)
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(KnockdownTimer);
	StruggleProgress = 0.f;
	SetState(EPVVCondition::Restrained);

	if (APVVGameMode* GM = GetWorld()->GetAuthGameMode<APVVGameMode>())
	{
		GM->NotifyCaptured(this, Captor);
	}
}

void APVVCharacter::ClearRestraint()
{
	if (!HasAuthority() || Condition != EPVVCondition::Restrained)
	{
		return;
	}
	StruggleProgress = 0.f;
	Balance = MaxBalance * 0.5f;
	SetState(EPVVCondition::Normal);
}

void APVVCharacter::Release(APVVCharacter* Rescuer)
{
	if (!HasAuthority() || Condition != EPVVCondition::Restrained)
	{
		return;
	}
	if (CarriedBy)
	{
		CarriedBy->TakeCarried();
	}
	if (CurrentBoat)
	{
		CurrentBoat->Unseat(this);
	}

	StruggleProgress = 0.f;
	Balance = MaxBalance * 0.5f;
	SetState(EPVVCondition::Normal);

	if (APVVGameMode* GM = GetWorld()->GetAuthGameMode<APVVGameMode>())
	{
		GM->NotifyFreed(this, Rescuer);
	}
}

void APVVCharacter::Server_Struggle_Implementation()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Condition != EPVVCondition::Restrained || Now - LastStruggleTime < 0.06f)
	{
		return;
	}
	LastStruggleTime = Now;

	StruggleProgress += StrugglePerPress * (CarriedBy ? CarriedStruggleMultiplier : 1.f);
	if (StruggleProgress >= 1.f)
	{
		BreakFree();
	}
}

void APVVCharacter::BreakFree()
{
	APVVCharacter* Carrier = CarriedBy;
	Release(nullptr);

	// Slip out of the carrier's arms; they stumble.
	if (Carrier)
	{
		const FVector Back = -Carrier->GetActorForwardVector();
		LaunchCharacter(Back * 300.f + FVector(0.f, 0.f, 450.f), true, true);
		Carrier->LaunchCharacter(Carrier->GetActorForwardVector() * 400.f + FVector(0.f, 0.f, 150.f), true, true);
	}
}

void APVVCharacter::Grab(APVVCharacter* Target)
{
	if (!HasAuthority() || !Target || Target == this || CarriedCharacter || CarriedBy || CurrentBoat || Target->CarriedBy
		|| Target->Condition == EPVVCondition::Detained || Target->CarriedCharacter)
	{
		return;
	}
	if (Target->CurrentBoat)
	{
		Target->CurrentBoat->Unseat(Target);
	}
	CancelHold();
	Target->CancelHold();
	CarriedCharacter = Target;
	Target->CarriedBy = this;
	Target->StruggleProgress = 0.f;
	Target->RefreshStateEffects();
	Target->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Target->SetActorRelativeLocation(CarryOffset);
	Target->SetActorRelativeRotation(CarryRotation);
	RefreshStateEffects();
	ForceNetUpdate();
	Target->ForceNetUpdate();
}

void APVVCharacter::StartCarrying(APVVCharacter* Prisoner)
{
	if (!HasAuthority() || !Prisoner || CarriedCharacter || CarriedBy || CurrentBoat
		|| Prisoner->CarriedBy || Prisoner->Condition != EPVVCondition::Restrained)
	{
		return;
	}
	if (Prisoner->CurrentBoat)
	{
		Prisoner->CurrentBoat->Unseat(Prisoner);
	}

	CancelHold();
	Prisoner->CancelHold();
	CarriedCharacter = Prisoner;
	Prisoner->CarriedBy = this;
	Prisoner->StruggleProgress = 0.f;
	Prisoner->RefreshStateEffects();
	Prisoner->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Prisoner->SetActorRelativeLocation(CarryOffset);
	Prisoner->SetActorRelativeRotation(CarryRotation);
	RefreshStateEffects();
	ForceNetUpdate();
	Prisoner->ForceNetUpdate();
}

APVVCharacter* APVVCharacter::TakeCarried()
{
	APVVCharacter* Prisoner = CarriedCharacter;
	if (!HasAuthority() || !Prisoner)
	{
		return nullptr;
	}

	CarriedCharacter = nullptr;
	Prisoner->CarriedBy = nullptr;
	Prisoner->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Prisoner->SetActorRotation(FRotator(0.f, GetActorRotation().Yaw, 0.f));
	Prisoner->RefreshStateEffects();
	RefreshStateEffects();
	ForceNetUpdate();
	Prisoner->ForceNetUpdate();
	return Prisoner;
}

void APVVCharacter::DropCarried()
{
	if (APVVCharacter* Prisoner = TakeCarried())
	{
		const FVector Dest = GetActorLocation() + GetActorForwardVector() * 90.f;
		Prisoner->TeleportTo(Dest, FRotator(0.f, GetActorRotation().Yaw, 0.f));
	}
}

void APVVCharacter::Detain(APVVDetentionZone* Zone, const FVector& CellLocation)
{
	if (!HasAuthority())
	{
		return;
	}
	if (CarriedBy)
	{
		CarriedBy->TakeCarried();
	}
	if (CurrentBoat)
	{
		CurrentBoat->Unseat(this);
	}
	DetentionZone = Zone;
	StruggleProgress = 0.f;
	SetState(EPVVCondition::Detained);
	TeleportTo(CellLocation, GetActorRotation());
}

void APVVCharacter::ReleaseFromDetention(const FVector& Location)
{
	if (!HasAuthority() || Condition != EPVVCondition::Detained)
	{
		return;
	}
	DetentionZone = nullptr;
	Balance = MaxBalance;
	SetState(EPVVCondition::Normal);
	TeleportTo(Location, GetActorRotation());
}

// -------------------------------------------------------------------------------------------
// Boats

void APVVCharacter::EnterSeat(APVVBoat* Boat, USceneComponent* SeatParent, const FVector& LocalOffset)
{
	if (!HasAuthority() || !Boat || !SeatParent)
	{
		return;
	}
	CancelHold();
	CurrentBoat = Boat;
	RefreshStateEffects();
	AttachToComponent(SeatParent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	SetActorRelativeLocation(LocalOffset);
	SetActorRelativeRotation(FRotator::ZeroRotator);
	ForceNetUpdate();
}

void APVVCharacter::ExitSeat(const FVector& ExitLocation, float ExitYaw)
{
	if (!HasAuthority() || !CurrentBoat)
	{
		return;
	}
	CurrentBoat = nullptr;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	RefreshStateEffects();
	TeleportTo(ExitLocation, FRotator(0.f, ExitYaw, 0.f));
	ForceNetUpdate();
}

// -------------------------------------------------------------------------------------------
// Respawn / cleanup

void APVVCharacter::FellOutOfWorld(const UDamageType& DmgType)
{
	if (!HasAuthority())
	{
		return;
	}
	if (APVVGameMode* GM = GetWorld()->GetAuthGameMode<APVVGameMode>())
	{
		GM->RespawnCharacter(this);
		return;
	}
	Super::FellOutOfWorld(DmgType);
}

void APVVCharacter::ResetForRespawn(const FTransform& SpawnTransform)
{
	if (!HasAuthority())
	{
		return;
	}
	ReleaseAllAttachments();
	GetWorldTimerManager().ClearTimer(KnockdownTimer);
	DetentionZone = nullptr;
	Balance = MaxBalance;
	StruggleProgress = 0.f;
	SetState(EPVVCondition::Normal);
	GetCharacterMovement()->StopMovementImmediately();
	TeleportTo(SpawnTransform.GetLocation(), SpawnTransform.Rotator());
}

void APVVCharacter::ReleaseAllAttachments()
{
	CancelHold();
	if (CarriedCharacter)
	{
		DropCarried();
	}
	if (CarriedBy)
	{
		CarriedBy->TakeCarried();
	}
	if (CurrentBoat)
	{
		CurrentBoat->Unseat(this);
	}
}

// -------------------------------------------------------------------------------------------
// State presentation (runs on every machine)

void APVVCharacter::SetState(EPVVCondition NewCondition)
{
	Condition = NewCondition;
	RefreshStateEffects();
	ForceNetUpdate();
}

void APVVCharacter::OnRep_State()
{
	RefreshStateEffects();
}

void APVVCharacter::RefreshStateEffects()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	const bool bAttached = CarriedBy || CurrentBoat;

	if (bAttached)
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}
	else if (Move->MovementMode == MOVE_None)
	{
		Move->SetDefaultMovementMode();
	}

	// Don't shove the carrier or the boat we're riding.
	Capsule->SetCollisionResponseToChannel(ECC_Pawn, CarriedBy ? ECR_Ignore : ECR_Block);
	Capsule->SetCollisionResponseToChannel(ECC_Vehicle, CurrentBoat ? ECR_Ignore : ECR_Block);

	SetRagdoll(bRagdollOnKnockdown && Condition == EPVVCondition::KnockedDown && !bAttached);

	// No-art fallback: the placeholder topples over when knocked down.
	const bool bToppled = Condition == EPVVCondition::KnockedDown && !bAttached;
	PlaceholderBody->SetRelativeLocationAndRotation(
		bToppled ? FVector(0.f, 0.f, -70.f) : FVector::ZeroVector,
		bToppled ? FRotator(90.f, 0.f, 0.f) : FRotator::ZeroRotator);
	UpdateMovementSpeed();
	UpdateStatusLabel();
	OnStateEffectsRefreshed();
	OnStateChanged();
}

void APVVCharacter::UpdateMovementSpeed()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	float Speed = bIsSprinting ? SprintSpeed : WalkSpeed;
	float Swim = SwimSpeed;
	if (Condition == EPVVCondition::KnockedDown)
	{
		Speed = 0.f;
		Swim = 0.f;
	}
	else if (Condition == EPVVCondition::Restrained)
	{
		Speed = RestrainedSpeed;
		Swim = RestrainedSpeed;
	}
	if (CarriedCharacter)
	{
		Speed *= CarrySpeedMultiplier;
		Swim *= CarrySpeedMultiplier;
	}
	if (bIsWading)
	{
		Speed *= WadeSpeedMultiplier;
	}
	Move->MaxWalkSpeed = Speed;
	Move->MaxSwimSpeed = Swim;
}

void APVVCharacter::SetRagdoll(bool bEnable)
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (bEnable == bRagdollActive || !MeshComp || !MeshComp->GetSkeletalMeshAsset())
	{
		return;
	}
	bRagdollActive = bEnable;

	if (bEnable)
	{
		MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));
		MeshComp->SetSimulatePhysics(true);
		MeshComp->WakeAllRigidBodies();
		MeshComp->SetAllPhysicsLinearVelocity(LastHitImpulse);
	}
	else
	{
		MeshComp->SetSimulatePhysics(false);
		MeshComp->SetCollisionProfileName(TEXT("CharacterMesh"));
		MeshComp->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		MeshComp->SetRelativeTransform(MeshRelativeTransform);
	}
}

void APVVCharacter::UpdateStatusLabel()
{
	if (!StatusLabel)
	{
		return;
	}

	FString Label = PVVFactionName(Faction).ToString().ToUpper();
	switch (Condition)
	{
	case EPVVCondition::KnockedDown: Label += TEXT("\n(knocked down)"); break;
	case EPVVCondition::Restrained: Label += CarriedBy ? TEXT("\n(being hauled)") : TEXT("\n(zip-tied)"); break;
	case EPVVCondition::Detained: Label += TEXT("\n(DETAINED)"); break;
	default: break;
	}
	StatusLabel->SetText(FText::FromString(Label));
	StatusLabel->SetTextRenderColor(PVV::FactionColor(Faction).ToFColor(true));

	if (PlaceholderMID)
	{
		const float Shade = Condition == EPVVCondition::Normal ? 1.f : 0.4f;
		PlaceholderMID->SetVectorParameterValue(TEXT("Color"), PVV::FactionColor(Faction) * Shade);
	}
}

// -------------------------------------------------------------------------------------------
// Interaction (this character as the one pressing Interact)

AActor* APVVCharacter::FindBestInteractable()
{
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_Vehicle);
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(PVVInteract), false, this);
	if (CarriedCharacter)
	{
		Params.AddIgnoredActor(CarriedCharacter);
	}

	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, GetActorLocation(), FQuat::Identity, ObjectParams,
		FCollisionShape::MakeSphere(InteractRadius), Params);

	const FVector Forward = GetBaseAimRotation().Vector().GetSafeNormal2D();
	AActor* Best = nullptr;
	float BestScore = -MAX_flt;
	TSet<AActor*> Seen;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		if (!Candidate || Candidate == this || Seen.Contains(Candidate) || !Candidate->Implements<UPVVInteractable>())
		{
			continue;
		}
		Seen.Add(Candidate);
		if (!IPVVInteractable::Execute_CanInteract(Candidate, this))
		{
			continue;
		}

		const FVector ToCandidate = Candidate->GetActorLocation() - GetActorLocation();
		float Score = FVector::DotProduct(Forward, ToCandidate.GetSafeNormal2D()) - ToCandidate.Size() / InteractRadius;
		// People beat props, so a nearby boat never steals a rescue.
		if (Candidate->IsA<APVVCharacter>())
		{
			Score += 0.5f;
		}
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Candidate;
		}
	}
	return Best;
}

void APVVCharacter::UpdateFocus()
{
	AActor* NewFocus = (Condition == EPVVCondition::Normal && !CarriedBy) ? FindBestInteractable() : nullptr;
	FocusPrompt = NewFocus ? IPVVInteractable::Execute_GetInteractPrompt(NewFocus, this)
		: (CarriedCharacter ? LOCTEXT("Drop", "Drop prisoner") : FText::GetEmpty());

	if (NewFocus != FocusedInteractable)
	{
		FocusedInteractable = NewFocus;
		OnFocusChanged(NewFocus);
	}
}

void APVVCharacter::Server_InteractStart_Implementation()
{
	if (Condition != EPVVCondition::Normal || CarriedBy || HoldTarget)
	{
		return;
	}

	AActor* Target = FindBestInteractable();
	if (!Target)
	{
		if (CarriedCharacter)
		{
			DropCarried();
		}
		return;
	}

	const float Duration = IPVVInteractable::Execute_GetInteractDuration(Target, this);
	if (Duration <= 0.f)
	{
		IPVVInteractable::Execute_OnInteract(Target, this);
		return;
	}

	HoldTarget = Target;
	HoldDuration = Duration;
	HoldElapsed = 0.f;
	HoldProgress = 0.f;
}

void APVVCharacter::Server_InteractStop_Implementation()
{
	CancelHold();
}

void APVVCharacter::UpdateWading()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextLakeSearchTime)
	{
		NextLakeSearchTime = Now + 0.25f;
		CachedLake = APVVLake::FindWaterAt(GetWorld(), GetActorLocation());
	}

	const float FeetZ = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const bool bWadingNow = CachedLake.IsValid() && !CarriedBy && !CurrentBoat
		&& FeetZ < CachedLake->GetWaterSurfaceZ() - 15.f;
	if (bWadingNow != bIsWading)
	{
		bIsWading = bWadingNow;
		UpdateMovementSpeed();
	}
}

void APVVCharacter::TickHold(float DeltaSeconds)
{
	if (!HoldTarget)
	{
		return;
	}
	if (!IsValid(HoldTarget))
	{
		CancelHold();
		return;
	}

	// Props (cell doors etc.) check their own range in CanInteract; people must stay close.
	const bool bInRange = !HoldTarget->IsA<APVVCharacter>()
		|| FVector::Dist(HoldTarget->GetActorLocation(), GetActorLocation()) <= InteractRadius * 1.5f;
	const bool bStillValid = Condition == EPVVCondition::Normal
		&& bInRange
		&& IPVVInteractable::Execute_CanInteract(HoldTarget, this);
	if (!bStillValid)
	{
		CancelHold();
		return;
	}

	HoldElapsed += DeltaSeconds;
	HoldProgress = FMath::Clamp(HoldElapsed / HoldDuration, 0.f, 1.f);
	if (HoldElapsed >= HoldDuration)
	{
		AActor* Target = HoldTarget;
		CancelHold();
		IPVVInteractable::Execute_OnInteract(Target, this);
	}
}

void APVVCharacter::CancelHold()
{
	HoldTarget = nullptr;
	HoldElapsed = 0.f;
	HoldProgress = 0.f;
}

// -------------------------------------------------------------------------------------------
// Interaction (others pressing Interact on this character)

bool APVVCharacter::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	if (!Interactor || Interactor == this || CarriedBy || Interactor->CarriedCharacter
		|| Interactor->Condition != EPVVCondition::Normal
		|| (Condition != EPVVCondition::KnockedDown && Condition != EPVVCondition::Restrained))
	{
		return false;
	}

	const bool bTeammate = Faction != EPVVFaction::None && Interactor->Faction == Faction;
	const bool bCaptor = PVV::CanCapture(Interactor->Faction) && PVV::IsHostile(Interactor->Faction, Faction);

	// Picking someone up needs both feet on the ground.
	if (bCaptor && Condition == EPVVCondition::Restrained && Interactor->CurrentBoat)
	{
		return false;
	}
	return bTeammate || bCaptor;
}

float APVVCharacter::GetInteractDuration_Implementation(APVVCharacter* Interactor) const
{
	const bool bTeammate = Interactor && Interactor->Faction == Faction;
	if (Condition == EPVVCondition::KnockedDown)
	{
		return bTeammate ? HelpUpDuration : CaptureDuration;
	}
	return bTeammate ? RescueDuration : 0.f;
}

FText APVVCharacter::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	const bool bTeammate = Interactor && Interactor->Faction == Faction;
	if (Condition == EPVVCondition::KnockedDown)
	{
		return bTeammate ? LOCTEXT("HelpUp", "Help up (hold)") : LOCTEXT("Restrain", "Zip-tie (hold)");
	}
	return bTeammate ? LOCTEXT("CutLoose", "Cut loose (hold)") : LOCTEXT("PickUp", "Pick up");
}

void APVVCharacter::OnInteract_Implementation(APVVCharacter* Interactor)
{
	if (!HasAuthority() || !Interactor)
	{
		return;
	}

	const bool bTeammate = Interactor->Faction == Faction;
	if (Condition == EPVVCondition::KnockedDown)
	{
		if (bTeammate)
		{
			Recover(1.f);
		}
		else
		{
			Restrain(Interactor);
		}
	}
	else if (Condition == EPVVCondition::Restrained)
	{
		if (bTeammate)
		{
			Release(Interactor);
		}
		else
		{
			Interactor->StartCarrying(this);
		}
	}
}

#undef LOCTEXT_NAMESPACE
