#pragma once

#include "CoreMinimal.h"
#include "PVVLake.h"
#include "BRDesertArena.generated.h"

class ABRCatapult;
class ABRGameMode;
class UBoxComponent;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UTextRenderComponent;

/**
 * Self-building desert greybox for Border Run (X runs west -> east):
 *
 *   TOWN (Runners) | THE LINE | razor wire | RIVER | THE WALL | WARDEN BASE + pen + catapult | SAFE ZONE
 *   -16000 .. -5000   -5000     -3500     -1500..1500  +3500        +4500 .. +9000              +14000..
 *
 * The lake surface (river) is at the actor's Z; the desert floor is 100 cm above it.
 */
UCLASS()
class PIRATESVSVIKINGS_API ABRDesertArena : public APVVLake
{
	GENERATED_BODY()

public:
	ABRDesertArena();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Server: shops, fences, wall, catapult, pen, cars, player starts. */
	void SpawnGameplayActors();

	bool IsInRiver(const FVector& WorldLocation) const;
	FVector GetRiverFlowDirection() const { return FVector(0.f, 1.f, 0.f); }
	float GetLineX() const { return GetActorLocation().X + LineX; }
	float GetSafeX() const { return GetActorLocation().X + SafeX; }
	FVector GetCatapultLanding() const;
	ABRCatapult* GetCatapult() const { return Catapult; }

	static constexpr float Ground = 100.f;
	static constexpr float HalfY = 8000.f;
	static constexpr float HalfX = 16000.f;
	static constexpr float RiverHalfWidth = 1500.f;
	static constexpr float LineX = -5000.f;
	static constexpr float FenceX = -3500.f;
	static constexpr float WallX = 3500.f;
	static constexpr float SafeX = 14000.f;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> RiverBed;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UDirectionalLightComponent> Sun;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UDirectionalLightComponent> Moon;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USkyLightComponent> SkyLight;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USkyAtmosphereComponent> Atmosphere;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UExponentialHeightFogComponent> Fog;
	UPROPERTY() TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY() TMap<FName, FLinearColor> BlockColors;
	UPROPERTY() TObjectPtr<ABRCatapult> Catapult;

private:
	float DisplayedTime = -1.f;
	float FogDensity = 0.002f;

	UStaticMeshComponent* AddBlock(const TCHAR* Name, const FVector& Center, const FVector& SizeCm, const FLinearColor& Color, const FRotator& Rotation = FRotator::ZeroRotator, bool bCollide = true);
	UTextRenderComponent* AddSign(const TCHAR* Name, const FString& Text, const FVector& Location, float Yaw, float Size, const FColor& Color);
	void AddHut(const TCHAR* Prefix, const FVector& Center, const FVector& Size, const FLinearColor& Color);
};
