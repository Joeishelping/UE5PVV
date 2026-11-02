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
	AddRow(CustomizeBox, MakeText(TEXT("Shirt/Pants slider all the way left = the outfit's own colour."), 12, FLinearColor(0.6f, 0.6f, 0.6f)), 2.f);

	CustomizeBox->SetVisibility(GI && GI->PreferredTeam != EPVVFaction::None ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	AddRow(Box, CustomizeBox, 4.f);
	RefreshSwatches();
}

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
