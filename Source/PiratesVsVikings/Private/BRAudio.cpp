#include "BRAudio.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Modules/ModuleManager.h"
#include "Sound/SoundBase.h"

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
