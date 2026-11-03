#include "BRAudio.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Modules/ModuleManager.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "UObject/Package.h"

void BRAudio::LoadSounds(const FName& ContentPath, TArray<USoundBase*>& OutSounds)
{
	OutSounds.Reset();
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(ContentPath, Assets, true);
	Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.LexicalLess(B.AssetName); });

	for (const FAssetData& Asset : Assets)
	{
		if (USoundBase* Sound = Cast<USoundBase>(Asset.GetAsset()))
		{
			OutSounds.Add(Sound);
		}
	}
}

USoundAttenuation* BRAudio::Falloff(float Range)
{
	static TMap<int32, TObjectPtr<USoundAttenuation>> Cache;
	const int32 Key = FMath::RoundToInt(FMath::Max(100.f, Range) / 100.f);
	if (const TObjectPtr<USoundAttenuation>* Found = Cache.Find(Key))
	{
		if (IsValid(Found->Get()))
		{
			return Found->Get();
		}
	}
	USoundAttenuation* Att = NewObject<USoundAttenuation>(GetTransientPackage(), NAME_None, RF_Transient);
	Att->AddToRoot(); // shared for the whole session
	const float R = Key * 100.f;
	Att->Attenuation.bAttenuate = true;
	Att->Attenuation.bSpatialize = true;
	Att->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
	Att->Attenuation.AttenuationShapeExtents = FVector(R * 0.12f, 0.f, 0.f); // full volume this close
	Att->Attenuation.FalloffDistance = R * 0.88f;                           // then fades to silence
	Att->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	Att->Attenuation.dBAttenuationAtMax = -60.f;
	Cache.Add(Key, Att);
	return Att;
}

void BRAudio::PlayAt(const UObject* WorldContext, USoundBase* Sound, const FVector& Where, float Range, float Volume, float Pitch)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!Sound || !World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// Way out of earshot: don't even start it (saves voices when 30 NPCs are walking around).
	FVector Ear;
	if (GetListener(WorldContext, Ear) && FVector::DistSquared(Ear, Where) > FMath::Square(Range * 1.05f))
	{
		return;
	}
	UGameplayStatics::PlaySoundAtLocation(WorldContext, Sound, Where, FRotator::ZeroRotator, Volume, Pitch, 0.f, Falloff(Range));
}

bool BRAudio::GetListener(const UObject* WorldContext, FVector& OutLocation)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}
	if (const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(WorldContext, 0))
	{
		OutLocation = Cam->GetCameraLocation();
		if (const APawn* Me = UGameplayStatics::GetPlayerPawn(WorldContext, 0))
		{
			OutLocation = Me->GetActorLocation(); // third-person: hear from your character, not the camera boom
		}
		return true;
	}
	return false;
}
