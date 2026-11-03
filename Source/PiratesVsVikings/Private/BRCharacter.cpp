#include "BRCharacter.h"
#include "BRSettings.h"
#include "BRCar.h"
#include "BRDesertArena.h"
#include "BRDog.h"
#include "BRMapKit.h"
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
#include "Engine/OverlapResult.h"
#include "Net/VoiceConfig.h"
#include "Sound/SoundAttenuation.h"
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
	// Always work out bone positions, even off-screen (the wobbly body needs them every frame).
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	KnockdownDuration = 4.5f; // arcade: get flattened, get back up
	KOStars = CreateDefaultSubobject<UTextRenderComponent>(TEXT("KOStars"));
	KOStars->SetupAttachment(GetCapsuleComponent());
	KOStars->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
	KOStars->SetText(FText::FromString(TEXT("*  @  *")));
	KOStars->SetTextRenderColor(FColor(255, 230, 60));
	KOStars->SetWorldSize(40.f);
	KOStars->SetHorizontalAlignment(EHTA_Center);
	KOStars->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	KOStars->SetHiddenInGame(true);
	HeadTag = CreateDefaultSubobject<UTextRenderComponent>(TEXT("HeadTag"));
	HeadTag->SetupAttachment(GetCapsuleComponent());
	HeadTag->SetRelativeLocation(FVector(0.f, 0.f, 135.f));
	HeadTag->SetWorldSize(30.f);
	HeadTag->SetHorizontalAlignment(EHTA_Center);
	HeadTag->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadTag->SetHiddenInGame(true);


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
	DisguiseOutfit = MakeOutfit(TEXT("DisguiseOutfit"));
	HeldItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeldItemMesh"));
	HeldItemMesh->SetupAttachment(GetMesh());
	HeldItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeldItemMesh->SetVisibility(false);
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> RopeCyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		static ConstructorHelpers::FObjectFinder<UStaticMesh> BoxCube(TEXT("/Engine/BasicShapes/Cube.Cube"));
		RopeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RopeMesh"));
		RopeMesh->SetupAttachment(GetCapsuleComponent());
		RopeMesh->SetStaticMesh(RopeCyl.Object);
		RopeMesh->SetUsingAbsoluteLocation(true);
		RopeMesh->SetUsingAbsoluteRotation(true);
		RopeMesh->SetUsingAbsoluteScale(true);
		RopeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		RopeMesh->SetCastShadow(false);
		RopeMesh->SetVisibility(false);
		BoxMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoxMesh"));
		BoxMesh->SetupAttachment(GetCapsuleComponent());
		BoxMesh->SetStaticMesh(BoxCube.Object);
		BoxMesh->SetRelativeLocation(FVector(0.f, 0.f, -15.f));
		BoxMesh->SetRelativeScale3D(FVector(0.95f, 0.95f, 1.25f));
		BoxMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BoxMesh->SetVisibility(false);
		CigaretteMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CigaretteMesh"));
		CigaretteMesh->SetupAttachment(GetMesh(), TEXT("hand_r"));
		CigaretteMesh->SetStaticMesh(RopeCyl.Object);
		CigaretteMesh->SetRelativeLocationAndRotation(FVector(4.f, 6.f, 2.f), FRotator(0.f, 0.f, 90.f));
		CigaretteMesh->SetRelativeScale3D(FVector(0.012f, 0.012f, 0.08f));
		CigaretteMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CigaretteMesh->SetCastShadow(false);
		CigaretteMesh->SetVisibility(false);
	}

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
	DOREPLIFETIME(ABRCharacter, bInCannon);
	DOREPLIFETIME(ABRCharacter, bFlying);
	DOREPLIFETIME(ABRCharacter, DanceUntil);
	DOREPLIFETIME(ABRCharacter, bOnShoulders);
	DOREPLIFETIME(ABRCharacter, bDiving);
	DOREPLIFETIME(ABRCharacter, HeldProp);
	DOREPLIFETIME(ABRCharacter, WornHat);
	DOREPLIFETIME(ABRCharacter, bDisguised);
	DOREPLIFETIME(ABRCharacter, HeldItem);
	DOREPLIFETIME(ABRCharacter, LuchaName);
	DOREPLIFETIME(ABRCharacter, bWeaponDrawn);
	DOREPLIFETIME(ABRCharacter, ArmsReaching);
	DOREPLIFETIME(ABRCharacter, HandGrabMask);
	DOREPLIFETIME(ABRCharacter, HandGrabTarget);
	DOREPLIFETIME(ABRCharacter, GrabbedBy);
	DOREPLIFETIME(ABRCharacter, GrabStruggle);
	DOREPLIFETIME(ABRCharacter, bClimbing);
	DOREPLIFETIME(ABRCharacter, bHiddenInSpot);
	DOREPLIFETIME(ABRCharacter, ActiveEmote);
	DOREPLIFETIME(ABRCharacter, bInBox);
	DOREPLIFETIME(ABRCharacter, LeashedDog);
	DOREPLIFETIME(ABRCharacter, bHanging);
	DOREPLIFETIME(ABRCharacter, bLastClimbLeft);
	DOREPLIFETIME(ABRCharacter, ClimbNormal);
}

void ABRCharacter::StartFleeing(float Seconds)
{
	FleeUntil = GetWorld()->GetTimeSeconds() + Seconds;
	if (!bWandering)
	{
		StartWandering(GetActorLocation(), 1500.f);
	}
}

void ABRCharacter::InitAsNPC(float SkinToneMin, float SkinToneMax)
{
	bIsNPC = true;
	KnockdownDuration = NPCKnockdownSeconds; // stay down long enough to be zip-tied
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
	if (HatMeshes.Num() > 0)
	{
		// Locals wear everyday hats (or none), not helmets.
		TArray<EBRHat> Hats = { EBRHat::None };
		for (const EBRHat Everyday : { EBRHat::CowboyHat, EBRHat::Sombrero })
		{
			if (HatMeshes.Contains(Everyday))
			{
				Hats.Add(Everyday);
			}
		}
		WornHat = Hats[FMath::RandRange(0, Hats.Num() - 1)];
		bHatAssigned = true;
	}
}

void ABRCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (!bHatAssigned && !bIsNPC)
	{
		const ABRPlayerState* HatPS = GetBRPlayerState();
		const int32 Choice = HatPS && HatPS->bHasLook ? HatPS->Look.HatChoice : -1;
		SetHat(Choice >= 0 ? static_cast<EBRHat>(Choice) : GetDefaultHat());
	}
	if (const ABRPlayerState* PS = GetBRPlayerState(); PS && PS->bHasLook)
	{
		SetLook(PS->Look);
	}
}

void ABRCharacter::StartFlight(const FVector& Velocity)
{
	if (!HasAuthority())
	{
		return;
	}
	bFlying = true;
	GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	LaunchCharacter(Velocity, true, true);
	ForceNetUpdate();
}

void ABRCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	if (GetNetMode() != NM_DedicatedServer && GetVelocity().Z < -700.f)
	{
		PlayBodySound(GetDefault<UBRSettings>()->BodyThumpSounds, GetActorLocation() - FVector(0.f, 0.f, 80.f), FMath::Clamp(-GetVelocity().Z / 1400.f, 0.3f, 1.f));
	}
	if (HasAuthority() && FallFromZ > 0.f)
	{
		const float Drop = FallFromZ - GetActorLocation().Z;
		FallFromZ = -1.f;
		if (Drop > FallKnockoutHeight && GetCondition() == EPVVCondition::Normal)
		{
			ApplyKnockdown(nullptr, FVector(0.f, 0.f, 100.f)); // splat
			ABRGameState::FX(this, EBRFX::Dust, GetActorLocation() - FVector(0.f, 0.f, 80.f));
			ABRGameState::Sfx(this, EBRSfx::Landing, GetActorLocation());
		}
	}
	if (IsLocallyControlled() && bFlying)
	{
		AddShake(1.2f); // big landing
	}
	if (HasAuthority() && bDiving)
	{
		bDiving = false;
		ABRGameState::FX(this, EBRFX::Dust, GetActorLocation() - FVector(0.f, 0.f, 80.f));
		ApplyKnockdown(nullptr, GetActorForwardVector() * 150.f); // missed: belly flop
	}
	if (!HasAuthority() || !bFlying)
	{
		return;
	}
	bFlying = false;
	ABRGameState::FX(this, EBRFX::Dust, GetActorLocation() - FVector(0.f, 0.f, 80.f));
	ABRGameState::Sfx(this, EBRSfx::Landing, GetActorLocation());
	ClearRestraint(); // the zip-ties snap on impact
	if (WornHat != EBRHat::CannonHelmet)
	{
		ApplyKnockdown(nullptr, -GetActorForwardVector() * 150.f + FVector(0.f, 0.f, 150.f)); // tumble
	}
}

void ABRCharacter::SetInCannon(bool bIn)
{
	if (!HasAuthority() || bInCannon == bIn)
	{
		return;
	}
	bInCannon = bIn;
	OnRep_InCannon();
	ForceNetUpdate();
}

void ABRCharacter::OnRep_InCannon()
{
	SetActorHiddenInGame(bInCannon);
	SetActorEnableCollision(!bInCannon);
	if (bInCannon)
	{
		DisableWobble(); // no wobbly physics while stuffed inside the barrel
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
	}
	else
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		EnableWobble();
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
	if (OutfitHead && HatMeshes.Num() == 0)
	{
		OutfitHead->SetHiddenInGame(!L.bHat); // hat system off: the Hat checkbox shows/hides your Blueprint hat
	}
	PendingFace = L;
	bFacePending = true; // applied in Tick once the body is ready
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
		// Only on the body: hair, hats and glasses copy the body's shape keys by name automatically.
		USkeletalMeshComponent* Body = GetMesh();
		const USkeletalMesh* Asset = Body ? Body->GetSkeletalMeshAsset() : nullptr;
		if (!Asset)
		{
			return;
		}
		if (Asset->FindMorphTarget(Small))
		{
			Body->SetMorphTarget(Small, SmallWeight, true);
		}
		if (Asset->FindMorphTarget(Big))
		{
			Body->SetMorphTarget(Big, BigWeight, true);
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

void ABRCharacter::StartFollowing(ABRCharacter* NewLeader, int32 Pay)
{
	Leader = NewLeader;
	ClientPay = Pay;
	bIllegal = true;
	bWandering = false;
	GetCharacterMovement()->bRunPhysicsWithNoController = true;
	GetCharacterMovement()->bOrientRotationToMovement = true;
}

void ABRCharacter::TickFollow(float DeltaSeconds)
{
	if (!bIsNPC || (!Leader.IsValid() && ClientPay == 0))
	{
		return;
	}
	ABRCharacter* L = Leader.Get();
	// Leader caught, gone or switched teams: the client gives up and hangs around here.
	if (!L || L->GetCondition() == EPVVCondition::Detained || !L->IsRunner())
	{
		Leader = nullptr;
		ClientPay = 0;
		if (GetCurrentBoat())
		{
			GetCurrentBoat()->Unseat(this);
		}
		StartWandering(GetActorLocation(), 900.f);
		return;
	}
	if (GetCondition() != EPVVCondition::Normal || GetCarrier())
	{
		return;
	}
	APVVBoat* LeaderRide = L->GetCurrentBoat();
	APVVBoat* MyRide = GetCurrentBoat();
	if (MyRide && MyRide != LeaderRide)
	{
		MyRide->Unseat(this); // leader got out: hop out too
		return;
	}
	if (MyRide)
	{
		return;
	}
	const FVector ToLeader = (L->GetActorLocation() - GetActorLocation()) * FVector(1.f, 1.f, 0.f);
	const float Dist = ToLeader.Size();
	if (LeaderRide && Dist < 700.f)
	{
		LeaderRide->TryBoard(this); // jump in the truck / boat with them
		return;
	}
	if (Dist > 3500.f && !LeaderRide)
	{
		// Leader went over the Wall, through a tunnel or flew: the client scrambles after them.
		TeleportTo(L->GetActorLocation() - L->GetActorForwardVector() * 200.f + FVector(0.f, 0.f, 30.f), L->GetActorRotation());
		return;
	}
	if (Dist > 250.f)
	{
		GetCharacterMovement()->MaxWalkSpeed = Dist > 900.f ? 650.f : 420.f;
		AddMovementInput(ToLeader.GetSafeNormal(), 1.f);
	}
}

void ABRCharacter::TickWander(float DeltaSeconds)
{
	if (!bWandering || Controller || GetCurrentBoat() || GetCarrier() || GetCondition() != EPVVCondition::Normal)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < FleeUntil)
	{
		// Running from Border Patrol: away from the nearest agent, zig-zagging a little.
		FVector Away = FVector::ZeroVector;
		float Closest = 3500.f;
		for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
		{
			const float D = FVector::Dist2D(It->GetActorLocation(), GetActorLocation());
			if (It->IsWarden() && !It->IsNPC() && D < Closest)
			{
				Closest = D;
				Away = (GetActorLocation() - It->GetActorLocation()) * FVector(1.f, 1.f, 0.f);
			}
		}
		if (Away.IsNearlyZero())
		{
			Away = (GetActorLocation() - WanderHome) * FVector(1.f, 1.f, 0.f);
		}
		const FVector Wiggle = FVector::CrossProduct(Away.GetSafeNormal(), FVector::UpVector) * FMath::Sin(Now * 2.3f) * 0.4f;
		GetCharacterMovement()->MaxWalkSpeed = FleeSpeed;
		AddMovementInput((Away.GetSafeNormal() + Wiggle).GetSafeNormal(), 1.f);
		return;
	}
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

	for (USkeletalMeshComponent* Outfit : { OutfitTop.Get(), OutfitBottom.Get(), OutfitHead.Get(), OutfitFace.Get(), Hair.Get(), FacialHair.Get(), DisguiseOutfit.Get() })
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
	ApplyCameraOptions();
	CameraBaseLocation = FollowCamera->GetRelativeLocation();
	SetupProximityVoice();
	EnableWobble();
	if (GetMesh())
	{
		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}
	FaceReadyTime = GetWorld()->GetTimeSeconds() + 1.f;
	ApplyLook(Look);
	if (HeldItemMesh && GetMesh() && GetMesh()->DoesSocketExist(TEXT("hand_r")))
	{
		HeldItemMesh->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("hand_r"));
		HeldItemMesh->SetRelativeTransform(HeldItemOffset);
	}
	OnRep_Hat();
	OnRep_Disguise();
	if (UMaterialInstanceDynamic* RopeMID = RopeMesh ? RopeMesh->CreateDynamicMaterialInstance(0) : nullptr)
	{
		RopeMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.6f, 0.45f, 0.25f));
	}
	if (UMaterialInstanceDynamic* CigMID = CigaretteMesh ? CigaretteMesh->CreateDynamicMaterialInstance(0) : nullptr)
	{
		CigMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.95f, 0.93f, 0.88f));
	}
	if (UMaterialInstanceDynamic* BoxMID = BoxMesh ? BoxMesh->CreateDynamicMaterialInstance(0) : nullptr)
	{
		BoxMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.62f, 0.46f, 0.27f));
	}
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
	if (WornHat == EBRHat::CowboyHat)
	{
		Multiplier *= 1.12f; // Cowboy Hat: rodeo legs
	}
	if (bInBox)
	{
		Multiplier *= 0.5f; // shuffling along in a box
	}
	if (GrabbedBy)
	{
		Multiplier *= 0.05f; // someone's got hold of you: you're not walking anywhere (mash Space!)
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
	TrumpetAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	NextItemAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	PrevItemAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	DropAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	GrabAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	DiveAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	HeadbuttAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	EmoteAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	CashAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);

	UInputMappingContext* IMC = DefaultMappingContext;
	PVVInput::Map(IMC, AimAction, EKeys::RightMouseButton);
	PVVInput::Map(IMC, AimAction, EKeys::Gamepad_LeftTrigger);
	PVVInput::Map(IMC, ReloadAction, EKeys::R);
	PVVInput::Map(IMC, ReloadAction, EKeys::Gamepad_FaceButton_Top);
	PVVInput::Map(IMC, CrouchAction, EKeys::C);
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
	PVVInput::Map(IMC, NextItemAction, EKeys::MouseScrollDown);
	PVVInput::Map(IMC, NextItemAction, EKeys::Gamepad_RightShoulder);
	PVVInput::Map(IMC, PrevItemAction, EKeys::MouseScrollUp);
	PVVInput::Map(IMC, DropAction, EKeys::G);
	PVVInput::Map(IMC, GrabAction, EKeys::Z);
	PVVInput::Map(IMC, GrabAction, EKeys::Gamepad_FaceButton_Right);
	PVVInput::Map(IMC, DiveAction, EKeys::LeftAlt);
	PVVInput::Map(IMC, DiveAction, EKeys::LeftControl);
	PVVInput::Map(IMC, DiveAction, EKeys::ThumbMouseButton);
	PVVInput::Map(IMC, DiveAction, EKeys::Gamepad_LeftThumbstick);
	PVVInput::Map(IMC, EmoteAction, EKeys::T);
	PVVInput::Map(IMC, HeadbuttAction, EKeys::MiddleMouseButton);
	PVVInput::Map(IMC, CashAction, EKeys::H);
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
	if (AttackAction)
	{
		Input->BindAction(AttackAction, ETriggerEvent::Started, this, &ABRCharacter::Input_FireIfAiming);
		Input->BindAction(AttackAction, ETriggerEvent::Completed, this, &ABRCharacter::Input_LeftHandStop);
	}
	if (ReloadAction) Input->BindAction(ReloadAction, ETriggerEvent::Started, this, &ABRCharacter::Input_Reload);
	if (CrouchAction) Input->BindAction(CrouchAction, ETriggerEvent::Started, this, &ABRCharacter::Input_Crouch);
	if (EatAction) Input->BindAction(EatAction, ETriggerEvent::Started, this, &ABRCharacter::Input_Eat);
	if (NextItemAction) Input->BindAction(NextItemAction, ETriggerEvent::Started, this, &ABRCharacter::Input_NextItem);
	if (PrevItemAction) Input->BindAction(PrevItemAction, ETriggerEvent::Started, this, &ABRCharacter::Input_PrevItem);
	if (DropAction) Input->BindAction(DropAction, ETriggerEvent::Started, this, &ABRCharacter::Input_Drop);
	if (GrabAction) Input->BindAction(GrabAction, ETriggerEvent::Started, this, &ABRCharacter::Input_Grab);
	if (DiveAction) Input->BindAction(DiveAction, ETriggerEvent::Started, this, &ABRCharacter::Input_Dive);
	if (DroneAction) Input->BindAction(DroneAction, ETriggerEvent::Started, this, &ABRCharacter::Input_Drone);
	if (WeaponModeAction) Input->BindAction(WeaponModeAction, ETriggerEvent::Started, this, &ABRCharacter::Input_WeaponMode);
	if (ScanAction) Input->BindAction(ScanAction, ETriggerEvent::Started, this, &ABRCharacter::Input_Scan);
	if (TrapAction) Input->BindAction(TrapAction, ETriggerEvent::Started, this, &ABRCharacter::Input_Trap);
	if (EmoteAction)
	{
		Input->BindAction(EmoteAction, ETriggerEvent::Started, this, &ABRCharacter::Input_EmoteStart);
		Input->BindAction(EmoteAction, ETriggerEvent::Completed, this, &ABRCharacter::Input_EmoteStop);
	}
	if (CashAction) Input->BindAction(CashAction, ETriggerEvent::Started, this, &ABRCharacter::Input_DropCash);
	if (HeadbuttAction) Input->BindAction(HeadbuttAction, ETriggerEvent::Started, this, &ABRCharacter::Input_Headbutt);
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
	// F = use the selected item. The lasso is thrown where you're looking.
	const ABRPlayerState* PS = GetBRPlayerState();
	if (PS && PS->Inventory.IsValidIndex(SelectedSlot) && PS->Inventory[SelectedSlot] == EBRItem::Lasso && FollowCamera)
	{
		Server_Lasso(FollowCamera->GetComponentLocation(), FollowCamera->GetForwardVector());
		return;
	}
	Server_UseItem(SelectedSlot);
}
void ABRCharacter::Input_Trumpet() { Server_PlayTrumpet(); }
void ABRCharacter::Input_Drop() { Server_DropItem(SelectedSlot); }

void ABRCharacter::Input_NextItem()
{
	const ABRPlayerState* PS = GetBRPlayerState();
	const int32 Count = PS ? FMath::Max(1, PS->Inventory.Num()) : 1;
	SelectedSlot = (SelectedSlot + 1) % Count;
	Server_SelectSlot(SelectedSlot);
}

void ABRCharacter::Input_PrevItem()
{
	const ABRPlayerState* PS = GetBRPlayerState();
	const int32 Count = PS ? FMath::Max(1, PS->Inventory.Num()) : 1;
	SelectedSlot = (SelectedSlot - 1 + Count) % Count;
	Server_SelectSlot(SelectedSlot);
}

void ABRCharacter::Server_UseItem_Implementation(int32 Slot)
{
	ABRPlayerState* PS = GetBRPlayerState();
	if (!PS || GetCondition() != EPVVCondition::Normal || GetCarrier())
	{
		return;
	}
	if (!PS->Inventory.IsValidIndex(Slot))
	{
		Slot = 0;
	}
	if (!PS->Inventory.IsValidIndex(Slot))
	{
		Notify(LOCTEXT("EmptyHands", "Your pockets are empty."));
		return;
	}
	switch (PS->Inventory[Slot])
	{
	case EBRItem::Taco: Server_Eat_Implementation(); break;
	case EBRItem::Trumpet:
	{
		// On a band stage: play a note in time. Anywhere else: the dance-attack trumpet blast.
		ABRMariachiBand* Band = nullptr;
		for (TActorIterator<ABRMariachiBand> It(GetWorld()); It; ++It)
		{
			if (It->IsPerformer(this))
			{
				Band = *It;
				break;
			}
		}
		if (Band)
		{
			Band->PlayNote(this);
		}
		else
		{
			PlayTrumpet();
		}
		break;
	}
	case EBRItem::TacoPlatter: PlaceTacoPlatter(); break;
	case EBRItem::CardboardBox: ToggleBox(); break;
	case EBRItem::Lasso: Notify(LOCTEXT("UseLasso", "Aim at someone with your view and press F to throw the lasso.")); break;
	case EBRItem::Ladder: Notify(LOCTEXT("UseLadder", "Walk up to the Wall and press E to lean your ladder on it.")); break;
	case EBRItem::WireCutters: Notify(LOCTEXT("UseCutters", "Walk up to the razor wire and hold E to cut it.")); break;
	case EBRItem::GasCan: Notify(LOCTEXT("UseGas", "Walk up to the truck and press E to fill it up.")); break;
	case EBRItem::Shovel: Notify(LOCTEXT("UseShovel", "Find a soft dirt mound and hold E to dig a tunnel.")); break;
	case EBRItem::CannonSuit: Notify(LOCTEXT("UseSuit", "Climb into the Human Cannon (E) once the Line is open.")); break;
	case EBRItem::Drugs:
	case EBRItem::IllegalGun: Notify(LOCTEXT("UseContraband", "Contraband! Get it to the Safe Zone for $500. Don't get caught with it.")); break;
	default: Notify(LOCTEXT("UseNothing", "You can't use that here.")); break;
	}
}

void ABRCharacter::Server_DropItem_Implementation(int32 Slot)
{
	ABRPlayerState* PS = GetBRPlayerState();
	if (!PS || GetCurrentBoat() || bInCannon)
	{
		return;
	}
	const int32 ChargesBefore = PS->WireCutterCharges;
	EBRItem Dropped = EBRItem::None;
	if (!PS->RemoveItemAt(PS->Inventory.IsValidIndex(Slot) ? Slot : 0, Dropped))
	{
		return;
	}
	const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 5.f);
	const FTransform Where(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Feet + GetActorForwardVector() * 90.f);
	if (ABRItemPickup* Pickup = GetWorld()->SpawnActorDeferred<ABRItemPickup>(ABRItemPickup::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Pickup->Item = Dropped;
		Pickup->Charges = Dropped == EBRItem::WireCutters ? FMath::Max(1, ChargesBefore - PS->WireCutterCharges) : 0;
		Pickup->FinishSpawning(Where);
	}
}

void ABRCharacter::PlaceTacoPlatter()
{
	ABRPlayerState* PS = GetBRPlayerState();
	if (!PS || GetCurrentBoat() || !PS->RemoveItem(EBRItem::TacoPlatter))
	{
		return;
	}
	const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	GetWorld()->SpawnActor<ABRTacoPlatter>(ABRTacoPlatter::StaticClass(), Feet + GetActorForwardVector() * 150.f, FRotator::ZeroRotator, Params);
	Notify(LOCTEXT("PlatterPlaced", "Taco platter served! Your crew can grab tacos here (E)."));
}

bool ABRCharacter::IsDancing() const
{
	const ABRGameState* GS = GetBRGameState();
	return GS && GS->GetServerWorldTimeSeconds() < DanceUntil;
}

void ABRCharacter::ForceDance(float Seconds)
{
	const ABRGameState* GS = GetBRGameState();
	if (!HasAuthority() || !GS)
	{
		return;
	}
	DanceUntil = GS->GetServerWorldTimeSeconds() + Seconds;
	bAiming = false;
	ApplySlow(Seconds, 0.f); // feet are busy dancing
	Notify(LOCTEXT("Dancing", "The mariachi got you! You can't stop DANCING!"));
	ForceNetUpdate();
}

void ABRCharacter::Server_PlayTrumpet_Implementation()
{
	PlayTrumpet();
}

void ABRCharacter::PlayTrumpet()
{
	ABRPlayerState* PS = GetBRPlayerState();
	const float Now = GetWorld()->GetTimeSeconds();
	if (!PS || !PS->HasItem(EBRItem::Trumpet) || GetCondition() != EPVVCondition::Normal || GetCurrentBoat() || bInCannon)
	{
		return;
	}
	if (Now - LastTrumpetTime < TrumpetCooldown)
	{
		Notify(FText::Format(LOCTEXT("TrumpetTired", "Catch your breath... ({0} s)"), FMath::CeilToInt(TrumpetCooldown - (Now - LastTrumpetTime))));
		return;
	}
	LastTrumpetTime = Now;
	int32 Dancers = 0;
	for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
	{
		ABRCharacter* Other = *It;
		if (Other != this && Other->IsWarden() && Other->GetCondition() == EPVVCondition::Normal && !Other->GetCurrentBoat()
			&& FVector::Dist(Other->GetActorLocation(), GetActorLocation()) < TrumpetRange)
		{
			Other->ForceDance(DanceSeconds);
			++Dancers;
			if (ABRPlayerState* MyPS = GetBRPlayerState())
			{
				++MyPS->StatDances;
			}
		}
	}
	Spot(4.f); // a trumpet solo is not stealthy
	if (ABRGameState* GS = GetBRGameState())
	{
		GS->Multicast_NoisePing(GetActorLocation());
	}
	Multicast_Trumpet();
	Notify(Dancers > 0
		? FText::Format(LOCTEXT("TrumpetHit", "AY AY AY! {0} Border Patrol dancing - RUN!"), Dancers)
		: LOCTEXT("TrumpetMiss", "Beautiful solo. Nobody was close enough to dance."));
}

void ABRCharacter::Multicast_Trumpet_Implementation()
{
	PlayOneShot(TrumpetAnim);
	if (TrumpetSound && GetNetMode() != NM_DedicatedServer)
	{
		UGameplayStatics::PlaySoundAtLocation(this, TrumpetSound, GetActorLocation());
	}
}
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
	return bWeaponDrawn && OwnsWeapon();
}

bool ABRCharacter::OwnsWeapon() const
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
	if (!HasWeapon())
	{
		if (bClimbing)
		{
			bRightIsAim = false;
			Server_ClimbStep(false);
			return;
		}
		// Fists: right hand. Tap = punch, hold = reach out and grab.
		bRightIsAim = false;
		bRightDown = true;
		RightDownAt = GetWorld()->GetTimeSeconds();
		if (CanAttack())
		{
			Server_PunchHand(false);
		}
		return;
	}
	bRightIsAim = true;
	if (!CanAttack())
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
	if (!bRightIsAim)
	{
		Input_RightHandStop();
	}
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
	bAiming = bNewAiming && HasWeapon() && !IsDancing();
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
	if (!bAiming || !HasWeapon())
	{
		Input_LeftHandStart(); // fists: left hand
		return;
	}
	if (!CanAttack())
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
	if (!HasWeapon())
	{
		Server_Kick(); // fists out: R kicks
		return;
	}
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
	// X cycles: FISTS -> TASER -> PISTOL -> FISTS (Runners: fists <-> revolver).
	if (!OwnsWeapon())
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(ReloadTimer);
	bReloading = false;
	const ABRGameState* GS = GetBRGameState();
	if (!bWeaponDrawn)
	{
		bWeaponDrawn = true;
		bLethalMode = IsRunner();
		Notify(IsRunner() ? LOCTEXT("RevolverOut", "REVOLVER out. Right mouse to aim. X to put it away.")
			: LOCTEXT("Taser", "TASER out. Right mouse to aim, left click to shoot. Knocks them down so you can cuff them."));
	}
	else if (IsWarden() && !bLethalMode && GS && GS->LethalAllowed())
	{
		bLethalMode = true;
		Notify(LOCTEXT("Lethal", "SERVICE PISTOL. Shooting someone who hasn't crossed = SCANDAL."));
	}
	else
	{
		bWeaponDrawn = false;
		bLethalMode = false;
		if (bAiming)
		{
			Server_SetAiming_Implementation(false);
		}
		Notify(LOCTEXT("Fists", "FISTS. Left / right click = left / right hand: tap to punch, hold to grab."));
	}
	ForceNetUpdate();
}

void ABRCharacter::Server_Fire_Implementation(FVector_NetQuantize Start, FVector_NetQuantizeNormal Direction)
{
	ABRPlayerState* PS = GetBRPlayerState();
	ABRGameState* GS = GetBRGameState();
	if (!CanAttack() || !PS || !GS || !HasWeapon() || bReloading || IsDancing() || FVector::Dist(Start, GetActorLocation()) > 900.f)
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
	if (HasAuthority())
	{
		BlowDisguise(); // nobody in a poncho throws punches
	}
	// Physics fists: throw one arm forward (alternating), on every screen.
	if (ForcedPunchHand >= 0)
	{
		bPunchLeft = ForcedPunchHand == 1;
		ForcedPunchHand = -1;
	}
	else
	{
		bPunchLeft = !bPunchLeft;
	}
	if (bWobbleOn && GetMesh())
	{
		const FName Hand = bPunchLeft ? FName(TEXT("hand_l")) : FName(TEXT("hand_r"));
		const FName Fore = bPunchLeft ? FName(TEXT("lowerarm_l")) : FName(TEXT("lowerarm_r"));
		const FName Upper = bPunchLeft ? FName(TEXT("upperarm_l")) : FName(TEXT("upperarm_r"));
		const FName Clavicle = bPunchLeft ? FName(TEXT("clavicle_l")) : FName(TEXT("clavicle_r"));
		if (GetMesh()->GetBoneIndex(Hand) != INDEX_NONE)
		{
			// Aim the fist at whoever is in front of you (a little help, like Gang Beasts' auto-aim).
			const FVector Shoulder = GetMesh()->GetBoneLocation(Upper);
			FVector Dir = (GetActorForwardVector() + FVector(0.f, 0.f, 0.1f)).GetSafeNormal();
			float Best = 280.f;
			for (TActorIterator<APVVCharacter> It(GetWorld()); It; ++It)
			{
				const APVVCharacter* Other = *It;
				const FVector Chest = Other->GetActorLocation() + FVector(0.f, 0.f, 45.f);
				const float D = FVector::Dist(Chest, Shoulder);
				if (Other != this && D < Best && FVector::DotProduct((Chest - Shoulder).GetSafeNormal2D(), GetActorForwardVector()) > 0.5f)
				{
					Best = D;
					Dir = (Chest - Shoulder).GetSafeNormal();
				}
			}
			// Let the arm go loose for a moment so it really flies out, then fling it.
			FPhysicalAnimationData Loose;
			Loose.bIsLocalSimulation = false;
			Loose.OrientationStrength = 20.f;
			Loose.AngularVelocityStrength = 2.f;
			PhysicalAnimation->ApplyPhysicalAnimationSettingsBelow(Clavicle, Loose, true);
			PunchArmRestoreAt = GetWorld()->GetTimeSeconds() + 0.28f;
			GetMesh()->AddImpulse(Dir * PunchImpulse, Hand, true);
			GetMesh()->AddImpulse(Dir * PunchImpulse * 0.7f, Fore, true);
			GetMesh()->AddImpulse(Dir * PunchImpulse * 0.35f, Upper, true);
			// Twist the shoulders into it.
			GetMesh()->AddImpulseToAllBodiesBelow(Dir * 220.f, TEXT("spine_03"), true, false);
		}
	}
	if (PunchSwingSound && GetNetMode() != NM_DedicatedServer)
	{
		UGameplayStatics::PlaySoundAtLocation(this, PunchSwingSound, GetActorLocation());
	}
	else if (GetNetMode() != NM_DedicatedServer)
	{
		PlayBodySound(GetDefault<UBRSettings>()->WhooshSounds, GetActorLocation(), 0.7f);
	}
}

bool ABRCharacter::CanAttackWhileCarrying() const
{
	const APVVCharacter* Held = GetCarriedCharacter();
	return Held && Held->GetCondition() != EPVVCondition::Restrained;
}

bool ABRCharacter::HandlePunchHit()
{
	// Holding someone with one hand? The other fist lands on them.
	if (ABRCharacter* Held = HandGrabTarget.Get(); Held && FVector::Dist(Held->GetActorLocation(), GetActorLocation()) < 220.f)
	{
		const FVector Dir = (Held->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		Held->ReceiveMeleeHit(this, AttackDamage * 1.2f, Dir * 150.f);
		Held->Multicast_BodyHit(TEXT("head"), Dir * HitReaction);
		Multicast_Sfx(3);
		return true;
	}
	// Holding someone? Punch them in the face with your free hand.
	if (APVVCharacter* Held = GetCarriedCharacter())
	{
		Held->ReceiveHeldPunch(this, AttackDamage);
		Multicast_Sfx(3);
		return true;
	}
	// Wobbly body: the actual fist has to connect (checked in TickBrawl during the punch window).
	if (bWobbleOn && GetMesh() && GetMesh()->GetBoneIndex(TEXT("hand_r")) != INDEX_NONE)
	{
		PunchUntil = GetWorld()->GetTimeSeconds() + PunchWindow;
		bPunchConnected = false;
		return true;
	}
	return false; // no physics body: classic punch
}

void ABRCharacter::Server_Mash_Implementation()
{
	const float Now = GetWorld()->GetTimeSeconds();
	// Stay down at least a moment, and only count presses a human could make (the physics body needs time to settle).
	if (GetCondition() != EPVVCondition::KnockedDown || GetCarrier() || Now - KnockedDownAt < 1.2f || Now - LastMashTime < 0.1f)
	{
		return;
	}
	LastMashTime = Now;
	if (++MashCount >= MashToWake)
	{
		MashCount = 0;
		Recover(0.4f); // shook it off
	}
}

bool ABRCharacter::TryClimb()
{
	// Grab the edge in front of you and haul yourself up (crates, truck beds, low walls, roofs).
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, HalfHeight);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BRClimb), false, this);
	FHitResult Wall;
	if (!GetWorld()->LineTraceSingleByChannel(Wall, GetActorLocation(), GetActorLocation() + Forward * 110.f, ECC_Visibility, Params))
	{
		return false;
	}
	FHitResult Top;
	const FVector Above = Wall.ImpactPoint + Forward * 40.f + FVector(0.f, 0.f, ClimbHeight + 60.f);
	if (!GetWorld()->LineTraceSingleByChannel(Top, Above, Above - FVector(0.f, 0.f, ClimbHeight + 120.f), ECC_Visibility, Params) || Top.bStartPenetrating)
	{
		return false;
	}
	const float LedgeHeight = Top.ImpactPoint.Z - Feet.Z;
	if (LedgeHeight < 40.f || LedgeHeight > ClimbHeight)
	{
		return false;
	}
	const float Up = FMath::Sqrt(2.f * 980.f * (LedgeHeight + 70.f));
	LaunchCharacter(Forward * 280.f + FVector(0.f, 0.f, Up), true, true);
	return true;
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
	else if (IsDancing() && DanceAnim) Wanted = DanceAnim.Get();
	else if (ActiveEmote >= 0 && EmoteAnimFor(ActiveEmote)) Wanted = EmoteAnimFor(ActiveEmote);
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
		++PS->StatTacos;
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
	const bool bCap = WornHat == EBRHat::PatrolCap;
	if (!IsWarden() || Now - LastScanTime < (bCap ? 0.75f : 1.5f) || FVector::Dist(Start, GetActorLocation()) > 900.f)
	{
		return;
	}
	if (GetCurrentBoat())
	{
		Notify(LOCTEXT("ScanOnFoot", "Get out and walk up to people to scan them."));
		return;
	}
	LastScanTime = Now;

	// One person at a time, up close (walk up to the truck window).
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BRScan), false, this);
	FHitResult Hit;
	const float Range = bCap ? 680.f : 450.f;
	const FVector From = GetActorLocation() + FVector(0.f, 0.f, 40.f);
	if (!GetWorld()->SweepSingleByObjectType(Hit, From, From + FVector(Direction) * Range, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(60.f), Params))
	{
		Notify(LOCTEXT("ScanNothing", "SCAN: nobody in range - walk right up to them."));
		return;
	}
	ABRCharacter* Target = Cast<ABRCharacter>(Hit.GetActor());
	if (!Target)
	{
		Notify(LOCTEXT("ScanNothing2", "SCAN: nobody there."));
		return;
	}
	Target->LastScannedTime = Now;
	Target->ScannedBy = this;
	if (Target->IsWarden())
	{
		Notify(LOCTEXT("ScanWarden", "SCAN: that's a fellow agent."));
	}
	else if (!Target->IsNPC() && !Target->IsDisguised() && !Target->IsHiddenFromPatrol())
	{
		Target->Spot(8.f);
		ABRGameState::Sfx(this, EBRSfx::ScanAlarm, Target->GetActorLocation());
		Notify(LOCTEXT("ScanRunner", "SCAN: RUNNER! (tagged) - order them out / zip-tie them!"));
	}
	else if (Target->bIllegal)
	{
		ABRGameState::Sfx(this, EBRSfx::ScanAlarm, Target->GetActorLocation());
		Notify(LOCTEXT("ScanIllegal", "SCAN: NO PAPERS! Order them out of the truck and arrest them."));
	}
	else if (Target->bCarriesContraband)
	{
		ABRGameState::Sfx(this, EBRSfx::ScanAlarm, Target->GetActorLocation());
		Notify(LOCTEXT("ScanContraband", "SCAN: CONTRABAND! Order them out of the truck to seize it."));
	}
	else
	{
		ABRGameState::Sfx(this, EBRSfx::ScanOK, Target->GetActorLocation());
		Notify(LOCTEXT("ScanCitizen", "SCAN: papers in order. Regular citizen."));
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
	const ABRGameState* TrapGS = GetBRGameState();
	const int32 TrapCap = MaxTeamTraps + (TrapGS ? TrapGS->BonusTraps : 0);
	if (Count >= TrapCap)
	{
		Notify(FText::Format(LOCTEXT("TrapLimit", "Trap limit reached ({0}). Wait for some to be sprung, or buy more at the supply counter."), TrapCap));
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
			FText::FromString(Names[static_cast<int32>(Type)]), Count + 1, TrapCap));
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
	if (GetCurrentBoat() || GetCarrier() || bInCannon || bOnShoulders)
	{
		return; // switched on again when you get out
	}
	// The bones must have been posed at least once, or the physics reads bones that don't exist yet (crash).
	if (MeshComp->GetComponentSpaceTransforms().Num() < MeshComp->GetNumBones() || !MeshComp->IsRegistered())
	{
		bWobblePending = true;
		return;
	}
	bWobblePending = false;
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

void ABRCharacter::DisableWobble()
{
	bWobblePending = false;
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!bWobbleOn || !MeshComp)
	{
		return;
	}
	bWobbleOn = false;
	PhysicalAnimation->SetSkeletalMeshComponent(nullptr); // drops all the muscle constraints
	const FName SimRoot = bWobblyLegs ? FName(TEXT("pelvis")) : FName(TEXT("spine_01"));
	MeshComp->SetAllBodiesBelowSimulatePhysics(SimRoot, false, true);
	MeshComp->SetAllBodiesBelowPhysicsBlendWeight(SimRoot, 0.f, false, true);
}

void ABRCharacter::ApplyMuscles(float ArmMuscle)
{
	auto Muscle = [this](float Strength)
	{
		FPhysicalAnimationData Data;
		Data.bIsLocalSimulation = false;
		Data.OrientationStrength = Strength * WobbleStrength * DizzyScale;
		Data.AngularVelocityStrength = Strength * WobbleStrength * DizzyScale * 0.1f;
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

float ABRCharacter::CurrentArmMuscle() const
{
	return bArmsStiff ? (bAiming ? AimArmStrength : DriveArmStrength) : ArmStrength;
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
	const float Now = GetWorld()->GetTimeSeconds();
	// The punching arm goes back to normal after the swing.
	if (PunchArmRestoreAt > 0.f && Now >= PunchArmRestoreAt)
	{
		PunchArmRestoreAt = -1.f;
		ApplyMuscles(CurrentArmMuscle());
		AppliedReachMuscles = 0xFF; // re-loosen any reaching arm
	}
	// Getting beaten up makes you woozy: weaker muscles, floppier body (Gang Beasts).
	const float WantDizzy = GetCondition() == EPVVCondition::Normal ? FMath::Lerp(0.3f, 1.f, GetBalanceFraction()) : 1.f;
	if (FMath::Abs(WantDizzy - DizzyScale) > 0.08f && PunchArmRestoreAt < 0.f)
	{
		DizzyScale = WantDizzy;
		ApplyMuscles(CurrentArmMuscle());
		AppliedReachMuscles = 0xFF;
	}
	// Drunken sway: little random shoves to the upper body.
	if (Now >= NextSwayTime && GetCondition() == EPVVCondition::Normal && !bAiming && !IsDriving())
	{
		NextSwayTime = Now + FMath::FRandRange(0.5f, 1.4f);
		const float Woozy = 1.f + (Hunger < WeakHunger ? 1.5f : 0.f) + (1.f - GetBalanceFraction()) * 2.f;
		const FVector Shove = FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f) * SwayStrength * Woozy;
		GetMesh()->AddImpulseToAllBodiesBelow(Shove, TEXT("spine_02"), true, true);
	}
}

void ABRCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	SetupProximityVoice();
}

void ABRCharacter::SetupProximityVoice()
{
	// Other players' voices come out of their character and fade with distance.
	APlayerState* PS = GetPlayerState();
	if (!bProximityVoice || !PS || PS->IsABot() || IsLocallyControlled() || GetNetMode() == NM_DedicatedServer || bIsNPC)
	{
		return;
	}
	if (!VoiceAttenuation)
	{
		VoiceAttenuation = NewObject<USoundAttenuation>(this);
		VoiceAttenuation->Attenuation.bAttenuate = true;
		VoiceAttenuation->Attenuation.bSpatialize = true;
		VoiceAttenuation->Attenuation.AttenuationShapeExtents = FVector(300.f, 0.f, 0.f);
		VoiceAttenuation->Attenuation.FalloffDistance = VoiceRange;
	}
	if (!VoiceTalker)
	{
		VoiceTalker = UVOIPTalker::CreateTalkerForPlayer(PS);
	}
	if (VoiceTalker)
	{
		VoiceTalker->Settings.AttenuationSettings = VoiceAttenuation;
		VoiceTalker->Settings.ComponentToAttachTo = GetRootComponent();
	}
}

void ABRCharacter::TickShake(float DeltaSeconds)
{
	if (!IsLocallyControlled() || !FollowCamera)
	{
		return;
	}
	if (ShakeAmount <= 0.f)
	{
		return;
	}
	ShakeAmount = FMath::Max(0.f, ShakeAmount - DeltaSeconds * 2.5f);
	if (ShakeAmount <= 0.f)
	{
		FollowCamera->SetRelativeLocation(CameraBaseLocation);
		return;
	}
	const float S = ShakeAmount * ScreenShake * 25.f;
	FollowCamera->SetRelativeLocation(CameraBaseLocation + FVector(0.f, FMath::FRandRange(-S, S), FMath::FRandRange(-S, S)));
}

void ABRCharacter::OnHitReceivedNative(APVVCharacter* Attacker)
{
	if (GetNetMode() != NM_DedicatedServer && GetWorld()->GetTimeSeconds() - LastGruntTime > 0.4f && FMath::FRand() < 0.75f)
	{
		LastGruntTime = GetWorld()->GetTimeSeconds();
		PlayBodySound(GetDefault<UBRSettings>()->HitGruntSounds, GetActorLocation() + FVector(0.f, 0.f, 60.f));
	}
	if (HasAuthority())
	{
		BlowDisguise();
	}
	if (IsLocallyControlled())
	{
		AddShake(0.5f);
	}
	if (!bWobbleOn || !GetMesh())
	{
		return;
	}
	const FVector Dir = Attacker ? (GetActorLocation() - Attacker->GetActorLocation()).GetSafeNormal2D() : -GetActorForwardVector();
	GetMesh()->AddImpulseToAllBodiesBelow(Dir * HitWobble + FVector(0.f, 0.f, HitWobble * 0.3f), TEXT("spine_01"), true, true);
}

void ABRCharacter::OnStateEffectsRefreshed()
{
	// Zip-tie / cell-door sounds when your state changes (plays on every machine, no extra network message).
	if (GetCondition() != LastSoundCondition)
	{
		const EPVVCondition Was = LastSoundCondition;
		LastSoundCondition = GetCondition();
		if (GetNetMode() != NM_DedicatedServer && GetWorld()->GetTimeSeconds() > 2.f)
		{
			const EBRSfx Which = GetCondition() == EPVVCondition::Restrained && Was != EPVVCondition::Restrained ? EBRSfx::ZipTie
				: (GetCondition() == EPVVCondition::Detained ? EBRSfx::JailDoor : EBRSfx::Countdown);
			if (Which != EBRSfx::Countdown)
			{
				if (USoundBase* Wave = GetDefault<UBRSettings>()->GetSound(Which))
				{
					UGameplayStatics::PlaySoundAtLocation(this, Wave, GetActorLocation());
				}
			}
		}
	}
	if (!PhysicalAnimation || !GetMesh() || !GetMesh()->GetPhysicsAsset())
	{
		return;
	}
	// Riding in a vehicle or being carried: no wobbly physics (it crashes the engine on moving, attached bodies).
	if (GetCurrentBoat() || GetCarrier() || bInCannon || bOnShoulders)
	{
		DisableWobble();
		return;
	}
	const bool bRagdolling = GetCondition() == EPVVCondition::KnockedDown;
	if (bRagdolling)
	{
		if (IsLocallyControlled())
		{
			AddShake(1.f);
		}
		MashCount = 0;
		KnockedDownAt = GetWorld()->GetTimeSeconds();
		if (KnockoutSound && GetNetMode() != NM_DedicatedServer)
		{
			UGameplayStatics::PlaySoundAtLocation(this, KnockoutSound, GetActorLocation());
		}
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
	const FVector Location = GetActorLocation();
	const bool bFree = !GetCurrentBoat() && !GetCarrier();
	const bool bInRiver = bFree && BRMap::IsInRiver(GetWorld(), Location);
	const bool bDeep = bInRiver && BRMap::IsDeepWater(GetWorld(), Location);
	if (bDeep != bInDeepWater)
	{
		bInDeepWater = bDeep;
		UpdateMovementSpeed();
		if (bDeep && HasAuthority())
		{
			ABRGameState::FX(this, EBRFX::Splash, Location);
			ABRGameState::Sfx(this, EBRSfx::Splash, Location);
		}
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
		TickFollow(DeltaSeconds);
		TickBrawl(DeltaSeconds);
		TickOutfitState();
	}
	if (HasAuthority() && ActiveEmote >= 0 && (GetVelocity().Size2D() > 60.f || GetCondition() != EPVVCondition::Normal || GetCarrier()
		|| GetCurrentBoat() || bClimbing || GrabbedBy || HandGrabTarget))
	{
		StopEmote();
	}
	if (ActiveEmote == 6 && GetNetMode() != NM_DedicatedServer && GetWorld()->GetTimeSeconds() >= NextSmokePuff)
	{
		// Exhale (matches the 4 s smoking loop).
		NextSmokePuff = GetWorld()->GetTimeSeconds() + 4.f;
		const FVector Mouth = GetMesh() && GetMesh()->GetBoneIndex(TEXT("head")) != INDEX_NONE ? GetMesh()->GetBoneLocation(TEXT("head")) : GetActorLocation() + FVector(0.f, 0.f, 70.f);
		ABRGameState::SpawnLocalFX(this, EBRFX::CigaretteSmoke, Mouth + GetActorForwardVector() * 25.f + FVector(0.f, 0.f, -5.f));
	}
	TickBodySounds(DeltaSeconds);
	TickShake(DeltaSeconds);
	TickReachVisuals();
	TickRope();
	TickEmoteWheel();
	TickHeadTag();
	if (KOStars && GetNetMode() != NM_DedicatedServer)
	{
		const bool bKO = GetCondition() == EPVVCondition::KnockedDown;
		KOStars->SetHiddenInGame(!bKO);
		if (bKO)
		{
			// Stars circle the head (follow the ragdoll's head).
			const FVector Head = GetMesh() && GetMesh()->GetBoneIndex(TEXT("head")) != INDEX_NONE ? GetMesh()->GetBoneLocation(TEXT("head")) : GetActorLocation();
			KOStars->SetWorldLocationAndRotation(Head + FVector(0.f, 0.f, 45.f), FRotator(0.f, GetWorld()->GetTimeSeconds() * 360.f, 0.f));
		}
	}
	if (bWobblePending && GetCondition() != EPVVCondition::KnockedDown && !GetCurrentBoat() && !GetCarrier())
	{
		EnableWobble(); // retries until the body has been posed
	}
	if (bFacePending && GetNetMode() != NM_DedicatedServer && GetWorld()->GetTimeSeconds() >= FaceReadyTime)
	{
		bFacePending = false;
		ApplyFaceShape(PendingFace);
	}

	if (HasAuthority() && GS)
	{
		const float Now = GetWorld()->GetTimeSeconds();

		// Hunger (Runner players only - Wardens are government-fed, NPCs eat at home).
		if (IsRunner() && !bIsNPC && GetBRPlayerState() && GetCondition() != EPVVCondition::Detained && GS->Phase != EBRPhase::Ended)
		{
			const float Drain = HungerPerSecond * (IsSprinting() ? 1.8f : 1.f) * (bInRiver ? 2.f : 1.f) * (GS->IsNight() ? 0.8f : 1.f)
				* (WornHat == EBRHat::Sombrero ? 0.6f : 1.f);
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
			float V = GS->IsNight() ? 0.3f : (WornHat == EBRHat::Sombrero ? 0.85f : 1.f);
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
			AddMovementInput(BRMap::RiverFlow(GetWorld(), Location), Strength);
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

// -------------------------------------------------------------------------------------------
// Brawling: slaps, grab & throw, dive tackles, shoulder rides, props

void ABRCharacter::Input_Grab() { Server_Grab(); }
bool ABRCharacter::IsHoldingProp() const { return HeldProp != nullptr; }
void ABRCharacter::Input_Dive() { Server_Dive(); }

bool ABRCharacter::HandleJumpPressed()
{
	if (bOnShoulders)
	{
		Server_LeapOff();
		return true;
	}
	if (GetCondition() == EPVVCondition::KnockedDown)
	{
		Server_Mash(); // mash Space to wake up
		return true;
	}
	if (GrabbedBy)
	{
		Server_GrabStruggle(); // mash Space to wriggle out of their hands
		return true;
	}
	if (bClimbing)
	{
		Server_ClimbJump(); // hanging at the top: pull up. Otherwise: let go.
		return true;
	}
	return false;
}

void ABRCharacter::Multicast_Sfx_Implementation(uint8 Which)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	USoundBase* Sound = nullptr;
	switch (Which)
	{
	case 0: Sound = SlapSound.Get(); break;
	case 1: Sound = TackleSound.Get(); break;
	case 2: Sound = ThrowSound.Get(); break;
	case 3: Sound = PunchHitSound.Get(); break;
	case 4: Sound = KnockoutSound.Get(); break;
	case 5: Sound = GrabSound.Get(); break;
	default: break;
	}
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
	}
}

void ABRCharacter::SetHeldProp(ABRPhysicsProp* Prop)
{
	HeldProp = Prop;
	if (BaseAttackDamage < 0.f)
	{
		BaseAttackDamage = AttackDamage;
		BaseKnockback = KnockbackStrength;
	}
	// Swinging a crate hurts more than a fist.
	AttackDamage = Prop ? BaseAttackDamage * 2.f : BaseAttackDamage;
	KnockbackStrength = Prop ? BaseKnockback * 1.8f : BaseKnockback;
}

void ABRCharacter::Server_Grab_Implementation()
{
	if (GetCondition() != EPVVCondition::Normal || GetCurrentBoat() || GetCarrier() || bInCannon || bOnShoulders)
	{
		return;
	}
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	BlowDisguise();

	// Holding someone with your hands? THROW them.
	if (ABRCharacter* Held = HandGrabTarget.Get())
	{
		ReleaseHandGrab();
		Held->LaunchCharacter(Forward * ThrowSpeed + FVector(0.f, 0.f, ThrowSpeed * 0.45f), true, true);
		if (ABRPlayerState* PS = GetBRPlayerState())
		{
			++PS->StatThrows;
		}
		if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
		{
			GM->Feed(FString::Printf(TEXT("%s THREW %s"), *GetPlayerDisplayName(), *Held->GetPlayerDisplayName()));
		}
		Multicast_Sfx(2);
		return;
	}
	// Holding something? Throw it.
	if (HeldProp)
	{
		ABRPhysicsProp* Prop = HeldProp;
		SetHeldProp(nullptr);
		Prop->Throw(Forward * ThrowSpeed + FVector(0.f, 0.f, ThrowSpeed * 0.35f) + GetVelocity() * 0.5f);
		Multicast_Sfx(2);
		return;
	}
	if (APVVCharacter* Held = GetCarriedCharacter())
	{
		TakeCarried();
		Held->TeleportTo(GetActorLocation() + Forward * 100.f + FVector(0.f, 0.f, 60.f), Held->GetActorRotation(), false, true);
		if (ABRCharacter* Flying = Cast<ABRCharacter>(Held))
		{
			Flying->bFlying = false;
			Flying->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		}
		Held->LaunchCharacter(Forward * ThrowSpeed + FVector(0.f, 0.f, ThrowSpeed * 0.45f), true, true);
		if (ABRPlayerState* PS = GetBRPlayerState())
		{
			++PS->StatThrows;
		}
		if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
		{
			GM->Feed(FString::Printf(TEXT("%s THREW %s"), *GetPlayerDisplayName(), *Held->GetPlayerDisplayName()));
		}
		Multicast_Sfx(2);
		return;
	}

	// K9: Z on a dog puts it on your leash; Z again lets it loose to chase.
	if (ABRDog* Dog = LeashedDog.Get())
	{
		Dog->ReleaseLeash();
		LeashedDog = nullptr;
		Notify(LOCTEXT("DogLoose", "SIC 'EM! The dog's off the leash."));
		ForceNetUpdate();
		return;
	}
	if (IsWarden())
	{
		for (TActorIterator<ABRDog> It(GetWorld()); It; ++It)
		{
			if (!It->HasHandler() && FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < 220.f)
			{
				It->SetHandler(this);
				LeashedDog = *It;
				Notify(LOCTEXT("DogLeashed", "Dog on the leash. It'll pull toward Runners it smells. Z = let it loose."));
				ForceNetUpdate();
				return;
			}
		}
	}

	// Find the closest thing in front of us.
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BRGrab), false, this);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
	GetWorld()->OverlapMultiByObjectType(Overlaps, GetActorLocation() + Forward * 90.f, FQuat::Identity, Objects, FCollisionShape::MakeSphere(130.f), Params);
	AActor* Best = nullptr;
	float BestDist = MAX_flt;
	for (const FOverlapResult& O : Overlaps)
	{
		AActor* A = O.GetActor();
		const ABRPhysicsProp* Prop = Cast<ABRPhysicsProp>(A);
		const ABRCharacter* Other = Cast<ABRCharacter>(A);
		if ((Prop && !Prop->IsHeld()) || (Other && Other != this && !Other->GetCarrier() && !Other->GetCurrentBoat() && !Other->IsInCannon()
			&& Other->GetCondition() != EPVVCondition::Detained))
		{
			const float D = FVector::DistSquared(A->GetActorLocation(), GetActorLocation());
			if (D < BestDist)
			{
				BestDist = D;
				Best = A;
			}
		}
	}
	if (ABRPhysicsProp* Prop = Cast<ABRPhysicsProp>(Best))
	{
		Prop->PickUp(this);
		SetHeldProp(Prop);
		Notify(LOCTEXT("PropGrabbed", "Got it! Left click to swing, Z to throw."));
		return;
	}
	ABRCharacter* Target = Cast<ABRCharacter>(Best);
	if (!Target)
	{
		if (!TryClimb()) // nothing to grab: grab the ledge in front and climb
		{
			StartWallClimb(); // too tall to vault: climb it hand over hand
		}
		return;
	}
	// Knocked out (or zip-tied): scoop them up and carry them. Z again throws them.
	if (Target->GetCondition() == EPVVCondition::KnockedDown || Target->GetCondition() == EPVVCondition::Restrained)
	{
		Grab(Target);
		if (GetCarriedCharacter() == Target)
		{
			GrabStartTime = GetWorld()->GetTimeSeconds();
			Multicast_Sfx(5);
			Notify(LOCTEXT("PickedUpBody", "Picked them up! Z = throw. Border Patrol: carry them to the pen or a truck."));
		}
		return;
	}
	// A teammate standing up: climb on their shoulders. Anyone else: grab them.
	if (Target->GetFaction() == GetFaction() && !Target->IsNPC() && !IsNPC() && Target->GetCondition() == EPVVCondition::Normal && !Target->IsOnShoulders())
	{
		ClimbOnto(Target);
		return;
	}
	// Grab them with both hands (they can mash Space to wriggle free).
	ArmsReaching = 3;
	if (!TryHandGrab(3))
	{
		ArmsReaching = 0;
	}
}

void ABRCharacter::Server_Dive_Implementation()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (GetCondition() != EPVVCondition::Normal || GetCurrentBoat() || GetCarrier() || GetCarriedCharacter() || bInCannon || bOnShoulders
		|| GetCharacterMovement()->IsFalling() || Now - LastDiveTime < DiveCooldown)
	{
		return;
	}
	LastDiveTime = Now;
	bDiving = true;
	BlowDisguise();
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	const float MaskBoost = WornHat == EBRHat::LuchaMask ? 1.4f : 1.f; // Lucha Mask: flying dives
	LaunchCharacter(Forward * DiveSpeed * MaskBoost + FVector(0.f, 0.f, 350.f), true, true);
	Multicast_DiveLunge();
}

void ABRCharacter::ClimbOnto(ABRCharacter* Base)
{
	if (!Base || !HasAuthority())
	{
		return;
	}
	ShoulderBase = Base;
	bOnShoulders = true;
	AttachToComponent(Base->GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	SetActorRelativeLocation(FVector(0.f, 0.f, Base->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2.f + 15.f));
	OnRep_Shoulders();
	Notify(LOCTEXT("OnShoulders", "On your friend's shoulders! SPACE to leap (great for getting over the Wall)."));
	Base->Notify(LOCTEXT("Carrying", "Someone's on your shoulders. Walk them to the Wall!"));
	ForceNetUpdate();
}

void ABRCharacter::LeapOff(const FVector& Velocity)
{
	if (!bOnShoulders)
	{
		return;
	}
	bOnShoulders = false;
	ShoulderBase = nullptr;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	OnRep_Shoulders();
	LaunchCharacter(Velocity, true, true);
	ForceNetUpdate();
}

void ABRCharacter::Server_LeapOff_Implementation()
{
	const FVector Forward = FRotator(0.f, GetControlRotation().Yaw, 0.f).Vector();
	LeapOff(Forward * 450.f + FVector(0.f, 0.f, ShoulderLeap));
}

void ABRCharacter::OnRep_Shoulders()
{
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, bOnShoulders ? ECR_Ignore : ECR_Block);
	if (bOnShoulders)
	{
		DisableWobble();
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
	}
	else
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		EnableWobble();
	}
}

void ABRCharacter::TickBrawl(float DeltaSeconds)
{
	const float Now = GetWorld()->GetTimeSeconds();

	// Shoulder rider falls off if the person underneath goes down / gets in a car.
	if (bOnShoulders)
	{
		const ABRCharacter* Base = ShoulderBase.Get();
		if (!Base || Base->GetCondition() != EPVVCondition::Normal || Base->GetCurrentBoat() || Base->GetCarrier() || GetCondition() != EPVVCondition::Normal)
		{
			LeapOff(FVector(0.f, 0.f, 200.f));
		}
		return;
	}

	// Drop what you're holding if you get knocked down / grabbed / get in a car.
	if (HeldProp && (GetCondition() != EPVVCondition::Normal || GetCarrier() || GetCurrentBoat() || bInCannon))
	{
		HeldProp->Drop();
		SetHeldProp(nullptr);
	}

	// Someone grabbed (not zip-tied) wriggles free after a few seconds.
	if (APVVCharacter* Held = GetCarriedCharacter())
	{
		// Awake (not knocked out, not zip-tied): they wriggle free after a few seconds.
		if (Held->GetCondition() == EPVVCondition::Normal && Now - GrabStartTime > GrabHoldSeconds)
		{
			DropCarried();
		}
	}

	// Dive tackle: knock over the first person we fly into.
	if (bDiving)
	{
		TArray<FOverlapResult> Hits;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BRTackle), false, this);
		const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
		GetWorld()->OverlapMultiByObjectType(Hits, GetActorLocation() + Forward * 90.f, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(110.f), Params);
		for (const FOverlapResult& H : Hits)
		{
			APVVCharacter* Victim = Cast<APVVCharacter>(H.GetActor());
			if (Victim && Victim != this && Victim->GetCondition() == EPVVCondition::Normal && !Victim->GetCarrier() && !Victim->GetCurrentBoat())
			{
				bDiving = false;
				const float TackleBoost = WornHat == EBRHat::LuchaMask ? 1.5f : 1.f;
				Victim->ApplyKnockdown(this, (Forward * 650.f + FVector(0.f, 0.f, 300.f)) * TackleBoost);
				if (ABRPlayerState* PS = GetBRPlayerState())
				{
					++PS->StatTackles;
				}
				Multicast_Sfx(1);
				break;
			}
		}
	}

	TickHands(DeltaSeconds);
	TickHandGrab(DeltaSeconds);
	TickClimb(DeltaSeconds);

	// (Old fist check, only used when the hands can't be tracked.)
	if (!bHandsTracked && Now < PunchUntil && !bPunchConnected && GetMesh())
	{
		const FName Hand = bPunchLeft ? FName(TEXT("hand_l")) : FName(TEXT("hand_r"));
		TArray<FOverlapResult> Hits;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BRPunch), false, this);
		GetWorld()->OverlapMultiByObjectType(Hits, GetMesh()->GetBoneLocation(Hand), FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(55.f), Params);
		for (const FOverlapResult& H : Hits)
		{
			APVVCharacter* Victim = Cast<APVVCharacter>(H.GetActor());
			if (Victim && Victim != this && !Victim->GetCarrier())
			{
				bPunchConnected = true;
				const FVector Dir = (Victim->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
				Victim->ReceiveMeleeHit(this, AttackDamage, Dir * KnockbackStrength + FVector(0.f, 0.f, 200.f));
				Multicast_Sfx(3);
				break;
			}
		}
	}
}

// -------------------------------------------------------------------------------------------
// Hand grabs (Gang Beasts): hold a mouse button to reach out and grab with that hand

void ABRCharacter::Input_LeftHandStart()
{
	if (bClimbing)
	{
		Server_ClimbStep(true);
		return;
	}
	bLeftDown = true;
	LeftDownAt = GetWorld()->GetTimeSeconds();
	if (CanAttack())
	{
		Server_PunchHand(true);
	}
}

void ABRCharacter::Input_LeftHandStop()
{
	bLeftDown = false;
	if (SentReach & 1)
	{
		SentReach = static_cast<uint8>(SentReach & ~1);
		Server_HandHold(1, false);
	}
}

void ABRCharacter::Input_RightHandStop()
{
	bRightDown = false;
	if (SentReach & 2)
	{
		SentReach = static_cast<uint8>(SentReach & ~2);
		Server_HandHold(2, false);
	}
}

void ABRCharacter::Server_PunchHand_Implementation(bool bLeft)
{
	StopEmote();
	Multicast_PunchHand(bLeft);
	BotAttack(); // same punch as before, just with the hand you chose
}

void ABRCharacter::Multicast_PunchHand_Implementation(bool bLeft)
{
	ForcedPunchHand = bLeft ? 1 : 0;
}

void ABRCharacter::Server_HandHold_Implementation(uint8 Hand, bool bDown)
{
	Hand = static_cast<uint8>(Hand & 3);
	if (bDown)
	{
		ArmsReaching = static_cast<uint8>(ArmsReaching | Hand);
		return;
	}
	ArmsReaching = static_cast<uint8>(ArmsReaching & ~Hand);
	HandGrabMask = static_cast<uint8>(HandGrabMask & ~Hand);
	if (HandGrabMask == 0 && HandGrabTarget)
	{
		ReleaseHandGrab();
	}
}

bool ABRCharacter::IsHeldByHands(const APVVCharacter* By) const
{
	return By && GrabbedBy.Get() == By;
}

bool ABRCharacter::TryHandGrab(uint8 HandMask)
{
	if (!HasAuthority() || GetCondition() != EPVVCondition::Normal || GetCarriedCharacter() || GetCurrentBoat() || HeldProp)
	{
		return false;
	}
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	ABRCharacter* Best = HandGrabTarget.Get();
	if (!Best)
	{
		float BestDist = 170.f;
		for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
		{
			ABRCharacter* Other = *It;
			if (Other == this || Other->GetCarrier() || Other->GetCurrentBoat() || Other->IsInCannon() || Other->GrabbedBy
				|| Other->GetCondition() == EPVVCondition::Detained || Other->GetCondition() == EPVVCondition::Restrained)
			{
				continue;
			}
			const FVector To = Other->GetActorLocation() - GetActorLocation();
			const float D = To.Size2D();
			if (D < BestDist && FVector::DotProduct(To.GetSafeNormal2D(), Forward) > 0.3f && FMath::Abs(To.Z) < 150.f)
			{
				BestDist = D;
				Best = Other;
			}
		}
	}
	if (!Best)
	{
		return false;
	}
	if (Best->IsClimbing())
	{
		// Grabbed someone on the wall: yank them off it!
		Best->StopClimbing(true);
		Best->LaunchCharacter((GetActorLocation() - Best->GetActorLocation()).GetSafeNormal2D() * 400.f + FVector(0.f, 0.f, 100.f), true, true);
		Multicast_Sfx(2);
		return true;
	}
	const bool bNew = HandGrabTarget.Get() != Best;
	HandGrabTarget = Best;
	HandGrabMask = static_cast<uint8>(HandGrabMask | HandMask);
	if (bNew)
	{
		Best->GrabbedBy = this;
		Best->GrabStruggle = 0.f;
		Best->CancelHold();
		Best->UpdateMovementSpeed();
		Best->BlowDisguise();
		BlowDisguise();
		Multicast_Sfx(5);
		Notify(IsWarden() ? LOCTEXT("GrabbedW", "Got them! Other click = punch, Z = throw, hold E = zip-tie (they can fight it).")
			: LOCTEXT("GrabbedR", "Got them! Other click = punch, Z = throw."));
		Best->Notify(LOCTEXT("YouGrabbed", "GRABBED! Mash SPACE to wriggle free (or punch back)."));
	}
	ForceNetUpdate();
	return true;
}

void ABRCharacter::ReleaseHandGrab()
{
	if (ABRCharacter* Held = HandGrabTarget.Get())
	{
		if (Held->GrabbedBy.Get() == this)
		{
			Held->GrabbedBy = nullptr;
			Held->GrabStruggle = 0.f;
			Held->UpdateMovementSpeed();
			Held->ForceNetUpdate();
		}
	}
	HandGrabTarget = nullptr;
	HandGrabMask = 0;
	ArmsReaching = 0; // let go completely (press again to grab again)
	ForceNetUpdate();
}

void ABRCharacter::Server_GrabStruggle_Implementation()
{
	ABRCharacter* Holder = GrabbedBy.Get();
	const float Now = GetWorld()->GetTimeSeconds();
	if (!Holder || Now - LastMashTime < 0.08f)
	{
		return;
	}
	LastMashTime = Now;
	// One hand is easier to slip than two.
	GrabStruggle += Holder->HandGrabMask == 3 ? 0.07f : 0.12f;
	if (GrabStruggle >= 1.f)
	{
		Holder->ReleaseHandGrab();
		Holder->LaunchCharacter((Holder->GetActorLocation() - GetActorLocation()).GetSafeNormal2D() * 300.f, true, false);
		Notify(LOCTEXT("WriggledFree", "You wriggled free!"));
		Holder->Notify(LOCTEXT("TheyEscaped", "They broke free!"));
	}
	ForceNetUpdate();
}

void ABRCharacter::BotStruggleGrab()
{
	Server_GrabStruggle_Implementation();
}

void ABRCharacter::TickHandGrab(float DeltaSeconds)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (GrabStruggle > 0.f)
	{
		GrabStruggle = FMath::Max(0.f, GrabStruggle - 0.12f * DeltaSeconds);
	}
	// Whoever held us is gone (or let go): free to move again.
	if (GrabbedBy && (!IsValid(GrabbedBy) || GrabbedBy->HandGrabTarget.Get() != this))
	{
		GrabbedBy = nullptr;
		GrabStruggle = 0.f;
		UpdateMovementSpeed();
	}
	// NPCs with no papers fight back when grabbed.
	if (bIsNPC && GrabbedBy && bIllegal && FMath::FRand() < DeltaSeconds * 3.f)
	{
		Server_GrabStruggle_Implementation();
	}
	ABRCharacter* Held = HandGrabTarget.Get();
	if (!Held)
	{
		// Reaching with an empty hand: grab whoever it touches.
		if (ArmsReaching != 0 && Now - LastGrabTryTime > 0.1f)
		{
			LastGrabTryTime = Now;
			TryHandGrab(ArmsReaching);
		}
		return;
	}
	const float Dist = FVector::Dist2D(Held->GetActorLocation(), GetActorLocation());
	if (GetCondition() != EPVVCondition::Normal || GetCurrentBoat() || GetCarrier() || Held->GetCurrentBoat() || Held->GetCarrier()
		|| Held->GetCondition() == EPVVCondition::Detained || Held->GetCondition() == EPVVCondition::Restrained || Dist > 280.f
		|| (HandGrabMask == 0 && ArmsReaching == 0))
	{
		ReleaseHandGrab();
		return;
	}
	// Grip more hands if they're reaching too.
	HandGrabMask = static_cast<uint8>(HandGrabMask | ArmsReaching);
	// Really hold them: they stay at arm's length in front of you and get dragged wherever you go.
	if (Held->GetCondition() == EPVVCondition::Normal)
	{
		const FVector Want = GetActorLocation() + GetActorForwardVector().GetSafeNormal2D() * 90.f;
		FVector Next = FMath::VInterpTo(Held->GetActorLocation(), FVector(Want.X, Want.Y, Held->GetActorLocation().Z), DeltaSeconds, 12.f);
		Held->SetActorLocation(Next, true, nullptr, ETeleportType::None);
		Held->GetCharacterMovement()->Velocity = GetVelocity();
		const FRotator Face = (GetActorLocation() - Held->GetActorLocation()).Rotation();
		Held->SetActorRotation(FRotator(0.f, Face.Yaw, 0.f));
	}
}

void ABRCharacter::TickReachVisuals()
{
	USkeletalMeshComponent* Body = GetMesh();
	// Local player: tell the server when a mouse button has been held long enough to reach.
	if (IsLocallyControlled() && Cast<APlayerController>(GetController()))
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (bLeftDown && !(SentReach & 1) && Now - LeftDownAt > 0.22f)
		{
			SentReach = static_cast<uint8>(SentReach | 1);
			Server_HandHold(1, true);
		}
		if (bRightDown && !(SentReach & 2) && Now - RightDownAt > 0.22f)
		{
			SentReach = static_cast<uint8>(SentReach | 2);
			Server_HandHold(2, true);
		}
	}
	if (!bWobbleOn || !Body || Body->GetBoneIndex(TEXT("hand_l")) == INDEX_NONE)
	{
		return;
	}
	// Loosen the reaching arms so they can stretch out (climbing = both arms up on the wall).
	const uint8 Reach = bClimbing ? 3 : ArmsReaching;
	if (Reach != AppliedReachMuscles)
	{
		AppliedReachMuscles = Reach;
		ApplyMuscles(CurrentArmMuscle());
		FPhysicalAnimationData Loose;
		Loose.bIsLocalSimulation = false;
		Loose.OrientationStrength = 40.f;
		Loose.AngularVelocityStrength = 4.f;
		if (Reach & 1) PhysicalAnimation->ApplyPhysicalAnimationSettingsBelow(TEXT("clavicle_l"), Loose, true);
		if (Reach & 2) PhysicalAnimation->ApplyPhysicalAnimationSettingsBelow(TEXT("clavicle_r"), Loose, true);
	}
	if (Reach == 0)
	{
		return;
	}
	if (bClimbing)
	{
		// Hands on the wall: the last hand you moved is higher.
		const FVector Wall = -FVector(ClimbNormal);
		const FName ClimbHands[2] = { FName(TEXT("hand_l")), FName(TEXT("hand_r")) };
		for (int32 i = 0; i < 2; ++i)
		{
			const bool bHigh = bHanging || ((i == 0) == bLastClimbLeft);
			const float Side = i == 0 ? -1.f : 1.f;
			const FVector Point = GetActorLocation() + Wall * 45.f + FVector::CrossProduct(FVector::UpVector, Wall) * Side * 22.f + FVector(0.f, 0.f, bHigh ? 110.f : 60.f);
			const FVector HandAt = Body->GetBoneLocation(ClimbHands[i]);
			Body->SetPhysicsLinearVelocity(((Point - HandAt) * 10.f).GetClampedToMaxSize(900.f), false, ClimbHands[i]);
		}
		return;
	}
	// Steer each reaching hand to the person you're holding (or straight out in front).
	const ABRCharacter* Held = HandGrabTarget.Get();
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	const FName HandBones[2] = { FName(TEXT("hand_l")), FName(TEXT("hand_r")) };
	for (int32 i = 0; i < 2; ++i)
	{
		if (!(ArmsReaching & (1 << i)))
		{
			continue;
		}
		const float Side = i == 0 ? -1.f : 1.f;
		FVector Point = GetActorLocation() + Forward * 75.f + GetActorRightVector() * Side * 18.f + FVector(0.f, 0.f, 45.f);
		if (Held)
		{
			Point = Held->GetActorLocation() + Held->GetActorRightVector() * Side * 14.f + FVector(0.f, 0.f, 40.f);
		}
		const FVector Hand = Body->GetBoneLocation(HandBones[i]);
		Body->SetPhysicsLinearVelocity(((Point - Hand) * 9.f).GetClampedToMaxSize(900.f), false, HandBones[i]);
	}
}

// -------------------------------------------------------------------------------------------
// Hiding spots, cardboard box, lasso, K9 leash

void ABRCharacter::ApplyCameraOptions()
{
	if (FollowCamera && IsLocallyControlled())
	{
		DefaultFOV = FMath::Clamp(FPVVPlayerOptions::FieldOfView, 60.f, 120.f);
		FollowCamera->SetFieldOfView(DefaultFOV);
	}
}

void ABRCharacter::PlayBodySound(const TArray<TSoftObjectPtr<USoundBase>>& List, const FVector& Where, float Volume)
{
	if (USoundBase* Sound = UBRSettings::PickSound(List))
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Where, FMath::Clamp(Volume, 0.05f, 1.5f), FMath::FRandRange(0.92f, 1.08f));
	}
}

void ABRCharacter::TickBodySounds(float DeltaSeconds)
{
	if (GetNetMode() == NM_DedicatedServer || bHiddenInSpot)
	{
		return;
	}
	const UBRSettings* Sounds = GetDefault<UBRSettings>();
	const float Now = GetWorld()->GetTimeSeconds();
	const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	// Footsteps in time with your stride.
	const float Speed = GetVelocity().Size2D();
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	if (GetCondition() == EPVVCondition::Normal && Move && Move->IsMovingOnGround() && Speed > 80.f && !GetCurrentBoat() && !GetCarrier())
	{
		StepClock -= DeltaSeconds;
		if (StepClock <= 0.f)
		{
			StepClock = FMath::Clamp(165.f / Speed, 0.25f, 0.7f);
			PlayBodySound(Sounds->FootstepSounds, GetActorLocation() - FVector(0.f, 0.f, Half), bIsCrouched ? 0.3f : 0.6f);
		}
	}
	else
	{
		StepClock = 0.05f;
	}
	// Ragdoll tumbling: thump when the body slams into the ground.
	if (GetCondition() == EPVVCondition::KnockedDown && GetMesh() && GetMesh()->GetBoneIndex(TEXT("pelvis")) != INDEX_NONE)
	{
		const float Vz = GetMesh()->GetPhysicsLinearVelocity(TEXT("pelvis")).Z;
		if (PelvisFallSpeed < -350.f && Vz > -80.f && Now - LastThumpTime > 0.25f)
		{
			LastThumpTime = Now;
			PlayBodySound(Sounds->BodyThumpSounds, GetMesh()->GetBoneLocation(TEXT("pelvis")), FMath::Clamp(-PelvisFallSpeed / 900.f, 0.3f, 1.2f));
		}
		PelvisFallSpeed = Vz;
	}
	else
	{
		PelvisFallSpeed = 0.f;
	}
	// Screaming while flying through the air.
	if (bFlying && !bYelled)
	{
		bYelled = true;
		PlayBodySound(Sounds->FlyingYellSounds, GetActorLocation());
	}
	else if (!bFlying)
	{
		bYelled = false;
	}
}

ABRDog* ABRCharacter::GetLeashedDog() const
{
	return LeashedDog.Get();
}

bool ABRCharacter::IsHiddenFromPatrol() const
{
	return bHiddenInSpot || (bInBox && GetVelocity().Size2D() < 30.f);
}

void ABRCharacter::SetHiddenInSpot(bool bHide, const FVector& ExitAt)
{
	if (!HasAuthority() || bHide == bHiddenInSpot)
	{
		return;
	}
	bHiddenInSpot = bHide;
	if (bHide)
	{
		ReleaseHandGrab();
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
	}
	else
	{
		TeleportTo(ExitAt, GetActorRotation(), false, true);
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
	OnRep_HiddenInSpot();
	ForceNetUpdate();
}

void ABRCharacter::OnRep_HiddenInSpot()
{
	SetActorHiddenInGame(bHiddenInSpot);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, bHiddenInSpot ? ECR_Ignore : ECR_Block);
}

void ABRCharacter::ToggleBox()
{
	if (!HasAuthority())
	{
		return;
	}
	bInBox = !bInBox;
	OnRep_Box();
	Notify(bInBox ? LOCTEXT("BoxOn", "In the box. Stand still and nobody sees you. Don't run!") : LOCTEXT("BoxOff", "Out of the box."));
	ForceNetUpdate();
}

void ABRCharacter::OnRep_Box()
{
	if (BoxMesh)
	{
		BoxMesh->SetVisibility(bInBox);
	}
	UpdateMovementSpeed();
}

void ABRCharacter::Server_Lasso_Implementation(FVector_NetQuantize From, FVector_NetQuantizeNormal Dir)
{
	const float Now = GetWorld()->GetTimeSeconds();
	const ABRPlayerState* PS = GetBRPlayerState();
	if (!PS || !PS->HasItem(EBRItem::Lasso) || GetCondition() != EPVVCondition::Normal || GetCarrier() || GetCurrentBoat() || Now - LastLassoTime < 4.f)
	{
		return;
	}
	LastLassoTime = Now;
	BlowDisguise();
	// Throw from your shoulder, along your view.
	const FVector Start = GetActorLocation() + FVector(0.f, 0.f, 60.f);
	const FVector Aim = FVector(Dir).GetSafeNormal();
	const FVector End = Start + Aim * 1800.f;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BRLasso), false, this);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	const bool bHit = GetWorld()->SweepSingleByObjectType(Hit, Start, End, FQuat::Identity, Objects, FCollisionShape::MakeSphere(35.f), Params);
	ABRCharacter* Victim = bHit ? Cast<ABRCharacter>(Hit.GetActor()) : nullptr;
	Multicast_LassoRope(bHit ? FVector(Hit.ImpactPoint) : End);
	if (!Victim || Victim->GetCarrier() || Victim->GetCurrentBoat() || Victim->IsHidingInSpot() || Victim->GetCondition() == EPVVCondition::Detained)
	{
		return;
	}
	// YEEHAW: yank them off their feet toward you (and off walls / ladders).
	Victim->StopClimbing(true);
	Victim->BlowDisguise();
	const FVector Yank = (GetActorLocation() - Victim->GetActorLocation()).GetSafeNormal2D() * 900.f + FVector(0.f, 0.f, 300.f);
	Victim->ApplyKnockdown(this, Yank);
	Multicast_Sfx(2);
	if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
	{
		GM->Feed(FString::Printf(TEXT("%s LASSOED %s"), *GetPlayerDisplayName(), *Victim->GetPlayerDisplayName()));
	}
}

void ABRCharacter::Multicast_LassoRope_Implementation(FVector_NetQuantize End)
{
	LassoRopeEnd = End;
	LassoRopeUntil = GetWorld()->GetTimeSeconds() + 0.6f;
}

void ABRCharacter::DrawRope(const FVector& From, const FVector& To)
{
	const FVector Span = To - From;
	const float Len = Span.Size();
	if (Len < 1.f)
	{
		RopeMesh->SetVisibility(false);
		return;
	}
	RopeMesh->SetWorldLocationAndRotation((From + To) * 0.5f, FRotationMatrix::MakeFromZ(Span / Len).Rotator());
	RopeMesh->SetWorldScale3D(FVector(0.03f, 0.03f, Len / 100.f));
	RopeMesh->SetVisibility(true);
}

void ABRCharacter::TickRope()
{
	if (!RopeMesh || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const FVector Hand = GetMesh() && GetMesh()->GetBoneIndex(TEXT("hand_r")) != INDEX_NONE ? GetMesh()->GetBoneLocation(TEXT("hand_r")) : GetActorLocation() + FVector(0.f, 0.f, 40.f);
	if (GetWorld()->GetTimeSeconds() < LassoRopeUntil)
	{
		DrawRope(Hand, LassoRopeEnd);
	}
	else if (const ABRDog* Dog = LeashedDog.Get())
	{
		DrawRope(Hand, Dog->GetActorLocation() + FVector(0.f, 0.f, 20.f));
	}
	else if (RopeMesh->IsVisible())
	{
		RopeMesh->SetVisibility(false);
	}
}

// -------------------------------------------------------------------------------------------
// Kick + headbutt

void ABRCharacter::BrawlerAct(ABRCharacter* Foe, float Aggression, float DeltaSeconds)
{
	if (!HasAuthority() || !Foe || GetCondition() != EPVVCondition::Normal || GetCarrier() || GetCurrentBoat())
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	const FVector ToFoe = (Foe->GetActorLocation() - GetActorLocation()) * FVector(1.f, 1.f, 0.f);
	const float Dist = ToFoe.Size();
	// Windmill: spin with floppy arms; any hand that whips past someone hits them.
	if (Now < BrawlSpinUntil)
	{
		AddActorWorldRotation(FRotator(0.f, 1100.f * DeltaSeconds, 0.f));
		return;
	}
	// Being held: fight your way out.
	if (GrabbedBy)
	{
		if (Now >= BrawlNextMove)
		{
			BrawlNextMove = Now + 0.22f;
			if (FMath::FRand() < 0.3f)
			{
				SetActorRotation(FRotator(0.f, (GrabbedBy->GetActorLocation() - GetActorLocation()).Rotation().Yaw, 0.f));
				DoHeadbutt();
			}
			else
			{
				BotStruggleGrab();
			}
		}
		return;
	}
	if (Now < BrawlNextMove)
	{
		return;
	}
	const float Pace = FMath::Lerp(1.f, 0.35f, FMath::Clamp(Aggression, 0.f, 1.f));
	BrawlNextMove = Now + Pace * FMath::FRandRange(0.7f, 1.3f);
	if (Foe->GetCondition() != EPVVCondition::Normal)
	{
		return; // they're down: let them get up (the crowd loves it)
	}
	SetActorRotation(FRotator(0.f, ToFoe.Rotation().Yaw, 0.f));
	// Got them in your hands: beat on them, then THROW them.
	if (HandGrabTarget.Get() == Foe)
	{
		if (Now > BrawlHoldUntil)
		{
			Server_Grab_Implementation(); // throw
			return;
		}
		const float Roll = FMath::FRand();
		if (Roll < 0.55f)
		{
			Server_PunchHand_Implementation(bBrawlLeft);
			bBrawlLeft = !bBrawlLeft;
			BrawlNextMove = Now + 0.35f;
		}
		else if (Roll < 0.85f)
		{
			DoHeadbutt();
		}
		else
		{
			DoKick();
		}
		return;
	}
	if (Dist < 175.f)
	{
		const float Roll = FMath::FRand();
		if (Roll < 0.32f)
		{
			Server_PunchHand_Implementation(bBrawlLeft); // one-two combos
			bBrawlLeft = !bBrawlLeft;
			BrawlNextMove = Now + 0.3f;
		}
		else if (Roll < 0.52f)
		{
			ArmsReaching = 3;
			if (TryHandGrab(3))
			{
				BrawlHoldUntil = Now + FMath::FRandRange(1.5f, 3.f);
			}
			else
			{
				ArmsReaching = 0;
			}
		}
		else if (Roll < 0.67f)
		{
			DoHeadbutt();
		}
		else if (Roll < 0.82f)
		{
			DoKick();
		}
		else
		{
			BrawlSpinUntil = Now + 0.7f; // windmill!
			BlowDisguise();
		}
	}
	else if (Dist < 430.f && FMath::FRand() < 0.3f * Aggression)
	{
		Server_Dive_Implementation(); // flying cross-body
	}
}

void ABRCharacter::Input_Headbutt()
{
	Server_Headbutt();
}

void ABRCharacter::Server_Kick_Implementation() { DoKick(); }
void ABRCharacter::Server_Headbutt_Implementation() { DoHeadbutt(); }

void ABRCharacter::DoKick()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (!HasAuthority() || GetCondition() != EPVVCondition::Normal || GetCarrier() || GetCurrentBoat() || bClimbing || bInCannon || Now - LastKickTime < 0.9f)
	{
		return;
	}
	LastKickTime = Now;
	BlowDisguise();
	Multicast_Kick();
	GetWorldTimerManager().SetTimer(MoveHitTimer, this, &ABRCharacter::KickHit, 0.18f, false);
}

void ABRCharacter::DoHeadbutt()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (!HasAuthority() || GetCondition() != EPVVCondition::Normal || GetCarrier() || GetCurrentBoat() || bClimbing || bInCannon || Now - LastHeadbuttTime < 1.f)
	{
		return;
	}
	LastHeadbuttTime = Now;
	BlowDisguise();
	Multicast_Headbutt();
	GetWorldTimerManager().SetTimer(MoveHitTimer, this, &ABRCharacter::HeadbuttHit, 0.12f, false);
}

void ABRCharacter::Multicast_Kick_Implementation()
{
	PlayOneShot(KickAnim);
	PlayBodySound(GetDefault<UBRSettings>()->WhooshSounds, GetActorLocation(), 0.8f);
	if (bWobbleOn && GetMesh() && GetMesh()->GetBoneIndex(TEXT("foot_r")) != INDEX_NONE)
	{
		const FVector Fwd = GetActorForwardVector().GetSafeNormal2D();
		GetMesh()->AddImpulse(Fwd * 1600.f + FVector(0.f, 0.f, 500.f), TEXT("foot_r"), true);
		GetMesh()->AddImpulse(Fwd * 900.f + FVector(0.f, 0.f, 300.f), TEXT("calf_r"), true);
		GetMesh()->AddImpulseToAllBodiesBelow(-Fwd * 150.f, TEXT("spine_02"), true, true); // lean back into it
	}
}

void ABRCharacter::Multicast_Headbutt_Implementation()
{
	PlayOneShot(HeadbuttAnim);
	PlayBodySound(GetDefault<UBRSettings>()->WhooshSounds, GetActorLocation(), 0.6f);
	if (bWobbleOn && GetMesh() && GetMesh()->GetBoneIndex(TEXT("head")) != INDEX_NONE)
	{
		const FVector Fwd = GetActorForwardVector().GetSafeNormal2D();
		GetMesh()->AddImpulse(Fwd * 900.f, TEXT("head"), true);
		GetMesh()->AddImpulseToAllBodiesBelow(Fwd * 350.f, TEXT("spine_03"), true, true);
	}
}

void ABRCharacter::KickHit()
{
	if (GetCondition() != EPVVCondition::Normal)
	{
		return;
	}
	const FVector Fwd = GetActorForwardVector().GetSafeNormal2D();
	const FVector At = GetActorLocation() + Fwd * 85.f - FVector(0.f, 0.f, 45.f);
	TArray<FOverlapResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BRKick), false, this);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
	GetWorld()->OverlapMultiByObjectType(Hits, At, FQuat::Identity, Objects, FCollisionShape::MakeSphere(60.f), Params);
	TSet<AActor*> Done;
	for (const FOverlapResult& H : Hits)
	{
		AActor* A = H.GetActor();
		if (!A || A == this || Done.Contains(A))
		{
			continue;
		}
		Done.Add(A);
		if (ABRPhysicsProp* Prop = Cast<ABRPhysicsProp>(A); Prop && !Prop->IsHeld())
		{
			if (UPrimitiveComponent* PropBody = Cast<UPrimitiveComponent>(Prop->GetRootComponent()))
			{
				PropBody->AddImpulse(Fwd * 900.f + FVector(0.f, 0.f, 400.f), NAME_None, true); // punt it
			}
			continue;
		}
		APVVCharacter* Victim = Cast<APVVCharacter>(A);
		if (!Victim || Victim->GetCarrier() || Victim->GetCondition() == EPVVCondition::Detained)
		{
			continue;
		}
		Victim->ReceiveMeleeHit(this, KickDamage, Fwd * KickPush + FVector(0.f, 0.f, 250.f));
		if (ABRCharacter* BRVictim = Cast<ABRCharacter>(Victim))
		{
			BRVictim->Multicast_BodyHit(TEXT("pelvis"), Fwd * HitReaction * 1.2f);
		}
		Multicast_Sfx(3);
	}
}

void ABRCharacter::HeadbuttHit()
{
	if (GetCondition() != EPVVCondition::Normal || !GetMesh())
	{
		return;
	}
	const FVector Fwd = GetActorForwardVector().GetSafeNormal2D();
	const FVector Head = GetMesh()->GetBoneIndex(TEXT("head")) != INDEX_NONE ? GetMesh()->GetBoneLocation(TEXT("head")) : GetActorLocation() + FVector(0.f, 0.f, 70.f);
	TArray<FOverlapResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BRHeadbutt), false, this);
	GetWorld()->OverlapMultiByObjectType(Hits, Head + Fwd * 40.f, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(50.f), Params);
	for (const FOverlapResult& H : Hits)
	{
		ABRCharacter* Victim = Cast<ABRCharacter>(H.GetActor());
		if (!Victim || Victim == this || Victim->GetCarrier() || Victim->GetCondition() != EPVVCondition::Normal)
		{
			continue;
		}
		// Headbutting whoever's holding you breaks their grip.
		if (GrabbedBy.Get() == Victim)
		{
			Victim->ReleaseHandGrab();
			Notify(LOCTEXT("HeadbuttFree", "HEADBUTT! You broke their grip."));
		}
		Victim->ReceiveMeleeHit(this, HeadbuttDamage, Fwd * 450.f + FVector(0.f, 0.f, 120.f));
		Victim->Multicast_BodyHit(TEXT("head"), Fwd * HitReaction * 1.5f);
		Multicast_Sfx(3);
		return;
	}
}

// -------------------------------------------------------------------------------------------
// Wall climbing (Gang Beasts): Z at a tall wall, then left, right, left, right...

bool ABRCharacter::StartWallClimb()
{
	if (!HasAuthority() || bClimbing || GetCondition() != EPVVCondition::Normal || GetCarriedCharacter() || HeldProp || GetCurrentBoat())
	{
		return false;
	}
	const FVector Fwd = GetActorForwardVector().GetSafeNormal2D();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BRWallClimb), false, this);
	FHitResult Wall;
	if (!GetWorld()->LineTraceSingleByChannel(Wall, GetActorLocation(), GetActorLocation() + Fwd * 120.f, ECC_Visibility, Params)
		|| FMath::Abs(Wall.ImpactNormal.Z) > 0.3f || Cast<APawn>(Wall.GetActor()))
	{
		return false; // nothing climbable (or it's a slope / a person)
	}
	const FVector Normal = Wall.ImpactNormal.GetSafeNormal2D();
	// Find the top of the wall (if it has one within 15 m).
	FHitResult Top;
	const FVector Above = Wall.ImpactPoint - Normal * 30.f + FVector(0.f, 0.f, 1500.f);
	bClimbHasTop = GetWorld()->LineTraceSingleByChannel(Top, Above, Wall.ImpactPoint - Normal * 30.f, ECC_Visibility, Params) && !Top.bStartPenetrating;
	ClimbTopZ = bClimbHasTop ? Top.ImpactPoint.Z : 0.f;
	bClimbingBorderWall = Wall.GetActor() && (Wall.GetActor()->IsA<ABRWall>() || Wall.GetActor()->IsA<ABRWallSegment>());
	ClimbNormal = Normal;
	bClimbing = true;
	bHanging = false;
	bClimbStarted = false;
	ClimbSlips = 0;
	LastClimbStepTime = GetWorld()->GetTimeSeconds();
	NextSensorPing = 0.f;
	ReleaseHandGrab();
	GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	GetCharacterMovement()->StopMovementImmediately();
	SetActorLocation(FVector(Wall.ImpactPoint.X, Wall.ImpactPoint.Y, GetActorLocation().Z) + Normal * (GetCapsuleComponent()->GetScaledCapsuleRadius() + 5.f));
	SetActorRotation(FRotator(0.f, (-Normal).Rotation().Yaw, 0.f));
	OnRep_Climbing();
	BlowDisguise();
	Notify(LOCTEXT("ClimbStart", "CLIMBING! Click LEFT, RIGHT, LEFT, RIGHT... Same hand twice = slip. Space = let go."));
	ForceNetUpdate();
	return true;
}

void ABRCharacter::OnRep_Climbing()
{
	// Nobody walks or flies around while stuck to a wall; the server moves you up.
	GetCharacterMovement()->MaxFlySpeed = bClimbing ? 0.f : 600.f;
	if (!bClimbing)
	{
		AppliedReachMuscles = 0xFF; // relax the arms again
	}
}

void ABRCharacter::StopClimbing(bool bFall)
{
	if (!bClimbing)
	{
		return;
	}
	bClimbing = false;
	bHanging = false;
	OnRep_Climbing();
	GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	if (bFall)
	{
		FallFromZ = GetActorLocation().Z;
		ABRGameState::Sfx(this, EBRSfx::HatPop, GetActorLocation());
	}
	ForceNetUpdate();
}

void ABRCharacter::Server_ClimbStep_Implementation(bool bLeft)
{
	if (!bClimbing || bHanging)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	const bool bRightOrder = !bClimbStarted || bLeft != bLastClimbLeft;
	bLastClimbLeft = bLeft;
	LastClimbStepTime = Now;
	if (!bRightOrder)
	{
		// Same hand twice: you slip down a bit. Three slips and you fall off.
		++ClimbSlips;
		SetActorLocation(GetActorLocation() - FVector(0.f, 0.f, ClimbStep * 1.2f), true);
		if (ClimbSlips >= 3)
		{
			Notify(LOCTEXT("ClimbFell", "You lost your grip!"));
			StopClimbing(true);
		}
		else
		{
			Notify(LOCTEXT("ClimbSlip", "SLIP! Alternate your hands!"));
		}
		return;
	}
	bClimbStarted = true;
	FHitResult Blocked;
	SetActorLocation(GetActorLocation() + FVector(0.f, 0.f, ClimbStep), true, &Blocked);
	if (Blocked.bBlockingHit)
	{
		StopClimbing(false); // roof / overhang
		return;
	}
	// Made it to the top: hang from the ledge.
	const float Hands = GetActorLocation().Z + 90.f;
	if (bClimbHasTop && Hands >= ClimbTopZ)
	{
		bHanging = true;
		Notify(LOCTEXT("ClimbTop", "At the top! SPACE to pull yourself up."));
	}
	ForceNetUpdate();
}

void ABRCharacter::Server_ClimbJump_Implementation()
{
	if (!bClimbing)
	{
		return;
	}
	if (bHanging)
	{
		// Pull up and over onto the top.
		const FVector Wall = -FVector(ClimbNormal);
		const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		bClimbing = false;
		bHanging = false;
		OnRep_Climbing();
		GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		TeleportTo(GetActorLocation() + Wall * 110.f + FVector(0.f, 0.f, (ClimbTopZ - GetActorLocation().Z) + Half + 10.f), GetActorRotation(), false, true);
		ForceNetUpdate();
		return;
	}
	// Let go: push off the wall.
	StopClimbing(true);
	LaunchCharacter(FVector(ClimbNormal) * 300.f + FVector(0.f, 0.f, 150.f), true, true);
}

void ABRCharacter::TickClimb(float DeltaSeconds)
{
	if (!bClimbing)
	{
		return;
	}
	if (GetCondition() != EPVVCondition::Normal || GetCarrier() || GetCurrentBoat())
	{
		StopClimbing(GetCondition() == EPVVCondition::Normal);
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	// Wall sensors on the Border Wall: Border Patrol gets a ping.
	if (bClimbingBorderWall && Now >= NextSensorPing)
	{
		NextSensorPing = Now + 5.f;
		if (ABRGameState* GS = GetBRGameState())
		{
			GS->Multicast_NoisePing(GetActorLocation());
		}
		ABRGameState::Sfx(this, EBRSfx::TripwireAlarm, GetActorLocation());
	}
	// Not climbing for a while: your grip gives out and you slide down.
	if (!bHanging && Now - LastClimbStepTime > 2.5f)
	{
		FHitResult Floor;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BRClimbSlide), false, this);
		const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const bool bNearGround = GetWorld()->LineTraceSingleByChannel(Floor, GetActorLocation(), GetActorLocation() - FVector(0.f, 0.f, Half + 20.f), ECC_Visibility, Params);
		if (bNearGround)
		{
			StopClimbing(false);
			return;
		}
		SetActorLocation(GetActorLocation() - FVector(0.f, 0.f, 60.f * DeltaSeconds), true);
	}
	// Hanging too long at the top: arms give out.
	if (bHanging && Now - LastClimbStepTime > 8.f)
	{
		StopClimbing(true);
	}
	GetCharacterMovement()->Velocity = FVector::ZeroVector;
}

// -------------------------------------------------------------------------------------------
// Hands: any fast-moving hand hits whoever it touches (Gang Beasts)

void ABRCharacter::TickHands(float DeltaSeconds)
{
	USkeletalMeshComponent* Body = GetMesh();
	if (!Body || GetCondition() != EPVVCondition::Normal || GetCarrier() || GetCurrentBoat() || bInCannon || bOnShoulders
		|| Body->GetBoneIndex(TEXT("hand_l")) == INDEX_NONE || DeltaSeconds <= 0.f)
	{
		bHandsTracked = false;
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	const FName Hands[2] = { FName(TEXT("hand_l")), FName(TEXT("hand_r")) };
	for (int32 i = 0; i < 2; ++i)
	{
		const FVector Pos = Body->GetBoneLocation(Hands[i]);
		if (!bHandsTracked)
		{
			LastHandPos[i] = Pos;
			continue;
		}
		// Counts how fast the hand swings (relative to you) AND how fast it actually moves (running into people with your arms out hits too).
		const FVector WorldVel = (Pos - LastHandPos[i]) / DeltaSeconds;
		const FVector RelVel = WorldVel - GetVelocity();
		const FVector HandVel = RelVel.SizeSquared() >= (WorldVel * 0.7f).SizeSquared() ? RelVel : WorldVel * 0.7f;
		LastHandPos[i] = Pos;
		const bool bPunching = Now < PunchUntil && (i == 0) == bPunchLeft;
		if ((ArmsReaching & (1 << i)) && !bPunching)
		{
			continue; // a hand that's reaching / holding doesn't slap
		}
		const float Speed = HandVel.Size();
		if (Now < HandReadyAt[i] || (Speed < SlapSpeed && !bPunching) || Speed > 6000.f)
		{
			continue; // too slow (or a teleport)
		}
		TArray<FOverlapResult> Hits;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BRHand), false, this);
		GetWorld()->OverlapMultiByObjectType(Hits, Pos, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(bPunching ? 34.f : 22.f), Params);
		for (const FOverlapResult& H : Hits)
		{
			APVVCharacter* Victim = Cast<APVVCharacter>(H.GetActor());
			if (!Victim || Victim == this || Victim == GetCarriedCharacter() || Victim->GetCarrier() || Victim->GetCondition() != EPVVCondition::Normal
				|| Victim == HandGrabTarget.Get())
			{
				continue; // (the person you're holding gets hit by your punches instead - see HandlePunchHit)
			}
			const float Hard = FMath::Clamp((Speed - SlapSpeed) / 900.f, 0.f, 1.f);
			float Damage = FMath::Lerp(HandHitMinDamage, HandHitMaxDamage, Hard);
			if (bPunching)
			{
				Damage = FMath::Max(Damage, AttackDamage);
			}
			FName HitBone = TEXT("spine_03");
			if (const USkeletalMeshComponent* VictimBody = Victim->GetMesh(); VictimBody && VictimBody->GetBoneIndex(TEXT("head")) != INDEX_NONE
				&& FVector::Dist(VictimBody->GetBoneLocation(TEXT("head")), Pos) < 40.f)
			{
				Damage *= HeadHitMultiplier; // right on the chin
				HitBone = TEXT("head");
			}
			const FVector Dir = HandVel.GetSafeNormal();
			const FVector Push = Dir.GetSafeNormal2D() * FMath::Lerp(120.f, bPunching ? 520.f : 380.f, Hard) + FVector(0.f, 0.f, 80.f);
			Victim->ReceiveMeleeHit(this, Damage, Push);
			if (ABRCharacter* BRVictim = Cast<ABRCharacter>(Victim))
			{
				BRVictim->Multicast_BodyHit(HitBone, Dir * HitReaction * FMath::Lerp(0.5f, 1.3f, Hard));
			}
			Multicast_Sfx(bPunching || Hard > 0.4f ? 3 : 0);
			HandReadyAt[i] = Now + 0.3f;
			if (bPunching)
			{
				bPunchConnected = true;
				PunchUntil = -1.f;
			}
			break;
		}
	}
	bHandsTracked = true;
}

void ABRCharacter::Multicast_DiveLunge_Implementation()
{
	PlayBodySound(GetDefault<UBRSettings>()->WhooshSounds, GetActorLocation(), 1.f);
	// Superman: throw the upper body and arms forward so the dive reads from far away.
	if (bWobbleOn && GetMesh() && GetMesh()->GetBoneIndex(TEXT("spine_03")) != INDEX_NONE)
	{
		const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
		GetMesh()->AddImpulseToAllBodiesBelow(Forward * 700.f - FVector(0.f, 0.f, 150.f), TEXT("spine_02"), true, true);
		for (const TCHAR* Hand : { TEXT("hand_l"), TEXT("hand_r") })
		{
			if (GetMesh()->GetBoneIndex(Hand) != INDEX_NONE)
			{
				GetMesh()->AddImpulse(Forward * 1400.f, Hand, true);
			}
		}
	}
	if (TackleSound && GetNetMode() != NM_DedicatedServer && IsLocallyControlled())
	{
		AddShake(0.3f);
	}
}

void ABRCharacter::Multicast_BodyHit_Implementation(FName Bone, FVector_NetQuantize Impulse)
{
	USkeletalMeshComponent* Body = GetMesh();
	if (!bWobbleOn || !Body || Body->GetBoneIndex(Bone) == INDEX_NONE)
	{
		return;
	}
	// The part that got hit snaps back; the rest of the upper body follows a bit.
	ABRGameState::SpawnLocalFX(this, EBRFX::PunchHit, Body->GetBoneLocation(Bone));
	Body->AddImpulse(Impulse, Bone, true);
	Body->AddImpulseToAllBodiesBelow(Impulse * 0.35f, TEXT("spine_02"), true, true);
	if (IsLocallyControlled())
	{
		AddShake(0.35f);
	}
}

// -------------------------------------------------------------------------------------------
// Hats, disguise, held items

void ABRCharacter::SetHat(EBRHat Hat)
{
	if (!HasAuthority())
	{
		return;
	}
	WornHat = Hat;
	bHatAssigned = true;
	OnRep_Hat();
	ForceNetUpdate();
}

void ABRCharacter::OnRep_Hat()
{
	UpdateMovementSpeed();
	if (HatMeshes.Num() == 0 || !OutfitHead)
	{
		return; // hat system not set up: keep the Blueprint's OutfitHead
	}
	USkeletalMesh* HatMesh = WornHat == EBRHat::None ? nullptr : HatMeshes.FindRef(WornHat).Get();
	OutfitHead->SetSkeletalMeshAsset(HatMesh);
	OutfitHead->SetLeaderPoseComponent(GetMesh(), true);
	OutfitHead->SetHiddenInGame(HatMesh == nullptr);
}

void ABRCharacter::KnockOffHat()
{
	if (!HasAuthority() || WornHat == EBRHat::None)
	{
		return;
	}
	const FVector Head = GetMesh() && GetMesh()->GetBoneIndex(TEXT("head")) != INDEX_NONE ? GetMesh()->GetBoneLocation(TEXT("head")) : GetActorLocation();
	FHitResult Ground;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BRHatDrop), false, this);
	const FVector Drop = Head + FVector(FMath::FRandRange(-120.f, 120.f), FMath::FRandRange(-120.f, 120.f), 0.f);
	const FVector At = GetWorld()->LineTraceSingleByChannel(Ground, Drop, Drop - FVector(0.f, 0.f, 600.f), ECC_Visibility, Params) ? Ground.ImpactPoint : GetActorLocation();
	const FTransform T(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), At);
	if (ABRHatPickup* Pickup = GetWorld()->SpawnActorDeferred<ABRHatPickup>(ABRHatPickup::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Pickup->Hat = WornHat;
		Pickup->HatMesh = HatMeshes.FindRef(WornHat).Get();
		Pickup->FinishSpawning(T);
	}
	ABRGameState::Sfx(this, EBRSfx::HatPop, Head);
	SetHat(EBRHat::None);
}

float ABRCharacter::GetDamageTakenMultiplier() const
{
	return WornHat == EBRHat::HardHat ? 0.6f : 1.f;
}

void ABRCharacter::Server_SelectSlot_Implementation(int32 Slot)
{
	ServerSelectedSlot = Slot;
}

void ABRCharacter::BlowDisguise()
{
	if (HasAuthority() && bInBox)
	{
		bInBox = false; // the box falls off
		OnRep_Box();
	}
	if (!HasAuthority() || !bDisguised || bIsNPC)
	{
		return;
	}
	if (ABRPlayerState* PS = GetBRPlayerState())
	{
		PS->RemoveItem(EBRItem::Poncho);
	}
	bDisguised = false;
	OnRep_Disguise();
	Notify(LOCTEXT("PonchoRipped", "Your poncho ripped - disguise BLOWN!"));
}

void ABRCharacter::OnRep_Disguise()
{
	if (!DisguiseOutfit)
	{
		return;
	}
	USkeletalMesh* Poncho = bDisguised ? PonchoMesh.Get() : nullptr;
	DisguiseOutfit->SetSkeletalMeshAsset(Poncho);
	DisguiseOutfit->SetLeaderPoseComponent(GetMesh(), true);
	DisguiseOutfit->SetHiddenInGame(Poncho == nullptr);
}

void ABRCharacter::TickOutfitState()
{
	ABRPlayerState* PS = GetBRPlayerState();
	if (bIsNPC || !PS)
	{
		return;
	}
	// Poncho: you look like a villager (scanners say "papers in order") until you sprint, fight or get hit.
	const bool bShouldDisguise = IsRunner() && PS->HasItem(EBRItem::Poncho);
	if (bShouldDisguise && IsSprinting())
	{
		BlowDisguise();
	}
	else if (bShouldDisguise != bDisguised)
	{
		bDisguised = bShouldDisguise;
		OnRep_Disguise();
		if (bDisguised)
		{
			Notify(LOCTEXT("PonchoOn", "Disguised! You look like a local. Don't sprint, fight or get bumped."));
		}
	}
	// Show the selected item in your hand.
	EBRItem Want = PS->Inventory.IsValidIndex(ServerSelectedSlot) ? PS->Inventory[ServerSelectedSlot] : EBRItem::None;
	switch (Want)
	{
	case EBRItem::Trumpet: case EBRItem::Taco: case EBRItem::TacoPlatter: case EBRItem::Drugs:
	case EBRItem::GasCan: case EBRItem::WireCutters: case EBRItem::Shovel: break;
	default: Want = EBRItem::None; break;
	}
	if (bAiming || HeldProp || GetCurrentBoat() || bInCannon)
	{
		Want = EBRItem::None;
	}
	if (Want != HeldItem)
	{
		HeldItem = Want;
		OnRep_HeldItem();
	}
}

void ABRCharacter::OnRep_HeldItem()
{
	if (!HeldItemMesh)
	{
		return;
	}
	if (HeldItem == EBRItem::None)
	{
		HeldItemMesh->SetVisibility(false);
		return;
	}
	UStaticMesh* ItemMesh = HeldItemMeshes.FindRef(HeldItem).Get();
	FVector Scale(1.f);
	FRotator Turn = FRotator::ZeroRotator;
	FLinearColor Color(1.f, 1.f, 1.f);
	if (!ItemMesh)
	{
		// No model assigned: a simple stand-in shape.
		const TCHAR* Shape = TEXT("/Engine/BasicShapes/Cube.Cube");
		switch (HeldItem)
		{
		case EBRItem::Trumpet: Shape = TEXT("/Engine/BasicShapes/Cone.Cone"); Scale = FVector(.12f, .12f, .35f); Turn = FRotator(-90.f, 0.f, 0.f); Color = FLinearColor(1.f, .75f, .2f); break;
		case EBRItem::Taco: Shape = TEXT("/Engine/BasicShapes/Sphere.Sphere"); Scale = FVector(.22f, .1f, .14f); Color = FLinearColor(1.f, .8f, .3f); break;
		case EBRItem::TacoPlatter: Shape = TEXT("/Engine/BasicShapes/Cylinder.Cylinder"); Scale = FVector(.45f, .45f, .04f); Color = FLinearColor(.95f, .95f, .9f); break;
		case EBRItem::Drugs: Scale = FVector(.22f, .16f, .1f); Color = FLinearColor(.9f, .9f, .85f); break;
		case EBRItem::GasCan: Scale = FVector(.22f, .12f, .3f); Color = FLinearColor(.8f, .1f, .05f); break;
		case EBRItem::WireCutters: Scale = FVector(.28f, .04f, .04f); Color = FLinearColor(.3f, .3f, .32f); break;
		case EBRItem::Shovel: Shape = TEXT("/Engine/BasicShapes/Cylinder.Cylinder"); Scale = FVector(.04f, .04f, 1.f); Color = FLinearColor(.45f, .3f, .15f); break;
		default: break;
		}
		ItemMesh = LoadObject<UStaticMesh>(nullptr, Shape);
	}
	HeldItemMesh->SetStaticMesh(ItemMesh);
	HeldItemMesh->SetRelativeTransform(FTransform(Turn, HeldItemOffset.GetLocation(), Scale) * FTransform(HeldItemOffset.GetRotation()));
	if (!HeldItemMeshes.Contains(HeldItem))
	{
		if (UMaterialInstanceDynamic* MID = HeldItemMesh->CreateDynamicMaterialInstance(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
		}
	}
	HeldItemMesh->SetVisibility(true);
}

// -------------------------------------------------------------------------------------------
// Emote wheel, cash drop, head tag

FString ABRCharacter::EmoteName(int32 Index)
{
	switch (Index)
	{
	case 0: return TEXT("Wave");
	case 1: return TEXT("Flex");
	case 2: return TEXT("Taunt");
	case 3: return TEXT("Beg");
	case 4: return TEXT("Cheer");
	case 5: return TEXT("Dance");
	case 6: return TEXT("Smoke");
	default: return TEXT("");
	}
}

void ABRCharacter::Input_EmoteStart()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || bEmoteWheelOpen)
	{
		return;
	}
	bEmoteWheelOpen = true;
	EmoteChoice = -1;
	EmoteAim = FVector2D::ZeroVector;
	PC->SetIgnoreLookInput(true); // the mouse picks a slice instead of turning the camera
}

void ABRCharacter::Input_EmoteStop()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!bEmoteWheelOpen)
	{
		return;
	}
	bEmoteWheelOpen = false;
	if (PC)
	{
		PC->SetIgnoreLookInput(false);
	}
	if (EmoteChoice >= 0)
	{
		Server_Emote(static_cast<uint8>(EmoteChoice));
	}
	EmoteChoice = -1;
}

void ABRCharacter::TickEmoteWheel()
{
	if (!bEmoteWheelOpen)
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		bEmoteWheelOpen = false;
		return;
	}
	float DX = 0.f, DY = 0.f;
	PC->GetInputMouseDelta(DX, DY);
	EmoteAim += FVector2D(DX, DY);
	if (EmoteAim.Size() > 40.f)
	{
		EmoteAim = EmoteAim.GetSafeNormal() * 40.f;
	}
	if (EmoteAim.Size() < 12.f)
	{
		EmoteChoice = -1;
		return;
	}
	// Slice 0 is straight up, going clockwise.
	float Angle = FMath::RadiansToDegrees(FMath::Atan2(EmoteAim.X, EmoteAim.Y));
	Angle = FMath::Fmod(Angle + 360.f + 180.f / NumEmotes, 360.f);
	EmoteChoice = FMath::Clamp(FMath::FloorToInt(Angle / (360.f / NumEmotes)), 0, NumEmotes - 1);
}

void ABRCharacter::Server_Emote_Implementation(uint8 Index)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Index >= NumEmotes || GetCondition() != EPVVCondition::Normal || GetCurrentBoat() || Now - LastEmoteTime < 1.5f)
	{
		return;
	}
	LastEmoteTime = Now;
	// Same emote again = stop. Otherwise it loops until you move or fight.
	ActiveEmote = (ActiveEmote == static_cast<int32>(Index)) ? -1 : static_cast<int32>(Index);
	OnRep_ActiveEmote();
	ForceNetUpdate();
}

UAnimSequenceBase* ABRCharacter::EmoteAnimFor(int32 Index) const
{
	switch (Index)
	{
	case 0: return EmoteWaveAnim.Get();
	case 1: return EmoteFlexAnim.Get();
	case 2: return EmoteTauntAnim.Get();
	case 3: return EmoteBegAnim.Get();
	case 4: return EmoteCheerAnim.Get();
	case 5: return DanceAnim.Get();
	case 6: return EmoteSmokeAnim.Get();
	default: return nullptr;
	}
}

void ABRCharacter::StopEmote()
{
	if (HasAuthority() && ActiveEmote >= 0)
	{
		ActiveEmote = -1;
		OnRep_ActiveEmote();
		ForceNetUpdate();
	}
}

void ABRCharacter::OnRep_ActiveEmote()
{
	static const TCHAR* Bubbles[] = { TEXT("*waves*"), TEXT("*FLEXES*"), TEXT("*taunts*"), TEXT("*begs*"), TEXT("*cheers*"), TEXT("*dances*"), TEXT("*smokes*") };
	if (ActiveEmote >= 0 && ActiveEmote < NumEmotes)
	{
		EmoteBubble = Bubbles[ActiveEmote];
		EmoteBubbleUntil = GetWorld()->GetTimeSeconds() + 2.5f;
	}
	if (CigaretteMesh)
	{
		CigaretteMesh->SetVisibility(ActiveEmote == 6);
	}
	CurrentSlotAnim = nullptr; // pick the right animation next frame
	OneShotUntil = 0.f;
}

void ABRCharacter::Multicast_Emote_Implementation(uint8 Index)
{
	// (Emotes now loop through ActiveEmote; kept so old Blueprints calling this still compile.)
	PlayOneShot(EmoteAnimFor(Index));
}

// (In the .cpp on purpose: an RPC's _Implementation name written in a header stops Unreal declaring it.)
void ABRCharacter::BotMash() { Server_Mash_Implementation(); }
void ABRCharacter::BotEat() { Server_Eat_Implementation(); }
void ABRCharacter::BotDive() { Server_Dive_Implementation(); }

void ABRCharacter::MakeLuchador(const FString& RingName, float Toughness)
{
	if (!HasAuthority())
	{
		return;
	}
	LuchaName = RingName;
	SetCombatStats(100.f * Toughness, 25.f * (0.8f + 0.25f * Toughness));
	BaseAttackDamage = -1.f; // re-read the new damage next time it holds something
	SetHat(EBRHat::LuchaMask);
	FBRLook Fighter = Look;
	Fighter.ShirtColor = FMath::FRandRange(0.06f, 1.f);
	Fighter.PantsColor = FMath::FRandRange(0.06f, 1.f);
	SetLook(Fighter);
	ForceNetUpdate();
}

void ABRCharacter::Input_DropCash() { Server_DropCash(); }

void ABRCharacter::Server_DropCash_Implementation()
{
	ABRPlayerState* PS = GetBRPlayerState();
	ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>();
	if (!PS || !GM || bIsNPC || GetCondition() != EPVVCondition::Normal || GetCurrentBoat())
	{
		return;
	}
	const int32 Amount = FMath::Max(1, CashDropAmount);
	if (IsWarden())
	{
		if (!GM->SpendBudget(Amount))
		{
			Notify(LOCTEXT("NoBudgetCash", "The budget's empty."));
			return;
		}
	}
	else
	{
		if (PS->Money < Amount)
		{
			Notify(LOCTEXT("NoCash", "You don't have that much cash."));
			return;
		}
		PS->AddMoney(-Amount);
	}
	const FVector At = GetActorLocation() + GetActorForwardVector() * 110.f - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const FTransform T(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), At);
	if (ABRCashPickup* Cash = GetWorld()->SpawnActorDeferred<ABRCashPickup>(ABRCashPickup::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Cash->Amount = Amount;
		Cash->DropperName = PS->GetPlayerName();
		Cash->DropperTeam = GetFaction();
		Cash->FinishSpawning(T);
	}
	Notify(FText::Format(LOCTEXT("CashDropped", "Dropped ${0}. Anyone can pick it up..."), Amount));
}

void ABRCharacter::TickHeadTag()
{
	if (!HeadTag || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	const ABRGameState* GS = GetBRGameState();
	const bool bBounty = GS && GS->BountyTarget && GS->BountyTarget == GetPlayerState() && GetCondition() != EPVVCondition::Restrained;
	FString Text;
	FColor Color = FColor::White;
	if (Now < EmoteBubbleUntil)
	{
		Text = EmoteBubble;
	}
	else if (!LuchaName.IsEmpty())
	{
		Text = LuchaName;
		Color = FColor(255, 150, 40);
	}
	else if (bBounty)
	{
		Text = TEXT("$ WANTED $");
		Color = FColor(80, 255, 80);
	}
	HeadTag->SetHiddenInGame(Text.IsEmpty());
	if (Text.IsEmpty())
	{
		return;
	}
	HeadTag->SetText(FText::FromString(Text));
	HeadTag->SetTextRenderColor(Color);
	if (const APlayerController* Viewer = GetWorld()->GetFirstPlayerController(); Viewer && Viewer->PlayerCameraManager)
	{
		const FVector ToCam = Viewer->PlayerCameraManager->GetCameraLocation() - HeadTag->GetComponentLocation();
		HeadTag->SetWorldRotation(FRotator(0.f, ToCam.Rotation().Yaw, 0.f));
	}
}

#undef LOCTEXT_NAMESPACE
