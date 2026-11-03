#include "BRReplay.h"
#include "BRCharacter.h"
#include "BRGameState.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"

namespace
{
	constexpr float SampleRate = 1.f / 20.f;
	constexpr float KeepSeconds = 3.f;
	constexpr float AfterSeconds = 3.f;
	constexpr float SlowMo = 0.55f;
}

ABRReplayDirector::ABRReplayDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

bool ABRReplayDirector::Capture(ABRCharacter* Who, FPoseFrame& Out) const
{
	const USkeletalMeshComponent* Body = Who ? Who->GetMesh() : nullptr;
	const USkeletalMesh* Mesh = Body ? Body->GetSkeletalMeshAsset() : nullptr;
	if (!Mesh || Who->IsHidden())
	{
		return false;
	}
	const TArray<FTransform>& Space = Body->GetComponentSpaceTransforms();
	const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
	if (Space.Num() == 0 || Space.Num() != Ref.GetNum())
	{
		return false;
	}
	Out.Time = GetWorld()->GetTimeSeconds();
	Out.MeshTransform = Body->GetComponentTransform();
	Out.Bones.SetNumUninitialized(Space.Num());
	for (int32 i = 0; i < Space.Num(); ++i)
	{
		const int32 Parent = Ref.GetParentIndex(i);
		Out.Bones[i] = Parent >= 0 ? Space[i].GetRelativeTransform(Space[Parent]) : Space[i];
	}
	return true;
}

void ABRReplayDirector::OnHighlight(ABRCharacter* Star, float Score, const FString& NewCaption)
{
	const ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>();
	if (!Star || bPlaying || bPlayed || Score <= BestScore || (GS && GS->Phase == EBRPhase::Ended))
	{
		return;
	}
	const TArray<FPoseFrame>* Past = History.Find(Star);
	BestScore = Score;
	Caption = NewCaption;
	ClipStar = Star;
	Clip = Past ? *Past : TArray<FPoseFrame>();
	RecordClipUntil = GetWorld()->GetTimeSeconds() + AfterSeconds;
	// Remember how they looked (body, materials, clothes, hair, hat).
	if (const USkeletalMeshComponent* Body = Star->GetMesh())
	{
		ClipMesh = Body->GetSkeletalMeshAsset();
		ClipMaterials.Reset();
		for (int32 i = 0; i < Body->GetNumMaterials(); ++i)
		{
			ClipMaterials.Add(Body->GetMaterial(i));
		}
	}
	ClipWearables.Reset();
	TArray<USkeletalMeshComponent*> Parts;
	Star->GetComponents<USkeletalMeshComponent>(Parts);
	for (const USkeletalMeshComponent* Part : Parts)
	{
		if (Part != Star->GetMesh() && Part->IsVisible() && Part->GetSkeletalMeshAsset())
		{
			ClipWearables.Add(Part->GetSkeletalMeshAsset());
		}
	}
}

void ABRReplayDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = GetWorld()->GetTimeSeconds();
	if (bPlaying)
	{
		TickPlayback(DeltaSeconds);
		return;
	}
	// Record everyone a few times a second.
	if (Now >= NextSample && !bPlayed)
	{
		NextSample = Now + SampleRate;
		int32 Count = 0;
		for (TActorIterator<ABRCharacter> It(GetWorld()); It && Count < 40; ++It, ++Count)
		{
			FPoseFrame Frame;
			if (!Capture(*It, Frame))
			{
				continue;
			}
			TArray<FPoseFrame>& Track = History.FindOrAdd(*It);
			Track.Add(Frame);
			int32 Old = 0;
			while (Old < Track.Num() && Track[Old].Time < Now - KeepSeconds)
			{
				++Old;
			}
			if (Old > 0)
			{
				Track.RemoveAt(0, Old, EAllowShrinking::No);
			}
			if (*It == ClipStar.Get() && Now < RecordClipUntil)
			{
				Clip.Add(Frame);
			}
		}
		// Forget people who left.
		for (auto It = History.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid())
			{
				It.RemoveCurrent();
			}
		}
	}
	// Round over: after the "TIME!" moment, show the play of the round.
	const ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>();
	if (GS && GS->Phase == EBRPhase::Ended && !bPlayed)
	{
		EndedFor += DeltaSeconds;
		if (EndedFor > 2.5f)
		{
			if (Clip.Num() > 10 && ClipMesh)
			{
				StartPlayback();
			}
			else
			{
				bPlayed = true; // nothing worth showing
			}
		}
	}
}

void ABRReplayDirector::StartPlayback()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		bPlayed = true;
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Ghost = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), Clip[0].MeshTransform, Params);
	if (!Ghost)
	{
		bPlayed = true;
		return;
	}
	USceneComponent* GhostRoot = NewObject<USceneComponent>(Ghost, TEXT("GhostRoot"));
	Ghost->SetRootComponent(GhostRoot);
	GhostRoot->RegisterComponent();
	GhostRoot->SetWorldTransform(Clip[0].MeshTransform);
	GhostBody = NewObject<UPoseableMeshComponent>(Ghost, TEXT("GhostBody"));
	GhostBody->SetupAttachment(GhostRoot);
	GhostBody->SetSkinnedAssetAndUpdate(ClipMesh, true);
	GhostBody->RegisterComponent();
	for (int32 i = 0; i < ClipMaterials.Num(); ++i)
	{
		GhostBody->SetMaterial(i, ClipMaterials[i]);
	}
	for (USkeletalMesh* Wear : ClipWearables)
	{
		USkeletalMeshComponent* Part = NewObject<USkeletalMeshComponent>(Ghost);
		Part->SetupAttachment(GhostBody);
		Part->SetSkeletalMeshAsset(Wear);
		Part->RegisterComponent();
		Part->SetLeaderPoseComponent(GhostBody);
	}
	// Hide the real star while the ghost plays (if they're near), so there aren't two of them.
	Camera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Clip[0].MeshTransform, Params);
	if (Camera && Camera->GetCameraComponent())
	{
		Camera->GetCameraComponent()->SetFieldOfView(75.f);
	}
	PC->SetViewTargetWithBlend(Camera, 0.4f);
	PlayTime = 0.f;
	PlayIndex = 0;
	bPlaying = true;
}

void ABRReplayDirector::TickPlayback(float DeltaSeconds)
{
	if (!GhostBody || Clip.Num() < 2)
	{
		StopPlayback();
		return;
	}
	PlayTime += DeltaSeconds * SlowMo;
	const float T = Clip[0].Time + PlayTime;
	while (PlayIndex < Clip.Num() - 2 && Clip[PlayIndex + 1].Time < T)
	{
		++PlayIndex;
	}
	const FPoseFrame& A = Clip[PlayIndex];
	const FPoseFrame& B = Clip[PlayIndex + 1];
	const float Alpha = FMath::Clamp((T - A.Time) / FMath::Max(0.001f, B.Time - A.Time), 0.f, 1.f);
	FTransform Where;
	Where.Blend(A.MeshTransform, B.MeshTransform, Alpha);
	Ghost->GetRootComponent()->SetWorldTransform(Where);
	const int32 NumBones = FMath::Min3(A.Bones.Num(), B.Bones.Num(), GhostBody->BoneSpaceTransforms.Num());
	for (int32 i = 0; i < NumBones; ++i)
	{
		GhostBody->BoneSpaceTransforms[i].Blend(A.Bones[i], B.Bones[i], Alpha);
	}
	GhostBody->MarkRefreshTransformDirty();
	// Slowly circle the action.
	if (Camera)
	{
		const FVector Target = Where.GetLocation() + FVector(0.f, 0.f, 60.f);
		const float Angle = FMath::DegreesToRadians(30.f + PlayTime * 40.f);
		const FVector From = Target + FVector(FMath::Cos(Angle) * 520.f, FMath::Sin(Angle) * 520.f, 180.f);
		Camera->SetActorLocationAndRotation(From, (Target - From).Rotation());
	}
	if (T > Clip.Last().Time + 0.3f)
	{
		StopPlayback();
	}
}

void ABRReplayDirector::StopPlayback()
{
	bPlaying = false;
	bPlayed = true;
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController(); PC && PC->GetPawn())
	{
		PC->SetViewTargetWithBlend(PC->GetPawn(), 0.4f);
	}
	if (Ghost)
	{
		Ghost->Destroy();
		Ghost = nullptr;
	}
	if (Camera)
	{
		Camera->Destroy();
		Camera = nullptr;
	}
	GhostBody = nullptr;
	Clip.Reset();
}

void ABRReplayDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Ghost)
	{
		Ghost->Destroy();
	}
	if (Camera)
	{
		Camera->Destroy();
	}
	Super::EndPlay(EndPlayReason);
}
