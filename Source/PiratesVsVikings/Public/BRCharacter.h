#pragma once

#include "CoreMinimal.h"
#include "PVVCharacter.h"
#include "BRTypes.h"
#include "BRCharacter.generated.h"

class ABRDesertArena;
class ABRGameState;
class ABRPlayerState;
class UInputAction;
class UPhysicalAnimationComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class USoundBase;
class UAnimSequenceBase;

/**
 * Border Run character.
 *  Everyone: hunger (Runners), stealth, crouch, swimming/drowning, wobbly physics body, outfit slots.
 *  Guns: hold RMB to aim, LMB to fire, R to reload. Wardens: X = taser/gun, Q = drone, V = scanner, B = place trap.
 *  NPC passengers (bIsNPC) use the same class so they look exactly like players.
 */
UCLASS()
class PIRATESVSVIKINGS_API ABRCharacter : public APVVCharacter
{
	GENERATED_BODY()

public:
	ABRCharacter();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	ABRPlayerState* GetBRPlayerState() const;
	ABRGameState* GetBRGameState() const;
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsRunner() const { return GetFaction() == BR::Runners; }
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsWarden() const { return GetFaction() == BR::Wardens; }
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsNPC() const { return bIsNPC; }

	/** Call before FinishSpawning: a computer-controlled look-alike passenger. Gets a random look. */
	void InitAsNPC(float SkinToneMin = 0.f, float SkinToneMax = 1.f);
	/** Server, after spawning an NPC: stroll around Home (village life). */
	void StartWandering(const FVector& Home, float Radius);

	// --- NPC secrets (server) -------------------------------------------------------------
	/** No papers. Border Patrol score for catching them; Runners score for smuggling them through. */
	bool bIllegal = false;
	/** Carrying contraband (found by scanning). */
	bool bCarriesContraband = false;
	/** Smuggling client: follows this Runner. */
	TWeakObjectPtr<ABRCharacter> Leader;
	int32 ClientPay = 0;
	/** Last time a Border Patrol scanner checked this person (server world time), and who. */
	float LastScannedTime = -1000.f;
	TWeakObjectPtr<ABRCharacter> ScannedBy;
	/** Something Border Patrol must deal with: a Runner player, or an NPC with no papers / contraband. */
	bool IsSuspect() const { return (!bIsNPC && IsRunner()) || bIllegal || bCarriesContraband; }
	void StartFollowing(ABRCharacter* NewLeader, int32 Pay);

	virtual void PossessedBy(AController* NewController) override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void OnRep_PlayerState() override;

	/** Server: fly through the air (catapult / cannon). Lands with a comedy tumble; zip-ties come off on landing. */
	void StartFlight(const FVector& Velocity);
	/** Server: hide inside a cannon / pop back out. */
	void SetInCannon(bool bIn);
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsInCannon() const { return bInCannon; }
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsFlying() const { return bFlying; }
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsDancing() const;
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsOnShoulders() const { return bOnShoulders; }
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsDiving() const { return bDiving; }
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsHoldingProp() const;
	/** Selected inventory slot (mouse wheel). */
	int32 GetSelectedSlot() const { return SelectedSlot; }
	/** Server: dance uncontrollably for a while (mariachi). */
	void ForceDance(float Seconds);

	/** Server: set this character's customization (replicates to everyone). */
	void SetLook(const FBRLook& NewLook);
	/** Local only: show a look immediately (menu preview while dragging sliders). */
	void PreviewLook(const FBRLook& NewLook);
	UFUNCTION(BlueprintPure, Category = "BR|Look") FBRLook GetLook() const { return Look; }
	int32 GetHairStyleCount() const { return HairStyles.Num(); }
	int32 GetFacialHairCount() const { return FacialHairStyles.Num(); }

	// --- Read these from an Animation Blueprint (Try Get Pawn Owner -> Cast to BRCharacter) ---
	UFUNCTION(BlueprintPure, Category = "BR|Anim") float GetHunger() const { return Hunger; }
	UFUNCTION(BlueprintPure, Category = "BR|Anim") float GetVisibility() const { return Visibility; }
	UFUNCTION(BlueprintPure, Category = "BR|Anim") float GetBreath() const { return Breath; }
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsLethalMode() const { return bLethalMode; }
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsAiming() const { return bAiming; }
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsReloading() const { return bReloading; }
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsSlowed() const;
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsSpotted() const;
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool HasWeapon() const;
	UFUNCTION(BlueprintPure, Category = "BR|Anim") int32 GetAmmo() const;
	UFUNCTION(BlueprintPure, Category = "BR|Anim") int32 GetMagSize() const;
	/** 0 = no gun, 1 = taser, 2 = pistol, 3 = revolver. */
	UFUNCTION(BlueprintPure, Category = "BR|Anim") int32 GetWeaponType() const { return GunStyle < 0 ? 0 : GunStyle; }
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsKnockedDown() const { return GetCondition() == EPVVCondition::KnockedDown; }
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsRestrained() const { return GetCondition() == EPVVCondition::Restrained; }
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsSeated() const { return GetCurrentBoat() != nullptr; }
	/** In the driver's seat of a truck/boat. */
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsDriving() const;
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsBeingCarried() const { return GetCarrier() != nullptr; }
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsCarryingSomeone() const { return GetCarriedCharacter() != nullptr; }
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsInDeepWater() const { return bInDeepWater; }
	FString GetWeaponName() const;

	/** Server: short message on this player's screen. */
	void Notify(const FText& Message) const;

	// Server-side effects used by traps, dogs, drones and food.
	void Eat(float Amount) { Hunger = FMath::Min(100.f, Hunger + Amount); }
	void Spot(float Seconds);
	void ApplySlow(float Seconds, float SpeedFactor);
	void OnDroneFinished();

protected:
	virtual void EnsureDefaultInput() override;
	virtual void UpdateMovementSpeed() override;
	virtual void OnStateEffectsRefreshed() override;
	virtual bool WantsMeleeOnAttack() const override { return !bAiming; }
	virtual void OnHitReceivedNative(APVVCharacter* Attacker) override;
	virtual void OnAttackStartedNative() override;

	// --- Animation slots: drag your (retargeted) animations in. No Animation Blueprint needed. ---
	// Used when the Mesh has no Anim Class. The game picks the right one automatically.
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> IdleAnim;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> WalkAnim;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> RunAnim;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> CrouchIdleAnim;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> CrouchWalkAnim;
	/** Jumping, falling, flying off the catapult. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> FallAnim;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> SwimAnim;
	/** Passenger seats and the truck bed. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> SeatedAnim;
	/** Driver's seat: hands on the wheel (Mixamo "Driving"). Falls back to SeatedAnim. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> DriveAnim;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> RestrainedAnim;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> StruggleAnim;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> CarriedAnim;
	/** Holding E: jobs, cutting wire, zip-tying, refuelling. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> WorkAnim;
	/** Standing still and aiming: both hands on the gun (Mixamo "Pistol Idle" / "Rifle Aiming Idle"). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> AimAnim;
	/** Walking while aiming (Mixamo "Pistol Walk"). Falls back to AimAnim. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> AimWalkAnim;
	// One-shots.
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> PunchAnim;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> FireAnim;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> ReloadAnim;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> EatAnim;
	/** Speed (cm/s) where walking switches to running. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") float RunAnimAboveSpeed = 520.f;

	// --- Animation events (play montages from these in your character Blueprint) ---------
	UFUNCTION(BlueprintImplementableEvent, Category = "BR|Anim") void OnWeaponFired(bool bLethal);
	UFUNCTION(BlueprintImplementableEvent, Category = "BR|Anim") void OnReloadStarted();
	UFUNCTION(BlueprintImplementableEvent, Category = "BR|Anim") void OnAte();

	// --- Components -------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BR|Components")
	TObjectPtr<UPhysicalAnimationComponent> PhysicalAnimation;

	/** Outfit slots: assign Skeletal Meshes made on the body's skeleton (they follow the body via Leader Pose). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BR|Outfit") TObjectPtr<USkeletalMeshComponent> OutfitTop;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BR|Outfit") TObjectPtr<USkeletalMeshComponent> OutfitBottom;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BR|Outfit") TObjectPtr<USkeletalMeshComponent> OutfitHead;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BR|Outfit") TObjectPtr<USkeletalMeshComponent> OutfitFace;
	/** Filled from Hair Styles / Facial Hair Styles (menu choice). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BR|Outfit") TObjectPtr<USkeletalMeshComponent> Hair;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BR|Outfit") TObjectPtr<USkeletalMeshComponent> FacialHair;

	/** The gun in your hand (greybox parts; set CustomGunMesh to use a real model instead). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BR|Weapons") TObjectPtr<USceneComponent> WeaponRoot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BR|Weapons") TObjectPtr<UStaticMeshComponent> GunBody;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BR|Weapons") TObjectPtr<UStaticMeshComponent> GunBarrel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BR|Weapons") TObjectPtr<UStaticMeshComponent> GunGrip;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BR|Weapons") TObjectPtr<UStaticMeshComponent> GunExtra;

	// --- Input (built at runtime unless assigned) -------------------------------------

	UPROPERTY(EditDefaultsOnly, Category = "BR|Input") TObjectPtr<UInputAction> AimAction;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Input") TObjectPtr<UInputAction> ReloadAction;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Input") TObjectPtr<UInputAction> CrouchAction;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Input") TObjectPtr<UInputAction> EatAction;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Input") TObjectPtr<UInputAction> DroneAction;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Input") TObjectPtr<UInputAction> WeaponModeAction;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Input") TObjectPtr<UInputAction> ScanAction;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Input") TObjectPtr<UInputAction> TrapAction;
	/** (Unused - the trumpet is played with F when selected.) */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Input") TObjectPtr<UInputAction> TrumpetAction;
	/** Mouse wheel: pick an inventory slot. G: drop it. F: use it. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Input") TObjectPtr<UInputAction> NextItemAction;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Input") TObjectPtr<UInputAction> PrevItemAction;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Input") TObjectPtr<UInputAction> DropAction;
	/** Z: grab a person or object / throw it. On a teammate: climb onto their shoulders. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Input") TObjectPtr<UInputAction> GrabAction;
	/** Left Alt: dive / tackle. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Input") TObjectPtr<UInputAction> DiveAction;

	// --- Brawling (Gang Beasts style) ------------------------------------------------------
	/** Flailing hands faster than this (cm/s, relative to you) smack people they hit. 0 = off. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float SlapSpeed = 650.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float SlapDamage = 12.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float SlapPush = 350.f;
	/** Throw speed for people and objects (cm/s). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float ThrowSpeed = 1300.f;
	/** Seconds you can hold someone who isn't zip-tied before they wriggle free. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float GrabHoldSeconds = 3.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float DiveSpeed = 1100.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float DiveCooldown = 3.f;
	/** How high you leap off a friend's shoulders (cm/s up). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float ShoulderLeap = 1100.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") TObjectPtr<USoundBase> SlapSound;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") TObjectPtr<USoundBase> TackleSound;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") TObjectPtr<USoundBase> ThrowSound;
	/** Proximity voice: how far away (cm) you can still hear this player. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Voice") float VoiceRange = 2500.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Voice") bool bProximityVoice = true;
	/** Screen shake when you get hit / knocked down / land a big flight (0 = off). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float ScreenShake = 1.f;

	// --- Mariachi trumpet ---------------------------------------------------------------
	/** Border Patrol within this range (cm) can't stop dancing. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Mariachi") float TrumpetRange = 1300.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Mariachi") float DanceSeconds = 4.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Mariachi") float TrumpetCooldown = 25.f;
	/** Your mariachi music (drag a sound in). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Mariachi") TObjectPtr<USoundBase> TrumpetSound;
	/** Dancing animation for whoever got hit by the music (Mixamo "Samba Dancing"). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> DanceAnim;

	// --- Tuning -----------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, Category = "BR|Hunger") float HungerPerSecond = 100.f / 480.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Hunger") float TacoFood = 45.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Hunger") float WeakHunger = 25.f;

	/** Seconds you can stay in deep water before drowning. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Water") float BreathSeconds = 14.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Water") float RiverCurrent = 0.65f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Water") float DeepWaterSpeedMultiplier = 0.55f;

	UPROPERTY(EditDefaultsOnly, Category = "BR|Weapons") float WeaponRange = 6000.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Weapons") float AimSpreadDegrees = 0.6f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Weapons") float RecoilPitch = 0.9f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Weapons") float AimArmLength = 170.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Weapons") float AimFOV = 60.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Weapons") TObjectPtr<USoundBase> GunSound;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Weapons") TObjectPtr<USoundBase> TaserSound;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Weapons") TObjectPtr<USoundBase> ReloadSound;
	/** Optional real gun models (e.g. from Fab). Leave empty for the built-in greybox guns. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Weapons") TObjectPtr<UStaticMesh> CustomPistolMesh;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Weapons") TObjectPtr<UStaticMesh> CustomTaserMesh;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Weapons") TObjectPtr<UStaticMesh> CustomRevolverMesh;
	/** Hand bone the gun attaches to, and the offset/rotation that makes it sit right in your model's hand. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Weapons") FName HandSocket = TEXT("hand_r");
	UPROPERTY(EditDefaultsOnly, Category = "BR|Weapons") FTransform HandGripOffset;
	/** On + an Aim animation = the gun stays in your hand while aiming (hands on the gun). Off = it floats in front of you. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Weapons") bool bGunInHandWhileAiming = true;
	/** Where the gun floats while aiming when there's no aim animation (relative to the character, cartoon style). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Weapons") FVector AimGunOffset = FVector(55.f, 24.f, 48.f);

	UPROPERTY(EditDefaultsOnly, Category = "BR|Drone") float DroneCooldown = 20.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Wardens") int32 MaxTeamTraps = 8;

	/** Wobbly active-ragdoll body (needs a skeletal mesh with a Physics Asset). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Physics") bool bWobblyBody = true;
	/** Overall muscle strength (multiplies the ones below). Lower = drunker. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Physics") float WobbleStrength = 1.f;
	/** Off = legs follow the walk animation exactly (feet stay planted), only the upper body wobbles. On = legs wobble too. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Physics") bool bWobblyLegs = false;
	/** Gang Beasts feel: firm legs, loose spine, floppy arms. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Physics") float LegStrength = 1600.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Physics") float SpineStrength = 700.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Physics") float HeadStrength = 350.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Physics") float ArmStrength = 180.f;
	/** Arms stiffen up while aiming so shooting looks and feels precise. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Physics") float AimArmStrength = 5000.f;
	/** How hard a punch/shot shoves the upper body. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Physics") float HitWobble = 350.f;
	/** Random drunken sway. Gets worse when you're starving or nearly knocked out. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Physics") float SwayStrength = 40.f;
	/** Arm strength while driving (high = hands stay on the wheel like the animation). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Physics") float DriveArmStrength = 5000.f;

	// --- Customization: how the menu sliders reach your materials ---------------------
	/** Vector parameters on your BODY material(s) that get the skin colour. Add one called SkinColor to your body material. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Look") TArray<FName> SkinColorParameters = { TEXT("SkinColor"), TEXT("SkinTint"), TEXT("BaseColorFactor") };
	/** Only material slots whose slot name or material name contains one of these words get the skin colour (eyes, lips, clothes stay as they are). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Look") TArray<FString> SkinSlotKeywords = { TEXT("Skin") };
	/** If your skin material has none of the parameters above, swap it for a plain colour material so the slider still works. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Look") bool bAutoSkinMaterial = true;
	/** Used only when no slot name matches SkinSlotKeywords. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Look") int32 SkinMaterialSlot = 0;
	/** Shirt / pants colour only goes on this material slot of the outfit (0 = the main fabric, so logos and letters keep their colour). -1 = all slots. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Look") int32 ClothMaterialSlot = 0;
	/** Vector parameters on the shirt (OutfitTop) / pants (OutfitBottom) materials that get the colour. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Look") TArray<FName> ShirtColorParameters = { TEXT("ShirtColor"), TEXT("Tint"), TEXT("BaseColorFactor") };
	UPROPERTY(EditDefaultsOnly, Category = "BR|Look") TArray<FName> PantsColorParameters = { TEXT("PantsColor"), TEXT("Tint"), TEXT("BaseColorFactor") };
	UPROPERTY(EditDefaultsOnly, Category = "BR|Look") TArray<FName> HairColorParameters = { TEXT("HairColor"), TEXT("Tint"), TEXT("BaseColorFactor") };
	/** Hairstyles players can pick in the menu (SK_Hair_... meshes, skeleton = your body's). Empty = keep the Hair component as set. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Look") TArray<TObjectPtr<USkeletalMesh>> HairStyles;
	/** Mustaches and beards (SK_Face_... meshes). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Look") TArray<TObjectPtr<USkeletalMesh>> FacialHairStyles;

	// --- NPC villagers ----------------------------------------------------------------
	UPROPERTY(EditDefaultsOnly, Category = "BR|NPC") float WanderSpeed = 160.f;

private:
	UPROPERTY(Replicated) bool bIsNPC = false;
	UPROPERTY(ReplicatedUsing = OnRep_Look) FBRLook Look;
	UPROPERTY(ReplicatedUsing = OnRep_InCannon) bool bInCannon = false;
	UPROPERTY(Replicated) bool bFlying = false;
	UPROPERTY(Replicated) float DanceUntil = 0.f;
	UPROPERTY(ReplicatedUsing = OnRep_Shoulders) bool bOnShoulders = false;
	UPROPERTY(Replicated) bool bDiving = false;
	UPROPERTY(Replicated) TObjectPtr<class ABRPhysicsProp> HeldProp;
	TWeakObjectPtr<ABRCharacter> ShoulderBase;
	float LastSlapTime = -100.f;
	float LastDiveTime = -100.f;
	float GrabStartTime = 0.f;
	float BaseAttackDamage = -1.f;
	float BaseKnockback = -1.f;
	// Local screen shake.
	float ShakeAmount = 0.f;
	FVector CameraBaseLocation = FVector::ZeroVector;
	void AddShake(float Amount) { ShakeAmount = FMath::Clamp(ShakeAmount + Amount, 0.f, 1.5f); }
	void TickShake(float DeltaSeconds);
	UPROPERTY(Transient) TObjectPtr<class UVOIPTalker> VoiceTalker;
	UPROPERTY(Transient) TObjectPtr<class USoundAttenuation> VoiceAttenuation;
	void SetupProximityVoice();
	float LastTrumpetTime = -100.f;
	UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> SkinFallbackMID;
	bool bWandering = false;
	FVector WanderHome = FVector::ZeroVector;
	FVector WanderTarget = FVector::ZeroVector;
	float WanderRadius = 1500.f;
	float WanderPauseUntil = 0.f;
	float WanderGiveUpAt = 0.f;
	UPROPERTY(Replicated) float Hunger = 100.f;
	UPROPERTY(Replicated) float Visibility = 1.f;
	UPROPERTY(Replicated) float SpottedUntil = 0.f;
	UPROPERTY(Replicated) float Breath = 1.f;
	UPROPERTY(Replicated) bool bLethalMode = false;
	UPROPERTY(ReplicatedUsing = OnRep_Aiming) bool bAiming = false;
	UPROPERTY(Replicated) bool bReloading = false;
	UPROPERTY(Replicated) int32 AmmoTaser = 1;
	UPROPERTY(Replicated) int32 AmmoGun = 12;
	UPROPERTY(Replicated) int32 AmmoRunnerGun = 6;
	UPROPERTY(ReplicatedUsing = OnRep_Slow) float SlowUntil = 0.f;
	UPROPERTY(ReplicatedUsing = OnRep_Slow) float SlowFactor = 1.f;

	TWeakObjectPtr<ABRDesertArena> Arena;
	float LastFireTime = -100.f;
	float LastLocalFireTime = -100.f;
	float LastDroneTime = -100.f;
	float LastFaintTime = -100.f;
	float LastScanTime = -100.f;
	float NextVisibilityUpdate = 0.f;
	float DefaultArmLength = 450.f;
	float DefaultFOV = 90.f;
	FVector DefaultSocketOffset = FVector::ZeroVector;
	int32 NextTrapType = 0;
	bool bInDrone = false;
	bool bStealthHidden = false;
	bool bInDeepWater = false;
	bool bSlowApplied = false;
	bool bWobbleOn = false;
	bool bArmsStiff = false;
	UPROPERTY(Transient) TObjectPtr<UAnimSequenceBase> CurrentSlotAnim;
	float OneShotUntil = 0.f;
	float NextSwayTime = 0.f;
	int32 GunStyle = -1;   // 0 none, 1 taser, 2 pistol, 3 revolver
	int32 GunPlacement = -1; // 0 hand, 1 aiming (floating), 2 aiming (in hand)
	FTimerHandle ReloadTimer;

	ABRDesertArena* FindArena();
	void UpdateLocalVisibility();
	void UpdateAimCamera(float DeltaSeconds);
	void ApplyAimRotationMode();
	void EnableWobble();
	void DisableWobble();
	void ApplyMuscles(float ArmMultiplierOverride);
	void TickWobble();
	void TickAnimationSlots();
	void PlayOneShot(UAnimSequenceBase* Anim);
	bool UsesAnimationSlots() const;
	UFUNCTION(NetMulticast, Unreliable) void Multicast_ReloadStarted();
	UFUNCTION(NetMulticast, Unreliable) void Multicast_Ate();
	int32& AmmoRef();
	float FireCooldown() const;
	float ReloadSeconds() const;
	void FinishReload();
	void Drown();
	void UpdateGunVisual();
	FVector GetMuzzleLocation() const;

	UFUNCTION() void OnRep_Aiming();
	UFUNCTION() void OnRep_Look();
	UFUNCTION() void OnRep_InCannon();
	UFUNCTION() void OnRep_Shoulders();
	virtual bool HandleJumpPressed() override;
	void Input_Grab();
	void Input_Dive();
	UFUNCTION(Server, Reliable) void Server_Grab();
	UFUNCTION(Server, Reliable) void Server_Dive();
	UFUNCTION(Server, Reliable) void Server_LeapOff();
	UFUNCTION(NetMulticast, Unreliable) void Multicast_Sfx(uint8 Which);
	void TickBrawl(float DeltaSeconds);
	void ClimbOnto(ABRCharacter* Base);
	void LeapOff(const FVector& Velocity);
	void SetHeldProp(class ABRPhysicsProp* Prop);
	void ApplyLook(const FBRLook& L);
	void ApplySkin(const FLinearColor& Color);
	void TintOutfit(USkeletalMeshComponent* Part, const TArray<FName>& Parameters, bool bOriginal, const FLinearColor& Color, int32 OnlySlot = -1);
	TArray<int32> SkinSlotsCache;
	TWeakObjectPtr<USkeletalMesh> SkinSlotsMesh;
	bool bWarnedNoMorphs = false;
	// Face shapes are applied a moment after spawning (the body must be fully set up first).
	bool bFacePending = false;
	bool bWobblePending = false;
	FBRLook PendingFace;
	float FaceReadyTime = 0.f;
	void PickStyle(USkeletalMeshComponent* Part, const TArray<TObjectPtr<USkeletalMesh>>& Styles, int32 Index);
	void ApplyFaceShape(const FBRLook& L);
	void TickWander(float DeltaSeconds);
	void TickFollow(float DeltaSeconds);
	bool WantsStiffArms() const;
	UFUNCTION() void OnRep_Slow();

	void Input_AimStart();
	void Input_AimStop();
	void Input_FireIfAiming();
	void Input_Reload();
	void Input_Crouch();
	void Input_Eat();
	void Input_Trumpet();
	void Input_NextItem();
	void Input_PrevItem();
	void Input_Drop();
	void PlayTrumpet();
	void PlaceTacoPlatter();
	int32 SelectedSlot = 0;
	UFUNCTION(Server, Reliable) void Server_PlayTrumpet();
	UFUNCTION(Server, Reliable) void Server_UseItem(int32 Slot);
	UFUNCTION(Server, Reliable) void Server_DropItem(int32 Slot);
	UFUNCTION(NetMulticast, Unreliable) void Multicast_Trumpet();
	void Input_Drone();
	void Input_WeaponMode();
	void Input_Scan();
	void Input_Trap();

	UFUNCTION(Server, Reliable) void Server_SetAiming(bool bNewAiming);
	UFUNCTION(Server, Reliable) void Server_Fire(FVector_NetQuantize Start, FVector_NetQuantizeNormal Direction);
	UFUNCTION(Server, Reliable) void Server_Reload();
	UFUNCTION(Server, Reliable) void Server_Eat();
	UFUNCTION(Server, Reliable) void Server_LaunchDrone();
	UFUNCTION(Server, Reliable) void Server_ToggleWeaponMode();
	UFUNCTION(Server, Reliable) void Server_Scan(FVector_NetQuantize Start, FVector_NetQuantizeNormal Direction);
	UFUNCTION(Server, Reliable) void Server_PlaceTrap();
	UFUNCTION(NetMulticast, Unreliable) void Multicast_Shot(FVector_NetQuantize Start, FVector_NetQuantize End, bool bLethal, bool bHitSomeone);
};
