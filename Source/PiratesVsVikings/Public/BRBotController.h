#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BRBotController.generated.h"

class ABRCharacter;
class ABRPlayerState;

/**
 * A computer player. It has a PlayerState (shows on the scoreboard, scores points, can be caught)
 * and "presses buttons" on its character like a person: walk, sprint, jump, punch, hold E.
 * No NavMesh needed: it steers straight at its goal and jumps / sidesteps when it gets stuck.
 *   Runner bots: work their job and buy a ladder in Prep, then run for the Safe Zone (ladder over the Wall,
 *                otherwise through the checkpoint gate), dodging Border Patrol.
 *   Border Patrol bots: chase Runners who crossed, punch them down, zip-tie them, carry them to the pen.
 * Add them with the console:  AddBots 3   /  AddBot Runners   /  RemoveBots
 * or tick "Fill With Bots" in BP_BRGameMode.
 */
UCLASS()
class PIRATESVSVIKINGS_API ABRBotController : public AAIController
{
	GENERATED_BODY()

public:
	ABRBotController();
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnPossess(APawn* InPawn) override;

private:
	ABRCharacter* Me() const;
	ABRPlayerState* MyState() const;

	void ThinkRunner(ABRCharacter* C, ABRPlayerState* PS);
	void ThinkWarden(ABRCharacter* C, ABRPlayerState* PS);
	/** Walk toward a spot; returns true once within Radius. */
	bool SteerTo(ABRCharacter* C, const FVector& Where, float Radius, bool bSprint);
	/** Use the nearest thing of this kind (station, wall, ladder...) when close; walk to it otherwise. */
	bool GoUse(ABRCharacter* C, AActor* Target, float UseRange = 230.f);
	void FaceTowards(const FVector& Where);

	AActor* FindStation(ABRCharacter* C, ABRPlayerState* PS, bool bJob) const;
	AActor* FindNearestUsable(ABRCharacter* C, float Radius, bool (*Filter)(const AActor*)) const;
	FVector SafeZoneGoal(const ABRCharacter* C) const;

	TWeakObjectPtr<ABRCharacter> Target;
	/** Who we're brawling with up close right now (Gang Beasts brain runs every frame). */
	TWeakObjectPtr<ABRCharacter> MeleeFoe;
	FVector LastPos = FVector::ZeroVector;
	float StuckTime = 0.f;
	float SidestepUntil = 0.f;
	FVector SidestepDir = FVector::ZeroVector;
	float NextThink = 0.f;
	float NextAttack = 0.f;
	float NextMash = 0.f;
	float WakeUpAt = 0.f;
	FVector PatrolPoint = FVector::ZeroVector;
	float PatrolUntil = 0.f;
	FVector MoveDir = FVector::ZeroVector;
	bool bWantSprint = false;
};
