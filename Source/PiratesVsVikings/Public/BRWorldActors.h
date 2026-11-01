#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PVVInteractable.h"
#include "BRTypes.h"
#include "BRWorldActors.generated.h"

class APVVCharacter;
class UBoxComponent;
class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UStaticMesh;

/** Shared helpers for the greybox world pieces. */
UCLASS(Abstract)
class PIRATESVSVIKINGS_API ABRGreyboxActor : public AActor
{
	GENERATED_BODY()

public:
	ABRGreyboxActor();
	virtual void BeginPlay() override;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	/** Colour per mesh component name, applied at BeginPlay on every machine. */
	UPROPERTY() TMap<FName, FLinearColor> ComponentColors;
	UPROPERTY() TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> ConeMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> CylinderMesh;

	UStaticMeshComponent* AddMesh(const TCHAR* Name, UStaticMesh* Mesh, const FVector& Location, const FVector& SizeCm, const FLinearColor& Color,
		const FRotator& Rotation = FRotator::ZeroRotator, bool bCollide = true);
	UTextRenderComponent* AddLabel(const TCHAR* Name, const FVector& Location, float Size, const FColor& Color);
	static void Paint(UStaticMeshComponent* Mesh, const FLinearColor& Color);
};

// ===========================================================================================
/** A counter or work spot in town: buy, sell, buy a boat, or work a job for cash. Runners only. */
UCLASS()
class PIRATESVSVIKINGS_API ABRStation : public ABRGreyboxActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRStation();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

	// Set before FinishSpawning (replicated so clients can show prompts).
	UPROPERTY(Replicated) EBRStationKind Kind = EBRStationKind::BuyItem;
	UPROPERTY(Replicated) EBRItem Item = EBRItem::None;
	UPROPERTY(Replicated) int32 Price = 10;
	UPROPERTY(Replicated) float WorkSeconds = 0.f;
	UPROPERTY(Replicated) float CooldownSeconds = 0.f;
	UPROPERTY(Replicated) bool bNightOnly = false;
	UPROPERTY(Replicated) FString Title;
	UPROPERTY(Replicated) FLinearColor Color = FLinearColor(0.6f, 0.6f, 0.6f);
	FVector BoatSpawn = FVector::ZeroVector;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Counter;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Label;

private:
	UPROPERTY(Replicated) float NextUseServerTime = 0.f;
	bool IsOpen() const;
};

// ===========================================================================================
/** 4 m razor-wire fence segment. Touching it hurts. Runners with wire cutters can cut it (loud). */
UCLASS()
class PIRATESVSVIKINGS_API ABRSpikeFence : public ABRGreyboxActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRSpikeFence();
	virtual void BeginPlay() override;

	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override { return 4.f; }
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> HurtZone;

	UFUNCTION()
	void OnHurtOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};

// ===========================================================================================
/** The Wall. Runners holding a ladder can lean one against it. */
UCLASS()
class PIRATESVSVIKINGS_API ABRWall : public ABRGreyboxActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRWall();

	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override { return 2.f; }
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

	static constexpr float Height = 600.f;
	static constexpr float HalfLength = 8100.f;
};

// ===========================================================================================
/** A ladder leaning over the wall. Runners climb it; Wardens kick it down. */
UCLASS()
class PIRATESVSVIKINGS_API ABRLadder : public ABRGreyboxActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRLadder();

	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> UseZone;
};

// ===========================================================================================
/** The Line. A solid invisible barrier during Prep; open during the Invasion. */
UCLASS()
class PIRATESVSVIKINGS_API ABRLineBarrier : public ABRGreyboxActor
{
	GENERATED_BODY()

public:
	ABRLineBarrier();
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Barrier;

private:
	int32 AppliedState = -1;
};

// ===========================================================================================
/** Launches released detainees back over the wall. No damage, maximum comedy. */
UCLASS()
class PIRATESVSVIKINGS_API ABRCatapult : public ABRGreyboxActor
{
	GENERATED_BODY()

public:
	ABRCatapult();
	virtual void Tick(float DeltaSeconds) override;

	/** Server: fling this detainee at Target. */
	void Launch(APVVCharacter* Passenger, const FVector& Target);

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> ArmPivot;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Basket;

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Fling();

private:
	float FlingTime = -100.f;
};
