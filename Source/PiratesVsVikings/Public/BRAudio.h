#pragma once

#include "CoreMinimal.h"

class USoundBase;

/**
 * "Plug in your own music": import songs (.wav) into these Content folders and the game finds them.
 *   Content/Music/Radio    -> truck radio stations (one song = one station)
 *   Content/Music/Ambient  -> background music
 */
namespace BRAudio
{
	/** All sounds under a content path (e.g. "/Game/Music/Radio"), sorted by name so every machine agrees on the order. */
	PIRATESVSVIKINGS_API void LoadSounds(const FName& ContentPath, TArray<USoundBase*>& OutSounds);
}
