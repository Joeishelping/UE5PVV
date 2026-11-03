#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BRMenuWidget.generated.h"

class UBorder;
class UButton;
class UCheckBox;
class UHorizontalBox;
class USlider;
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

	// Character customization (shows after you pick a side).
	UPROPERTY() TObjectPtr<UVerticalBox> CustomizeBox;
	UPROPERTY() TObjectPtr<USlider> SkinSlider;
	UPROPERTY() TObjectPtr<USlider> ShirtSlider;
	UPROPERTY() TObjectPtr<USlider> PantsSlider;
	UPROPERTY() TObjectPtr<UCheckBox> HatBox;
	UPROPERTY() TObjectPtr<UBorder> SkinSwatch;
	UPROPERTY() TObjectPtr<UBorder> ShirtSwatch;
	UPROPERTY() TObjectPtr<UBorder> PantsSwatch;
	UPROPERTY() TObjectPtr<USlider> HeadSlider;
	UPROPERTY() TObjectPtr<USlider> NoseSlider;
	UPROPERTY() TObjectPtr<USlider> LipSlider;
	UPROPERTY() TObjectPtr<USlider> EarSlider;
	UPROPERTY() TObjectPtr<USlider> HairColorSlider;
	UPROPERTY() TObjectPtr<UBorder> HairColorSwatch;
	UPROPERTY() TObjectPtr<UTextBlock> HairLabel;
	UPROPERTY() TObjectPtr<UTextBlock> FacialHairLabel;
	UPROPERTY() TObjectPtr<UTextBlock> HatLabel;
	UPROPERTY() TObjectPtr<UTextBlock> HatLockedText;

	// Host settings (only shown to the host).
	UPROPERTY() TObjectPtr<UVerticalBox> HostBox;
	UPROPERTY() TObjectPtr<UTextBlock> HostPrepValue;
	UPROPERTY() TObjectPtr<UTextBlock> HostInvasionValue;
	UPROPERTY() TObjectPtr<UTextBlock> HostLobbyValue;
	UPROPERTY() TObjectPtr<UTextBlock> HostSeriesValue;
	UPROPERTY() TObjectPtr<UTextBlock> HostBotsValue;
	UPROPERTY() TObjectPtr<UTextBlock> HostBotRunnersValue;
	UPROPERTY() TObjectPtr<UTextBlock> HostBotWardensValue;
	void BuildHostPanel(class UCanvasPanel* Canvas);
	UHorizontalBox* MakeStepRow(const FString& Label, TObjectPtr<UTextBlock>& OutValue, UButton*& OutPrev, UButton*& OutNext);
	void StepHost(int32 Which, int32 Dir);
	void RefreshHost();
	UFUNCTION() void OnPrepLess();
	UFUNCTION() void OnPrepMore();
	UFUNCTION() void OnInvasionLess();
	UFUNCTION() void OnInvasionMore();
	UFUNCTION() void OnLobbyToggle();
	UFUNCTION() void OnSeriesToggle();
	UFUNCTION() void OnBotsToggle();
	UFUNCTION() void OnBotRunnersLess();
	UFUNCTION() void OnBotRunnersMore();
	UFUNCTION() void OnBotWardensLess();
	UFUNCTION() void OnBotWardensMore();
	UFUNCTION() void OnStartNow();

	void BuildLayout();
	UTextBlock* MakeText(const FString& Text, int32 Size, const FLinearColor& Color);
	UButton* MakeButton(const FString& Label, const FLinearColor& Color, TObjectPtr<UTextBlock>* OutLabel = nullptr);
	void AddRow(UVerticalBox* Box, UWidget* Widget, float RowPadding = 6.f);
	class ABRPlayerController* GetBRController() const;
	class UBRGameInstance* GetBRGameInstance() const;
	void ChooseTeam(bool bRunner);
	void BuildCustomize(UVerticalBox* Box);
	UHorizontalBox* MakeSliderRow(const FString& Label, float Value, TObjectPtr<USlider>& OutSlider, TObjectPtr<UBorder>& OutSwatch);
	void PreviewLook();
	void SendLook();
	void RefreshSwatches();

	UFUNCTION() void OnSkinChanged(float Value);
	UFUNCTION() void OnShirtChanged(float Value);
	UFUNCTION() void OnPantsChanged(float Value);
	UFUNCTION() void OnHatChanged(bool bChecked);
	UFUNCTION() void OnSliderReleased();
	UFUNCTION() void OnRandomLook();
	UFUNCTION() void OnHeadChanged(float Value);
	UFUNCTION() void OnNoseChanged(float Value);
	UFUNCTION() void OnLipChanged(float Value);
	UFUNCTION() void OnEarChanged(float Value);
	UFUNCTION() void OnHairColorChanged(float Value);
	UFUNCTION() void OnHairPrev();
	UFUNCTION() void OnHairNext();
	UFUNCTION() void OnFacialPrev();
	UFUNCTION() void OnFacialNext();
	UFUNCTION() void OnHatPrev();
	// Settings panel (left side).
	void BuildSettingsPanel(class UCanvasPanel* Canvas);
	UPROPERTY() TObjectPtr<USlider> SensSlider;
	UPROPERTY() TObjectPtr<USlider> FovSlider;
	UPROPERTY() TObjectPtr<USlider> MasterSlider;
	UPROPERTY() TObjectPtr<USlider> MusicSlider;
	UPROPERTY() TObjectPtr<UCheckBox> InvertBox;
	UPROPERTY() TObjectPtr<UTextBlock> SettingsReadout;
	UFUNCTION() void OnSensChanged(float Value);
	UFUNCTION() void OnFovChanged(float Value);
	UFUNCTION() void OnMasterChanged(float Value);
	UFUNCTION() void OnMusicChanged(float Value);
	UFUNCTION() void OnInvertChanged(bool bChecked);
	void SettingsChanged();
	UFUNCTION() void OnHatNext();
	void StepStyle(bool bFacial, int32 Step);
	UHorizontalBox* MakePickerRow(const FString& Label, TObjectPtr<UTextBlock>& OutValue, int32 Which); // 0 hair, 1 beard, 2 hat
	void StepHat(int32 Step);
	void RefreshPickers();

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
