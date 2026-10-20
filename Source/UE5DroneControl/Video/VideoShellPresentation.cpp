#include "Video/VideoShellWidget.h"
#include "Shared/FigmaRoleHeaderWidget.h"
#include "Shared/ProductText.h"
#include "Shared/OperationalContextSubsystem.h"
#include "Command/FigmaCommandStyle.h"
#include "Command/CommandAlertStore.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Brushes/SlateImageBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Misc/Paths.h"
#include "WebBrowser.h"

namespace VideoUI
{
const FLinearColor PanelColor(FColor(20,24,34));
const FLinearColor Muted(FColor(170,180,193));
const FLinearColor Dim(FColor(120,126,147));
const FLinearColor Badge(FColor(44,56,76));

UCanvasPanelSlot* Place(UCanvasPanel* Canvas,UWidget* Widget,float X,float Y,float W,float H)
{
    auto* Slot=Canvas->AddChildToCanvas(Widget);Slot->SetPosition({X,Y});Slot->SetSize({W,H});return Slot;
}
USizeBox* Fixed(UWidgetTree* Tree,UWidget* Child,float W,float H)
{
    auto* Box=Tree->ConstructWidget<USizeBox>();Box->SetWidthOverride(W);Box->SetHeightOverride(H);Box->SetContent(Child);return Box;
}
void Fill(UOverlay* Overlay,UWidget* Child)
{
    auto* Slot=Overlay->AddChildToOverlay(Child);Slot->SetHorizontalAlignment(HAlign_Fill);Slot->SetVerticalAlignment(VAlign_Fill);
}
UImage* Asset(UWidgetTree* Tree,const TCHAR* File,FVector2D Native,FVector2D Requested)
{
    auto* Image=Tree->ConstructWidget<UImage>();Image->SetVisibility(ESlateVisibility::HitTestInvisible);
    const FString Path=FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()/TEXT("UI/Figma/Video")/File);
    ensureMsgf(FPaths::FileExists(Path),TEXT("Missing local Figma Video asset %s"),*Path);
    FSlateBrush Copy;
    if(Path.EndsWith(TEXT(".svg"))){
        static TMap<FString,TUniquePtr<FSlateVectorImageBrush>> Cache;
        auto& Brush=Cache.FindOrAdd(File);if(!Brush)Brush=MakeUnique<FSlateVectorImageBrush>(Path,Native,FLinearColor::White);Copy=*Brush;
    }else{
        // A file-name FSlateImageBrush is not a loaded runtime texture. Import the
        // original PNG so alpha survives; UImage's reflected brush owns its UObject.
        static TMap<FString,TWeakObjectPtr<UTexture2D>> Textures;
        auto& Texture=Textures.FindOrAdd(File);if(!Texture.IsValid())Texture=FImageUtils::ImportFileAsTexture2D(Path);
        ensureMsgf(Texture.IsValid(),TEXT("Unable to load Figma Video PNG %s"),*Path);
        Image->SetBrushFromTexture(Texture.Get(),false);Copy=Image->GetBrush();
    }
    const auto Size=Native*FMath::Min(Requested.X/Native.X,Requested.Y/Native.Y);
    Copy.SetImageSize(FVector2f(Size));Image->SetBrush(Copy);Image->SetDesiredSizeOverride(Size);return Image;
}
UWidget* ReserveIcon(UWidgetTree* Tree)
{
    auto* Canvas=Tree->ConstructWidget<UCanvasPanel>();
    Place(Canvas,Asset(Tree,TEXT("4513d.png"),{443,443},{36,36}),0,0,36,36);
    Place(Canvas,Asset(Tree,TEXT("988a7.svg"),{156,15},{12.7,1.22}),11.65f,17.39f,12.7f,1.22f);
    auto* Vertical=Asset(Tree,TEXT("4ca4a.svg"),{156,15},{12.7,1.22});Vertical->SetRenderTransformAngle(-90);
    Place(Canvas,Vertical,11.65f,17.39f,12.7f,1.22f);
    return Fixed(Tree,Canvas,36,36);
}
void FeedStyle(UVideoFeedButton* Button,bool Selected)
{
    FButtonStyle Style;
    Style.SetNormal(FSlateRoundedBoxBrush(Selected?FigmaUI::SelectedSurface:PanelColor,9.f,Selected?FigmaUI::Accent:Dim,Selected?.8f:.4f));
    Style.SetHovered(FSlateRoundedBoxBrush(FigmaUI::SelectedSurface,9.f,FigmaUI::Accent,.8f));
    Style.SetPressed(FSlateRoundedBoxBrush(FigmaUI::SelectedSurface,9.f,FigmaUI::Accent,1.2f));
    Style.SetDisabled(FSlateRoundedBoxBrush(PanelColor,9.f,Dim,.4f));
    Style.SetNormalPadding(FMargin(9,7)).SetPressedPadding(FMargin(9,7));
    Button->SetStyle(Style);Button->SetBackgroundColor(FLinearColor::White);
}
void ToolStyle(UButton* Button)
{
    FButtonStyle Style;
    Style.SetNormal(FSlateRoundedBoxBrush(FigmaUI::Surface,8.f,FigmaUI::Outline,.4f));
    Style.SetHovered(FSlateRoundedBoxBrush(FigmaUI::SelectedSurface,8.f,FigmaUI::Accent,.8f));
    Style.SetPressed(FSlateRoundedBoxBrush(FigmaUI::SelectedSurface,8.f,FigmaUI::Accent,1.f));
    Style.SetDisabled(FSlateRoundedBoxBrush(FigmaUI::Surface,8.f,FigmaUI::Outline,.4f));
    Style.SetNormalPadding(FMargin(8,0)).SetPressedPadding(FMargin(8,0));Button->SetStyle(Style);
}
}

UTextBlock* UVideoShellWidget::MakeText(const FString& Value,int32 Size)
{
    auto* Label=WidgetTree->ConstructWidget<UTextBlock>();Label->SetText(ProductText::Source(Value));
    FigmaUI::Text(Label,Size);Label->SetAutoWrapText(false);Label->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);return Label;
}

UVideoFeedButton* UVideoShellWidget::MakeFeedButton(bool Secondary)
{
    using namespace VideoUI;
    auto* Button=WidgetTree->ConstructWidget<UVideoFeedButton>();Button->Owner=this;Button->bSecondary=Secondary;
    FeedStyle(Button,false);Button->OnClicked.AddDynamic(Button,&UVideoFeedButton::ActivateFeed);
    Button->Label=MakeText(TEXT(""),Secondary?12:15);FigmaUI::Text(Button->Label,Secondary?12:15,FigmaUI::PrimaryText,true);
    Button->Availability=MakeText(TEXT(""),Secondary?9:10);
    Button->Model=MakeText(TEXT(""),Secondary?10:13);FigmaUI::Text(Button->Model,Secondary?10:13,Muted);
    Button->Source=MakeText(TEXT(""),13);FigmaUI::Text(Button->Source,13,Muted);
    auto* BadgeBox=WidgetTree->ConstructWidget<UBorder>();FigmaUI::Panel(BadgeBox,Badge,FLinearColor::Transparent,10,FMargin(4,1));BadgeBox->SetContent(Button->Availability);
    auto* Header=WidgetTree->ConstructWidget<UHorizontalBox>();
    Header->AddChildToHorizontalBox(Button->Label)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Header->AddChildToHorizontalBox(BadgeBox)->SetVerticalAlignment(VAlign_Center);
    auto* IconStack=WidgetTree->ConstructWidget<UOverlay>();
    Button->AssignedIcon=Asset(WidgetTree,Secondary?TEXT("88d4b.svg"):TEXT("f088c.svg"),Secondary?FVector2D(753,753):FVector2D(543.134,314.596),Secondary?FVector2D(33,33):FVector2D(45,27));
    Button->ReservedIcon=ReserveIcon(WidgetTree);
    for(auto* Icon:{Button->AssignedIcon.Get(),Button->ReservedIcon.Get()}){auto* IconSlot=IconStack->AddChildToOverlay(Icon);IconSlot->SetHorizontalAlignment(HAlign_Center);IconSlot->SetVerticalAlignment(VAlign_Center);}
    if(Secondary){
        auto* Column=WidgetTree->ConstructWidget<UVerticalBox>();Button->SetContent(Column);
        Column->AddChildToVerticalBox(Header);
        auto* Center=WidgetTree->ConstructWidget<UVerticalBox>();
        Center->AddChildToVerticalBox(Fixed(WidgetTree,IconStack,36,36))->SetHorizontalAlignment(HAlign_Center);
        Button->Source->SetJustification(ETextJustify::Center);Center->AddChildToVerticalBox(Button->Source)->SetPadding(FMargin(0,3,0,0));
        auto* CenterSlot=Column->AddChildToVerticalBox(Center);CenterSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));CenterSlot->SetVerticalAlignment(VAlign_Center);
        Column->AddChildToVerticalBox(Button->Model);
    }else{
        auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();Button->SetContent(Row);
        Button->IconPanel=WidgetTree->ConstructWidget<UBorder>();FigmaUI::Panel(Button->IconPanel,PanelColor,Dim,8,FMargin(0));Button->IconPanel->SetContent(IconStack);
        Row->AddChildToHorizontalBox(Fixed(WidgetTree,Button->IconPanel,51,54))->SetVerticalAlignment(VAlign_Center);
        auto* Body=WidgetTree->ConstructWidget<UVerticalBox>();auto* BodySlot=Row->AddChildToHorizontalBox(Body);
        BodySlot->SetPadding(FMargin(10,0,0,0));BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));BodySlot->SetVerticalAlignment(VAlign_Center);
        Body->AddChildToVerticalBox(Header);Body->AddChildToVerticalBox(Button->Model)->SetPadding(FMargin(0,2,0,0));Body->AddChildToVerticalBox(Button->Source);
    }
    auto* ContentSlot=Cast<UButtonSlot>(Button->GetContent()->Slot);ContentSlot->SetHorizontalAlignment(HAlign_Fill);ContentSlot->SetVerticalAlignment(VAlign_Fill);
    return Button;
}

void UVideoShellWidget::BuildPresentation()
{
    using namespace VideoUI;
    auto* Background=WidgetTree->ConstructWidget<UBorder>();FigmaUI::Panel(Background,FigmaUI::Background,FLinearColor::Transparent,0,FMargin(0));WidgetTree->RootWidget=Background;
    auto* Scale=WidgetTree->ConstructWidget<UScaleBox>();Scale->SetStretch(EStretch::ScaleToFit);Background->SetContent(Scale);
    DesignCanvas=WidgetTree->ConstructWidget<UCanvasPanel>();Scale->SetContent(Fixed(WidgetTree,DesignCanvas,1440,810));
    Place(DesignCanvas,CreateWidget<UFigmaRoleHeaderWidget>(GetOwningPlayer()),0,0,1440,90);
    auto Key=[&](const TCHAR* Name,int Size,bool Bold=false,FLinearColor Color=FigmaUI::PrimaryText){
        auto* Text=MakeText(TEXT(""),Size);Text->SetText(ProductText::Get(Name));FigmaUI::Text(Text,Size,Color,Bold);StaticLabels.Add(Text);StaticKeys.Add(Name);return Text;
    };
    auto Panel=[&](float X,float Y,float W,float H){
        auto* Border=WidgetTree->ConstructWidget<UBorder>();FigmaUI::Panel(Border,PanelColor,FLinearColor::Transparent,9,FMargin(0));Place(DesignCanvas,Border,X,Y,W,H);return Border;
    };
    Place(DesignCanvas,Key(TEXT("VideoUI.Title"),36,true),31,96,600,47);
    auto* Note=Key(TEXT("VideoUI.Single"),12,false,Muted);Note->SetJustification(ETextJustify::Right);Place(DesignCanvas,Note,820,110,588,24);
    Panel(31,157,259,574);
    Place(DesignCanvas,Key(TEXT("VideoUI.Aircraft"),20,true),44,166,230,26);
    Place(DesignCanvas,Key(TEXT("VideoUI.SelectSource"),14,false,Muted),44,193,230,20);
    auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>();Scroll->SetScrollbarThickness({3,3});Scroll->SetAnimateWheelScrolling(true);
    Place(DesignCanvas,Scroll,38,220,245,482);
    AircraftList=WidgetTree->ConstructWidget<UVerticalBox>();Scroll->AddChild(AircraftList);
    Place(DesignCanvas,Key(TEXT("VideoUI.ModelDisclaimer"),9,false,Muted),39,709,244,16);
    PrimaryPanel=Panel(303,157,816,432);
    Heading=Key(TEXT("VideoUI.Primary"),20,true);Place(DesignCanvas,Heading,322,166,260,27);
    Status=MakeText(TEXT(""),11);Status->SetJustification(ETextJustify::Right);Place(DesignCanvas,Status,650,171,265,20);
    Place(DesignCanvas,Key(TEXT("VideoUI.Current"),13),941,171,83,20);
    auto MakeTarget=[&](float X){
        auto* BadgeBox=WidgetTree->ConstructWidget<UBorder>();FigmaUI::Panel(BadgeBox,Badge,FLinearColor::Transparent,6,FMargin(0));
        auto* Text=MakeText(TEXT(""),14);Text->SetJustification(ETextJustify::Center);BadgeBox->SetContent(Text);BadgeBox->SetVerticalAlignment(VAlign_Center);
        Place(DesignCanvas,BadgeBox,X,169,73,22);return Text;
    };
    PrimaryTarget=MakeTarget(1027);DetectionTarget=MakeTarget(1318);
    VideoSurface=WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(),TEXT("VideoSurface"));
    Place(DesignCanvas,VideoSurface,322,200,777,354);VideoSurface->SetClipping(EWidgetClipping::ClipToBounds);
    auto* Matte=WidgetTree->ConstructWidget<UBorder>();FigmaUI::Panel(Matte,FigmaUI::Background,FLinearColor::Transparent,9,FMargin(0));Fill(VideoSurface,Matte);
    Browser=WidgetTree->ConstructWidget<UWebBrowser>(UWebBrowser::StaticClass(),TEXT("VideoView"));
    Browser->OnUrlChanged.AddDynamic(this,&UVideoShellWidget::UrlChanged);Browser->OnConsoleMessage.AddDynamic(this,&UVideoShellWidget::ConsoleMessage);Fill(VideoSurface,Browser);
    auto* Detection=WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("DetectionOverlay"));Detection->SetVisibility(ESlateVisibility::HitTestInvisible);Fill(VideoSurface,Detection);
    auto* StatusLayer=WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(),TEXT("StatusOverlay"));StatusLayer->SetVisibility(ESlateVisibility::HitTestInvisible);Fill(VideoSurface,StatusLayer);
    MediaCover=WidgetTree->ConstructWidget<UBorder>();FigmaUI::Panel(MediaCover,FigmaUI::Background,FLinearColor::Transparent,9,FMargin(0));
    MediaCover->SetHorizontalAlignment(HAlign_Center);MediaCover->SetVerticalAlignment(VAlign_Center);
    auto* Empty=WidgetTree->ConstructWidget<UVerticalBox>();MediaCover->SetContent(Empty);Fill(StatusLayer,MediaCover);
    Empty->AddChildToVerticalBox(Asset(WidgetTree,TEXT("88d4b.svg"),{753,753},{62,62}))->SetHorizontalAlignment(HAlign_Center);
    EmptyVideo=MakeText(TEXT(""),20);FigmaUI::Text(EmptyVideo,20,FigmaUI::PrimaryText,true);EmptyVideo->SetJustification(ETextJustify::Center);
    Empty->AddChildToVerticalBox(EmptyVideo)->SetPadding(FMargin(0,4,0,0));
    EmptyHint=MakeText(TEXT(""),14);FigmaUI::Text(EmptyHint,14,Muted);EmptyHint->SetJustification(ETextJustify::Center);Empty->AddChildToVerticalBox(EmptyHint)->SetPadding(FMargin(0,4,0,0));
    auto* Interaction=WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("UserInteractionLayer"));Interaction->SetVisibility(ESlateVisibility::SelfHitTestInvisible);Fill(VideoSurface,Interaction);
    Details=MakeText(TEXT(""),12);FigmaUI::Text(Details,12,Muted);Place(DesignCanvas,Details,322,562,777,21);
    SecondaryArea=WidgetTree->ConstructWidget<UHorizontalBox>();Place(DesignCanvas,SecondaryArea,300,599,822,132);
    for(int I=0;I<5;++I){auto* Card=MakeFeedButton(true);SecondaryButtons.Add(Card);auto* FeedSlot=SecondaryArea->AddChildToHorizontalBox(Card);FeedSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));FeedSlot->SetPadding(FMargin(4,0));}
    Panel(1131,157,277,193);
    // The AI target badge is above its panel in z-order, and never claims detection results.
    Cast<UCanvasPanelSlot>(DetectionTarget->GetParent()->Slot)->SetZOrder(1);
    Place(DesignCanvas,Key(TEXT("VideoUI.Detection"),20,true),1146,166,164,27);
    Place(DesignCanvas,Asset(WidgetTree,TEXT("be4b8.svg"),{3089.016,5},{253,.41}),1144,200,253,.41f);
    const TCHAR* Categories[]={TEXT("VideoUI.People"),TEXT("VideoUI.Vehicles"),TEXT("VideoUI.Drones"),TEXT("VideoUI.Behavior"),TEXT("VideoUI.Other")};
    for(int I=0;I<5;++I){Place(DesignCanvas,Key(Categories[I],14),1146,208+27.4f*I,195,22);auto* Value=MakeText(TEXT("--"),14);Value->SetJustification(ETextJustify::Right);Place(DesignCanvas,Value,1350,208+27.4f*I,46,22);}
    Panel(1131,367,277,179);Place(DesignCanvas,Key(TEXT("VideoUI.Alerts"),20,true),1146,374,245,27);
    auto* AlertsBorder=WidgetTree->ConstructWidget<UBorder>();FigmaUI::Panel(AlertsBorder,PanelColor,Dim,5,FMargin(7));Place(DesignCanvas,AlertsBorder,1146,408,250,120);
    auto* AlertsOverlay=WidgetTree->ConstructWidget<UOverlay>();AlertsBorder->SetContent(AlertsOverlay);
    auto* EmptyAlert=WidgetTree->ConstructWidget<UVerticalBox>();AlertEmpty=EmptyAlert;
    EmptyAlert->AddChildToVerticalBox(Asset(WidgetTree,TEXT("0949b.svg"),{463,463},{38,38}))->SetHorizontalAlignment(HAlign_Center);
    auto* NoAlert=Key(TEXT("VideoUI.NoAlerts"),16,true);NoAlert->SetJustification(ETextJustify::Center);EmptyAlert->AddChildToVerticalBox(NoAlert)->SetPadding(FMargin(0,5,0,0));
    auto* AlertHint=Key(TEXT("VideoUI.AlertSession"),12,false,Muted);AlertHint->SetJustification(ETextJustify::Center);EmptyAlert->AddChildToVerticalBox(AlertHint)->SetPadding(FMargin(0,4,0,0));
    auto* EmptySlot=AlertsOverlay->AddChildToOverlay(EmptyAlert);EmptySlot->SetHorizontalAlignment(HAlign_Center);EmptySlot->SetVerticalAlignment(VAlign_Center);
    auto* AlertScroll=WidgetTree->ConstructWidget<UScrollBox>();AlertScroll->SetScrollbarThickness({2,2});Fill(AlertsOverlay,AlertScroll);AlertRows=WidgetTree->ConstructWidget<UVerticalBox>();AlertScroll->AddChild(AlertRows);
    Panel(1131,563,277,168);Place(DesignCanvas,Key(TEXT("VideoUI.System"),20,true),1146,573,245,27);
    Place(DesignCanvas,Asset(WidgetTree,TEXT("be4b8.svg"),{3089.016,5},{253,.41}),1145,607,253,.41f);
    const TCHAR* Metrics[]={TEXT("VideoUI.Online"),TEXT("VideoUI.AI"),TEXT("VideoUI.Storage"),TEXT("VideoUI.Network")};
    for(int I=0;I<4;++I){Place(DesignCanvas,Key(Metrics[I],14),1146,618+27.4f*I,140,22);
        auto* Value=I==0||I==3?MakeText(TEXT(""),13):Key(I==1?TEXT("VideoUI.NotConnected"):TEXT("VideoUI.Unavailable"),13,false,Muted);
        Value->SetJustification(ETextJustify::Right);Place(DesignCanvas,Value,1285,618+27.4f*I,111,22);if(I==0)OnlineValue=Value;if(I==3)NetworkValue=Value;}
    auto* Modes=WidgetTree->ConstructWidget<UHorizontalBox>();auto* ModePanel=WidgetTree->ConstructWidget<UBorder>();FigmaUI::Panel(ModePanel,FigmaUI::Surface,FLinearColor::Transparent,20,FMargin(0));ModePanel->SetContent(Modes);Place(DesignCanvas,ModePanel,31,750,285,38);
    const TCHAR* ModesKeys[]={TEXT("VideoUI.Four"),TEXT("VideoUI.Six"),TEXT("VideoUI.Focus")};
    for(int I=0;I<3;++I){auto* Button=WidgetTree->ConstructWidget<UButton>();Button->SetContent(Key(ModesKeys[I],14));ModeButtons.Add(Button);
        Modes->AddChildToHorizontalBox(Button)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));}
    ModeButtons[0]->OnClicked.AddDynamic(this,&UVideoShellWidget::ViewFour);ModeButtons[1]->OnClicked.AddDynamic(this,&UVideoShellWidget::ViewSix);ModeButtons[2]->OnClicked.AddDynamic(this,&UVideoShellWidget::ViewFocus);
    auto* Divider=Asset(WidgetTree,TEXT("326dd.svg"),{315.129,7},{26,.58});Divider->SetRenderTransformAngle(-90);Place(DesignCanvas,Divider,334,768,26,.58f);
    // Existing explicit video-to-selection action remains available without changing source semantics.
    SetActiveButton=WidgetTree->ConstructWidget<UButton>();SetActiveButton->SetContent(Key(TEXT("VideoUI.SetActive"),12));ToolStyle(SetActiveButton);
    SetActiveButton->OnClicked.AddDynamic(this,&UVideoShellWidget::PublishActive);Place(DesignCanvas,SetActiveButton,369,750,164,40);
    SystemHeader=MakeText(TEXT(""),11);FigmaUI::Text(SystemHeader,11,Muted);Place(DesignCanvas,SystemHeader,548,759,275,22);
    const TCHAR* Names[]={TEXT("VideoUI.Retry"),TEXT("VideoUI.Record"),TEXT("VideoUI.Playback"),TEXT("VideoUI.Screenshot"),TEXT("VideoUI.Filter")};
    const TCHAR* Icons[]={TEXT("a9033.svg"),TEXT("85010.svg"),TEXT("7b89e.svg"),TEXT("a03b0.svg"),TEXT("15368.svg")};
    const int Sizes[]={224,225,225,227,220};
    for(int I=0;I<5;++I){
        auto* Button=WidgetTree->ConstructWidget<UButton>();ToolStyle(Button);auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();Button->SetContent(Row);
        auto* Icon=Asset(WidgetTree,Icons[I],{double(Sizes[I]),double(Sizes[I])},{18,18});
        if(I==2)Icon->SetRenderTransformAngle(180); // Figma playback instance faces right.
        if(I==0){
            auto* IconsLayer=WidgetTree->ConstructWidget<UOverlay>();RetryReadyIcon=Asset(WidgetTree,TEXT("2ad6e.svg"),{224,224},{18,18});
            for(auto* Layer:{Icon,RetryReadyIcon.Get()}){auto* IconSlot=IconsLayer->AddChildToOverlay(Layer);IconSlot->SetHorizontalAlignment(HAlign_Center);IconSlot->SetVerticalAlignment(VAlign_Center);}
            Row->AddChildToHorizontalBox(Fixed(WidgetTree,IconsLayer,18,18))->SetVerticalAlignment(VAlign_Center);
            auto Style=Button->GetStyle();Style.SetNormal(FSlateRoundedBoxBrush(FigmaUI::Background,8.f,FigmaUI::Outline,.4f));Button->SetStyle(Style);
        }else Row->AddChildToHorizontalBox(Icon)->SetVerticalAlignment(VAlign_Center);
        auto* Caption=Key(Names[I],12,true,Dim);auto* CaptionSlot=Row->AddChildToHorizontalBox(Caption);CaptionSlot->SetVerticalAlignment(VAlign_Center);CaptionSlot->SetPadding(FMargin(8,0,0,0));
        auto* BS=Cast<UButtonSlot>(Row->Slot);BS->SetHorizontalAlignment(HAlign_Center);BS->SetVerticalAlignment(VAlign_Center);
        Place(DesignCanvas,Button,839+117.f*I,750,101,40);
        if(I==0){Retry=Button;RetryIcon=Icon;RetryCaption=Caption;Button->OnClicked.AddDynamic(this,&UVideoShellWidget::RetrySource);}
        else{Button->SetIsEnabled(false);UnsupportedButtons.Add(Button);}
    }
    ProbeToken=FGuid::NewGuid().ToString(EGuidFormats::Digits);SetViewMode(ViewMode);
}

TSharedRef<SWidget> UVideoShellWidget::RebuildWidget()
{
    if(!WidgetTree->RootWidget)BuildPresentation();
    return Super::RebuildWidget();
}

void UVideoShellWidget::SetViewMode(int32 Channels)
{
    using namespace VideoUI;
    if(Channels!=1 && Channels!=4 && Channels!=6)return;ViewMode=Channels;
    if(!SecondaryArea)return;
    SecondaryArea->SetVisibility(Channels==1?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
    for(int I=0;I<SecondaryButtons.Num();++I)SecondaryButtons[I]->SetVisibility(I<Channels-1?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    Cast<UCanvasPanelSlot>(PrimaryPanel->Slot)->SetSize({816,Channels==1?574.f:432.f});
    Cast<UCanvasPanelSlot>(VideoSurface->Slot)->SetSize({777,Channels==1?491.f:354.f});
    Cast<UCanvasPanelSlot>(Details->Slot)->SetPosition({322,Channels==1?702.f:562.f});
    const int Modes[]={4,6,1};for(int I=0;I<ModeButtons.Num();++I){
        FigmaUI::Button(ModeButtons[I],Channels==Modes[I],true);
        auto Style=ModeButtons[I]->GetStyle();Style.SetNormalPadding(FMargin(4,0)).SetPressedPadding(FMargin(4,0));ModeButtons[I]->SetStyle(Style);}
}

void UVideoShellWidget::UpdateFeed(UVideoFeedButton* Button)
{
    using namespace VideoUI;
    FDroneDescriptor Descriptor;const bool Assigned=Registry->GetDroneDescriptor(Button->DroneId,Descriptor) && !Descriptor.bIsEnemyTarget;
    const bool Selected=Assigned && Button->DroneId==DisplayedDroneId;FeedStyle(Button,Selected);
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    Button->SetIsEnabled(Assigned && !bStopped && (!Sync->IsEnabled() || Sync->IsReady()));
    Button->Label->SetText(Assigned?FText::AsCultureInvariant(FString::Printf(TEXT("UAV-%02d"),Button->DroneId)):ProductText::Get(TEXT("VideoUI.Reserved")));
    FDroneTelemetrySnapshot Telemetry;const bool HasTelemetry=Assigned && Registry->GetTelemetry(Button->DroneId,Telemetry);
    const TCHAR* State=!HasTelemetry?TEXT("VideoUI.NA"):Telemetry.Availability==EDroneAvailability::Online?TEXT("Fleet.Online"):Telemetry.Availability==EDroneAvailability::Lost?TEXT("Fleet.Lost"):TEXT("Fleet.Offline");
    Button->Availability->SetText(ProductText::Get(State));Button->Availability->GetParent()->SetVisibility(Assigned?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    Button->Model->SetText(ProductText::Get(Assigned?TEXT("VideoUI.ModelUnavailable"):TEXT("VideoUI.Unassigned")));
    Button->Source->SetText(ProductText::Get(!Assigned?TEXT("VideoUI.NoSource"):Descriptor.VideoUrl.IsEmpty()?TEXT("VideoUI.NoSource"):Button->bSecondary?TEXT("VideoUI.InactiveSource"):TEXT("VideoUI.SourceSet")));
    Button->AssignedIcon->SetVisibility(Assigned?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
    Button->ReservedIcon->SetVisibility(!Assigned?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
    if(Button->IconPanel)FigmaUI::Panel(Button->IconPanel,Selected?FigmaUI::Accent:PanelColor,Assigned?Dim:FLinearColor::Transparent,8,FMargin(0));
    Button->SetToolTipText(Assigned?FText::Format(ProductText::Get(TEXT("Video.DescriptorTip")),FText::AsCultureInvariant(Descriptor.Name),FText::AsCultureInvariant(GetDroneModelDisplayName(Registry->GetDroneModelType(Button->DroneId)))):ProductText::Get(TEXT("VideoUI.Unassigned")));
}

void UVideoShellWidget::UpdateFeeds()
{
    auto Drones=Registry->GetFriendlyDroneDescriptors();Drones.Sort([](const FDroneDescriptor& A,const FDroneDescriptor& B){return A.DroneId<B.DroneId;});
    TArray<int32> Ids;for(const auto& D:Drones)Ids.Add(D.DroneId);
    if(AircraftIds!=Ids || AircraftButtons.IsEmpty()){
        AircraftIds=Ids;AircraftList->ClearChildren();AircraftButtons.Reset();
        for(int I=0;I<FMath::Max(6,Ids.Num());++I){auto* B=MakeFeedButton();B->DroneId=Ids.IsValidIndex(I)?Ids[I]:0;AircraftButtons.Add(B);
            AircraftList->AddChildToVerticalBox(VideoUI::Fixed(WidgetTree,B,235,80))->SetPadding(FMargin(0,0,0,8));}
    }
    SecondaryIds.RemoveAll([&](int32 Id){return !Ids.Contains(Id) || Id==DisplayedDroneId;});
    for(int32 Id:Ids)if(Id!=DisplayedDroneId)SecondaryIds.AddUnique(Id);
    for(auto B:AircraftButtons)UpdateFeed(B);
    for(int I=0;I<SecondaryButtons.Num();++I){SecondaryButtons[I]->DroneId=GetSecondaryDroneId(I);UpdateFeed(SecondaryButtons[I]);}
}

void UVideoShellWidget::UpdatePresentation()
{
    if(!DesignCanvas || !Registry.IsValid())return;
    for(int I=0;I<StaticLabels.Num();++I)StaticLabels[I]->SetText(ProductText::Get(StaticKeys[I]));
    const auto Target=DisplayedDroneId>0?FText::AsCultureInvariant(FString::Printf(TEXT("UAV-%02d"),DisplayedDroneId)):ProductText::Get(TEXT("VideoUI.NA"));
    PrimaryTarget->SetText(Target);DetectionTarget->SetText(Target);
    int Online=0;const auto Drones=Registry->GetFriendlyDroneDescriptors();for(const auto& D:Drones){FDroneTelemetrySnapshot T;if(Registry->GetTelemetry(D.DroneId,T)&&T.Availability==EDroneAvailability::Online)++Online;}
    OnlineValue->SetText(FText::AsCultureInvariant(FString::Printf(TEXT("%d / %d"),Online,Drones.Num())));
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    NetworkValue->SetText(ProductText::Get(Sync->IsReady()?TEXT("Common.Online"):TEXT("Common.Offline")));NetworkValue->SetToolTipText(FText::AsCultureInvariant(Sync->GetStatusText()));
    FString Active;if(Sync->GetContext())Sync->GetContext()->TryGetStringField(TEXT("active_uav_id"),Active);
    SystemHeader->SetText(FText::Format(ProductText::Get(TEXT("VideoUI.ActiveSelection")),Active.IsEmpty()?ProductText::Get(TEXT("VideoUI.NA")):FText::AsCultureInvariant(Active)));
    if(!Sync->ViewSyncError.IsEmpty())SystemHeader->SetText(ProductText::Get(TEXT("Errors.")+Sync->ViewSyncError));
    SystemHeader->SetToolTipText(SystemHeader->GetText());
    SetActiveButton->SetIsEnabled(!bStopped && DisplayedDroneId>0 && Sync->IsReady());SetActiveButton->SetToolTipText(ProductText::Get(TEXT("VideoUI.SetActiveHint")));
    for(auto B:UnsupportedButtons)B->SetToolTipText(ProductText::Get(TEXT("VideoUI.Unsupported")));
    // AlertStore contains received Backend/session events. Do not manufacture AI detections.
    const auto Alerts=GetGameInstance()->GetSubsystem<UCommandAlertStore>()->GetAlerts();
    FString Signature=ProductText::Get(TEXT("VideoUI.Alerts")).ToString();for(const auto& A:Alerts)if(!A.bHandled)Signature+=FString::Printf(TEXT("|%d:%s:%s"),A.Id,*A.Type,*A.Message);
    if(Signature!=AlertSignature){
        AlertSignature=Signature;AlertRows->ClearChildren();int Count=0;
        for(int I=Alerts.Num()-1;I>=0;--I){const auto& A=Alerts[I];if(A.bHandled)continue;++Count;
            auto* Label=MakeText(TEXT(""),12);Label->SetText(FText::AsCultureInvariant(FString::Printf(TEXT("%s  UAV-%02d\n%s"),*A.ReceivedAt.ToString(TEXT("%H:%M")),A.DroneId,*A.Message)));
            Label->SetAutoWrapText(true);FigmaUI::Text(Label,12,FigmaUI::Warning);AlertRows->AddChildToVerticalBox(Label)->SetPadding(FMargin(0,0,0,7));}
        AlertEmpty->SetVisibility(Count?ESlateVisibility::Collapsed:ESlateVisibility::SelfHitTestInvisible);
    }
    SetViewMode(ViewMode);
}
