#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BRReplay.generated.h"

class ABRCharacter;
class ACameraActor;
class UPoseableMeshComponent;
class USkeletalMesh;
class UMaterialInterface;

/**
 * "PLAY OF THE ROUND" (runs on each screen by itself, nothing extra is sent over the network).
 * Keeps the last few seconds of everyone's body poses. When the server reports a highlight
 * (cannon launch, catapult, throw, lasso, big knockout...), it saves the best one, and when the
 * round ends it replays it in slow motion with a circling camera.
 */
UCLASS(NotPlaceable)
class PIRATESVSVIKINGS_API ABRReplayDirector : public AActor
{
	GENERATED_BODY()

public:
	ABRReplayDirector();
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** A highlight happened (Star = the one flying / getting wrecked). Bigger Score wins. */
	void OnHighlight(ABRCharacter* Star, float Score, const FString& Caption);

	bool IsPlaying() const { return bPlaying; }
	const FString& GetCaption() const { return Caption; }

private:
	struct FPoseFrame
	{
		float Time = 0.f;
		FTransform MeshTransform;
		TArray<FTransform> Bones; // parent-space
	};
	TMap<TWeakObjectPtr<ABRCharacter>, TArray<FPoseFrame>> History;
	float NextSample = 0.f;

	// The saved clip.
	TArray<FPoseFrame> Clip;
	TWeakObjectPtr<ABRCharacter> ClipStar;
	float BestScore = 0.f;
	float RecordClipUntil = 0.f;
	FString Caption;
	UPROPERTY() TObjectPtr<USkeletalMesh> ClipMesh;
	UPROPERTY() TArray<TObjectPtr<UMaterialInterface>> ClipMaterials;
	UPROPERTY() TArray<TObjectPtr<USkeletalMesh>> ClipWearables;

	// Playback.
	bool bPlaying = false;
	bool bPlayed = false;
	float EndedFor = 0.f;
	float PlayTime = 0.f;
	int32 PlayIndex = 0;
	UPROPERTY() TObjectPtr<AActor> Ghost;
	UPROPERTY() TObjectPtr<UPoseableMeshComponent> GhostBody;
	UPROPERTY() TObjectPtr<ACameraActor> Camera;

	bool Capture(ABRCharacter* Who, FPoseFrame& Out) const;
	void StartPlayback();
	void StopPlayback();
	void TickPlayback(float DeltaSeconds);
};
