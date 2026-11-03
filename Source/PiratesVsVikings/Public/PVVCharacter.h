#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PVVInteractable.h"
#include "PVVTypes.h"
#include "PVVCharacter.generated.h"

class APVVBoat;
class APVVDetentionZone;
class APVVLake;
class UMaterialInstanceDynamic;
class APVVPlayerState;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class USpringArmComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
struct FInputActionValue;

/**
 * Player character for both factions. Make one Blueprint child per faction (mesh, anim BP, tuning).
 *
 * Core loop (all server-authoritative):
 *   Attack drains Balance -> 0 Balance = KnockedDown (ragdoll, auto-recovers)
 *   Viking holds Interact on a knocked-down Pirate -> Restrained
 *   Viking taps Interact on a restrained Pirate -> Carried
 *   Carrier taps Interact on a boat -> prisoner loaded; on a Detention Zone -> Detained
 *   Pirates hold Interact on restrained teammates to cut them loose, or on a cell door to jailbreak
 *   Restrained / carried players mash Jump to struggle free
 */
UCLASS()
class PIRATESVSVIKINGS_API APVVCharacter : public ACharacter, public IPVVInteractable
{
	GENERATED_BODY()

public:
	APVVCharacter();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void FellOutOfWorld(const UDamageType& DmgType) override;

	// IPVVInteractable (others interacting with this character)
	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

	// --- Queries ---------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "PVV")
	EPVVFaction GetFaction() const { return Faction; }
	/** NPCs only: put them on a team (e.g. reinforcement officers). */
	void SetNPCFaction(EPVVFaction NewFaction) { Faction = NewFaction; DefaultFaction = NewFaction; ForceNetUpdate(); }

	UFUNCTION(BlueprintPure, Category = "PVV")
	EPVVCondition GetCondition() const { return Condition; }

	UFUNCTION(BlueprintPure, Category = "PVV")
	APVVCharacter* GetCarriedCharacter() const { return CarriedCharacter; }

	UFUNCTION(BlueprintPure, Category = "PVV")
	APVVCharacter* GetCarrier() const { return CarriedBy; }

	UFUNCTION(BlueprintPure, Category = "PVV")
	APVVBoat* GetCurrentBoat() const { return CurrentBoat; }

	UFUNCTION(BlueprintPure, Category = "PVV")
	float GetBalanceFraction() const { return MaxBalance > 0.f ? Balance / MaxBalance : 0.f; }
	bool IsRagdollActive() const { return bRagdollActive; }
	const FTransform& GetMeshRelativeTransform() const { return MeshRelativeTransform; }

	const FText& GetFocusPrompt() const { return FocusPrompt; }
	/** 0..1 while holding Interact (working a job, cutting wire, zip-tying...). */
	UFUNCTION(BlueprintPure, Category = "PVV|Anim") float GetHoldProgress() const { return HoldProgress; }
	UFUNCTION(BlueprintPure, Category = "PVV|Anim") float GetStruggleProgress() const { return StruggleProgress; }
	UFUNCTION(BlueprintPure, Category = "PVV|Anim") bool IsWading() const { return bIsWading; }

	APVVDetentionZone* GetDetentionZone() const { return DetentionZone; }
	bool IsPlayerCharacter() const { return bIsPlayerCharacter; }
	APVVPlayerState* GetPVVPlayerState() const;
	FString GetPlayerDisplayName() const;

	bool CanAttack() const;
	bool CanMove() const;
	bool IsSprinting() const { return bIsSprinting; }

	// --- Server actions --------------------------------------------------------------------

	void ReceiveMeleeHit(APVVCharacter* Attacker, float Damage, const FVector& Impulse);
	/** Multiplies how far hits and knockdowns throw you (the lucha ring turns it down so you stay inside the ropes). */
	float KnockbackScale = 1.f;
	/** Server: full balance again (candy!). */
	void RestoreBalance() { Balance = MaxBalance; }
	/** Server: drain balance without a hit (chokeholds). Knocks you out at 0. */
	void DrainBalance(APVVCharacter* By, float Amount);
	/** Server: stay down longer (body splash). */
	void ExtendKnockdown(float Seconds);
	/** Knocked down: get up in exactly this many seconds (e.g. a quick throw knockdown). */
	void SetKnockdownTimeLeft(float Seconds);
	/** Knocked down: the body never flies faster than this (cm/s), so you land nearby instead of across the map. */
	UPROPERTY(EditDefaultsOnly, Category = "PVV|Combat") float MaxKnockdownSpeed = 550.f;
	/** Blocking etc: change an incoming melee hit (return the new damage, edit the push). */
	virtual float ModifyIncomingHit(APVVCharacter* Attacker, float Damage, FVector& Impulse) { return Damage; }
	void ApplyKnockdown(AActor* Cause, const FVector& Impulse);
	void Recover(float BalanceFraction = 0.5f);
	void Restrain(APVVCharacter* Captor);
	/** Back to Normal from Restrained. Rescuer null = struggled free. */
	void Release(APVVCharacter* Rescuer);
	/** Back to Normal from Restrained without any announcement (e.g. landing after the catapult). */
	void ClearRestraint();

	void StartCarrying(APVVCharacter* Prisoner);
	/** Pick up anyone (not just zip-tied prisoners) - used for grab & throw. Held in front of you. */
	void Grab(APVVCharacter* Target);
	/** Server: punched while being held by someone (bypasses the usual "carried = untouchable"). */
	void ReceiveHeldPunch(APVVCharacter* Attacker, float Damage);
	/** Detaches the carried prisoner (left where they are) and returns them. */
	APVVCharacter* TakeCarried();
	/** Sets the carried prisoner down in front of us. */
	void DropCarried();

	void Detain(APVVDetentionZone* Zone, const FVector& CellLocation);
	void ReleaseFromDetention(const FVector& Location);

	/** Called by APVVBoat only. */
	void EnterSeat(APVVBoat* Boat, USceneComponent* SeatParent, const FVector& LocalOffset);
	void ExitSeat(const FVector& ExitLocation, float ExitYaw);
	/** Where the body really is (the pelvis while ragdolled - it can slide away from the capsule). */
	FVector GetBodyLocation() const;

	void ResetForRespawn(const FTransform& SpawnTransform);

	// --- Bot controls (server only: computer players press the same "buttons" as people) -----
	void BotAttack();
	/** Start interacting with this thing (hold-to-use things keep going until done or BotStopInteract). */
	bool BotInteractWith(AActor* Target);
	void BotStopInteract() { CancelHold(); }
	void BotStruggle();
	void BotSetSprinting(bool bSprint);
	bool IsBusyInteracting() const { return HoldTarget != nullptr; }
	/** Is this character being held in someone's hands right now (Gang Beasts grab)? Lets Border Patrol zip-tie them while they struggle. */
	virtual bool IsHeldByHands(const APVVCharacter* By) const { return false; }
	/** Server: make this character tougher / hit harder (the luchador). */
	void SetCombatStats(float NewMaxBalance, float NewAttackDamage) { MaxBalance = NewMaxBalance; Balance = NewMaxBalance; AttackDamage = NewAttackDamage; }

protected:
	// --- Components ------------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PVV|Components")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PVV|Components")
	TObjectPtr<UCameraComponent> FollowCamera;

	/** Faction + status label above the head. Prototype readability; hide when real art lands. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PVV|Components")
	TObjectPtr<UTextRenderComponent> StatusLabel;

	/** Shown only when no skeletal mesh is assigned, so the game is playable with zero art. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PVV|Components")
	TObjectPtr<UStaticMeshComponent> PlaceholderBody;

	// --- Input (leave empty to use built-in defaults: WASD/mouse, Space, Shift, LMB, E) ----

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Input")
	TObjectPtr<UInputAction> LookAction;

	/** Also used to struggle while restrained and to leave a boat seat. */
	UPROPERTY(EditDefaultsOnly, Category = "PVV|Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Input")
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Input")
	TObjectPtr<UInputAction> AttackAction;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Input")
	TObjectPtr<UInputAction> InteractAction;

	// --- Tuning ----------------------------------------------------------------------------

	/** Used only before possession (e.g. placed dummies for testing). Players take their PlayerState's faction. */
	UPROPERTY(EditAnywhere, Category = "PVV|Faction")
	EPVVFaction DefaultFaction = EPVVFaction::None;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Movement")
	float WalkSpeed = 500.f;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Movement")
	float SprintSpeed = 800.f;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Movement")
	float SwimSpeed = 400.f;

	/** Speed multiplier while wading through the lake. */
	UPROPERTY(EditDefaultsOnly, Category = "PVV|Movement")
	float WadeSpeedMultiplier = 0.45f;

	/** Zip-tied shuffle. */
	UPROPERTY(EditDefaultsOnly, Category = "PVV|Movement")
	float RestrainedSpeed = 140.f;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Movement")
	float CarrySpeedMultiplier = 0.7f;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Combat")
	float MaxBalance = 100.f;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Combat")
	float BalanceRegenPerSecond = 15.f;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Combat")
	float BalanceRegenDelay = 3.f;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Combat")
	float AttackDamage = 25.f;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Combat")
	float AttackCooldown = 0.55f;

	/** Delay between the swing starting (BP montage) and the hit check. */
	UPROPERTY(EditDefaultsOnly, Category = "PVV|Combat")
	float AttackWindup = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Combat")
	float AttackReach = 90.f;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Combat")
	float AttackRadius = 70.f;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Combat")
	float KnockbackStrength = 650.f;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Combat")
	float KnockbackLift = 250.f;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Combat")
	float KnockdownDuration = 8.f;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Combat")
	bool bRagdollOnKnockdown = true;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Interaction")
	float InteractRadius = 250.f;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Interaction")
	float CaptureDuration = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Interaction")
	float RescueDuration = 3.f;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Interaction")
	float HelpUpDuration = 2.f;

	/** Where a carried prisoner sits relative to the carrier's capsule (over the shoulder). */
	UPROPERTY(EditDefaultsOnly, Category = "PVV|Interaction")
	FVector CarryOffset = FVector(10.f, 0.f, 110.f);

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Interaction")
	FRotator CarryRotation = FRotator(0.f, 90.f, 90.f);

	/** Where someone you grabbed (not zip-tied) dangles: in front of you, facing you. */
	UPROPERTY(EditDefaultsOnly, Category = "PVV|Carry")
	FVector GrabOffset = FVector(80.f, 0.f, 15.f);

	/** Progress added per Jump press while restrained (1 = free). */
	UPROPERTY(EditDefaultsOnly, Category = "PVV|Struggle")
	float StrugglePerPress = 0.07f;

	UPROPERTY(EditDefaultsOnly, Category = "PVV|Struggle")
	float StruggleDecayPerSecond = 0.12f;

	/** Being carried is harder to escape than being left on the ground. */
	UPROPERTY(EditDefaultsOnly, Category = "PVV|Struggle")
	float CarriedStruggleMultiplier = 0.6f;

	// --- Blueprint hooks (cosmetics / UI) --------------------------------------------------

	/** Condition, carry or seat changed. Fires on every machine. */
	UFUNCTION(BlueprintImplementableEvent, Category = "PVV")
	void OnStateChanged();

	/** Play the swing montage. */
	UFUNCTION(BlueprintImplementableEvent, Category = "PVV")
	void OnAttackStarted();

	UFUNCTION(BlueprintImplementableEvent, Category = "PVV")
	void OnHitReceived(APVVCharacter* Attacker);

	/** Local player only: interaction target changed. Use FocusPrompt / HoldProgress for the HUD. */
	UFUNCTION(BlueprintImplementableEvent, Category = "PVV")
	void OnFocusChanged(AActor* NewFocus);

	UPROPERTY(BlueprintReadOnly, Category = "PVV|Interaction")
	TObjectPtr<AActor> FocusedInteractable;

	UPROPERTY(BlueprintReadOnly, Category = "PVV|Interaction")
	FText FocusPrompt;

	/** 0..1 while holding Interact. Owner only. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PVV|Interaction")
	float HoldProgress = 0.f;

	/** 0..1 toward breaking free. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PVV|Struggle")
	float StruggleProgress = 0.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PVV|Combat")
	float Balance = 100.f;

private:
	// --- Replicated state ------------------------------------------------------------------

	UPROPERTY(ReplicatedUsing = OnRep_State)
	EPVVFaction Faction = EPVVFaction::None;

	UPROPERTY(ReplicatedUsing = OnRep_State)
	EPVVCondition Condition = EPVVCondition::Normal;

	UPROPERTY(ReplicatedUsing = OnRep_State)
	TObjectPtr<APVVCharacter> CarriedBy;

	UPROPERTY(ReplicatedUsing = OnRep_State)
	TObjectPtr<APVVCharacter> CarriedCharacter;

	UPROPERTY(ReplicatedUsing = OnRep_State)
	TObjectPtr<APVVBoat> CurrentBoat;

	UPROPERTY(ReplicatedUsing = OnRep_Sprinting)
	bool bIsSprinting = false;

	UPROPERTY(Replicated)
	FVector_NetQuantize LastHitImpulse;

	UPROPERTY()
	TObjectPtr<APVVDetentionZone> DetentionZone;

	UPROPERTY()
	TObjectPtr<AActor> HoldTarget;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PlaceholderMID;

	TWeakObjectPtr<APVVPlayerState> CachedPlayerState;
	TWeakObjectPtr<APVVLake> CachedLake;
	float NextLakeSearchTime = 0.f;
	bool bIsWading = false;
	bool bIsPlayerCharacter = false;
	bool bRagdollActive = false;
	FTransform MeshRelativeTransform;
	float HoldElapsed = 0.f;
	float HoldDuration = 0.f;
	float LastAttackTime = -100.f;
	float LastDamageTime = -100.f;
	float LastStruggleTime = -100.f;
	FTimerHandle KnockdownTimer;
	FTimerHandle AttackTimer;

	UFUNCTION()
	void OnRep_State();

	UFUNCTION()
	void OnRep_Sprinting();

	void SetRagdoll(bool bEnable);
	void TickRagdollCapsule();
	void UpdateStatusLabel();
	void UpdateWading();
	AActor* FindBestInteractable();
	void UpdateFocus();
	void TickHold(float DeltaSeconds);
	void PerformAttackSweep();
	void BreakFree();
	void ReleaseAllAttachments();

protected:
	void RefreshStateEffects();
	/** Called at the end of RefreshStateEffects on every machine. */
	virtual void OnStateEffectsRefreshed() {}
	/** Runs on every machine when this character starts a punch (before the Blueprint OnAttackStarted event). */
	virtual void OnAttackStartedNative() {}
	/** Runs on every machine when this character gets hit (before the Blueprint OnHitReceived event). */
	virtual void OnHitReceivedNative(APVVCharacter* Attacker) {}
	/** Left-click punches unless a subclass wants it for something else (e.g. firing while aiming). */
	virtual bool WantsMeleeOnAttack() const { return true; }
	virtual void UpdateMovementSpeed();
	virtual void EnsureDefaultInput();
	/** Return true to use the Jump key for something else this time (e.g. leaping off a friend's shoulders). */
	virtual bool HandleJumpPressed() { return false; }
	/** Return true if this character deals punch damage itself (physics fists) instead of the invisible sphere in front. */
	virtual bool HandlePunchHit() { return false; }
	/** Damage taken from hits is multiplied by this (e.g. a hard hat). */
	virtual float GetDamageTakenMultiplier() const { return 1.f; }
	/** Allow punching while holding someone (Gang Beasts style hold + punch). */
	virtual bool CanAttackWhileCarrying() const { return false; }
	void CancelHold();
	void SetState(EPVVCondition NewCondition);

private:

	// Input handlers
	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_Jump();
	void Input_StopJump();
	void Input_SprintStart();
	void Input_SprintStop();
	void Input_Attack();
	void Input_InteractStart();
	void Input_InteractStop();

	// RPCs
	UFUNCTION(Server, Reliable)
	void Server_Attack();

	UFUNCTION(Server, Reliable)
	void Server_InteractStart();

	UFUNCTION(Server, Reliable)
	void Server_InteractStop();

	UFUNCTION(Server, Reliable)
	void Server_SetSprinting(bool bNewSprinting);

	UFUNCTION(Server, Unreliable)
	void Server_Struggle();

	UFUNCTION(Server, Reliable)
	void Server_LeaveSeat();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_AttackStarted();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_HitReceived(APVVCharacter* Attacker);
};
