#include "BRCharacter.h"
#include "BRAudio.h"
#include "BRGadgets.h"
#include "BRAirWater.h"
#include "BRTunnels.h"
#include "Components/AudioComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "PVVDetentionZone.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
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
	DOREPLIFETIME(ABRCharacter, bLiftOverhead);
	DOREPLIFETIME(ABRCharacter, ChugGag);
	DOREPLIFETIME(ABRCharacter, bKnockedOut);
	DOREPLIFETIME(ABRCharacter, bDropKicking);
	DOREPLIFETIME(ABRCharacter, bFakeUniform);
	DOREPLIFETIME(ABRCharacter, bOnRide);
	DOREPLIFETIME_CONDITION(ABRCharacter, SelectedTrap, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ABRCharacter, SprintStamina, COND_OwnerOnly);
	DOREPLIFETIME(ABRCharacter, LiftStartTime);
	DOREPLIFETIME(ABRCharacter, DragBone);
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
	DOREPLIFETIME(ABRCharacter, bPeaceful);
	DOREPLIFETIME(ABRCharacter, bBlocking);
	DOREPLIFETIME(ABRCharacter, DazedUntil);
	DOREPLIFETIME(ABRCharacter, bChoked);
	DOREPLIFETIME_CONDITION(ABRCharacter, EscapeProgress, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ABRCharacter, bGuardWatching, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ABRCharacter, GuardMeter, COND_OwnerOnly);
	DOREPLIFETIME(ABRCharacter, TipsyUntil);
	DOREPLIFETIME_CONDITION(ABRCharacter, bChugging, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ABRCharacter, ChugMine, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ABRCharacter, ChugRival, COND_OwnerOnly);
}

void ABRCharacter::StartBrawling(ABRCharacter* Target, float Seconds, float Aggression)
{
	if (!Target)
	{
		StartFleeing(Seconds);
		return;
	}
	AngryAt = Target;
	BrawlAggression = Aggression;
	AngryUntil = GetWorld()->GetTimeSeconds() + Seconds;
	if (!bWandering)
	{
		StartWandering(GetActorLocation(), 1500.f);
	}
}

void ABRCharacter::ReactToBust(ABRCharacter* Cop, bool bMinorOffense)
{
	if (!HasAuthority() || !bIsNPC)
	{
		return;
	}
	ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>();
	if (!Cop)
	{
		StartFleeing(30.f); // nobody to fight: RUN!
		return;
	}
	// Armed: they pull the gun and start shooting (self-defense = shoot back, no scandal).
	if (bCarriesContraband && ContrabandItem == EBRItem::IllegalGun)
	{
		SetNPCHeldItem(EBRItem::IllegalGun);
		bAggressiveSuspect = true;
		StartBrawling(Cop, 90.f, 1.f);
		if (GM)
		{
			GM->Feed(FString::Printf(TEXT("A suspect pulled a GUN on %s!"), *Cop->GetPlayerDisplayName()));
		}
		return;
	}
	// Most busted people FIGHT (no papers = more likely). Some run, a few give up.
	const float Fight = FMath::Clamp(bMinorOffense ? SuspectFightChance * 0.7f : SuspectFightChance, 0.f, 1.f);
	const float GiveUp = bMinorOffense ? SuspectSurrenderChance * 2.f : SuspectSurrenderChance;
	const float Roll = FMath::FRand();
	if (Roll < Fight)
	{
		const bool bKnife = FMath::FRand() < SuspectKnifeShare;
		if (bKnife)
		{
			SetNPCHeldItem(EBRItem::Knife);
			Multicast_Sfx(3);
		}
		// Comes out swinging, and keeps at it (Gang Beasts: a real scrap, not one punch and run).
		bAggressiveSuspect = true;
		StartBrawling(Cop, 90.f, FMath::FRandRange(0.8f, 1.f));
		if (GM)
		{
			GM->Feed(bKnife ? FString::Printf(TEXT("A suspect pulled a KNIFE on %s!"), *Cop->GetPlayerDisplayName())
				: FString::Printf(TEXT("A suspect wants to FIGHT %s!"), *Cop->GetPlayerDisplayName()));
		}
		return;
	}
	if (Roll < Fight + GiveUp)
	{
		// Gives up: stands there with their hands up (zip-tie them).
		StartWandering(GetActorLocation(), 40.f);
		if (GM)
		{
			GM->Feed(TEXT("A suspect put their hands up: \"OK OK, don't tase me!\""));
		}
		return;
	}
	StartFleeing(30.f); // RUN!
}

void ABRCharacter::StartFleeing(float Seconds)
{
	FleeUntil = GetWorld()->GetTimeSeconds() + Seconds;
	if (!bWandering)
	{
		StartWandering(GetActorLocation(), 1500.f);
	}
}

namespace
{
	/** Every asset of a class in the project (scanned once). */
	const TArray<FAssetData>& BRAllAssets(UClass* Class)
	{
		static TMap<UClass*, TArray<FAssetData>> Cache;
		if (TArray<FAssetData>* Found = Cache.Find(Class))
		{
			return *Found;
		}
		TArray<FAssetData>& List = Cache.Add(Class);
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		Registry.GetAssetsByClass(Class->GetClassPathName(), List, true);
		List.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.LexicalLess(B.AssetName); });
		return List;
	}
}

void ABRCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	AutoFillSlots();
}

void ABRCharacter::AutoFillSlots()
{
	if (!bAutoFillEmptySlots || bSlotsAutoFilled)
	{
		return;
	}
	bSlotsAutoFilled = true;
	const TArray<FAssetData>& Meshes = BRAllAssets(USkeletalMesh::StaticClass());
	auto FillList = [&Meshes](TArray<TObjectPtr<USkeletalMesh>>& Slot, std::initializer_list<const TCHAR*> Prefixes, std::initializer_list<const TCHAR*> Exact)
	{
		if (Slot.Num() > 0)
		{
			return; // you filled it yourself
		}
		for (const FAssetData& A : Meshes)
		{
			const FString Name = A.AssetName.ToString();
			bool bMatch = false;
			for (const TCHAR* P : Prefixes) { bMatch |= Name.StartsWith(P); }
			for (const TCHAR* E : Exact) { bMatch |= Name == E; }
			if (bMatch)
			{
				if (USkeletalMesh* M = Cast<USkeletalMesh>(A.GetAsset()))
				{
					Slot.Add(M);
				}
			}
		}
	};
	auto FindMesh = [&Meshes](const FString& Wanted) -> USkeletalMesh*
	{
		for (const FAssetData& A : Meshes)
		{
			if (A.AssetName.ToString() == Wanted)
			{
				return Cast<USkeletalMesh>(A.GetAsset());
			}
		}
		return nullptr;
	};
	FillList(HairStyles, { TEXT("SK_Hair_") }, {});
	FillList(FacialHairStyles, { TEXT("SK_Face_") }, {});
	FillList(ShirtStyles, { TEXT("SK_Shirt_") }, { TEXT("SK_Outfit_WorkShirt") });
	FillList(PantsStyles, { TEXT("SK_Pants_") }, { TEXT("SK_Outfit_Jeans") });
	FillList(GlassesStyles, { TEXT("SK_Glasses_") }, { TEXT("SK_Outfit_Sunglasses") });
	if (!PonchoMesh)
	{
		PonchoMesh = FindMesh(TEXT("SK_Outfit_Poncho"));
	}
	const TPair<EBRHat, const TCHAR*> Hats[] = { { EBRHat::CowboyHat, TEXT("SK_Hat_CowboyHat") }, { EBRHat::Sombrero, TEXT("SK_Hat_Sombrero") },
		{ EBRHat::PatrolCap, TEXT("SK_Hat_PatrolCap") }, { EBRHat::HardHat, TEXT("SK_Hat_HardHat") }, { EBRHat::CannonHelmet, TEXT("SK_Hat_CannonHelmet") },
		{ EBRHat::LuchaMask, TEXT("SK_Hat_LuchaMask") } };
	for (const TPair<EBRHat, const TCHAR*>& H : Hats)
	{
		if (!HatMeshes.FindRef(H.Key))
		{
			if (USkeletalMesh* M = FindMesh(H.Value))
			{
				HatMeshes.Add(H.Key, M);
			}
		}
	}
	// Animations: by name ("Kick", or "..._Kick"), on the same skeleton as the body.
	const USkeleton* BodySkeleton = GetMesh() && GetMesh()->GetSkeletalMeshAsset() ? GetMesh()->GetSkeletalMeshAsset()->GetSkeleton() : nullptr;
	const TArray<FAssetData>& Anims = BRAllAssets(UAnimSequence::StaticClass());
	auto FillAnim = [&Anims, BodySkeleton](TObjectPtr<UAnimSequenceBase>& Slot, const TCHAR* Wanted)
	{
		if (Slot)
		{
			return;
		}
		const FString Suffix = FString(TEXT("_")) + Wanted;
		for (const FAssetData& A : Anims)
		{
			const FString Name = A.AssetName.ToString();
			if (Name != Wanted && !Name.EndsWith(Suffix))
			{
				continue;
			}
			UAnimSequence* Seq = Cast<UAnimSequence>(A.GetAsset());
			if (Seq && (!BodySkeleton || Seq->GetSkeleton() == BodySkeleton))
			{
				Slot = Seq;
				return;
			}
		}
	};
	FillAnim(IdleAnim, TEXT("Idle")); FillAnim(WalkAnim, TEXT("Walk")); FillAnim(RunAnim, TEXT("Run"));
	FillAnim(CrouchIdleAnim, TEXT("CrouchIdle")); FillAnim(CrouchWalkAnim, TEXT("CrouchWalk")); FillAnim(FallAnim, TEXT("Fall"));
	FillAnim(SwimAnim, TEXT("Swim")); FillAnim(SeatedAnim, TEXT("Seated")); FillAnim(DriveAnim, TEXT("Drive"));
	FillAnim(RestrainedAnim, TEXT("Restrained")); FillAnim(StruggleAnim, TEXT("Struggle")); FillAnim(CarriedAnim, TEXT("Carried"));
	FillAnim(WorkAnim, TEXT("Work")); FillAnim(AimAnim, TEXT("Aim")); FillAnim(AimWalkAnim, TEXT("AimWalk"));
	FillAnim(PunchAnim, TEXT("Punch")); FillAnim(FireAnim, TEXT("Fire")); FillAnim(ReloadAnim, TEXT("Reload"));
	FillAnim(EatAnim, TEXT("Eat")); FillAnim(TrumpetAnim, TEXT("Trumpet")); FillAnim(DanceAnim, TEXT("Dance"));
	FillAnim(EmoteWaveAnim, TEXT("Emote_Wave")); FillAnim(EmoteFlexAnim, TEXT("Emote_Flex")); FillAnim(EmoteTauntAnim, TEXT("Emote_Taunt"));
	FillAnim(EmoteBegAnim, TEXT("Emote_Beg")); FillAnim(EmoteCheerAnim, TEXT("Emote_Cheer")); FillAnim(EmoteSmokeAnim, TEXT("Emote_Smoke"));
	FillAnim(KickAnim, TEXT("Kick")); FillAnim(HeadbuttAnim, TEXT("Headbutt")); FillAnim(BlockAnim, TEXT("Block")); FillAnim(LiftAnim, TEXT("Lift_Overhead")); FillAnim(GetUpAnim, TEXT("GetUp")); FillAnim(ClimbAnim, TEXT("Climb"));
}

void ABRCharacter::InitAsNPC(float SkinToneMin, float SkinToneMax)
{
	AutoFillSlots(); // NPCs pick random clothes from these lists right away
	bIsNPC = true;
	KnockdownDuration = NPCKnockdownSeconds; // stay down long enough to be zip-tied
	DefaultFaction = BR::Runners;
	// Every NPC gets a random look so the crowd isn't all clones (and players can blend in).
	// Every slider gets used: all sorts of faces, natural hair colours, any of the clothes / beards / glasses.
	FBRLook Random;
	// Locals use the Runner skin range (Project Settings) unless the game mode narrows it further.
	const UBRSettings* SkinRange = GetDefault<UBRSettings>();
	const float NPCMin = FMath::Max(FMath::Min(SkinToneMin, SkinToneMax), FMath::Min(SkinRange->RunnerSkinMin, SkinRange->RunnerSkinMax));
	const float NPCMax = FMath::Min(FMath::Max(SkinToneMin, SkinToneMax), FMath::Max(SkinRange->RunnerSkinMin, SkinRange->RunnerSkinMax));
	BR::RandomizeLook(Random, NPCMin, FMath::Max(NPCMin, NPCMax), HairStyles.Num(), FacialHairStyles.Num(), ShirtStyles.Num(), PantsStyles.Num(), GlassesStyles.Num());
	Random.bHat = FMath::RandRange(0, 2) != 0;
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
	FlightStartedAt = GetWorld()->GetTimeSeconds();
	FlightVelocity = Velocity;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->SetMovementMode(MOVE_Falling);
	Move->BrakingDecelerationFalling = 0.f; // nothing slows the flight down
	Move->AirControl = 0.f;
	LaunchCharacter(Velocity, true, true);
	Multicast_FlightFlop(true);
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
			ABRGameState::Highlight(this, this, 45.f + Drop * 0.02f, FString::Printf(TEXT("%s fell %d m and went SPLAT"), *GetPlayerDisplayName(), FMath::RoundToInt(Drop / 100.f)));
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
		// BODY SPLASH: landed on someone who's already down = they stay down longer.
		for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
		{
			ABRCharacter* Down = *It;
			if (Down != this && Down->GetCondition() == EPVVCondition::KnockedDown && FVector::Dist(Down->GetBodyLocation(), GetActorLocation()) < 190.f)
			{
				Down->ExtendKnockdown(4.f);
				Multicast_Sfx(1);
				ABRGameState::FX(this, EBRFX::PointsText, GetActorLocation() + FVector(0.f, 0.f, 80.f), TEXT("BODY SPLASH!"));
				if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
				{
					GM->Feed(FString::Printf(TEXT("%s BODY SPLASHED %s!"), *GetPlayerDisplayName(), *Down->GetPlayerDisplayName()));
				}
				Recover(0.8f);
				return;
			}
		}
		// Missed: belly flop and scramble back up (no knockdown - you only go down if you dive head-first into something).
		ApplySlow(0.6f, 0.4f);
	}
	if (!HasAuthority() || !bFlying)
	{
		return;
	}
	bFlying = false;
	EndFlightTweaks();
	ABRGameState::FX(this, EBRFX::Dust, GetActorLocation() - FVector(0.f, 0.f, 80.f));
	ABRGameState::Sfx(this, EBRSfx::Landing, GetActorLocation());
	ClearRestraint(); // the zip-ties snap on impact
	if (WornHat != EBRHat::CannonHelmet)
	{
		ApplyKnockdown(nullptr, -GetActorForwardVector() * 150.f + FVector(0.f, 0.f, 150.f)); // tumble
	}
}

void ABRCharacter::EndFlightTweaks()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	const UCharacterMovementComponent* Defaults = GetClass()->GetDefaultObject<ABRCharacter>()->GetCharacterMovement();
	if (Move && Defaults)
	{
		Move->BrakingDecelerationFalling = Defaults->BrakingDecelerationFalling;
		Move->AirControl = Defaults->AirControl;
	}
	for (AActor* Ignored : FlightIgnored)
	{
		if (IsValid(Ignored))
		{
			MoveIgnoreActorRemove(Ignored);
		}
	}
	FlightIgnored.Reset();
	Multicast_FlightFlop(false);
}

void ABRCharacter::IgnoreDuringFlight(AActor* Other)
{
	if (Other && Other != this)
	{
		MoveIgnoreActorAdd(Other);
		FlightIgnored.AddUnique(Other);
	}
}

void ABRCharacter::TickFlight()
{
	if (!HasAuthority() || !bFlying)
	{
		return;
	}
	const float Age = GetWorld()->GetTimeSeconds() - FlightStartedAt;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	// Something stopped the flight early (snagged on the catapult, movement switched off): keep them flying.
	if (Age < 0.6f && Move && (Move->MovementMode != MOVE_Falling || GetVelocity().Size() < FlightVelocity.Size() * 0.3f))
	{
		Move->SetMovementMode(MOVE_Falling);
		Move->Velocity = FlightVelocity - FVector(0.f, 0.f, 980.f * Age);
	}
	// Never landed (stuck somewhere): end it.
	if (Age > 12.f)
	{
		bFlying = false;
		EndFlightTweaks();
	}
}

void ABRCharacter::Multicast_FlightFlop_Implementation(bool bFlop)
{
	// Flying through the air: go floppy and flail (muscles back on when they land / get up).
	if (!bWobbleOn || !GetMesh())
	{
		return;
	}
	if (bFlop)
	{
		ApplyMuscles(ArmStrength * 0.15f);
		FPhysicalAnimationData Limp;
		Limp.bIsLocalSimulation = false;
		Limp.OrientationStrength = SpineStrength * WobbleStrength * 0.15f;
		Limp.AngularVelocityStrength = Limp.OrientationStrength * 0.1f;
		PhysicalAnimation->ApplyPhysicalAnimationSettingsBelow(TEXT("spine_01"), Limp, true);
		GetMesh()->AddImpulseToAllBodiesBelow(FVector(FMath::FRandRange(-200.f, 200.f), FMath::FRandRange(-200.f, 200.f), 300.f), TEXT("spine_02"), true, true);
	}
	else
	{
		ApplyMuscles(CurrentArmMuscle());
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

// Clothes/hats copy the body's bones (leader pose), so they must never build physics bodies of their own:
// the hat .glb imports come with a physics asset, and a follower's bodies read bones it doesn't have
// ("Array index out of bounds: 2 into an array of size 0" engine crash a few seconds into play).
static void BRMakeFollowerSafe(USkeletalMeshComponent* Part)
{
	if (!Part)
	{
		return;
	}
	Part->SetSimulatePhysics(false);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetGenerateOverlapEvents(false);
	Part->bAlwaysCreatePhysicsState = false;
	Part->SetEnableGravity(false);
	Part->SetCanEverAffectNavigation(false);
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
	// Shirt / pants: 0 = the Blueprint's own; glasses: 0 = none.
	auto PickOrDefault = [this](USkeletalMeshComponent* Part, const TArray<TObjectPtr<USkeletalMesh>>& Styles, int32 Index, USkeletalMesh* Default)
	{
		if (!Part || Styles.Num() == 0)
		{
			return;
		}
		USkeletalMesh* Wanted = Index > 0 ? Styles[(Index - 1) % Styles.Num()].Get() : Default;
		if (Part->GetSkeletalMeshAsset() != Wanted)
		{
			BRMakeFollowerSafe(Part);
			Part->SetSkeletalMeshAsset(Wanted);
			Part->SetLeaderPoseComponent(GetMesh(), true);
		}
	};
	// Border Patrol always wear their uniform (no custom shirts/pants).
	PickOrDefault(OutfitTop, ShirtStyles, IsWarden() ? 0 : L.ShirtStyle, DefaultTopMesh.Get());
	PickOrDefault(OutfitBottom, PantsStyles, IsWarden() ? 0 : L.PantsStyle, DefaultBottomMesh.Get());
	PickOrDefault(OutfitFace, GlassesStyles, L.Glasses, nullptr);
	PickStyle(FacialHair, FacialHairStyles, L.FacialHair);
	// Players: the slider only covers their team's allowed skin range (Project Settings -> Border Run -> Skin Range).
	// NPCs already rolled a tone inside the village range.
	ApplySkin(BR::SkinColor(bIsNPC ? L.SkinTone : UBRSettings::SkinToneFor(GetFaction(), L.SkinTone)));
	TintOutfit(OutfitTop, ShirtColorParameters, BR::IsOriginalCloth(L.ShirtColor) || IsWarden(), BR::ClothColor(L.ShirtColor), ClothMaterialSlot);
	TintOutfit(OutfitBottom, PantsColorParameters, BR::IsOriginalCloth(L.PantsColor) || IsWarden(), BR::ClothColor(L.PantsColor), ClothMaterialSlot);
	TintOutfit(Hair, HairColorParameters, BR::IsOriginalCloth(L.HairColor), BR::HairColor(L.HairColor));
	TintOutfit(FacialHair, HairColorParameters, BR::IsOriginalCloth(L.HairColor), BR::HairColor(L.HairColor));
	// Eyebrows (the "M_Brow" slot on the body) match the hair colour.
	if (USkeletalMeshComponent* Body = GetMesh())
	{
		const TArray<FName> Slots = Body->GetMaterialSlotNames();
		for (int32 i = 0; i < Slots.Num(); ++i)
		{
			if (Slots[i].ToString().Contains(TEXT("Brow")))
			{
				TintOutfit(Body, HairColorParameters, BR::IsOriginalCloth(L.HairColor), BR::HairColor(L.HairColor), i);
			}
		}
	}
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
		BRMakeFollowerSafe(Part);
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
		for (const FBRFaceSlider& S : BR::FaceSliders())
		{
			L.*S.Field = 0.5f;
		}
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
			Body->SetMorphTarget(Small, SmallWeight, false); // keep the entry (removing/adding entries every frame is crash-prone with followers)
		}
		if (Asset->FindMorphTarget(Big))
		{
			Body->SetMorphTarget(Big, BigWeight, false);
		}
	};
	Pair(TEXT("Head_Small"), TEXT("Head_Big"), L.HeadSize);
	Pair(TEXT("Nose_Small"), TEXT("Nose_Big"), L.NoseSize);
	Pair(TEXT("Lips_Small"), TEXT("Lips_Big"), L.LipSize);
	Pair(TEXT("Ears_Small"), TEXT("Ears_Big"), L.EarSize);
	for (const FBRFaceSlider& S : BR::FaceSliders())
	{
		Pair(S.SmallKey, S.BigKey, L.*S.Field);
	}
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
	return bAiming || bBlocking || (IsDriving() && (DriveAnim || SeatedAnim));
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
	// Somebody's going after our coyote? The clients jump in and fight for them.
	{
		const float Now = GetWorld()->GetTimeSeconds();
		ABRCharacter* Threat = nullptr;
		if (ABRCharacter* P = L->ProvokedBy.Get(); P && P != this && Now - L->ProvokeTime < 6.f && P->Leader.Get() != L)
		{
			Threat = P;
		}
		if (!Threat && L->GrabbedBy && L->GrabbedBy.Get() != this && L->GrabbedBy->Leader.Get() != L)
		{
			Threat = L->GrabbedBy.Get();
		}
		if (!Threat)
		{
			Threat = Cast<ABRCharacter>(L->GetCarrier()); // being hauled off: get them!
		}
		if (Threat && Threat->GetCondition() == EPVVCondition::Normal && !Threat->GetCurrentBoat()
			&& FVector::Dist2D(Threat->GetActorLocation(), GetActorLocation()) < 1800.f)
		{
			AngryAt = Threat;
			AngryUntil = Now + 3.f;
			BrawlAggression = 0.8f;
			const FVector ToThreat = (Threat->GetActorLocation() - GetActorLocation()) * FVector(1.f, 1.f, 0.f);
			GetCharacterMovement()->MaxWalkSpeed = 620.f;
			if (ToThreat.Size() > 130.f)
			{
				AddMovementInput(ToThreat.GetSafeNormal(), 1.f);
			}
			BrawlerAct(Threat, 0.8f, DeltaSeconds);
			return;
		}
		if (AngryAt.IsValid() && Now >= AngryUntil)
		{
			AngryAt = nullptr; // threat's gone: back to following
		}
	}
	const FVector ToLeader = (L->GetActorLocation() - GetActorLocation()) * FVector(1.f, 1.f, 0.f);
	const float Dist = ToLeader.Size();
	if (LeaderRide && Dist < 700.f)
	{
		LeaderRide->TryBoard(this); // jump in the truck / boat with them
		return;
	}
	if (bClimbing)
	{
		return; // on the Wall: TickClimb takes them up and over
	}
	if (Dist > 9000.f && !LeaderRide)
	{
		// Leader flew off in a balloon / went a very long way: the client catches up.
		TeleportTo(L->GetActorLocation() - L->GetActorForwardVector() * 200.f + FVector(0.f, 0.f, 30.f), L->GetActorRotation());
		return;
	}
	if (Dist > 250.f)
	{
		// The Wall is between us and our coyote: climb it, the same way players do.
		if (NPCClimbBorderWall(ToLeader.GetSafeNormal()))
		{
			return;
		}
		const bool bWhistled = GetWorld()->GetTimeSeconds() < WhistleRunUntil;
		GetCharacterMovement()->MaxWalkSpeed = bWhistled ? 760.f : (Dist > 900.f ? 650.f : 420.f);
		AddMovementInput(ToLeader.GetSafeNormal(), 1.f);
	}
}

void ABRCharacter::TickWander(float DeltaSeconds)
{
	if (bOfficer)
	{
		if (!Controller && GetCondition() == EPVVCondition::Normal && !GetCarrier() && !GetCurrentBoat())
		{
			TickOfficer(DeltaSeconds);
		}
		return;
	}
	if (!bWandering || Controller || GetCurrentBoat() || GetCarrier() || GetCondition() != EPVVCondition::Normal)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	// Fighting mad: go after them, then run for it.
	if (ABRCharacter* Foe = AngryAt.Get())
	{
		if (Now < AngryUntil && Foe->GetCondition() == EPVVCondition::Normal)
		{
			const FVector ToFoe = (Foe->GetActorLocation() - GetActorLocation()) * FVector(1.f, 1.f, 0.f);
			GetCharacterMovement()->MaxWalkSpeed = FleeSpeed;
			// Armed: shoot from range instead of walking up.
			if (HeldItem == EBRItem::IllegalGun && ToFoe.Size() > 260.f && ToFoe.Size() < 1200.f)
			{
				SetActorRotation(FRotator(0.f, ToFoe.Rotation().Yaw, 0.f));
				if (Now >= NextNPCShot)
				{
					NextNPCShot = Now + FMath::FRandRange(2.f, 3.5f);
					const FVector Muzzle = GetActorLocation() + FVector(0.f, 0.f, 50.f);
					FHitResult Hit;
					FCollisionQueryParams Params(SCENE_QUERY_STAT(BRNPCShot), false, this);
					const FVector Aim = Foe->GetActorLocation() + FVector(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-60.f, 60.f), 20.f);
					const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, Muzzle, Aim, ECC_Visibility, Params) && Hit.GetActor() != Foe;
					const bool bHitFoe = !bBlocked && FMath::FRand() < 0.45f; // they're terrible shots
					Multicast_Shot(Muzzle, bBlocked ? FVector(Hit.ImpactPoint) : Aim, true, bHitFoe);
					if (bHitFoe)
					{
						Foe->ApplyKnockdown(this, ToFoe.GetSafeNormal() * 300.f + FVector(0.f, 0.f, 150.f));
						Foe->LastTosser = this;
						Foe->LastTossTime = Now;
					}
				}
				return;
			}
			if (MugTarget.Get() == Foe && ToFoe.Size() < 170.f)
			{
				MugTarget = nullptr;
				MugPlayer(Foe); // shove, grab, go
				return;
			}
			if (ToFoe.Size() > 130.f)
			{
				AddMovementInput(ToFoe.GetSafeNormal(), 1.f);
			}
			BrawlerAct(Foe, BrawlAggression, DeltaSeconds);
			return;
		}
		// Knocked them flat? Gang Beasts: grab them, haul them up over your head and THROW them - then leg it.
		if (Now < AngryUntil && BrawlAggression >= 0.75f && Foe->GetCondition() == EPVVCondition::KnockedDown)
		{
			BrawlerFinish(Foe);
			return;
		}
		if (HandGrabTarget.Get() == Foe)
		{
			ReleaseHandGrab();
		}
		AngryAt = nullptr;
		FleeUntil = Now + 20.f; // done fighting: leg it
	}
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
		// Busted near the Wall (and not the fighting kind)? Make for the Wall and climb it, like a player would.
		if (!bAggressiveSuspect)
		{
			for (TActorIterator<ABRWall> WallIt(GetWorld()); WallIt; ++WallIt)
			{
				const FVector WallLocal = WallIt->GetActorTransform().InverseTransformPositionNoScale(GetActorLocation());
				if (WallLocal.X < 0.f && WallLocal.X > -3500.f)
				{
					const FVector ToWall = WallIt->GetActorForwardVector().GetSafeNormal2D();
					Away = ToWall * 1.5f + Away.GetSafeNormal() * 0.5f;
					if (NPCClimbBorderWall(ToWall))
					{
						return;
					}
				}
				else if (WallLocal.X >= 0.f && WallLocal.X < 1500.f)
				{
					WanderHome = GetActorLocation(); // made it over: hang around this side now
				}
				break;
			}
		}
		const FVector Wiggle = FVector::CrossProduct(Away.GetSafeNormal(), FVector::UpVector) * FMath::Sin(Now * 2.3f) * 0.7f;
		if (Closest < 260.f && FMath::FRand() < 0.04f)
		{
			Server_Dive_Implementation(); // dive away from the grab
		}
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

	DefaultTopMesh = OutfitTop ? OutfitTop->GetSkeletalMeshAsset() : nullptr;
	DefaultBottomMesh = OutfitBottom ? OutfitBottom->GetSkeletalMeshAsset() : nullptr;
	if (StatusLabel)
	{
		StatusLabel->SetVisibility(false); // shown only to teammates (UpdateLocalVisibility)
	}
	for (USkeletalMeshComponent* Outfit : { OutfitTop.Get(), OutfitBottom.Get(), OutfitHead.Get(), OutfitFace.Get(), Hair.Get(), FacialHair.Get(), DisguiseOutfit.Get() })
	{
		// Clothes copy every bone of the body (incl. physics/ragdoll) and sit exactly on it.
		if (!Outfit)
		{
			continue;
		}
		BRMakeFollowerSafe(Outfit);
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
	if (PhysicalAnimation && GetMesh())
	{
		PhysicalAnimation->AddTickPrerequisiteComponent(GetMesh()); // muscles read the pose only after it's been made this frame
		PhysicalAnimation->AddTickPrerequisiteActor(this); // ...and after our Tick's safety check (MusclesSafe) has run
	}
	if (bHeavyMovement)
	{
		// Gang Beasts: heavy and clumsy - slow to get going, slides to a stop, wide lazy turns.
		UCharacterMovementComponent* HeavyMove = GetCharacterMovement();
		HeavyMove->MaxAcceleration = HeavyAcceleration;
		HeavyMove->BrakingDecelerationWalking = HeavyBraking;
		HeavyMove->GroundFriction = HeavyGroundFriction;
		HeavyMove->RotationRate = FRotator(0.f, HeavyTurnRate, 0.f);
	}
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
	if (bBlocking)
	{
		Multiplier *= 0.45f; // shuffle with your guard up
	}
	if (IsDazed())
	{
		Multiplier *= 0.35f; // staggering
	}
	if (bWinded && IsSprinting())
	{
		Move->MaxWalkSpeed = FMath::Min(Move->MaxWalkSpeed, WalkSpeed); // out of breath: no sprinting
	}
	if (bInBox)
	{
		Multiplier *= 0.5f; // shuffling along in a box
	}
	if (IsDraggingBody())
	{
		Multiplier *= DragSpeedScale; // hauling a body / somebody over your head
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
	TrapCycleAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	TrumpetAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	NextItemAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	PrevItemAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	DropAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	GrabAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	DiveAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	BlockAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
	OrderOutAction = PVVInput::MakeAction(this, EInputActionValueType::Boolean);
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
	PVVInput::Map(IMC, TrapCycleAction, EKeys::N);
	PVVInput::Map(IMC, TrapAction, EKeys::Gamepad_LeftShoulder);
	PVVInput::Map(IMC, NextItemAction, EKeys::MouseScrollDown);
	PVVInput::Map(IMC, NextItemAction, EKeys::Gamepad_RightShoulder);
	PVVInput::Map(IMC, PrevItemAction, EKeys::MouseScrollUp);
	PVVInput::Map(IMC, DropAction, EKeys::G);
	PVVInput::Map(IMC, GrabAction, EKeys::Z);
	PVVInput::Map(IMC, GrabAction, EKeys::Gamepad_FaceButton_Right);
	PVVInput::Map(IMC, BlockAction, EKeys::LeftAlt);
	PVVInput::Map(IMC, OrderOutAction, EKeys::O);
	PVVInput::Map(IMC, BlockAction, EKeys::Gamepad_LeftTrigger);
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
	if (OrderOutAction) Input->BindAction(OrderOutAction, ETriggerEvent::Started, this, &ABRCharacter::Input_OrderOut);
	if (BlockAction)
	{
		Input->BindAction(BlockAction, ETriggerEvent::Started, this, &ABRCharacter::Input_BlockStart);
		Input->BindAction(BlockAction, ETriggerEvent::Completed, this, &ABRCharacter::Input_BlockStop);
	}
	if (DroneAction) Input->BindAction(DroneAction, ETriggerEvent::Started, this, &ABRCharacter::Input_Drone);
	if (WeaponModeAction) Input->BindAction(WeaponModeAction, ETriggerEvent::Started, this, &ABRCharacter::Input_WeaponMode);
	if (ScanAction) Input->BindAction(ScanAction, ETriggerEvent::Started, this, &ABRCharacter::Input_Scan);
	if (TrapAction) Input->BindAction(TrapAction, ETriggerEvent::Started, this, &ABRCharacter::Input_Trap);
	if (TrapCycleAction) Input->BindAction(TrapCycleAction, ETriggerEvent::Started, this, &ABRCharacter::Input_TrapCycle);
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
	case EBRItem::SecurityCamera: PlaceSecurityCamera(); break;
	case EBRItem::SmokeBomb: ThrowSmokeBomb(); break;
	case EBRItem::TNT: PlantTNT(); break;
	case EBRItem::FuseKit: UseFuseKit(); break;
	case EBRItem::WallReinforcement: Notify(LOCTEXT("UseReinforce", "Walk up to a section of the Wall and hold E to reinforce it (it'll survive one TNT blast).")); break;
	case EBRItem::GrappleHook: UseGrapple(); break;
	case EBRItem::FakeUniform: ToggleFakeUniform(); break;
	case EBRItem::Lockpick: Notify(LOCTEXT("UseLockpick", "Lockpick: hold E on the Border Patrol door in the Wall, or on the holding pen door (much faster jailbreak).")); break;
	case EBRItem::FakePassport: Notify(LOCTEXT("UsePassport", "Fake passport: the papers scan says you're legal - unless you're carrying contraband. A search finds it.")); break;
	case EBRItem::Lasso: Notify(LOCTEXT("UseLasso", "Aim at someone with your view and press F to throw the lasso.")); break;
	case EBRItem::Ladder: Notify(LOCTEXT("UseLadder", "Walk up to the Wall and press E to lean your ladder on it.")); break;
	case EBRItem::WireCutters: Notify(LOCTEXT("UseCutters", "Walk up to the razor wire and hold E to cut it.")); break;
	case EBRItem::GasCan: Notify(LOCTEXT("UseGas", "Walk up to the truck and press E to fill it up.")); break;
	case EBRItem::Shovel: Notify(LOCTEXT("UseShovel", "Go to a TUNNEL sign near the line and hold E to break ground, then dig at the end of the tunnel. Shovels wear out!")); break;
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
		const bool bTeammate = !Other->IsNPC() && Other->GetFaction() == GetFaction();
		if (Other != this && !bTeammate && !Other->bPeaceful && !Other->bNoFightBack && Other->GetCondition() == EPVVCondition::Normal && !Other->GetCurrentBoat()
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
	Multicast_Trumpet(TrumpetSoloSeconds);
	Notify(Dancers > 0
		? FText::Format(LOCTEXT("TrumpetHit", "AY AY AY! {0} Border Patrol dancing - RUN!"), Dancers)
		: LOCTEXT("TrumpetMiss", "Beautiful solo. Nobody was close enough to dance."));
}

void ABRCharacter::Multicast_TrumpetPose_Implementation()
{
	PlayOneShot(TrumpetAnim);
}

void ABRCharacter::Multicast_Trumpet_Implementation(float HoldSeconds)
{
	PlayOneShot(TrumpetAnim);
	if (TrumpetSound && GetNetMode() != NM_DedicatedServer)
	{
		// Keep the tune going while you keep playing; it fades out once you stop (see TickTrumpetAudio).
		TrumpetPlayUntil = GetWorld()->GetTimeSeconds() + FMath::Max(0.3f, HoldSeconds);
		// One tune at a time per trumpet: never start it again on top of itself.
		if (TrumpetAudio && TrumpetAudio->IsPlaying())
		{
			TrumpetAudio->AdjustVolume(0.1f, 1.f); // cancel a fade-out that had started
			return;
		}
		if (!TrumpetAudio)
		{
			TrumpetAudio = UGameplayStatics::SpawnSoundAttached(TrumpetSound, GetRootComponent(), NAME_None, FVector::ZeroVector,
				EAttachLocation::KeepRelativeOffset, true, 1.f, 1.f, 0.f, BRAudio::Falloff(BRAudio::Medium), nullptr, false);
		}
		else
		{
			TrumpetAudio->SetSound(TrumpetSound);
			TrumpetAudio->SetVolumeMultiplier(1.f);
			TrumpetAudio->Play();
		}
	}
}

void ABRCharacter::TickTrumpetAudio()
{
	if (!TrumpetAudio || !TrumpetAudio->IsPlaying() || TrumpetPlayUntil < 0.f)
	{
		return;
	}
	// Stopped playing, got knocked down, got in a car or hid: the trumpet goes quiet.
	const bool bInterrupted = GetCondition() != EPVVCondition::Normal || GetCurrentBoat() || GetCarrier() || bInCannon || bHiddenInSpot;
	if (bInterrupted || GetWorld()->GetTimeSeconds() > TrumpetPlayUntil)
	{
		TrumpetPlayUntil = -1.f;
		TrumpetAudio->FadeOut(bInterrupted ? 0.15f : 0.6f, 0.f);
	}
}
void ABRCharacter::Input_Drone()
{
	if (IsRunner())
	{
		Server_CoyoteWhistle(); // Runners: whistle for your clients
		return;
	}
	Server_LaunchDrone();
}

void ABRCharacter::Server_CoyoteWhistle_Implementation()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (!IsRunner() || bIsNPC || Now - LastWhistleTime < 6.f || GetCondition() != EPVVCondition::Normal)
	{
		return;
	}
	LastWhistleTime = Now;
	ABRGameState::Sfx(this, EBRSfx::Whistle, GetActorLocation());
	int32 Called = 0;
	for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
	{
		if (It->IsNPC() && It->Leader.Get() == this)
		{
			It->WhistleRunUntil = Now + 8.f;
			++Called;
		}
	}
	// Loud: Border Patrol hears where it came from.
	if (ABRGameState* GS = GetBRGameState())
	{
		GS->Multicast_NoisePing(GetActorLocation());
	}
	Notify(Called > 0 ? FText::Format(LOCTEXT("Whistled", "*FWEEEET* {0} client(s) coming running! (Border Patrol heard that.)"), Called)
		: LOCTEXT("WhistleNobody", "*FWEEEET* ...nobody's following you. (Border Patrol heard that.)"));
}
void ABRCharacter::Input_WeaponMode() { Server_ToggleWeaponMode(); }
void ABRCharacter::Input_Trap() { Server_PlaceTrap(); }
void ABRCharacter::Input_TrapCycle() { Server_CycleTrap(); }

void ABRCharacter::Server_CycleTrap_Implementation()
{
	if (!IsWarden())
	{
		return;
	}
	SelectedTrap = static_cast<uint8>((SelectedTrap + 1) % 4);
	static const TCHAR* Names[] = { TEXT("BEAR TRAP (roots them)"), TEXT("GLUE (slows them)"), TEXT("ALARM WIRE (tags them)"), TEXT("SPIKE STRIP (pops tyres - put it across a road)") };
	Notify(FText::FromString(FString::Printf(TEXT("Trap (B): %s   - N to change"), Names[SelectedTrap])));
}

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
			bRightDown = true;
			RightDownAt = GetWorld()->GetTimeSeconds();
			if (bSpaceHeld)
			{
				Server_ClimbStep(false);
			}
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
	// Holding someone over your head: pull the camera back and up so they're not in your face.
	const bool bLiftCam = bLiftOverhead && !bZoom;
	CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, (bZoom ? AimArmLength : DefaultArmLength) + (bLiftCam ? 170.f : 0.f), DeltaSeconds, Speed * 0.5f);
	CameraBoom->SocketOffset = FMath::VInterpTo(CameraBoom->SocketOffset, (bZoom ? FVector(0.f, 55.f, 65.f) : DefaultSocketOffset) + (bLiftCam ? FVector(0.f, 0.f, 120.f) : FVector::ZeroVector), DeltaSeconds, Speed * 0.5f);
	KOPunch = FMath::Max(0.f, KOPunch - DeltaSeconds * 2.5f);
	FollowCamera->SetFieldOfView(FMath::FInterpTo(FollowCamera->FieldOfView, (bZoom ? AimFOV : DefaultFOV) - 14.f * KOPunch, DeltaSeconds, Speed));
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
		BRAudio::PlayAt(this, ReloadSound, GetActorLocation(), BRAudio::Quiet);
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
	if (bHit && (bLethal || bRunnerGun) && !Victim)
	{
		BRAir::TryPop(Hit.GetActor(), this); // shot a balloon or a raft: POP
	}

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
	if (!bWobbleOn || bPunchAnimWithPhysics)
	{
		PlayOneShot(PunchAnim); // no physics body: the animation is the punch
	}
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
				// Go for the head (that's what knocks people out).
				const FVector Chest = Other->GetMesh() && Other->GetMesh()->GetBoneIndex(TEXT("head")) != INDEX_NONE
					? Other->GetMesh()->GetBoneLocation(TEXT("head")) : Other->GetActorLocation() + FVector(0.f, 0.f, 70.f);
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
			PunchArmRestoreAt = GetWorld()->GetTimeSeconds() + PunchWindup + PunchExtendSeconds + 0.2f;
			// A straight jab: after the wind-up the fist is steered out to the target and HELD there for a moment
			// (and keeps reaching if you keep holding the button) - no instant snap back.
			{
				const int32 Limb = bPunchLeft ? 0 : 1;
				const FVector Target = Shoulder + Dir * PunchReach; // full arm's length (and the shoulders turn into it)
				SteerTargetLocal[Limb] = GetActorTransform().InverseTransformPositionNoScale(Target);
				SteerStart[Limb] = GetWorld()->GetTimeSeconds() + PunchWindup * 0.6f;
				SteerUntil[Limb] = SteerStart[Limb] + PunchExtendSeconds;
			}
			// Wind-up: yank the fist back first (and twist the shoulders away)...
			GetMesh()->AddImpulse(-Dir * PunchImpulse * 0.3f + FVector(0.f, 0.f, PunchImpulse * 0.05f), Hand, true);
			GetMesh()->AddImpulseToAllBodiesBelow(-Dir * 90.f, TEXT("spine_03"), true, false);
			// ...then FLING it (with your running speed behind it).
			TWeakObjectPtr<ABRCharacter> WeakThis(this);
			auto Fling = [WeakThis, Dir, Hand, Fore, Upper]()
			{
				ABRCharacter* Self = WeakThis.Get();
				USkeletalMeshComponent* Body = Self ? Self->GetMesh() : nullptr;
				if (!Body || !Self->bWobbleOn || Body->GetBoneIndex(Hand) == INDEX_NONE)
				{
					return;
				}
				const float Run = 1.f + FMath::Clamp(FVector::DotProduct(Self->GetVelocity(), Dir) / 900.f, 0.f, 0.5f);
				Body->AddImpulse(Dir * Self->PunchImpulse * Run, Hand, true);
				Body->AddImpulse(Dir * Self->PunchImpulse * 0.7f * Run, Fore, true);
				Body->AddImpulse(Dir * Self->PunchImpulse * 0.35f, Upper, true);
				Body->AddImpulseToAllBodiesBelow(Dir * 420.f, TEXT("spine_03"), true, false); // shoulders and chest into it
				if (Self->HasAuthority() && Self->GetCharacterMovement() && Self->GetCharacterMovement()->IsMovingOnGround())
				{
					Self->LaunchCharacter(Dir.GetSafeNormal2D() * Self->PunchStep, false, false); // a little step into the punch
				}
			};
			if (PunchWindup > 0.01f)
			{
				GetWorldTimerManager().SetTimer(PunchWindupTimer, FTimerDelegate::CreateLambda(Fling), PunchWindup, false);
			}
			else
			{
				Fling();
			}
		}
	}
	if (PunchSwingSound && GetNetMode() != NM_DedicatedServer)
	{
		BRAudio::PlayAt(this, PunchSwingSound, GetActorLocation(), BRAudio::Quiet);
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
	// (Holding someone with one hand: the other fist is thrown at them like any punch - it only hurts if it lands.)
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
	else if (bClimbing && ClimbAnim) Wanted = ClimbAnim.Get();
	else if (GetCurrentBoat()) Wanted = (IsDriving() && DriveAnim) ? DriveAnim.Get() : Pick(SeatedAnim, IdleAnim);
	else if (GetCondition() == EPVVCondition::Restrained) Wanted = (GetStruggleProgress() > 0.f && StruggleAnim) ? StruggleAnim.Get() : Pick(RestrainedAnim, Locomotion);
	else if (bInDeepWater) Wanted = Pick(SwimAnim, Locomotion);
	else if (Move && Move->IsFalling()) Wanted = Pick(FallAnim, Locomotion);
	else if (GetHoldProgress() > 0.f) Wanted = Pick(WorkAnim, Locomotion);
	else if (IsDancing() && DanceAnim) Wanted = DanceAnim.Get();
	else if (ActiveEmote >= 0 && EmoteAnimFor(ActiveEmote)) Wanted = EmoteAnimFor(ActiveEmote);
	else if (bLiftOverhead && LiftAnim) Wanted = LiftAnim.Get();
	else if (bBlocking && BlockAnim) Wanted = BlockAnim.Get();
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
		BRAudio::PlayAt(this, Sound, Start, BRAudio::Loud);
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
	if (Target->IsWarden() || Target->IsFakeAgent())
	{
		Notify(LOCTEXT("ScanWarden", "SCAN: that's a fellow agent."));
	}
	else if (Target->IsHiddenFromPatrol())
	{
		Notify(LOCTEXT("ScanNothing3", "SCAN: nobody there."));
	}
	else if (Target->FailsPapersScan())
	{
		// The scanner checks PAPERS (contraband and weapons need a search on foot). Players and NPCs read the same.
		const bool bNoPapers = true;
		TArray<FString> Found;
		if (Target->bCarriesContraband)
		{
			Found.AddUnique(BR::ItemName(Target->ContrabandItem));
		}
		if (const ABRPlayerState* TPS = Target->GetBRPlayerState())
		{
			for (const EBRItem I : TPS->Inventory)
			{
				if (BR::IsIllegal(I))
				{
					Found.AddUnique(BR::ItemName(I));
				}
			}
		}
		FString Line = bNoPapers ? TEXT("SCAN: NO PAPERS - ILLEGAL!") : TEXT("SCAN: papers OK...");
		if (Found.Num() > 0)
		{
			Line += TEXT("  CONTRABAND: ") + FString::Join(Found, TEXT(", ")).ToUpper();
		}
		Line += TEXT("  (tagged - order them out / zip-tie them)");
		Target->Spot(8.f);
		// Busted NPCs react: in a truck the truck panics in a moment; on foot they react right away.
		if (Target->IsNPC())
		{
			if (ABRTruck* Ride = Cast<ABRTruck>(Target->GetCurrentBoat()))
			{
				Ride->OnSuspectScanned();
			}
			else if (!Target->IsBrawling())
			{
				Target->ReactToBust(this, !Target->bIllegal);
			}
		}
		ABRGameState::Sfx(this, EBRSfx::ScanAlarm, Target->GetActorLocation());
		ABRGameState::FX(this, EBRFX::PointsText, Target->GetActorLocation() + FVector(0.f, 0.f, 130.f), bNoPapers ? TEXT("ILLEGAL") : TEXT("CONTRABAND"));
		Notify(FText::FromString(Line));
	}
	else
	{
		ABRGameState::Sfx(this, EBRSfx::ScanOK, Target->GetActorLocation());
		const bool bNervous = Target->bCarriesContraband || (Target->GetBRPlayerState() && Target->GetBRPlayerState()->Inventory.ContainsByPredicate(
			[](EBRItem I) { return BR::IsIllegal(I) || I == EBRItem::Knife; }));
		Notify(bNervous ? LOCTEXT("ScanNervous", "SCAN: papers in order... but they look NERVOUS. Order them out and SEARCH them (hold E).")
			: LOCTEXT("ScanCitizen", "SCAN: papers in order. Regular citizen."));
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

	const EBRTrap Type = static_cast<EBRTrap>(SelectedTrap % 4); // pick with N
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
	if (GetCurrentBoat() || GetCarrier() || bInCannon || bOnShoulders || bOnRide)
	{
		return; // switched on again when you get out (balloon / wheelbarrow rides switch collision off, so the body has no physics then)
	}
	// The bones must have been posed at least once, and the body must have its physics bodies, or the physics reads
	// bones / bodies that don't exist (engine crash: "index 2 into an array of size 0").
	if (!MusclesSafe())
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
	PhysicalAnimation->SetComponentTickEnabled(true);
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
	PhysicalAnimation->SetComponentTickEnabled(false); // and it doesn't tick at all while it's off
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
		const float LegMuscle = bClimbing ? LegStrength * 0.08f : LegStrength; // on a wall: legs dangle and kick
		FPhysicalAnimationData Legs = Muscle(LegMuscle);
		Legs.PositionStrength = bClimbing ? 0.f : LegStrength * WobbleStrength * 0.5f;
		Legs.VelocityStrength = LegMuscle * WobbleStrength * 0.05f;
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
	// Momentum: start running, stop or turn and the upper body flops the other way first (Gang Beasts drunk gait).
	{
		const FVector Vel = GetVelocity();
		const FVector Change = (Vel - LastWobbleVelocity) * FVector(1.f, 1.f, 0.f);
		LastWobbleVelocity = Vel;
		if (MomentumSway > 0.f && Change.Size() > 8.f && Change.Size() < 1500.f && GetMesh()->IsSimulatingPhysics(TEXT("spine_02")))
		{
			GetMesh()->AddImpulseToAllBodiesBelow(-Change * MomentumSway, TEXT("spine_02"), true, true);
		}
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
	float WantDizzy = GetCondition() == EPVVCondition::Normal ? FMath::Lerp(0.3f, 1.f, GetBalanceFraction()) * (IsTipsy() ? 0.55f : 1.f) : 1.f;
	if (Now < HitFlopUntil)
	{
		WantDizzy *= 0.2f; // just got clobbered: everything goes limp for a split second
	}
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
		const float Woozy = 1.f + (Hunger < WeakHunger ? 1.5f : 0.f) + (1.f - GetBalanceFraction()) * 2.f + (IsTipsy() ? 2.5f : 0.f);
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
		// Remember who started it (self-defense: shooting someone who's attacking you isn't a scandal).
		if (ABRCharacter* Hitter = Cast<ABRCharacter>(Attacker); Hitter && Hitter != this)
		{
			Hitter->LastAggressionTime = GetWorld()->GetTimeSeconds();
			Hitter->LastAggressionTarget = this;
			LastTosser = Hitter;
			LastTossTime = GetWorld()->GetTimeSeconds();
		}
		// Hit an NPC and they fight you back (Gang Beasts style). Starting fights is trouble.
		ABRCharacter* BRAttacker = Cast<ABRCharacter>(Attacker);
		// Only a REAL hit starts a fight (a proper punch, or a few hard knocks) - not bumping into someone.
		// And your own smuggling clients never turn on you.
		const bool bProvoked = ProvokedBy.Get() == BRAttacker && ProvokeDamage >= FightBackDamage;
		const bool bMyCoyote = Leader.IsValid() && Leader.Get() == BRAttacker;
		if (bIsNPC && !bNoFightBack && !bPeaceful && bProvoked && !bMyCoyote && BRAttacker && !BRAttacker->IsNPC() && BRAttacker != this && GetCondition() == EPVVCondition::Normal)
		{
			if (!IsBrawling() || FMath::FRand() < 0.3f)
			{
				StartBrawling(BRAttacker, 25.f, FMath::FRandRange(0.55f, 0.9f));
			}
			if (!BRAttacker->IsNPC() && LastHitBy.Get() != Attacker)
			{
				LastHitBy = Attacker;
				if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
				{
					GM->AddTrouble(BRAttacker, 1);
				}
			}
		}
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
		if (HasAuthority())
		{
			OnPrisonerStateChanged(Was, GetCondition());
		}
		if (Was == EPVVCondition::KnockedDown && GetCondition() == EPVVCondition::Normal && !GetCurrentBoat() && !GetCarrier())
		{
			if (HasAuthority())
			{
				bDropKicking = false;
			}
			// Gang Beasts get-up: push up off the floor (can't walk for a moment - grab them while they're getting up!).
			GetUpUntil = GetWorld()->GetTimeSeconds() + GetUpSeconds;
			if (GetUpAnim)
			{
				PlayOneShot(GetUpAnim);
			}
			if (HasAuthority())
			{
				ApplySlow(GetUpSeconds, 0.05f);
			}
		}
		if (GetNetMode() != NM_DedicatedServer && GetWorld()->GetTimeSeconds() > 2.f)
		{
			const EBRSfx Which = GetCondition() == EPVVCondition::Restrained && Was != EPVVCondition::Restrained ? EBRSfx::ZipTie
				: (GetCondition() == EPVVCondition::Detained ? EBRSfx::JailDoor : EBRSfx::Countdown);
			if (Which != EBRSfx::Countdown)
			{
				if (USoundBase* Wave = GetDefault<UBRSettings>()->GetSound(Which))
				{
					BRAudio::PlayAt(this, Wave, GetActorLocation(), BRAudio::Medium);
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
		// Big knockout near you: a punchy zoom + shake (and a moment of slow-mo when playing alone).
		if (GetNetMode() != NM_DedicatedServer)
		{
			const APlayerController* LocalPC = GetWorld()->GetFirstPlayerController();
			ABRCharacter* Viewer = LocalPC ? Cast<ABRCharacter>(LocalPC->GetPawn()) : nullptr;
			const float NowKO = GetWorld()->GetTimeSeconds();
			if (Viewer && FVector::Dist(Viewer->GetActorLocation(), GetActorLocation()) < 700.f && NowKO - Viewer->LastKOPunchTime > 3.f)
			{
				Viewer->LastKOPunchTime = NowKO;
				Viewer->KOPunch = 1.f;
				Viewer->AddShake(0.7f);
				if (GetNetMode() == NM_Standalone)
				{
					UGameplayStatics::SetGlobalTimeDilation(this, 0.3f);
					FTimerHandle SlowMo;
					GetWorldTimerManager().SetTimer(SlowMo, FTimerDelegate::CreateWeakLambda(this, [this]() { UGameplayStatics::SetGlobalTimeDilation(this, 1.f); }), 0.12f, false);
				}
			}
		}
		MashCount = 0;
		KnockedDownAt = GetWorld()->GetTimeSeconds();
		if (KnockoutSound && GetNetMode() != NM_DedicatedServer)
		{
			BRAudio::PlayAt(this, KnockoutSound, GetActorLocation(), BRAudio::Medium);
		}
		// Go fully limp: unhook the muscles completely while knocked down (they're hooked back up when you get up).
		PhysicalAnimation->SetSkeletalMeshComponent(nullptr);
		PhysicalAnimation->SetComponentTickEnabled(false);
		bWobbleOn = false;
		// Drop kick: throw the feet forward and the head back so you fly feet-first.
		if (bDropKicking && GetMesh()->IsSimulatingPhysics())
		{
			const FVector Fwd = GetActorForwardVector().GetSafeNormal2D();
			for (const TCHAR* Foot : { TEXT("foot_l"), TEXT("foot_r") })
			{
				if (GetMesh()->GetBoneIndex(Foot) != INDEX_NONE)
				{
					GetMesh()->AddImpulse(Fwd * 900.f + FVector(0.f, 0.f, 350.f), Foot, true);
				}
			}
			if (GetMesh()->GetBoneIndex(TEXT("head")) != INDEX_NONE)
			{
				GetMesh()->AddImpulse(-Fwd * 450.f - FVector(0.f, 0.f, 150.f), TEXT("head"), true);
			}
		}
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
		TickFlight();
		TickPrisonBreak(DeltaSeconds);
		TickWander(DeltaSeconds);
		TickFollow(DeltaSeconds);
		if (bIsNPC)
		{
			TickSearchWait();
		}
		TickBrawl(DeltaSeconds);
		TickThrown();
		TickNPCStruggle();
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
	TickBlink();
	TickTrumpetAudio();
	UpdateHairUnderHat(); // hats from the Blueprint, NPC hats, hats knocked off / picked up: always squash the hair right
	if (KOStars && GetNetMode() != NM_DedicatedServer)
	{
		const bool bKO = GetCondition() == EPVVCondition::KnockedDown;
		const bool bDizzy = !bKO && GetCondition() == EPVVCondition::Normal && (GetBalanceFraction() < 0.3f || IsDazed()) && !GetCurrentBoat();
		KOStars->SetHiddenInGame(!bKO && !bDizzy);
		if (bDizzy)
		{
			// Rocked: little birds circle your head until you get your balance back.
			KOStars->SetText(FText::FromString(TEXT("v  ~  v  ~  v")));
			KOStars->SetTextRenderColor(FColor(140, 200, 255));
			const FVector Head = GetMesh() && GetMesh()->GetBoneIndex(TEXT("head")) != INDEX_NONE ? GetMesh()->GetBoneLocation(TEXT("head")) : GetActorLocation();
			KOStars->SetWorldLocationAndRotation(Head + FVector(0.f, 0.f, 40.f), FRotator(0.f, GetWorld()->GetTimeSeconds() * 420.f, 0.f));
		}
		else if (bKO && bKnockedOut)
		{
			// Out cold: Z z z floating up off the head.
			const int32 Phase = FMath::FloorToInt(GetWorld()->GetTimeSeconds() * 1.5f) % 3;
			KOStars->SetText(FText::FromString(Phase == 0 ? TEXT("z") : Phase == 1 ? TEXT("z  Z") : TEXT("z  Z  Z")));
			KOStars->SetTextRenderColor(FColor(200, 225, 255));
		}
		else if (bKO)
		{
			KOStars->SetText(FText::FromString(TEXT("*  @  *")));
			KOStars->SetTextRenderColor(FColor(255, 230, 60));
		}
		if (bKO)
		{
			// Stars circle the head (follow the ragdoll's head).
			const FVector Head = GetMesh() && GetMesh()->GetBoneIndex(TEXT("head")) != INDEX_NONE ? GetMesh()->GetBoneLocation(TEXT("head")) : GetActorLocation();
			KOStars->SetWorldLocationAndRotation(Head + FVector(0.f, 0.f, 45.f), FRotator(0.f, GetWorld()->GetTimeSeconds() * 360.f, 0.f));
		}
	}
	// Daze wears off: walk normally again.
	if (IsDazed() != bDazedSpeedApplied)
	{
		bDazedSpeedApplied = IsDazed();
		UpdateMovementSpeed();
	}
	// Guard: comes back over time; NPC blocks end on their own; can't keep blocking while knocked down etc.
	if (HasAuthority())
	{
		GuardMeter = FMath::Min(1.f, GuardMeter + DeltaSeconds * (bBlocking ? 0.05f : 0.3f));
		if (!bBlocking && bBlockHeld && GuardResumeAt > 0.f && GetWorld()->GetTimeSeconds() >= GuardResumeAt)
		{
			GuardResumeAt = -1.f;
			Server_SetBlock_Implementation(true); // guard back up after the punch
		}
		if (bBlocking && ((bIsNPC && GetWorld()->GetTimeSeconds() >= BlockReleaseAt) || GetCondition() != EPVVCondition::Normal || GetCarrier() || GetCurrentBoat()))
		{
			bBlocking = false;
			UpdateMovementSpeed();
		}
	}
	// Knocked out, cuffed, carried, in a vehicle or locked up: the cardboard box falls off.
	if (HasAuthority() && bInBox && (GetCondition() != EPVVCondition::Normal || GetCarrier() || GetCurrentBoat()))
	{
		bInBox = false;
		OnRep_Box();
		ForceNetUpdate();
	}
	// Knocked down or cuffed: the knife goes clattering away.
	if (HasAuthority() && bIsNPC && HeldItem == EBRItem::Knife && GetCondition() != EPVVCondition::Normal)
	{
		SetNPCHeldItem(EBRItem::None);
	}
	if (HasAuthority() && bIsNPC && HeldItem == EBRItem::IllegalGun && GetCondition() != EPVVCondition::Normal)
	{
		SetNPCHeldItem(EBRItem::None); // drops the gun when they go down
	}
	// Safety: if the body ever loses its posed bones (mesh swapped, re-registered), switch the muscles off
	// before the physics reads bones that aren't there, and switch them back on once it's posed again.
	if (PhysicalAnimation && (PhysicalAnimation->GetSkeletalMesh() || PhysicalAnimation->IsComponentTickEnabled()) && !MusclesSafe())
	{
		if (bWobbleOn)
		{
			DisableWobble();
		}
		PhysicalAnimation->SetSkeletalMeshComponent(nullptr);
		PhysicalAnimation->SetComponentTickEnabled(false);
		bWobbleOn = false;
		bWobblePending = GetCondition() != EPVVCondition::KnockedDown;
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
	TickActiveRagdoll(DeltaSeconds);
	DragHeldBody(DeltaSeconds);
	TickAttackSteer();
	TickSprintStamina(DeltaSeconds);
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
		bool bShowLabel = !bIsNPC && Viewer == GetFaction() && Viewer != EPVVFaction::None && !IsLocallyControlled();
		if (bShowLabel && LocalPC && LocalPC->PlayerCameraManager)
		{
			// Fade out with distance (smaller, then gone) so far-away teammates don't clutter the screen.
			const float Dist = FVector::Dist(LocalPC->PlayerCameraManager->GetCameraLocation(), GetActorLocation());
			bShowLabel = Dist < 3000.f;
			StatusLabel->SetWorldSize(FMath::GetMappedRangeValueClamped(FVector2D(800.f, 3000.f), FVector2D(22.f, 10.f), Dist));
		}
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
		if (!bHide)
		{
			// Showing everything again also switched on parts that should stay hidden (the cardboard box, the
			// cigarette, the rope, holstered guns...). Put them back how they should be.
			OnRep_Box();
			OnRep_HeldItem();
			if (CigaretteMesh)
			{
				CigaretteMesh->SetVisibility(ActiveEmote == 6);
			}
			if (RopeMesh)
			{
				RopeMesh->SetVisibility(false);
			}
			GunStyle = -1; // re-applies the gun visibility next frame
		}
		if (StatusLabel && !bHide)
		{
			StatusLabel->SetVisibility(!bIsNPC && Viewer == GetFaction() && Viewer != EPVVFaction::None && !IsLocallyControlled());
		}
	}
}

// -------------------------------------------------------------------------------------------
// Brawling: slaps, grab & throw, dive tackles, shoulder rides, props

void ABRCharacter::Input_Grab() { Server_Grab(); }
bool ABRCharacter::IsHoldingProp() const { return HeldProp != nullptr; }
void ABRCharacter::Input_Dive()
{
	// Clicking (fists out) = superman dive punch. Not clicking = flying DROP KICK.
	if (bLeftDown || bRightDown)
	{
		Server_Dive();
	}
	else
	{
		Server_DropKick();
	}
}

void ABRCharacter::StopJumping()
{
	if (bSpaceHeld && bClimbing)
	{
		Server_ClimbHold(false);
	}
	bSpaceHeld = false;
	Super::StopJumping();
}

bool ABRCharacter::HandleJumpPressed()
{
	bSpaceHeld = true;
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
	if (bChugging)
	{
		Server_Chug(); // drinking contest: mash Space to chug
		return true;
	}
	if (GetCondition() == EPVVCondition::Detained)
	{
		Server_PickCuffs(); // locked up: mash Space to pick your cuffs when nobody's watching
		return true;
	}
	if (GrabbedBy)
	{
		Server_GrabStruggle(); // mash Space to wriggle out of their hands
		return true;
	}
	if (HandGrabTarget && (HandGrabMask == 3 || bLiftOverhead))
	{
		Server_Lift(); // both hands on someone: SPACE = heave them over your head (again = BODY SLAM)
		return true;
	}
	if (bHandHanging)
	{
		Server_HangJump(); // hanging off your hands: kick and heave yourself UP (a real jump - you still have to make it over)
		return true;
	}
	if (bClimbing)
	{
		if (bHanging)
		{
			Server_ClimbJump(); // at the top: pull yourself up
		}
		else
		{
			Server_ClimbHold(true); // hold SPACE to climb (look up = up, look down = down)
		}
		return true;
	}
	return false;
}

void ABRCharacter::Server_ClimbLetGo_Implementation()
{
	if (bClimbing)
	{
		WallHandMask = 0;
		StopClimbing(true);
		LaunchCharacter(FVector(ClimbNormal) * 150.f, true, false);
	}
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
		BRAudio::PlayAt(this, Sound, GetActorLocation(), BRAudio::Near);
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
	if (bClimbing)
	{
		Server_ClimbLetGo_Implementation(); // Z on a wall: let go
		return;
	}
	if (GetCondition() != EPVVCondition::Normal || GetCurrentBoat() || GetCarrier() || bInCannon || bOnShoulders)
	{
		return;
	}
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	BlowDisguise();

	// Holding someone with your hands? THROW them. Hold BACK (S) = SUPLEX over your head, hold FORWARD (W) = big toss.
	if (ABRCharacter* Held = HandGrabTarget.Get())
	{
		const bool bFromOverhead = bLiftOverhead && GetWorld()->GetTimeSeconds() - LiftStartTime >= LiftSeconds;
		const bool bLimp = Held->GetCondition() == EPVVCondition::KnockedDown;
		ReleaseHandGrab();
		Held->LastTosser = this;
		if (bFromOverhead || bLimp)
		{
			// Chuck the floppy body (from over your head it really flies).
			const float Power = bFromOverhead ? OverheadThrowScale : 0.8f;
			const FVector Vel = Forward * ThrowSpeed * Power + FVector(0.f, 0.f, ThrowSpeed * (bFromOverhead ? 0.35f : 0.5f)) + GetVelocity() * 0.5f;
			Held->LastTossTime = GetWorld()->GetTimeSeconds();
			Held->FallFromZ = FMath::Max(Held->FallFromZ, Held->GetActorLocation().Z + (bFromOverhead ? 150.f : 0.f));
			Held->Multicast_FlingBody(Vel);
			Held->MarkThrown(this);
			ABRGameState::Highlight(this, Held, bFromOverhead ? 75.f : 55.f, FString::Printf(TEXT("%s %s %s"), *GetPlayerDisplayName(), bFromOverhead ? TEXT("HURLED") : TEXT("FLUNG"), *Held->GetPlayerDisplayName()));
			if (ABRPlayerState* PS = GetBRPlayerState())
			{
				++PS->StatThrows;
			}
			if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
			{
				GM->Feed(FString::Printf(TEXT("%s %s %s"), *GetPlayerDisplayName(), bFromOverhead ? TEXT("HURLED") : TEXT("FLUNG"), *Held->GetPlayerDisplayName()));
			}
			AddShake(0.4f);
			Multicast_Sfx(2);
			return;
		}
		Held->LastTossTime = GetWorld()->GetTimeSeconds();
		const FVector Input = GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D();
		const float Dir = Input.IsNearlyZero() ? 0.f : FVector::DotProduct(Input, Forward);
		if (Dir < -0.4f)
		{
			Held->ApplyKnockdown(this, -Forward * 450.f + FVector(0.f, 0.f, 650.f)); // SUPLEX
			if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
			{
				GM->Feed(FString::Printf(TEXT("%s SUPLEXED %s!"), *GetPlayerDisplayName(), *Held->GetPlayerDisplayName()));
			}
			ABRGameState::Highlight(this, Held, 70.f, FString::Printf(TEXT("%s SUPLEXED %s"), *GetPlayerDisplayName(), *Held->GetPlayerDisplayName()));
			AddShake(0.6f);
			Multicast_Sfx(1);
			return;
		}
		const float Power = Dir > 0.4f ? 1.35f : 1.f;
		const FVector ThrowVel = Forward * ThrowSpeed * Power + FVector(0.f, 0.f, ThrowSpeed * 0.45f) + GetVelocity() * 0.4f;
		// Gang Beasts: they fly FLOPPY - a ragdoll throw, then up again quickly (unless they were out cold).
		Held->MarkThrown(this);
		Held->ApplyKnockdown(this, ThrowVel / FMath::Max(0.1f, Held->KnockbackScale));
		if (Held->GetCondition() == EPVVCondition::KnockedDown)
		{
			Held->Multicast_FlingBody(ThrowVel);
			if (!Held->bKnockedOut)
			{
				Held->SetKnockdownTimeLeft(ThrowKnockdownSeconds);
			}
		}
		else
		{
			Held->LaunchCharacter(ThrowVel, true, true);
		}
		ABRGameState::Highlight(this, Held, 60.f, FString::Printf(TEXT("%s THREW %s"), *GetPlayerDisplayName(), *Held->GetPlayerDisplayName()));
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
		ABRGameState::Highlight(this, Cast<ABRCharacter>(Held), 62.f, FString::Printf(TEXT("%s THREW %s"), *GetPlayerDisplayName(), *Held->GetPlayerDisplayName()));
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
		if (!TryClimb() && !Cast<APlayerController>(GetController())) // nothing to grab: grab the ledge in front and climb
		{
			StartWallClimb(); // (bots / NPCs; players climb with their hands)
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
	const float WaterBoost = (bInDeepWater || GetCharacterMovement()->IsSwimming()) ? 1.7f : 1.f; // belly-flop slide through water
	LaunchCharacter(Forward * DiveSpeed * MaskBoost * WaterBoost + FVector(0.f, 0.f, 350.f), true, true);
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

	// Diving head-first into a wall / car / fence: BONK - that one knocks you down.
	if (bDiving && HasAuthority() && GetVelocity().Size2D() > 250.f)
	{
		const FVector DiveDir = GetVelocity().GetSafeNormal2D();
		const FVector HeadAt = GetMesh() && GetMesh()->GetBoneIndex(TEXT("head")) != INDEX_NONE ? GetMesh()->GetBoneLocation(TEXT("head"))
			: GetActorLocation() + FVector(0.f, 0.f, 60.f);
		FHitResult Bonk;
		FCollisionQueryParams BonkParams(SCENE_QUERY_STAT(BRDiveBonk), false, this);
		if (GetWorld()->LineTraceSingleByChannel(Bonk, HeadAt, HeadAt + DiveDir * 60.f, ECC_Visibility, BonkParams)
			&& !Cast<APawn>(Bonk.GetActor()) && FMath::Abs(Bonk.ImpactNormal.Z) < 0.5f)
		{
			bDiving = false;
			ABRGameState::FX(this, EBRFX::PointsText, Bonk.ImpactPoint + FVector(0.f, 0.f, 40.f), TEXT("BONK!"));
			Multicast_Sfx(1);
			AddKO(HeadHitKO, nullptr);
			ApplyKnockdown(nullptr, -DiveDir * 250.f + FVector(0.f, 0.f, 100.f));
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
				if (ArmsReaching != 0)
				{
					// Superman DIVE PUNCH: fists first into them.
					Victim->ReceiveMeleeHit(this, AttackDamage * 1.4f, Forward * 400.f + FVector(0.f, 0.f, 150.f));
					if (ABRCharacter* BRV = Cast<ABRCharacter>(Victim))
					{
						BRV->AddKO(BRV->HeadHitKO * 1.2f, this);
						BRV->Multicast_BodyHit(TEXT("head"), Forward * HitReaction * 1.4f);
					}
					Victim->ApplyKnockdown(this, (Forward * 450.f + FVector(0.f, 0.f, 200.f)) * TackleBoost);
					ABRGameState::FX(this, EBRFX::PointsText, Victim->GetActorLocation() + FVector(0.f, 0.f, 80.f), TEXT("DIVE PUNCH!"));
				}
				else
				{
					Victim->ApplyKnockdown(this, (Forward * 650.f + FVector(0.f, 0.f, 300.f)) * TackleBoost);
				}
				ABRGameState::Highlight(this, Cast<ABRCharacter>(Victim), 50.f, FString::Printf(TEXT("%s flying-tackled %s"), *GetPlayerDisplayName(), *Victim->GetPlayerDisplayName()));
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
	TickKickContact(DeltaSeconds);
	TickHandHang(DeltaSeconds);
	TickHandGrab(DeltaSeconds);
	TickKOMeter(DeltaSeconds);
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
		// On a wall: hold Space + click = reach up with this hand (alternate L, R, L, R...).
		bLeftDown = true;
		LeftDownAt = GetWorld()->GetTimeSeconds();
		if (bSpaceHeld)
		{
			Server_ClimbStep(true);
		}
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
	if (bClimbing && !bRightDown)
	{
		Server_ClimbLetGo(); // both hands off the wall: drop
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
	if (bClimbing && !bLeftDown)
	{
		Server_ClimbLetGo();
	}
}

void ABRCharacter::BotScan(ABRCharacter* Who)
{
	if (Who)
	{
		const FVector From = GetActorLocation() + FVector(0.f, 0.f, 40.f);
		SetActorRotation(FRotator(0.f, (Who->GetActorLocation() - GetActorLocation()).Rotation().Yaw, 0.f));
		Server_Scan_Implementation(From, (Who->GetActorLocation() - From).GetSafeNormal());
	}
}

void ABRCharacter::BotOrderOut()
{
	Server_OrderOut_Implementation();
}

bool ABRCharacter::BotTase(ABRCharacter* Who)
{
	if (!HasAuthority() || !Who || !IsWarden() || GetCurrentBoat() || AmmoTaser <= 0)
	{
		return false;
	}
	const bool bWasDrawn = bWeaponDrawn;
	const bool bWasLethal = bLethalMode;
	bWeaponDrawn = true;
	bLethalMode = false;
	const FVector Eye = GetActorLocation() + FVector(0.f, 0.f, 60.f);
	const FVector Aim = (Who->GetActorLocation() + Who->GetVelocity() * 0.15f - Eye).GetSafeNormal(); // lead the target a little
	SetActorRotation(FRotator(0.f, Aim.Rotation().Yaw, 0.f));
	const int32 Before = AmmoTaser;
	Server_Fire_Implementation(Eye, Aim);
	bWeaponDrawn = bWasDrawn;
	bLethalMode = bWasLethal;
	return AmmoTaser != Before;
}

void ABRCharacter::Input_BlockStart() { Server_SetBlock(true); }
void ABRCharacter::Input_OrderOut() { Server_OrderOut(); }

void ABRCharacter::Server_OrderOut_Implementation()
{
	if (!IsWarden())
	{
		return;
	}
	// The closest vehicle with people in it.
	ABRCar* Best = nullptr;
	float BestDist = 700.f;
	for (TActorIterator<ABRCar> It(GetWorld()); It; ++It)
	{
		const float D = FVector::Dist(It->GetActorLocation(), GetActorLocation());
		if (D < BestDist && (It->CanOrderOut(this) || It->HasPrisonerFor(this)))
		{
			BestDist = D;
			Best = *It;
		}
	}
	if (!Best)
	{
		Notify(LOCTEXT("NoCarToOrder", "Walk up to a vehicle with people in it, then press O to order them out."));
		return;
	}
	if (Best->HasPrisonerFor(this) && !Best->HasFreePeopleAboard(this))
	{
		// Just a zip-tied prisoner in there: haul them out onto your shoulder.
		Best->TakePrisonerOut(this);
		return;
	}
	ABRGameState::Sfx(this, EBRSfx::Whistle, GetActorLocation()); // *TWEEEET*
	Best->OrderOut(this);
	Notify(LOCTEXT("OrderedOut", "\"EVERYBODY OUT!\""));
}
void ABRCharacter::Input_BlockStop() { Server_SetBlock(false); }

void ABRCharacter::Server_SetBlock_Implementation(bool bOn)
{
	const float Now = GetWorld()->GetTimeSeconds();
	bBlockHeld = bOn;
	if (!bOn)
	{
		GuardResumeAt = -1.f;
	}
	const bool bCan = GetCondition() == EPVVCondition::Normal && !GetCarrier() && !GetCurrentBoat() && !GetCarriedCharacter() && !bClimbing && Now >= GuardBrokenUntil;
	const bool bWant = bOn && bCan;
	if (bWant != bBlocking)
	{
		bBlocking = bWant;
		if (bBlocking)
		{
			StopEmote();
			ReleaseHandGrab();
		}
		UpdateMovementSpeed();
		ForceNetUpdate();
	}
}

void ABRCharacter::BotBlock(float Seconds)
{
	if (HasAuthority() && GetWorld()->GetTimeSeconds() >= GuardBrokenUntil)
	{
		Server_SetBlock_Implementation(true);
		BlockReleaseAt = GetWorld()->GetTimeSeconds() + Seconds;
	}
}

bool ABRCharacter::IsThreatTo(const ABRCharacter* Who) const
{
	if (!Who || GetCondition() != EPVVCondition::Normal)
	{
		return false;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	return (LastAggressionTarget.Get() == Who && Now - LastAggressionTime < 12.f)	// hit you in the last 12 s
		|| (IsBrawling() && GetBrawlTarget() == Who)							// NPC coming at you
		|| Who->GrabbedBy.Get() == this											// has you in their hands
		|| (HeldItem == EBRItem::Knife && FVector::Dist(GetActorLocation(), Who->GetActorLocation()) < 1500.f); // knife out
}

void ABRCharacter::Server_PickCuffs_Implementation()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (GetCondition() != EPVVCondition::Detained || bGuardWatching || Now - LastMashTime < 0.08f)
	{
		return;
	}
	LastMashTime = Now;
	EscapeProgress = FMath::Min(1.f, EscapeProgress + 0.012f);
	if (EscapeProgress >= 1.f)
	{
		BreakOut();
	}
}

void ABRCharacter::TickPrisonBreak(float DeltaSeconds)
{
	if (!HasAuthority() || GetCondition() != EPVVCondition::Detained)
	{
		EscapeProgress = 0.f;
		return;
	}
	// Is any Border Patrol player close enough to see you?
	bool bWatched = false;
	for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
	{
		if (It->IsWarden() && !It->IsNPC() && It->GetCondition() == EPVVCondition::Normal && FVector::Dist(It->GetActorLocation(), GetActorLocation()) < 1800.f)
		{
			bWatched = true;
			break;
		}
	}
	bGuardWatching = bWatched;
	if (bWatched)
	{
		EscapeProgress = FMath::Max(0.f, EscapeProgress - DeltaSeconds * 0.05f); // they notice your fiddling
	}
	else if (bIsNPC)
	{
		EscapeProgress += DeltaSeconds / 40.f; // NPCs work their cuffs loose on their own (~40 s unwatched)
		if (EscapeProgress >= 1.f)
		{
			BreakOut();
		}
	}
}

void ABRCharacter::BreakOut()
{
	if (!HasAuthority() || GetCondition() != EPVVCondition::Detained)
	{
		return;
	}
	EscapeProgress = 0.f;
	const FVector Here = GetActorLocation();
	const FVector Zone = GetDetentionZone() ? GetDetentionZone()->GetActorLocation() : Here - GetActorForwardVector() * 100.f;
	FVector Away = (Here - Zone).GetSafeNormal2D();
	if (Away.IsNearlyZero())
	{
		Away = GetActorForwardVector().GetSafeNormal2D();
	}
	ReleaseFromDetention(Here + Away * 900.f + FVector(0.f, 0.f, 60.f));
	if (ABRPlayerState* PS = GetBRPlayerState())
	{
		PS->SentenceSeconds = 0;
	}
	if (bIsNPC)
	{
		StartFleeing(40.f);
	}
	Notify(LOCTEXT("BrokeOut", "You slipped the cuffs and got out! RUN!"));
	ABRGameState::Sfx(this, EBRSfx::TripwireAlarm, Here);
	if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
	{
		GM->Feed(FString::Printf(TEXT("PRISON BREAK! %s escaped the holding pen!"), *GetPlayerDisplayName()));
	}
	if (ABRGameState* GS = GetBRGameState())
	{
		GS->Multicast_NoisePing(Here);
	}
}

bool ABRCharacter::IsDazed() const
{
	const ABRGameState* GS = GetBRGameState();
	return GS && GetCondition() == EPVVCondition::Normal && DazedUntil > GS->GetServerWorldTimeSeconds();
}

float ABRCharacter::ModifyIncomingHit(APVVCharacter* Attacker, float Damage, FVector& Impulse)
{
	// Gang Beasts pacing: hits wear you down slowly, push you less, and the "knockout" hit dazes you first.
	Damage /= FMath::Max(0.1f, FightToughness);
	Impulse *= HitPushScale;
	// Remember how hard this person has been hitting us lately (NPCs only fight back once it's real).
	if (ABRCharacter* Hitter = Cast<ABRCharacter>(Attacker))
	{
		const float NowP = GetWorld()->GetTimeSeconds();
		if (ProvokedBy.Get() != Hitter || NowP - ProvokeTime > 6.f)
		{
			ProvokedBy = Hitter;
			ProvokeDamage = 0.f;
		}
		ProvokeDamage += Damage;
		ProvokeTime = NowP;
	}
	Damage = ApplyBlock(Attacker, Damage, Impulse);
	const ABRGameState* GS = GetBRGameState();
	if (!GS || GetCondition() != EPVVCondition::Normal || Damage <= 0.f)
	{
		return Damage;
	}
	AddLean(Impulse, Cast<ABRCharacter>(Attacker)); // the hit rocks your whole body (balance spring)
	const float Now = GS->GetServerWorldTimeSeconds();
	const float After = Balance - Damage * GetDamageTakenMultiplier();
	// A solid hit rocks you: you stagger for half a second (slow, wobbly) instead of just losing a number.
	if (Damage > 4.f && !IsSlowed())
	{
		ApplySlow(0.5f, 0.35f);
	}
	if (IsDazed())
	{
		return Damage * 1.5f; // already seeing birds: this one puts you down
	}
	if (After <= 0.f && Now - LastDazeTime > 6.f)
	{
		// Would've been a knockout: DAZED instead. Stagger around for a moment - one more good hit and you're out.
		LastDazeTime = Now;
		DazedUntil = Now + DazeSeconds;
		bBlocking = false;
		UpdateMovementSpeed();
		ABRGameState::FX(this, EBRFX::PointsText, GetActorLocation() + FVector(0.f, 0.f, 130.f), TEXT("DAZED!"));
		ForceNetUpdate();
		return FMath::Max(0.f, Balance / FMath::Max(0.01f, GetDamageTakenMultiplier()) - 1.f); // left hanging on by a thread
	}
	return Damage;
}

float ABRCharacter::ApplyBlock(APVVCharacter* Attacker, float Damage, FVector& Impulse)
{
	if (!bBlocking || !Attacker || GetCondition() != EPVVCondition::Normal)
	{
		return Damage;
	}
	// Only hits from the front are blocked (get hit from behind and you eat it).
	const FVector ToAttacker = (Attacker->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	if (FVector::DotProduct(ToAttacker, GetActorForwardVector().GetSafeNormal2D()) < 0.15f)
	{
		return Damage;
	}
	// Hits on your guard (arms up in front) only wear down the GUARD, not you - like Gang Beasts.
	GuardMeter -= FMath::Clamp(Damage / 25.f, 0.1f, 0.4f);
	ABRGameState::FX(this, EBRFX::Sparks, GetActorLocation() + GetActorForwardVector() * 40.f + FVector(0.f, 0.f, 60.f));
	Impulse *= 0.25f; // your arms soak the hit; you slide back a little
	if (GuardMeter <= 0.f)
	{
		// GUARD BREAK: arms knocked aside, you stagger for a moment and can't block.
		GuardMeter = 0.f;
		bBlocking = false;
		GuardBrokenUntil = GetWorld()->GetTimeSeconds() + 2.5f;
		if (const ABRGameState* GBS = GetBRGameState())
		{
			DazedUntil = GBS->GetServerWorldTimeSeconds() + 1.2f;
		}
		UpdateMovementSpeed();
		ABRGameState::FX(this, EBRFX::PointsText, GetActorLocation() + FVector(0.f, 0.f, 120.f), TEXT("GUARD BREAK!"));
		Notify(LOCTEXT("GuardBreak", "GUARD BROKEN! You're staggered and can't block for a moment."));
		ForceNetUpdate();
		return 0.f;
	}
	ABRGameState::FX(this, EBRFX::PointsText, GetActorLocation() + FVector(0.f, 0.f, 120.f), TEXT("BLOCKED"));
	return 0.f;
}

void ABRCharacter::Server_PunchHand_Implementation(bool bLeft)
{
	if (IsDazed())
	{
		return; // seeing birds
	}
	if (bBlocking)
	{
		// Punch out of your guard (like a real fight): the guard drops for the punch, then comes back up if you're still holding block.
		bBlocking = false;
		GuardResumeAt = GetWorld()->GetTimeSeconds() + 0.45f;
		UpdateMovementSpeed();
		ForceNetUpdate();
	}
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
	WallHandMask = static_cast<uint8>(WallHandMask & ~Hand);
	if (bPropInHands && ArmsReaching == 0)
	{
		bPropInHands = false;
		if (HeldProp)
		{
			HeldProp->Drop(); // let go of the button = let go of the thing
			SetHeldProp(nullptr);
		}
	}
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
	// Nobody right in front: anyone your HAND is actually touching (Gang Beasts - grab whatever you touch).
	if (!Best && GetMesh())
	{
		for (TActorIterator<ABRCharacter> It(GetWorld()); It && !Best; ++It)
		{
			ABRCharacter* Other = *It;
			if (Other == this || Other->GetCarrier() || Other->GetCurrentBoat() || Other->IsInCannon() || Other->GrabbedBy
				|| Other->GetCondition() == EPVVCondition::Detained || Other->GetCondition() == EPVVCondition::Restrained)
			{
				continue;
			}
			for (const TCHAR* HandBone : { TEXT("hand_l"), TEXT("hand_r") })
			{
				if (GetMesh()->GetBoneIndex(HandBone) != INDEX_NONE && FVector::Dist(GetMesh()->GetBoneLocation(HandBone), Other->GetBodyLocation()) < 75.f)
				{
					Best = Other;
					break;
				}
			}
		}
	}
	if (!Best)
	{
		// No person: a crate / chair / bottle your hands touch? Grab it (it stays in your hands while you hold the button).
		TArray<FOverlapResult> Near;
		FCollisionQueryParams PropParams(SCENE_QUERY_STAT(BRHandProp), false, this);
		FCollisionObjectQueryParams PropTypes;
		PropTypes.AddObjectTypesToQuery(ECC_PhysicsBody);
		PropTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
		GetWorld()->OverlapMultiByObjectType(Near, GetActorLocation() + Forward * 75.f + FVector(0.f, 0.f, 10.f), FQuat::Identity, PropTypes, FCollisionShape::MakeSphere(70.f), PropParams);
		for (const FOverlapResult& O : Near)
		{
			if (ABRPhysicsProp* Prop = Cast<ABRPhysicsProp>(O.GetActor()); Prop && !Prop->IsHeld())
			{
				Prop->PickUp(this);
				SetHeldProp(Prop);
				bPropInHands = true;
				Multicast_Sfx(5);
				return true;
			}
		}
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
		Best->bBlocking = false; // grabs beat blocks
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
	EndLift();
	HandGrabTarget = nullptr;
	HandGrabMask = 0;
	DragBone = 255;
	ArmsReaching = 0; // let go completely (press again to grab again)
	ForceNetUpdate();
}

void ABRCharacter::MarkThrown(ABRCharacter* By)
{
	ThrownBy = By;
	ThrownUntil = GetWorld()->GetTimeSeconds() + 1.6f;
	BowledOver.Reset();
}

void ABRCharacter::TickThrown()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now > ThrownUntil)
	{
		return;
	}
	// Flying body (ragdoll or not): anyone it slams into goes down too.
	USkeletalMeshComponent* Body = GetMesh();
	const bool bRag = IsRagdollActive() && Body && Body->GetBoneIndex(TEXT("pelvis")) != INDEX_NONE;
	const FVector Pos = bRag ? Body->GetBoneLocation(TEXT("pelvis")) : GetActorLocation();
	const FVector Vel = bRag ? Body->GetPhysicsLinearVelocity(TEXT("pelvis")) : GetVelocity();
	const bool bDropKick = bDropKicking && Now < DropKickHitUntil;
	if (Vel.Size() < (bDropKick ? 200.f : 450.f))
	{
		return;
	}
	TArray<FOverlapResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BRThrownHit), false, this);
	GetWorld()->OverlapMultiByObjectType(Hits, Pos, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(80.f), Params);
	ABRCharacter* Thrower = ThrownBy.Get();
	for (const FOverlapResult& H : Hits)
	{
		ABRCharacter* Other = Cast<ABRCharacter>(H.GetActor());
		if (!Other || Other == this || Other == Thrower || Other->GetCondition() != EPVVCondition::Normal || Other->GetCarrier() || Other->GetCurrentBoat()
			|| BowledOver.Contains(Other))
		{
			continue;
		}
		BowledOver.Add(Other);
		Other->MarkThrown(Thrower); // they can knock over the next one too (chain reaction!)
		Other->ApplyKnockdown(Thrower, Vel.GetSafeNormal2D() * FMath::Min(Vel.Size(), 1600.f) * 0.5f + FVector(0.f, 0.f, 220.f));
		if (Other->GetCondition() == EPVVCondition::KnockedDown && !Other->bKnockedOut)
		{
			Other->SetKnockdownTimeLeft(ThrowKnockdownSeconds);
		}
		Other->LastTosser = Thrower;
		Other->LastTossTime = Now;
		if (bDropKick)
		{
			Other->AddKO(Other->HeadHitKO, this); // two feet to the chest: that rings your bell
			Other->Multicast_BodyHit(TEXT("spine_03"), Vel.GetSafeNormal() * HitReaction * 1.5f);
		}
		ABRGameState::FX(this, EBRFX::PointsText, Other->GetActorLocation() + FVector(0.f, 0.f, 120.f), bDropKick ? TEXT("DROP KICK!") : TEXT("STRIKE!"));
		Multicast_Sfx(1);
		if (Thrower)
		{
			ABRGameState::Highlight(this, Thrower, 72.f, FString::Printf(TEXT("%s threw %s into %s"), *Thrower->GetPlayerDisplayName(), *GetPlayerDisplayName(), *Other->GetPlayerDisplayName()));
		}
	}
}

float ABRCharacter::TugPull() const
{
	// Two hands beat one, a steady body beats a wobbly one, pulling away (walking backwards) and mashing Space help.
	const int32 Hands = (HandGrabMask & 1) + ((HandGrabMask >> 1) & 1);
	float Pull = Hands + GetBalanceFraction() + GrabStruggle * 2.f;
	if (const ABRCharacter* Other = HandGrabTarget.Get())
	{
		const FVector Away = (GetActorLocation() - Other->GetActorLocation()).GetSafeNormal2D();
		Pull += FMath::Clamp(FVector::DotProduct(GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D(), Away), 0.f, 1.f) * 0.8f;
	}
	if (IsDazed())
	{
		Pull *= 0.5f;
	}
	if (WornHat == EBRHat::LuchaMask)
	{
		Pull += 0.3f; // wrestler
	}
	return Pull;
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
	if (bChoked && (!GrabbedBy || GetCondition() != EPVVCondition::Normal))
	{
		bChoked = false;
	}
	if (!Held)
	{
		const uint8 FreeHands = static_cast<uint8>(ArmsReaching & ~WallHandMask);
		// SUMO: both arms out, moving fast into someone = shove them (off ledges, into the river...).
		if (FreeHands == 3 && !bClimbing && Now >= NextSumoPush && GetVelocity().Size2D() > 300.f)
		{
			const FVector Fwd = GetActorForwardVector().GetSafeNormal2D();
			for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
			{
				ABRCharacter* Other = *It;
				const FVector To = Other->GetActorLocation() - GetActorLocation();
				if (Other != this && Other->GetCondition() == EPVVCondition::Normal && !Other->GetCurrentBoat() && To.Size2D() < 130.f
					&& FVector::DotProduct(To.GetSafeNormal2D(), Fwd) > 0.5f && FMath::Abs(To.Z) < 120.f)
				{
					NextSumoPush = Now + 0.7f;
					Other->LaunchCharacter(Fwd * 750.f * Other->KnockbackScale + FVector(0.f, 0.f, 160.f), true, true);
					Other->LastTosser = this;
					Other->LastTossTime = Now;
					Multicast_Sfx(1);
					return;
				}
			}
		}
		// Reaching with an empty hand: grab whoever it touches - or the wall in front of you.
		if (FreeHands != 0 && Now - LastGrabTryTime > 0.1f)
		{
			LastGrabTryTime = Now;
			if (!TryHandGrab(FreeHands) && !bClimbing)
			{
				TryWallGrab(FreeHands);
			}
		}
		return;
	}
	// Holding a person with one hand and reaching at a wall with the other: grab the wall too.
	{
		const uint8 NewHands = static_cast<uint8>(ArmsReaching & ~HandGrabMask & ~WallHandMask);
		if (NewHands != 0 && !bClimbing && Now - LastGrabTryTime > 0.1f)
		{
			LastGrabTryTime = Now;
			TryWallGrab(NewHands);
		}
	}
	const float Dist = FVector::Dist2D(Held->GetActorLocation(), GetActorLocation());
	if (GetCondition() != EPVVCondition::Normal || GetCurrentBoat() || GetCarrier() || Held->GetCurrentBoat() || Held->GetCarrier()
		|| Held->GetCondition() == EPVVCondition::Detained || Held->GetCondition() == EPVVCondition::Restrained || Dist > 320.f
		|| (HandGrabMask == 0 && ArmsReaching == 0))
	{
		ReleaseHandGrab();
		return;
	}
	// Grip more hands if they're reaching too (not the hand that's on the wall).
	HandGrabMask = static_cast<uint8>(HandGrabMask | (ArmsReaching & ~WallHandMask));
	// Holding a limp body: grab whatever part is nearest your hands (arm, leg, collar...) and DRAG it.
	if (Held->GetCondition() == EPVVCondition::KnockedDown)
	{
		if (DragBone == 255)
		{
			PickDragBone();
		}
		Held->ExtendKnockdown(DeltaSeconds * 0.5f); // being hauled around keeps you groggy a bit longer
		if (bLiftOverhead && HandGrabMask != 3)
		{
			EndLift(); // one hand let go: they flop down in front of you (still dragging by the other hand)
		}
	}
	else if (bLiftOverhead)
	{
		// They came round up there: they wriggle down in front of you.
		ReleaseHandGrab();
		Held->LaunchCharacter(GetActorForwardVector().GetSafeNormal2D() * 250.f + FVector(0.f, 0.f, 150.f), true, true);
		return;
	}
	// Tug of war: we're holding each other. Whoever pulls harder drags the other; even = stalemate.
	if (bTugOfWar && Held->GetCondition() == EPVVCondition::Normal && Held->HandGrabTarget.Get() == this && Held->HandGrabMask != 0)
	{
		const float Mine = TugPull();
		const float Theirs = Held->TugPull();
		if (Mine < Theirs + 0.15f)
		{
			if (Mine > Theirs - 0.15f)
			{
				// Stalemate: both locked together, shoving back and forth.
				const FVector Mid = (GetActorLocation() + Held->GetActorLocation()) * 0.5f;
				const FVector Axis = (Held->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
				const float Rock = 25.f * FMath::Sin(Now * 5.f);
				SetActorLocation(FMath::VInterpTo(GetActorLocation(), Mid - Axis * 50.f + Axis * Rock + FVector(0.f, 0.f, GetActorLocation().Z - Mid.Z), DeltaSeconds, 6.f), true);
			}
			return; // they're winning (their hands drag us), or nobody is
		}
	}
	// Really hold them: they stay at arm's length in front of you and get dragged wherever you go.
	if (Held->GetCondition() == EPVVCondition::Normal)
	{
		const FVector Fwd = GetActorForwardVector().GetSafeNormal2D();
		// Both hands, from BEHIND them: CHOKEHOLD (drains their balance; they can struggle or headbutt back).
		const bool bBehind = FVector::DotProduct(Held->GetActorForwardVector().GetSafeNormal2D(), (Held->GetActorLocation() - GetActorLocation()).GetSafeNormal2D()) > 0.35f;
		const bool bChoke = HandGrabMask == 3 && bBehind && !bClimbing;
		if (Held->bChoked != bChoke)
		{
			Held->bChoked = bChoke;
			if (bChoke)
			{
				Held->Notify(LOCTEXT("Choked", "CHOKEHOLD! Mash SPACE or headbutt (middle mouse) before you pass out!"));
			}
		}
		if (bChoke)
		{
			Held->DrainBalance(this, 13.f * DeltaSeconds);
		}
		FVector Want = GetActorLocation() + Fwd * (bChoke ? 55.f : 90.f);
		bool bDangle = false;
		if (bClimbing)
		{
			// Hanging off the wall with them in your other hand: they dangle below you.
			Want = GetActorLocation() + FVector(ClimbNormal) * 85.f - FVector(0.f, 0.f, 95.f);
			bDangle = true;
		}
		else
		{
			// Holding them out over a drop (ledge, bridge, Wall top): they dangle in the air.
			FHitResult Floor;
			FCollisionQueryParams FloorParams(SCENE_QUERY_STAT(BRDangle), false, this);
			FloorParams.AddIgnoredActor(Held);
			const float HeldHalf = Held->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			bDangle = !GetWorld()->LineTraceSingleByChannel(Floor, Want + FVector(0.f, 0.f, 40.f), Want - FVector(0.f, 0.f, HeldHalf + 160.f), ECC_Visibility, FloorParams);
			Want.Z = bDangle ? GetActorLocation().Z - 25.f : Held->GetActorLocation().Z;
		}
		const FVector Next = FMath::VInterpTo(Held->GetActorLocation(), Want, DeltaSeconds, 12.f);
		Held->SetActorLocation(Next, true, nullptr, ETeleportType::None);
		Held->GetCharacterMovement()->Velocity = bDangle ? FVector::ZeroVector : GetVelocity();
		if (bDangle)
		{
			Held->FallFromZ = FMath::Max(Held->FallFromZ, Held->GetActorLocation().Z); // drop them from up here = SPLAT
			Held->LastTosser = this;
			Held->LastTossTime = Now;
		}
		const FRotator Face = bChoke ? GetActorRotation() : (GetActorLocation() - Held->GetActorLocation()).Rotation();
		Held->SetActorRotation(FRotator(0.f, Face.Yaw, 0.f));
	}
}

bool ABRCharacter::TryWallGrab(uint8 Hands)
{
	if (Cast<APlayerController>(GetController()))
	{
		return false; // players climb with their hands and physics (sticky hands + TickHandHang), not a climb mode
	}
	if (!HasAuthority() || bClimbing || GetCondition() != EPVVCondition::Normal || GetCurrentBoat() || GetCarriedCharacter() || HeldProp || Hands == 0)
	{
		return false;
	}
	// Is there a wall / ledge right in front of the reaching hands?
	const FVector Fwd = GetActorForwardVector().GetSafeNormal2D();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BRWallGrab), false, this);
	if (ABRCharacter* Held = HandGrabTarget.Get())
	{
		Params.AddIgnoredActor(Held);
	}
	FHitResult Hit;
	for (const float Up : { 150.f, 110.f, 60.f, 20.f })
	{
		const FVector From = GetActorLocation() + FVector(0.f, 0.f, Up);
		if (GetWorld()->LineTraceSingleByChannel(Hit, From, From + Fwd * 95.f, ECC_Visibility, Params) && !Cast<APawn>(Hit.GetActor()) && FMath::Abs(Hit.ImpactNormal.Z) < 0.4f)
		{
			if (StartWallClimb())
			{
				WallHandMask = Hands;
				return true;
			}
			return false;
		}
	}
	return false;
}

void ABRCharacter::TickReachVisuals()
{
	USkeletalMeshComponent* Body = GetMesh();
	for (int32 h = 0; h < 2; ++h)
	{
		if (!(ArmsReaching & (1 << h)) || bClimbing || GetCondition() != EPVVCondition::Normal)
		{
			bHandAnchored[h] = false; // let go: the hand comes off whatever it was stuck to
		}
	}
	// Local player: tell the server when a mouse button has been held long enough to reach.
	if (IsLocallyControlled() && Cast<APlayerController>(GetController()))
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (bLeftDown && !(SentReach & 1) && Now - LeftDownAt > 0.15f)
		{
			SentReach = static_cast<uint8>(SentReach | 1);
			Server_HandHold(1, true);
		}
		if (bRightDown && !(SentReach & 2) && Now - RightDownAt > 0.15f)
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
	// Steer each reaching hand to the person you're holding (or straight out where you're looking:
	// move the mouse and your arms swing round with it - swing them into people to hit them, like Gang Beasts).
	const ABRCharacter* Held = HandGrabTarget.Get();
	FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	if (!bIsNPC)
	{
		const FVector AimFwd = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f).Vector();
		// Arms can swing up to ~100 degrees either side of where your body faces.
		if (FVector::DotProduct(AimFwd, Forward) > -0.2f)
		{
			Forward = AimFwd;
		}
	}
	const FName HandBones[2] = { FName(TEXT("hand_l")), FName(TEXT("hand_r")) };
	for (int32 i = 0; i < 2; ++i)
	{
		if (!(ArmsReaching & (1 << i)))
		{
			continue;
		}
		const float Side = i == 0 ? -1.f : 1.f;
		// Zombie arms: straight out at shoulder height, and up/down with where you look (look up = arms up).
		// Gorilla style: arms out in front, a bit low and wide, so swinging the mouse sweeps them through whoever's in front of you.
		const float Pitch = FMath::DegreesToRadians(FMath::Clamp(FRotator::NormalizeAxis(GetBaseAimRotation().Pitch) - 5.f, -40.f, 75.f));
		FVector Point = GetActorLocation() + Forward * (90.f * FMath::Cos(Pitch) + 15.f) + GetActorRightVector() * Side * 30.f
			+ FVector(0.f, 0.f, 45.f + 90.f * FMath::Sin(Pitch));
		if (Held)
		{
			Point = Held->GetActorLocation() + Held->GetActorRightVector() * Side * 14.f + FVector(0.f, 0.f, 40.f);
		}
		else if (!HeldProp)
		{
			// Gang Beasts sticky hands: a reaching hand that touches anything (wall, car, fence, crate...) sticks to it
			// for as long as you hold the button, until you walk too far away for the arm to stay on.
			const FName ShoulderBone = i == 0 ? FName(TEXT("upperarm_l")) : FName(TEXT("upperarm_r"));
			const FVector Shoulder = Body->GetBoneIndex(ShoulderBone) != INDEX_NONE ? Body->GetBoneLocation(ShoulderBone)
				: GetActorLocation() + GetActorRightVector() * Side * 20.f + FVector(0.f, 0.f, 50.f);
			if (!bHandAnchored[i])
			{
				FHitResult Touch;
				FCollisionQueryParams TouchParams(SCENE_QUERY_STAT(BRHandStick), false, this);
				const FVector ReachDir = (Point - Shoulder).GetSafeNormal();
				if (GetWorld()->LineTraceSingleByChannel(Touch, Shoulder, Point + ReachDir * 12.f, ECC_Visibility, TouchParams) && !Cast<APawn>(Touch.GetActor())
					&& !(Touch.GetComponent() && Touch.GetComponent()->GetName().StartsWith(TEXT("Edge"))) // the map's outer walls are too smooth to grab
					&& !(Touch.GetActor() && Touch.GetActor()->ActorHasTag(TEXT("NoGrab"))))
				{
					bHandAnchored[i] = true;
					HandAnchor[i] = Touch.ImpactPoint + Touch.ImpactNormal * 6.f;
				}
			}
			if (bHandAnchored[i])
			{
				if (FVector::Dist(Shoulder, HandAnchor[i]) > 115.f)
				{
					bHandAnchored[i] = false; // arm can't stretch that far: it slips off
				}
				else
				{
					Point = HandAnchor[i];
				}
			}
		}
		const FVector Hand = Body->GetBoneLocation(HandBones[i]);
		// Fast enough that a quick mouse swing whips the arms round (that's what hurts), heavy enough to lag a little.
		Body->SetPhysicsLinearVelocity(((Point - Hand) * 10.f).GetClampedToMaxSize(ArmSwingMaxSpeed) + GetVelocity() * 0.5f, false, HandBones[i]);
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

void ABRCharacter::PlayBodySound(const TArray<TSoftObjectPtr<USoundBase>>& List, const FVector& Where, float Volume, float Range)
{
	FVector Ear;
	if (BRAudio::GetListener(this, Ear) && FVector::DistSquared(Ear, Where) > FMath::Square(Range))
	{
		return; // too far to hear: don't even load/pick one
	}
	if (USoundBase* Sound = UBRSettings::PickSound(List))
	{
		BRAudio::PlayAt(this, Sound, Where, Range, FMath::Clamp(Volume, 0.05f, 1.5f), FMath::FRandRange(0.92f, 1.08f));
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
	// Footsteps: a step sound the moment each foot comes down on the ground (not on a timer).
	const float Speed = GetVelocity().Size2D();
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	const bool bWalking = GetCondition() == EPVVCondition::Normal && Move && Move->IsMovingOnGround() && Speed > 60.f && !GetCurrentBoat() && !GetCarrier();
	const float Range = FootstepRange * (bIsNPC ? 0.8f : 1.f);
	const float Vol = FootstepVolume * (bIsCrouched ? 0.5f : 1.f) * (bIsNPC ? 0.7f : 1.f) * FMath::Clamp(Speed / 450.f, 0.6f, 1.3f);
	const float FeetZ = GetActorLocation().Z - Half;
	USkeletalMeshComponent* Body = GetMesh();
	static const FName FootBones[2] = { FName(TEXT("foot_l")), FName(TEXT("foot_r")) };
	const bool bHasFeet = Body && Body->GetBoneIndex(FootBones[0]) != INDEX_NONE && Body->GetBoneIndex(FootBones[1]) != INDEX_NONE;
	FVector Ear;
	const bool bListener = BRAudio::GetListener(this, Ear);
	const float EarDistSq = bListener ? FVector::DistSquared(Ear, GetActorLocation()) : MAX_flt;
	const bool bInEarshot = EarDistSq < FMath::Square(Range);
	const bool bInSight = EarDistSq < FMath::Square(3000.f); // close enough to see the head bob
	if (bWalking && bHasFeet && bInSight)
	{
		for (int32 i = 0; i < 2; ++i)
		{
			const FVector Foot = Body->GetBoneLocation(FootBones[i]);
			const float Height = Foot.Z - FeetZ;
			// Learn where this foot sits when it's planted (slowly forgets, so slopes/stairs are fine).
			FootFloor[i] = FMath::Min(FootFloor[i] + DeltaSeconds * 6.f, Height);
			if (Height > FootFloor[i] + 5.f)
			{
				bFootUp[i] = true; // lifted
			}
			else if (bFootUp[i] && Height < FootFloor[i] + 2.f && Now - LastStepTime[i] > 0.18f)
			{
				bFootUp[i] = false; // ...and it just touched down: STEP
				LastStepTime[i] = Now;
				if (bInEarshot)
				{
					PlayBodySound(Sounds->FootstepSounds, FVector(Foot.X, Foot.Y, FeetZ), Vol, Range);
				}
				// Gang Beasts waddle: the head and shoulders jiggle on every step (toward the foot that landed).
				if (bWobbleOn && StepBob > 0.f && Body->GetBoneIndex(TEXT("head")) != INDEX_NONE && Body->IsSimulatingPhysics(TEXT("head")))
				{
					const FVector Side = GetActorRightVector() * (i == 0 ? -1.f : 1.f);
					Body->AddImpulse(FVector(0.f, 0.f, -StepBob) + Side * StepBob * 0.5f, TEXT("head"), true);
					if (Body->GetBoneIndex(TEXT("spine_03")) != INDEX_NONE)
					{
						Body->AddImpulse(Side * StepBob * 0.35f, TEXT("spine_03"), true);
					}
				}
			}
		}
	}
	else if (bWalking && !bHasFeet && bInEarshot)
	{
		// No foot bones on this mesh: fall back to stepping in time with the stride.
		StepClock -= DeltaSeconds;
		if (StepClock <= 0.f)
		{
			StepClock = FMath::Clamp(165.f / Speed, 0.25f, 0.7f);
			PlayBodySound(Sounds->FootstepSounds, GetActorLocation() - FVector(0.f, 0.f, Half), Vol, Range);
		}
	}
	else
	{
		StepClock = 0.05f;
		bFootUp[0] = bFootUp[1] = false;
		FootFloor[0] = FootFloor[1] = 1000.f;
	}
	// Ragdoll tumbling: thump when the body slams into the ground.
	if (GetCondition() == EPVVCondition::KnockedDown && GetMesh() && GetMesh()->GetBoneIndex(TEXT("pelvis")) != INDEX_NONE)
	{
		const float Vz = GetMesh()->GetPhysicsLinearVelocity(TEXT("pelvis")).Z;
		if (PelvisFallSpeed < -350.f && Vz > -80.f && Now - LastThumpTime > 0.25f)
		{
			LastThumpTime = Now;
			PlayBodySound(Sounds->BodyThumpSounds, GetMesh()->GetBoneLocation(TEXT("pelvis")), FMath::Clamp(-PelvisFallSpeed / 900.f, 0.3f, 1.2f), BRAudio::Near);
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
		PlayBodySound(Sounds->FlyingYellSounds, GetActorLocation(), 1.f, BRAudio::Medium);
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
	return bHiddenInSpot || (bInBox && GetVelocity().Size2D() < 30.f) || ABRSmokeCloud::IsInSmoke(GetWorld(), GetActorLocation())
		|| ABRDigTunnel::IsUnderground(GetWorld(), GetActorLocation());
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
	// Can't climb into the box while zip-tied, carried, knocked out, in a vehicle or held.
	if (!bInBox && (GetCondition() != EPVVCondition::Normal || GetCarrier() || GetCurrentBoat() || GrabbedBy || GetCarriedCharacter()))
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
	FVector RopeEnd = bHit ? FVector(Hit.ImpactPoint) : End;
	if (!Victim)
	{
		// A little aim help: whoever's closest to where you threw it (within a narrow cone).
		float BestDot = FMath::Cos(FMath::DegreesToRadians(10.f));
		for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
		{
			const FVector To = It->GetActorLocation() - Start;
			const float Dot = FVector::DotProduct(To.GetSafeNormal(), Aim);
			if (*It != this && To.Size() < 1800.f && Dot > BestDot && (!bHit || To.Size() < Hit.Distance + 100.f))
			{
				BestDot = Dot;
				Victim = *It;
				RopeEnd = It->GetActorLocation();
			}
		}
	}
	Multicast_LassoRope(RopeEnd);
	if (!Victim || Victim->GetCarrier() || Victim->GetCurrentBoat() || Victim->IsHidingInSpot() || Victim->GetCondition() == EPVVCondition::Detained)
	{
		return;
	}
	// YEEHAW: yank them off their feet toward you (and off walls / ladders).
	Victim->StopClimbing(true);
	Victim->BlowDisguise();
	const FVector Yank = (GetActorLocation() - Victim->GetActorLocation()).GetSafeNormal2D() * 900.f + FVector(0.f, 0.f, 300.f);
	Victim->ApplyKnockdown(this, Yank);
	ABRGameState::Highlight(this, Victim, 55.f, FString::Printf(TEXT("%s LASSOED %s"), *GetPlayerDisplayName(), *Victim->GetPlayerDisplayName()));
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

void ABRCharacter::TickSprintStamina(float DeltaSeconds)
{
	if (bIsNPC || (!HasAuthority() && !IsLocallyControlled()))
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	const bool bRunning = IsSprinting() && !bWinded && Move && Move->IsMovingOnGround() && GetVelocity().Size2D() > 250.f;
	if (bRunning)
	{
		SprintStamina = FMath::Max(0.f, SprintStamina - DeltaSeconds / FMath::Max(1.f, SprintSeconds));
		LastSprintDrainTime = Now;
		if (SprintStamina <= 0.f)
		{
			bWinded = true;
			UpdateMovementSpeed();
		}
	}
	else if (Now - LastSprintDrainTime > 0.8f)
	{
		SprintStamina = FMath::Min(1.f, SprintStamina + DeltaSeconds / FMath::Max(1.f, SprintRefillSeconds));
	}
	if (bWinded && SprintStamina >= 0.3f)
	{
		bWinded = false; // got your breath back
		UpdateMovementSpeed();
	}
}

bool ABRCharacter::FailsPapersScan() const
{
	if (bIsNPC)
	{
		return bIllegal;
	}
	if (!IsRunner())
	{
		return false;
	}
	// A fake passport fools the scanner - unless you're carrying contraband (the scanner's sniffer catches it).
	const ABRPlayerState* PS = GetBRPlayerState();
	const bool bPassport = PS && PS->HasItem(EBRItem::FakePassport);
	const bool bDirty = PS && PS->Inventory.ContainsByPredicate([](EBRItem I) { return BR::IsIllegal(I); });
	return !bPassport || bDirty;
}

void ABRCharacter::OnPrisonerStateChanged(EPVVCondition Was, EPVVCondition Now)
{
	// Zip-tied: everything illegal on you is taken (the nearest agent gets the credit).
	if (Now == EPVVCondition::Restrained && Was != EPVVCondition::Restrained)
	{
		const int32 Taken = Confiscate();
		if (Taken > 0)
		{
			ABRCharacter* Cop = nullptr;
			float Best = 500.f;
			for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
			{
				const float D = FVector::Dist(It->GetActorLocation(), GetActorLocation());
				if (It->IsWarden() && !It->IsNPC() && D < Best)
				{
					Best = D;
					Cop = *It;
				}
			}
			if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>(); GM && Cop)
			{
				GM->OnContrabandSeized(Cop, Taken);
				Cop->Notify(FText::Format(LOCTEXT("CuffSeized", "Zip-tied and searched: {0} thing(s) seized."), Taken));
			}
		}
		NPCBreakFreeAt = GetWorld()->GetTimeSeconds() + FMath::FRandRange(60.f, 240.f);
	}
	// Untied (or broken out of the pen) by a Runner: an NPC follows whoever freed them, like a smuggling client.
	if (bIsNPC && !bOfficer && Now == EPVVCondition::Normal && (Was == EPVVCondition::Restrained || Was == EPVVCondition::Detained) && !Leader.IsValid())
	{
		ABRCharacter* Rescuer = nullptr;
		float Best = 600.f;
		for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
		{
			const float D = FVector::Dist(It->GetActorLocation(), GetActorLocation());
			if (It->IsRunner() && !It->IsNPC() && It->GetCondition() == EPVVCondition::Normal && D < Best)
			{
				Best = D;
				Rescuer = *It;
			}
		}
		if (Rescuer)
		{
			CalmDown();
			StartFollowing(Rescuer, 120);
			Rescuer->Notify(LOCTEXT("FreedFollows", "You freed them - they're following you now. Get them to the Safe Zone (+$120)."));
		}
		else
		{
			StartFleeing(20.f); // broke free on their own: leg it
		}
	}
}

void ABRCharacter::TickNPCStruggle()
{
	// Zip-tied NPCs very rarely manage to wriggle out (once every few minutes at most, and only if left alone).
	if (!bIsNPC || GetCondition() != EPVVCondition::Restrained || NPCBreakFreeAt <= 0.f)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now >= NPCBreakFreeAt)
	{
		if (FMath::FRand() < 0.35f)
		{
			NPCStruggleUntil = Now + 3.f; // a real attempt
		}
		NPCBreakFreeAt = Now + FMath::FRandRange(90.f, 240.f);
	}
	if (Now < NPCStruggleUntil)
	{
		BotStruggle();
	}
}

void ABRCharacter::BotDive()
{
	Server_Dive_Implementation();
}

void ABRCharacter::BrawlerFinish(ABRCharacter* Foe)
{
	if (!HasAuthority() || !Foe || GetCondition() != EPVVCondition::Normal || GetCarrier() || GetCurrentBoat())
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	const FVector ToFoe = (Foe->GetActorLocation() - GetActorLocation()) * FVector(1.f, 1.f, 0.f);
	if (HandGrabTarget.Get() == Foe)
	{
		ArmsReaching = 3;
		if (bLiftOverhead)
		{
			if (Now >= BrawlHoldUntil)
			{
				Server_Grab_Implementation(); // HURL them
				AngryAt = nullptr;
				FleeUntil = Now + 20.f; // and run while they're down
			}
			return;
		}
		if (Now >= BrawlHoldUntil && HandGrabMask == 3)
		{
			Server_Lift_Implementation(); // up over the head...
			BrawlHoldUntil = Now + LiftSeconds + FMath::FRandRange(0.4f, 1.2f);
			return;
		}
		// ...dragging them a couple of steps first.
		AddMovementInput(-ToFoe.GetSafeNormal(), 0.6f);
		return;
	}
	// Walk up and grab the body.
	GetCharacterMovement()->MaxWalkSpeed = FleeSpeed;
	if (ToFoe.Size() > 110.f)
	{
		AddMovementInput(ToFoe.GetSafeNormal(), 1.f);
		return;
	}
	SetActorRotation(FRotator(0.f, ToFoe.Rotation().Yaw, 0.f));
	if (Now >= BrawlNextMove)
	{
		BrawlNextMove = Now + 0.4f;
		ArmsReaching = 3;
		if (TryHandGrab(3))
		{
			BrawlHoldUntil = Now + FMath::FRandRange(0.8f, 1.8f);
		}
		else
		{
			ArmsReaching = 0;
		}
	}
}

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
	float Pace = FMath::Lerp(1.f, 0.35f, FMath::Clamp(Aggression, 0.f, 1.f));
	if (Foe->IsDazed())
	{
		Pace *= 0.45f; // they're seeing birds: finish them
	}
	BrawlNextMove = Now + Pace * FMath::FRandRange(0.7f, 1.3f);
	// Hurt: back off with the guard up for a moment (then come back swinging).
	if (GetBalanceFraction() < 0.3f && !Foe->IsDazed() && Dist < 350.f && FMath::FRand() < 0.45f)
	{
		BotBlock(FMath::FRandRange(0.6f, 1.1f));
		LaunchCharacter(-ToFoe.GetSafeNormal() * 350.f, true, false);
		BrawlNextMove = Now + 0.8f;
		return;
	}
	// Mid range: circle to the side instead of walking straight into punches.
	if (Dist > 180.f && Dist < 330.f && FMath::FRand() < 0.35f)
	{
		const FVector Side = FVector::CrossProduct(ToFoe.GetSafeNormal(), FVector::UpVector) * (FMath::RandBool() ? 1.f : -1.f);
		LaunchCharacter(Side * 260.f + ToFoe.GetSafeNormal() * 120.f, true, false);
	}
	if (Foe->GetCondition() != EPVVCondition::Normal)
	{
		return; // they're down: let them get up (the crowd loves it)
	}
	SetActorRotation(FRotator(0.f, ToFoe.Rotation().Yaw, 0.f));
	if (bBlocking)
	{
		return; // guard's up for a moment
	}
	// Up close and they're swinging (or they're blocking us): put the guard up now and then.
	const ABRCharacter* BRFoe = Foe;
	if (Dist < 230.f && HandGrabTarget.Get() != Foe && FMath::FRand() < 0.14f * (0.5f + Aggression) * (BRFoe->IsBlocking() ? 0.3f : 1.f))
	{
		BotBlock(FMath::FRandRange(0.5f, 1.2f));
		return;
	}
	// They're blocking: grab them or kick them (blocks only stop hits from the front).
	if (BRFoe->IsBlocking() && Dist < 175.f && FMath::FRand() < 0.5f)
	{
		if (FMath::FRand() < 0.5f)
		{
			ArmsReaching = 3;
			if (!TryHandGrab(3))
			{
				ArmsReaching = 0;
			}
		}
		else
		{
			DoKick();
		}
		return;
	}
	// Got them in your hands: beat on them, then THROW them.
	if (HandGrabTarget.Get() == Foe)
	{
		// They're seeing birds and we've got both hands on them: UP over the head (then hurl - see BrawlerFinish).
		if (HandGrabMask == 3 && Foe->IsDazed() && !bLiftOverhead && Aggression >= 0.7f)
		{
			Server_Lift_Implementation();
			BrawlHoldUntil = Now + LiftSeconds + FMath::FRandRange(0.4f, 1.f);
			return;
		}
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
	if (HasAuthority() && GetCondition() == EPVVCondition::Normal && GetCharacterMovement()->IsFalling() && !bClimbing && !GetCurrentBoat() && !GetCarrier())
	{
		DoDropKick(); // kick in the air: DROP KICK
		return;
	}
	if (!HasAuthority() || GetCondition() != EPVVCondition::Normal || GetCarrier() || GetCurrentBoat() || bClimbing || bInCannon || Now - LastKickTime < 0.9f)
	{
		return;
	}
	LastKickTime = Now;
	BlowDisguise();
	Multicast_Kick();
	// Aim the foot at the nearest person's LEG in front of you (Gang Beasts: kick their legs out from under them).
	if (bWobbleOn)
	{
		const FVector Fwd = GetActorForwardVector().GetSafeNormal2D();
		APVVCharacter* Best = nullptr;
		float BestD = 190.f;
		for (TActorIterator<APVVCharacter> It(GetWorld()); It; ++It)
		{
			APVVCharacter* Other = *It;
			const FVector To = (Other->GetActorLocation() - GetActorLocation()) * FVector(1.f, 1.f, 0.f);
			const float D = To.Size();
			if (Other != this && D < BestD && Other->GetCondition() == EPVVCondition::Normal && FVector::DotProduct(To.GetSafeNormal(), Fwd) > 0.4f)
			{
				BestD = D;
				Best = Other;
			}
		}
		FVector TargetLocal(100.f, 12.f, -50.f); // shin height out in front
		if (Best)
		{
			const USkeletalMeshComponent* TheirBody = Best->GetMesh();
			const FName Shin = TheirBody && TheirBody->GetBoneIndex(TEXT("calf_l")) != INDEX_NONE ? FName(TEXT("calf_l")) : NAME_None;
			const FVector Aim = Shin != NAME_None ? TheirBody->GetBoneLocation(Shin) : Best->GetActorLocation() - FVector(0.f, 0.f, 50.f);
			TargetLocal = GetActorTransform().InverseTransformPositionNoScale(Aim);
			TargetLocal.X = FMath::Clamp(TargetLocal.X + 25.f, 60.f, 140.f); // kick THROUGH the leg
			TargetLocal.Y = FMath::Clamp(TargetLocal.Y, -40.f, 50.f);
			TargetLocal.Z = FMath::Clamp(TargetLocal.Z, -80.f, 10.f);
		}
		Multicast_AttackSteer(2, TargetLocal, KickExtendSeconds);
		KickHitUntil = Now + KickExtendSeconds + 0.12f;
		bFootTracked = false;
	}
	// Props get punted (and bodies without physics still use the simple kick).
	GetWorldTimerManager().SetTimer(MoveHitTimer, this, &ABRCharacter::KickHit, 0.18f, false);
}

void ABRCharacter::TickKickContact(float DeltaSeconds)
{
	// The kick only counts if the FOOT actually lands on them.
	USkeletalMeshComponent* Body = GetMesh();
	const float Now = GetWorld()->GetTimeSeconds();
	if (!HasAuthority() || Now > KickHitUntil || !Body || !bWobbleOn || Body->GetBoneIndex(TEXT("foot_r")) == INDEX_NONE || DeltaSeconds <= 0.f
		|| GetCondition() != EPVVCondition::Normal)
	{
		bFootTracked = false;
		return;
	}
	const FVector Foot = Body->GetBoneLocation(TEXT("foot_r"));
	if (!bFootTracked)
	{
		LastFootPos = Foot;
		bFootTracked = true;
		return;
	}
	const FVector FootVel = (Foot - LastFootPos) / DeltaSeconds - GetVelocity() * 0.5f;
	LastFootPos = Foot;
	TArray<FOverlapResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BRKickFoot), false, this);
	GetWorld()->OverlapMultiByObjectType(Hits, Foot, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(30.f), Params);
	for (const FOverlapResult& H : Hits)
	{
		APVVCharacter* Victim = Cast<APVVCharacter>(H.GetActor());
		if (!Victim || Victim == this || Victim->GetCarrier() || Victim->GetCondition() != EPVVCondition::Normal)
		{
			continue;
		}
		const USkeletalMeshComponent* TheirBody = Victim->GetMesh();
		FName HitBone = TEXT("pelvis");
		float BestD = 1.e9f;
		if (TheirBody)
		{
			for (const TCHAR* BoneName : { TEXT("calf_l"), TEXT("calf_r"), TEXT("thigh_l"), TEXT("thigh_r"), TEXT("foot_l"), TEXT("foot_r"), TEXT("pelvis"), TEXT("spine_02"), TEXT("spine_03"), TEXT("head") })
			{
				if (TheirBody->GetBoneIndex(BoneName) != INDEX_NONE)
				{
					const float D = FVector::Dist(TheirBody->GetBoneLocation(BoneName), Foot);
					if (D < BestD)
					{
						BestD = D;
						HitBone = BoneName;
					}
				}
			}
		}
		const FString BoneStr = HitBone.ToString();
		const bool bLeg = BoneStr.StartsWith(TEXT("calf")) || BoneStr.StartsWith(TEXT("thigh")) || BoneStr.StartsWith(TEXT("foot"));
		const bool bHead = HitBone == FName(TEXT("head"));
		const float Power = FMath::Clamp((FootVel.Size() - SlapSpeed) / FMath::Max(100.f, HardHitSpeed * 1.1f - SlapSpeed), 0.f, 1.f);
		const float Curve = Power * Power;
		const FVector Dir = FootVel.GetSafeNormal2D().IsNearlyZero() ? GetActorForwardVector().GetSafeNormal2D() : FootVel.GetSafeNormal2D();
		ABRCharacter* BRVictim = Cast<ABRCharacter>(Victim);
		KickHitUntil = -1.f;
		if (bLeg)
		{
			// Leg kick: hurts a little, and if they're moving (or it's a hard one) it sweeps their legs: TRIPPED.
			Victim->ReceiveMeleeHit(this, KickDamage * FMath::Lerp(0.25f, 0.6f, Curve), Dir * 120.f);
			const bool bTrip = Victim->GetVelocity().Size2D() > 180.f || Power > 0.65f;
			if (bTrip)
			{
				Victim->ApplyKnockdown(this, Dir * 250.f + FVector(0.f, 0.f, 120.f));
				ABRGameState::FX(this, EBRFX::PointsText, Victim->GetActorLocation() + FVector(0.f, 0.f, 90.f), TEXT("TRIPPED!"));
			}
			else if (BRVictim)
			{
				BRVictim->ApplySlow(1.f, 0.5f); // dead leg: limping for a second
			}
		}
		else
		{
			// Body / head kick: a big shove (and a head kick rattles them like a punch).
			Victim->ReceiveMeleeHit(this, KickDamage * FMath::Lerp(0.4f, 1.1f, Curve) * (bHead ? 1.4f : 1.f), Dir * KickPush * FMath::Lerp(0.4f, 1.f, Curve) + FVector(0.f, 0.f, 200.f));
			if (BRVictim && !BRVictim->bBlocking)
			{
				BRVictim->AddKO(bHead ? BRVictim->HeadHitKO * FMath::Lerp(0.5f, 2.2f, Curve) : BRVictim->BodyHitKO * FMath::Lerp(0.3f, 1.5f, Curve), this);
			}
		}
		if (BRVictim)
		{
			BRVictim->Multicast_BodyHit(HitBone, Dir * HitReaction * FMath::Lerp(0.5f, 1.6f, Curve));
		}
		Body->AddImpulse(-Dir * 400.f, TEXT("foot_r"), true); // the foot stops on them
		Multicast_Sfx(3);
		return;
	}
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
	if (!bWobbleOn)
	{
		PlayOneShot(KickAnim); // no physics body: the animation is the kick
	}
	else if (GetMesh() && GetMesh()->GetBoneIndex(TEXT("thigh_r")) != INDEX_NONE)
	{
		// Physics kick: the leg goes loose and is driven out and up through the kick, then eases back.
		FPhysicalAnimationData Loose;
		Loose.bIsLocalSimulation = false;
		Loose.OrientationStrength = 60.f;
		Loose.AngularVelocityStrength = 6.f;
		PhysicalAnimation->ApplyPhysicalAnimationSettingsBelow(TEXT("thigh_r"), Loose, true);
		const float NowK = GetWorld()->GetTimeSeconds();
		PunchArmRestoreAt = FMath::Max(PunchArmRestoreAt, NowK + KickExtendSeconds + 0.25f);
		SteerTargetLocal[2] = FVector(95.f, 12.f, -15.f); // out in front, about waist high
		SteerStart[2] = NowK;
		SteerUntil[2] = NowK + KickExtendSeconds;
	}
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
		if (!Victim || Victim->GetCarrier() || Victim->GetCondition() == EPVVCondition::Detained || bWobbleOn)
		{
			continue; // (wobbly bodies: the foot has to actually land - see TickKickContact)
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
		if (!Victim->bBlocking)
		{
			Victim->AddKO(Victim->HeadHitKO * 1.4f, this); // head-to-head: the best way to knock someone out
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
	ClimbGrip = 1.f;
	ClimbStamina = 1.f;
	bWarnedClimbTired = false;
	// Bots just climb. Players started with Z (no mouse buttons held): both hands go on the wall automatically.
	bClimbUpHeld = !Cast<APlayerController>(GetController());
	if (ArmsReaching == 0)
	{
		ArmsReaching = 3;
		WallHandMask = 3;
	}
	if (HandGrabMask == 3)
	{
		ReleaseHandGrab(); // both hands were on a person: let them go to grab the wall
	}
	GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	GetCharacterMovement()->StopMovementImmediately();
	SetActorLocation(FVector(Wall.ImpactPoint.X, Wall.ImpactPoint.Y, GetActorLocation().Z) + Normal * (GetCapsuleComponent()->GetScaledCapsuleRadius() + 5.f));
	SetActorRotation(FRotator(0.f, (-Normal).Rotation().Yaw, 0.f));
	OnRep_Climbing();
	BlowDisguise();
	Notify(LOCTEXT("ClimbStart", "ON THE WALL! Hold BOTH mouse buttons to hang on, hold SPACE to climb (look up/down), A/D to shimmy. Let go to drop."));
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
	// Optional: clicking hand over hand while climbing gives a little extra boost (no penalty for getting it "wrong").
	if (!bClimbing || bHanging || bLeft == bLastClimbLeft || ClimbStamina <= 0.1f)
	{
		return;
	}
	bLastClimbLeft = bLeft;
	LastClimbStepTime = GetWorld()->GetTimeSeconds();
	ClimbStamina -= 0.015f;
	FHitResult Blocked;
	SetActorLocation(GetActorLocation() + FVector(0.f, 0.f, ClimbStep * 0.4f), true, &Blocked);
	ForceNetUpdate();
}

void ABRCharacter::Server_ClimbHold_Implementation(bool bHeld)
{
	bClimbUpHeld = bHeld;
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
		if (ABRGameMode* AlarmGM = GetWorld()->GetAuthGameMode<ABRGameMode>(); AlarmGM && !bIsNPC)
		{
			AlarmGM->TriggerBorderAlarm(GetActorLocation(), TEXT("Someone is climbing the Wall"));
		}
	}
	// Gang Beasts climbing: your hands hold you up. Both hands = you hang; one hand = you slowly slide.
	const int32 Grip = (ArmsReaching & 1) + ((ArmsReaching >> 1) & 1);
	if (Grip == 0)
	{
		StopClimbing(true); // let go of everything: drop
		return;
	}
	const FVector Wall = -FVector(ClimbNormal);
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Wall).GetSafeNormal();
	float Vz = 0.f;
	if (!bHanging)
	{
		if (bClimbUpHeld && ClimbStamina > 0.f)
		{
			// Hold SPACE: climb hand over hand. Look down to climb down instead.
			const float Pitch = FRotator::NormalizeAxis(GetBaseAimRotation().Pitch);
			const float Dir = Pitch < -25.f ? -1.f : 1.f;
			Vz = Dir * ClimbSpeed * (Grip == 2 ? 1.f : 0.45f) * FMath::Lerp(0.45f, 1.f, ClimbStamina);
		}
		else if (Grip == 1)
		{
			Vz = -ClimbSlideSpeed;
		}
	}
	// A / D: shimmy sideways along the wall.
	const FVector Wish = GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D();
	const float Side = bHanging ? 0.f : FMath::Clamp(FVector::DotProduct(Wish, Right) * 1.4f, -1.f, 1.f);
	const FVector Before = GetActorLocation();
	const FVector Delta = (FVector::UpVector * Vz + Right * Side * ShimmySpeed * (Grip == 2 ? 1.f : 0.5f)) * DeltaSeconds;
	if (!Delta.IsNearlyZero())
	{
		FHitResult Blocked;
		SetActorLocation(Before + Delta, true, &Blocked);
		// Shimmied off the end of the wall? Step back on.
		FHitResult Still;
		FCollisionQueryParams WallParams(SCENE_QUERY_STAT(BRClimbWall), false, this);
		if (!GetWorld()->LineTraceSingleByChannel(Still, GetActorLocation() + FVector(0.f, 0.f, 40.f), GetActorLocation() + FVector(0.f, 0.f, 40.f) + Wall * (GetCapsuleComponent()->GetScaledCapsuleRadius() + 45.f), ECC_Visibility, WallParams))
		{
			SetActorLocation(Before, false);
		}
		// Hands swap as you go (the arms reach up one after the other).
		if (Now >= ClimbHandSwapAt)
		{
			ClimbHandSwapAt = Now + FMath::Clamp(30.f / FMath::Max(10.f, FMath::Abs(Vz) + FMath::Abs(Side) * 60.f), 0.22f, 0.5f);
			bLastClimbLeft = !bLastClimbLeft;
		}
		LastClimbStepTime = Now;
	}
	// Reached the top: hang from the ledge (SPACE = pull up).
	const float HandsZ = GetActorLocation().Z + 90.f;
	if (!bHanging && bClimbHasTop && Vz > 0.f && HandsZ >= ClimbTopZ)
	{
		bHanging = true;
		Notify(LOCTEXT("ClimbTop", "At the top! SPACE to pull yourself up."));
	}
	if (bHanging && !Cast<APlayerController>(GetController()))
	{
		Server_ClimbJump_Implementation(); // NPCs / bots pull themselves over
		return;
	}
	// Climbed/slid back down to the ground: just stand up.
	if (Vz < 0.f)
	{
		FHitResult Floor;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BRClimbSlide), false, this);
		const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		if (GetWorld()->LineTraceSingleByChannel(Floor, GetActorLocation(), GetActorLocation() - FVector(0.f, 0.f, Half + 15.f), ECC_Visibility, Params))
		{
			StopClimbing(false);
			return;
		}
	}
	// Arms get tired (climbing tires you faster than just hanging; one hand faster still).
	const float Drain = (Vz > 0.f ? 1.4f : 1.f) * (Grip == 2 ? 1.f : 1.6f) / FMath::Max(3.f, ClimbStaminaSeconds);
	ClimbStamina -= Drain * DeltaSeconds;
	if (ClimbStamina < 0.25f && !bWarnedClimbTired)
	{
		bWarnedClimbTired = true;
		Notify(LOCTEXT("ClimbTired", "Your arms are shaking... get up or let go!"));
	}
	if (ClimbStamina <= 0.f)
	{
		Notify(LOCTEXT("ClimbFell", "Your arms gave out!"));
		StopClimbing(true);
		return;
	}
	GetCharacterMovement()->Velocity = FVector::ZeroVector;
}

// -------------------------------------------------------------------------------------------
// Hands: any fast-moving hand hits whoever it touches (Gang Beasts)

void ABRCharacter::TickHands(float DeltaSeconds)
{
	USkeletalMeshComponent* Body = GetMesh();
	// NPCs only swing for real when they're actually in a fight (no accidental brawls from bumping into each other).
	const bool bCalmNPC = bIsNPC && !bNoFightBack && !IsBrawling();
	if (!Body || bPeaceful || bBlocking || bCalmNPC || GetCondition() != EPVVCondition::Normal || GetCarrier() || GetCurrentBoat() || bInCannon || bOnShoulders
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
		const bool bGripping = (HandGrabMask & (1 << i)) || (WallHandMask & (1 << i)) || bClimbing || (bPropInHands && HeldProp);
		if ((ArmsReaching & (1 << i)) && !bPunching && (!bSwingAttacks || bGripping))
		{
			continue; // a hand that's holding something doesn't slap (a swinging empty hand does)
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
				|| (Cast<ABRCharacter>(Victim) && Cast<ABRCharacter>(Victim)->bPeaceful))
			{
				continue; // (the person you're holding CAN be hit - by your free hand, which has to actually land)
			}
			// Which part of them did the hand actually land on? (closest bone to the fist)
			FName HitBone = TEXT("spine_03");
			float BestBoneDist = 1.e9f;
			const USkeletalMeshComponent* VictimBody = Victim->GetMesh();
			if (VictimBody)
			{
				for (const TCHAR* BoneName : { TEXT("head"), TEXT("neck_01"), TEXT("spine_03"), TEXT("spine_02"), TEXT("pelvis"), TEXT("upperarm_l"), TEXT("upperarm_r"),
					TEXT("lowerarm_l"), TEXT("lowerarm_r"), TEXT("thigh_l"), TEXT("thigh_r") })
				{
					if (VictimBody->GetBoneIndex(BoneName) != INDEX_NONE)
					{
						const float D = FVector::Dist(VictimBody->GetBoneLocation(BoneName), Pos);
						if (D < BestBoneDist)
						{
							BestBoneDist = D;
							HitBone = BoneName;
						}
					}
				}
			}
			const bool bHeadHit = HitBone == FName(TEXT("head")) || HitBone == FName(TEXT("neck_01"));
			const bool bArmHit = HitBone.ToString().Contains(TEXT("arm"));
			// How HARD was it? The speed the fist was travelling INTO them (a glancing brush barely counts), plus your weight behind it.
			const FVector HitPoint = VictimBody && VictimBody->GetBoneIndex(HitBone) != INDEX_NONE ? VictimBody->GetBoneLocation(HitBone) : Victim->GetActorLocation();
			const FVector Into = (HitPoint - Pos).IsNearlyZero() ? HandVel.GetSafeNormal() : (HitPoint - Pos).GetSafeNormal();
			const float Closing = FMath::Max(0.f, FVector::DotProduct(HandVel, Into));
			const float Effective = 0.35f * Speed + 0.65f * Closing + FMath::Max(0.f, FVector::DotProduct(GetVelocity(), Into)) * 0.3f;
			const float Power = FMath::Clamp((Effective - SlapSpeed) / FMath::Max(100.f, HardHitSpeed - SlapSpeed), 0.f, 1.f);
			const float Curve = Power * Power; // weak taps do very little, real swings do a lot
			if (Effective < SlapSpeed * 0.8f)
			{
				continue; // just a brush
			}
			float Damage = FMath::Lerp(HandHitMinDamage, HandHitMaxDamage, Curve) * (bPunching ? 1.15f : 1.f);
			if (HeldItem == EBRItem::Knife)
			{
				Damage *= 2.f; // stabby
			}
			Damage *= bHeadHit ? HeadHitMultiplier : (bArmHit ? 0.25f : BodyHitDamageScale); // head hurts, body shoves, arms mostly get blocked
			ABRCharacter* KOVictim = Cast<ABRCharacter>(Victim);
			const bool bGuarded = KOVictim && KOVictim->bBlocking;
			if (KOVictim && !bGuarded)
			{
				// Head: a clean, hard shot rattles you a LOT. Body: a few weak shots barely matter.
				const float KO = bHeadHit ? KOVictim->HeadHitKO * FMath::Lerp(0.25f, 2.4f, Curve)
					: (bArmHit ? 0.f : KOVictim->BodyHitKO * FMath::Lerp(0.2f, 1.4f, Curve));
				KOVictim->AddKO(KO, this);
			}
			const FVector Dir = HandVel.GetSafeNormal();
			const FVector Push = Dir.GetSafeNormal2D() * FMath::Lerp(90.f, bHeadHit ? 420.f : 600.f, Curve) + FVector(0.f, 0.f, 60.f + 120.f * Curve);
			Victim->ReceiveMeleeHit(this, Damage, Push);
			// A HARD clean shot to the head drops them on the spot (wobbly bodies fold).
			if (bHeadHit && !bGuarded && Power >= HeadKnockdownPower && Victim->GetCondition() == EPVVCondition::Normal)
			{
				Victim->ApplyKnockdown(this, Dir.GetSafeNormal2D() * 450.f + FVector(0.f, 0.f, 150.f));
				ABRGameState::FX(this, EBRFX::PointsText, HitPoint + FVector(0.f, 0.f, 40.f), TEXT("CRACK!"));
			}
			else if (Curve > 0.55f)
			{
				ABRGameState::FX(this, EBRFX::PointsText, HitPoint + FVector(0.f, 0.f, 40.f), bHeadHit ? TEXT("WHAP!") : TEXT("OOF!"));
			}
			if (ABRCharacter* Climber = Cast<ABRCharacter>(Victim); Climber && Climber->bClimbing)
			{
				// Hitting someone hanging off a wall/ledge: their grip gives (Gang Beasts: punch their hands!).
				Climber->ClimbGrip -= 0.35f;
				if (Climber->ClimbGrip <= 0.f)
				{
					Climber->Notify(LOCTEXT("PunchedOff", "They knocked you off!"));
					Climber->StopClimbing(true);
					Climber->LastTosser = this;
					Climber->LastTossTime = Now;
				}
			}
			if (KOVictim)
			{
				// The part that got hit visibly snaps back - a hard one to the head whips it right round.
				KOVictim->Multicast_BodyHit(HitBone, Dir * HitReaction * FMath::Lerp(0.3f, bHeadHit ? 2.1f : 1.5f, Curve));
			}
			// Your own arm recoils a bit off them (you can see it connect).
			Body->AddImpulse(-Dir * FMath::Lerp(150.f, 500.f, Curve), Hands[i], true);
			Multicast_Sfx(Curve > 0.3f ? 3 : 0);
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
	if (!HasAuthority())
	{
		AddLean(FVector(Impulse) * 0.6f, nullptr); // clients see the hit rock the whole body too
	}
	if (!bWobbleOn || !Body || Body->GetBoneIndex(Bone) == INDEX_NONE)
	{
		return;
	}
	if (FVector(Impulse).Size() >= HitFlopThreshold)
	{
		HitFlopUntil = GetWorld()->GetTimeSeconds() + HitFlopSeconds;
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
	BRMakeFollowerSafe(OutfitHead);
	OutfitHead->SetSkeletalMeshAsset(HatMesh);
	OutfitHead->SetLeaderPoseComponent(GetMesh(), true);
	OutfitHead->SetHiddenInGame(HatMesh == nullptr);
	UpdateHairUnderHat();
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
	return (WornHat == EBRHat::HardHat ? 0.6f : 1.f) * (IsTipsy() ? 0.7f : 1.f); // liquid courage
}

float ABRCharacter::GetTipsySecondsLeft() const
{
	const ABRGameState* GS = GetBRGameState();
	return GS ? FMath::Max(0.f, TipsyUntil - GS->GetServerWorldTimeSeconds()) : 0.f;
}

bool ABRCharacter::IsTipsy() const
{
	const ABRGameState* GS = GetBRGameState();
	return GS && TipsyUntil > GS->GetServerWorldTimeSeconds();
}

void ABRCharacter::MakeTipsy(float Seconds)
{
	const ABRGameState* GS = GetBRGameState();
	if (!HasAuthority() || !GS)
	{
		return;
	}
	const float Now = GS->GetServerWorldTimeSeconds();
	TipsyUntil = FMath::Min(FMath::Max(TipsyUntil, Now) + Seconds, Now + 45.f); // drinks stack a bit, but never more than 45 s
	ForceNetUpdate();
}

void ABRCharacter::Server_Chug_Implementation()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (!bChugging || Now < ChugGaggedUntil || Now - LastChugTime < 0.05f)
	{
		return;
	}
	// Rhythm, not spam: a gulp every ~0.15s is perfect. Faster than that and you start to GAG.
	const float Gap = LastChugTime < 0.f ? 1.f : Now - LastChugTime;
	LastChugTime = Now;
	if (Gap < 0.13f)
	{
		ChugGag = FMath::Min(1.f, ChugGag + 0.2f);
	}
	if (ChugGag >= 1.f)
	{
		// GAG! Spit it back up and lose a big gulp.
		ChugGag = 0.f;
		ChugMine = FMath::Max(0.f, ChugMine - 0.18f);
		ChugGaggedUntil = Now + 1.f;
		Notify(LOCTEXT("ChugGag", "*GAG* Too fast! Steady rhythm, amigo."));
		ForceNetUpdate();
		return;
	}
	// Drunker = sloppier gulps.
	ChugMine = FMath::Min(1.f, ChugMine + (IsTipsy() ? 0.016f : 0.021f));
}

bool ABRCharacter::MugPlayer(ABRCharacter* Victim)
{
	ABRPlayerState* VPS = Victim ? Victim->GetBRPlayerState() : nullptr;
	if (!HasAuthority() || !bIsNPC || !VPS || Victim->GetCondition() != EPVVCondition::Normal || StolenItem != EBRItem::None)
	{
		return false;
	}
	TArray<int32> Slots;
	for (int32 i = 0; i < VPS->Inventory.Num(); ++i)
	{
		if (VPS->Inventory[i] != EBRItem::None)
		{
			Slots.Add(i);
		}
	}
	if (Slots.Num() == 0)
	{
		return false;
	}
	EBRItem Taken = EBRItem::None;
	if (!VPS->RemoveItemAt(Slots[FMath::RandRange(0, Slots.Num() - 1)], Taken) || Taken == EBRItem::None)
	{
		return false;
	}
	StolenItem = Taken;
	StolenFrom = VPS;
	SetNPCHeldItem(Taken);
	// Shove them over and leg it (or stand and fight).
	SetActorRotation(FRotator(0.f, (Victim->GetActorLocation() - GetActorLocation()).Rotation().Yaw, 0.f));
	Victim->ApplyKnockdown(this, GetActorForwardVector() * 350.f + FVector(0.f, 0.f, 200.f));
	if (FMath::FRand() < 0.5f)
	{
		StartFleeing(40.f);
	}
	else
	{
		StartBrawling(Victim, 40.f, 0.75f);
	}
	Victim->Notify(FText::Format(LOCTEXT("Mugged", "A local MUGGED you and took your {0}! Beat them up to get it back."), FText::FromString(BR::ItemName(Taken))));
	return true;
}

void ABRCharacter::Server_SelectSlot_Implementation(int32 Slot)
{
	ServerSelectedSlot = Slot;
}

void ABRCharacter::BlowDisguise()
{
	if (HasAuthority() && bFakeUniform)
	{
		TakeOffFakeUniform(true); // fighting / caught out: the fake uniform fools nobody now
	}
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
	BRMakeFollowerSafe(DisguiseOutfit);
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
	if (bFakeUniform && IsSprinting())
	{
		for (TActorIterator<ABRCharacter> It(GetWorld()); It; ++It)
		{
			if (It->IsWarden() && !It->IsNPC() && FVector::Dist(It->GetActorLocation(), GetActorLocation()) < 1500.f)
			{
				TakeOffFakeUniform(true); // agents don't sprint around like that
				break;
			}
		}
	}
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
	case EBRItem::GasCan: case EBRItem::WireCutters: case EBRItem::Shovel: case EBRItem::Knife: break;
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
		case EBRItem::Knife: Scale = FVector(.3f, .02f, .05f); Color = FLinearColor(.85f, .87f, .9f); break;
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

void ABRCharacter::ForceEmote(int32 Index)
{
	if (HasAuthority() && Index >= 0 && Index < NumEmotes)
	{
		ActiveEmote = Index;
		OnRep_ActiveEmote();
		ForceNetUpdate();
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

void ABRCharacter::MakeLuchador(const FString& RingName, float Toughness)
{
	if (!HasAuthority())
	{
		return;
	}
	LuchaName = RingName;
	bNoFightBack = true; // the ring runs the fight
	// No AI controller: movement has to run anyway, or he just stands there.
	GetCharacterMovement()->bRunPhysicsWithNoController = true;
	GetCharacterMovement()->bOrientRotationToMovement = false; // he faces you while he circles
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

void ABRCharacter::Client_Banner_Implementation(const FString& Text, bool bGood)
{
	BannerText = Text;
	bBannerGood = bGood;
	BannerUntil = GetWorld()->GetTimeSeconds() + 5.f;
}

int32 ABRCharacter::FindClothesStyle(bool bPants, const TCHAR* NamePart) const
{
	const TArray<TObjectPtr<USkeletalMesh>>& List = bPants ? PantsStyles : ShirtStyles;
	for (int32 i = 0; i < List.Num(); ++i)
	{
		if (List[i] && List[i]->GetName().Contains(NamePart))
		{
			return i + 1;
		}
	}
	return 0;
}

void ABRCharacter::UpdateHairUnderHat()
{
	USkeletalMeshComponent* Body = GetMesh();
	if (!Body || !Body->GetSkeletalMeshAsset() || !Body->GetSkeletalMeshAsset()->FindMorphTarget(TEXT("Hair_UnderHat")))
	{
		return;
	}
	const bool bHatOn = OutfitHead && OutfitHead->GetSkeletalMeshAsset() && !OutfitHead->bHiddenInGame && OutfitHead->IsVisible();
	const int8 Want = bHatOn ? 1 : 0;
	if (Want == AppliedHairUnderHat && Body->GetMorphTarget(TEXT("Hair_UnderHat")) == (bHatOn ? 1.f : 0.f))
	{
		return;
	}
	AppliedHairUnderHat = Want;
	Body->SetMorphTarget(TEXT("Hair_UnderHat"), bHatOn ? 1.f : 0.f, false);
}

void ABRCharacter::TickBlink()
{
	USkeletalMeshComponent* Body = GetMesh();
	if (GetNetMode() == NM_DedicatedServer || !Body || !Body->GetSkeletalMeshAsset() || !Body->GetSkeletalMeshAsset()->FindMorphTarget(TEXT("Blink")))
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (NextBlinkTime <= 0.f)
	{
		NextBlinkTime = Now + FMath::FRandRange(0.5f, 4.f); // everyone starts out of sync
	}
	float Weight = 0.f;
	if (GetCondition() == EPVVCondition::KnockedDown)
	{
		Weight = 1.f; // out cold: eyes shut
	}
	else
	{
		if (Now >= NextBlinkTime)
		{
			BlinkStartTime = Now;
			bDoubleBlink = FMath::FRand() < 0.15f;
			NextBlinkTime = Now + FMath::FRandRange(2.f, 6.f) * (IsTipsy() ? 0.5f : 1.f); // drunk = blinky
		}
		// Quick close, slightly slower open (~0.18 s). Sometimes twice in a row.
		const float T = Now - BlinkStartTime;
		auto Shape = [](float X) { return X < 0.f ? 0.f : (X < 0.07f ? X / 0.07f : FMath::Max(0.f, 1.f - (X - 0.07f) / 0.11f)); };
		Weight = FMath::Max(Shape(T), bDoubleBlink ? Shape(T - 0.25f) : 0.f);
		if (IsTipsy())
		{
			Weight = FMath::Max(Weight, 0.35f); // heavy-lidded
		}
	}
	if (FMath::Abs(Weight - AppliedBlink) > 0.01f)
	{
		AppliedBlink = Weight;
		Body->SetMorphTarget(TEXT("Blink"), Weight, false);
	}
}

void ABRCharacter::TickHeadTag()
{
	if (!HeadTag || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	const ABRGameState* GS = GetBRGameState();
	const APlayerController* LocalPC = GetWorld()->GetFirstPlayerController();
	const ABRPlayerState* LocalPS = LocalPC ? LocalPC->GetPlayerState<ABRPlayerState>() : nullptr;
	// Border Patrol never gets a free "that one's a player" tag: only teammates (or a tagged/spotted Runner) show anything.
	const bool bFriendly = bIsNPC || (LocalPS && LocalPS->GetFaction() == GetFaction());
	const bool bRevealed = bFriendly || IsSpotted();
	const bool bBounty = GS && GS->BountyTarget && GS->BountyTarget == GetPlayerState() && GetCondition() != EPVVCondition::Restrained && bRevealed;
	const ABRPlayerState* MyPS = GetBRPlayerState();
	const int32 Stars = MyPS ? MyPS->WantedStars : 0;
	FString Text;
	FColor Color = FColor::White;
	if (Now < EmoteBubbleUntil && bFriendly)
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
	else if (Stars > 0 && bRevealed && !bIsNPC)
	{
		Text = Stars >= 3 ? TEXT("* * *") : (Stars == 2 ? TEXT("* *") : TEXT("*"));
		Color = FColor(255, 210, 40);
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

bool ABRCharacter::MusclesSafe() const
{
	const USkeletalMeshComponent* MeshComp = GetMesh();
	return MeshComp && MeshComp->IsRegistered() && MeshComp->GetSkeletalMeshAsset() && GetActorEnableCollision()
		&& MeshComp->GetComponentSpaceTransforms().Num() >= MeshComp->GetNumBones() && MeshComp->GetNumBones() > 0
		// (the muscles read the EDITABLE pose buffer - with double buffering it can be empty while the other one isn't)
		&& const_cast<USkeletalMeshComponent*>(MeshComp)->GetEditableComponentSpaceTransforms().Num() >= MeshComp->GetNumBones()
		&& MeshComp->IsPhysicsStateCreated() && MeshComp->Bodies.Num() > 0
		&& !bOnRide && !bInCannon;
}

bool ABRCharacter::NPCClimbBorderWall(const FVector& Direction)
{
	if (bClimbing)
	{
		return true;
	}
	if (!HasAuthority() || !bIsNPC || GetCondition() != EPVVCondition::Normal || GetCarrier() || GetCurrentBoat())
	{
		return false;
	}
	const FVector Dir = Direction.GetSafeNormal2D();
	if (Dir.IsNearlyZero())
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BRNPCWallCheck), false, this);
	const FVector From = GetActorLocation() + FVector(0.f, 0.f, 40.f);
	if (!GetWorld()->LineTraceSingleByChannel(Hit, From, From + Dir * 110.f, ECC_Visibility, Params)
		|| !Hit.GetActor() || !(Hit.GetActor()->IsA<ABRWall>() || Hit.GetActor()->IsA<ABRWallSegment>()))
	{
		return false;
	}
	SetActorRotation(FRotator(0.f, Dir.Rotation().Yaw, 0.f));
	return StartWallClimb();
}

// -------------------------------------------------------------------------------------------
// Gang Beasts climbing: your stuck hands hold your body up like ropes; you pull and jump yourself up.

void ABRCharacter::TickHandHang(float DeltaSeconds)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	const bool bPlayer = Cast<APlayerController>(GetController()) != nullptr;
	if (!Move || !bPlayer || !(HasAuthority() || IsLocallyControlled()) || bClimbing || GetCondition() != EPVVCondition::Normal || GetCurrentBoat() || GetCarrier()
		|| bOnShoulders || bInCannon || DeltaSeconds <= 0.f)
	{
		bHandHanging = false;
		return;
	}
	// Which stuck hands are above your chest (those are the ones that can hold you up)?
	int32 Holding = 0;
	FVector Anchor = FVector::ZeroVector;
	for (int32 h = 0; h < 2; ++h)
	{
		if (bHandAnchored[h] && HandAnchor[h].Z > GetActorLocation().Z + 15.f)
		{
			++Holding;
			Anchor += HandAnchor[h];
		}
	}
	if (Holding == 0 || Move->IsMovingOnGround())
	{
		bHandHanging = false; // feet on the ground: the hands just rest on the wall
		HangGrip = FMath::Min(1.f, HangGrip + DeltaSeconds * 0.5f);
		return;
	}
	bHandHanging = true;
	Anchor /= Holding;
	const FVector Shoulder = GetActorLocation() + FVector(0.f, 0.f, 50.f);
	const FVector ToAnchor = Anchor - Shoulder;
	const float Dist = ToAnchor.Size();
	const FVector Dir = Dist > 1.f ? ToAnchor / Dist : FVector::UpVector;
	const float ArmLen = Holding == 2 ? 95.f : 110.f;
	FVector V = Move->Velocity;
	if (Move->MovementMode != MOVE_Falling)
	{
		Move->SetMovementMode(MOVE_Falling);
	}
	// Your arms are ropes: you can swing and dangle, but not fall further than arm's length from your hands.
	if (Dist > ArmLen)
	{
		const float Out = FVector::DotProduct(V, -Dir);
		if (Out > 0.f)
		{
			V += Dir * Out;
		}
		V += Dir * (Dist - ArmLen) * 12.f;
	}
	// Look up and your arms PULL you up towards your hands (until your shoulders reach them - then re-grab higher).
	const float LookPitch = FRotator::NormalizeAxis(GetBaseAimRotation().Pitch);
	const bool bPulling = LookPitch > 15.f && ToAnchor.Z > 20.f && HangGrip > 0.f;
	if (bPulling)
	{
		const float Pull = (Holding == 2 ? 170.f : 75.f) * FMath::Lerp(0.4f, 1.f, HangGrip);
		V.Z = FMath::Max(V.Z, Pull);
	}
	// One hand: you slowly slip. Grip wears out the longer you hang (faster while pulling, much faster on one hand).
	if (Holding == 1 && !bPulling)
	{
		V.Z = FMath::Min(V.Z, -40.f);
	}
	HangGrip -= DeltaSeconds * (Holding == 2 ? 1.f / 16.f : 1.f / 6.f) * (bPulling ? 1.6f : 1.f);
	Move->Velocity = V;
	if (HangGrip <= 0.f)
	{
		HangGrip = 0.f;
		bHandAnchored[0] = bHandAnchored[1] = false; // your grip gives out: you drop
		bHandHanging = false;
		if (HasAuthority())
		{
			Notify(LOCTEXT("GripGone", "Your grip gave out!"));
		}
	}
}

void ABRCharacter::Server_HangJump_Implementation()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (!bHandHanging || Now - LastHangJump < 0.7f || HangGrip < 0.15f || GetCondition() != EPVVCondition::Normal)
	{
		return;
	}
	LastHangJump = Now;
	HangGrip -= 0.2f;
	// A real jump: push off with your arms, up and in towards the wall. Make it over the top or fall back down.
	FVector Anchor = FVector::ZeroVector;
	int32 Holding = 0;
	for (int32 h = 0; h < 2; ++h)
	{
		if (bHandAnchored[h])
		{
			Anchor += HandAnchor[h];
			++Holding;
		}
	}
	const FVector In = Holding > 0 ? ((Anchor / Holding) - GetActorLocation()).GetSafeNormal2D() : GetActorForwardVector().GetSafeNormal2D();
	LaunchCharacter(FVector(0.f, 0.f, HangJumpUp * (Holding == 2 ? 1.f : 0.7f)) + In * 160.f, false, true);
	if (bWobbleOn && GetMesh() && GetMesh()->GetBoneIndex(TEXT("spine_03")) != INDEX_NONE)
	{
		GetMesh()->AddImpulseToAllBodiesBelow(FVector(0.f, 0.f, 300.f) + In * 150.f, TEXT("spine_02"), true, true);
	}
}
