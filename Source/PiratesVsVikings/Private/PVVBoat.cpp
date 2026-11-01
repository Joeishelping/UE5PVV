#include "PVVBoat.h"
#include "PVVCharacter.h"
#include "PVVInputUtils.h"
#include "PVVLake.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "PVVBoat"

APVVBoat::APVVBoat()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(60.f);
	SetNetCullDistanceSquared(FMath::Square(50000.f));
	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = nullptr;
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	Hull = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hull"));
	SetRootComponent(Hull);
	Hull->SetCollisionProfileName(TEXT("PhysicsActor"));
	Hull->SetCollisionObjectType(ECC_Vehicle);
	Hull->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Hull->SetSimulatePhysics(true);
	Hull->SetNotifyRigidBodyCollision(true);
	Hull->SetLinearDamping(0.1f);
	Hull->SetAngularDamping(1.5f);
	Hull->BodyInstance.COMNudge = FVector(0.f, 0.f, -40.f); // Low centre of mass = harder to capsize.

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Hull->SetStaticMesh(CubeMesh.Object);
		Hull->SetRelativeScale3D(FVector(4.f, 1.8f, 0.6f)); // 4m x 1.8m skiff placeholder
	}

	SeatRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SeatRoot"));
	SeatRoot->SetupAttachment(Hull);
	SeatRoot->SetUsingAbsoluteScale(true);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(SeatRoot);
	CameraBoom->TargetArmLength = 900.f;
	CameraBoom->SocketOffset = FVector(0.f, 0.f, 250.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 6.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	// Capsule centres sit above the deck (deck top is +30 for the placeholder hull).
	SeatOffsets = {
		FVector(-130.f, 0.f, 125.f),	// driver at the tiller
		FVector(30.f, -50.f, 125.f),
		FVector(30.f, 50.f, 125.f),
		FVector(130.f, 0.f, 125.f)
	};
	PontoonOffsets = {
		FVector(180.f, 80.f, -30.f),
		FVector(180.f, -80.f, -30.f),
		FVector(-180.f, 80.f, -30.f),
		FVector(-180.f, -80.f, -30.f),
		FVector(0.f, 80.f, -30.f),
		FVector(0.f, -80.f, -30.f)
	};
}

void APVVBoat::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APVVBoat, Occupants);
	DOREPLIFETIME_CONDITION(APVVBoat, ThrottleInput, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(APVVBoat, SteerInput, COND_SkipOwner);
}

void APVVBoat::BeginPlay()
{
	Super::BeginPlay();

	Hull->SetMassOverrideInKg(NAME_None, MassKg, true);
	SpawnTransform = GetActorTransform();

	// Paint the placeholder cube like a wooden skiff.
	if (Hull->GetStaticMesh() && Hull->GetStaticMesh()->GetPathName().StartsWith(TEXT("/Engine/BasicShapes")))
	{
		if (UMaterialInstanceDynamic* MID = Hull->CreateDynamicMaterialInstance(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.35f, 0.18f, 0.07f));
		}
	}

	if (HasAuthority())
	{
		Occupants.SetNum(SeatOffsets.Num());
		Hull->OnComponentHit.AddDynamic(this, &APVVBoat::OnHullHit);
	}
}

void APVVBoat::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority() && EndPlayReason == EEndPlayReason::Destroyed)
	{
		EjectAll();
	}
	Super::EndPlay(EndPlayReason);
}

void APVVBoat::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ApplyWaterForces(DeltaSeconds);
	if (HasAuthority())
	{
		TickFlipRecovery(DeltaSeconds);
	}
}

// -------------------------------------------------------------------------------------------
// Physics

void APVVBoat::ApplyWaterForces(float DeltaSeconds)
{
	if (!Hull->IsSimulatingPhysics() || PontoonOffsets.Num() == 0)
	{
		return;
	}

	if (!Lake.IsValid())
	{
		for (TActorIterator<APVVLake> It(GetWorld()); It; ++It)
		{
			Lake = *It;
			break;
		}
	}

	const FVector Origin = Hull->GetComponentLocation();
	const FQuat Rotation = Hull->GetComponentQuat();
	const float Mass = Hull->GetMass();
	const float Gravity = -GetWorld()->GetGravityZ();
	const float NumPontoons = PontoonOffsets.Num();
	const float LiftPerPontoon = Mass * Gravity * BuoyancyMultiplier / NumPontoons;

	int32 Wet = 0;
	for (const FVector& Offset : PontoonOffsets)
	{
		const FVector Point = Origin + Rotation.RotateVector(Offset);
		float SurfaceZ = 0.f;
		if (!GetSurfaceZ(Point, SurfaceZ))
		{
			continue;
		}
		const float Depth = SurfaceZ - Point.Z;
		if (Depth <= 0.f)
		{
			continue;
		}
		++Wet;
		const float Submersion = FMath::Clamp(Depth / PontoonDepth, 0.f, 1.f);
		const float PointVelZ = Hull->GetPhysicsLinearVelocityAtPoint(Point).Z;
		const float Lift = LiftPerPontoon * Submersion - PointVelZ * (Mass / NumPontoons) * VerticalDamping;
		Hull->AddForceAtLocation(FVector(0.f, 0.f, Lift), Point);
	}

	WetFraction = Wet / NumPontoons;
	if (Wet == 0)
	{
		return;
	}

	const FVector Velocity = Hull->GetPhysicsLinearVelocity();
	const FVector Forward = Rotation.GetForwardVector().GetSafeNormal2D();
	const FVector Right = Rotation.GetRightVector().GetSafeNormal2D();
	const float ForwardSpeed = FVector::DotProduct(Velocity, Forward);
	const float LateralSpeed = FVector::DotProduct(Velocity, Right);

	const float Throttle = ThrottleInput * GetDriveScale();
	FVector Accel = Forward * (Throttle >= 0.f ? Throttle * ForwardAcceleration : Throttle * ReverseAcceleration);
	Accel -= Forward * ForwardSpeed * ForwardDrag;
	Accel -= Right * LateralSpeed * LateralGrip;
	Hull->AddForce(Accel * WetFraction, NAME_None, true);

	// Rudder: needs water flowing past it, reversed when going backwards.
	const float Authority = FMath::Clamp(FMath::Abs(ForwardSpeed) / FullSteerSpeed, MinSteerAuthority, 1.f);
	const float Direction = ForwardSpeed < -50.f ? -1.f : 1.f;
	const float TargetYawRate = SteerInput * MaxTurnRate * Authority * Direction;
	const float YawRate = Hull->GetPhysicsAngularVelocityInRadians().Z;
	Hull->AddTorqueInRadians(FVector(0.f, 0.f, (TargetYawRate - YawRate) * TurnResponsiveness * WetFraction), NAME_None, true);
}

bool APVVBoat::GetSurfaceZ(const FVector& Point, float& OutZ) const
{
	OutZ = Lake.IsValid() ? Lake->GetWaterSurfaceZ() : WaterSurfaceZ;
	return true;
}

void APVVBoat::TickFlipRecovery(float DeltaSeconds)
{
	if (Hull->GetUpVector().Z > 0.3f)
	{
		FlippedTime = 0.f;
		return;
	}

	const float Previous = FlippedTime;
	FlippedTime += DeltaSeconds;
	if (Previous < 0.5f && FlippedTime >= 0.5f)
	{
		EjectAll(); // Capsized: everyone in the drink.
	}
	if (FlippedTime >= FlipRecoveryTime)
	{
		FlippedTime = 0.f;
		ResetToTransform(FTransform(FRotator(0.f, GetActorRotation().Yaw, 0.f), GetActorLocation() + FVector(0.f, 0.f, 100.f)));
	}
}

void APVVBoat::ResetToTransform(const FTransform& Transform)
{
	SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics);
	Hull->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Hull->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
}

void APVVBoat::FellOutOfWorld(const UDamageType& DmgType)
{
	if (HasAuthority())
	{
		EjectAll();
		ResetToTransform(SpawnTransform);
	}
}

void APVVBoat::OnHullHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority() || !OtherActor)
	{
		return;
	}

	const FVector Velocity = Hull->GetPhysicsLinearVelocity();

	if (APVVCharacter* Victim = Cast<APVVCharacter>(OtherActor))
	{
		if (Victim->GetCurrentBoat() != this && Velocity.Size() >= RamKnockdownSpeed)
		{
			AActor* Cause = GetDriver() ? static_cast<AActor*>(GetDriver()) : this;
			Victim->ApplyKnockdown(Cause, Velocity.GetSafeNormal() * RamLaunchStrength + FVector(0.f, 0.f, 400.f));
		}
		return;
	}

	if (APVVBoat* OtherBoat = Cast<APVVBoat>(OtherActor))
	{
		const float Now = GetWorld()->GetTimeSeconds();
		const FVector OtherVelocity = OtherBoat->GetHull()->GetPhysicsLinearVelocity();
		if (Now - LastRamTime > 1.f && (Velocity - OtherVelocity).Size() >= RamEjectSpeed
			&& Velocity.SizeSquared() > OtherVelocity.SizeSquared())
		{
			LastRamTime = Now;
			OtherBoat->EjectRandomOccupant();
		}
	}
}

// -------------------------------------------------------------------------------------------
// Seats

APVVCharacter* APVVBoat::GetDriver() const
{
	return Occupants.Num() > 0 ? Occupants[0].Get() : nullptr;
}

float APVVBoat::GetForwardSpeed() const
{
	return FVector::DotProduct(GetVelocity(), GetActorForwardVector());
}

int32 APVVBoat::FindFreePassengerSeat() const
{
	for (int32 i = 1; i < Occupants.Num(); ++i)
	{
		if (!Occupants[i])
		{
			return i;
		}
	}
	return INDEX_NONE;
}

bool APVVBoat::TryBoard(APVVCharacter* Character)
{
	if (!HasAuthority() || !Character || Character->GetCurrentBoat())
	{
		return false;
	}

	const bool bCanDrive = Character->GetCondition() == EPVVCondition::Normal && Character->GetController() && CanDrive(Character);
	int32 Seat = (bCanDrive && Occupants.Num() > 0 && !Occupants[0]) ? 0 : FindFreePassengerSeat();
	if (Seat == INDEX_NONE)
	{
		return false;
	}
	SeatCharacter(Character, Seat);
	return true;
}

bool APVVBoat::LoadPrisoner(APVVCharacter* Carrier)
{
	if (!HasAuthority() || !Carrier)
	{
		return false;
	}
	const int32 Seat = FindFreePassengerSeat();
	if (Seat == INDEX_NONE)
	{
		return false;
	}
	APVVCharacter* Prisoner = Carrier->TakeCarried();
	if (!Prisoner)
	{
		return false;
	}
	SeatCharacter(Prisoner, Seat);
	return true;
}

void APVVBoat::SeatCharacter(APVVCharacter* Character, int32 SeatIndex)
{
	Occupants[SeatIndex] = Character;
	AController* CharacterController = Character->GetController();
	Character->EnterSeat(this, SeatRoot, SeatOffsets[SeatIndex]);

	if (SeatIndex == 0 && CharacterController)
	{
		CharacterController->Possess(this);
	}
	OnRep_Occupants();
	ForceNetUpdate();
}

void APVVBoat::Unseat(APVVCharacter* Character)
{
	const int32 SeatIndex = Occupants.IndexOfByKey(Character);
	if (!HasAuthority() || SeatIndex == INDEX_NONE)
	{
		return;
	}

	AController* DriverController = SeatIndex == 0 ? GetController() : nullptr;
	ClearSeat(SeatIndex);

	// Hop out over the side nearest the seat.
	const FVector SeatWorld = SeatRoot->GetComponentTransform().TransformPosition(SeatOffsets[SeatIndex]);
	const float Side = SeatOffsets[SeatIndex].Y >= 0.f ? 1.f : -1.f;
	const FVector Exit = SeatWorld + GetActorRightVector().GetSafeNormal2D() * Side * 180.f + FVector(0.f, 0.f, 40.f);
	Character->ExitSeat(Exit, GetActorRotation().Yaw);

	if (DriverController)
	{
		DriverController->UnPossess();
		if (!Character->IsActorBeingDestroyed())
		{
			DriverController->Possess(Character);
		}
	}
}

void APVVBoat::ClearSeat(int32 SeatIndex)
{
	Occupants[SeatIndex] = nullptr;
	if (SeatIndex == 0)
	{
		ThrottleInput = 0.f;
		SteerInput = 0.f;
	}
	OnRep_Occupants();
	ForceNetUpdate();
}

void APVVBoat::EjectAll()
{
	for (int32 i = Occupants.Num() - 1; i >= 0; --i)
	{
		if (APVVCharacter* Occupant = Occupants[i])
		{
			Unseat(Occupant);
			Occupant->LaunchCharacter(FVector(FMath::FRandRange(-300.f, 300.f), FMath::FRandRange(-300.f, 300.f), 500.f), true, true);
		}
	}
}

void APVVBoat::EjectRandomOccupant()
{
	TArray<APVVCharacter*> Candidates;
	for (APVVCharacter* Occupant : Occupants)
	{
		if (Occupant)
		{
			Candidates.Add(Occupant);
		}
	}
	if (Candidates.Num() > 0)
	{
		APVVCharacter* Unlucky = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
		Unseat(Unlucky);
		Unlucky->LaunchCharacter(GetActorRightVector() * FMath::RandRange(-1, 1) * 500.f + FVector(0.f, 0.f, 700.f), true, true);
	}
}

void APVVBoat::OnRep_Occupants()
{
	OnOccupantsChanged();
}

// -------------------------------------------------------------------------------------------
// Interaction

bool APVVBoat::CanInteract_Implementation(APVVCharacter* Interactor) const
{
	if (!Interactor || Interactor->GetCurrentBoat() || Interactor->GetCondition() != EPVVCondition::Normal)
	{
		return false;
	}
	if (Interactor->GetCarriedCharacter())
	{
		return FindFreePassengerSeat() != INDEX_NONE;
	}
	return (Occupants.Num() > 0 && !Occupants[0]) || FindFreePassengerSeat() != INDEX_NONE;
}

float APVVBoat::GetInteractDuration_Implementation(APVVCharacter* Interactor) const
{
	return 0.f;
}

FText APVVBoat::GetInteractPrompt_Implementation(APVVCharacter* Interactor) const
{
	if (Interactor && Interactor->GetCarriedCharacter())
	{
		return LOCTEXT("Load", "Load prisoner");
	}
	return (Occupants.Num() > 0 && !Occupants[0]) ? LOCTEXT("Drive", "Drive") : LOCTEXT("Board", "Board");
}

void APVVBoat::OnInteract_Implementation(APVVCharacter* Interactor)
{
	if (!Interactor)
	{
		return;
	}
	if (Interactor->GetCarriedCharacter())
	{
		LoadPrisoner(Interactor);
	}
	else
	{
		TryBoard(Interactor);
	}
}

// -------------------------------------------------------------------------------------------
// Input

void APVVBoat::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	EnsureDefaultInput();
	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->ClearAllMappings();
			Subsystem->AddMappingContext(BoatMappingContext, 0);
		}
	}
}

void APVVBoat::EnsureDefaultInput()
{
	if (BoatMappingContext)
	{
		return;
	}

	ThrottleAction = PVVInput::MakeAction(this, EInputActionValueType::Axis1D);
	SteerAction = PVVInput::MakeAction(this, EInputActionValueType::Axis1D);
	LookAction = PVVInput::MakeAction(this, EInputActionValueType::Axis2D);
	ExitAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);

	UInputMappingContext* IMC = NewObject<UInputMappingContext>(this);
	PVVInput::Map(IMC, ThrottleAction, EKeys::W);
	PVVInput::Map(IMC, ThrottleAction, EKeys::S, true);
	PVVInput::Map(IMC, ThrottleAction, EKeys::Gamepad_RightTriggerAxis);
	PVVInput::Map(IMC, ThrottleAction, EKeys::Gamepad_LeftTriggerAxis, true);
	PVVInput::Map(IMC, SteerAction, EKeys::D);
	PVVInput::Map(IMC, SteerAction, EKeys::A, true);
	PVVInput::Map(IMC, SteerAction, EKeys::Gamepad_LeftX);
	PVVInput::Map(IMC, LookAction, EKeys::Mouse2D);
	PVVInput::Map(IMC, LookAction, EKeys::Gamepad_Right2D, false, false, 3.f);
	PVVInput::Map(IMC, ExitAction, EKeys::E);
	PVVInput::Map(IMC, ExitAction, EKeys::Gamepad_FaceButton_Right);
	BoatMappingContext = IMC;
}

void APVVBoat::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	EnsureDefaultInput();
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		return;
	}

	if (ThrottleAction)
	{
		Input->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &APVVBoat::Input_Throttle);
		Input->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &APVVBoat::Input_ThrottleReleased);
	}
	if (SteerAction)
	{
		Input->BindAction(SteerAction, ETriggerEvent::Triggered, this, &APVVBoat::Input_Steer);
		Input->BindAction(SteerAction, ETriggerEvent::Completed, this, &APVVBoat::Input_SteerReleased);
	}
	if (LookAction)
	{
		Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &APVVBoat::Input_Look);
	}
	if (ExitAction)
	{
		Input->BindAction(ExitAction, ETriggerEvent::Started, this, &APVVBoat::Input_Exit);
	}
}

void APVVBoat::Input_Throttle(const FInputActionValue& Value)
{
	ThrottleInput = FMath::Clamp(Value.Get<float>(), -1.f, 1.f);
	SendDriveInput();
}

void APVVBoat::Input_ThrottleReleased()
{
	ThrottleInput = 0.f;
	SendDriveInput();
}

void APVVBoat::Input_Steer(const FInputActionValue& Value)
{
	SteerInput = FMath::Clamp(Value.Get<float>(), -1.f, 1.f);
	SendDriveInput();
}

void APVVBoat::Input_SteerReleased()
{
	SteerInput = 0.f;
	SendDriveInput();
}

void APVVBoat::Input_Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(-Axis.Y);
}

void APVVBoat::Input_Exit()
{
	Server_Exit();
}

void APVVBoat::SendDriveInput()
{
	if (HasAuthority() || (FMath::IsNearlyEqual(ThrottleInput, SentThrottle) && FMath::IsNearlyEqual(SteerInput, SentSteer)))
	{
		return;
	}
	SentThrottle = ThrottleInput;
	SentSteer = SteerInput;
	Server_SetDriveInput(ThrottleInput, SteerInput);
}

void APVVBoat::Server_SetDriveInput_Implementation(float Throttle, float Steer)
{
	ThrottleInput = FMath::Clamp(Throttle, -1.f, 1.f);
	SteerInput = FMath::Clamp(Steer, -1.f, 1.f);
}

void APVVBoat::Server_Exit_Implementation()
{
	if (APVVCharacter* Driver = GetDriver())
	{
		Unseat(Driver);
	}
}

#undef LOCTEXT_NAMESPACE
