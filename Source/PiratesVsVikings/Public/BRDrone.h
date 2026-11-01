#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "BRDrone.generated.h"

class ABRCharacter;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class USphereComponent;
class UStaticMeshComponent;
struct FInputActionValue;

/**
 * Warden spy drone. The pilot's body stays behind (and can be jumped!).
 * WASD fly, Space/Ctrl up/down, mouse look, Left click = tag a Runner (visible to all Wardens for 10 s), Q = return.
 * Movement is server-driven (the client sends its wish direction) so everybody sees the same drone.
 */
UCLASS()
class PIRATESVSVIKINGS_API ABRDrone : public APawn
{
	GENERATED_BODY()

public:
	ABRDrone();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR|Drone")
	TObjectPtr<ABRCharacter> Pilot;

	/** 0..1 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "BR|Drone")
	float Battery = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "BR|Drone") float BatterySeconds = 90.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Drone") float MaxSpeed = 1600.f;
	UPROPERTY(EditDefaultsOnly, Category = "BR|Drone") float TagRange = 9000.f;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;

	UPROPERTY() TObjectPtr<UInputMappingContext> DroneMappingContext;
	UPROPERTY() TObjectPtr<UInputAction> MoveAction;
	UPROPERTY() TObjectPtr<UInputAction> LiftAction;
	UPROPERTY() TObjectPtr<UInputAction> LookAction;
	UPROPERTY() TObjectPtr<UInputAction> TagAction;
	UPROPERTY() TObjectPtr<UInputAction> ExitAction;

private:
	FVector2D MoveInput = FVector2D::ZeroVector;
	float LiftInput = 0.f;
	FVector SentWish = FVector::ZeroVector;
	FVector ServerWish = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	bool bExiting = false;

	void EnsureInput();
	void SendWish();
	void Exit();

	void Input_Move(const FInputActionValue& Value);
	void Input_MoveStop();
	void Input_Lift(const FInputActionValue& Value);
	void Input_LiftStop();
	void Input_Look(const FInputActionValue& Value);
	void Input_Tag();
	void Input_Exit();

	UFUNCTION(Server, Unreliable) void Server_SetWish(FVector_NetQuantizeNormal Wish);
	UFUNCTION(Server, Reliable) void Server_Tag(FVector_NetQuantize Start, FVector_NetQuantizeNormal Direction);
	UFUNCTION(Server, Reliable) void Server_Exit();
};
