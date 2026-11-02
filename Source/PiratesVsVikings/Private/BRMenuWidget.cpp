#include "BRMenuWidget.h"
#include "BRGameInstance.h"
#include "BRPlayerController.h"
#include "BRTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
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
	if (RunnerLabel && WardenLabel)
	{
		RunnerLabel->SetText(FText::FromString(GI->PreferredTeam == BR::Runners ? TEXT("> RUNNER <") : TEXT("RUNNER")));
		WardenLabel->SetText(FText::FromString(GI->PreferredTeam == BR::Wardens ? TEXT("> BORDER PATROL <") : TEXT("BORDER PATROL")));
	}
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
