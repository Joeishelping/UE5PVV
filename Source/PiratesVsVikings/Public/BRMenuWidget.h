#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BRMenuWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;
class UVerticalBox;

/**
 * Main menu built entirely in code (no widget asset needed).
 * Pick a side, then host (Steam invites or Radmin/LAN), find a Steam game, join by IP, play solo or quit.
 * In game, M opens it again to switch teams.
 */
UCLASS()
class PIRATESVSVIKINGS_API UBRMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY() TObjectPtr<UTextBlock> StatusLine;
	UPROPERTY() TObjectPtr<UTextBlock> RunnerLabel;
	UPROPERTY() TObjectPtr<UTextBlock> WardenLabel;
	UPROPERTY() TObjectPtr<UEditableTextBox> AddressBox;

	void BuildLayout();
	UTextBlock* MakeText(const FString& Text, int32 Size, const FLinearColor& Color);
	UButton* MakeButton(const FString& Label, const FLinearColor& Color, TObjectPtr<UTextBlock>* OutLabel = nullptr);
	void AddRow(UVerticalBox* Box, UWidget* Widget, float RowPadding = 6.f);
	class ABRPlayerController* GetBRController() const;
	class UBRGameInstance* GetBRGameInstance() const;
	void ChooseTeam(bool bRunner);

	UFUNCTION() void OnRunner();
	UFUNCTION() void OnWarden();
	UFUNCTION() void OnHostSteam();
	UFUNCTION() void OnHostLan();
	UFUNCTION() void OnFindSteam();
	UFUNCTION() void OnInviteSteam();
	UFUNCTION() void OnJoinAddress();
	UFUNCTION() void OnPlay();
	UFUNCTION() void OnQuit();
};
