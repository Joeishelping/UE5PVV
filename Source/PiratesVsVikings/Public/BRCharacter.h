#pragma once

#include "CoreMinimal.h"
#include "PVVCharacter.h"
#include "BRTypes.h"
#include "BRCharacter.generated.h"

class ABRDesertArena;
class ABRTruck;
class ABRGameState;
class ABRPlayerState;
class UInputAction;
class UPhysicalAnimationComponent;
class USkeletalMesh;
class UStaticMesh;
class UStaticMeshComponent;
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
	/** Server (NPCs): make a run for it, away from Border Patrol, for this many seconds. */
	void StartFleeing(float Seconds);
	/** Server (NPCs): attack this person Gang Beasts style for a while, then run. */
	void StartBrawling(ABRCharacter* Target, float Seconds, float Aggression = 0.5f);
	/** NPC busted at the checkpoint (no papers / contraband): fight, pull a knife, run, or give up - random, by how bad they are. */
	void ReactToBust(ABRCharacter* Cop, bool bMinorOffense);
	/** Server (NPCs): climb the Border Wall if it's right in front of us, going this way. True if climbing (or already). */
	bool NPCClimbBorderWall(const FVector& Direction);
	/** Server (NPCs): stepped out of their van at the checkpoint to be searched. Clean people get back in once searched;
	 *  a suspect (no papers / contraband) reacts after a few seconds instead (fight, run or hands up). */
	void WaitForSearch(ABRTruck* Truck, int32 Seat, ABRCharacter* SuspectCop);
	bool IsWaitingForSearch() const { return SearchWaitTruck.IsValid(); }
	void ReturnToTruckAfterSearch();
	void TickSearchWait();
	TWeakObjectPtr<ABRTruck> SearchWaitTruck;
	int32 SearchWaitSeat = INDEX_NONE;
	float SearchWaitSince = 0.f;
	float BustReactAt = -1.f;
	TWeakObjectPtr<ABRCharacter> BustCop;
	/** Picked a fight with Border Patrol when busted (the aggressive kind - they don't run for the Wall). */
	bool bAggressiveSuspect = false;

	// --- NPC secrets (server) -------------------------------------------------------------
	/** No papers. Border Patrol score for catching them; Runners score for smuggling them through. */
	bool bIllegal = false;
	/** Carrying contraband (found by scanning). */
	bool bCarriesContraband = false;
	/** Already counted as making it across the border (points for the Runners). */
	bool bScoredCrossing = false;
	/** What the contraband is (shows on the scanner). */
	EBRItem ContrabandItem = EBRItem::Drugs;
	/** Smuggling client: follows this Runner. */
	TWeakObjectPtr<ABRCharacter> Leader;
	int32 ClientPay = 0;
	/** Last time a Border Patrol scanner checked this person (server world time), and who. */
	float LastScannedTime = -1000.f;
	TWeakObjectPtr<ABRCharacter> ScannedBy;
	/** Something Border Patrol must deal with: a Runner player, or an NPC with no papers / contraband. */
	bool IsSuspect() const { return (!bIsNPC && IsRunner() && !bDisguised) || bIllegal || bCarriesContraband; }
	void StartFollowing(ABRCharacter* NewLeader, int32 Pay);

	virtual void PossessedBy(AController* NewController) override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void OnRep_PlayerState() override;

	/** Server: fly through the air (catapult / cannon). Lands with a comedy tumble; zip-ties come off on landing. */
	void StartFlight(const FVector& Velocity);
	/** Server: the flyer passes through this (e.g. the catapult that threw them) until they land. */
	void IgnoreDuringFlight(AActor* Other);
	/** Server: hide inside a cannon / pop back out. */
	void SetInCannon(bool bIn);
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsInCannon() const { return bInCannon; }
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsFlying() const { return bFlying; }
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsDancing() const;
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsOnShoulders() const { return bOnShoulders; }
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsDiving() const { return bDiving; }
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsHoldingProp() const;
	UFUNCTION(BlueprintPure, Category = "BR|Hats") EBRHat GetHat() const { return WornHat; }
	float GetHatKnockOffChance() const { return HatKnockOffChance; }
	/** The hat you get when you didn't pick one (Border Patrol gets the Patrol Cap unless the Blueprint says otherwise). */
	EBRHat GetDefaultHat() const { return (IsWarden() && DefaultHat == EBRHat::CowboyHat) ? EBRHat::PatrolCap : DefaultHat; }

	// --- Emote wheel (local) ----------------------------------------------------------------
	static constexpr int32 NumEmotes = 7;
	static FString EmoteName(int32 Index);
	bool IsEmoteWheelOpen() const { return bEmoteWheelOpen; }
	/** Highlighted slice (-1 = none: let go to cancel). */
	int32 GetEmoteChoice() const { return EmoteChoice; }
	/** Server: drone ready to launch again right now (bought with the budget). */
	void RechargeDrone() { LastDroneTime = -1000.f; }
	/** Server: turn this NPC into a luchador for a ring fight (tougher with a bigger wager). */
	void MakeLuchador(const FString& RingName, float Toughness);
	/** Ring name shown over a luchador's head. */
	UPROPERTY(Replicated) FString LuchaName;

	// Bot controls (server).
	void BotMash();
	void BotEat();
	void BotDive();
	/** Bots: scan this person (Border Patrol). */
	void BotScan(ABRCharacter* Who);
	/** Bots: order the suspects out of the nearest vehicle. */
	void BotOrderOut();
	/** Bots: zap someone with the taser (needs ammo). True if it fired. */
	bool BotTase(ABRCharacter* Who);
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsDisguised() const { return bDisguised; }
	/** Server: put on a hat (drops nothing - see KnockOffHat). */
	void SetHat(EBRHat Hat);
	/** Server: the hat flies off and lands on the ground for anyone to grab. */
	void KnockOffHat();
	/** Server: tell the server which inventory slot is selected (shows that item in your hand). */
	UFUNCTION(Server, Reliable) void Server_SelectSlot(int32 Slot);
	/** NPCs: wear a poncho just for looks (so disguised Runners blend in). */
	void SetNPCPoncho(bool bOn) { bDisguised = bOn; OnRep_Disguise(); }
	/** Server (NPCs): hold this item in your hand (band musicians hold trumpets). */
	void SetNPCHeldItem(EBRItem Item) { HeldItem = Item; OnRep_HeldItem(); }
	/** Server: a band musician / performer plays a note (trumpet animation). */
	void PlayBandNote() { Multicast_Trumpet(TrumpetNoteHold); }
	/** Same, but only the animation (the band plays the sound itself). */
	void PlayBandNoteSilent() { Multicast_TrumpetPose(); }
	UFUNCTION(NetMulticast, Unreliable) void Multicast_TrumpetPose();
	/** Selected inventory slot (mouse wheel). */
	int32 GetSelectedSlot() const { return SelectedSlot; }
	/** Server: dance uncontrollably for a while (mariachi). */
	void ForceDance(float Seconds);

	// --- Town life (NPCs, server) -------------------------------------------------------
	/** Band musicians etc: their flailing hands never hit anyone and they don't fight back. */
	UPROPERTY(Replicated) bool bPeaceful = false;
	/** Scripted fighters (ring luchador): don't start brawls of their own when hit. */
	bool bNoFightBack = false;
	/** Belongs to the cantina (patron / bouncer): town events leave them alone. */
	bool bCantinaRegular = false;
	/** 1 in 20 NPCs secretly carries a fat wad of cash - knock them out to find out. */
	bool bLootWad = false;
	bool bLootDropped = false;
	/** An item this NPC stole from a player (they're holding it - beat them up to get it back). */
	EBRItem StolenItem = EBRItem::None;
	TWeakObjectPtr<APlayerState> StolenFrom;
	/** Who this NPC last got mad at for hitting them (so trouble only counts once). */
	TWeakObjectPtr<APVVCharacter> LastHitBy;
	/** Walking up to this player to rob them (see MugPlayer). */
	TWeakObjectPtr<ABRCharacter> MugTarget;
	/** Server (NPC): punch this player, grab one of their items and run/fight. */
	bool MugPlayer(ABRCharacter* Victim);
	/** NPC is busy in a brawl right now. */
	bool IsBrawling() const { return AngryAt.IsValid() && GetWorld() && GetWorld()->GetTimeSeconds() < AngryUntil; }
	ABRCharacter* GetBrawlTarget() const { return AngryAt.Get(); }
	/** Server (NPC): stop fighting / running and just hang around again. */
	void CalmDown() { AngryAt = nullptr; AngryUntil = -1.f; FleeUntil = -1.f; MugTarget = nullptr; }
	void SetKnockdownSeconds(float Seconds) { KnockdownDuration = Seconds; }
	/** Fills any EMPTY Class Defaults slot (hats, poncho, hair, beards, shirts, pants, glasses, animations) with the
	 *  matching asset you imported, found by name (SK_Shirt_..., SK_Hat_Sombrero, Kick, Emote_Smoke...). Slots you filled are kept. */
	void AutoFillSlots();
	virtual void PostInitializeComponents() override;
	/** Off = only use what you put in the slots yourself. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Look") bool bAutoFillEmptySlots = true;
	/** Server: play an emote right now (round-end poses), ignoring the cooldown. */
	void ForceEmote(int32 Index);
	/** Coyote whistle (Runners, Q): your smuggling clients sprint to you. Loud: Border Patrol hears it. */
	UFUNCTION(Server, Reliable) void Server_CoyoteWhistle();
	float LastWhistleTime = -100.f;
	/** NPC client: running to the coyote's whistle until then. */
	float WhistleRunUntil = -1.f;
	/** Local: big knockout punch (zoom + shake) nearby. */
	float KOPunch = 0.f;
	float LastKOPunchTime = -100.f;

	// --- Blocking (hold Left Alt): hits from the front do a fraction of the damage and barely push you.
	UPROPERTY(Replicated) bool bBlocking = false;
	/** 1 = full guard. Each blocked hit chips it; at 0 the guard breaks (stunned, can't block for a moment). */
	UPROPERTY(Replicated) float GuardMeter = 1.f;
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsBlocking() const { return bBlocking; }
	UFUNCTION(Server, Reliable) void Server_SetBlock(bool bOn);
	/** Server: NPC brawlers put their guard up for a moment. */
	void BotBlock(float Seconds);
	virtual float ModifyIncomingHit(APVVCharacter* Attacker, float Damage, FVector& Impulse) override;
	float ApplyBlock(APVVCharacter* Attacker, float Damage, FVector& Impulse);
	/** Gang Beasts pacing: every hit is divided by this (higher = longer fights). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float FightToughness = 1.9f;
	/** Gang Beasts pacing: hits push you this much (lower = you stay in the fight instead of flying off). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float HitPushScale = 0.55f;
	/** Rocked: the hit that WOULD knock you out dazes you instead (staggering, can't punch). The next good hit knocks you out. */
	UPROPERTY(Replicated) float DazedUntil = -1.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float DazeSeconds = 2.2f;
	float LastDazeTime = -100.f;

	// --- Gang Beasts feel --------------------------------------------------------------------
	/** Heavy, clumsy movement: slow to get going, slow to stop, wide turns (like steering a sack of potatoes). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|GangBeasts") bool bHeavyMovement = true;
	UPROPERTY(EditDefaultsOnly, Category = "BR|GangBeasts") float HeavyAcceleration = 900.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|GangBeasts") float HeavyBraking = 650.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|GangBeasts") float HeavyGroundFriction = 3.5f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|GangBeasts") float HeavyTurnRate = 280.f;
	/** How far the body leans into the way you're walking (degrees per m/s). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|GangBeasts") float LeanIntoMove = 1.3f;
	/** Head and shoulders bob/jiggle on every step. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|GangBeasts") float StepBob = 40.f;
	/** Punches: the arm pulls back this long before it's flung out (seconds). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|GangBeasts") float PunchWindup = 0.14f;
	/** Also play the Punch animation on top of the physics fling (off = pure physics punches, like Gang Beasts). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|GangBeasts") bool bPunchAnimWithPhysics = false;
	/** KO meter: how much one clean hit to the HEAD fills it (1 = knocked out). Body hits mostly just shove. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|GangBeasts") float HeadHitKO = 0.3f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|GangBeasts") float BodyHitKO = 0.04f;
	/** Body hits do this much of their normal damage (head hits do full). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|GangBeasts") float BodyHitDamageScale = 0.6f;
	/** KO meter drains at this rate once you haven't been hit for KORecoverDelay seconds. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|GangBeasts") float KORecoverRate = 0.15f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|GangBeasts") float KORecoverDelay = 2.5f;
	/** A real knockout keeps you down this much longer than a normal knockdown. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|GangBeasts") float KOExtraSeconds = 3.f;
	/** After a knockout you get up groggy (slow, wobbly, can't punch) for this long. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|GangBeasts") float GroggySeconds = 1.6f;
	/** Server: fill the KO meter (head hits, headbutts). At 0.6 you see birds, at 1 you're OUT. */
	void AddKO(float Amount, ABRCharacter* By);
	void KnockOut(ABRCharacter* By);
	void TickKOMeter(float DeltaSeconds);
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsKnockedOut() const { return bKnockedOut; }
	/** 0..1 how close to being knocked out (server). */
	float KOMeter = 0.f;
	float LastKOHitTime = -100.f;
	UPROPERTY(Replicated) bool bKnockedOut = false;
	bool bWasKnockedOut = false;
	FTimerHandle PunchWindupTimer;
	/** Busted NPCs (scanned illegal / ordered out): chance they FIGHT you (contraband-only people a bit less). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Suspects") float SuspectFightChance = 0.75f;
	/** Of the ones who fight, how many pull a knife. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Suspects") float SuspectKnifeShare = 0.3f;
	/** Chance they put their hands up (the rest run). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Suspects") float SuspectSurrenderChance = 0.1f;
	/** NPC finishing move on someone they knocked down: grab, drag, lift overhead, throw. */
	void BrawlerFinish(ABRCharacter* Foe);
	/** AI moves (bots / luchador / NPCs). */
	void BotReach(uint8 Hands) { ArmsReaching = Hands; }
	void BotDropKick() { DoDropKick(); }
	bool IsDropKicking() const { return bDropKicking; }
	// --- Police tools (BRPolice.cpp) ---
	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;
	/** Border Patrol can search (frisk) a standing person: weapons + contraband get seized. */
	bool WantsFrisk(const APVVCharacter* Interactor) const;
	void Frisk(ABRCharacter* Cop);
	/** Take away contraband, weapons and fake papers. Returns how many things were taken. */
	int32 Confiscate();
	/** Seen by a security camera. */
	void CameraSpot(float Seconds);
	bool PlaceSecurityCamera();
	/** NPC Border Patrol officer (reinforcements): chases and zip-ties Runners who crossed the Line, leaves after a while. */
	void MakeOfficer(float LifeSeconds);
	void TickOfficer(float DeltaSeconds);
	bool IsOfficer() const { return bOfficer; }
	/** Would the papers scan flag this person? (no papers; fake passport fails if carrying contraband) */
	bool FailsPapersScan() const;
	bool bOfficer = false;
	float OfficerUntil = 0.f;
	float LastFriskedTime = -100.f;
	// --- Prisoners ---
	void OnPrisonerStateChanged(EPVVCondition Was, EPVVCondition Now);
	void TickNPCStruggle();
	float NPCBreakFreeAt = -1.f;
	float NPCStruggleUntil = -1.f;
	// --- Gadgets (BRGadgets.cpp) ---
	void ThrowSmokeBomb();
	void UseGrapple();
	void ToggleFakeUniform();
	void TakeOffFakeUniform(bool bBlown);
	/** A Runner wearing the fake Border Patrol uniform (scans as an agent, opens the BP door). */
	bool IsFakeAgent() const { return bFakeUniform; }
	UPROPERTY(Replicated) bool bFakeUniform = false;
	FBRLook SavedLook;
	EBRHat SavedHat = EBRHat::None;
	float LastGrappleTime = -100.f;
	/** Riding a fixture (balloon basket, wheelbarrow): visible, can't walk, no collision. */
	void SetOnRide(bool bOn);
	UFUNCTION() void OnRep_OnRide();
	UPROPERTY(ReplicatedUsing = OnRep_OnRide) bool bOnRide = false;
	/** Remember you're falling from this height (a big drop knocks you out when you land). */
	void MarkFallFrom(float Z) { FallFromZ = FMath::Max(FallFromZ, Z); }
	void SlowFor(float Seconds, float SpeedFactor) { ApplySlow(Seconds, SpeedFactor); }
	void Input_TrapCycle();
	UFUNCTION(Server, Reliable) void Server_CycleTrap();
	UPROPERTY(Replicated) uint8 SelectedTrap = 0;
	void PlantTNT();
	void UseFuseKit();
	/** Getting up takes this long (can't walk while you push yourself up). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|GangBeasts") float GetUpSeconds = 1.f;
	/** Thrown people go flying FLOPPY (ragdoll) and are down this long (unless they were already KO'd). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|GangBeasts") float ThrowKnockdownSeconds = 2.2f;
	/** A hit at least this hard makes the whole body go limp for a split second (Gang Beasts stagger). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|GangBeasts") float HitFlopThreshold = 700.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|GangBeasts") float HitFlopSeconds = 0.3f;
	/** Both people grabbing each other: tug of war (more hands, more balance, pulling away and mashing Space win). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|GangBeasts") bool bTugOfWar = true;
	/** Thrown / flung bodies never fly faster than this (cm/s): ~4-5 m throws, like Gang Beasts. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|GangBeasts") float MaxFlingSpeed = 750.f;
	float HitFlopUntil = -1.f;
	float GetUpUntil = -1.f;
	float TugPull() const;
	// --- Throwing people into people ---
	/** Someone thrown at another person knocks them over too (bowling!). */
	void MarkThrown(ABRCharacter* By);
	void TickThrown();
	TWeakObjectPtr<ABRCharacter> ThrownBy;
	float ThrownUntil = -1.f;
	TArray<TWeakObjectPtr<ABRCharacter>> BowledOver;
	/** Grabbed a prop with your hands (hold the mouse button): it drops when you let go. */
	bool bPropInHands = false;
	// --- Climbing (Gang Beasts): hold on with your hands, hold SPACE to climb (look up = up, look down = down), A/D = shimmy ---
	UPROPERTY(EditDefaultsOnly, Category = "BR|Climb") float ClimbSpeed = 115.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Climb") float ShimmySpeed = 95.f;
	/** Holding on with only one hand: you slowly slide down. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Climb") float ClimbSlideSpeed = 35.f;
	/** How long your arms last on a wall (seconds of hanging with both hands; climbing tires you faster). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Climb") float ClimbStaminaSeconds = 22.f;
	UFUNCTION(Server, Reliable) void Server_ClimbHold(bool bHeld);
	bool bClimbUpHeld = false;
	float ClimbStamina = 1.f;
	float ClimbHandSwapAt = 0.f;
	bool bWarnedClimbTired = false;
	// --- Fluid physical attacks ---------------------------------------------------------
	/** Punch: the fist is steered out to the target this long (then stays out if you keep holding the button). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|GangBeasts") float PunchExtendSeconds = 0.42f;
	/** Kick: the leg is steered through the kick this long. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|GangBeasts") float KickExtendSeconds = 0.35f;
	/** Swinging your arms out (move the mouse while holding the buttons) hits people if the hand is fast enough. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|GangBeasts") bool bSwingAttacks = true;
	/** Drop kick (kick in the air, or Alt without clicking): how hard you launch and how long you're down after. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|GangBeasts") float DropKickSpeed = 850.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|GangBeasts") float DropKickDownSeconds = 1.3f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> ClimbAnim;
	UFUNCTION(Server, Reliable) void Server_DropKick();
	UFUNCTION(NetMulticast, Reliable) void Multicast_AttackSteer(uint8 Limb, FVector_NetQuantize TargetLocal, float Seconds);
	void DoDropKick();
	void TickAttackSteer();
	UPROPERTY(Replicated) bool bDropKicking = false;
	float DropKickHitUntil = -1.f;
	float LastDropKickTime = -10.f;
	/** 0 = left hand, 1 = right hand, 2 = right foot: steered toward a point (in our local space) until the time runs out. */
	FVector SteerTargetLocal[3];
	float SteerUntil[3] = { -1.f, -1.f, -1.f };
	float SteerStart[3] = { -1.f, -1.f, -1.f };
	// --- Sprint stamina (players): sprinting drains it, resting refills it; empty = winded (no sprint for a bit) ---
	UPROPERTY(EditDefaultsOnly, Category = "BR|Sprint") float SprintSeconds = 7.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Sprint") float SprintRefillSeconds = 6.f;
	UPROPERTY(Replicated) float SprintStamina = 1.f;
	bool bWinded = false;
	float LastSprintDrainTime = -10.f;
	void TickSprintStamina(float DeltaSeconds);
	bool bDazedSpeedApplied = false;
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsDazed() const;
	/** Self-defense: is this person attacking Who right now (hit them lately, fighting them, holding them, knife out)? */
	bool IsThreatTo(const ABRCharacter* Who) const;

	// --- Gang Beasts hands -------------------------------------------------------------------
	/** Local: Space is being held (climb = hold Space + alternate mouse clicks). */
	bool bSpaceHeld = false;
	/** Squash the hair under the hat (Hair_UnderHat shape key) whenever a hat is on. */
	void UpdateHairUnderHat();
	virtual void StopJumping() override;
	/** Server: let go of the wall (both mouse buttons released). */
	UFUNCTION(Server, Reliable) void Server_ClimbLetGo();
	/** Server: a reaching hand touches a wall/ledge in front: grab it (start hanging/climbing). */
	bool TryWallGrab(uint8 Hands);
	/** Which hands are on the wall (the other one can hold a person). */
	uint8 WallHandMask = 0;
	/** Grip on the wall: punches to your hands knock it down; 0 = you fall. */
	float ClimbGrip = 1.f;
	/** Someone is choking you out (held from behind with both hands). */
	UPROPERTY(Replicated) bool bChoked = false;
	float NextSumoPush = 0.f;
	/** Throw a floppy body (all machines). */
	UFUNCTION(NetMulticast, Reliable) void Multicast_FlingBody(FVector_NetQuantize Velocity);
	/** Let cars pass through this capsule for a moment (we just got run over; the ragdoll still tumbles off the car). */
	UFUNCTION(NetMulticast, Reliable) void Multicast_IgnoreVehicles(uint8 Channel, float Seconds);
	/** Who last threw / shoved / hit you (for "X sent Y for a swim"). */
	TWeakObjectPtr<ABRCharacter> LastTosser;
	float LastTossTime = -100.f;
	/** Locked in the pen: picking your cuffs (only while no Border Patrol is watching). 1 = out! */
	UPROPERTY(Replicated) float EscapeProgress = 0.f;
	UPROPERTY(Replicated) bool bGuardWatching = false;
	UFUNCTION(Server, Reliable) void Server_PickCuffs();
	void TickPrisonBreak(float DeltaSeconds);
	void BreakOut();
	float LastAggressionTime = -100.f;
	float NextNPCShot = 0.f;
	FVector LastWobbleVelocity = FVector::ZeroVector;
	TWeakObjectPtr<ABRCharacter> LastAggressionTarget;
	float GuardBrokenUntil = -1.f;
	float BlockReleaseAt = -1.f;
	/** Block button held (the guard comes back up after you punch out of it). */
	bool bBlockHeld = false;
	/** NPCs fight back once someone has done at least this much damage to them in a few seconds (a real punch - not a bump). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|NPC") float FightBackDamage = 11.f;
	TWeakObjectPtr<ABRCharacter> ProvokedBy;
	float ProvokeDamage = 0.f;
	float ProvokeTime = -100.f;
	float GuardResumeAt = -1.f;

	// --- Cantina ---------------------------------------------------------------------------
	/** Server: a drink. Wobblier but harder to knock down for this many seconds. */
	void MakeTipsy(float Seconds);
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsTipsy() const;
	/** Drinking contest (replicated to the drinker for the HUD). */
	UPROPERTY(Replicated) bool bChugging = false;
	UPROPERTY(Replicated) float ChugMine = 0.f;
	UPROPERTY(Replicated) float ChugRival = 0.f;
	/** Drinking contest: chug too fast and you GAG (lose progress). Keep a steady rhythm instead of spamming. */
	UPROPERTY(Replicated) float ChugGag = 0.f;
	float ChugGaggedUntil = 0.f;
	UFUNCTION(Server, Reliable) void Server_Chug();
	/** Big message across the middle of this player's screen for a few seconds (YOU WON THE BAR FIGHT!). */
	UFUNCTION(Client, Reliable) void Client_Banner(const FString& Text, bool bGood);
	FString BannerText;
	bool bBannerGood = true;
	float BannerUntil = -1.f;
	/** Seconds of tipsiness left (server time based), for the HUD. */
	float GetTipsySecondsLeft() const;

	/** Server: set this character's customization (replicates to everyone). */
	void SetLook(const FBRLook& NewLook);
	/** Local only: show a look immediately (menu preview while dragging sliders). */
	void PreviewLook(const FBRLook& NewLook);
	UFUNCTION(BlueprintPure, Category = "BR|Look") FBRLook GetLook() const { return Look; }
	int32 GetHairStyleCount() const { return HairStyles.Num(); }
	int32 GetShirtStyleCount() const { return ShirtStyles.Num(); }
	int32 GetPantsStyleCount() const { return PantsStyles.Num(); }
	int32 GetGlassesCount() const { return GlassesStyles.Num(); }
	/** Style number (1..) of the first shirt (bPants = false) or pants whose asset name contains NamePart; 0 if none. */
	int32 FindClothesStyle(bool bPants, const TCHAR* NamePart) const;
	int32 GetFacialHairCount() const { return FacialHairStyles.Num(); }

	// --- Read these from an Animation Blueprint (Try Get Pawn Owner -> Cast to BRCharacter) ---
	UFUNCTION(BlueprintPure, Category = "BR|Anim") float GetHunger() const { return Hunger; }
	UFUNCTION(BlueprintPure, Category = "BR|Anim") float GetVisibility() const { return Visibility; }
	UFUNCTION(BlueprintPure, Category = "BR|Anim") float GetBreath() const { return Breath; }
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsLethalMode() const { return bLethalMode; }
	/** Taser / gun out (X). Holstered = both mouse buttons are your hands. */
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsWeaponDrawn() const { return bWeaponDrawn; }
	/** Who you're holding with your hands (Gang Beasts grab), and who's holding you. */
	ABRCharacter* GetHandGrabTarget() const { return HandGrabTarget; }
	ABRCharacter* GetGrabbedBy() const { return GrabbedBy; }
	float GetGrabStruggle() const { return GrabStruggle; }
	/** Local: use the Settings menu's field of view. */
	void ApplyCameraOptions();
	/** Server: let go of whoever you're holding. */
	void ReleaseHandGrab();
	/**
	 * Server: Gang Beasts fighting brain for computer fighters (luchador, Border Patrol bots, angry NPCs).
	 * Call every tick while near Foe: punch combos, grab-and-pummel, headbutts, kicks, windmill spins, flying dives, throws.
	 * Aggression 0..1 = how fast it decides. Movement is up to the caller.
	 */
	void BrawlerAct(ABRCharacter* Foe, float Aggression, float DeltaSeconds);
	/** Server (also used by bots / the luchador). */
	void DoKick();
	void DoHeadbutt();
	bool IsClimbing() const { return bClimbing; }
	/** Hiding in a dumpster / porta-potty, or standing still in a cardboard box: Border Patrol can't see you. */
	bool IsHiddenFromPatrol() const;
	bool IsHidingInSpot() const { return bHiddenInSpot; }
	bool IsInBox() const { return bInBox; }
	/** Server: hide inside / come out of a hiding spot. */
	void SetHiddenInSpot(bool bHide, const FVector& ExitAt);
	class ABRDog* GetLeashedDog() const;
	bool IsHanging() const { return bHanging; }
	/** Server: let go of the wall (bFall = slipped / yanked off: counts as a fall). */
	void StopClimbing(bool bFall);
	void BotStruggleGrab();
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
	virtual bool WantsMeleeOnAttack() const override { return false; } // left / right click are handled per hand below
	virtual bool IsHeldByHands(const APVVCharacter* By) const override;
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
	/** Border Patrol: pick which trap B places (N cycles bear / glue / alarm / spike strip). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Input") TObjectPtr<UInputAction> TrapCycleAction;
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
	UPROPERTY(EditDefaultsOnly, Category = "BR|Input") TObjectPtr<UInputAction> BlockAction;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Input") TObjectPtr<UInputAction> OrderOutAction;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Input") TObjectPtr<UInputAction> EmoteAction;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Input") TObjectPtr<UInputAction> CashAction;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Input") TObjectPtr<UInputAction> HeadbuttAction;

	// --- Brawling (Gang Beasts style) ------------------------------------------------------
	/** Flailing hands faster than this (cm/s, relative to you) smack people they hit. 0 = off. */
	/** Any hand moving faster than this (cm/s, compared to your body) hits whoever it touches - Gang Beasts style. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float SlapSpeed = 300.f;
	/** Hand / foot speed (cm/s, into the target) that counts as a full-power hit. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float HardHitSpeed = 1500.f;
	/** A head shot at least this hard (0-1 power) knocks them down on the spot. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float HeadKnockdownPower = 0.82f;
	/** How far a punch reaches from the shoulder (cm), and the little step you take into it. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float PunchReach = 110.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float PunchStep = 260.f;
	/** Top speed of a reaching hand following your mouse (higher = faster, harder swings). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float ArmSwingMaxSpeed = 1600.f;
	/** Balance damage for the slowest / fastest hand hit (a full punch always does at least Attack Damage). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float HandHitMinDamage = 5.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float HandHitMaxDamage = 22.f;
	/** Hits to the head do this much more. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float HeadHitMultiplier = 1.6f;
	/** How hard a hit snaps the body part it lands on (bigger = more dramatic reactions). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float HitReaction = 1100.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float SlapDamage = 12.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float SlapPush = 350.f;
	/** How hard a punch throws your arm forward (physics fists). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float PunchImpulse = 2200.f;
	/** Seconds after the wind-up during which your fist can connect. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float PunchWindow = 0.3f;
	/** Jump presses to shake off a knockout early. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") int32 MashToWake = 7;
	/** Tallest ledge you can climb onto with Z (cm above your feet). Walls taller than this need a ladder or a friend. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float ClimbHeight = 240.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") TObjectPtr<USoundBase> PunchSwingSound;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") TObjectPtr<USoundBase> PunchHitSound;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") TObjectPtr<USoundBase> KnockoutSound;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") TObjectPtr<USoundBase> GrabSound;
	/** Throw speed for people and objects (cm/s). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float ThrowSpeed = 1200.f;
	/** Seconds you can hold someone who isn't zip-tied before they wriggle free. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float GrabHoldSeconds = 3.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float DiveSpeed = 1300.f;
	/** Kick (R with fists out) and headbutt (middle mouse). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float KickDamage = 22.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float KickPush = 750.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float HeadbuttDamage = 30.f;
	/** Wall climbing: height gained per correct left/right click, and how far a fall has to be to knock you out. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float ClimbStep = 45.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float FallKnockoutHeight = 320.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Brawl") float DiveCooldown = 2.f;
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
	/** Trumpet blast (G): the tune plays this long, then fades out. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Mariachi") float TrumpetSoloSeconds = 4.f;
	/** Playing in the band: the tune keeps going this long after your last note, then fades out. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Mariachi") float TrumpetNoteHold = 1.5f;
	/** How far away footsteps can be heard (cm). NPC steps are quieter than players'. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Sound") float FootstepRange = 1200.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Sound") float FootstepVolume = 0.6f;
	/** Played while you blow the trumpet (the animation pack has "Trumpet"). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> TrumpetAnim;
	/** Dancing animation for whoever got hit by the music (Mixamo "Samba Dancing"). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> DanceAnim;
	/** Emote wheel animations (hold T, move the mouse, let go). The animation pack has Emote_Wave, _Flex, _Taunt, _Beg, _Cheer. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> EmoteWaveAnim;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> EmoteFlexAnim;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> EmoteTauntAnim;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> EmoteBegAnim;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> EmoteCheerAnim;
	/** Smoking emote (the animation pack has Emote_Smoke). A cigarette appears in your hand and you puff smoke. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> EmoteSmokeAnim;
	/** Optional: played with the physics kick / headbutt / while climbing. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> KickAnim;
	/** Guard up (looping): fists in front of the face while you hold Left Alt. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> BlockAnim;
	/** Getting up off the floor after being knocked down (plays once). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> GetUpAnim;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> HeadbuttAnim;

	// --- Cash ---------------------------------------------------------------------------
	/** How much H drops on the ground (anyone can choose to pick it up). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Cash") int32 CashDropAmount = 50;

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
	UPROPERTY(EditDefaultsOnly, Category = "BR|Physics") bool bWobblyLegs = true;
	/** Gang Beasts feel: firm legs, loose spine, floppy arms. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Physics") float LegStrength = 1600.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Physics") float SpineStrength = 340.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Physics") float HeadStrength = 110.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Physics") float ArmStrength = 130.f;
	// --- Active ragdoll (Gang Beasts balance) ------------------------------------------------------------
	/** The physics puppet: your body balances itself on its feet, every hit and every start/stop tips you,
	 *  lean too far and you FALL. Grabs physically tug on whoever you hold. Untick to go back to the old feel. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Ragdoll") bool bActiveRagdoll = true;
	/** How hard you pull yourself back upright (higher = steadier on your feet). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Ragdoll") float BalanceStiffness = 55.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Ragdoll") float BalanceDamping = 7.5f;
	/** Lean past this (degrees) and you go over. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Ragdoll") float FallOverAngle = 36.f;
	/** How much a hit tips you (per unit of hit push). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Ragdoll") float HitTip = 0.06f;
	/** How much starting, stopping and turning tips you. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Ragdoll") float MoveTip = 0.05f;
	/** Hand grabs are physical: your hand is tied to their body (arms stretch, bodies tug). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Ragdoll") bool bPhysicalGrabs = true;
	/** Current lean (world direction * degrees), on every machine. */
	FVector BodyLean = FVector::ZeroVector;
	FVector BodyLeanVel = FVector::ZeroVector;
	FVector LastLeanVelocity = FVector::ZeroVector;
	bool bLeanApplied = false;
	float DiveTilt = 0.f;
	TWeakObjectPtr<ABRCharacter> LastLeanPusher;
	/** Tip the body (world-space push). Any machine; the server decides who falls. */
	void AddLean(const FVector& WorldPush, ABRCharacter* By);
	void TickActiveRagdoll(float DeltaSeconds);
	void UpdateGrabTethers();
	UPROPERTY() TObjectPtr<class UPhysicsConstraintComponent> GrabTether[2];
	// --- Dragging + lifting people over your head ---
	/** How hard a knocked-out body you're holding gets pulled along (higher = less floppy lag). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Ragdoll") float DragStrength = 9.f;
	/** Walk speed while dragging a limp body or carrying someone over your head. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Ragdoll") float DragSpeedScale = 0.7f;
	/** Seconds to heave someone up over your head. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Ragdoll") float LiftSeconds = 0.6f;
	/** Throws from over your head are this much stronger. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Ragdoll") float OverheadThrowScale = 1.25f;
	/** Arms-up pose while holding someone overhead (optional; leave empty to just use the reaching arms). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Animations") TObjectPtr<UAnimSequenceBase> LiftAnim;
	UPROPERTY(Replicated) bool bLiftOverhead = false;
	UPROPERTY(Replicated) float LiftStartTime = 0.f;
	/** Which bone of a limp body you've got hold of (index into the drag bone list; 255 = none yet). */
	UPROPERTY(Replicated) uint8 DragBone = 255;
	bool bDragSlowed = false;
	UFUNCTION(Server, Reliable) void Server_Lift();
	FTimerHandle IgnoreVehiclesTimer;
	uint8 IgnoredVehicleChannel = 255;
	FVector GetHoldPoint() const;
	void DragHeldBody(float DeltaSeconds);
	void PickDragBone();
	bool IsDraggingBody() const;
	void EndLift();
	UFUNCTION(BlueprintPure, Category = "BR|Anim") bool IsLiftingOverhead() const { return bLiftOverhead; }
	TWeakObjectPtr<UPrimitiveComponent> TetherOther[2];
	FVector TetherAnchor[2];
	bool bTetherToWorld[2] = { false, false };

	/** Gang Beasts gait: the upper body lags behind when you start, stop and turn (0 = off). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Physics") float MomentumSway = 0.35f;
	/** Arms stiffen up while aiming so shooting looks and feels precise. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Physics") float AimArmStrength = 5000.f;
	/** How hard a punch/shot shoves the upper body. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Physics") float HitWobble = 350.f;
	/** Random drunken sway. Gets worse when you're starving or nearly knocked out. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Physics") float SwayStrength = 40.f;
	/** Arm strength while driving (high = hands stay on the wheel like the animation). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Physics") float DriveArmStrength = 5000.f;

	// --- Hats with skills -----------------------------------------------------------------
	/** Hat meshes (SK_Outfit_CowboyHat, SK_Hat_Sombrero...). Leave empty to keep your OutfitHead as set. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Hats") TMap<EBRHat, TObjectPtr<USkeletalMesh>> HatMeshes;
	/** Hat this team starts with (Runner: Cowboy Hat, Border Patrol: Patrol Cap). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Hats") EBRHat DefaultHat = EBRHat::CowboyHat;
	/** Chance (0..1) your hat flies off when someone knocks you out. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Hats") float HatKnockOffChance = 0.6f;
	/** Disguise poncho mesh (SK_Outfit_Poncho), shown while disguised. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Hats") TObjectPtr<USkeletalMesh> PonchoMesh;
	/** What you hold in your hand for each selected item (trumpet: SM_Trumpet). Missing ones use a simple shape. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Hats") TMap<EBRItem, TObjectPtr<UStaticMesh>> HeldItemMeshes;
	/** Position/turn of held items in your hand. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Hats") FTransform HeldItemOffset = FTransform(FRotator(0.f, 0.f, 0.f), FVector(0.f, 8.f, 0.f));

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
	/** Shirts players can pick (SK_Shirt_...). Slot 0 in the menu = the Blueprint's own OutfitTop. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Look") TArray<TObjectPtr<USkeletalMesh>> ShirtStyles;
	/** Pants players can pick (SK_Pants_..., SK_Outfit_Jeans). Slot 0 = the Blueprint's own OutfitBottom. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Look") TArray<TObjectPtr<USkeletalMesh>> PantsStyles;
	/** Glasses / sunglasses (SK_Glasses_..., SK_Outfit_Sunglasses), worn on the OutfitFace slot. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Look") TArray<TObjectPtr<USkeletalMesh>> GlassesStyles;
	/** Mustaches and beards (SK_Face_... meshes). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Look") TArray<TObjectPtr<USkeletalMesh>> FacialHairStyles;

	// --- NPC villagers ----------------------------------------------------------------
	UPROPERTY(EditDefaultsOnly, Category = "BR|NPC") float WanderSpeed = 160.f;
	/** How fast an NPC with no papers runs when ordered out of a truck. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|NPC") float FleeSpeed = 480.f;
	/** NPCs stay down this long when knocked down / tased (players get back up faster). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|NPC") float NPCKnockdownSeconds = 14.f;

private:
	UPROPERTY(Replicated) bool bIsNPC = false;
	UPROPERTY(ReplicatedUsing = OnRep_Look) FBRLook Look;
	UPROPERTY(ReplicatedUsing = OnRep_InCannon) bool bInCannon = false;
	UPROPERTY(Replicated) bool bFlying = false;
	float FlightStartedAt = -100.f;
	FVector FlightVelocity = FVector::ZeroVector;
	UPROPERTY() TArray<TObjectPtr<AActor>> FlightIgnored;
	void EndFlightTweaks();
	void TickFlight();
	UFUNCTION(NetMulticast, Reliable) void Multicast_FlightFlop(bool bFlop);
	UPROPERTY(Replicated) float DanceUntil = 0.f;
	UPROPERTY(ReplicatedUsing = OnRep_Shoulders) bool bOnShoulders = false;
	UPROPERTY(Replicated) bool bDiving = false;
	UPROPERTY(Replicated) TObjectPtr<class ABRPhysicsProp> HeldProp;
	UPROPERTY(ReplicatedUsing = OnRep_Hat) EBRHat WornHat = EBRHat::None;
	UPROPERTY(ReplicatedUsing = OnRep_Disguise) bool bDisguised = false;
	UPROPERTY(ReplicatedUsing = OnRep_HeldItem) EBRItem HeldItem = EBRItem::None;
	int32 ServerSelectedSlot = 0;
	bool bHatAssigned = false;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> DisguiseOutfit;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> HeldItemMesh;
	TWeakObjectPtr<ABRCharacter> ShoulderBase;
	float LastSlapTime = -100.f;
	float LastDiveTime = -100.f;
	float GrabStartTime = 0.f;
	float BaseAttackDamage = -1.f;
	float BaseKnockback = -1.f;
	float PunchUntil = -1.f;
	bool bPunchLeft = false;
	bool bPunchConnected = false;
	int32 MashCount = 0;
	float KnockedDownAt = -100.f;
	float LastMashTime = -100.f;
	// Hand hits.
	FVector LastHandPos[2];
	float HandReadyAt[2] = { 0.f, 0.f };
	bool bHandsTracked = false;
	void TickHands(float DeltaSeconds);
	UFUNCTION(NetMulticast, Unreliable) void Multicast_BodyHit(FName Bone, FVector_NetQuantize Impulse);
	UFUNCTION(NetMulticast, Unreliable) void Multicast_DiveLunge();
	// Woozy: muscles get weaker as your balance runs out.
	float DizzyScale = 1.f;
	float PunchArmRestoreAt = -1.f;
	float CurrentArmMuscle() const;
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UTextRenderComponent> KOStars;
	/** Floating text over your head: emote bubbles and the bounty "$ WANTED $". */
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UTextRenderComponent> HeadTag;
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
	float FleeUntil = -1.f;
	TWeakObjectPtr<ABRCharacter> AngryAt;
	float AngryUntil = -1.f;
	UPROPERTY() TObjectPtr<USkeletalMesh> DefaultTopMesh;
	bool bSlotsAutoFilled = false;
	UPROPERTY() TObjectPtr<USkeletalMesh> DefaultBottomMesh;
	UPROPERTY(Replicated) float TipsyUntil = -1.f;
	/** Eyes: random blinks (needs the "Blink" shape key on the body). Shut while knocked out. */
	void TickBlink();
	float NextBlinkTime = 0.f;
	float BlinkStartTime = -10.f;
	float AppliedBlink = -1.f;
	bool bDoubleBlink = false;
	float LastChugTime = -1.f;
	float BrawlAggression = 0.5f;
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
	UFUNCTION() void OnRep_Hat();
	UFUNCTION() void OnRep_Disguise();
	UFUNCTION() void OnRep_HeldItem();
	virtual float GetDamageTakenMultiplier() const override;
	/** Server: something blew your disguise (sprinting, fighting, getting hit). */
	void BlowDisguise();
	void TickOutfitState();
	virtual bool HandleJumpPressed() override;
	virtual bool HandlePunchHit() override;
	virtual bool CanAttackWhileCarrying() const override;
	UFUNCTION(Server, Reliable) void Server_Mash();
	bool TryClimb();
	void Input_Grab();
	void Input_Dive();
	void Input_BlockStart();
	void Input_OrderOut();
	UFUNCTION(Server, Reliable) void Server_OrderOut();
	void Input_BlockStop();
	void Input_EmoteStart();
	void Input_EmoteStop();
	void Input_DropCash();
	UFUNCTION(Server, Reliable) void Server_Emote(uint8 Index);
	UFUNCTION(NetMulticast, Unreliable) void Multicast_Emote(uint8 Index);
	UFUNCTION(Server, Reliable) void Server_DropCash();
	void TickEmoteWheel();
	void TickHeadTag();
	bool bEmoteWheelOpen = false;
	int32 EmoteChoice = -1;
	FVector2D EmoteAim = FVector2D::ZeroVector;
	FString EmoteBubble;
	/** Emote playing right now (-1 = none). Loops until you move, fight or pick it again. */
	UPROPERTY(ReplicatedUsing = OnRep_ActiveEmote) int32 ActiveEmote = -1;
	UFUNCTION() void OnRep_ActiveEmote();
	UAnimSequenceBase* EmoteAnimFor(int32 Index) const;
	void StopEmote();
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> CigaretteMesh;
	float NextSmokePuff = 0.f;
	// Body sounds (local, every screen)
	void TickBodySounds(float DeltaSeconds);
	void PlayBodySound(const TArray<TSoftObjectPtr<USoundBase>>& List, const FVector& Where, float Volume = 1.f, float Range = 2000.f);
	float StepClock = 0.f;
	/** Footsteps: each foot was up in the air last frame (so we step when it comes down). */
	bool bFootUp[2] = { false, false };
	float LastStepTime[2] = { -1.f, -1.f };
	float FootFloor[2] = { 1000.f, 1000.f };
	/** The trumpet tune that's playing (a new one never starts on top of it). */
	UPROPERTY() TObjectPtr<class UAudioComponent> TrumpetAudio;
	/** The tune fades out this long after your last note / blast (keeps going while you keep playing). */
	float TrumpetPlayUntil = -1.f;
	void TickTrumpetAudio();
	int8 AppliedHairUnderHat = -1;
	float PelvisFallSpeed = 0.f;
	float LastThumpTime = -100.f;
	float LastGruntTime = -100.f;
	bool bYelled = false;
	float EmoteBubbleUntil = -100.f;
	float LastEmoteTime = -100.f;
	EPVVCondition LastSoundCondition = EPVVCondition::Normal;
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
	void Input_LeftHandStart();
	void Input_Headbutt();
	UFUNCTION(Server, Reliable) void Server_Kick();
	UFUNCTION(Server, Reliable) void Server_Headbutt();
	UFUNCTION(NetMulticast, Unreliable) void Multicast_Kick();
	UFUNCTION(NetMulticast, Unreliable) void Multicast_Headbutt();
	void KickHit();
	void HeadbuttHit();
	float LastKickTime = -100.f;
	/** Physics kick: the foot is "live" until this time; it hurts only if it touches someone. */
	float KickHitUntil = -1.f;
	FVector LastFootPos = FVector::ZeroVector;
	bool bFootTracked = false;
	void TickKickContact(float DeltaSeconds);
	// Hiding, box, lasso, leash
	UPROPERTY(ReplicatedUsing = OnRep_HiddenInSpot) bool bHiddenInSpot = false;
	UPROPERTY(ReplicatedUsing = OnRep_Box) bool bInBox = false;
	UPROPERTY(Replicated) TObjectPtr<class ABRDog> LeashedDog;
	UFUNCTION() void OnRep_HiddenInSpot();
	UFUNCTION() void OnRep_Box();
	UFUNCTION(Server, Reliable) void Server_Lasso(FVector_NetQuantize From, FVector_NetQuantizeNormal Dir);
	UFUNCTION(NetMulticast, Unreliable) void Multicast_LassoRope(FVector_NetQuantize End);
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> RopeMesh;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> BoxMesh;
	float LassoRopeUntil = -1.f;
	FVector LassoRopeEnd = FVector::ZeroVector;
	float LastLassoTime = -100.f;
	void ToggleBox();
	void TickRope();
	void DrawRope(const FVector& From, const FVector& To);
	float BrawlNextMove = 0.f;
	float BrawlSpinUntil = 0.f;
	float BrawlHoldUntil = 0.f;
	bool bBrawlLeft = false;
	float LastHeadbuttTime = -100.f;
	FTimerHandle MoveHitTimer;
	// Wall climbing (Gang Beasts): Z at a wall, then alternate left / right click.
	bool StartWallClimb();
	/** Is the body in a state where the wobbly-body muscles can safely read it? (posed bones, physics bodies, collision on) */
	bool MusclesSafe() const;
	/** Sticky hands: where each reaching hand is stuck to a surface (left, right). */
	FVector HandAnchor[2];
	bool bHandAnchored[2] = { false, false };
	/** Hanging off your stuck hands (no climb mode - your arms hold you like ropes). */
	bool bHandHanging = false;
	float HangGrip = 1.f;
	float LastHangJump = -10.f;
	void TickHandHang(float DeltaSeconds);
	UFUNCTION(Server, Reliable) void Server_HangJump();
	/** Upward push of a jump while hanging from your hands (cm/s). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|GangBeasts") float HangJumpUp = 560.f;
	UFUNCTION(Server, Reliable) void Server_ClimbStep(bool bLeft);
	UFUNCTION(Server, Reliable) void Server_ClimbJump();
	UFUNCTION() void OnRep_Climbing();
	void TickClimb(float DeltaSeconds);
	UPROPERTY(ReplicatedUsing = OnRep_Climbing) bool bClimbing = false;
	UPROPERTY(Replicated) bool bHanging = false;
	UPROPERTY(Replicated) bool bLastClimbLeft = false;
	UPROPERTY(Replicated) FVector_NetQuantizeNormal ClimbNormal;
	float ClimbTopZ = 0.f;
	bool bClimbHasTop = false;
	bool bClimbingBorderWall = false;
	float LastClimbStepTime = 0.f;
	float NextSensorPing = 0.f;
	int32 ClimbSlips = 0;
	bool bClimbStarted = false;
	float FallFromZ = -1.f;
	void Input_LeftHandStop();
	void Input_RightHandStop();
	UFUNCTION(Server, Reliable) void Server_PunchHand(bool bLeft);
	UFUNCTION(Server, Reliable) void Server_HandHold(uint8 Hand, bool bDown);
	UFUNCTION(Server, Reliable) void Server_GrabStruggle();
	UFUNCTION(NetMulticast, Unreliable) void Multicast_PunchHand(bool bLeft);
	void TickHandGrab(float DeltaSeconds);
	void TickReachVisuals();
	bool TryHandGrab(uint8 HandMask);
	bool OwnsWeapon() const;
	UPROPERTY(Replicated) bool bWeaponDrawn = false;
	/** Hands raised/reaching (bit 1 = left, 2 = right). */
	UPROPERTY(Replicated) uint8 ArmsReaching = 0;
	/** Hands actually gripping HandGrabTarget. */
	UPROPERTY(Replicated) uint8 HandGrabMask = 0;
	UPROPERTY(Replicated) TObjectPtr<ABRCharacter> HandGrabTarget;
	UPROPERTY(Replicated) TObjectPtr<ABRCharacter> GrabbedBy;
	UPROPERTY(Replicated) float GrabStruggle = 0.f;
	bool bLeftDown = false;
	bool bRightDown = false;
	float LeftDownAt = 0.f;
	float RightDownAt = 0.f;
	bool bRightIsAim = false;
	uint8 SentReach = 0;
	uint8 AppliedReachMuscles = 0;
	int8 ForcedPunchHand = -1;
	float LastGrabTryTime = 0.f;
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
	UFUNCTION(NetMulticast, Unreliable) void Multicast_Trumpet(float HoldSeconds);
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
