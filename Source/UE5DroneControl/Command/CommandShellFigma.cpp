#include "Command/CommandShellWidget.h"
#include "Command/FigmaCommandStyle.h"
#include "Command/FigmaFleetWidget.h"
#include "Command/CommandActionButton.h"
#include "Command/CommandCenterPanel.h"
#include "Command/CommandTacticalMap.h"
#include "Command/CommandAlertStore.h"
#include "Shared/ActiveExecutionControlsWidget.h"
#include "Shared/OperationalContextSubsystem.h"
#include "Shared/OperationalEventStore.h"
#include "Shared/PlanConfiguration.h"
#include "Shared/PlanWidgetSupport.h"
#include "Shared/ProductText.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
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
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"

namespace {
UTextBlock* Label(UWidgetTree* T,const FText& Text,int32 Size=14,bool Bold=false) {
    auto* L=T->ConstructWidget<UTextBlock>();L->SetText(Text);FigmaUI::Text(L,Size,FigmaUI::PrimaryText,Bold);return L;
}
UTextBlock* Key(UWidgetTree* T,const TCHAR* K,int32 Size=14,bool Bold=false){return Label(T,ProductText::Get(K),Size,Bold);}
void Fill(UHorizontalBoxSlot* S,float Weight=1){FSlateChildSize Z(ESlateSizeRule::Fill);Z.Value=Weight;S->SetSize(Z);}
void Fill(UVerticalBoxSlot* S,float Weight=1){FSlateChildSize Z(ESlateSizeRule::Fill);Z.Value=Weight;S->SetSize(Z);}
USizeBox* Fixed(UWidgetTree* T,UWidget* W,float Width,float Height){auto* B=T->ConstructWidget<USizeBox>();if(Width>0)B->SetWidthOverride(Width);if(Height>0)B->SetHeightOverride(Height);B->SetContent(W);return B;}
UCanvasPanelSlot* Place(UCanvasPanel* P,UWidget* W,FVector2D Position,FVector2D Size){auto* S=P->AddChildToCanvas(W);S->SetPosition(Position);S->SetSize(Size);return S;}
FText Number(int32 N){return FText::AsNumber(N);}
FLinearColor EventColor(EOperationalLevel L){return L==EOperationalLevel::CRITICAL?FLinearColor(FColor(241,91,104)):L==EOperationalLevel::WARNING?FLinearColor(FColor(235,180,83)):FLinearColor(FColor(69,219,90));}
}

void UCommandShellWidget::BuildFigmaShell(UCanvasPanel* Root) {
    // Design uses a 16:9 command surface. Scale the complete surface as one unit,
    // so the layout and modal hit regions agree at all viewport resolutions.
    auto* Backdrop=WidgetTree->ConstructWidget<UBorder>();Backdrop->SetBrushColor(FigmaUI::Background);Backdrop->SetPadding(FMargin(0));
    auto* Scale=WidgetTree->ConstructWidget<UScaleBox>();Scale->SetStretch(EStretch::ScaleToFit);Backdrop->SetContent(Scale);
    Scale->SetContent(Fixed(WidgetTree,Root,1440,810));WidgetTree->RootWidget=Backdrop;
    Header->RemoveFromParent();
    Header=WidgetTree->ConstructWidget<UBorder>();Header->SetBrushColor(FLinearColor::Transparent);Header->SetPadding(FMargin(0));
    auto* Head=WidgetTree->ConstructWidget<UCanvasPanel>();Header->SetContent(Head);Place(Root,Header,{30,14},{1380,77});
    Place(Head,FigmaUI::Icon(WidgetTree,TEXT("brand-mark"),{44,34}),{0,6},{44,25.5});
    Place(Head,FigmaUI::Icon(WidgetTree,TEXT("brand-wordmark"),{184,28}),{52,11},{184,16.5});
    auto* Divider=WidgetTree->ConstructWidget<UBorder>();Divider->SetBrushColor(FigmaUI::Muted);Place(Head,Divider,{253,6},{1,25});
    Place(Head,Key(WidgetTree,TEXT("Stage1.Command"),14),{271,9},{130,25});
    auto ActionButton=[&](const TCHAR* Action,const TCHAR* TextKey,int32 Id=0) {
        auto* B=WidgetTree->ConstructWidget<UCommandActionButton>();B->Configure(Action,Id);B->OnAction.AddDynamic(this,&UCommandShellWidget::FigmaAction);
        FigmaUI::Button(B,false,true);B->SetContent(Key(WidgetTree,TextKey));return B;
    };
    auto* NavPlate=WidgetTree->ConstructWidget<UBorder>();FigmaUI::Panel(NavPlate,FigmaUI::Surface,FLinearColor::Transparent,24,FMargin(0));
    auto* Nav=WidgetTree->ConstructWidget<UHorizontalBox>();NavPlate->SetContent(Nav);Place(Head,NavPlate,{555,4},{324,34});
    const TCHAR* NavKeys[]={TEXT("Nav.Overview"),TEXT("Nav.Plans"),TEXT("Nav.Fleet")};
    for(int I=0;I<3;++I){auto* B=ActionButton(TEXT("navigate"),NavKeys[I],I);Fill(Nav->AddChildToHorizontalBox(B),I==1?1.4f:1.f);PrimaryNavigationButtons.Add(B);FigmaUI::Button(B,I==0,true);}
    auto* LangPlate=WidgetTree->ConstructWidget<UBorder>();FigmaUI::Panel(LangPlate,FigmaUI::Surface,FLinearColor::Transparent,24,FMargin(0));
    auto* Lang=WidgetTree->ConstructWidget<UHorizontalBox>();LangPlate->SetContent(Lang);Place(Head,LangPlate,{1040,4},{178,34});
    for(const TCHAR* L:{TEXT("zh-Hans"),TEXT("en")}){auto* B=ActionButton(L,FString(L)==TEXT("en")?TEXT("Workflow.English"):TEXT("Workflow.Chinese"));Fill(Lang->AddChildToHorizontalBox(B));LanguageButtons.Add(B);}
    HeaderClock=Label(WidgetTree,FText::GetEmpty(),13);HeaderClock->SetJustification(ETextJustify::Right);Place(Head,HeaderClock,{1230,11},{150,26});
    auto* Health=WidgetTree->ConstructWidget<UHorizontalBox>();Place(Head,Health,{0,52},{950,22});
    HealthOnlineIcon=FigmaUI::Icon(WidgetTree,TEXT("status-online"),{9,9});HealthOfflineIcon=FigmaUI::Icon(WidgetTree,TEXT("status-offline"),{9,9});
    for(int I=0;I<4;++I){auto* Dot=FigmaUI::Icon(WidgetTree,TEXT("status-offline"),{9,9});auto* DS=Health->AddChildToHorizontalBox(Dot);DS->SetVerticalAlignment(VAlign_Center);DS->SetPadding(FMargin(0,0,8,0));HealthDots.Add(Dot);auto* L=Label(WidgetTree,FText::GetEmpty(),12);Health->AddChildToHorizontalBox(L)->SetPadding(FMargin(0,0,24,0));HealthLabels.Add(L);}

    WorkspacePages=WidgetTree->ConstructWidget<UWidgetSwitcher>();auto* WorkspaceSlot=Root->AddChildToCanvas(WorkspacePages);
    WorkspaceSlot->SetAnchors(FAnchors(0,0,1,1));WorkspaceSlot->SetOffsets(FMargin(30,102,30,50));
    auto* Home=WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("FigmaOverview"));WorkspacePages->AddChild(Home);
    auto* Heading=WidgetTree->ConstructWidget<UHorizontalBox>();Home->AddChildToVerticalBox(Heading)->SetPadding(FMargin(0,0,0,14));
    auto* HeadingTitle=Heading->AddChildToHorizontalBox(Key(WidgetTree,TEXT("Nav.Overview"),34,true));Fill(HeadingTitle);HeadingTitle->SetVerticalAlignment(VAlign_Center);
    auto* Stats=WidgetTree->ConstructWidget<UHorizontalBox>();Home->AddChildToVerticalBox(Fixed(WidgetTree,Stats,0,78))->SetPadding(FMargin(0,0,0,16));
    const TCHAR* StatKeys[]={TEXT("Figma.TotalUAV"),TEXT("Figma.OnlineDevices"),TEXT("Nav.Alerts"),TEXT("Nav.Logs")};
    const TCHAR* StatActions[]={TEXT("fleet"),TEXT("devices"),TEXT("alerts"),TEXT("logs")};
    const TCHAR* Icons[]={TEXT("drone"),TEXT("online"),TEXT("alarm"),TEXT("health")};
    const float IconSizes[]={34.f,24.f,27.f,28.f};
    for(int I=0;I<4;++I){
        auto* B=ActionButton(StatActions[I],StatKeys[I]);B->SetToolTipText(ProductText::Get(StatKeys[I]));
        FButtonStyle Style=B->GetStyle();Style.SetNormal(FSlateRoundedBoxBrush(FigmaUI::Surface,10,FigmaUI::Outline,0.5));Style.SetNormalPadding(FMargin(16,10));B->SetStyle(Style);
        auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();B->SetContent(Row);Cast<UButtonSlot>(Row->Slot)->SetHorizontalAlignment(HAlign_Fill);
        auto* IconPlate=WidgetTree->ConstructWidget<UOverlay>();
        for(auto* Icon:{FigmaUI::Icon(WidgetTree,TEXT("icon-disc"),{46,46}),FigmaUI::Icon(WidgetTree,Icons[I],FVector2D(IconSizes[I]))}){
            auto* IconSlot=IconPlate->AddChildToOverlay(Icon);IconSlot->SetHorizontalAlignment(HAlign_Center);IconSlot->SetVerticalAlignment(VAlign_Center);
        }
        Row->AddChildToHorizontalBox(Fixed(WidgetTree,IconPlate,46,46))->SetVerticalAlignment(VAlign_Center);
        auto* Labels=WidgetTree->ConstructWidget<UVerticalBox>();auto* LS=Row->AddChildToHorizontalBox(Labels);Fill(LS);LS->SetPadding(FMargin(16,0));
        Labels->AddChild(Key(WidgetTree,StatKeys[I],13));auto* Count=Label(WidgetTree,Number(0),28);Labels->AddChild(Count);StatisticValues.Add(Count);
        Row->AddChildToHorizontalBox(FigmaUI::Icon(WidgetTree,TEXT("arrow-right"),{29,29}))->SetVerticalAlignment(VAlign_Center);
        auto* S=Stats->AddChildToHorizontalBox(B);Fill(S);S->SetPadding(FMargin(0,0,I<3?18:0,0));
    }
    // Figma 2522:230 / 2522:237: equal task and plan cards below the metrics.
    // The live task list can move into the side column when executions need
    // more room, without shrinking the map used to choose destinations.
    auto* Summary=WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),TEXT("FigmaOverviewSummaryRow"));
    Home->AddChildToVerticalBox(Summary)->SetPadding(FMargin(0,0,0,16));
    auto SummaryIcon=[&](const TCHAR* Name,FName WidgetName=NAME_None){
        auto* Plate=WidgetTree->ConstructWidget<UBorder>();
        FigmaUI::Panel(Plate,FLinearColor(FColor(30,34,46)),FLinearColor::Transparent,6,FMargin(0));
        Plate->SetContent(FigmaUI::Icon(WidgetTree,Name,{25,25}));
        if(auto* IconSlot=Cast<UBorderSlot>(Plate->GetContent()->Slot)){IconSlot->SetHorizontalAlignment(HAlign_Center);IconSlot->SetVerticalAlignment(VAlign_Center);}
        auto* Size=WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),WidgetName);Size->SetWidthOverride(44);Size->SetHeightOverride(44);Size->SetContent(Plate);return Size;
    };
    auto* TaskCard=WidgetTree->ConstructWidget<UBorder>();FigmaUI::Panel(TaskCard,FigmaUI::Surface,FigmaUI::Outline,9,FMargin(16,10));
    OverviewSummarySize=Fixed(WidgetTree,TaskCard,0,70);
    auto* TaskSlot=Summary->AddChildToHorizontalBox(OverviewSummarySize);Fill(TaskSlot);TaskSlot->SetPadding(FMargin(0,0,14,0));
    auto* TaskRow=WidgetTree->ConstructWidget<UHorizontalBox>();TaskCard->SetContent(TaskRow);
    auto* TaskIconSlot=TaskRow->AddChildToHorizontalBox(SummaryIcon(TEXT("log")));TaskIconSlot->SetVerticalAlignment(VAlign_Top);TaskIconSlot->SetPadding(FMargin(0,3,12,0));
    auto* TaskBody=WidgetTree->ConstructWidget<UVerticalBox>();Fill(TaskRow->AddChildToHorizontalBox(TaskBody));
    RunningTaskTitle=Label(WidgetTree,FText::GetEmpty(),16,true);TaskBody->AddChild(RunningTaskTitle);
    RunningTaskEmpty=Key(WidgetTree,TEXT("Execution.None"),13);TaskBody->AddChildToVerticalBox(RunningTaskEmpty)->SetPadding(FMargin(0,5,0,0));
    auto* TaskScroll=WidgetTree->ConstructWidget<UScrollBox>();Fill(TaskBody->AddChildToVerticalBox(TaskScroll));
    RunningTaskRows=WidgetTree->ConstructWidget<UVerticalBox>();TaskScroll->AddChild(RunningTaskRows);
    auto* EmptyTaskAction=WidgetTree->ConstructWidget<UCommandActionButton>(UCommandActionButton::StaticClass(),TEXT("FigmaOverviewEmptyTaskAction"));
    EmptyTaskAction->SetContent(Key(WidgetTree,TEXT("Overview.ViewTask"),13));EmptyTaskAction->SetIsEnabled(false);FigmaUI::Button(EmptyTaskAction,false,true);
    auto* EmptyActionSlot=TaskRow->AddChildToHorizontalBox(EmptyTaskAction);EmptyActionSlot->SetVerticalAlignment(VAlign_Center);EmptyActionSlot->SetPadding(FMargin(12,0,0,0));
    CurrentExecutionSummary=Label(WidgetTree,FText::GetEmpty(),13);
    auto* PlanCard=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("FigmaOverviewPlanCard"));FigmaUI::Panel(PlanCard,FigmaUI::Surface,FigmaUI::Outline,9,FMargin(16,10));
    auto* PlanSize=WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),TEXT("FigmaOverviewPlan"));PlanSize->SetHeightOverride(70);PlanSize->SetContent(PlanCard);Fill(Summary->AddChildToHorizontalBox(PlanSize));
    auto* PlanRow=WidgetTree->ConstructWidget<UHorizontalBox>();PlanCard->SetContent(PlanRow);
    auto* PlanIcon=SummaryIcon(TEXT("plan"),TEXT("FigmaOverviewPlanIcon"));
    PlanRow->AddChildToHorizontalBox(PlanIcon)->SetVerticalAlignment(VAlign_Center);
    auto* PlanTexts=WidgetTree->ConstructWidget<UVerticalBox>();auto* PTS=PlanRow->AddChildToHorizontalBox(PlanTexts);Fill(PTS);PTS->SetPadding(FMargin(12,0));PTS->SetVerticalAlignment(VAlign_Center);
    auto* PlanTitle=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("FigmaOverviewPlanTitle"));PlanTitle->SetText(ProductText::Get(TEXT("Figma.CurrentPlan")));FigmaUI::Text(PlanTitle,16,FigmaUI::PrimaryText,true);PlanTexts->AddChild(PlanTitle);CurrentPlanSummary=Label(WidgetTree,FText::GetEmpty(),13);
    CurrentPlanSummary->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);CurrentPlanSummary->SetClipping(EWidgetClipping::ClipToBounds);PlanTexts->AddChildToVerticalBox(CurrentPlanSummary)->SetPadding(FMargin(0,5,0,0));
    PlanRow->AddChildToHorizontalBox(ActionButton(TEXT("open_plan"),TEXT("Workflow.OpenPlan"),1))->SetVerticalAlignment(VAlign_Center);
    auto* Lower=WidgetTree->ConstructWidget<UHorizontalBox>();Fill(Home->AddChildToVerticalBox(Lower));
    auto* MapCard=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("FigmaOverviewMap"));FigmaUI::Panel(MapCard,FigmaUI::Surface,FigmaUI::Outline,10,FMargin(16,12));auto* MS=Lower->AddChildToHorizontalBox(MapCard);Fill(MS,2.43f);MS->SetPadding(FMargin(0,0,16,0));
    auto* MapContent=WidgetTree->ConstructWidget<UVerticalBox>();MapCard->SetContent(MapContent);auto* Toolbar=WidgetTree->ConstructWidget<UHorizontalBox>();MapContent->AddChild(Toolbar);
    Toolbar->AddChildToHorizontalBox(Key(WidgetTree,TEXT("Situation.Title"),16,true))->SetPadding(FMargin(0,0,24,0));ZoomCaption=Label(WidgetTree,FText::GetEmpty(),14);Fill(Toolbar->AddChildToHorizontalBox(ZoomCaption));
    for(const TCHAR* A:{TEXT("zoom_in"),TEXT("zoom_out")}){auto* B=ActionButton(A,TEXT("Situation.Title"));B->SetContent(FigmaUI::Icon(WidgetTree,FString(A)==TEXT("zoom_in")?TEXT("plus"):TEXT("minus"),{18,18}));auto Style=B->GetStyle();Style.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));B->SetStyle(Style);Cast<UButtonSlot>(B->GetContent()->Slot)->SetHorizontalAlignment(HAlign_Center);Cast<UButtonSlot>(B->GetContent()->Slot)->SetVerticalAlignment(VAlign_Center);Toolbar->AddChildToHorizontalBox(Fixed(WidgetTree,B,30,30))->SetPadding(FMargin(8,0,0,0));}
    SituationMap=CreateWidget<UCommandTacticalMap>(GetOwningPlayer());SituationMap->SetFigmaPresentation(true);Fill(MapContent->AddChildToVerticalBox(SituationMap));
    FollowButton=ActionButton(TEXT("follow_situation"),TEXT("Situation.Follow"));auto* FS=MapContent->AddChildToVerticalBox(FollowButton);FS->SetHorizontalAlignment(HAlign_Right);
    auto* Activity=WidgetTree->ConstructWidget<UVerticalBox>();Fill(Lower->AddChildToHorizontalBox(Activity));
    auto* DenseSummary=WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("FigmaOverviewDenseSummary"));Activity->AddChild(DenseSummary);
    auto* EventCard=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("FigmaOverviewEvents"));FigmaUI::Panel(EventCard,FigmaUI::Surface,FigmaUI::Outline,10,FMargin(16,12));Fill(Activity->AddChildToVerticalBox(EventCard));
    auto* EV=WidgetTree->ConstructWidget<UVerticalBox>();EventCard->SetContent(EV);auto* EH=WidgetTree->ConstructWidget<UHorizontalBox>();EV->AddChild(EH);Fill(EH->AddChildToHorizontalBox(Key(WidgetTree,TEXT("Workflow.RecentEvents"),16,true)));
    EH->AddChild(ActionButton(TEXT("logs"),TEXT("Figma.ViewAll")));auto* ES=WidgetTree->ConstructWidget<UScrollBox>();Fill(EV->AddChildToVerticalBox(ES));EventRows=WidgetTree->ConstructWidget<UVerticalBox>();ES->AddChild(EventRows);
    RecentEvents=Label(WidgetTree,FText::GetEmpty()); // Retain legacy summary for accessibility/diagnostics.
    CenterPanel=CreateWidget<UCommandCenterPanel>(GetOwningPlayer());WorkspacePages->AddChild(CenterPanel);
    FigmaFleet=CreateWidget<UFigmaFleetWidget>(GetOwningPlayer());WorkspacePages->AddChild(FigmaFleet);
    ConfigurationHeader=WidgetTree->ConstructWidget<UVerticalBox>();auto* CSlot=Root->AddChildToCanvas(ConfigurationHeader);CSlot->SetAnchors(FAnchors(0,0,1,0));CSlot->SetOffsets(FMargin(30,102,30,76));
    ConfigurationCaption=Label(WidgetTree,FText::GetEmpty(),16);ConfigurationHeader->AddChild(ConfigurationCaption);auto* CA=WidgetTree->ConstructWidget<UHorizontalBox>();ConfigurationHeader->AddChild(CA);
    CA->AddChild(ActionButton(TEXT("configuration_return"),TEXT("Configuration.Return"),1));ConfigurationFinish=ActionButton(TEXT("configuration_finish"),TEXT("Configuration.Finish"),1);CA->AddChild(ConfigurationFinish);
    ActiveControls=CreateWidget<UActiveExecutionControlsWidget>(GetOwningPlayer());auto* AS=Root->AddChildToCanvas(ActiveControls);AS->SetAnchors(FAnchors(0,0,1,0));AS->SetOffsets(FMargin(30,102,30,154));
    auto* Workflow=WidgetTree->ConstructWidget<UHorizontalBox>();Place(Root,Workflow,{32,779},{1376,20});
    const TCHAR* WorkflowKeys[]={TEXT("Figma.Plans.FlowStart"),TEXT("Figma.Plans.FlowDevices"),TEXT("Figma.Plans.FlowRoutes"),TEXT("Figma.Plans.FlowCheck"),TEXT("Figma.Plans.FlowDeploy")};
    for(int I=0;I<5;++I){
        if(I){auto* ArrowSlot=Workflow->AddChildToHorizontalBox(FigmaUI::Icon(WidgetTree,TEXT("workflow-arrow"),{13.4,4.3}));ArrowSlot->SetVerticalAlignment(VAlign_Center);ArrowSlot->SetPadding(FMargin(7,0));}
        Workflow->AddChildToHorizontalBox(Key(WidgetTree,WorkflowKeys[I],11))->SetVerticalAlignment(VAlign_Center);
    }

    FigmaDialog=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("FigmaModalBackdrop"));FigmaDialog->SetBrushColor(FLinearColor(0,0,0,.78f));FigmaDialog->SetPadding(FMargin(0));
    auto* DS=Root->AddChildToCanvas(FigmaDialog);DS->SetAnchors(FAnchors(0,0,1,1));DS->SetOffsets(FMargin(0));DS->SetZOrder(100);
    auto* Overlay=WidgetTree->ConstructWidget<UOverlay>();FigmaDialog->SetContent(Overlay);
    auto* Panel=WidgetTree->ConstructWidget<UBorder>();FigmaUI::Panel(Panel,FigmaUI::Surface,FigmaUI::Outline,12,FMargin(32));
    auto* ModalScale=WidgetTree->ConstructWidget<UScaleBox>();ModalScale->SetStretch(EStretch::ScaleToFit);ModalScale->SetStretchDirection(EStretchDirection::DownOnly);ModalScale->SetContent(Fixed(WidgetTree,Panel,900,620));auto* PS=Overlay->AddChildToOverlay(ModalScale);PS->SetHorizontalAlignment(HAlign_Fill);PS->SetVerticalAlignment(VAlign_Fill);
    auto* Body=WidgetTree->ConstructWidget<UVerticalBox>();Panel->SetContent(Body);auto* DH=WidgetTree->ConstructWidget<UHorizontalBox>();Body->AddChildToVerticalBox(DH)->SetPadding(FMargin(0,0,0,24));
    DialogTitle=Label(WidgetTree,FText::GetEmpty(),28,true);Fill(DH->AddChildToHorizontalBox(DialogTitle));auto* Close=ActionButton(TEXT("close_dialog"),TEXT("Figma.Close"));Close->SetContent(FigmaUI::Icon(WidgetTree,TEXT("close"),{22,22}));DH->AddChild(Close);
    DialogColumns=WidgetTree->ConstructWidget<UVerticalBox>();Body->AddChild(DialogColumns);DialogScroll=WidgetTree->ConstructWidget<UScrollBox>();Fill(Body->AddChildToVerticalBox(DialogScroll));DialogRows=WidgetTree->ConstructWidget<UVerticalBox>();DialogScroll->AddChild(DialogRows);
    DialogFeedback=Label(WidgetTree,FText::GetEmpty(),13);Body->AddChild(DialogFeedback);
    auto* Foot=WidgetTree->ConstructWidget<UHorizontalBox>();Body->AddChildToVerticalBox(Foot)->SetPadding(FMargin(0,18,0,0));DialogCount=Label(WidgetTree,FText::GetEmpty(),15);Fill(Foot->AddChildToHorizontalBox(DialogCount));Foot->AddChild(ActionButton(TEXT("refresh_dialog"),TEXT("Figma.Refresh")));
    FigmaDialog->SetVisibility(ESlateVisibility::Collapsed);WorkspacePages->SetActiveWidgetIndex(0);
}

void UCommandShellWidget::RefreshFigmaShell() {
    auto* GI=GetGameInstance();if(!GI || !HeaderClock)return;
    auto* Sync=GI->GetSubsystem<UOperationalContextSubsystem>();auto* R=GI->GetSubsystem<UDroneRegistrySubsystem>();
    HeaderClock->SetText(FText::AsCultureInvariant(FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M"))));
    const TCHAR* Roles[]={TEXT("Command"),TEXT("Map"),TEXT("Video")};
    for(int I=0;I<4;++I){const bool Online=I?Sync->IsRoleOnline(Roles[I-1]):Sync->GetSystemState()==TEXT("READY");
        FText T=I?FText::Format(ProductText::Get(TEXT("Figma.RoleHealth")),ProductText::Get(TEXT("Stage1.")+FString(Roles[I-1])),ProductText::Get(Online?TEXT("Stage1.Online"):TEXT("Stage1.Offline"))):ProductText::Get(TEXT("Stage1.System.")+Sync->GetSystemState());
        HealthLabels[I]->SetText(T);HealthDots[I]->SetBrush(Online?HealthOnlineIcon->GetBrush():HealthOfflineIcon->GetBrush());FigmaUI::Text(HealthLabels[I],12,FigmaUI::PrimaryText);
    }
    FString Language=PlanUI::Field(Sync->GetUIPreferences(),TEXT("language"));for(int I=0;I<LanguageButtons.Num();++I)FigmaUI::Button(LanguageButtons[I],I==0?Language!=TEXT("en"):Language==TEXT("en"),true);
    const auto Drones=R->GetFriendlyDroneDescriptors();int32 Online=0;for(const auto& D:Drones){FDroneTelemetrySnapshot T;if(R->GetTelemetry(D.DroneId,T)&&T.Availability==EDroneAvailability::Online)++Online;}
    const auto* Alerts=GI->GetSubsystem<UCommandAlertStore>();const auto& Events=GI->GetSubsystem<UOperationalEventStore>()->GetEvents();
    StatisticValues[0]->SetText(Number(Drones.Num()));StatisticValues[1]->SetText(Number(Online));StatisticValues[2]->SetText(Number(Alerts->GetUnhandledCount()));StatisticValues[3]->SetText(Number(Events.Num()));
    RefreshRunningTasks();
    if(FigmaFleet)FigmaFleet->Refresh();
    ZoomCaption->SetText(FText::AsCultureInvariant(FString::Printf(TEXT("Z%d"),SituationMap->GetZoom())));FigmaUI::Button(FollowButton,SituationMap->IsFollowing(),true);
    const FString Signature=FString::Printf(TEXT("%llu/%s"),GI->GetSubsystem<UOperationalEventStore>()->GetLatestSequence(),*Language);
    if(Signature!=EventSignature){EventSignature=Signature;EventRows->ClearChildren();
        if(Events.IsEmpty())EventRows->AddChild(Key(WidgetTree,TEXT("Log.Empty")));
        for(int I=Events.Num()-1;I>=FMath::Max(0,Events.Num()-5);--I){const auto& E=Events[I];auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();EventRows->AddChildToVerticalBox(Row)->SetPadding(FMargin(0,20,0,16));
            auto* Dot=FigmaUI::Icon(WidgetTree,E.Level==EOperationalLevel::INFO?TEXT("status-online-large"):TEXT("status-offline"),{12,12});auto* DotSlot=Row->AddChildToHorizontalBox(Dot);DotSlot->SetVerticalAlignment(VAlign_Center);DotSlot->SetPadding(FMargin(0,0,14,0));
            auto* Message=Label(WidgetTree,E.DisplayText(),13);Message->SetAutoWrapText(true);Fill(Row->AddChildToHorizontalBox(Message));Row->AddChildToHorizontalBox(Label(WidgetTree,FText::AsDateTime(E.Timestamp,TEXT("%H:%M")),11))->SetPadding(FMargin(12,0,0,0));
        }
    }
    if(AlertSelectionAt>0){
        const bool Ack=PendingAlertId.IsEmpty()?PlanUI::Field(Sync->GetContext(),TEXT("active_uav_id"))==PendingAlertUAV:PlanUI::Field(Sync->GetContext(),TEXT("active_alert_id"))==PendingAlertId;
        if(Ack){DialogFeedback->SetText(ProductText::Get(TEXT("Common.Confirmed")));AlertSelectionAt=0;}
        else if(!Sync->IsReady() || FPlatformTime::Seconds()-AlertSelectionAt>10){DialogFeedback->SetText(ProductText::Get(TEXT("Errors.SYNC_FAILED")));AlertSelectionAt=0;}
    }
    if(!DialogKind.IsEmpty())RefreshFigmaDialog();
}

void UCommandShellWidget::FigmaAction(FName Action,int32 Index) {
    if(Action==TEXT("close_dialog")){DialogKind.Empty();FigmaDialog->SetVisibility(ESlateVisibility::Collapsed);return;}
    if(Action==TEXT("devices") || Action==TEXT("alerts") || Action==TEXT("logs")){DialogKind=Action.ToString();DialogSignature.Empty();DialogFeedback->SetText(FText::GetEmpty());FigmaDialog->SetVisibility(ESlateVisibility::Visible);RefreshFigmaDialog();return;}
    if(Action==TEXT("refresh_dialog")){DialogSignature.Empty();RefreshFigmaDialog();return;}
    if(Action==TEXT("zoom_in") || Action==TEXT("zoom_out")){SituationMap->SetZoom(SituationMap->GetZoom()+(Action==TEXT("zoom_in")?1:-1));return;}
    if(Action==TEXT("device_detail")){
        auto* S=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
        if(PlanConfiguration::Active(S->GetPlans())){DialogFeedback->SetText(ProductText::Get(TEXT("Figma.ConfigurationBusy")));return;}
        if(!S->SetActiveUAV(Index)){DialogFeedback->SetText(ProductText::Get(TEXT("Errors.SYNC_OFFLINE")));return;}
        FigmaAction(TEXT("close_dialog"),0);Navigate(TEXT("navigate"),2);return;
    }
    if(Action==TEXT("alert_select")){for(const auto& A:GetGameInstance()->GetSubsystem<UCommandAlertStore>()->GetAlerts())if(A.Id==Index){
        auto* S=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();const bool Accepted=A.SharedId.IsEmpty()?S->SetActiveUAV(A.DroneId):S->SelectAlert(A.SharedId,A.DroneId);
        if(!Accepted)DialogFeedback->SetText(ProductText::Get(TEXT("Errors.SYNC_OFFLINE")));else{PendingAlertId=A.SharedId;PendingAlertUAV=FString::Printf(TEXT("UAV-%02d"),A.DroneId);AlertSelectionAt=FPlatformTime::Seconds();DialogFeedback->SetText(ProductText::Get(TEXT("Common.Waiting")));}break;}return;}
    if(Action==TEXT("alert_handled")){GetGameInstance()->GetSubsystem<UCommandAlertStore>()->MarkHandled(Index);DialogSignature.Empty();RefreshFigmaDialog();return;}
    if(Action==TEXT("fleet")){Navigate(TEXT("navigate"),2);return;}
    if(Action==TEXT("open_plan")){Navigate(TEXT("navigate"),1);return;}
    Navigate(Action,Index);
}

void UCommandShellWidget::RefreshFigmaDialog() {
    if(DialogKind.IsEmpty())return;auto* GI=GetGameInstance();auto* R=GI->GetSubsystem<UDroneRegistrySubsystem>();auto* Sync=GI->GetSubsystem<UOperationalContextSubsystem>();
    const auto Drones=R->GetFriendlyDroneDescriptors();const auto Alerts=GI->GetSubsystem<UCommandAlertStore>()->GetAlerts();const auto& Events=GI->GetSubsystem<UOperationalEventStore>()->GetEvents();
    FString Signature=DialogKind+PlanUI::Field(Sync->GetUIPreferences(),TEXT("language"))+FString::Printf(TEXT("/%llu/%d"),GI->GetSubsystem<UOperationalEventStore>()->GetLatestSequence(),Alerts.Num());
    if(DialogKind==TEXT("devices"))for(const auto& D:Drones){FDroneTelemetrySnapshot T;R->GetTelemetry(D.DroneId,T);FDroneTaskStateSnapshot Task;R->GetTaskState(D.DroneId,Task);Signature+=FString::Printf(TEXT("/%d/%s/%d/%d/%d"),D.DroneId,*D.Name,int(T.Availability),T.BatteryPercent,int(Task.State));}
    if(DialogKind==TEXT("alerts"))for(const auto& A:Alerts)Signature+=FString::Printf(TEXT("/%d/%d"),A.Id,A.bHandled);
    if(Signature==DialogSignature)return;DialogSignature=Signature;DialogRows->ClearChildren();DialogColumns->ClearChildren();
    const bool Devices=DialogKind==TEXT("devices"),IsAlerts=DialogKind==TEXT("alerts");DialogTitle->SetText(ProductText::Get(Devices?TEXT("Figma.OnlineDevices"):IsAlerts?TEXT("Figma.AlertCenter"):TEXT("Nav.Logs")));
    auto Row=[&](UVerticalBox* Parent,bool Heading){auto* B=WidgetTree->ConstructWidget<UBorder>();FigmaUI::Panel(B,Heading?FLinearColor(FColor(36,39,47)):FigmaUI::Surface,Heading?FLinearColor::Transparent:FigmaUI::Outline,Heading?5:0,FMargin(14,Heading?10:20));auto* H=WidgetTree->ConstructWidget<UHorizontalBox>();B->SetContent(H);Parent->AddChild(B);return H;};
    auto Cell=[&](UHorizontalBox* H,const FText& T,float W=1,bool Bold=false){auto* L=Label(WidgetTree,T,15,Bold);L->SetAutoWrapText(true);Fill(H->AddChildToHorizontalBox(L),W);return L;};
    auto Action=[&](UHorizontalBox* H,const TCHAR* A,const TCHAR* K,int32 Id){auto* B=WidgetTree->ConstructWidget<UCommandActionButton>();B->Configure(A,Id);B->OnAction.AddDynamic(this,&UCommandShellWidget::FigmaAction);FigmaUI::Button(B,false,true);B->SetContent(Key(WidgetTree,K,13));Fill(H->AddChildToHorizontalBox(B));};
    auto* Headers=Row(DialogColumns,true);
    const TArray<FString> Columns=Devices?TArray<FString>{TEXT("Figma.DeviceId"),TEXT("Figma.DeviceName"),TEXT("Figma.Connection"),TEXT("Figma.Battery"),TEXT("Figma.CurrentTask"),TEXT("Figma.Actions")}:IsAlerts?TArray<FString>{TEXT("Figma.TimeUTC"),TEXT("Figma.AlertType"),TEXT("Figma.Content"),TEXT("Figma.Actions")}:TArray<FString>{TEXT("Figma.TimeUTC"),TEXT("Figma.EventType"),TEXT("Figma.Content")};
    for(int I=0;I<Columns.Num();++I)Cell(Headers,ProductText::Get(Columns[I]),!Devices&&I==2?2.4f:1,true);
    int Count=0;
    if(Devices){auto Sorted=Drones;Sorted.Sort([](const FDroneDescriptor& A,const FDroneDescriptor& B){return A.DroneId<B.DroneId;});for(const auto& D:Sorted){
        auto* H=Row(DialogRows,false);Cell(H,PlanUI::User(FString::Printf(TEXT("UAV-%02d"),D.DroneId)));Cell(H,PlanUI::User(D.Name));FDroneTelemetrySnapshot T;const bool Has=R->GetTelemetry(D.DroneId,T);
        Cell(H,ProductText::Get(Has&&T.Availability==EDroneAvailability::Online?TEXT("Fleet.Online"):Has&&T.Availability==EDroneAvailability::Lost?TEXT("Fleet.Lost"):TEXT("Fleet.Offline")));
        Cell(H,PlanUI::User(Has&&T.BatteryPercent>=0?FString::Printf(TEXT("%d%%"),T.BatteryPercent):TEXT("N/A")));FDroneTaskStateSnapshot TS;
        Cell(H,R->GetTaskState(D.DroneId,TS)?ProductText::Source(StaticEnum<EDroneTaskState>()->GetDisplayNameTextByValue(int64(TS.State)).ToString()):ProductText::Get(TEXT("Fleet.Unknown")));Action(H,TEXT("device_detail"),TEXT("Figma.Details"),D.DroneId);++Count;
    }}else if(IsAlerts){for(int I=Alerts.Num()-1;I>=0;--I){const auto& A=Alerts[I];auto* H=Row(DialogRows,false);Cell(H,PlanUI::User(A.ReceivedAt.ToString(TEXT("%H:%M:%S"))));Cell(H,ProductText::Source(A.Type));Cell(H,ProductText::Source(A.Message),2.4f);auto* ActionCell=WidgetTree->ConstructWidget<UVerticalBox>();Fill(H->AddChildToHorizontalBox(ActionCell));auto* SelectRow=WidgetTree->ConstructWidget<UHorizontalBox>();ActionCell->AddChild(SelectRow);Action(SelectRow,TEXT("alert_select"),TEXT("Command.Select"),A.Id);if(!A.bHandled){auto* HandleRow=WidgetTree->ConstructWidget<UHorizontalBox>();ActionCell->AddChildToVerticalBox(HandleRow)->SetPadding(FMargin(0,4,0,0));Action(HandleRow,TEXT("alert_handled"),TEXT("Command.Handled"),A.Id);}++Count;}}
    else{for(int I=Events.Num()-1;I>=0;--I){const auto& E=Events[I];auto* H=Row(DialogRows,false);Cell(H,PlanUI::User(E.Timestamp.ToString(TEXT("%H:%M:%S"))));Cell(H,ProductText::Source(StaticEnum<EOperationalCategory>()->GetNameStringByValue(int64(E.Category))));Cell(H,E.DisplayText(),2.4f);++Count;}}
    if(Count==0){auto* Empty=WidgetTree->ConstructWidget<UVerticalBox>();auto* S=DialogRows->AddChildToVerticalBox(Empty);S->SetPadding(FMargin(0,140,0,0));S->SetHorizontalAlignment(HAlign_Center);
        Empty->AddChildToVerticalBox(FigmaUI::Icon(WidgetTree,IsAlerts?TEXT("no-alerts"):Devices?TEXT("drone"):TEXT("log"),{60,60}))->SetHorizontalAlignment(HAlign_Center);
        Empty->AddChildToVerticalBox(Key(WidgetTree,IsAlerts?TEXT("Command.NoAlerts"):Devices?TEXT("Figma.NoDevices"):TEXT("Log.Empty"),16))->SetPadding(FMargin(0,12));
    }
    DialogCount->SetText(FText::Format(ProductText::Get(Devices?TEXT("Figma.DeviceCount"):IsAlerts?TEXT("Figma.AlertCount"):TEXT("Figma.LogCount")),Number(Count)));
}
