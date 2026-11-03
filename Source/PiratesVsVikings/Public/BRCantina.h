#pragma once

#include "CoreMinimal.h"
#include "BRWorldActors.h"
#include "BRCantina.generated.h"

class ABRCantina;

/** What a spot inside the cantina does. */
UENUM(BlueprintType)
enum class EBRCantinaSpot : uint8
{
	Drink,		// buy a tequila: tipsy (wobblier but tougher)
	Contest,	// drinking contest vs El Borracho: mash Space
	Brawl		// shove the big guy at the bar: BAR FIGHT
};

/** One thing to do in the cantina (the bar, the contest table, the rowdy guy). Spawned by the cantina. */
UCLASS(NotPlaceable)
class PIRATESVSVIKINGS_API ABRCantinaSpot : public ABRGreyboxActor, public IPVVInteractable
{
	GENERATED_BODY()

public:
	ABRCantinaSpot();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	virtual bool CanInteract_Implementation(APVVCharacter* Interactor) const override;
	virtual float GetInteractDuration_Implementation(APVVCharacter* Interactor) const override { return 0.4f; }
	virtual FText GetInteractPrompt_Implementation(APVVCharacter* Interactor) const override;
	virtual void OnInteract_Implementation(APVVCharacter* Interactor) override;

	UPROPERTY(Replicated) EBRCantinaSpot Kind = EBRCantinaSpot::Drink;
	UPROPERTY(Replicated) TObjectPtr<ABRCantina> Cantina;
};

/**
 * La Cantina del Diablo: a saloon full of locals. Buy a drink, enter a drinking contest, or start a BAR FIGHT.
 * Bar fight: everybody fights everybody (Gang Beasts style). Last one standing collects $50 per knocked-out local;
 * get knocked out and you pay $50 and get nothing. Start too many and the bouncer throws you out.
 */
UCLASS(meta = (DisplayName = "BR Cantina"))
class PIRATESVSVIKINGS_API ABRCantina : public ABRGreyboxActor
{
	GENERATED_BODY()

public:
	ABRCantina();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "BR|Cantina") FString CantinaName = TEXT("LA CANTINA DEL DIABLO");
	UPROPERTY(EditAnywhere, Category = "BR|Cantina") int32 PatronCount = 8;
	UPROPERTY(EditAnywhere, Category = "BR|Cantina") int32 DrinkPrice = 15;
	UPROPERTY(EditAnywhere, Category = "BR|Cantina") float TipsySeconds = 25.f;
	UPROPERTY(EditAnywhere, Category = "BR|Cantina") int32 ContestEntry = 50;
	UPROPERTY(EditAnywhere, Category = "BR|Cantina") int32 ContestPrize = 150;
	/** Bar fight: the WINNER (the last one standing) finds this much next to each knocked-out local. Nobody else gets anything. */
	UPROPERTY(EditAnywhere, Category = "BR|Cantina") int32 CashPerLocal = 40;
	/** Bar fight: what you pay if you get knocked out. */
	UPROPERTY(EditAnywhere, Category = "BR|Cantina") int32 LoseFine = 50;
	/** Bar fight: seconds before the cops break it up (nobody wins). */
	UPROPERTY(EditAnywhere, Category = "BR|Cantina") float FightSeconds = 150.f;
	/** Seconds after a bar fight before fresh locals wander in. */
	UPROPERTY(EditAnywhere, Category = "BR|Cantina") float RefillSeconds = 150.f;
	/** Bar fights you can start before the bouncer stops letting you in. */
	UPROPERTY(EditAnywhere, Category = "BR|Cantina") int32 StrikesBeforeBanned = 2;
	/** Bar-fight bell / crowd roar. */
	UPROPERTY(EditAnywhere, Category = "BR|Cantina") TObjectPtr<USoundBase> FightSound;
	/** Glug glug / cheers. */
	UPROPERTY(EditAnywhere, Category = "BR|Cantina") TObjectPtr<USoundBase> DrinkSound;
	/** Saloon piano music (loops). */
	UPROPERTY(EditAnywhere, Category = "BR|Cantina") TObjectPtr<USoundBase> Music;

	bool IsInside(const FVector& WorldLocation) const;
	bool IsFightOn() const { return bFight; }
	int32 GetLocalsStanding() const { return LocalsStanding; }
	float GetFightSecondsLeft() const;
	bool IsContestOn() const { return ContestDrinker != nullptr; }
	bool IsBanned(const APVVCharacter* Who) const;

	// Called by the spots (server).
	bool CanUse(EBRCantinaSpot Kind, const APVVCharacter* Who) const;
	FText GetPrompt(EBRCantinaSpot Kind, const APVVCharacter* Who) const;
	void Use(EBRCantinaSpot Kind, APVVCharacter* Who);
	/** Server: kick off the bar fight. */
	void StartFight(ABRCharacter* Starter);

protected:
	UPROPERTY(Replicated) bool bFight = false;
	UPROPERTY(Replicated) int32 LocalsStanding = 0;
	UPROPERTY(Replicated) float FightEndsAt = 0.f;
	bool bToldFinalDuel = false;
	/** Times each player has been knocked down this fight (2 = out). */
	TMap<TWeakObjectPtr<ABRCharacter>, int32> PlayerKOs;
	TSet<TWeakObjectPtr<ABRCharacter>> PlayersDown;
	UPROPERTY() TArray<TObjectPtr<ABRCharacter>> Patrons;
	UPROPERTY() TObjectPtr<ABRCharacter> Bouncer;
	UPROPERTY() TArray<TObjectPtr<ABRCharacter>> Contestants;
	UPROPERTY() TArray<TObjectPtr<ABRCharacter>> Losers;
	UPROPERTY() TArray<TObjectPtr<ABRCharacter>> KnockedOut;
	UPROPERTY() TObjectPtr<ABRCharacter> ContestDrinker;
	UPROPERTY() TObjectPtr<UAudioComponent> MusicAudio;
	float FightStartTime = 0.f;
	float RefillAt = -1.f;
	float ContestStart = 0.f;
	float RivalRate = 0.12f;
	float NextThink = 0.f;
	float LastBounceTime = -10.f;

	static constexpr float HalfX = 800.f;
	static constexpr float HalfY = 600.f;

	FVector Local(float X, float Y, float Z = 0.f) const;
	void SpawnPatrons();
	void ClearPatrons();
	void TickFight();
	void TickContest(float Now);
	void TickBouncer();
	void EndFight(const TArray<ABRCharacter*>& Winners, bool bTimeUp = false);
	ABRCharacter* PickFoe(ABRCharacter* For) const;
	TArray<ABRCharacter*> PlayersInside() const;
};

/** A wanted poster on a wall: shows whoever has the bounty (or is winning). */
UCLASS(meta = (DisplayName = "BR Wanted Poster"))
class PIRATESVSVIKINGS_API ABRWantedPoster : public ABRGreyboxActor
{
	GENERATED_BODY()

public:
	ABRWantedPoster();
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY() TObjectPtr<UTextRenderComponent> Text;
	float NextRefresh = 0.f;
};
