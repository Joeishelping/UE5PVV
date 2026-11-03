#pragma once

#include "CoreMinimal.h"

class USoundBase;
class USoundAttenuation;

/**
 * "Plug in your own music": import songs (.wav) into these Content folders and the game finds them.
 *   Content/Music/Radio    -> truck radio stations (one song = one station)
 *   Content/Music/Ambient  -> background music
 *
 * 3D sounds: everything in the world plays through PlayAt, which fades the sound out with distance
 * (full volume close up, silent at Range). So you hear what's near you, not every NPC on the map.
 */
namespace BRAudio
{
	/** All sounds under a content path (e.g. "/Game/Music/Radio"), sorted by name so every machine agrees on the order. */
	PIRATESVSVIKINGS_API void LoadSounds(const FName& ContentPath, TArray<USoundBase*>& OutSounds);

	/** Typical hearing distances (cm). */
	constexpr float Quiet = 1200.f;		// footsteps, reloads, swings
	constexpr float Near = 2000.f;		// punches, grunts, clucks, drinks
	constexpr float Medium = 3500.f;	// trumpet, barks, game sounds (cash, zip ties...)
	constexpr float Loud = 6000.f;		// horns, gunshots, crashes, catapult
	constexpr float Huge = 15000.f;		// cannon boom, thunder-ish

	/** Shared "fade out over Range" settings (one per distance, made once). */
	PIRATESVSVIKINGS_API USoundAttenuation* Falloff(float Range);
	/** Play a sound in the world that fades with distance. No-op on a dedicated server. */
	PIRATESVSVIKINGS_API void PlayAt(const UObject* WorldContext, USoundBase* Sound, const FVector& Where, float Range = Medium, float Volume = 1.f, float Pitch = 1.f);
	/** Where the local player hears from (camera / your character). False on a dedicated server. */
	PIRATESVSVIKINGS_API bool GetListener(const UObject* WorldContext, FVector& OutLocation);
}
