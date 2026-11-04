#pragma once

#include "CoreMinimal.h"
#include "PVVBoat.h"
#include "BRTypes.h"
#include "BRCar.generated.h"

class APlayerState;
class ABRCharacter;
class UAudioComponent;
class UInputAction;
class USoundBase;

/**
 * Arcade hover-vehicle: reuses the boat physics but "floats" on the ground (line traces).
 * Key + fuel for the Runners' car, free unlimited trucks for Wardens.
 * Radio: R = next station, T = previous, Backspace = off, H = horn.
 *   Stations = every song in Content/Music/Radio (plus any in the Stations array).
 */
UCLASS()
class PIRATESVSVIKINGS_API ABRCar : public APVVBoat
{
	GENERATED_BODY()

public:
	ABRCar();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	virtual bool GetSurfaceZ(const FVector& Point, float& OutZ) const override;
	virtual float GetDriveScale() const override;
	virtual bool CanDrive(const APVVCharacter* Character) const override;

	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BR|Car") bool bRequiresKey = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BR|Car") bool bInfiniteFuel = false;
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "BR|Car") FLinearColor PaintColor = FLinearColor(0.15f, 0.3f, 0.7f);
	/** Car lot: nobody can drive it until somebody buys it (E, hold) for SalePrice. */
	UPROPERTY(ReplicatedUsing = OnRep_ForSale, EditAnywhere, BlueprintReadWrite, Category = "BR|Car") bool bForSale = false;
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "BR|Car") int32 SalePrice = 500;
	/** Whoever bought it (they and their teammates can drive it). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR|Car") TObjectPtr<APlayerState> OwnerPlayer;
	virtual FString GetModelName() const { return TEXT("Car"); }
	/** Server: Border Patrol orders the suspects out of this vehicle (O key, or hold E). True if it worked. */
	bool OrderOut(APVVCharacter* Officer);
	/** Can this officer order people out of it right now? */
	bool CanOrderOut(const APVVCharacter* Officer) const { return WantsSearch(Officer); }
	/** A zip-tied prisoner this officer could pull out of the car (O key). */
	bool HasPrisonerFor(const APVVCharacter* Officer) const { return FindPrisonerToUnload(Officer) != nullptr; }
	/** Server: pull a zip-tied prisoner out onto the officer's shoulder. */
	bool TakePrisonerOut(APVVCharacter* Officer);
	/** Anyone aboard who isn't zip-tied (and isn't you)? */
	bool HasFreePeopleAboard(const APVVCharacter* Except) const;
	/** Contraband hidden in the vehicle (some NPC trucks): Border Patrol finds it by searching the empty vehicle (hold E). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BR|Car") bool bHiddenCargo = false;
	bool bCargoSearched = false;
	bool WantsCargoSearch(const APVVCharacter* Interactor) const;
	/** Spike strip: flat tyres for a while (crawls along). */
	void PopTyres(float Seconds);
	bool HasFlatTyres() const;
	UPROPERTY(Replicated) float FlatTyresUntil = 0.f;
	/** Server: beep beep. */
	void HonkHorn() { Multicast_Horn(); }
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR|Car") float Fuel = 0.f;
	UPROPERTY(EditAnywhere, Category = "BR|Car") float FuelPerSecond = 1.f / 60.f;
	UPROPERTY(EditAnywhere, Category = "BR|Car") float RideHeight = 45.f;
	/** Hit someone faster than this (cm/s) and you RUN THEM OVER: they get knocked flying (never killed). */
	UPROPERTY(EditAnywhere, Category = "BR|Car") float RunOverSpeed = 450.f;
	/** How far people get thrown when run over (x car speed). */
	UPROPERTY(EditAnywhere, Category = "BR|Car") float RunOverLaunch = 0.55f;

	/** Extra radio songs (on top of everything in Content/Music/Radio). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Radio") TArray<TObjectPtr<USoundBase>> Stations;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Radio") TObjectPtr<USoundBase> HornSound;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Radio") float RadioVolume = 0.8f;

	bool IsRadioOn() const { return RadioStation >= 0 && LoadedStations.IsValidIndex(RadioStation); }
	FString GetStationName() const;
	int32 GetStationCount() const { return LoadedStations.Num(); }
	bool IsFlooded() const;

	/** Server: tune to a station (-1 = off). */
	void SetRadioStation(int32 Station);

protected:
	virtual void EnsureDefaultInput() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BR|Radio") TObjectPtr<UAudioComponent> RadioAudio;
	/** "FOR SALE $500" sign on the roof. */
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UTextRenderComponent> PriceTag;
	UFUNCTION() void OnRep_ForSale();
	bool WantsBuy(const APVVCharacter* Interactor) const;

	UPROPERTY() TObjectPtr<UInputAction> RadioNextAction;
	UPROPERTY() TObjectPtr<UInputAction> RadioPrevAction;
	UPROPERTY() TObjectPtr<UInputAction> RadioOffAction;
	UPROPERTY() TObjectPtr<UInputAction> HornAction;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Radio) int32 RadioStation = -1;
	UPROPERTY(Replicated) float RadioStartTime = 0.f;
	UPROPERTY() TArray<TObjectPtr<USoundBase>> LoadedStations;

	float LastHornTime = -100.f;
	TMap<TWeakObjectPtr<APVVCharacter>, float> LastRunOver;
	void TickRunOver();
	mutable TWeakObjectPtr<class ABRDesertArena> CachedArena;

	UFUNCTION() void OnRep_Radio();
	UFUNCTION() void OnRadioFinished();
	bool WantsRefuel(const APVVCharacter* Interactor) const;
	/** Driver's seat is free and you're allowed to drive: E = get in and drive (beats every other prompt). */
	bool WantsDrive(const APVVCharacter* Interactor) const;
	bool WantsSearch(const APVVCharacter* Interactor) const;
	/** A zip-tied passenger a Border Patrol agent standing next to the truck can take out. */
	APVVCharacter* FindPrisonerToUnload(const APVVCharacter* Interactor) const;

	void Input_RadioNext();
	void Input_RadioPrev();
	void Input_RadioOff();
	void Input_Horn();

	UFUNCTION(Server, Reliable) void Server_ChangeStation(int32 Delta, bool bTurnOff);
	UFUNCTION(Server, Unreliable) void Server_Horn();
	UFUNCTION(NetMulticast, Unreliable) void Multicast_Horn();
};

/**
 * A pickup truck you can actually see and sit in: open cab (driver + passenger) and a bed (2 more).
 * NPC trucks drive the road through the checkpoint on their own with 1-4 look-alike NPCs aboard.
 */
UCLASS()
class PIRATESVSVIKINGS_API ABRTruck : public ABRCar
{
	GENERATED_BODY()

public:
	ABRTruck();

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual FString GetModelName() const override;

	/** Body style: set it before it spawns (or in your truck Blueprint). Changes seats, speed and looks. */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "BR|Truck") EBRCarModel Model = EBRCarModel::Pickup;

	/** Server: make this an NPC truck following Route (world points). CheckpointIndex = where it waits for the gate. */
	void StartNPCRoute(const TArray<FVector>& InRoute, int32 InCheckpointIndex, AActor* InGate);
	/** Server: put a spawned NPC in the next free seat (driver first). */
	bool AddNPC(APVVCharacter* NPC);

	bool IsNPCTruck() const { return bNPCDriven; }
	/** Server: the driver floors it - smashes the checkpoint barrier and races off. */
	void FloorIt();
	/** Server: Border Patrol just scanned someone aboard as illegal / carrying contraband: they react in a moment. */
	void OnSuspectScanned();
	/** Server: a passenger who stepped out to be searched climbs back into their old seat (or any free one). */
	void ReturnFromSearch(APVVCharacter* Passenger, int32 Seat);

protected:
	/** Steering wheel in front of the driver. Move it in your truck Blueprint so it lines up with your driving animation's hands. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BR|Truck") TObjectPtr<USceneComponent> SteeringWheel;
	/** NPC trucks: seconds they wait at the checkpoint for Border Patrol to scan everyone before pushing through (approval hit). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Truck") float MaxCheckpointWait = 120.f;
	/** How far the wheel turns at full steer (degrees). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Truck") float SteeringWheelTurn = 120.f;
	/** NPC trucks drive in two lanes this far either side of the road's middle (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Truck") float LaneOffset = 320.f;
	/** Seconds an NPC truck with nobody driving it sits there before it's towed away. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Truck") float TowAfterSeconds = 10.f;

private:
	bool bNPCDriven = false;
	TArray<FVector> Route;
	int32 RouteIndex = 0;
	int32 CheckpointIndex = INDEX_NONE;
	float CheckpointArrivalTime = -1.f;
	bool bCheckpointDone = false;
	/** Made it all the way to the US side (anyone illegal aboard scores for the Runners). */
	bool bReachedFarSide = false;
	float DriverSpeedTaste = 1.f;
	float DriverWobble = 0.f;
	float DriverWobbleRate = 0.7f;
	float DriverPhase = 0.f;
	float NoDriverSince = -1.f;
	/** Driver got pulled out: somebody else aboard takes the wheel (or bails). */
	void TickDriverTakeover(float Now);
	TWeakObjectPtr<AActor> Gate;
	TArray<TWeakObjectPtr<APVVCharacter>> SpawnedNPCs;
	/** -1 = right lane, +1 = left lane. */
	float Lane = -1.f;
	float BlockedSince = -1.f;
	float NextHonk = 0.f;
	float ReactAt = -1.f;
	bool bReacted = false;
	/** Checkpoint: everybody got out to be searched; the truck waits until they're all back in (or gone). */
	bool bSteppedOut = false;
	TArray<TWeakObjectPtr<APVVCharacter>> WaitingOutside;
	/** When the "fed up waiting" clock started (restarts once everybody is back in after a search). */
	float FedUpFrom = -1.f;
	void StepOutForSearch(ABRCharacter* Inspector);
	bool bRunningGate = false;

	void DriveNPC(float DeltaSeconds);
	/** Show/hide the body parts and set seats + handling for Model. */
	void ApplyModel();
	FVector LaneTarget(int32 Index, float InLane) const;
	/** Another vehicle in my lane ahead within Range? */
	bool IsLaneBlocked(float InLane, float Range, float BehindToo) const;
	/** Suspects aboard got caught out: floor it, bail and run, or come out swinging. */
	void SuspectsReact();
};
