#pragma once

#include "CoreMinimal.h"
#include "PVVBoat.h"
#include "BRCar.generated.h"

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BR|Car") FLinearColor PaintColor = FLinearColor(0.15f, 0.3f, 0.7f);
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR|Car") float Fuel = 0.f;
	UPROPERTY(EditAnywhere, Category = "BR|Car") float FuelPerSecond = 1.f / 60.f;
	UPROPERTY(EditAnywhere, Category = "BR|Car") float RideHeight = 45.f;

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

	UPROPERTY() TObjectPtr<UInputAction> RadioNextAction;
	UPROPERTY() TObjectPtr<UInputAction> RadioPrevAction;
	UPROPERTY() TObjectPtr<UInputAction> RadioOffAction;
	UPROPERTY() TObjectPtr<UInputAction> HornAction;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Radio) int32 RadioStation = -1;
	UPROPERTY(Replicated) float RadioStartTime = 0.f;
	UPROPERTY() TArray<TObjectPtr<USoundBase>> LoadedStations;

	float LastHornTime = -100.f;
	mutable TWeakObjectPtr<class ABRDesertArena> CachedArena;

	UFUNCTION() void OnRep_Radio();
	UFUNCTION() void OnRadioFinished();
	bool WantsRefuel(const APVVCharacter* Interactor) const;
	bool WantsSearch(const APVVCharacter* Interactor) const;

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

	/** Server: make this an NPC truck following Route (world points). CheckpointIndex = where it waits for the gate. */
	void StartNPCRoute(const TArray<FVector>& InRoute, int32 InCheckpointIndex, AActor* InGate);
	/** Server: put a spawned NPC in the next free seat (driver first). */
	bool AddNPC(APVVCharacter* NPC);

	bool IsNPCTruck() const { return bNPCDriven; }

protected:
	/** Steering wheel in front of the driver. Move it in your truck Blueprint so it lines up with your driving animation's hands. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BR|Truck") TObjectPtr<USceneComponent> SteeringWheel;
	/** NPC trucks: seconds they wait at the checkpoint for Border Patrol to scan everyone before pushing through (approval hit). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Truck") float MaxCheckpointWait = 120.f;
	/** How far the wheel turns at full steer (degrees). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Truck") float SteeringWheelTurn = 120.f;

private:
	bool bNPCDriven = false;
	TArray<FVector> Route;
	int32 RouteIndex = 0;
	int32 CheckpointIndex = INDEX_NONE;
	float CheckpointArrivalTime = -1.f;
	bool bCheckpointDone = false;
	float NoDriverSince = -1.f;
	TWeakObjectPtr<AActor> Gate;
	TArray<TWeakObjectPtr<APVVCharacter>> SpawnedNPCs;

	void DriveNPC(float DeltaSeconds);
};
