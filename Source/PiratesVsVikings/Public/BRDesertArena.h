#pragma once

#include "CoreMinimal.h"
#include "PVVLake.h"
#include "BRDesertArena.generated.h"

class ABRCatapult;
class ABRCockfightPit;
class ABRVillage;
class ABRCheckpointGate;
class ABRDog;
class ABRTruck;
class UMaterialInterface;
class ABRGameMode;
class UAudioComponent;
class USoundBase;
class UBoxComponent;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UTextRenderComponent;
class UPostProcessComponent;
class UPointLightComponent;
class UInstancedStaticMeshComponent;
class UNiagaraComponent;

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

	/**
	 * On = build the built-in test map (desert, river, wall, town...).
	 * Off = only the sky, day/night, weather and ambient music (for your own map built with the Map Kit).
	 */
	UPROPERTY(EditAnywhere, Category = "BR|Map") bool bBuildTestMap = true;

	/** The built-in river only counts while the test map is on. */
	virtual bool ContainsPoint(const FVector& WorldLocation) const override { return bBuildTestMap; }

	/** Server: shops, fences, wall, catapult, pen, cars, player starts. */
	void SpawnGameplayActors(TSubclassOf<ABRTruck> RunnerTruckClass, TSubclassOf<ABRTruck> WardenTruckClass, TSubclassOf<ABRDog> DogClass);

	bool IsInRiver(const FVector& WorldLocation) const;
	/** Middle of the river: you can't touch the bottom (drowning), and vehicles flood. Not on the bridge. */
	bool IsDeepWater(const FVector& WorldLocation) const;
	/** Road route for NPC traffic (town -> checkpoint -> far side). */
	void GetNPCRoute(TArray<FVector>& OutRoute, int32& OutCheckpointIndex) const;
	FTransform GetNPCSpawn() const;
	ABRCheckpointGate* GetGate() const { return Gate; }
	void GetDogHomes(TArray<FVector>& OutHomes) const;
	FVector GetRiverFlowDirection() const { return FVector(0.f, 1.f, 0.f); }
	float GetLineX() const { return GetActorLocation().X + LineX; }
	float GetSafeX() const { return GetActorLocation().X + SafeX; }
	/** The Runner town side (west of the Line). */
	bool IsInTown(const FVector& WorldLocation) const { const FVector L = WorldLocation - GetActorLocation(); return L.X < LineX && FMath::Abs(L.Y) < HalfY; }
	FVector GetCatapultLanding() const;
	/** Places in town where NPC villagers hang out. */
	void GetVillageSpots(TArray<FVector>& OutSpots) const;

	/** Your cockfight pit Blueprint (to give the roosters a real model). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Your Blueprints") TSubclassOf<ABRCockfightPit> CockfightPitClass;
	/** Your village Blueprint (to recolour houses etc.). */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Your Blueprints") TSubclassOf<ABRVillage> VillageClass;
	/** Houses, church, plaza, market, lamps, chickens in the Runner town. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Your Blueprints") bool bBuildVillage = true;
	ABRCatapult* GetCatapult() const { return Catapult; }

	static constexpr float Ground = 100.f;
	static constexpr float HalfY = 8000.f;
	static constexpr float HalfX = 16000.f;
	static constexpr float RiverHalfWidth = 1500.f;
	static constexpr float LineX = -5000.f;
	static constexpr float FenceX = -3500.f;
	static constexpr float WallX = 3500.f;
	/** The far (Border Patrol) side runs further east than the town side: a long road to the safe zone. */
	static constexpr float EastX = 26000.f;
	static constexpr float SafeX = 23000.f;
	static constexpr float RoadY = -1200.f;
	/** Half width of the big two-lane road from the edge of town to the safe zone (the town street stays narrow). */
	static constexpr float RoadHalfWidth = 700.f;
	/** Where the town street ends and the wide road starts (NPC trucks start here). */
	static constexpr float TownEdgeX = -6000.f;
	static constexpr float RiverBedZ = -150.f;
	static constexpr float DeepHalfWidth = 1100.f;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> RiverBed;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UDirectionalLightComponent> Sun;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UDirectionalLightComponent> Moon;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USkyLightComponent> SkyLight;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USkyAtmosphereComponent> Atmosphere;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UExponentialHeightFogComponent> Fog;
	UPROPERTY() TObjectPtr<UStaticMesh> CubeMesh;
	/** Colour of every map block, by name (Sand_West, ToolShop_Back, Road_Strip...). Edit in a Blueprint child of this class. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Look") TMap<FName, FLinearColor> BlockColors;
	/** Put a real material (textured sand, wood, concrete...) on any block by name. Wins over BlockColors. */
	UPROPERTY(EditDefaultsOnly, Category = "BR|Look") TMap<FName, TObjectPtr<UMaterialInterface>> BlockMaterials;
	UPROPERTY() TObjectPtr<ABRCatapult> Catapult;
	UPROPERTY() TObjectPtr<ABRCheckpointGate> Gate;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UAudioComponent> AmbientMusic;
	UPROPERTY() TArray<TObjectPtr<USoundBase>> AmbientTracks;
	UPROPERTY(EditAnywhere, Category = "BR|Music") float AmbientVolume = 0.45f;
	/** Night look (darker, bluer) + weather tint. */
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPostProcessComponent> Grade;
	UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> StreetLights;
	/** Built-in rain streaks and blowing sand (follow your camera). Replaced by your Niagara effects if you set them. */
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> RainDrops;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> DustBits;
	UPROPERTY() TObjectPtr<UNiagaraComponent> RainFX;
	UPROPERTY() TObjectPtr<UNiagaraComponent> SandFX;
	UPROPERTY() TObjectPtr<UAudioComponent> RainAudio;
	UPROPERTY() TObjectPtr<UAudioComponent> WindAudio;

private:
	bool bGameplaySpawned = false;
	float DisplayedTime = -1.f;
	int32 AmbientIndex = -1;

	UFUNCTION() void PlayNextAmbient();
	float FogDensity = 0.002f;
	float RainAmount = 0.f;
	float SandAmount = 0.f;
	float LightningFlash = 0.f;
	float NextLightning = 10.f;
	float ThunderAt = -1.f;
	bool bLightsOn = false;
	TArray<FVector> DropPos;
	TArray<FVector> DustPos;
	void TickSkyAndWeather(float DeltaSeconds, const class ABRGameState* GS);
	void TickParticles(float DeltaSeconds);

	UStaticMeshComponent* AddBlock(const TCHAR* Name, const FVector& Center, const FVector& SizeCm, const FLinearColor& Color, const FRotator& Rotation = FRotator::ZeroRotator, bool bCollide = true);
	UTextRenderComponent* AddSign(const TCHAR* Name, const FString& Text, const FVector& Location, float Yaw, float Size, const FColor& Color);
	void AddHut(const TCHAR* Prefix, const FVector& Center, const FVector& Size, const FLinearColor& Color);
};
