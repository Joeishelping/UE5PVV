#include "BRMenuWidget.h"
#include "BRGameInstance.h"
#include "BRCharacter.h"
#include "BRPlayerController.h"
#include "BRTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Components/ScrollBox.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/KismetSystemLibrary.h"

namespace
{
	const FLinearColor RunnerColor(0.1f, 0.6f, 0.55f);
	const FLinearColor WardenColor(0.85f, 0.45f, 0.1f);
	const FLinearColor Neutral(0.25f, 0.25f, 0.28f);
	const FLinearColor Go(0.2f, 0.55f, 0.2f);
}

TSharedRef<SWidget> UBRMenuWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (!WidgetTree->RootWidget)
	{
		BuildLayout();
	}
	return Super::RebuildWidget();
}

UTextBlock* UBRMenuWidget::MakeText(const FString& Text, int32 Size, const FLinearColor& Color)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Block->SetText(FText::FromString(Text));
	FSlateFontInfo Font = Block->GetFont();
	Font.Size = Size;
	Block->SetFont(Font);
	Block->SetColorAndOpacity(FSlateColor(Color));
	Block->SetJustification(ETextJustify::Center);
	return Block;
}

UButton* UBRMenuWidget::MakeButton(const FString& Label, const FLinearColor& Color, TObjectPtr<UTextBlock>* OutLabel)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Button->SetBackgroundColor(Color);
	UTextBlock* Text = MakeText(Label, 20, FLinearColor::White);
	Button->AddChild(Text);
	if (OutLabel)
	{
		*OutLabel = Text;
	}
	return Button;
}

void UBRMenuWidget::AddRow(UVerticalBox* Box, UWidget* Widget, float RowPadding)
{
	if (UVerticalBoxSlot* ChildSlot = Box->AddChildToVerticalBox(Widget))
	{
		ChildSlot->SetPadding(FMargin(RowPadding));
		ChildSlot->SetHorizontalAlignment(HAlign_Fill);
	}
}

void UBRMenuWidget::BuildLayout()
{
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas"));
	WidgetTree->RootWidget = Canvas;

	// Dim the game behind the menu.
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.6f));
	if (UCanvasPanelSlot* DimSlot = Canvas->AddChildToCanvas(Dim))
	{
		DimSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		DimSlot->SetOffsets(FMargin(0.f));
	}

	USizeBox* Frame = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Frame->SetWidthOverride(560.f);
	if (UCanvasPanelSlot* FrameSlot = Canvas->AddChildToCanvas(Frame))
	{
		FrameSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		FrameSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		FrameSlot->SetAutoSize(true);
	}
	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Frame->AddChild(Box);

	AddRow(Box, MakeText(TEXT("BORDER RUN"), 54, FLinearColor(1.f, 0.85f, 0.3f)), 4.f);
	AddRow(Box, MakeText(TEXT("Choose your side"), 18, FLinearColor(0.85f, 0.85f, 0.85f)), 10.f);

	UHorizontalBox* Teams = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UButton* Runner = MakeButton(TEXT("RUNNER"), RunnerColor, &RunnerLabel);
	UButton* Warden = MakeButton(TEXT("BORDER PATROL"), WardenColor, &WardenLabel);
	Runner->OnClicked.AddDynamic(this, &UBRMenuWidget::OnRunner);
	Warden->OnClicked.AddDynamic(this, &UBRMenuWidget::OnWarden);
	for (UButton* TeamButton : { Runner, Warden })
	{
		if (UHorizontalBoxSlot* ChildSlot = Teams->AddChildToHorizontalBox(TeamButton))
		{
			ChildSlot->SetPadding(FMargin(4.f));
			ChildSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}
	AddRow(Box, Teams);
	BuildCustomize(Box);

	UButton* HostSteam = MakeButton(TEXT("HOST GAME  (invite Steam friends)"), Go);
	HostSteam->OnClicked.AddDynamic(this, &UBRMenuWidget::OnHostSteam);
	AddRow(Box, HostSteam);

	UButton* FindSteam = MakeButton(TEXT("FIND A FRIEND'S GAME  (Steam)"), Neutral);
	FindSteam->OnClicked.AddDynamic(this, &UBRMenuWidget::OnFindSteam);
	AddRow(Box, FindSteam);

	UButton* InviteSteam = MakeButton(TEXT("INVITE A STEAM FRIEND  (after hosting)"), Neutral);
	InviteSteam->OnClicked.AddDynamic(this, &UBRMenuWidget::OnInviteSteam);
	AddRow(Box, InviteSteam);

	UButton* HostLan = MakeButton(TEXT("HOST GAME  (Radmin / LAN)"), Neutral);
	HostLan->OnClicked.AddDynamic(this, &UBRMenuWidget::OnHostLan);
	AddRow(Box, HostLan);

	UHorizontalBox* JoinRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddressBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	AddressBox->SetHintText(FText::FromString(TEXT("Host's IP, e.g. 26.12.34.56")));
	UButton* Join = MakeButton(TEXT("JOIN IP"), Neutral);
	Join->OnClicked.AddDynamic(this, &UBRMenuWidget::OnJoinAddress);
	if (UHorizontalBoxSlot* ChildSlot = JoinRow->AddChildToHorizontalBox(AddressBox))
	{
		ChildSlot->SetPadding(FMargin(4.f));
		ChildSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ChildSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UHorizontalBoxSlot* ChildSlot = JoinRow->AddChildToHorizontalBox(Join))
	{
		ChildSlot->SetPadding(FMargin(4.f));
	}
	AddRow(Box, JoinRow);

	UHorizontalBox* Bottom = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UButton* Play = MakeButton(TEXT("PLAY / RESUME"), Go);
	UButton* Quit = MakeButton(TEXT("QUIT"), FLinearColor(0.5f, 0.12f, 0.1f));
	Play->OnClicked.AddDynamic(this, &UBRMenuWidget::OnPlay);
	Quit->OnClicked.AddDynamic(this, &UBRMenuWidget::OnQuit);
	for (UButton* B : { Play, Quit })
	{
		if (UHorizontalBoxSlot* ChildSlot = Bottom->AddChildToHorizontalBox(B))
		{
			ChildSlot->SetPadding(FMargin(4.f));
			ChildSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}
	AddRow(Box, Bottom, 10.f);

	StatusLine = MakeText(TEXT(""), 14, FLinearColor(0.7f, 0.7f, 0.7f));
	AddRow(Box, StatusLine);
	AddRow(Box, MakeText(TEXT("Press M in game to open this menu again."), 12, FLinearColor(0.55f, 0.55f, 0.55f)));
}

void UBRMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const UBRGameInstance* GI = GetBRGameInstance();
	if (!GI)
	{
		return;
	}
	if (StatusLine)
	{
		StatusLine->SetText(FText::FromString(GI->StatusText));
	}
	RefreshPickers();
	if (CustomizeBox)
	{
		CustomizeBox->SetVisibility(GI->PreferredTeam != EPVVFaction::None ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (RunnerLabel && WardenLabel)
	{
		const FString R = BR::RunnerName().ToUpper();
		const FString W = BR::WardenName().ToUpper();
		RunnerLabel->SetText(FText::FromString(GI->PreferredTeam == BR::Runners ? FString::Printf(TEXT("> %s <"), *R) : R));
		WardenLabel->SetText(FText::FromString(GI->PreferredTeam == BR::Wardens ? FString::Printf(TEXT("> %s <"), *W) : W));
	}
}

UHorizontalBox* UBRMenuWidget::MakeSliderRow(const FString& Label, float Value, TObjectPtr<USlider>& OutSlider, TObjectPtr<UBorder>& OutSwatch)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

	USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	LabelBox->SetWidthOverride(110.f);
	UTextBlock* Text = MakeText(Label, 16, FLinearColor::White);
	Text->SetJustification(ETextJustify::Left);
	LabelBox->AddChild(Text);

	OutSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
	OutSlider->SetValue(Value);
	OutSlider->SetSliderBarColor(FLinearColor(0.6f, 0.6f, 0.6f));
	OutSlider->SetSliderHandleColor(FLinearColor(1.f, 0.85f, 0.3f));
	OutSlider->OnMouseCaptureEnd.AddDynamic(this, &UBRMenuWidget::OnSliderReleased);

	USizeBox* SwatchBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SwatchBox->SetWidthOverride(44.f);
	SwatchBox->SetHeightOverride(26.f);
	OutSwatch = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	SwatchBox->AddChild(OutSwatch);

	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(LabelBox)) { S->SetVerticalAlignment(VAlign_Center); }
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(OutSlider))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(6.f, 0.f));
	}
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(SwatchBox)) { S->SetVerticalAlignment(VAlign_Center); }
	return Row;
}

void UBRMenuWidget::BuildCustomize(UVerticalBox* Box)
{
	const UBRGameInstance* GI = GetBRGameInstance();
	const FBRLook Look = GI ? GI->Look : FBRLook();

	CustomizeBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AddRow(CustomizeBox, MakeText(TEXT("Customize your character"), 18, FLinearColor(1.f, 0.85f, 0.3f)), 2.f);

	AddRow(CustomizeBox, MakeSliderRow(TEXT("Skin"), Look.SkinTone, SkinSlider, SkinSwatch), 2.f);
	AddRow(CustomizeBox, MakeSliderRow(TEXT("Shirt"), Look.ShirtColor, ShirtSlider, ShirtSwatch), 2.f);
	AddRow(CustomizeBox, MakeSliderRow(TEXT("Pants"), Look.PantsColor, PantsSlider, PantsSwatch), 2.f);
	SkinSlider->OnValueChanged.AddDynamic(this, &UBRMenuWidget::OnSkinChanged);
	ShirtSlider->OnValueChanged.AddDynamic(this, &UBRMenuWidget::OnShirtChanged);
	PantsSlider->OnValueChanged.AddDynamic(this, &UBRMenuWidget::OnPantsChanged);

	// Face shape (middle = normal).
	TObjectPtr<UBorder> NoSwatch;
	AddRow(CustomizeBox, MakeSliderRow(TEXT("Head"), Look.HeadSize, HeadSlider, NoSwatch), 2.f);
	AddRow(CustomizeBox, MakeSliderRow(TEXT("Nose"), Look.NoseSize, NoseSlider, NoSwatch), 2.f);
	AddRow(CustomizeBox, MakeSliderRow(TEXT("Lips"), Look.LipSize, LipSlider, NoSwatch), 2.f);
	AddRow(CustomizeBox, MakeSliderRow(TEXT("Ears"), Look.EarSize, EarSlider, NoSwatch), 2.f);
	HeadSlider->OnValueChanged.AddDynamic(this, &UBRMenuWidget::OnHeadChanged);
	NoseSlider->OnValueChanged.AddDynamic(this, &UBRMenuWidget::OnNoseChanged);
	LipSlider->OnValueChanged.AddDynamic(this, &UBRMenuWidget::OnLipChanged);
	EarSlider->OnValueChanged.AddDynamic(this, &UBRMenuWidget::OnEarChanged);

	// Hair + facial hair.
	AddRow(CustomizeBox, MakePickerRow(TEXT("Hair"), HairLabel, 0), 2.f);
	AddRow(CustomizeBox, MakePickerRow(TEXT("Beard"), FacialHairLabel, 1), 2.f);
	AddRow(CustomizeBox, MakePickerRow(TEXT("Hat"), HatLabel, 2), 2.f);
	HatLockedText = MakeText(TEXT(""), 12, FLinearColor(0.7f, 0.7f, 0.7f));
	AddRow(CustomizeBox, HatLockedText, 2.f);
	AddRow(CustomizeBox, MakeSliderRow(TEXT("Hair color"), Look.HairColor, HairColorSlider, HairColorSwatch), 2.f);
	HairColorSlider->OnValueChanged.AddDynamic(this, &UBRMenuWidget::OnHairColorChanged);

	UHorizontalBox* Extras = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	HatBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass());
	HatBox->SetIsChecked(Look.bHat);
	HatBox->OnCheckStateChanged.AddDynamic(this, &UBRMenuWidget::OnHatChanged);
	UTextBlock* HatText = MakeText(TEXT("Hat"), 16, FLinearColor::White);
	UButton* Random = MakeButton(TEXT("RANDOM"), Neutral);
	Random->OnClicked.AddDynamic(this, &UBRMenuWidget::OnRandomLook);
	if (UHorizontalBoxSlot* S = Extras->AddChildToHorizontalBox(HatBox)) { S->SetVerticalAlignment(VAlign_Center); }
	if (UHorizontalBoxSlot* S = Extras->AddChildToHorizontalBox(HatText))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	if (UHorizontalBoxSlot* S = Extras->AddChildToHorizontalBox(Random)) { S->SetVerticalAlignment(VAlign_Center); }
	AddRow(CustomizeBox, Extras, 2.f);
	AddRow(CustomizeBox, MakeText(TEXT("Colour sliders all the way left = original colour. Face sliders: middle = normal."), 12, FLinearColor(0.6f, 0.6f, 0.6f)), 2.f);

	CustomizeBox->SetVisibility(GI && GI->PreferredTeam != EPVVFaction::None ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	// Scrolls if the screen is small.
	USizeBox* Limit = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Limit->SetMaxDesiredHeight(380.f);
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	Scroll->AddChild(CustomizeBox);
	Limit->AddChild(Scroll);
	AddRow(Box, Limit, 4.f);
	RefreshSwatches();
	RefreshPickers();
}

UHorizontalBox* UBRMenuWidget::MakePickerRow(const FString& Label, TObjectPtr<UTextBlock>& OutValue, int32 Which)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	LabelBox->SetWidthOverride(110.f);
	UTextBlock* Text = MakeText(Label, 16, FLinearColor::White);
	Text->SetJustification(ETextJustify::Left);
	LabelBox->AddChild(Text);
	UButton* Prev = MakeButton(TEXT("<"), Neutral);
	UButton* Next = MakeButton(TEXT(">"), Neutral);
	if (Which == 2)
	{
		Prev->OnClicked.AddDynamic(this, &UBRMenuWidget::OnHatPrev);
		Next->OnClicked.AddDynamic(this, &UBRMenuWidget::OnHatNext);
	}
	else if (Which == 1)
	{
		Prev->OnClicked.AddDynamic(this, &UBRMenuWidget::OnFacialPrev);
		Next->OnClicked.AddDynamic(this, &UBRMenuWidget::OnFacialNext);
	}
	else
	{
		Prev->OnClicked.AddDynamic(this, &UBRMenuWidget::OnHairPrev);
		Next->OnClicked.AddDynamic(this, &UBRMenuWidget::OnHairNext);
	}
	OutValue = MakeText(TEXT(""), 16, FLinearColor(1.f, 0.85f, 0.3f));
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(LabelBox)) { S->SetVerticalAlignment(VAlign_Center); }
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Prev)) { S->SetVerticalAlignment(VAlign_Center); }
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(OutValue))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetVerticalAlignment(VAlign_Center);
	}
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Next)) { S->SetVerticalAlignment(VAlign_Center); }
	return Row;
}

void UBRMenuWidget::RefreshPickers()
{
	const UBRGameInstance* GI = GetBRGameInstance();
	const ABRPlayerController* PC = GetBRController();
	const ABRCharacter* Me = PC ? Cast<ABRCharacter>(PC->GetPawn()) : nullptr;
	if (!GI)
	{
		return;
	}
	auto Describe = [](int32 Index, int32 Count, const TCHAR* None)
	{
		if (Index <= 0) return FString(None);
		return Count > 0 ? FString::Printf(TEXT("Style %d / %d"), (Index - 1) % Count + 1, Count) : FString::Printf(TEXT("Style %d"), Index);
	};
	if (HairLabel) HairLabel->SetText(FText::FromString(Describe(GI->Look.HairStyle, Me ? Me->GetHairStyleCount() : 0, TEXT("Bald"))));
	if (FacialHairLabel) FacialHairLabel->SetText(FText::FromString(Describe(GI->Look.FacialHair, Me ? Me->GetFacialHairCount() : 0, TEXT("Clean shaven"))));
	if (HatLabel)
	{
		const FString HatText = GI->Look.HatChoice < 0 ? FString(TEXT("Team default"))
			: GI->Look.HatChoice == 0 ? FString(TEXT("No hat"))
			: FString::Printf(TEXT("%s: %s"), *BR::HatName(static_cast<EBRHat>(GI->Look.HatChoice)), *BR::HatSkill(static_cast<EBRHat>(GI->Look.HatChoice)));
		HatLabel->SetText(FText::FromString(HatText));
	}
	if (HatLockedText)
	{
		FString Locked;
		for (int32 i = 1; i <= static_cast<int32>(EBRHat::CannonHelmet); ++i)
		{
			const FString Need = GI->HatUnlockText(static_cast<EBRHat>(i));
			if (!Need.IsEmpty())
			{
				Locked += FString::Printf(TEXT("%sLOCKED %s: %s"), Locked.IsEmpty() ? TEXT("") : TEXT("\n"), *BR::HatName(static_cast<EBRHat>(i)), *Need);
			}
		}
		HatLockedText->SetText(FText::FromString(Locked));
	}
}

void UBRMenuWidget::StepHat(int32 Step)
{
	UBRGameInstance* GI = GetBRGameInstance();
	if (!GI)
	{
		return;
	}
	// Options: -1 team default, 0 no hat, then every unlocked hat.
	TArray<int32> Options = { -1, 0 };
	for (int32 i = 1; i <= static_cast<int32>(EBRHat::CannonHelmet); ++i)
	{
		if (GI->IsHatUnlocked(static_cast<EBRHat>(i)))
		{
			Options.Add(i);
		}
	}
	const int32 Current = FMath::Max(0, Options.IndexOfByKey(GI->Look.HatChoice));
	GI->Look.HatChoice = Options[((Current + Step) % Options.Num() + Options.Num()) % Options.Num()];
	RefreshPickers();
	SendLook();
}

void UBRMenuWidget::OnHatPrev() { StepHat(-1); }
void UBRMenuWidget::OnHatNext() { StepHat(1); }

void UBRMenuWidget::StepStyle(bool bFacial, int32 Step)
{
	UBRGameInstance* GI = GetBRGameInstance();
	const ABRPlayerController* PC = GetBRController();
	const ABRCharacter* Me = PC ? Cast<ABRCharacter>(PC->GetPawn()) : nullptr;
	if (!GI)
	{
		return;
	}
	const int32 Count = Me ? (bFacial ? Me->GetFacialHairCount() : Me->GetHairStyleCount()) : 16;
	int32& Index = bFacial ? GI->Look.FacialHair : GI->Look.HairStyle;
	const int32 Options = FMath::Max(1, Count) + 1; // + "none"
	Index = ((Index + Step) % Options + Options) % Options;
	RefreshPickers();
	SendLook();
}

void UBRMenuWidget::OnHairPrev() { StepStyle(false, -1); }
void UBRMenuWidget::OnHairNext() { StepStyle(false, 1); }
void UBRMenuWidget::OnFacialPrev() { StepStyle(true, -1); }
void UBRMenuWidget::OnFacialNext() { StepStyle(true, 1); }
void UBRMenuWidget::OnHeadChanged(float Value) { if (UBRGameInstance* GI = GetBRGameInstance()) { GI->Look.HeadSize = Value; PreviewLook(); } }
void UBRMenuWidget::OnNoseChanged(float Value) { if (UBRGameInstance* GI = GetBRGameInstance()) { GI->Look.NoseSize = Value; PreviewLook(); } }
void UBRMenuWidget::OnLipChanged(float Value) { if (UBRGameInstance* GI = GetBRGameInstance()) { GI->Look.LipSize = Value; PreviewLook(); } }
void UBRMenuWidget::OnEarChanged(float Value) { if (UBRGameInstance* GI = GetBRGameInstance()) { GI->Look.EarSize = Value; PreviewLook(); } }
void UBRMenuWidget::OnHairColorChanged(float Value) { if (UBRGameInstance* GI = GetBRGameInstance()) { GI->Look.HairColor = Value; PreviewLook(); } }

void UBRMenuWidget::RefreshSwatches()
{
	const UBRGameInstance* GI = GetBRGameInstance();
	if (!GI)
	{
		return;
	}
	const FLinearColor Original(0.3f, 0.3f, 0.3f);
	if (SkinSwatch) SkinSwatch->SetBrushColor(BR::SkinColor(GI->Look.SkinTone));
	if (ShirtSwatch) ShirtSwatch->SetBrushColor(BR::IsOriginalCloth(GI->Look.ShirtColor) ? Original : BR::ClothColor(GI->Look.ShirtColor));
	if (PantsSwatch) PantsSwatch->SetBrushColor(BR::IsOriginalCloth(GI->Look.PantsColor) ? Original : BR::ClothColor(GI->Look.PantsColor));
	if (HairColorSwatch) HairColorSwatch->SetBrushColor(BR::IsOriginalCloth(GI->Look.HairColor) ? Original : BR::HairColor(GI->Look.HairColor));
}

void UBRMenuWidget::PreviewLook()
{
	const UBRGameInstance* GI = GetBRGameInstance();
	const ABRPlayerController* PC = GetBRController();
	if (GI && PC)
	{
		if (ABRCharacter* Me = Cast<ABRCharacter>(PC->GetPawn()))
		{
			Me->PreviewLook(GI->Look);
		}
	}
	RefreshSwatches();
}

void UBRMenuWidget::SendLook()
{
	UBRGameInstance* GI = GetBRGameInstance();
	if (!GI)
	{
		return;
	}
	GI->SaveLook();
	if (ABRPlayerController* PC = GetBRController())
	{
		PC->SendLook(GI->Look);
	}
	RefreshSwatches();
}

void UBRMenuWidget::OnSkinChanged(float Value) { if (UBRGameInstance* GI = GetBRGameInstance()) { GI->Look.SkinTone = Value; PreviewLook(); } }
void UBRMenuWidget::OnShirtChanged(float Value) { if (UBRGameInstance* GI = GetBRGameInstance()) { GI->Look.ShirtColor = Value; PreviewLook(); } }
void UBRMenuWidget::OnPantsChanged(float Value) { if (UBRGameInstance* GI = GetBRGameInstance()) { GI->Look.PantsColor = Value; PreviewLook(); } }
void UBRMenuWidget::OnSliderReleased() { SendLook(); }

void UBRMenuWidget::OnHatChanged(bool bChecked)
{
	if (UBRGameInstance* GI = GetBRGameInstance())
	{
		GI->Look.bHat = bChecked;
		SendLook();
	}
}

void UBRMenuWidget::OnRandomLook()
{
	UBRGameInstance* GI = GetBRGameInstance();
	if (!GI)
	{
		return;
	}
	GI->Look.SkinTone = FMath::FRand();
	GI->Look.ShirtColor = FMath::FRand();
	GI->Look.PantsColor = FMath::FRand();
	GI->Look.bHat = FMath::RandBool();
	GI->Look.HeadSize = FMath::FRandRange(.4f, .6f);
	GI->Look.NoseSize = FMath::FRandRange(.3f, .7f);
	GI->Look.LipSize = FMath::FRandRange(.35f, .65f);
	GI->Look.EarSize = FMath::FRandRange(.35f, .65f);
	GI->Look.HairColor = FMath::RandBool() ? 0.f : FMath::FRandRange(.06f, .4f);
	const ABRPlayerController* PC = GetBRController();
	const ABRCharacter* Me = PC ? Cast<ABRCharacter>(PC->GetPawn()) : nullptr;
	GI->Look.HairStyle = FMath::RandRange(0, Me ? Me->GetHairStyleCount() : 0);
	GI->Look.FacialHair = FMath::RandBool() ? 0 : FMath::RandRange(0, Me ? Me->GetFacialHairCount() : 0);
	if (HeadSlider) HeadSlider->SetValue(GI->Look.HeadSize);
	if (NoseSlider) NoseSlider->SetValue(GI->Look.NoseSize);
	if (LipSlider) LipSlider->SetValue(GI->Look.LipSize);
	if (EarSlider) EarSlider->SetValue(GI->Look.EarSize);
	if (HairColorSlider) HairColorSlider->SetValue(GI->Look.HairColor);
	RefreshPickers();
	if (SkinSlider) SkinSlider->SetValue(GI->Look.SkinTone);
	if (ShirtSlider) ShirtSlider->SetValue(GI->Look.ShirtColor);
	if (PantsSlider) PantsSlider->SetValue(GI->Look.PantsColor);
	if (HatBox) HatBox->SetIsChecked(GI->Look.bHat);
	SendLook();
}

ABRPlayerController* UBRMenuWidget::GetBRController() const
{
	return Cast<ABRPlayerController>(GetOwningPlayer());
}

UBRGameInstance* UBRMenuWidget::GetBRGameInstance() const
{
	return Cast<UBRGameInstance>(GetGameInstance());
}

void UBRMenuWidget::ChooseTeam(bool bRunner)
{
	UBRGameInstance* GI = GetBRGameInstance();
	ABRPlayerController* PC = GetBRController();
	if (!GI)
	{
		return;
	}
	GI->PreferredTeam = bRunner ? BR::Runners : BR::Wardens;
	if (PC)
	{
		PC->RequestFaction(GI->PreferredTeam);
	}
}

void UBRMenuWidget::OnRunner() { ChooseTeam(true); }
void UBRMenuWidget::OnWarden() { ChooseTeam(false); }

void UBRMenuWidget::OnHostSteam()
{
	UBRGameInstance* GI = GetBRGameInstance();
	if (ABRPlayerController* PC = GetBRController())
	{
		PC->HideMenu();
	}
	if (GI)
	{
		GI->HostSteam();
	}
}

void UBRMenuWidget::OnFindSteam()
{
	if (UBRGameInstance* GI = GetBRGameInstance())
	{
		GI->bMenuDone = true;
		GI->FindGame();
	}
}

void UBRMenuWidget::OnInviteSteam()
{
	if (UBRGameInstance* GI = GetBRGameInstance())
	{
		GI->Invite();
	}
}

void UBRMenuWidget::OnHostLan()
{
	if (ABRPlayerController* PC = GetBRController())
	{
		PC->HideMenu();
		PC->Host();
	}
}

void UBRMenuWidget::OnJoinAddress()
{
	const FString Address = AddressBox ? AddressBox->GetText().ToString().TrimStartAndEnd() : FString();
	ABRPlayerController* PC = GetBRController();
	if (!PC || Address.IsEmpty())
	{
		return;
	}
	PC->HideMenu();
	PC->Join(Address);
}

void UBRMenuWidget::OnPlay()
{
	if (ABRPlayerController* PC = GetBRController())
	{
		PC->HideMenu();
	}
}

void UBRMenuWidget::OnQuit()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
