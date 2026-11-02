#include "BRCharacter.h"
#include "BRCar.h"
#include "BRDesertArena.h"
#include "BRDrone.h"
#include "BRGameMode.h"
#include "BRGameState.h"
#include "BRPlayerController.h"
#include "BRPlayerState.h"
#include "BRWorldActors.h"
#include "PVVBoat.h"
#include "PVVInputUtils.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Engine.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstance.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/TextRenderComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "BRCharacter"

ABRCharacter::ABRCharacter()
{
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
	GetCharacterMovement()->MaxWalkSpeedCrouched = 220.f;

	PhysicalAnimation = CreateDefaultSubobject<UPhysicalAnimationComponent>(TEXT("PhysicalAnimation"));

	auto MakeOutfit = [this](const TCHAR* Name)
	{
		USkeletalMeshComponent* Comp = CreateDefaultSubobject<USkeletalMeshComponent>(Name);
		Comp->SetupAttachment(GetMesh());
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return Comp;
	};
	OutfitTop = MakeOutfit(TEXT("OutfitTop"));
	OutfitBottom = MakeOutfit(TEXT("OutfitBottom"));
	OutfitHead = MakeOutfit(TEXT("OutfitHead"));
	OutfitFace = MakeOutfit(TEXT("OutfitFace"));
	Hair = MakeOutfit(TEXT("Hair"));
	FacialHair = MakeOutfit(TEXT("FacialHair"));

	// Greybox gun (pointing along +X, grip down). Restyled per weapon at runtime.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	WeaponRoot = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponRoot"));
	WeaponRoot->SetupAttachment(GetCapsuleComponent());
	auto GunPart = [this](const TCHAR* Name, UStaticMesh* PartMesh)
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(WeaponRoot);
		C->SetStaticMesh(PartMesh);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		return C;
	};
	GunBody = GunPart(TEXT("GunBody"), Cube.Object);
	GunBarrel = GunPart(TEXT("GunBarrel"), Cylinder.Object);
	GunGrip = GunPart(TEXT("GunGrip"), Cube.Object);
	GunExtra = GunPart(TEXT("GunExtra"), Cylinder.Object);
	WeaponRoot->SetVisibility(false, true);
}

void ABRCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABRCharacter, bIsNPC);
	DOREPLIFETIME(ABRCharacter, Hunger);
	DOREPLIFETIME(ABRCharacter, Visibility);
	DOREPLIFETIME(ABRCharacter, SpottedUntil);
	DOREPLIFETIME(ABRCharacter, Breath);
	DOREPLIFETIME(ABRCharacter, bLethalMode);
	DOREPLIFETIME_CONDITION(ABRCharacter, bAiming, COND_SkipOwner);
	DOREPLIFETIME(ABRCharacter, bReloading);
	DOREPLIFETIME_CONDITION(ABRCharacter, AmmoTaser, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ABRCharacter, AmmoGun, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ABRCharacter, AmmoRunnerGun, COND_OwnerOnly);
	DOREPLIFETIME(ABRCharacter, SlowUntil);
	DOREPLIFETIME(ABRCharacter, SlowFactor);
	DOREPLIFETIME(ABRCharacter, Look);
}

void ABRCharacter::InitAsNPC(float SkinToneMin, float SkinToneMax)
{
	bIsNPC = true;
	DefaultFaction = BR::Runners;
	// Every NPC gets a random look so the crowd isn't all clones (and players can blend in).
	// Ordinary-looking people: small face differences, natural dark/brown hair, plain clothes.
	FBRLook Random;
	Random.SkinTone = FMath::FRandRange(FMath::Clamp(FMath::Min(SkinToneMin, SkinToneMax), 0.f, 1.f), FMath::Clamp(FMath::Max(SkinToneMin, SkinToneMax), 0.f, 1.f));
	Random.ShirtColor = FMath::RandRange(0, 2) == 0 ? 0.f : FMath::FRandRange(0.06f, 1.f);
	Random.PantsColor = FMath::RandRange(0, 1) == 0 ? 0.f : FMath::FRandRange(0.06f, 1.f);
	Random.bHat = FMath::RandRange(0, 2) != 0;
	Random.HeadSize = FMath::FRandRange(.42f, .58f);
	Random.NoseSize = FMath::FRandRange(.35f, .65f);
	Random.LipSize = FMath::FRandRange(.4f, .6f);
	Random.EarSize = FMath::FRandRange(.4f, .6f);
	Random.HairStyle = HairStyles.Num() > 0 ? FMath::RandRange(1, HairStyles.Num()) : 1;
	Random.FacialHair = (FacialHairStyles.Num() > 0 && FMath::RandRange(0, 2) == 0) ? FMath::RandRange(1, FacialHairStyles.Num()) : 0;
	const float HairRoll = FMath::FRand();
	Random.HairColor = HairRoll < 0.5f ? 0.f : (HairRoll < 0.9f ? FMath::FRandRange(.06f, .3f) : FMath::FRandRange(.8f, .95f)); // own colour, dark/brown, or grey
	Random.bSet = true;
	Look = Random;
}

void ABRCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (const ABRPlayerState* PS = GetBRPlayerState(); PS && PS->bHasLook)
	{
		SetLook(PS->Look);
	}
}

void ABRCharacter::SetLook(const FBRLook& NewLook)
{
	if (!HasAuthority())
	{
		return;
	}
	Look = NewLook;
	Look.bSet = true;
	ApplyLook(Look);
	ForceNetUpdate();
}

void ABRCharacter::PreviewLook(const FBRLook& NewLook)
{
	FBRLook L = NewLook;
	L.bSet = true;
	ApplyLook(L);
}

void ABRCharacter::OnRep_Look()
{
	ApplyLook(Look);
}

static bool BRHasVectorParam(const UMaterialInterface* Material, FName Parameter)
{
	FLinearColor Ignored;
	return Material && Material->GetVectorParameterValue(FHashedMaterialParameterInfo(Parameter), Ignored);
}

void ABRCharacter::ApplyLook(const FBRLook& L)
{
	if (!L.bSet || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	PickStyle(Hair, HairStyles, L.HairStyle);
	PickStyle(FacialHair, FacialHairStyles, L.FacialHair);
	ApplySkin(BR::SkinColor(L.SkinTone));
	TintOutfit(OutfitTop, ShirtColorParameters, BR::IsOriginalCloth(L.ShirtColor), BR::ClothColor(L.ShirtColor), ClothMaterialSlot);
	TintOutfit(OutfitBottom, PantsColorParameters, BR::IsOriginalCloth(L.PantsColor), BR::ClothColor(L.PantsColor), ClothMaterialSlot);
	TintOutfit(Hair, HairColorParameters, BR::IsOriginalCloth(L.HairColor), BR::HairColor(L.HairColor));
	TintOutfit(FacialHair, HairColorParameters, BR::IsOriginalCloth(L.HairColor), BR::HairColor(L.HairColor));
	if (OutfitHead)
	{
		OutfitHead->SetHiddenInGame(!L.bHat);
	}
	ApplyFaceShape(L);
}

void ABRCharacter::PickStyle(USkeletalMeshComponent* Part, const TArray<TObjectPtr<USkeletalMesh>>& Styles, int32 Index)
{
	if (!Part || Styles.Num() == 0)
	{
		return; // no list on the Blueprint: leave the component as you set it
	}
	USkeletalMesh* Wanted = nullptr;
	if (Index > 0)
	{
		Wanted = Styles[(Index - 1) % Styles.Num()].Get();
	}
	if (Part->GetSkeletalMeshAsset() != Wanted)
	{
		Part->SetSkeletalMeshAsset(Wanted);
		Part->SetLeaderPoseComponent(GetMesh(), true);
	}
}

void ABRCharacter::ApplyFaceShape(const FBRLook& InLook)
{
	// The body must have the shape keys, or only the hair/hat would grow. Check and tell the player what to fix.
	const USkeletalMesh* BodyMesh = GetMesh() ? GetMesh()->GetSkeletalMeshAsset() : nullptr;
	const bool bBodyHasShapes = BodyMesh && BodyMesh->FindMorphTarget(TEXT("Head_Big")) != nullptr;
	FBRLook L = InLook;
	if (!bBodyHasShapes)
	{
		L.HeadSize = L.NoseSize = L.LipSize = L.EarSize = 0.5f; // keep hair/hats normal size so they still fit
		if (BodyMesh && !bWarnedNoMorphs && IsLocallyControlled() && GEngine)
		{
			bWarnedNoMorphs = true;
			const FString Msg = FString::Printf(TEXT("Face sliders: your body mesh '%s' has NO shape keys (Head_Big, Nose_Big...). ")
				TEXT("Import Art/Characters/SK_PVV_Body_Custom.glb with 'Import Morph Targets' ticked and use THAT mesh on this character's Mesh component."),
				*BodyMesh->GetName());
			GEngine->AddOnScreenDebugMessage(-1, 20.f, FColor::Orange, Msg);
			UE_LOG(LogTemp, Warning, TEXT("%s"), *Msg);
		}
	}
	// Each slider drives a pair of shape keys: left half = "_Small", right half = "_Big", middle = normal.
	auto Pair = [this](const TCHAR* Small, const TCHAR* Big, float Slider)
	{
		const float SmallWeight = FMath::Clamp((0.5f - Slider) * 2.f, 0.f, 1.f);
		const float BigWeight = FMath::Clamp((Slider - 0.5f) * 2.f, 0.f, 1.f);
		for (USkeletalMeshComponent* Part : { GetMesh(), OutfitHead.Get(), OutfitFace.Get(), Hair.Get(), FacialHair.Get() })
		{
			if (Part && Part->GetSkeletalMeshAsset())
			{
				Part->SetMorphTarget(Small, SmallWeight, false);
				Part->SetMorphTarget(Big, BigWeight, false);
			}
		}
	};
	Pair(TEXT("Head_Small"), TEXT("Head_Big"), L.HeadSize);
	Pair(TEXT("Nose_Small"), TEXT("Nose_Big"), L.NoseSize);
	Pair(TEXT("Lips_Small"), TEXT("Lips_Big"), L.LipSize);
	Pair(TEXT("Ears_Small"), TEXT("Ears_Big"), L.EarSize);
}

void ABRCharacter::ApplySkin(const FLinearColor& Color)
{
	USkeletalMeshComponent* Body = GetMesh();
	if (!Body || !Body->GetSkeletalMeshAsset())
	{
		return;
	}
	// Which slots are skin? Match by slot/material name ("M_Skin"), so eyes, lips, shoes and clothes are left alone.
	// Worked out once per mesh (after we recolour a slot its material name changes).
	TArray<int32>& SkinSlots = SkinSlotsCache;
	if (SkinSlotsMesh.Get() != Body->GetSkeletalMeshAsset())
	{
	SkinSlotsMesh = Body->GetSkeletalMeshAsset();
	SkinSlots.Reset();
	const TArray<FName> SlotNames = Body->GetMaterialSlotNames();
	for (int32 Index = 0; Index < Body->GetNumMaterials(); ++Index)
	{
		const UMaterialInterface* Material = Body->GetMaterial(Index);
		FString Names = SlotNames.IsValidIndex(Index) ? SlotNames[Index].ToString() : FString();
		if (Material)
		{
			Names += TEXT(" ") + Material->GetName();
			if (const UMaterialInstance* Instance = Cast<UMaterialInstance>(Material); Instance && Instance->Parent)
			{
				Names += TEXT(" ") + Instance->Parent->GetName(); // a coloured copy keeps its original's name here
			}
		}
		for (const FString& Word : SkinSlotKeywords)
		{
			if (!Word.IsEmpty() && Names.Contains(Word))
			{
				SkinSlots.Add(Index);
				break;
			}
		}
	}
	if (SkinSlots.Num() == 0 && SkinMaterialSlot >= 0 && SkinMaterialSlot < Body->GetNumMaterials())
	{
		SkinSlots.Add(SkinMaterialSlot);
	}
	}

	for (const int32 Index : SkinSlots)
	{
		bool bFound = false;
		for (const FName& Parameter : SkinColorParameters)
		{
			if (BRHasVectorParam(Body->GetMaterial(Index), Parameter))
			{
				if (UMaterialInstanceDynamic* MID = Body->CreateDynamicMaterialInstance(Index))
				{
					MID->SetVectorParameterValue(Parameter, Color);
					bFound = true;
				}
			}
		}
		if (!bFound && bAutoSkinMaterial)
		{
			// No colour parameter on your skin material: use a plain colour material for that slot.
			if (!SkinFallbackMID)
			{
				if (UMaterialInterface* Plain = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
				{
					SkinFallbackMID = UMaterialInstanceDynamic::Create(Plain, this);
				}
			}
			if (SkinFallbackMID)
			{
				SkinFallbackMID->SetVectorParameterValue(TEXT("Color"), Color);
				Body->SetMaterial(Index, SkinFallbackMID);
			}
		}
	}
}

void ABRCharacter::TintOutfit(USkeletalMeshComponent* Part, const TArray<FName>& Parameters, bool bOriginal, const FLinearColor& Color, int32 OnlySlot)
{
	if (!Part || !Part->GetSkeletalMeshAsset())
	{
		return;
	}
	for (int32 Index = 0; Index < Part->GetNumMaterials(); ++Index)
	{
		if (OnlySlot >= 0 && Index != OnlySlot)
		{
			continue;
		}
		UMaterialInterface* Material = Part->GetMaterial(Index);
		if (bOriginal)
		{
			if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Material))
			{
				MID->ClearParameterValues(); // back to the outfit's own colours
			}
			continue;
		}
		for (const FName& Parameter : Parameters)
		{
			if (BRHasVectorParam(Material, Parameter))
			{
				if (UMaterialInstanceDynamic* MID = Part->CreateDynamicMaterialInstance(Index))
				{
					MID->SetVectorParameterValue(Parameter, Color);
				}
			}
		}
	}
}

bool ABRCharacter::IsDriving() const
{
	const APVVBoat* Vehicle = GetCurrentBoat();
	return Vehicle && Vehicle->GetDriver() == this;
}

bool ABRCharacter::WantsStiffArms() const
{
	return bAiming || (IsDriving() && (DriveAnim || SeatedAnim));
}

void ABRCharacter::StartWandering(const FVector& Home, float Radius)
{
	bWandering = true;
	WanderHome = Home;
	WanderRadius = Radius;
	WanderTarget = GetActorLocation();
	GetCharacterMovement()->bRunPhysicsWithNoController = true;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->MaxWalkSpeed = WanderSpeed;
}

void ABRCharacter::TickWander(float DeltaSeconds)
{
	if (!bWandering || Controller || GetCurrentBoat() || GetCarrier() || GetCondition() != EPVVCondition::Normal)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < WanderPauseUntil)
	{
		return; // standing around chatting
	}
	const FVector ToTarget = (WanderTarget - GetActorLocation()) * FVector(1.f, 1.f, 0.f);
	if (ToTarget.Size() < 80.f || Now > WanderGiveUpAt)
	{
		// Arrived (or stuck on a wall): hang out for a bit, then pick a new spot near home.
		WanderPauseUntil = Now + FMath::FRandRange(2.f, 9.f);
		const FVector2D Offset = FVector2D(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f)) * WanderRadius;
		WanderTarget = WanderHome + FVector(Offset.X, Offset.Y, 0.f);
		WanderGiveUpAt = WanderPauseUntil + 20.f;
		return;
	}
	if (GetCharacterMovement()->MaxWalkSpeed > WanderSpeed)
	{
		GetCharacterMovement()->MaxWalkSpeed = WanderSpeed;
	}
	AddMovementInput(ToTarget.GetSafeNormal(), 1.f);
}

void ABRCharacter::BeginPlay()
{
	Super::BeginPlay();

	for (USkeletalMeshComponent* Outfit : { OutfitTop.Get(), OutfitBottom.Get(), OutfitHead.Get(), OutfitFace.Get(), Hair.Get(), FacialHair.Get() })
	{
		// Clothes copy every bone of the body (incl. physics/ragdoll) and sit exactly on it.
		Outfit->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Outfit->SetRelativeTransform(FTransform::Identity);
		Outfit->SetLeaderPoseComponent(GetMesh(), true);
		Outfit->bUseBoundsFromLeaderPoseComponent = true;
	}
	DefaultArmLength = CameraBoom->TargetArmLength;
	DefaultSocketOffset = CameraBoom->SocketOffset;
	DefaultFOV = FollowCamera->FieldOfView;
	EnableWobble();
	ApplyLook(Look);
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

void ABRCharacter::Notify(const FText& Message) const
{
	if (ABRPlayerController* PC = Cast<ABRPlayerController>(GetController()))
	{
		PC->Client_Notify(Message);
	}
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
		SpottedUntil = FMath::Max(SpottedUntil, GS->GetServerWorldTimeSeconds() + Seconds);
	}
}

bool ABRCharacter::IsSlowed() const
{
	const ABRGameState* GS = GetBRGameState();
	return GS && GS->GetServerWorldTimeSeconds() < SlowUntil;
}

void ABRCharacter::ApplySlow(float Seconds, float SpeedFactor)
{
	if (const ABRGameState* GS = GetBRGameState())
	{
		SlowUntil = GS->GetServerWorldTimeSeconds() + Seconds;
		SlowFactor = FMath::Clamp(SpeedFactor, 0.f, 1.f);
		UpdateMovementSpeed();
	}
}

void ABRCharacter::OnRep_Slow()
{
	UpdateMovementSpeed();
}

void ABRCharacter::UpdateMovementSpeed()
{
	Super::UpdateMovementSpeed();

	UCharacterMovementComponent* Move = GetCharacterMovement();
	float Multiplier = 1.f;
	if (bInDeepWater)
	{
		Multiplier *= DeepWaterSpeedMultiplier;
	}
	if (IsSlowed())
	{
		Multiplier *= SlowFactor;
	}
	if (Hunger < WeakHunger && IsSprinting())
	{
		Move->MaxWalkSpeed = FMath::Min(Move->MaxWalkSpeed, WalkSpeed);
	}
	if (bAiming)
	{
		Move->MaxWalkSpeed = FMath::Min(Move->MaxWalkSpeed, WalkSpeed * 0.6f);
	}
	Move->MaxWalkSpeed *= Multiplier;
	Move->MaxWalkSpeedCrouched = 220.f * Multiplier;
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

	AimAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	ReloadAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	CrouchAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	EatAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	DroneAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	WeaponModeAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	ScanAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	TrapAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);

	UInputMappingContext* IMC = DefaultMappingContext;
	PVVInput::Map(IMC, AimAction, EKeys::RightMouseButton);
	PVVInput::Map(IMC, AimAction, EKeys::Gamepad_LeftTrigger);
	PVVInput::Map(IMC, ReloadAction, EKeys::R);
	PVVInput::Map(IMC, ReloadAction, EKeys::Gamepad_FaceButton_Top);
	PVVInput::Map(IMC, CrouchAction, EKeys::C);
	PVVInput::Map(IMC, CrouchAction, EKeys::LeftControl);
	PVVInput::Map(IMC, CrouchAction, EKeys::Gamepad_RightThumbstick);
	PVVInput::Map(IMC, EatAction, EKeys::F);
	PVVInput::Map(IMC, EatAction, EKeys::Gamepad_DPad_Down);
	PVVInput::Map(IMC, DroneAction, EKeys::Q);
	PVVInput::Map(IMC, DroneAction, EKeys::Gamepad_DPad_Up);
	PVVInput::Map(IMC, WeaponModeAction, EKeys::X);
	PVVInput::Map(IMC, WeaponModeAction, EKeys::Gamepad_DPad_Right);
	PVVInput::Map(IMC, ScanAction, EKeys::V);
	PVVInput::Map(IMC, ScanAction, EKeys::Gamepad_DPad_Left);
	PVVInput::Map(IMC, TrapAction, EKeys::B);
	PVVInput::Map(IMC, TrapAction, EKeys::Gamepad_LeftShoulder);
	// Fire is the existing Attack action (left mouse / gamepad X) while aiming; also right trigger.
	PVVInput::Map(IMC, AttackAction, EKeys::Gamepad_RightTrigger);
}

void ABRCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		return;
	}
	if (AimAction)
	{
		Input->BindAction(AimAction, ETriggerEvent::Started, this, &ABRCharacter::Input_AimStart);
		Input->BindAction(AimAction, ETriggerEvent::Completed, this, &ABRCharacter::Input_AimStop);
	}
	if (AttackAction) Input->BindAction(AttackAction, ETriggerEvent::Started, this, &ABRCharacter::Input_FireIfAiming);
	if (ReloadAction) Input->BindAction(ReloadAction, ETriggerEvent::Started, this, &ABRCharacter::Input_Reload);
	if (CrouchAction) Input->BindAction(CrouchAction, ETriggerEvent::Started, this, &ABRCharacter::Input_Crouch);
	if (EatAction) Input->BindAction(EatAction, ETriggerEvent::Started, this, &ABRCharacter::Input_Eat);
	if (DroneAction) Input->BindAction(DroneAction, ETriggerEvent::Started, this, &ABRCharacter::Input_Drone);
	if (WeaponModeAction) Input->BindAction(WeaponModeAction, ETriggerEvent::Started, this, &ABRCharacter::Input_WeaponMode);
	if (ScanAction) Input->BindAction(ScanAction, ETriggerEvent::Started, this, &ABRCharacter::Input_Scan);
	if (TrapAction) Input->BindAction(TrapAction, ETriggerEvent::Started, this, &ABRCharacter::Input_Trap);
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

void ABRCharacter::Input_Eat() { Server_Eat(); }
void ABRCharacter::Input_Drone() { Server_LaunchDrone(); }
void ABRCharacter::Input_WeaponMode() { Server_ToggleWeaponMode(); }
void ABRCharacter::Input_Trap() { Server_PlaceTrap(); }

void ABRCharacter::Input_Scan()
{
	if (IsWarden())
	{
		Server_Scan(FollowCamera->GetComponentLocation(), FollowCamera->GetForwardVector());
	}
}

// -------------------------------------------------------------------------------------------
// Weapons

bool ABRCharacter::HasWeapon() const
{
	if (bIsNPC)
	{
		return false;
	}
	if (IsWarden())
	{
		return true;
	}
	const ABRPlayerState* PS = GetBRPlayerState();
	return PS && PS->HasItem(EBRItem::IllegalGun);
}

int32& ABRCharacter::AmmoRef()
{
	if (IsRunner())
	{
		return AmmoRunnerGun;
	}
	return bLethalMode ? AmmoGun : AmmoTaser;
}

int32 ABRCharacter::GetAmmo() const
{
	if (IsRunner())
	{
		return AmmoRunnerGun;
	}
	return bLethalMode ? AmmoGun : AmmoTaser;
}

int32 ABRCharacter::GetMagSize() const
{
	if (IsRunner())
	{
		return 6;
	}
	return bLethalMode ? 12 : 1;
}

FString ABRCharacter::GetWeaponName() const
{
	if (IsRunner())
	{
		return TEXT("BLACK-MARKET REVOLVER");
	}
	return bLethalMode ? TEXT("SERVICE PISTOL (lethal)") : TEXT("TASER");
}

float ABRCharacter::FireCooldown() const
{
	if (IsRunner())
	{
		return 0.35f;
	}
	return bLethalMode ? 0.2f : 0.8f;
}

float ABRCharacter::ReloadSeconds() const
{
	if (IsRunner())
	{
		return 2.2f;
	}
	return bLethalMode ? 1.8f : 1.4f;
}

void ABRCharacter::Input_AimStart()
{
	if (!HasWeapon() || !CanAttack())
	{
		return;
	}
	bAiming = true;
	ApplyAimRotationMode();
	UpdateMovementSpeed();
	Server_SetAiming(true);
}

void ABRCharacter::Input_AimStop()
{
	if (!bAiming)
	{
		return;
	}
	bAiming = false;
	ApplyAimRotationMode();
	UpdateMovementSpeed();
	Server_SetAiming(false);
}

void ABRCharacter::Server_SetAiming_Implementation(bool bNewAiming)
{
	bAiming = bNewAiming && HasWeapon();
	ApplyAimRotationMode();
	UpdateMovementSpeed();
}

void ABRCharacter::OnRep_Aiming()
{
	ApplyAimRotationMode();
}

void ABRCharacter::ApplyAimRotationMode()
{
	// Aiming: face where the camera looks (over-the-shoulder). Otherwise: face where you run.
	GetCharacterMovement()->bOrientRotationToMovement = !bAiming;
	bUseControllerRotationYaw = bAiming;
}

void ABRCharacter::UpdateAimCamera(float DeltaSeconds)
{
	const bool bZoom = bAiming && CanAttack();
	const float Speed = 12.f;
	CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, bZoom ? AimArmLength : DefaultArmLength, DeltaSeconds, Speed);
	CameraBoom->SocketOffset = FMath::VInterpTo(CameraBoom->SocketOffset, bZoom ? FVector(0.f, 55.f, 65.f) : DefaultSocketOffset, DeltaSeconds, Speed);
	FollowCamera->SetFieldOfView(FMath::FInterpTo(FollowCamera->FieldOfView, bZoom ? AimFOV : DefaultFOV, DeltaSeconds, Speed));
}

void ABRCharacter::Input_FireIfAiming()
{
	if (!bAiming || !CanAttack() || !HasWeapon())
	{
		return;
	}
	if (GetAmmo() <= 0)
	{
		Input_Reload();
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (bReloading || Now - LastLocalFireTime < FireCooldown())
	{
		return;
	}
	LastLocalFireTime = Now;

	const FVector Direction = FMath::VRandCone(FollowCamera->GetForwardVector(), FMath::DegreesToRadians(AimSpreadDegrees));
	Server_Fire(FollowCamera->GetComponentLocation(), Direction);

	// Recoil kick (local camera only).
	AddControllerPitchInput(-RecoilPitch * FMath::FRandRange(0.7f, 1.2f));
	AddControllerYawInput(FMath::FRandRange(-0.3f, 0.3f));
}

void ABRCharacter::Input_Reload()
{
	Server_Reload();
}

void ABRCharacter::Server_Reload_Implementation()
{
	if (!HasWeapon() || bReloading || GetAmmo() >= GetMagSize())
	{
		return;
	}
	bReloading = true;
	GetWorldTimerManager().SetTimer(ReloadTimer, this, &ABRCharacter::FinishReload, ReloadSeconds(), false);
	Multicast_ReloadStarted();
	if (ReloadSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ReloadSound, GetActorLocation());
	}
}

void ABRCharacter::FinishReload()
{
	bReloading = false;
	AmmoRef() = GetMagSize();
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
		Notify(LOCTEXT("GunsRevoked", "Approval too low - your guns were confiscated. Taser only."));
		return;
	}
	GetWorldTimerManager().ClearTimer(ReloadTimer);
	bReloading = false;
	bLethalMode = !bLethalMode;
	Notify(bLethalMode ? LOCTEXT("Lethal", "SERVICE PISTOL. Shooting someone who hasn't crossed = SCANDAL.")
		: LOCTEXT("Taser", "TASER. Knocks them down so you can cuff them."));
}

void ABRCharacter::Server_Fire_Implementation(FVector_NetQuantize Start, FVector_NetQuantizeNormal Direction)
{
	ABRPlayerState* PS = GetBRPlayerState();
	ABRGameState* GS = GetBRGameState();
	if (!CanAttack() || !PS || !GS || !HasWeapon() || bReloading || FVector::Dist(Start, GetActorLocation()) > 900.f)
	{
		return;
	}
	const bool bRunnerGun = IsRunner();
	const bool bLethal = IsWarden() && bLethalMode && GS->LethalAllowed();
	const float Now = GetWorld()->GetTimeSeconds();
	int32& Ammo = AmmoRef();
	if (Ammo <= 0 || Now - LastFireTime < FireCooldown() * 0.8f)
	{
		return;
	}
	LastFireTime = Now;
	--Ammo;

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
	if (GetCurrentBoat())
	{
		Params.AddIgnoredActor(GetCurrentBoat());
	}

	const FVector End = Start + FVector(Direction) * WeaponRange;
	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, Objects, Params);
	APVVCharacter* Victim = bHit ? Cast<APVVCharacter>(Hit.GetActor()) : nullptr;
	const bool bHostileHit = Victim && PVV::IsHostile(GetFaction(), Victim->GetFaction());

	const FVector Muzzle = GetMuzzleLocation();
	Multicast_Shot(Muzzle, bHit ? FVector(Hit.ImpactPoint) : End, bLethal || bRunnerGun, bHostileHit);
	if (bLethal || bRunnerGun)
	{
		GS->Multicast_NoisePing(GetActorLocation());
	}
	if (!bHostileHit)
	{
		return;
	}
	if (ABRPlayerController* PC = Cast<ABRPlayerController>(GetController()))
	{
		PC->Client_HitMarker(bLethal);
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

void ABRCharacter::Multicast_ReloadStarted_Implementation()
{
	PlayOneShot(ReloadAnim);
	OnReloadStarted();
}

void ABRCharacter::Multicast_Ate_Implementation()
{
	PlayOneShot(EatAnim);
	OnAte();
}

void ABRCharacter::OnAttackStartedNative()
{
	PlayOneShot(PunchAnim);
}

bool ABRCharacter::UsesAnimationSlots() const
{
	const USkeletalMeshComponent* MeshComp = GetMesh();
	return MeshComp && MeshComp->GetSkeletalMeshAsset() && !MeshComp->AnimClass && (IdleAnim || WalkAnim || RunAnim);
}

void ABRCharacter::PlayOneShot(UAnimSequenceBase* Anim)
{
	if (!Anim || !UsesAnimationSlots() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	GetMesh()->PlayAnimation(Anim, false);
	CurrentSlotAnim = Anim;
	OneShotUntil = GetWorld()->GetTimeSeconds() + FMath::Max(0.1f, Anim->GetPlayLength() - 0.05f);
}

void ABRCharacter::TickAnimationSlots()
{
	if (!UsesAnimationSlots() || GetNetMode() == NM_DedicatedServer || GetCondition() == EPVVCondition::KnockedDown)
	{
		return; // knocked down = ragdoll, no animation
	}
	if (GetWorld()->GetTimeSeconds() < OneShotUntil)
	{
		return; // let the punch / reload / eat finish
	}

	const float Speed = GetVelocity().Size2D();
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	auto Pick = [](UAnimSequenceBase* Preferred, UAnimSequenceBase* Fallback) { return Preferred ? Preferred : Fallback; };
	UAnimSequenceBase* Locomotion = Speed > RunAnimAboveSpeed ? Pick(RunAnim, WalkAnim) : (Speed > 15.f ? Pick(WalkAnim, IdleAnim) : IdleAnim.Get());

	UAnimSequenceBase* Wanted = nullptr;
	if (GetCarrier()) Wanted = Pick(CarriedAnim, IdleAnim);
	else if (GetCurrentBoat()) Wanted = (IsDriving() && DriveAnim) ? DriveAnim.Get() : Pick(SeatedAnim, IdleAnim);
	else if (GetCondition() == EPVVCondition::Restrained) Wanted = (GetStruggleProgress() > 0.f && StruggleAnim) ? StruggleAnim.Get() : Pick(RestrainedAnim, Locomotion);
	else if (bInDeepWater) Wanted = Pick(SwimAnim, Locomotion);
	else if (Move && Move->IsFalling()) Wanted = Pick(FallAnim, Locomotion);
	else if (GetHoldProgress() > 0.f) Wanted = Pick(WorkAnim, Locomotion);
	else if (bAiming) Wanted = (Speed > 15.f && AimWalkAnim) ? AimWalkAnim.Get() : Pick(AimAnim, Locomotion);
	else if (bIsCrouched) Wanted = Speed > 15.f ? Pick(CrouchWalkAnim, Locomotion) : Pick(CrouchIdleAnim, Locomotion);
	else Wanted = Locomotion;

	if (Wanted && Wanted != CurrentSlotAnim)
	{
		GetMesh()->PlayAnimation(Wanted, true);
		CurrentSlotAnim = Wanted;
	}
}

void ABRCharacter::Multicast_Shot_Implementation(FVector_NetQuantize Start, FVector_NetQuantize End, bool bLethal, bool bHitSomeone)
{
	PlayOneShot(FireAnim);
	OnWeaponFired(bLethal);
	// Recoil kick through the physics arms.
	if (bWobbleOn && GetMesh())
	{
		GetMesh()->AddImpulseToAllBodiesBelow(-GetActorForwardVector() * 60.f + FVector(0.f, 0.f, 40.f), TEXT("upperarm_r"), true, true);
	}
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (ABRTracerFX* FX = GetWorld()->SpawnActor<ABRTracerFX>(ABRTracerFX::StaticClass(), FTransform(Start)))
	{
		FX->Init(Start, End, bLethal, bHitSomeone);
	}
	if (USoundBase* Sound = bLethal ? GunSound.Get() : TaserSound.Get())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Start);
	}
}

// -------------------------------------------------------------------------------------------
// Other server actions

void ABRCharacter::Server_Eat_Implementation()
{
	ABRPlayerState* PS = GetBRPlayerState();
	if (PS && Hunger < 95.f && PS->RemoveItem(EBRItem::Taco))
	{
		Eat(TacoFood);
		Multicast_Ate();
		Notify(LOCTEXT("Ate", "Delicious. (+food)"));
	}
	else if (PS && !PS->HasItem(EBRItem::Taco))
	{
		Notify(LOCTEXT("NoFood", "No tacos! Buy some at the taco stand."));
	}
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
		Notify(FText::Format(LOCTEXT("DroneCharging", "Drone recharging ({0}s)"), FMath::CeilToInt(DroneCooldown - (Now - LastDroneTime))));
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
	bAiming = false;
	ApplyAimRotationMode();
	if (bIsCrouched)
	{
		UnCrouch();
	}
	Drone->Pilot = this;
	MyController->Possess(Drone);
}

void ABRCharacter::OnDroneFinished()
{
	bInDrone = false;
	LastDroneTime = GetWorld()->GetTimeSeconds();
}

void ABRCharacter::Server_Scan_Implementation(FVector_NetQuantize Start, FVector_NetQuantizeNormal Direction)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (!IsWarden() || Now - LastScanTime < 2.5f || FVector::Dist(Start, GetActorLocation()) > 900.f)
	{
		return;
	}
	LastScanTime = Now;

	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_Vehicle);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BRScan), false, this);
	FHitResult Hit;
	if (!GetWorld()->SweepSingleByObjectType(Hit, Start, Start + FVector(Direction) * 3000.f, FQuat::Identity, Objects, FCollisionShape::MakeSphere(50.f), Params))
	{
		Notify(LOCTEXT("ScanNothing", "SCAN: nothing there."));
		return;
	}

	if (const APVVBoat* Vehicle = Cast<APVVBoat>(Hit.GetActor()))
	{
		int32 People = 0, Players = 0;
		for (APVVCharacter* Occupant : Vehicle->GetOccupants())
		{
			if (!Occupant) continue;
			++People;
			const ABRCharacter* BRC = Cast<ABRCharacter>(Occupant);
			if (BRC && !BRC->IsNPC() && BRC->IsRunner())
			{
				++Players;
				const_cast<ABRCharacter*>(BRC)->Spot(6.f);
			}
		}
		Notify(Players > 0
			? FText::Format(LOCTEXT("ScanAnomaly", "SCAN: {0} aboard - {1} HEAT ANOMALY! (tagged)"), People, Players)
			: FText::Format(LOCTEXT("ScanClean", "SCAN: {0} aboard - all regular citizens."), People));
		return;
	}
	if (ABRCharacter* Target = Cast<ABRCharacter>(Hit.GetActor()))
	{
		if (Target->IsRunner() && !Target->IsNPC())
		{
			Target->Spot(6.f);
			Notify(LOCTEXT("ScanRunner", "SCAN: RUNNER! (tagged)"));
		}
		else
		{
			Notify(Target->IsNPC() ? LOCTEXT("ScanCitizen", "SCAN: regular citizen. Leave them alone.") : LOCTEXT("ScanWarden", "SCAN: that's a fellow Warden."));
		}
	}
}

void ABRCharacter::Server_PlaceTrap_Implementation()
{
	if (!IsWarden() || GetCondition() != EPVVCondition::Normal || GetCurrentBoat() || GetCarriedCharacter())
	{
		return;
	}
	int32 Count = 0;
	for (TActorIterator<ABRTrap> It(GetWorld()); It; ++It)
	{
		++Count;
	}
	if (Count >= MaxTeamTraps)
	{
		Notify(FText::Format(LOCTEXT("TrapLimit", "Trap limit reached ({0}). Wait for some to be sprung."), MaxTeamTraps));
		return;
	}

	const EBRTrap Type = static_cast<EBRTrap>(NextTrapType % 3);
	NextTrapType++;
	const FVector Feet = GetActorLocation() + GetActorForwardVector() * 90.f - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const FTransform Where(FRotator(0.f, GetActorRotation().Yaw, 0.f), Feet);
	if (ABRTrap* Trap = GetWorld()->SpawnActorDeferred<ABRTrap>(ABRTrap::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Trap->Type = Type;
		Trap->FinishSpawning(Where);
		const TCHAR* Names[] = { TEXT("BEAR TRAP"), TEXT("GLUE PAD"), TEXT("ALARM TRIPWIRE") };
		Notify(FText::Format(LOCTEXT("TrapPlaced", "Placed {0} ({1}/{2}). Press B again for the next type."),
			FText::FromString(Names[static_cast<int32>(Type)]), Count + 1, MaxTeamTraps));
	}
}

// -------------------------------------------------------------------------------------------
// Physics body

void ABRCharacter::EnableWobble()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!bWobblyBody || !MeshComp || !MeshComp->GetSkeletalMeshAsset() || !MeshComp->GetPhysicsAsset() || MeshComp->GetBoneIndex(TEXT("pelvis")) == INDEX_NONE)
	{
		return;
	}
	MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	MeshComp->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Ignore);
	MeshComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	PhysicalAnimation->SetSkeletalMeshComponent(MeshComp);
	// Simulate from the spine up (legs stay on the animation), or the whole body below the pelvis.
	const FName SimRoot = bWobblyLegs ? FName(TEXT("pelvis")) : FName(TEXT("spine_01"));
	MeshComp->SetAllBodiesBelowSimulatePhysics(SimRoot, true, !bWobblyLegs);
	MeshComp->SetAllBodiesBelowPhysicsBlendWeight(SimRoot, 1.f, false, !bWobblyLegs);
	bWobbleOn = true;
	bArmsStiff = WantsStiffArms();
	ApplyMuscles(bArmsStiff ? (bAiming ? AimArmStrength : DriveArmStrength) : ArmStrength);
}

void ABRCharacter::ApplyMuscles(float ArmMuscle)
{
	auto Muscle = [this](float Strength)
	{
		FPhysicalAnimationData Data;
		Data.bIsLocalSimulation = false;
		Data.OrientationStrength = Strength * WobbleStrength;
		Data.AngularVelocityStrength = Strength * WobbleStrength * 0.1f;
		Data.PositionStrength = 0.f;
		Data.VelocityStrength = 0.f;
		return Data;
	};
	// Order matters: each call overrides the bodies below that bone.
	PhysicalAnimation->ApplyPhysicalAnimationSettingsBelow(TEXT("spine_01"), Muscle(SpineStrength), true);
	PhysicalAnimation->ApplyPhysicalAnimationSettingsBelow(TEXT("neck_01"), Muscle(HeadStrength), true);
	PhysicalAnimation->ApplyPhysicalAnimationSettingsBelow(TEXT("clavicle_l"), Muscle(ArmMuscle), true);
	PhysicalAnimation->ApplyPhysicalAnimationSettingsBelow(TEXT("clavicle_r"), Muscle(ArmMuscle), true);
	if (bWobblyLegs)
	{
		// Legs also get a position pull so the feet track the walk cycle.
		FPhysicalAnimationData Legs = Muscle(LegStrength);
		Legs.PositionStrength = LegStrength * WobbleStrength * 0.5f;
		Legs.VelocityStrength = LegStrength * WobbleStrength * 0.05f;
		PhysicalAnimation->ApplyPhysicalAnimationSettingsBelow(TEXT("thigh_l"), Legs, true);
		PhysicalAnimation->ApplyPhysicalAnimationSettingsBelow(TEXT("thigh_r"), Legs, true);
	}
}

void ABRCharacter::TickWobble()
{
	if (!bWobbleOn || !GetMesh())
	{
		return;
	}
	// Arms snap to the aim pose while aiming; floppy otherwise.
	if (WantsStiffArms() != bArmsStiff)
	{
		bArmsStiff = WantsStiffArms();
		ApplyMuscles(bArmsStiff ? (bAiming ? AimArmStrength : DriveArmStrength) : ArmStrength);
	}
	// Drunken sway: little random shoves to the upper body.
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextSwayTime && GetCondition() == EPVVCondition::Normal && !bAiming && !IsDriving())
	{
		NextSwayTime = Now + FMath::FRandRange(0.5f, 1.4f);
		const float Woozy = 1.f + (Hunger < WeakHunger ? 1.5f : 0.f) + (1.f - GetBalanceFraction()) * 2.f;
		const FVector Shove = FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f) * SwayStrength * Woozy;
		GetMesh()->AddImpulseToAllBodiesBelow(Shove, TEXT("spine_02"), true, true);
	}
}

void ABRCharacter::OnHitReceivedNative(APVVCharacter* Attacker)
{
	if (!bWobbleOn || !GetMesh())
	{
		return;
	}
	const FVector Dir = Attacker ? (GetActorLocation() - Attacker->GetActorLocation()).GetSafeNormal2D() : -GetActorForwardVector();
	GetMesh()->AddImpulseToAllBodiesBelow(Dir * HitWobble + FVector(0.f, 0.f, HitWobble * 0.3f), TEXT("spine_01"), true, true);
}

void ABRCharacter::OnStateEffectsRefreshed()
{
	if (!PhysicalAnimation || !GetMesh() || !GetMesh()->GetPhysicsAsset())
	{
		return;
	}
	const bool bRagdolling = GetCondition() == EPVVCondition::KnockedDown && !GetCarrier() && !GetCurrentBoat();
	if (bRagdolling)
	{
		// Go fully limp: no muscles while knocked down.
		PhysicalAnimation->ApplyPhysicalAnimationSettingsBelow(TEXT("pelvis"), FPhysicalAnimationData(), false);
		bWobbleOn = false;
	}
	else if (!bWobbleOn || !GetMesh()->IsSimulatingPhysics(TEXT("spine_02")))
	{
		EnableWobble();
	}
}

// -------------------------------------------------------------------------------------------
// Tick

void ABRCharacter::Drown()
{
	Breath = 1.f;
	if (GetCondition() == EPVVCondition::Normal || GetCondition() == EPVVCondition::Restrained)
	{
		if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
		{
			GM->HandleDrowned(this);
		}
	}
}

void ABRCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ABRGameState* GS = GetBRGameState();
	ABRDesertArena* MyArena = FindArena();
	const FVector Location = GetActorLocation();
	const bool bFree = !GetCurrentBoat() && !GetCarrier();
	const bool bInRiver = MyArena && bFree && MyArena->IsInRiver(Location);
	const bool bDeep = bInRiver && MyArena->IsDeepWater(Location);
	if (bDeep != bInDeepWater)
	{
		bInDeepWater = bDeep;
		UpdateMovementSpeed();
	}
	const bool bSlowedNow = IsSlowed();
	if (bSlowedNow != bSlowApplied)
	{
		bSlowApplied = bSlowedNow;
		UpdateMovementSpeed();
	}

	if (HasAuthority())
	{
		TickWander(DeltaSeconds);
	}

	if (HasAuthority() && GS)
	{
		const float Now = GetWorld()->GetTimeSeconds();

		// Hunger (Runner players only - Wardens are government-fed, NPCs eat at home).
		if (IsRunner() && !bIsNPC && GetBRPlayerState() && GetCondition() != EPVVCondition::Detained && GS->Phase != EBRPhase::Ended)
		{
			const float Drain = HungerPerSecond * (IsSprinting() ? 1.8f : 1.f) * (bInRiver ? 2.f : 1.f) * (GS->IsNight() ? 0.8f : 1.f);
			const float Before = Hunger;
			Hunger = FMath::Max(0.f, Hunger - Drain * DeltaSeconds);
			if ((Before >= WeakHunger) != (Hunger >= WeakHunger))
			{
				UpdateMovementSpeed();
			}
			if (Hunger <= 0.f && GetCondition() == EPVVCondition::Normal && Now - LastFaintTime > 45.f)
			{
				LastFaintTime = Now;
				Notify(LOCTEXT("Fainted", "You fainted from hunger! Eat something (F)."));
				ApplyKnockdown(nullptr, FVector(0.f, 0.f, 50.f));
			}
		}

		// Breath: deep water drowns you without a boat.
		if (bDeep && !bIsNPC)
		{
			const float Before = Breath;
			Breath = FMath::Max(0.f, Breath - DeltaSeconds / BreathSeconds);
			if (Before > 0.5f && Breath <= 0.5f)
			{
				Notify(LOCTEXT("Drowning", "You're DROWNING! Get to shallow water!"));
			}
			if (Breath <= 0.f)
			{
				Drown();
			}
		}
		else
		{
			Breath = FMath::Min(1.f, Breath + DeltaSeconds / 3.f);
		}

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
				if (PS->HasItem(EBRItem::Ladder)) V *= 1.3f;
			}
			Visibility = FMath::Clamp(V, 0.05f, 1.5f);
		}

		// Can't keep aiming when you get knocked down, tied up or seated.
		if (bAiming && !CanAttack())
		{
			bAiming = false;
			ApplyAimRotationMode();
		}
	}

	if (IsLocallyControlled())
	{
		if (bAiming && !CanAttack())
		{
			Input_AimStop();
		}
		UpdateAimCamera(DeltaSeconds);

		// The current drags anyone in the river downstream (much harder when out of your depth).
		if (bInRiver && CanMove())
		{
			const float Strength = RiverCurrent * (bDeep ? 1.4f : 1.f) * (GS && GS->Weather == EBRWeather::Rain ? 1.5f : 1.f);
			AddMovementInput(MyArena->GetRiverFlowDirection(), Strength);
		}
	}

	UpdateGunVisual();
	TickAnimationSlots();
	TickWobble();
	UpdateLocalVisibility();
}

FVector ABRCharacter::GetMuzzleLocation() const
{
	if (WeaponRoot && WeaponRoot->IsVisible())
	{
		return WeaponRoot->GetComponentTransform().TransformPosition(FVector(26.f, 0.f, 5.f));
	}
	return GetActorLocation() + GetActorForwardVector() * 45.f + GetActorRightVector() * 18.f + FVector(0.f, 0.f, 45.f);
}

void ABRCharacter::UpdateGunVisual()
{
	// Which gun?
	int32 Style = 0;
	if (HasWeapon())
	{
		Style = IsRunner() ? 3 : (bLethalMode ? 2 : 1);
	}
	if (Style != GunStyle)
	{
		GunStyle = Style;
		WeaponRoot->SetVisibility(Style != 0, true);
		UStaticMesh* Custom = Style == 1 ? CustomTaserMesh.Get() : (Style == 2 ? CustomPistolMesh.Get() : (Style == 3 ? CustomRevolverMesh.Get() : nullptr));
		auto Shape = [](UStaticMeshComponent* Part, const FVector& At, const FVector& SizeCm, const FRotator& Rot, const FLinearColor& Color, bool bShow)
		{
			Part->SetVisibility(bShow);
			Part->SetRelativeLocationAndRotation(At, Rot);
			Part->SetRelativeScale3D(SizeCm / 100.f);
			if (UMaterialInstanceDynamic* MID = Part->CreateDynamicMaterialInstance(0))
			{
				MID->SetVectorParameterValue(TEXT("Color"), Color);
			}
		};
		const FRotator AlongX(90.f, 0.f, 0.f);
		const FLinearColor Black(0.03f, 0.03f, 0.03f), Steel(0.55f, 0.55f, 0.58f), Wood(0.35f, 0.18f, 0.07f), Yellow(0.95f, 0.75f, 0.05f);
		if (Custom)
		{
			GunBody->SetStaticMesh(Custom);
			GunBody->SetVisibility(Style != 0);
			GunBody->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
			GunBody->SetRelativeScale3D(FVector::OneVector);
			GunBarrel->SetVisibility(false);
			GunGrip->SetVisibility(false);
			GunExtra->SetVisibility(false);
		}
		else if (Style == 1) // taser: chunky yellow
		{
			Shape(GunBody, FVector(6.f, 0.f, 4.f), FVector(20.f, 4.5f, 7.f), FRotator::ZeroRotator, Yellow, true);
			Shape(GunBarrel, FVector(17.f, 0.f, 4.f), FVector(5.f, 5.f, 3.f), AlongX, Black, true);
			Shape(GunGrip, FVector(-1.f, 0.f, -3.f), FVector(4.5f, 3.8f, 10.f), FRotator(-15.f, 0.f, 0.f), Black, true);
			Shape(GunExtra, FVector(6.f, 0.f, 8.f), FVector(2.f, 2.f, 1.f), FRotator::ZeroRotator, Yellow, false);
		}
		else if (Style == 2) // service pistol: black slide
		{
			Shape(GunBody, FVector(7.f, 0.f, 4.f), FVector(20.f, 3.5f, 5.f), FRotator::ZeroRotator, Black, true);
			Shape(GunBarrel, FVector(17.5f, 0.f, 4.5f), FVector(1.6f, 1.6f, 2.f), AlongX, Steel, true);
			Shape(GunGrip, FVector(-1.f, 0.f, -3.5f), FVector(4.f, 3.3f, 10.f), FRotator(-15.f, 0.f, 0.f), Black, true);
			Shape(GunExtra, FVector(5.f, 0.f, -1.f), FVector(4.f, 4.f, 0.8f), FRotator(0.f, 0.f, 90.f), Black, true);
		}
		else if (Style == 3) // black-market revolver
		{
			Shape(GunBody, FVector(2.f, 0.f, 4.f), FVector(10.f, 3.f, 5.f), FRotator::ZeroRotator, Steel, true);
			Shape(GunBarrel, FVector(14.f, 0.f, 5.f), FVector(2.f, 2.f, 18.f), AlongX, Steel, true);
			Shape(GunGrip, FVector(-4.f, 0.f, -3.f), FVector(4.5f, 3.5f, 10.f), FRotator(-20.f, 0.f, 0.f), Wood, true);
			Shape(GunExtra, FVector(3.f, 0.f, 4.f), FVector(6.f, 6.f, 6.f), AlongX, FLinearColor(0.3f, 0.3f, 0.32f), true);
		}
	}
	if (GunStyle == 0)
	{
		return;
	}

	// Where is it? In the hand normally. While aiming: still in the hand if you have an aim animation
	// (so both hands are on the gun), otherwise raised in front of you, cartoon style.
	USkeletalMeshComponent* MeshComp = GetMesh();
	const bool bHasHand = MeshComp && MeshComp->GetSkeletalMeshAsset() && MeshComp->DoesSocketExist(HandSocket);
	const bool bAimInHand = bAiming && bHasHand && bGunInHandWhileAiming && (AimAnim || AimWalkAnim || (MeshComp && MeshComp->AnimClass));
	const int32 Placement = bAimInHand ? 2 : (bAiming ? 1 : 0);
	if (Placement != GunPlacement)
	{
		GunPlacement = Placement;
		if ((Placement == 0 || Placement == 2) && bHasHand)
		{
			WeaponRoot->AttachToComponent(MeshComp, FAttachmentTransformRules::SnapToTargetNotIncludingScale, HandSocket);
			WeaponRoot->SetRelativeTransform(HandGripOffset);
		}
		else
		{
			WeaponRoot->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			if (Placement == 0)
			{
				WeaponRoot->SetRelativeLocationAndRotation(FVector(22.f, 38.f, -5.f), FRotator(-60.f, 0.f, 0.f)); // held low at your side
			}
		}
	}
	if (Placement == 1)
	{
		const float Pitch = FRotator::NormalizeAxis(GetBaseAimRotation().Pitch);
		WeaponRoot->SetRelativeLocationAndRotation(AimGunOffset, FRotator(FMath::Clamp(Pitch, -70.f, 70.f), 0.f, 0.f));
	}
}

void ABRCharacter::UpdateLocalVisibility()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const APlayerController* LocalPC = GetWorld()->GetFirstPlayerController();
	const ABRPlayerState* LocalPS = LocalPC ? LocalPC->GetPlayerState<ABRPlayerState>() : nullptr;
	const ABRGameState* GS = GetBRGameState();
	const EPVVFaction Viewer = LocalPS ? LocalPS->GetFaction() : EPVVFaction::None;

	// Name tags: teammates only (and never on NPCs) so disguises and hiding in trucks work.
	if (StatusLabel)
	{
		const bool bShowLabel = !bIsNPC && Viewer == GetFaction() && Viewer != EPVVFaction::None;
		if (StatusLabel->IsVisible() != bShowLabel)
		{
			StatusLabel->SetVisibility(bShowLabel);
		}
	}

	// Stealth: Wardens' screens don't draw anyone from the Runner side beyond their visible range.
	bool bHide = false;
	if (IsRunner() && GS && Viewer == BR::Wardens && GS->Phase == EBRPhase::Invasion && !IsSpotted() && LocalPC && LocalPC->PlayerCameraManager && !GetCurrentBoat())
	{
		const float VisibleRange = 800.f + Visibility * 7000.f;
		bHide = FVector::Dist(LocalPC->PlayerCameraManager->GetCameraLocation(), GetActorLocation()) > VisibleRange;
	}
	if (bStealthHidden != bHide)
	{
		bStealthHidden = bHide;
		GetRootComponent()->SetVisibility(!bHide, true);
		if (StatusLabel && !bHide)
		{
			StatusLabel->SetVisibility(!bIsNPC && Viewer == GetFaction());
		}
	}
}

#undef LOCTEXT_NAMESPACE
