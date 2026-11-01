#include "BRDrone.h"
#include "BRCharacter.h"
#include "BRGameState.h"
#include "BRPlayerController.h"
#include "PVVInputUtils.h"
#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "BRDrone"

ABRDrone::ABRDrone()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(60.f);
	SetNetCullDistanceSquared(FMath::Square(50000.f));
	AutoPossessAI = EAutoPossessAI::Disabled;
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(35.f);
	Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Collision);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetRelativeScale3D(FVector(0.6f, 0.6f, 0.15f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Body->SetStaticMesh(Cube.Object);
	}

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Collision);
	Camera->SetRelativeLocation(FVector(0.f, 0.f, -20.f));
	Camera->bUsePawnControlRotation = true;
}

void ABRDrone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABRDrone, Pilot);
	DOREPLIFETIME(ABRDrone, Battery);
}

void ABRDrone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority())
	{
		return;
	}

	// Fly towards the wished direction with a little inertia.
	Velocity = FMath::VInterpTo(Velocity, ServerWish * MaxSpeed, DeltaSeconds, 3.f);
	if (!Velocity.IsNearlyZero())
	{
		FHitResult Hit;
		AddActorWorldOffset(Velocity * DeltaSeconds, true, &Hit);
		if (Hit.bBlockingHit)
		{
			Velocity = FVector::VectorPlaneProject(Velocity, Hit.Normal);
		}
	}

	Battery = FMath::Max(0.f, Battery - DeltaSeconds / BatterySeconds);
	const bool bPilotInTrouble = !Pilot || Pilot->GetCondition() != EPVVCondition::Normal;
	const ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>();
	if (Battery <= 0.f || bPilotInTrouble || (GS && !GS->DronesAllowed()))
	{
		Exit();
	}
}

void ABRDrone::Exit()
{
	if (bExiting)
	{
		return;
	}
	bExiting = true;
	AController* PilotController = GetController();
	if (PilotController)
	{
		PilotController->UnPossess();
		if (Pilot)
		{
			PilotController->Possess(Pilot);
		}
	}
	if (Pilot)
	{
		Pilot->OnDroneFinished();
	}
	Destroy();
}

// -------------------------------------------------------------------------------------------
// Input

void ABRDrone::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	EnsureInput();
	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->ClearAllMappings();
			Subsystem->AddMappingContext(DroneMappingContext, 0);
		}
	}
}

void ABRDrone::EnsureInput()
{
	if (DroneMappingContext)
	{
		return;
	}
	MoveAction = PVVInput::MakeAction(this, EInputActionValueType::Axis2D);
	LiftAction = PVVInput::MakeAction(this, EInputActionValueType::Axis1D);
	LookAction = PVVInput::MakeAction(this, EInputActionValueType::Axis2D);
	TagAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	ExitAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);

	UInputMappingContext* IMC = NewObject<UInputMappingContext>(this);
	PVVInput::Map(IMC, MoveAction, EKeys::W, false, true);
	PVVInput::Map(IMC, MoveAction, EKeys::S, true, true);
	PVVInput::Map(IMC, MoveAction, EKeys::D);
	PVVInput::Map(IMC, MoveAction, EKeys::A, true);
	PVVInput::Map(IMC, MoveAction, EKeys::Gamepad_Left2D);
	PVVInput::Map(IMC, LiftAction, EKeys::SpaceBar);
	PVVInput::Map(IMC, LiftAction, EKeys::LeftControl, true);
	PVVInput::Map(IMC, LiftAction, EKeys::Gamepad_RightTriggerAxis);
	PVVInput::Map(IMC, LiftAction, EKeys::Gamepad_LeftTriggerAxis, true);
	PVVInput::Map(IMC, LookAction, EKeys::Mouse2D);
	PVVInput::Map(IMC, LookAction, EKeys::Gamepad_Right2D, false, false, 3.f);
	PVVInput::Map(IMC, TagAction, EKeys::LeftMouseButton);
	PVVInput::Map(IMC, TagAction, EKeys::Gamepad_FaceButton_Left);
	PVVInput::Map(IMC, ExitAction, EKeys::Q);
	PVVInput::Map(IMC, ExitAction, EKeys::Gamepad_DPad_Up);
	DroneMappingContext = IMC;
}

void ABRDrone::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	EnsureInput();
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		return;
	}
	Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABRDrone::Input_Move);
	Input->BindAction(MoveAction, ETriggerEvent::Completed, this, &ABRDrone::Input_MoveStop);
	Input->BindAction(LiftAction, ETriggerEvent::Triggered, this, &ABRDrone::Input_Lift);
	Input->BindAction(LiftAction, ETriggerEvent::Completed, this, &ABRDrone::Input_LiftStop);
	Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABRDrone::Input_Look);
	Input->BindAction(TagAction, ETriggerEvent::Started, this, &ABRDrone::Input_Tag);
	Input->BindAction(ExitAction, ETriggerEvent::Started, this, &ABRDrone::Input_Exit);
}

void ABRDrone::Input_Move(const FInputActionValue& Value) { MoveInput = Value.Get<FVector2D>(); SendWish(); }
void ABRDrone::Input_MoveStop() { MoveInput = FVector2D::ZeroVector; SendWish(); }
void ABRDrone::Input_Lift(const FInputActionValue& Value) { LiftInput = Value.Get<float>(); SendWish(); }
void ABRDrone::Input_LiftStop() { LiftInput = 0.f; SendWish(); }

void ABRDrone::Input_Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(-Axis.Y);
}

void ABRDrone::SendWish()
{
	const FRotator Yaw(0.f, GetControlRotation().Yaw, 0.f);
	FVector Wish = FRotationMatrix(Yaw).GetUnitAxis(EAxis::X) * MoveInput.Y + FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y) * MoveInput.X + FVector::UpVector * LiftInput;
	Wish = Wish.GetClampedToMaxSize(1.f);
	if (!Wish.Equals(SentWish, 0.05f))
	{
		SentWish = Wish;
		Server_SetWish(Wish);
	}
}

void ABRDrone::Server_SetWish_Implementation(FVector_NetQuantizeNormal Wish)
{
	ServerWish = FVector(Wish).GetClampedToMaxSize(1.f);
}

void ABRDrone::Input_Tag()
{
	Server_Tag(Camera->GetComponentLocation(), Camera->GetForwardVector());
}

void ABRDrone::Server_Tag_Implementation(FVector_NetQuantize Start, FVector_NetQuantizeNormal Direction)
{
	if (FVector::Dist(Start, GetActorLocation()) > 300.f)
	{
		return;
	}
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BRDroneTag), false, this);
	FHitResult Hit;
	// A fat sweep so tagging a moving speck from the sky is fun rather than frustrating.
	if (GetWorld()->SweepSingleByObjectType(Hit, Start, Start + FVector(Direction) * TagRange, FQuat::Identity, Objects, FCollisionShape::MakeSphere(60.f), Params))
	{
		if (ABRCharacter* Target = Cast<ABRCharacter>(Hit.GetActor()); Target && Target->IsRunner())
		{
			Target->Spot(10.f);
			if (ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>())
			{
				GS->Multicast_NoisePing(Target->GetActorLocation());
			}
			if (ABRPlayerController* PC = Cast<ABRPlayerController>(GetController()))
			{
				PC->Client_Notify(LOCTEXT("Tagged", "Runner tagged! Every Warden can see them for 10 s."));
			}
		}
	}
}

void ABRDrone::Input_Exit()
{
	Server_Exit();
}

void ABRDrone::Server_Exit_Implementation()
{
	Exit();
}

#undef LOCTEXT_NAMESPACE
