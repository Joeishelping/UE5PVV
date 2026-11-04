#pragma once

// High-noon duel (Mexico). Hold E at the duel post to call out the pistolero:
//   you're both put on your marks, 3... 2... 1... then a random wait... DRAW!
//   Your revolver comes out at DRAW: right mouse to aim, left click to shoot. One hit kills.
//   Shoot first and you win BIG (your money doubled + a prize). Too slow (or shoot early) and you're DEAD -
//   and the pistolero takes ALL your money and everything in your pockets. Walk off your mark = coward (he takes your money).

#include "CoreMinimal.h"
#include "BRWorldActors.h"
#include "PVVInteractable.h"
#include "BRDuel.generated.h"

class ABRCharacter;

UCLASS(meta = (DisplayName = "BR Duel Post"))
class PIRATESVSVIKINGS_API ABRDuelSpot : public ABRGreyboxActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRDuelSpot();
	virtual void Tick(float DeltaSeconds) override;
	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override { return 1.5f; }
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

	/** Server: someone in this duel pulled the trigger (Victim = who the bullet hit, if anyone). */
	void OnShotFired(ABRCharacter* Shooter, class APVVCharacter* Victim);

	/** You need at least this much money to be worth the pistolero's time. */
	UPROPERTY(EditAnywhere, Category = "BR|Duel") int32 MinMoney = 200;
	/** Win = your money doubled + this. */
	UPROPERTY(EditAnywhere, Category = "BR|Duel") int32 Prize = 1500;
	/** Pistolero's draw time after DRAW! (seconds, random between these; he gets faster after each loss). */
	UPROPERTY(EditAnywhere, Category = "BR|Duel") FVector2D DrawTime = FVector2D(0.75f, 1.35f);
	/** Chance each of his shots hits. */
	UPROPERTY(EditAnywhere, Category = "BR|Duel") float HitChance = 0.8f;
	/** Distance between the two marks (cm). */
	UPROPERTY(EditAnywhere, Category = "BR|Duel") float Paces = 1500.f;

private:
	enum class EState : uint8 { Idle, Countdown, Waiting, Draw, Over };
	EState State = EState::Idle;
	TWeakObjectPtr<ABRCharacter> Challenger;
	TWeakObjectPtr<ABRCharacter> Pistolero;
	float StateTime = 0.f;
	float DrawAt = 0.f;
	float NextNPCShot = 0.f;
	int32 CountShown = 0;
	int32 Wins = 0;
	FVector MarkA = FVector::ZeroVector;
	FVector MarkB = FVector::ZeroVector;
	void Finish(bool bChallengerWon, const FText& Why);
	void Say(const FString& Text, float Height = 260.f);
};
