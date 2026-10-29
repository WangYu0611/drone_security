#include "Shared/SecurityPlanWorkspaceWidget.h"
#include "Shared/PlanWidgetSupport.h"
#include "Shared/ExecutionPresentation.h"
#include "Shared/PlanConfiguration.h"
#include "Shared/DeploymentConfirmation.h"
#include "Command/FigmaCommandStyle.h"
#include "Components/ProgressBar.h"
#include "Components/CheckBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif
using namespace PlanUI;
namespace {
// Length prefixes keep user-entered names from colliding with signature separators.
void AddVisibleKey(FString& Key,const FString& Value){Key+=FString::FromInt(Value.Len())+TEXT(":")+Value;}
TArray<FText> ValidationIssueLines(const TSharedPtr<FJsonObject>& State,const TSharedPtr<FJsonObject>& Plan){
    TArray<FText> Lines;const auto Validation=Object(Plan,TEXT("validation"));const TArray<TSharedPtr<FJsonValue>>* Issues=nullptr;
    if(Validation && Validation->TryGetArrayField(TEXT("issues"),Issues))for(const auto& V:*Issues){const auto I=V->AsObject();const auto Params=Object(I,TEXT("params"));
        const FString MissionName=Field(Find(State,TEXT("missions"),Field(I,TEXT("mission_id"))),TEXT("name"));
        Lines.Add(MissionName.IsEmpty()?ProductText::Get(TEXT("Errors.")+Field(I,TEXT("code"))):FText::Format(FText::AsCultureInvariant(TEXT("{0}: {1}")),User(MissionName),ProductText::Get(TEXT("Errors.")+Field(I,TEXT("code")))));
        if(Params && Params->HasField(TEXT("minimum_separation_m")) && Params->HasField(TEXT("required_separation_m")))
            Lines.Add(FText::Format(T(TEXT("Group.Conflict")),User(Field(Params,TEXT("uav_a"))),User(Field(Params,TEXT("uav_b"))),FText::AsNumber(Params->GetNumberField(TEXT("minimum_separation_m"))),FText::AsNumber(Params->GetNumberField(TEXT("required_separation_m")))));
    }
    return Lines;
}
TSharedPtr<FJsonObject> MissionExecution(const TSharedPtr<FJsonObject>& State,const FString& Plan,const FString& Mission){
    // A later execution in another task must never replace this task's controls.
    TSharedPtr<FJsonObject> Result;const auto All=Object(State,TEXT("executions"));
    if(Plan.IsEmpty() || Mission.IsEmpty() || !All)return Result;
    for(const auto& Entry:All->Values){const auto E=Entry.Value->AsObject();
        if(Field(E,TEXT("plan_id"))!=Plan || Field(E,TEXT("mission_id"))!=Mission)continue;
        double Created=0,Previous=0;E->TryGetNumberField(TEXT("created_at"),Created);if(Result)Result->TryGetNumberField(TEXT("created_at"),Previous);
        if(!Result || (ExecutionUI::Active(E) && !ExecutionUI::Active(Result)) || (ExecutionUI::Active(E)==ExecutionUI::Active(Result) && Created>Previous))Result=E;
    }return Result;
}
bool FocusMapWindow(){
#if PLATFORM_WINDOWS
    struct FMatch { HWND Window=nullptr;int Count=0; } Match;
    ::EnumWindows([](HWND Window,LPARAM Data)->BOOL {wchar_t Title[128];::GetWindowTextW(Window,Title,128);
        if(::IsWindowVisible(Window) && FCStringWide::Strcmp(Title,L"Drone Security | Map")==0){auto* M=reinterpret_cast<FMatch*>(Data);M->Window=Window;++M->Count;}return 1;},reinterpret_cast<LPARAM>(&Match));
    if(Match.Count==1){if(::IsIconic(Match.Window))::ShowWindow(Match.Window,SW_RESTORE);return ::SetForegroundWindow(Match.Window)!=0;}
#endif
    return false;
}
}

void UPlanMissionListWidget::NativeOnInitialized(){
    Super::NativeOnInitialized();
    auto* Surface=WidgetTree->ConstructWidget<UBorder>();FigmaUI::Panel(Surface,FigmaUI::Inset,FigmaUI::Outline,9,FMargin(20));WidgetTree->RootWidget=Surface;
    Content=WidgetTree->ConstructWidget<UVerticalBox>();Surface->SetContent(Content);
}
bool UPlanMissionListWidget::ResolveAction(int32 Id,FString& OutPlan,FString& OutMission) const {
    const auto* Target=ActionTargets.Find(Id);if(!Target)return false;OutPlan=PlanId;OutMission=*Target;return true;
}
void UPlanMissionListWidget::Refresh(const TSharedPtr<FJsonObject>& State,const TSharedPtr<FJsonObject>& Plan,const FString& SelectedMission){
    const auto NewPlan=Field(Plan,TEXT("id"));FString NewSignature;
    AddVisibleKey(NewSignature,NewPlan);AddVisibleKey(NewSignature,Field(Plan,TEXT("status")));AddVisibleKey(NewSignature,SelectedMission);
    const TArray<TSharedPtr<FJsonValue>>* Tasks=nullptr;
    if(Plan && Plan->TryGetArrayField(TEXT("mission_ids"),Tasks))for(const auto& Id:*Tasks){
        const auto M=Find(State,TEXT("missions"),Id->AsString()),R=Find(State,TEXT("paths"),Field(M,TEXT("route_id"))),E=MissionExecution(State,NewPlan,Id->AsString());
        const TArray<TSharedPtr<FJsonValue>>* Points=nullptr;
        for(const auto& Value:{Id->AsString(),Field(M,TEXT("name")),Field(M,TEXT("status")),AssignedUAVs(M),Field(E,TEXT("execution_id")),Field(E,TEXT("state"))})AddVisibleKey(NewSignature,Value);
        AddVisibleKey(NewSignature,FString::FromInt(R && R->TryGetArrayField(TEXT("waypoints"),Points)?Points->Num():0));
    }
    // Lease heartbeats and position ticks must not replace a pressed/focused button.
    // A visible row or selection change still retires every old action generation.
    if(VisibleSignature==NewSignature)return;
    VisibleSignature=NewSignature;PlanId=NewPlan;SelectedId=SelectedMission;Content->ClearChildren();MissionIds.Empty();MissionActionIds.Empty();ActionTargets.Empty();
    auto* Heading=WidgetTree->ConstructWidget<UHorizontalBox>();Content->AddChildToVerticalBox(Heading)->SetPadding(FMargin(0,0,0,16));
    auto* MissionHeading=Label(WidgetTree,Heading,T(TEXT("Plan.Tasks.Title")),24);FigmaUI::Text(MissionHeading,24,FigmaUI::PrimaryText,true);MissionHeading->SetAutoWrapText(false);
    auto* Hint=Label(WidgetTree,Heading,T(TEXT("Plan.Tasks.Hint")),12);FigmaUI::Text(Hint,12,FigmaUI::Muted);
    auto* HintSlot=CastChecked<UHorizontalBoxSlot>(Hint->Slot);HintSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));HintSlot->SetVerticalAlignment(VAlign_Center);HintSlot->SetPadding(FMargin(18,0,0,0));
    auto* HeadingLine=WidgetTree->ConstructWidget<USizeBox>();HeadingLine->SetWidthOverride(820);HeadingLine->SetHeightOverride(1);HeadingLine->SetContent(FigmaUI::Icon(WidgetTree,TEXT("plan-detail-divider"),FVector2D(820,1)));auto* HeadingDivider=Content->AddChildToVerticalBox(HeadingLine);HeadingDivider->SetHorizontalAlignment(HAlign_Center);HeadingDivider->SetPadding(FMargin(0,0,0,12));
    const TArray<TSharedPtr<FJsonValue>>* Ids;if(!Plan || !Plan->TryGetArrayField(TEXT("mission_ids"),Ids) || Ids->IsEmpty()){Label(WidgetTree,Content,T(TEXT("Plan.NoMissions")));return;}
    const bool Deployed=Field(Plan,TEXT("status"))==TEXT("DEPLOYED");
    for(const auto& V:*Ids){const auto M=Find(State,TEXT("missions"),V->AsString());const auto R=Find(State,TEXT("paths"),Field(M,TEXT("route_id")));const TArray<TSharedPtr<FJsonValue>>* Points;
        const int Count=R && R->TryGetArrayField(TEXT("waypoints"),Points)?Points->Num():0;
        MissionIds.Add(V->AsString());const int Index=++NextActionId;MissionActionIds.Add(Index);ActionTargets.Add(Index,V->AsString());const auto UAV=AssignedUAVs(M);
        const auto E=MissionExecution(State,PlanId,V->AsString());const bool Selected=SelectedId==V->AsString();
        auto* Card=WidgetTree->ConstructWidget<UBorder>();FigmaUI::Panel(Card,FigmaUI::Inset,Selected?FigmaUI::PrimaryText:FigmaUI::Outline,8,FMargin(14));Content->AddChildToVerticalBox(Card)->SetPadding(FMargin(0,10,0,0));
        auto* Body=WidgetTree->ConstructWidget<UHorizontalBox>();Card->SetContent(Body);
        auto* IconBox=WidgetTree->ConstructWidget<UBorder>();FigmaUI::Panel(IconBox,FigmaUI::Inset,FigmaUI::Outline,7,FMargin(8));
        IconBox->SetContent(FigmaUI::Icon(WidgetTree,TEXT("plan-detail-drone"),FVector2D(47,47)));
        auto* IconSlot=Body->AddChildToHorizontalBox(IconBox);IconSlot->SetVerticalAlignment(VAlign_Center);IconSlot->SetPadding(FMargin(0,0,16,0));
        auto* Info=WidgetTree->ConstructWidget<UVerticalBox>();auto* InfoSlot=Body->AddChildToHorizontalBox(Info);InfoSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));InfoSlot->SetVerticalAlignment(VAlign_Center);InfoSlot->SetPadding(FMargin(0,0,14,0));
        auto* Name=Label(WidgetTree,Info,User(Field(M,TEXT("name"))),17);FigmaUI::Text(Name,17,FigmaUI::PrimaryText,true);Name->SetAutoWrapText(false);Name->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);Name->SetToolTipText(Name->GetText());
        FigmaUI::Text(Label(WidgetTree,Info,FText::Format(T(TEXT("Plan.Tasks.Assignment")),UAV.IsEmpty()?T(TEXT("Common.None")):User(UAV),FText::AsNumber(Count))),12,FigmaUI::Muted);
        FigmaUI::Text(Label(WidgetTree,Info,FText::Format(T(TEXT("Plan.Tasks.State")),ProductText::Get(TEXT("Plan.")+Field(M,TEXT("status"))),E?ProductText::Get(TEXT("Execution.")+Field(E,TEXT("state"))):T(TEXT("Execution.None")))),12,ExecutionUI::Active(E)?FigmaUI::Accent:FigmaUI::Muted);
        if(Count<2)FigmaUI::Text(Label(WidgetTree,Info,T(TEXT("Errors.ROUTE_TOO_SHORT"))),12,FigmaUI::Warning);
        auto* VerticalDivider=WidgetTree->ConstructWidget<USizeBox>();VerticalDivider->SetWidthOverride(1);VerticalDivider->SetHeightOverride(72);auto* DividerOverlay=WidgetTree->ConstructWidget<UOverlay>();VerticalDivider->SetContent(DividerOverlay);auto* DividerSlot=DividerOverlay->AddChildToOverlay(FigmaUI::Icon(WidgetTree,TEXT("plan-detail-mission-divider"),FVector2D(1,72)));DividerSlot->SetHorizontalAlignment(HAlign_Center);DividerSlot->SetVerticalAlignment(VAlign_Center);auto* VerticalSlot=Body->AddChildToHorizontalBox(VerticalDivider);VerticalSlot->SetVerticalAlignment(VAlign_Center);VerticalSlot->SetPadding(FMargin(0,0,14,0));
        auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();auto* ActionSize=WidgetTree->ConstructWidget<USizeBox>();ActionSize->SetWidthOverride(UAV.IsEmpty()?322:430);ActionSize->SetContent(Row);
        Body->AddChildToHorizontalBox(ActionSize)->SetVerticalAlignment(VAlign_Center);
        auto Add=[&](const TCHAR* A,const TCHAR* K){auto* B=Button(WidgetTree,Row,A,K,Index);B->OnAction.AddDynamic(this,&UPlanMissionListWidget::Action);const bool Primary=A==FString(TEXT("execute_mission"));FigmaUI::Button(B,false,true);
            auto Style=B->GetStyle();const FLinearColor Fill=Primary?FigmaUI::Accent:FLinearColor(FColor(30,36,51));Style.SetNormal(FSlateRoundedBoxBrush(Fill,6));Style.SetHovered(FSlateRoundedBoxBrush(Fill,6,FigmaUI::PrimaryText,.7f));Style.SetPressed(FSlateRoundedBoxBrush(FigmaUI::SelectedSurface,6,FigmaUI::Accent,1));Style.SetNormalPadding(FMargin(8,10)).SetPressedPadding(FMargin(8,10));B->SetStyle(Style);
            auto* Caption=CastChecked<UTextBlock>(B->GetContent());FigmaUI::Text(Caption,12,FigmaUI::PrimaryText);Caption->SetJustification(ETextJustify::Center);
            auto* Slot=CastChecked<UHorizontalBoxSlot>(B->Slot);Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Slot->SetPadding(FMargin(4,0));};
        Add(TEXT("select_mission"),Selected?TEXT("Plan.Tasks.Selected"):TEXT("Plan.Tasks.Select"));
        Add(TEXT("edit_mission"),TEXT("Plan.Tasks.Edit"));
        Add(TEXT("execute_mission"),ExecutionUI::Active(E)?TEXT("Plan.Tasks.ViewExecution"):Deployed?TEXT("Plan.Tasks.Execute"):TEXT("Plan.Tasks.CheckDeploy"));
        if(!UAV.IsEmpty())Add(TEXT("video_mission"),TEXT("Video.Open"));
    }
}
void UPlanReviewWidget::NativeOnInitialized(){
    Super::NativeOnInitialized();auto* V=WidgetTree->ConstructWidget<UVerticalBox>();WidgetTree->RootWidget=V;
    Summary=Label(WidgetTree,V,FText::GetEmpty(),20);FigmaUI::Text(Summary,20,FigmaUI::PrimaryText,true);
    Description=Label(WidgetTree,V,FText::GetEmpty(),12);FigmaUI::Text(Description,12,FigmaUI::Muted);
    auto* Metrics=WidgetTree->ConstructWidget<UHorizontalBox>();V->AddChildToVerticalBox(Metrics)->SetPadding(FMargin(0,8));
    auto Metric=[&](const TCHAR* Key){auto* Box=WidgetTree->ConstructWidget<UBorder>();FigmaUI::Panel(Box,FigmaUI::Inset,FigmaUI::Outline,4,FMargin(8));Box->SetClipping(EWidgetClipping::ClipToBounds);
        auto* Slot=Metrics->AddChildToHorizontalBox(Box);Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Slot->SetPadding(FMargin(0,0,6,0));
        auto* Column=WidgetTree->ConstructWidget<UVerticalBox>();Box->SetContent(Column);FigmaUI::Text(Label(WidgetTree,Column,T(Key),11),11,FigmaUI::Muted);
        auto* Value=Label(WidgetTree,Column,FText::GetEmpty(),14);FigmaUI::Text(Value,14);Value->SetAutoWrapText(false);Value->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);Value->SetClipping(EWidgetClipping::ClipToBounds);return Value;};
    MissionValue=Metric(TEXT("Plan.Mission"));AircraftValue=Metric(TEXT("Figma.Fleet.Title"));WaypointsValue=Metric(TEXT("Figma.PlanDetail.Waypoints"));DistanceValue=Metric(TEXT("Figma.PlanDetail.RouteDistance"));
    auto* CheckNotice=WidgetTree->ConstructWidget<UBorder>();FigmaUI::Panel(CheckNotice,FigmaUI::Inset,FigmaUI::Outline,4,FMargin(10));V->AddChildToVerticalBox(CheckNotice)->SetPadding(FMargin(0,0,0,8));
    FigmaUI::Text(Label(WidgetTree,CheckNotice,T(TEXT("Workflow.CheckInformation")),12),12,FigmaUI::Muted);
    Assignments=Label(WidgetTree,V,FText::GetEmpty(),12);FigmaUI::Text(Assignments,12,FigmaUI::Muted);
    auto* Warning=WidgetTree->ConstructWidget<UBorder>();FigmaUI::Panel(Warning,FLinearColor(FColor(36,30,25)),FigmaUI::Warning,5,FMargin(10));V->AddChildToVerticalBox(Warning)->SetPadding(FMargin(0,0,0,8));
    auto* WarningRow=WidgetTree->ConstructWidget<UHorizontalBox>();Warning->SetContent(WarningRow);
    auto* WarningSize=WidgetTree->ConstructWidget<USizeBox>();WarningSize->SetWidthOverride(17);WarningSize->SetHeightOverride(17);auto* WarningBadge=WidgetTree->ConstructWidget<UOverlay>();WarningSize->SetContent(WarningBadge);
    auto* WarningDisc=WarningBadge->AddChildToOverlay(FigmaUI::Icon(WidgetTree,TEXT("plan-detail-warning"),FVector2D(17,17)));WarningDisc->SetHorizontalAlignment(HAlign_Center);WarningDisc->SetVerticalAlignment(VAlign_Center);
    auto* Exclamation=WidgetTree->ConstructWidget<UTextBlock>();FigmaUI::Text(Exclamation,12,FigmaUI::Inset,true);Exclamation->SetText(User(TEXT("!")));Exclamation->SetJustification(ETextJustify::Center);auto* MarkSlot=WarningBadge->AddChildToOverlay(Exclamation);MarkSlot->SetHorizontalAlignment(HAlign_Center);MarkSlot->SetVerticalAlignment(VAlign_Center);
    auto* WarningIcon=WarningRow->AddChildToHorizontalBox(WarningSize);WarningIcon->SetPadding(FMargin(0,0,8,0));WarningIcon->SetVerticalAlignment(VAlign_Top);
    Issues=Label(WidgetTree,WarningRow,FText::GetEmpty(),12);FigmaUI::Text(Issues,12,FigmaUI::Warning);CastChecked<UHorizontalBoxSlot>(Issues->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* ReviewNotice=WidgetTree->ConstructWidget<UBorder>();FigmaUI::Panel(ReviewNotice,FigmaUI::Inset,FigmaUI::Outline,4,FMargin(10));V->AddChildToVerticalBox(ReviewNotice)->SetPadding(FMargin(0,0,0,10));
    FigmaUI::Text(Label(WidgetTree,ReviewNotice,T(TEXT("Plan.ReviewNotice")),12),12,FigmaUI::Muted);
    FigmaUI::Text(Label(WidgetTree,V,T(TEXT("Plan.Deployment")),18),18,FigmaUI::PrimaryText,true);
    Deployment=Label(WidgetTree,V,FText::GetEmpty(),12);FigmaUI::Text(Deployment,12,FigmaUI::Muted);CastChecked<UVerticalBoxSlot>(Deployment->Slot)->SetPadding(FMargin(0,6));
    auto* RecordLine=WidgetTree->ConstructWidget<USizeBox>();RecordLine->SetWidthOverride(420);RecordLine->SetHeightOverride(1);RecordLine->SetContent(FigmaUI::Icon(WidgetTree,TEXT("plan-detail-record-divider"),FVector2D(420,1)));auto* RecordDivider=V->AddChildToVerticalBox(RecordLine);RecordDivider->SetHorizontalAlignment(HAlign_Center);RecordDivider->SetPadding(FMargin(0,4,0,8));
    auto* ReadyRow=WidgetTree->ConstructWidget<UHorizontalBox>();V->AddChild(ReadyRow);
    Readiness=Label(WidgetTree,ReadyRow,FText::GetEmpty(),12);FigmaUI::Text(Readiness,12,FigmaUI::Muted);CastChecked<UHorizontalBoxSlot>(Readiness->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* ReadyDivider=WidgetTree->ConstructWidget<USizeBox>();ReadyDivider->SetWidthOverride(1);ReadyDivider->SetHeightOverride(26);auto* ReadyOverlay=WidgetTree->ConstructWidget<UOverlay>();ReadyDivider->SetContent(ReadyOverlay);auto* ReadyLine=ReadyOverlay->AddChildToOverlay(FigmaUI::Icon(WidgetTree,TEXT("plan-detail-readiness-divider"),FVector2D(1,26)));ReadyLine->SetHorizontalAlignment(HAlign_Center);ReadyLine->SetVerticalAlignment(VAlign_Center);ReadyRow->AddChildToHorizontalBox(ReadyDivider)->SetPadding(FMargin(10,0));
    MapReadiness=Label(WidgetTree,ReadyRow,FText::GetEmpty(),12);FigmaUI::Text(MapReadiness,12,FigmaUI::Muted);CastChecked<UHorizontalBoxSlot>(MapReadiness->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
}
void UPlanReviewWidget::Refresh(const TSharedPtr<FJsonObject>& State,const TSharedPtr<FJsonObject>& Plan){
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    Summary->SetText(Plan?User(Field(Plan,TEXT("name"))):T(TEXT("Plan.NoPlan")));
    Description->SetText(User(Field(Plan,TEXT("description"))));Description->SetVisibility(Description->GetText().IsEmpty()?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
    const auto SelectedId=Field(Sync->GetContext(),TEXT("active_mission_id"));
    TArray<FText> AssignmentLines;TSharedPtr<FJsonObject> SelectedMission,SelectedRoute;
    const TArray<TSharedPtr<FJsonValue>>* Ids=nullptr;
    if(Plan && Plan->TryGetArrayField(TEXT("mission_ids"),Ids))for(const auto& V:*Ids){
        const auto M=Find(State,TEXT("missions"),V->AsString()),R=Find(State,TEXT("paths"),Field(M,TEXT("route_id")));const TArray<TSharedPtr<FJsonValue>>* Points=nullptr;
        const int Count=R && R->TryGetArrayField(TEXT("waypoints"),Points)?Points->Num():0;
        AssignmentLines.Add(FText::Format(T(TEXT("Plan.MissionSummary")),User(Field(M,TEXT("name"))),User(AssignedUAVs(M)),FText::AsNumber(Count)));
        if(!SelectedMission || V->AsString()==SelectedId){SelectedMission=M;SelectedRoute=R;}
    }
    MissionValue->SetText(SelectedMission?User(Field(SelectedMission,TEXT("name"))):T(TEXT("Common.None")));MissionValue->SetToolTipText(MissionValue->GetText());
    AircraftValue->SetText(SelectedMission?User(AssignedUAVs(SelectedMission)):T(TEXT("Common.None")));AircraftValue->SetToolTipText(AircraftValue->GetText());
    const TArray<TSharedPtr<FJsonValue>>* Points=nullptr;const int Count=SelectedRoute && SelectedRoute->TryGetArrayField(TEXT("waypoints"),Points)?Points->Num():0;
    double Distance=0;bool Loop=false;if(SelectedRoute)SelectedRoute->TryGetBoolField(TEXT("bClosedLoop"),Loop);
    if(Points)for(int I=1;I<Points->Num()+(Loop && Points->Num()>2?1:0);++I){const auto A=(*Points)[I-1]->AsObject(),B=(*Points)[I%Points->Num()]->AsObject();const double Lat=FMath::DegreesToRadians((A->GetNumberField(TEXT("latitude"))+B->GetNumberField(TEXT("latitude")))*.5);const double Y=FMath::DegreesToRadians(A->GetNumberField(TEXT("latitude"))-B->GetNumberField(TEXT("latitude")))*6371000.,X=FMath::DegreesToRadians(A->GetNumberField(TEXT("longitude"))-B->GetNumberField(TEXT("longitude")))*6371000.*FMath::Cos(Lat);Distance+=FMath::Sqrt(X*X+Y*Y);}
    WaypointsValue->SetText(FText::AsNumber(Count));DistanceValue->SetText(FText::Format(User(TEXT("{0} m")),FText::AsNumber(FMath::RoundToInt(Distance))));DistanceValue->SetToolTipText(FText::Format(T(TEXT("Workflow.ReviewDistance")),FText::AsNumber(Distance)));
    Assignments->SetText(FText::Join(User(TEXT("\n")),AssignmentLines));Assignments->SetVisibility(AssignmentLines.Num()>1?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    TArray<FText> IssueLines=ValidationIssueLines(State,Plan);if(!IssueLines.IsEmpty())IssueLines.Insert(T(TEXT("Plan.Blocking")),0);IssueLines.Add(T(TEXT("Plan.Warnings")));Issues->SetText(FText::Join(User(TEXT("\n")),IssueLines));
    const auto Record=Find(State,TEXT("deployments"),Field(Plan,TEXT("deployment_id")));double At=0;if(Record)Record->TryGetNumberField(TEXT("deployed_at"),At);
    Deployment->SetText(Record?FText::Format(User(TEXT("{0}\n{1}")),User(Field(Record,TEXT("id"))),FText::AsDateTime(FDateTime::FromUnixTimestamp(At))):T(TEXT("Common.None")));
    Readiness->SetText(FText::Format(User(TEXT("Backend: {0}")),T(Sync->IsReady()?TEXT("Stage1.Online"):TEXT("Stage1.Offline"))));
    MapReadiness->SetText(FText::Format(User(TEXT("Map: {0}")),T(Sync->IsRoleOnline(TEXT("Map"))?TEXT("Stage1.Online"):TEXT("Stage1.Offline"))));
}
void USecurityPlanWorkspaceWidget::NativeOnInitialized(){
    Super::NativeOnInitialized();
    Content=WidgetTree->ConstructWidget<UVerticalBox>();WidgetTree->RootWidget=Content;
    auto Add=[&](UPanelWidget* Parent,const TCHAR* A,const TCHAR* K){auto* B=Button(WidgetTree,Parent,A,K);B->OnAction.AddDynamic(this,&USecurityPlanWorkspaceWidget::Action);FigmaUI::Button(B);FigmaUI::Text(Cast<UTextBlock>(B->GetContent()),14);Actions.Add(A,B);return B;};
    ConfigurationStatus=Label(WidgetTree,Content,T(TEXT("Configuration.Saved")),14);
    ConfigurationGuard=WidgetTree->ConstructWidget<UVerticalBox>();Content->AddChild(ConfigurationGuard);
    Label(WidgetTree,ConfigurationGuard,T(TEXT("Configuration.UnsavedPrompt")),16);
    auto* GuardRow=WidgetTree->ConstructWidget<UHorizontalBox>();ConfigurationGuard->AddChild(GuardRow);
    Add(GuardRow,TEXT("guard_save"),TEXT("Configuration.SaveContinue"));Add(GuardRow,TEXT("guard_discard"),TEXT("Configuration.DiscardContinue"));Add(GuardRow,TEXT("guard_keep"),TEXT("Configuration.KeepEditing"));
    ListPage=WidgetTree->ConstructWidget<UVerticalBox>();Content->AddChildToVerticalBox(ListPage)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* TitleRow=WidgetTree->ConstructWidget<UHorizontalBox>();ListPage->AddChildToVerticalBox(TitleRow)->SetPadding(FMargin(0,0,0,21));
    auto* ListTitle=Label(WidgetTree,TitleRow,T(TEXT("Nav.Plans")),36);FigmaUI::Text(ListTitle,36,FigmaUI::PrimaryText,true);
    Cast<UHorizontalBoxSlot>(ListTitle->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* NewButton=Add(TitleRow,TEXT("new_plan"),TEXT("Figma.Plans.NewPlan"));
    auto NewButtonStyle=NewButton->GetStyle();NewButtonStyle.SetNormal(FSlateRoundedBoxBrush(FigmaUI::Background,19,FigmaUI::PrimaryText,.6f));NewButton->SetStyle(NewButtonStyle);
    Cast<UHorizontalBoxSlot>(NewButton->Slot)->SetVerticalAlignment(VAlign_Center);
    auto* NewButtonRow=WidgetTree->ConstructWidget<UHorizontalBox>();NewButton->SetContent(NewButtonRow);
    auto* PlusSlot=NewButtonRow->AddChildToHorizontalBox(FigmaUI::Icon(WidgetTree,TEXT("plus-plan"),FVector2D(12,12)));PlusSlot->SetVerticalAlignment(VAlign_Center);PlusSlot->SetPadding(FMargin(2,0,10,0));
    FigmaUI::Text(Label(WidgetTree,NewButtonRow,T(TEXT("Figma.Plans.NewPlan")),14),14);
    Add(ListPage,TEXT("confirm_delete"),TEXT("Plan.Delete.Confirm"));Add(ListPage,TEXT("cancel_delete"),TEXT("Common.Cancel"));Add(ListPage,TEXT("refresh_delete"),TEXT("Plan.Delete.Refresh"));
    auto* ListSurface=WidgetTree->ConstructWidget<UBorder>();FigmaUI::Panel(ListSurface,FigmaUI::Surface,FigmaUI::Outline,9,FMargin(0));
    ListPage->AddChildToVerticalBox(ListSurface)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* ListOverlay=WidgetTree->ConstructWidget<UOverlay>();ListSurface->SetContent(ListOverlay);
    auto* CardScroll=WidgetTree->ConstructWidget<UScrollBox>();auto* CardSlot=ListOverlay->AddChildToOverlay(CardScroll);CardSlot->SetHorizontalAlignment(HAlign_Fill);CardSlot->SetVerticalAlignment(VAlign_Fill);
    PlanCards=WidgetTree->ConstructWidget<UVerticalBox>();CardScroll->AddChild(PlanCards);
    EmptyPlanState=WidgetTree->ConstructWidget<UVerticalBox>();auto* EmptySlot=ListOverlay->AddChildToOverlay(EmptyPlanState);EmptySlot->SetHorizontalAlignment(HAlign_Center);EmptySlot->SetVerticalAlignment(VAlign_Center);EmptySlot->SetPadding(FMargin(0,0,0,50));
    EmptyPlanState->AddChildToVerticalBox(FigmaUI::Icon(WidgetTree,TEXT("folder"),FVector2D(100,100)))->SetHorizontalAlignment(HAlign_Center);
    auto* EmptyTitle=Label(WidgetTree,EmptyPlanState,T(TEXT("Figma.Plans.EmptyTitle")),24);FigmaUI::Text(EmptyTitle,24,FigmaUI::PrimaryText,true);EmptyTitle->SetJustification(ETextJustify::Center);
    auto* EmptyHint=Label(WidgetTree,EmptyPlanState,T(TEXT("Figma.Plans.EmptyHint")),17);FigmaUI::Text(EmptyHint,17,FigmaUI::Muted);EmptyHint->SetJustification(ETextJustify::Center);Cast<UVerticalBoxSlot>(EmptyHint->Slot)->SetPadding(FMargin(0,9,0,0));
    WorkspaceScroll=WidgetTree->ConstructWidget<UScrollBox>();Content->AddChildToVerticalBox(WorkspaceScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Workspace=WidgetTree->ConstructWidget<UVerticalBox>();WorkspaceScroll->AddChild(Workspace);
    Add(Workspace,TEXT("back_list"),TEXT("Workflow.BackPlans"));
    Add(Workspace,TEXT("resume_configuration"),TEXT("Configuration.Resume"));
    auto* PlanTools=WidgetTree->ConstructWidget<UHorizontalBox>();Workspace->AddChild(PlanTools);
    Add(PlanTools,TEXT("edit_plan"),TEXT("Plan.EditExisting"));Add(PlanTools,TEXT("delete_plan_prompt"),TEXT("Plan.Delete.Action"));
    WorkspaceTitle=Label(WidgetTree,Workspace,FText::GetEmpty(),26);
    Summary=Label(WidgetTree,Workspace,FText::GetEmpty(),14);
    Add(Workspace,TEXT("request_map_plan_move"),TEXT("Geometry.MovePlan"));
    GeometryStatus=Label(WidgetTree,Workspace,FText::GetEmpty(),16);
    Steps=Label(WidgetTree,Workspace,FText::GetEmpty(),16);
    MissionList=CreateWidget<UPlanMissionListWidget>(GetOwningPlayer());Workspace->AddChild(MissionList);MissionList->OnAction.AddDynamic(this,&USecurityPlanWorkspaceWidget::MissionAction);
    auto Page=[&](){auto* V=WidgetTree->ConstructWidget<UVerticalBox>();Workspace->AddChild(V);return V;};
    BasicPageHost=Page();BasicPage=WidgetTree->ConstructWidget<UVerticalBox>();BasicPageHost->AddChild(BasicPage);TaskPage=Page();RoutePage=Page();ReviewPage=Page();
    auto Input=[&](UPanelWidget* Parent,const TCHAR* Key){auto* Caption=Label(WidgetTree,Parent,T(Key),16);FigmaUI::Text(Caption,16,FigmaUI::PrimaryText,true);auto* E=WidgetTree->ConstructWidget<UEditableTextBox>();FigmaUI::Input(E);Parent->AddChild(E);return E;};
    BasicHeading=Label(WidgetTree,BasicPage,T(TEXT("Workflow.NewPlan")),24);
    PlanName=Input(BasicPage,TEXT("Plan.Name"));PlanName->SetHintText(T(TEXT("Figma.Plans.NameHint")));PlanName->OnTextChanged.AddDynamic(this,&USecurityPlanWorkspaceWidget::PlanNameChanged);
    // Keep the acknowledged model field shared by save/dirty-state logic and automation.
    // The multiline editor is its presentation; it never owns a separate draft.
    Description=WidgetTree->ConstructWidget<UEditableTextBox>();BasicPage->AddChild(Description);Description->SetVisibility(ESlateVisibility::Collapsed);
    auto* DescriptionLabel=Label(WidgetTree,BasicPage,T(TEXT("Plan.Description")),16);FigmaUI::Text(DescriptionLabel,16,FigmaUI::PrimaryText,true);
    DescriptionEditor=WidgetTree->ConstructWidget<UMultiLineEditableTextBox>();DescriptionEditor->SetHintText(T(TEXT("Figma.Plans.DescriptionHint")));DescriptionEditor->SetAutoWrapText(true);
    DescriptionEditor->WidgetStyle=PlanName->GetWidgetStyle();DescriptionEditor->OnTextChanged.AddDynamic(this,&USecurityPlanWorkspaceWidget::DescriptionChanged);
    auto* DescriptionHeight=WidgetTree->ConstructWidget<USizeBox>();DescriptionHeight->SetHeightOverride(104);DescriptionHeight->SetContent(DescriptionEditor);BasicPage->AddChild(DescriptionHeight);
    auto* BasicActions=WidgetTree->ConstructWidget<UHorizontalBox>();BasicPage->AddChildToVerticalBox(BasicActions)->SetHorizontalAlignment(HAlign_Right);
    Add(BasicActions,TEXT("cancel_new"),TEXT("Common.Cancel"));Add(BasicActions,TEXT("create_plan"),TEXT("Workflow.CreateContinue"));
    Add(BasicPage,TEXT("update_plan"),TEXT("Common.Save"));Add(BasicPage,TEXT("next_task"),TEXT("Workflow.NextTask"));
    Label(WidgetTree,TaskPage,T(TEXT("Workflow.TaskConfig")),20);
    MissionName=Input(TaskPage,TEXT("Plan.MissionName"));
    Label(WidgetTree,TaskPage,T(TEXT("Workflow.AssignedUAV")),15);
    UAVs=WidgetTree->ConstructWidget<UComboBoxString>();UAVs->OnGenerateWidgetEvent.BindDynamic(this,&USecurityPlanWorkspaceWidget::UAVOption);UAVs->OnSelectionChanged.AddDynamic(this,&USecurityPlanWorkspaceWidget::UAVSelected);
    auto* UAVHeight=WidgetTree->ConstructWidget<USizeBox>();UAVHeight->SetMinDesiredHeight(40);UAVHeight->SetContent(UAVs);TaskPage->AddChild(UAVHeight);
    auto ComboStyle=UAVs->GetWidgetStyle();auto ComboButton=ComboStyle.ComboButtonStyle;FButtonStyle ButtonStyle;ButtonStyle.SetNormal(FSlateRoundedBoxBrush(CommandTheme::Elevated,5));ButtonStyle.SetHovered(FSlateRoundedBoxBrush(CommandTheme::Hover,5));ButtonStyle.SetPressed(FSlateRoundedBoxBrush(CommandTheme::Selected,5));ComboButton.SetButtonStyle(ButtonStyle);ComboStyle.SetComboButtonStyle(ComboButton);UAVs->SetWidgetStyle(ComboStyle);
    UAVHeight->SetVisibility(ESlateVisibility::Collapsed);
    AssignmentList=WidgetTree->ConstructWidget<UVerticalBox>();TaskPage->AddChild(AssignmentList);
    Add(TaskPage,TEXT("clear_unavailable"),TEXT("Reservation.ClearUnavailable"));
    SpacingInput=Input(TaskPage,TEXT("Formation.LineSpacing"));SpacingInput->SetText(User(TEXT("3.0")));
    Add(TaskPage,TEXT("assign_group"),TEXT("Formation.SaveAssignment"));
    Label(WidgetTree,TaskPage,T(TEXT("Workflow.Patrol")),14);
    auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();TaskPage->AddChild(Row);
    Add(Row,TEXT("assign"),TEXT("Plan.Assign"));Add(Row,TEXT("rename_mission"),TEXT("Plan.Rename"));Add(Row,TEXT("add_mission"),TEXT("Plan.AddMission"));
    Add(TaskPage,TEXT("request_map_route_edit"),TEXT("Workflow.EditMap"));
    Add(TaskPage,TEXT("basic_step"),TEXT("Workflow.BasicInfo"));
    Label(WidgetTree,RoutePage,T(TEXT("Workflow.RouteEditing")),20);RouteStatus=Label(WidgetTree,RoutePage,FText::GetEmpty(),16);
    Add(RoutePage,TEXT("retry_map"),TEXT("Workflow.ReturnMap"));
    ReviewHeading=Label(WidgetTree,ReviewPage,T(TEXT("Workflow.PreDeployReview")),20);
    DeploymentBlockers=Label(WidgetTree,ReviewPage,FText::GetEmpty(),16);
    DeploymentBlockers->SetColorAndOpacity(CommandTheme::Warning);
    Add(ReviewPage,TEXT("confirm_deployment"),TEXT("Execution.Deploy"));
    Add(ReviewPage,TEXT("execution_start_prompt"),TEXT("Execution.Start"));
    Add(ReviewPage,TEXT("execution_open"),TEXT("Execution.Open"));
    Review=CreateWidget<UPlanReviewWidget>(GetOwningPlayer());ReviewPage->AddChild(Review);
    Add(ReviewPage,TEXT("task_step"),TEXT("Workflow.BackEdit"));
    Add(ReviewPage,TEXT("copy_plan"),TEXT("Plan.CopyExplicit"));
    ExecutionHistory=Label(WidgetTree,ReviewPage,FText::GetEmpty(),14);
    ExecutionPage=Page();ExecutionMode=Label(WidgetTree,ExecutionPage,T(TEXT("Execution.Simulation")),18);
    ExecutionSummary=Label(WidgetTree,ExecutionPage,FText::GetEmpty(),22);
    ScheduleEnabled=WidgetTree->ConstructWidget<UCheckBox>();ExecutionPage->AddChild(ScheduleEnabled);Label(WidgetTree,ScheduleEnabled,T(TEXT("Schedule.Enable")),16);
    ScheduleInput=Input(ExecutionPage,TEXT("Schedule.LocalTime"));
    Add(ExecutionPage,TEXT("execution_start_now"),TEXT("Schedule.StartNow"));Add(ExecutionPage,TEXT("execution_reschedule"),TEXT("Schedule.Reschedule"));Add(ExecutionPage,TEXT("execution_cancel_schedule"),TEXT("Schedule.Cancel"));
    ExecutionProgress=WidgetTree->ConstructWidget<UProgressBar>();ExecutionPage->AddChild(ExecutionProgress);
    ExecutionPrompt=Label(WidgetTree,ExecutionPage,FText::GetEmpty(),16);
    for(const auto& Item:TArray<TPair<FString,FString>>{{TEXT("confirm"),TEXT("Confirm")},{TEXT("pause"),TEXT("Pause")},{TEXT("resume"),TEXT("Resume")},{TEXT("return_prompt"),TEXT("Return")},{TEXT("abort_prompt"),TEXT("Abort")},{TEXT("cancel"),TEXT("Cancel")},{TEXT("details"),TEXT("History")},{TEXT("video"),TEXT("Video")}})
        Add(ExecutionPage,*(TEXT("execution_")+Item.Key),*(TEXT("Execution.")+Item.Value));
    // Keep the existing action bindings available to regression automation, hidden from the product flow.
    Plans=WidgetTree->ConstructWidget<UComboBoxString>();Plans->OnSelectionChanged.AddDynamic(this,&USecurityPlanWorkspaceWidget::PlanSelected);
    for(const TCHAR* A:{TEXT("unassign"),TEXT("delete_mission"),TEXT("validate"),TEXT("review_page"),TEXT("review"),TEXT("deploy_page"),TEXT("deploy"),TEXT("cancel")}){
        auto* B=Add(Content,A,TEXT("Common.Cancel"));B->SetVisibility(ESlateVisibility::Collapsed);
    }
    Result=Label(WidgetTree,Content,FText::GetEmpty(),14);
    // Workflow controls keep their existing bindings with the command palette.
    for(UVerticalBox* StepPage:{Workspace.Get(),TaskPage.Get(),RoutePage.Get(),ReviewPage.Get(),ExecutionPage.Get()})for(UWidget* Child:StepPage->GetAllChildren())if(auto* ChildSlot=Cast<UVerticalBoxSlot>(Child->Slot))ChildSlot->SetPadding(FMargin(0,6));
    for(const TCHAR* Key:{TEXT("create_plan"),TEXT("request_map_route_edit"),TEXT("confirm_deployment"),TEXT("execution_start_prompt"),TEXT("execution_confirm"),TEXT("execution_pause"),TEXT("execution_resume")})FigmaUI::Button(Actions[Key],true);
    CommandTheme::Text(Cast<UTextBlock>(Actions[TEXT("execution_return_prompt")]->GetContent()),14,CommandTheme::Warning);
    CommandTheme::Text(Cast<UTextBlock>(Actions[TEXT("execution_abort_prompt")]->GetContent()),14,FLinearColor(1,.3f,.3f));
    BuildPlanPresentation();
    Refresh();
}
void USecurityPlanWorkspaceWidget::Refresh(){
    if(!Content)return;auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();const auto State=Sync->GetPlans();
    const bool LocalDirty=HasUnsavedChanges();
    const FString IncomingPlan=Field(Sync->GetContext(),TEXT("active_security_plan_id")),IncomingMission=Field(Sync->GetContext(),TEXT("active_mission_id"));
    const bool PreserveDraft=!LoadedSelection.IsEmpty() && LocalDirty && !bPending && !bCreating && (IncomingPlan!=PlanId || IncomingMission!=MissionId);
    if(PreserveDraft)ErrorCode=TEXT("CONFIGURATION_CONTEXT_CHANGED");
    else if(!bCreating){PlanId=IncomingPlan;MissionId=IncomingMission;}
    const auto P=Find(State,TEXT("plans"),PlanId),M=Find(State,TEXT("missions"),MissionId);
    bRefreshing=true;TArray<FString> Ids,Labels;const auto All=Object(State,TEXT("plans"));if(All)for(const auto& E:All->Values){Ids.Add(FString(E.Key.ToView()));Labels.Add(Field(E.Value->AsObject(),TEXT("name")));}
    if(Labels!=PlanLabels || Ids!=PlanIds){Plans->ClearOptions();for(const auto& L:Labels)Plans->AddOption(L);PlanLabels=Labels;PlanIds=Ids;}
    const int Index=PlanIds.IndexOfByKey(PlanId);if(Index>=0)Plans->SetSelectedOption(PlanLabels[Index]);
    Ids.Empty();for(const auto& D:GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetFriendlyDroneDescriptors())Ids.Add(FString::Printf(TEXT("UAV-%02d"),D.DroneId));
    if(Ids!=UavIds){const auto Selected=UAVs->GetSelectedOption();UAVs->ClearOptions();for(const auto& ID:Ids)UAVs->AddOption(ID);UavIds=Ids;UAVs->SetSelectedOption(Selected);}
    const FString AssignmentKey=PlanId+TEXT("/")+MissionId+FString::Join(Ids,TEXT(","))+AssignedUAVs(M)+(M && M->HasField(TEXT("spacing_m"))?FString::SanitizeFloat(M->GetNumberField(TEXT("spacing_m"))):TEXT(""));
    if(AssignmentSelection!=AssignmentKey){
        const bool PreserveAssignment=LocalDirty && LoadedSelection==PlanId+TEXT("/")+MissionId && !AssignmentSelection.IsEmpty();
        TMap<FString,bool> PreviousChecks;for(const auto& Entry:AssignmentChecks)PreviousChecks.Add(Entry.Key,Entry.Value->IsChecked());
        const FText PreviousSpacing=SpacingInput->GetText();
        AssignmentSelection=AssignmentKey;AssignmentList->ClearChildren();AssignmentChecks.Empty();
        const TArray<TSharedPtr<FJsonValue>>* Members=nullptr;if(M)M->TryGetArrayField(TEXT("assigned_uav_ids"),Members);
        for(const auto& ID:Ids){auto* Check=WidgetTree->ConstructWidget<UCheckBox>();Check->HorizontalAlignment=HAlign_Fill;AssignmentList->AddChildToVerticalBox(Check)->SetHorizontalAlignment(HAlign_Fill);Label(WidgetTree,Check,User(ID),16)->SetAutoWrapText(false);bool Assigned=Field(M,TEXT("assigned_uav_id"))==ID;
            if(Members)for(const auto& V:*Members)Assigned|=V->AsString()==ID;if(PreserveAssignment && PreviousChecks.Contains(ID))Assigned=PreviousChecks[ID];Check->SetIsChecked(Assigned);AssignmentChecks.Add(ID,Check);}
        double Spacing=3.;if(M)M->TryGetNumberField(TEXT("spacing_m"),Spacing);SpacingInput->SetText(PreserveAssignment?PreviousSpacing:User(FString::SanitizeFloat(Spacing)));
    }
    // Refresh in place on every execution update: preserve unsaved checkbox choices.
    bool HasUnavailableSelection=false;
    for(const auto& Item:AssignmentChecks){const auto E=UAVReservation(State,PlanId,Item.Key);
        HasUnavailableSelection|=E.IsValid() && Item.Value->IsChecked();
        Item.Value->SetIsEnabled(!E);if(auto* Caption=Cast<UTextBlock>(Item.Value->GetContent()))
            Caption->SetText(E?FText::Format(T(TEXT("Reservation.InUse")),User(Item.Key),ProductText::Source(Field(E,TEXT("plan_name")))):User(Item.Key));
    }
    Actions[TEXT("clear_unavailable")]->SetVisibility(HasUnavailableSelection?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    bRefreshing=false;const FString Selection=PlanId+TEXT("/")+MissionId;
    if(!bCreating && Selection!=LoadedSelection && (PlanId.IsEmpty() || P)){LoadedSelection=Selection;bReview=false;bConfirm=false;bExecutionView=false;ExecutionId.Empty();ExecutionIntent.Empty();PlanName->SetText(User(Field(P,TEXT("name"))));Description->SetText(User(Field(P,TEXT("description"))));MissionName->SetText(User(Field(M,TEXT("name"))));if(Field(M,TEXT("assigned_uav_id")).IsEmpty())UAVs->ClearSelection();else UAVs->SetSelectedOption(Field(M,TEXT("assigned_uav_id")));}
    const auto Status=Field(P,TEXT("status"));const bool Deployed=Status==TEXT("DEPLOYED"),Ready=Status==TEXT("READY"),Reviewed=PlanUI::Reviewed(P);
    if(P && ConfirmRevision!=P->GetNumberField(TEXT("content_revision")))bConfirm=false;
    AssignmentList->SetIsEnabled(!Deployed && !bPending);SpacingInput->SetIsReadOnly(Deployed);
    PlanName->SetIsReadOnly(Deployed && !bCreating);Description->SetIsReadOnly(Deployed && !bCreating);MissionName->SetIsReadOnly(Deployed);UAVs->SetIsEnabled(!Deployed);
    for(const auto& A:Actions)A.Value->SetIsEnabled(!bPending && Sync->IsReady());
    for(const TCHAR* A:{TEXT("update_plan"),TEXT("add_mission"),TEXT("rename_mission"),TEXT("delete_mission"),TEXT("assign"),TEXT("assign_group"),TEXT("unassign"),TEXT("request_map_route_edit")})Actions[A]->SetIsEnabled(!bPending && Sync->IsReady() && P && !Deployed);
    const bool MovingBlocked=ExecutionUI::Latest(State,PlanId,true).IsValid();
    Actions[TEXT("request_map_plan_move")]->SetIsEnabled(!bPending && Sync->IsReady() && P && M && !MovingBlocked);
    GeometryStatus->SetText(MovingBlocked?T(TEXT("Errors.PLAN_EXECUTING")):FText::GetEmpty());GeometryStatus->SetVisibility(MovingBlocked && !bCreating?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    Actions[TEXT("create_plan")]->SetIsEnabled(!bPending && Sync->IsReady() && !PlanName->GetText().ToString().TrimStartAndEnd().IsEmpty());
    auto Show=[&](const TCHAR* A,bool B){Actions[A]->SetVisibility(B?ESlateVisibility::Visible:ESlateVisibility::Collapsed);};
    Show(TEXT("confirm_delete"),!PendingDeletePlan.IsEmpty());Show(TEXT("cancel_delete"),!PendingDeletePlan.IsEmpty());Show(TEXT("copy_plan"),Deployed);Show(TEXT("deploy_page"),false);
    ReviewHeading->SetText(T(Deployed?TEXT("Workflow.DeploymentSuccess"):TEXT("Workflow.PreDeployReview")));
    Review->Refresh(State,P);MissionList->Refresh(State,P,MissionId);
    if(!bInitializedSelection && Sync->IsHydrated()){bInitializedSelection=true;bList=!P;}
    const FString Workflow=Field(P,TEXT("workflow_step"));
    // Backend workflow updates choose the editor step, not whether an explicitly
    // closed editor should reopen over the plan list.
    if(LastWorkflow!=PlanId+Workflow){LastWorkflow=PlanId+Workflow;CurrentStep=Workflow==TEXT("BasicInfo")?0:Workflow==TEXT("RouteEditing")?2:Workflow==TEXT("PreDeployReview")?3:1;}
    if(Deployed)CurrentStep=4;
    if(bCreating)CurrentStep=0;
    auto Visible=[](UWidget* W,bool Yes){W->SetVisibility(Yes?ESlateVisibility::Visible:ESlateVisibility::Collapsed);};
    Visible(NewPlanDialog,false);Visible(BasicHeading,!bCreating);Visible(Summary,!bCreating);BasicHeading->SetText(P?FText::Format(T(TEXT("Workflow.CreatedAt")),FText::AsDateTime(FDateTime::FromUnixTimestamp(P->GetNumberField(TEXT("created_at"))))):FText::GetEmpty());
    Visible(ListPage,bList || bCreating);Visible(Workspace,!bList && !bCreating);Visible(WorkspaceScroll,!bList && !bCreating);Visible(BasicPage,CurrentStep==0);Visible(TaskPage,CurrentStep==1);Visible(RoutePage,CurrentStep==2);Visible(ReviewPage,CurrentStep>=3);
    Show(TEXT("assign"),false);Show(TEXT("next_task"),!bCreating);Show(TEXT("create_plan"),bCreating);Show(TEXT("cancel_new"),bCreating);Show(TEXT("update_plan"),!bCreating);Show(TEXT("task_step"),!Deployed && !bCreating);
    WorkspaceTitle->SetText(bCreating?T(TEXT("Workflow.NewPlan")):User(Field(P,TEXT("name"))));
    TArray<FText> StepNames;const TCHAR* Keys[]={TEXT("Workflow.StepInfo"),TEXT("Workflow.StepTask"),TEXT("Workflow.StepRoute"),TEXT("Workflow.StepCheck"),TEXT("Workflow.StepDeploy")};
    for(int I=0;I<5;++I)StepNames.Add(FText::Format(T(I==CurrentStep?TEXT("Workflow.ActiveStep"):TEXT("Workflow.Step")),FText::AsNumber(I+1),T(Keys[I])));
    Steps->SetText(FText::Join(User(TEXT("   /   ")),StepNames));
    Actions[TEXT("request_map_route_edit")]->SetIsEnabled(!bPending && Sync->IsReady() && P && !Deployed && !Field(M,TEXT("assigned_uav_id")).IsEmpty());
    const auto Validation=Object(P,TEXT("validation"));const TArray<TSharedPtr<FJsonValue>>* Issues=nullptr;
    const bool Complete=Validation && Validation->TryGetArrayField(TEXT("issues"),Issues) && Issues->IsEmpty();
    TArray<FText> Blockers=ValidationIssueLines(State,P);
    if(!Complete && Blockers.IsEmpty())Blockers.Add(T(TEXT("Errors.PLAN_NOT_READY")));
    if(!Sync->IsReady() || !Sync->IsRoleOnline(TEXT("Map")))Blockers.Add(FText::Format(T(TEXT("Workflow.Readiness")),T(Sync->IsReady()?TEXT("Stage1.Online"):TEXT("Stage1.Offline")),T(Sync->IsRoleOnline(TEXT("Map"))?TEXT("Stage1.Online"):TEXT("Stage1.Offline"))));
    if(!CanEditConfiguration())Blockers.Add(T(TEXT("Configuration.ReadOnly")));
    if(!Blockers.IsEmpty())Blockers.Insert(T(TEXT("Plan.Blocking")),0);
    DeploymentBlockers->SetText(FText::Join(User(TEXT("\n")),Blockers));
    DeploymentBlockers->SetVisibility(!Deployed && !Blockers.IsEmpty()?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    Show(TEXT("confirm_deployment"),!Deployed);Actions[TEXT("confirm_deployment")]->SetIsEnabled(!bPending && Sync->IsReady() && Sync->IsRoleOnline(TEXT("Map")) && Complete);
    const auto Route=Find(State,TEXT("paths"),Field(M,TEXT("route_id")));const TArray<TSharedPtr<FJsonValue>>* Points=nullptr;
    const int Count=Route && Route->TryGetArrayField(TEXT("waypoints"),Points)?Points->Num():0;
    RouteStatus->SetText(FText::Format(T(Sync->IsRoleOnline(TEXT("Map"))?TEXT("Workflow.MapEditingStatus"):TEXT("Workflow.MapOfflineStatus")),User(Field(P,TEXT("name"))),User(Field(M,TEXT("name"))),User(AssignedUAVs(M)),FText::AsNumber(Count)));
    Show(TEXT("edit_plan"),Deployed);Show(TEXT("delete_plan_prompt"),P.IsValid() && !bCreating);
    RefreshPlanCards(State);
    TArray<FText> Lines{P?ProductText::Get(TEXT("Plan.")+Status):T(TEXT("Plan.NoPlan"))};const auto V=Object(P,TEXT("validation"));if(V)Lines.Add(FText::Format(T(TEXT("Plan.Counts")),FText::AsNumber(V->GetNumberField(TEXT("mission_count"))),FText::AsNumber(V->GetNumberField(TEXT("assigned_count"))),FText::AsNumber(V->GetNumberField(TEXT("routes_ready")))));
    if(Deployed && ExecutionUI::Latest(State,PlanId))Lines[0]=T(TEXT("Workflow.DeploymentSuccess"));
    Summary->SetText(Lines.Num()>1?Lines[1]:User(Field(P,TEXT("description"))));
    RefreshExecution(State,P);
    RefreshConfiguration();
    RefreshPlanPresentation();
    FString EditingMission;const auto Sessions=Object(State,TEXT("edit_sessions"));if(Sessions)for(const auto& E:Sessions->Values){const auto Session=E.Value->AsObject();if(Field(Session,TEXT("plan_id"))==PlanId){EditingMission=Field(Find(State,TEXT("missions"),Field(Session,TEXT("mission_id"))),TEXT("name"));break;}}
    if(!EditingMission.IsEmpty()){
        for(const TCHAR* A:{TEXT("review_page"),TEXT("review"),TEXT("deploy_page"),TEXT("deploy"),TEXT("confirm_deployment")})Actions[A]->SetIsEnabled(false);
        Result->SetText(FText::Format(T(TEXT("Plan.EditingBlock")),User(EditingMission)));Result->SetVisibility(ESlateVisibility::Visible);return;
    }
    Result->SetText(bPending?T(TEXT("Common.Waiting")):ErrorCode.IsEmpty()?FText::GetEmpty():!ExecutionErrorDetails.IsEmpty()?ExecutionErrorDetails:ProductText::Get(TEXT("Errors.")+ErrorCode));
    Result->SetVisibility(Result->GetText().IsEmpty()?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
}
void USecurityPlanWorkspaceWidget::RefreshPlanCards(const TSharedPtr<FJsonObject>& State){
    FString NewSignature;
    for(const auto& Id:PlanIds){const auto Card=Find(State,TEXT("plans"),Id);const TArray<TSharedPtr<FJsonValue>>* Missions=nullptr;double Updated=0;
        if(Card)Card->TryGetNumberField(TEXT("updated_at"),Updated);
        for(const auto& Value:{Id,Field(Card,TEXT("name")),Field(Card,TEXT("status"))})AddVisibleKey(NewSignature,Value);
        AddVisibleKey(NewSignature,FString::FromInt(Card && Card->TryGetArrayField(TEXT("mission_ids"),Missions)?Missions->Num():0));
        AddVisibleKey(NewSignature,LexToString(Updated));
    }
    if(NewSignature!=CardsSignature){CardsSignature=NewSignature;PlanCards->ClearChildren();PlanActionTargets.Empty();PlanActionIds.Empty();
        for(int I=0;I<PlanIds.Num();++I){const auto Card=Find(State,TEXT("plans"),PlanIds[I]);
            const int32 ActionId=++NextPlanActionId;PlanActionTargets.Add(ActionId,PlanIds[I]);PlanActionIds.Add(ActionId);
            auto* CardPanel=WidgetTree->ConstructWidget<UBorder>();FigmaUI::Panel(CardPanel,FigmaUI::Inset,FigmaUI::Outline,9,FMargin(20));PlanCards->AddChildToVerticalBox(CardPanel)->SetPadding(FMargin(16,16,16,0));
            auto* CardContent=WidgetTree->ConstructWidget<UVerticalBox>();CardPanel->SetContent(CardContent);
            FigmaUI::Text(Label(WidgetTree,CardContent,User(Field(Card,TEXT("name"))),22),22,FigmaUI::PrimaryText,true);
            auto* CardSummary=Label(WidgetTree,CardContent,FText::Format(T(TEXT("Workflow.PlanCard")),ProductText::Get(TEXT("Plan.")+Field(Card,TEXT("status"))),FText::AsNumber(Card->GetArrayField(TEXT("mission_ids")).Num()),FText::AsDateTime(FDateTime::FromUnixTimestamp(Card->GetNumberField(TEXT("updated_at"))))));FigmaUI::Text(CardSummary,14,FigmaUI::Muted);Cast<UVerticalBoxSlot>(CardSummary->Slot)->SetPadding(FMargin(0,9));
            auto* CardActions=WidgetTree->ConstructWidget<UHorizontalBox>();CardContent->AddChild(CardActions);
            auto* B=Button(WidgetTree,CardActions,TEXT("open_card"),TEXT("Workflow.OpenPlan"),ActionId);B->OnAction.AddDynamic(this,&USecurityPlanWorkspaceWidget::Action);FigmaUI::Button(B,true);
            auto* CopyButton=Button(WidgetTree,CardActions,TEXT("copy_card"),TEXT("Plan.CopyExplicit"),ActionId);CopyButton->OnAction.AddDynamic(this,&USecurityPlanWorkspaceWidget::Action);FigmaUI::Button(CopyButton);Cast<UHorizontalBoxSlot>(CopyButton->Slot)->SetPadding(FMargin(10,0));
            auto* DeleteButton=Button(WidgetTree,CardActions,TEXT("delete_card"),TEXT("Plan.Delete.Action"),ActionId);DeleteButton->OnAction.AddDynamic(this,&USecurityPlanWorkspaceWidget::Action);FigmaUI::Button(DeleteButton);
        }
    }
}

void USecurityPlanWorkspaceWidget::Submit(const TSharedRef<FJsonObject>& R,TFunction<void()> OnSuccess){
    if(bPending)return;bPending=true;ErrorCode.Empty();ExecutionErrorDetails=FText::GetEmpty();const auto Weak=TWeakObjectPtr<USecurityPlanWorkspaceWidget>(this);
    if(!GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->SubmitPlan(R,[Weak,OnSuccess](TSharedPtr<FJsonObject> Reply){if(!Weak.IsValid())return;Weak->bPending=false;Weak->ErrorCode=PlanUI::Error(Reply);Weak->ExecutionErrorDetails=ReservationError(Reply);if(Weak->ErrorCode.IsEmpty()){Weak->bCreating=false;Weak->bList=Field(Weak->GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetContext(),TEXT("active_security_plan_id")).IsEmpty();}Weak->Refresh();if(Weak->ErrorCode.IsEmpty() && OnSuccess)OnSuccess();})){bPending=false;ErrorCode=TEXT("SYNC_OFFLINE");Refresh();}
}
void USecurityPlanWorkspaceWidget::PlanSelected(FString Item,ESelectInfo::Type){if(bRefreshing)return;const int I=PlanLabels.IndexOfByKey(Item);if(I<0)return;auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("action"),TEXT("select"));R->SetStringField(TEXT("plan_id"),PlanIds[I]);const auto P=Find(GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetPlans(),TEXT("plans"),PlanIds[I]);if(P && !P->GetArrayField(TEXT("mission_ids")).IsEmpty())R->SetStringField(TEXT("mission_id"),P->GetArrayField(TEXT("mission_ids"))[0]->AsString());GuardConfigurationChange([this,R]{Submit(R);});}
void USecurityPlanWorkspaceWidget::MissionAction(FName Name,int32 Index){
    FString TargetPlan,ID;if(!MissionList->ResolveAction(Index,TargetPlan,ID) || TargetPlan!=PlanId)return;auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    const auto Plan=Find(Sync->GetPlans(),TEXT("plans"),TargetPlan);const TArray<TSharedPtr<FJsonValue>>* Ids=nullptr;bool Contains=false;
    if(Plan && Plan->TryGetArrayField(TEXT("mission_ids"),Ids))for(const auto& Id:*Ids)Contains|=Id->AsString()==ID;
    if(!Contains)return;
    if(Name==TEXT("video_mission")){Sync->OpenVideo(Field(Find(Sync->GetPlans(),TEXT("missions"),ID),TEXT("assigned_uav_id")));return;}
    if(Name==TEXT("edit_mission")){EditMission(PlanId,ID);return;}
    if(Name==TEXT("execute_mission")){PrepareMissionExecution(PlanId,ID);return;}
    SelectMission(PlanId,ID,[this]{bExecutionView=false;ExecutionIntent.Empty();Refresh();});
}
void USecurityPlanWorkspaceWidget::Action(FName Name,int32 Index){
    if(bPending)return;
    if(!PendingDeletePlan.IsEmpty() && Name!=TEXT("cancel_delete") && Name!=TEXT("confirm_delete") && Name!=TEXT("refresh_delete"))return;
    if(Name==TEXT("guard_keep")){bConfigurationGuard=false;GuardedConfigurationChange={};AfterConfigurationFinish={};Refresh();return;}
    if(Name==TEXT("guard_save")){auto Continue=GuardedConfigurationChange;SaveConfiguration([this,Continue]{GuardedConfigurationChange={};if(Continue)Continue();});return;}
    if(Name==TEXT("guard_discard")){if(bPending)return;ResetConfigurationInputs();bConfigurationGuard=false;auto Continue=MoveTemp(GuardedConfigurationChange);if(Continue)Continue();Refresh();return;}
    if(Name==TEXT("resume_configuration")){BeginConfiguration(PlanId,[this]{FocusConfiguration();});return;}
    if(Name==TEXT("clear_unavailable")){
        if(bPending)return;const auto State=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetPlans();
        for(const auto& Item:AssignmentChecks)if(UAVReservation(State,PlanId,Item.Key))Item.Value->SetIsChecked(false);
        ErrorCode.Empty();ExecutionErrorDetails=FText::GetEmpty();Refresh();return;
    }
    if(Name.ToString().StartsWith(TEXT("execution_"))){ExecutionAction(Name);return;}
    if(Name==TEXT("back_list") || Name==TEXT("cancel_new")){RequestFinishConfiguration();return;}
    if(Name==TEXT("new_plan")){BeginConfiguration(TEXT(""),[this]{bList=false;bCreating=true;PlanName->SetText(FText::GetEmpty());Description->SetText(FText::GetEmpty());Refresh();});return;}
    if(Name==TEXT("create_plan") || Name==TEXT("update_plan") || Name==TEXT("assign_group") || Name==TEXT("rename_mission")){SaveConfiguration();return;}
    if(Name==TEXT("basic_step") || Name==TEXT("task_step") || Name==TEXT("next_task")){
        auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("action"),TEXT("set_workflow_step"));R->SetStringField(TEXT("plan_id"),PlanId);R->SetStringField(TEXT("workflow_step"),Name==TEXT("basic_step")?TEXT("BasicInfo"):TEXT("TaskConfig"));SaveConfiguration([this,R]{Submit(R);});return;
    }
    if(Name==TEXT("edit_plan")){if(!MissionId.IsEmpty())EditMission(PlanId,MissionId);return;}
    if(Name==TEXT("cancel_delete")){PendingDeletePlan.Empty();PendingDeleteName.Empty();ErrorCode.Empty();Refresh();return;}
    if(Name==TEXT("refresh_delete")){RefreshDeleteConfirmation();return;}
    if(Name==TEXT("confirm_delete")){
        if(PendingDeletePlan.IsEmpty() || DeletePlanNameInput->GetText().ToString()!=PendingDeleteName)return;
        auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("action"),TEXT("delete_plan"));R->SetStringField(TEXT("plan_id"),PendingDeletePlan);
        R->SetStringField(TEXT("confirmation_name"),PendingDeleteName);R->SetNumberField(TEXT("expected_content_revision"),PendingDeleteRevision);
        Submit(R,[this]{PendingDeletePlan.Empty();PendingDeleteName.Empty();bList=true;LoadedSelection.Empty();Refresh();});return;
    }
    if(Name==TEXT("delete_plan_prompt")){RequestPlanDeletion(PlanId);return;}
    if(Name==TEXT("delete_card")){if(const auto* Target=PlanActionTargets.Find(Index))RequestPlanDeletion(*Target);return;}
    if(Name==TEXT("open_card") || Name==TEXT("copy_card")){
        if(const auto* Target=PlanActionTargets.Find(Index)){const FString Id=*Target;auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("action"),Name==TEXT("open_card")?TEXT("open_plan"):TEXT("copy_plan"));R->SetStringField(TEXT("plan_id"),Id);R->SetBoolField(TEXT("allow_draft"),true);
            const auto P=Find(GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetPlans(),TEXT("plans"),Id);if(P && !P->GetArrayField(TEXT("mission_ids")).IsEmpty())R->SetStringField(TEXT("mission_id"),P->GetArrayField(TEXT("mission_ids"))[0]->AsString());
            auto Open=[this,R]{bList=false;bCreating=false;Submit(R);};
            if(Name==TEXT("open_card") && Field(P,TEXT("status"))==TEXT("DEPLOYED"))Open();else BeginConfiguration(Name==TEXT("copy_card")?TEXT(""):Id,Open);
        }return;
    }
    if(Name==TEXT("copy_plan")){auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("action"),TEXT("copy_plan"));R->SetStringField(TEXT("plan_id"),PlanId);BeginConfiguration(TEXT(""),[this,R]{Submit(R);});return;}
    if(Name==TEXT("request_map_route_edit")){SaveConfiguration([this]{auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("action"),TEXT("request_map_route_edit"));R->SetStringField(TEXT("plan_id"),PlanId);R->SetStringField(TEXT("mission_id"),MissionId);Submit(R);});return;}
    if(Name==TEXT("add_mission")){SaveConfiguration([this]{const auto P=Find(GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetPlans(),TEXT("plans"),PlanId);auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("action"),TEXT("add_mission"));R->SetStringField(TEXT("plan_id"),PlanId);R->SetStringField(TEXT("name"),FText::Format(T(TEXT("Configuration.NewMissionName")),FText::AsNumber(P?P->GetArrayField(TEXT("mission_ids")).Num()+1:1)).ToString());Submit(R);});return;}
    if(Name==TEXT("confirm_deployment")){ConfirmDeployment();return;}
    if(Name==TEXT("retry_map")){
        auto* S=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
        if(S->IsRoleOnline(TEXT("Map"))){if(!FocusMapWindow())ErrorCode=TEXT("MAP_FOCUS_MANUAL");return;}
        Name=TEXT("request_map_route_edit");
    }
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();if(Name==TEXT("en") || Name==TEXT("zh-Hans")){Sync->SetLanguage(Name.ToString());return;}
    if(Name==TEXT("active_video")){Sync->OpenVideo(Field(Sync->GetContext(),TEXT("active_uav_id")));return;}
    if(Name==TEXT("cancel")){bReview=false;bConfirm=false;return;}if(Name==TEXT("review_page")){bReview=true;return;}
    if(Name==TEXT("deploy_page")){const auto P=Find(Sync->GetPlans(),TEXT("plans"),PlanId);if(P && Reviewed(P)){ConfirmRevision=P->GetNumberField(TEXT("content_revision"));bConfirm=true;}return;}
    auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("action"),(Name==TEXT("unassign") || Name==TEXT("assign_group"))?TEXT("assign"):Name.ToString());R->SetStringField(TEXT("plan_id"),PlanId);R->SetStringField(TEXT("mission_id"),MissionId);
    if(Name==TEXT("copy_plan"))R->SetStringField(TEXT("name"),FText::Format(T(TEXT("Workflow.VersionName")),User(Field(Find(Sync->GetPlans(),TEXT("plans"),PlanId),TEXT("name")))).ToString());
    if(Name==TEXT("create_plan"))R->SetStringField(TEXT("default_mission_name"),T(TEXT("Workflow.DefaultTask")).ToString());
    if(Name==TEXT("create_plan") || Name==TEXT("update_plan")){R->SetStringField(TEXT("name"),PlanName->GetText().ToString());R->SetStringField(TEXT("description"),Description->GetText().ToString());}
    if(Name==TEXT("add_mission") || Name==TEXT("rename_mission"))R->SetStringField(TEXT("name"),MissionName->GetText().ToString());
    if(Name==TEXT("assign_group")){
        TArray<TSharedPtr<FJsonValue>> Members;for(const auto& ID:UavIds)if(AssignmentChecks.Contains(ID) && AssignmentChecks[ID]->IsChecked())Members.Add(MakeShared<FJsonValueString>(ID));
        double Spacing=0;if(!LexTryParseString(Spacing,*SpacingInput->GetText().ToString())){ErrorCode=TEXT("INVALID_SEPARATION");return;}
        R->SetArrayField(TEXT("assigned_uav_ids"),Members);R->SetStringField(TEXT("formation"),TEXT("Line"));R->SetNumberField(TEXT("spacing_m"),Spacing);
    }
    if(Name==TEXT("assign"))R->SetStringField(TEXT("assigned_uav_id"),UAVs->GetSelectedOption());
    if(Name==TEXT("deploy")){if(!bConfirm)return;bConfirm=false;}Submit(R);
}



void USecurityPlanWorkspaceWidget::ConfirmDeployment(){
    if(bPending)return;
    ErrorCode.Empty();ExecutionErrorDetails=FText::GetEmpty();
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    const auto Initial=Sync->GetPlans();
    const auto Intent=DeploymentConfirmation::Capture(Initial,PlanId,Sync->GetInstanceId());
    const auto Check=Intent.Request(TEXT("validate"),Initial);
    if(!Check || Field(Sync->GetContext(),TEXT("active_security_plan_id"))!=Intent.PlanId){ErrorCode=TEXT("CONFIGURATION_CONTEXT_CHANGED");Refresh();return;}
    bPending=true;const auto Weak=TWeakObjectPtr<USecurityPlanWorkspaceWidget>(this);
    auto StillConfirmed=[Weak,Sync,Intent]{
        return Weak.IsValid() && Weak->PlanId==Intent.PlanId
            && Field(Sync->GetContext(),TEXT("active_security_plan_id"))==Intent.PlanId && Intent.Matches(Sync->GetPlans());
    };
    if(!Sync->SubmitPlan(Check.ToSharedRef(),[Weak,Sync,Intent,StillConfirmed](TSharedPtr<FJsonObject> Reply){
        if(!Weak.IsValid())return;Weak->ErrorCode=PlanUI::Error(Reply);Weak->ExecutionErrorDetails=ReservationError(Reply);
        if(Weak->ErrorCode.IsEmpty() && !StillConfirmed())Weak->ErrorCode=TEXT("CONFIGURATION_CONTEXT_CHANGED");
        const auto Acknowledged=Object(Reply,TEXT("state")),P=Find(Acknowledged,TEXT("plans"),Intent.PlanId);
        if(!Weak->ErrorCode.IsEmpty() || Field(P,TEXT("status"))!=TEXT("READY")){Weak->bPending=false;Weak->Refresh();return;}
        // Only this stage's ACK supplies the next expected plan/document
        // versions. A later heartbeat snapshot is checked, never substituted.
        const auto ReviewRequest=Intent.Request(TEXT("review"),Acknowledged);
        if(!ReviewRequest){Weak->bPending=false;Weak->ErrorCode=TEXT("CONFIGURATION_CONTEXT_CHANGED");Weak->Refresh();return;}
        if(!Sync->SubmitPlan(ReviewRequest.ToSharedRef(),[Weak,Intent,StillConfirmed](TSharedPtr<FJsonObject> ReviewedReply){
            if(!Weak.IsValid())return;Weak->ErrorCode=PlanUI::Error(ReviewedReply);Weak->ExecutionErrorDetails=ReservationError(ReviewedReply);Weak->bPending=false;
            if(Weak->ErrorCode.IsEmpty() && !StillConfirmed())Weak->ErrorCode=TEXT("CONFIGURATION_CONTEXT_CHANGED");
            if(!Weak->ErrorCode.IsEmpty()){Weak->Refresh();return;}
            const auto Deploy=Intent.Request(TEXT("deploy"),Object(ReviewedReply,TEXT("state")));
            if(!Deploy){Weak->ErrorCode=TEXT("CONFIGURATION_CONTEXT_CHANGED");Weak->Refresh();return;}
            Weak->Submit(Deploy.ToSharedRef());
        })){Weak->bPending=false;Weak->ErrorCode=TEXT("SYNC_OFFLINE");Weak->Refresh();}
    })){bPending=false;ErrorCode=TEXT("SYNC_OFFLINE");Refresh();}
}

void UPlanUAVOptionWidget::NativeOnInitialized(){Super::NativeOnInitialized();Caption=WidgetTree->ConstructWidget<UTextBlock>();CommandTheme::Text(Caption,16,CommandTheme::PrimaryText);WidgetTree->RootWidget=Caption;}
void UPlanUAVOptionWidget::SetCaption(const FString& Item){Caption->SetText(User(Item));}
UWidget* USecurityPlanWorkspaceWidget::UAVOption(FString Item){auto* Option=CreateWidget<UPlanUAVOptionWidget>(GetOwningPlayer());const auto E=UAVReservation(GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetPlans(),PlanId,Item);
    Option->SetCaption(E?FText::Format(T(TEXT("Reservation.InUse")),User(Item),User(Field(E,TEXT("plan_name")))).ToString():Item);Option->SetIsEnabled(!E);return Option;}
void USecurityPlanWorkspaceWidget::UAVSelected(FString,ESelectInfo::Type Type){if(!bRefreshing && Type!=ESelectInfo::Direct && !bPending)Action(TEXT("assign"),0);}

TSharedPtr<FJsonObject> USecurityPlanWorkspaceWidget::SelectedExecution(const TSharedPtr<FJsonObject>& State) const {
    return MissionExecution(State,PlanId,MissionId);
}

void USecurityPlanWorkspaceWidget::RefreshExecution(const TSharedPtr<FJsonObject>& State,const TSharedPtr<FJsonObject>& P){
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    auto E=SelectedExecution(State);const auto LatestId=Field(E,TEXT("execution_id"));
    if(!LatestId.IsEmpty() && LatestId!=ExecutionId){ExecutionId=LatestId;bExecutionView=ExecutionUI::Active(E);ExecutionIntent.Empty();}
    auto Show=[&](const TCHAR* K,bool Yes){Actions[K]->SetVisibility(Yes?ESlateVisibility::Visible:ESlateVisibility::Collapsed);};
    const bool Deployed=Field(P,TEXT("status"))==TEXT("DEPLOYED");
    Show(TEXT("execution_start_prompt"),Deployed && !ExecutionUI::Active(E));Show(TEXT("execution_open"),E.IsValid());
    if(!Deployed){bExecutionView=false;ExecutionIntent.Empty();}
    const bool Visible=Deployed && bExecutionView && !bList && !bCreating;
    ExecutionPage->SetVisibility(Visible?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    if(Visible)ReviewPage->SetVisibility(ESlateVisibility::Collapsed);
    const bool Real=Field(State,TEXT("execution_adapter"))==TEXT("Real");
    ExecutionMode->SetText(T(Real?TEXT("Execution.Real"):TEXT("Execution.Simulation")));
    const auto StateName=Field(E,TEXT("state"));
    const bool Scheduled=StateName==TEXT("SCHEDULED");
    if(Scheduled && ExecutionIntent.IsEmpty())RefreshScheduleInput(E);
    Show(TEXT("execution_start_now"),Scheduled);Show(TEXT("execution_reschedule"),Scheduled);Show(TEXT("execution_cancel_schedule"),Scheduled);
    ScheduleEnabled->SetVisibility(ExecutionIntent==TEXT("start")?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    ScheduleInput->SetVisibility((ExecutionIntent==TEXT("start") && ScheduleEnabled->IsChecked()) || Scheduled?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    ExecutionSummary->SetText(ExecutionIntent==TEXT("start")?T(TEXT("Execution.StartConfirm")):ExecutionUI::Summary(E));ExecutionProgress->SetVisibility(ExecutionIntent.IsEmpty()?ESlateVisibility::Visible:ESlateVisibility::Collapsed);ExecutionProgress->SetPercent(E?E->GetNumberField(TEXT("progress")):0);
    const bool Prompt=!ExecutionIntent.IsEmpty();
    Show(TEXT("execution_pause"),!Prompt && (StateName==TEXT("EXECUTING") || StateName==TEXT("RETURNING")));Show(TEXT("execution_resume"),!Prompt && StateName==TEXT("PAUSED"));
    Show(TEXT("execution_return_prompt"),!Prompt && StateName==TEXT("EXECUTING"));Show(TEXT("execution_abort_prompt"),!Prompt && (StateName==TEXT("EXECUTING") || StateName==TEXT("PAUSED") || StateName==TEXT("RETURNING")));
    Show(TEXT("execution_confirm"),Prompt);Show(TEXT("execution_cancel"),Prompt);Show(TEXT("execution_details"),!Prompt);Show(TEXT("execution_video"),E.IsValid() && !Prompt);
    if(auto* Caption=Cast<UTextBlock>(Actions[TEXT("execution_confirm")]->GetContent()))Caption->SetText(T(ExecutionIntent==TEXT("start")?TEXT("Execution.ConfirmStart"):ExecutionIntent==TEXT("return")?TEXT("Execution.ConfirmReturn"):TEXT("Execution.ConfirmAbort")));
    TArray<FText> Lines;
    if(ExecutionIntent==TEXT("start")){
        const auto M=Find(State,TEXT("missions"),MissionId),R=Find(State,TEXT("paths"),Field(M,TEXT("route_id")));
        const TArray<TSharedPtr<FJsonValue>>* Points=nullptr;const int Count=R && R->TryGetArrayField(TEXT("waypoints"),Points)?Points->Num():0;
        Lines.Add(T(TEXT("Execution.StartConfirm")));Lines.Add(User(Field(P,TEXT("name"))));Lines.Add(User(Field(M,TEXT("name"))));Lines.Add(User(AssignedUAVs(M)));
        Lines.Add(FText::Format(T(TEXT("Map.Count")),FText::AsNumber(Count)));
        const auto Config=Object(State,TEXT("mock_execution"));
        auto From=ExecutionUI::MockPosition(State,Field(M,TEXT("assigned_uav_id")));
        int32 FromId=0;LexTryParseString(FromId,*Field(M,TEXT("assigned_uav_id")).Mid(4));FDroneTelemetrySnapshot FromTelemetry;
        if(GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetTelemetry(FromId,FromTelemetry) && FromTelemetry.TelemetrySource==TEXT("received")){
            From.Reset();
            if(FromTelemetry.bGpsFix && FMath::IsFinite(FromTelemetry.GpsLatitude) && FMath::IsFinite(FromTelemetry.GpsLongitude) && FMath::IsFinite(FromTelemetry.GpsAltitude)){
                From=MakeShared<FJsonObject>();From->SetNumberField(TEXT("latitude"),FromTelemetry.GpsLatitude);
                From->SetNumberField(TEXT("longitude"),FromTelemetry.GpsLongitude);From->SetNumberField(TEXT("altitude"),FromTelemetry.GpsAltitude);}}
        double Distance=0,Seconds=0;if(From && Points && Config)for(const auto& V:*Points){const auto Point=V->AsObject();const double Y=(Point->GetNumberField(TEXT("latitude"))-From->GetNumberField(TEXT("latitude")))*PI/180.*6371000.;const double X=(Point->GetNumberField(TEXT("longitude"))-From->GetNumberField(TEXT("longitude")))*PI/180.*6371000.*FMath::Cos((Point->GetNumberField(TEXT("latitude"))+From->GetNumberField(TEXT("latitude")))*.5*PI/180.);const double Z=Point->GetNumberField(TEXT("altitude"))-From->GetNumberField(TEXT("altitude"));const double D=FMath::Sqrt(X*X+Y*Y+Z*Z),Speed=Point->GetNumberField(TEXT("segmentSpeed"));Distance+=D;Seconds+=D/(Speed>0?Speed:Config->GetNumberField(TEXT("default_speed_mps")))+Point->GetNumberField(TEXT("waitTime"));From=Point;}
        bool Closed=false;if(R)R->TryGetBoolField(TEXT("bClosedLoop"),Closed);
        if(Closed && From && Points && Points->Num()>2 && Config){const auto Point=(*Points)[0]->AsObject();const double Y=(Point->GetNumberField(TEXT("latitude"))-From->GetNumberField(TEXT("latitude")))*PI/180.*6371000.,X=(Point->GetNumberField(TEXT("longitude"))-From->GetNumberField(TEXT("longitude")))*PI/180.*6371000.*FMath::Cos((Point->GetNumberField(TEXT("latitude"))+From->GetNumberField(TEXT("latitude")))*.5*PI/180.),Z=Point->GetNumberField(TEXT("altitude"))-From->GetNumberField(TEXT("altitude"));const double D=FMath::Sqrt(X*X+Y*Y+Z*Z),Speed=From->GetNumberField(TEXT("segmentSpeed"));Distance+=D;Seconds+=D/(Speed>0?Speed:Config->GetNumberField(TEXT("default_speed_mps")));}
        Lines.Add(Real?T(TEXT("Execution.RealEstimate")):FText::Format(T(TEXT("Execution.Estimate")),FText::AsNumber(FMath::RoundToInt(Distance)),FText::AsNumber(FMath::RoundToInt(Seconds))));
        const auto StartBlockers=ExecutionStartBlockers(State,M,UavIds);
        if(StartBlockers.IsEmpty())Lines.Add(T(TEXT("Execution.StartMembersReady")));else Lines.Append(StartBlockers);
        Lines.Add(T(Real?TEXT("Execution.RealCheck"):TEXT("Execution.MockCheck")));
        Lines.Add(T(TEXT("Execution.DeploymentValid")));
        Actions[TEXT("execution_confirm")]->SetIsEnabled(!bPending && Sync->IsReady() && Sync->IsRoleOnline(TEXT("Map")) && Count>=2 && StartBlockers.IsEmpty());
        Lines.Add(FText::Format(T(TEXT("Workflow.Readiness")),T(Sync->IsReady()?TEXT("Stage1.Online"):TEXT("Stage1.Offline")),T(Sync->IsRoleOnline(TEXT("Map"))?TEXT("Stage1.Online"):TEXT("Stage1.Offline"))));
    }else if(Prompt)Lines.Add(ProductText::Get((Real?TEXT("Execution.Real."):TEXT("Execution."))+ExecutionIntent+TEXT("Confirm")));
    else if(E){
        if(StateName==TEXT("SCHEDULED")){const double Seconds=FMath::Max(0.,E->GetNumberField(TEXT("scheduled_start_epoch"))-FDateTime::UtcNow().ToUnixTimestamp());
            Lines.Add(FText::Format(T(TEXT("Schedule.Countdown")),User(FTimespan::FromSeconds(Seconds).ToString(TEXT("%h:%m:%s")))));}
        Lines.Add(FText::Format(T(TEXT("Execution.Duration")),FText::AsNumber(FMath::RoundToInt(E->GetNumberField(TEXT("elapsed_seconds"))))));
        if(!Field(E,TEXT("completion_reason")).IsEmpty())Lines.Add(ProductText::Get(TEXT("Execution.")+Field(E,TEXT("completion_reason"))));
        if(!Field(E,TEXT("failure_reason")).IsEmpty())Lines.Add(ProductText::Get(TEXT("Errors.")+Field(E,TEXT("failure_reason"))));
    }
    if(bPending)Lines.Add(T(TEXT("Common.Waiting")));ExecutionPrompt->SetText(FText::Join(User(TEXT("\n")),Lines));
    Lines.Empty();const auto All=Object(State,TEXT("executions"));
    if(All)for(const auto& Entry:All->Values){const auto Item=Entry.Value->AsObject();if(Field(Item,TEXT("plan_id"))==PlanId && Field(Item,TEXT("mission_id"))==MissionId)Lines.Add(FText::Format(User(TEXT("{0}  {1}  {2}")),User(Field(Item,TEXT("execution_id"))),ProductText::Get(TEXT("Execution.")+Field(Item,TEXT("state"))),FText::AsDateTime(FDateTime::FromUnixTimestamp(Item->GetNumberField(TEXT("created_at"))))));}
    if(Lines.IsEmpty())Lines.Add(T(TEXT("Common.None")));ExecutionHistory->SetText(FText::Join(User(TEXT("\n")),Lines));
}
TArray<FText> USecurityPlanWorkspaceWidget::ExecutionStartBlockers(const TSharedPtr<FJsonObject>& State,const TSharedPtr<FJsonObject>& Mission,const TArray<FString>& AvailableUavs){
    TArray<FString> Members;const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;
    if(Mission && Mission->TryGetArrayField(TEXT("assigned_uav_ids"),Values)){for(const auto& V:*Values)Members.AddUnique(V->AsString());}
    else if(!Field(Mission,TEXT("assigned_uav_id")).IsEmpty())Members.Add(Field(Mission,TEXT("assigned_uav_id")));
    TArray<FText> Blockers;if(Members.IsEmpty()){Blockers.Add(T(TEXT("Errors.NO_UAV_ASSIGNED")));return Blockers;}
    const auto Config=Object(State,TEXT("mock_execution")),Executions=Object(State,TEXT("executions"));
    for(const auto& Uav:Members){
        if(!AvailableUavs.Contains(Uav) || !Find(Config,TEXT("uavs"),Uav))Blockers.Add(FText::Format(T(TEXT("Execution.StartMemberUnavailable")),User(Uav)));
        if(Executions)for(const auto& Item:Executions->Values){const auto E=Item.Value->AsObject();
            if(Field(E,TEXT("uav_id"))!=Uav || !ExecutionUI::Active(E))continue;
            const auto PlanName=Field(E,TEXT("plan_name")),MissionName=Field(E,TEXT("mission_name"));
            const FText Owner=PlanName.IsEmpty() && MissionName.IsEmpty()?T(TEXT("Group.Owner")):User(PlanName.IsEmpty()?MissionName:MissionName.IsEmpty()?PlanName:PlanName+TEXT(" / ")+MissionName);
            Blockers.Add(FText::Format(T(TEXT("Execution.StartMemberBusy")),User(Uav),Owner,ProductText::Get(TEXT("Execution.")+Field(E,TEXT("state")))));
        }
    }return Blockers;
}
void USecurityPlanWorkspaceWidget::ResetStartSchedule(const FDateTime& LocalNow){
    ScheduleExecutionId.Empty();ScheduleAcceptedAt.Empty();
    ScheduleLoadedText=(LocalNow+FTimespan::FromMinutes(2)).ToString(TEXT("%Y-%m-%dT%H:%M:%S"));ScheduleInput->SetText(User(ScheduleLoadedText));
}
void USecurityPlanWorkspaceWidget::RefreshScheduleInput(const TSharedPtr<FJsonObject>& Execution){
    if(Field(Execution,TEXT("state"))!=TEXT("SCHEDULED"))return;
    const auto Id=Field(Execution,TEXT("execution_id")),At=Field(Execution,TEXT("scheduled_start_at"));
    const bool NewExecution=Id!=ScheduleExecutionId;
    if(!NewExecution && (At==ScheduleAcceptedAt || ScheduleInput->GetText().ToString()!=ScheduleLoadedText))return;
    FDateTime UTC;if(!FDateTime::ParseIso8601(*At,UTC))return;
    FDateTime Local=UTC;
#if PLATFORM_WINDOWS
    // Use the scheduled date's timezone/DST rules, mirroring the submit conversion.
    SYSTEMTIME Universal{};Universal.wYear=UTC.GetYear();Universal.wMonth=UTC.GetMonth();Universal.wDay=UTC.GetDay();Universal.wHour=UTC.GetHour();Universal.wMinute=UTC.GetMinute();Universal.wSecond=UTC.GetSecond();SYSTEMTIME Civil{};
    if(!::SystemTimeToTzSpecificLocalTime(nullptr,&Universal,&Civil))return;
    Local=FDateTime(Civil.wYear,Civil.wMonth,Civil.wDay,Civil.wHour,Civil.wMinute,Civil.wSecond);
#else
    Local=UTC+(FDateTime::Now()-FDateTime::UtcNow());
#endif
    ScheduleExecutionId=Id;ScheduleAcceptedAt=At;ScheduleLoadedText=Local.ToString(TEXT("%Y-%m-%dT%H:%M:%S"));ScheduleInput->SetText(User(ScheduleLoadedText));
}

void USecurityPlanWorkspaceWidget::ExecutionAction(FName Name){
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();const auto E=SelectedExecution(Sync->GetPlans());
    if(bPending)return;
    if(Name==TEXT("execution_start_prompt")){bExecutionView=true;ExecutionIntent=TEXT("start");StartRequestId=FGuid::NewGuid().ToString();ResetStartSchedule(FDateTime::Now());Refresh();return;}
    if(Name==TEXT("execution_open")){bExecutionView=true;ExecutionIntent.Empty();Refresh();return;}
    if(Name==TEXT("execution_details")){bExecutionView=false;Refresh();return;}
    if(Name==TEXT("execution_cancel")){ExecutionIntent.Empty();bExecutionView=ExecutionUI::Active(E);Refresh();return;}
    if(Name==TEXT("execution_video")){if(E)Sync->OpenVideo(Field(E,TEXT("uav_id")));return;}
    if(Name==TEXT("execution_return_prompt") || Name==TEXT("execution_abort_prompt")){ExecutionIntent=Name==TEXT("execution_return_prompt")?TEXT("return"):TEXT("abort");Refresh();return;}
    FString Action=Name==TEXT("execution_cancel_schedule")?TEXT("execution_cancel"):Name.ToString();if(Name==TEXT("execution_confirm")){if(ExecutionIntent.IsEmpty())return;Action=TEXT("execution_")+ExecutionIntent;}
    auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("action"),Action);R->SetStringField(TEXT("plan_id"),PlanId);R->SetStringField(TEXT("mission_id"),MissionId);
    R->SetStringField(TEXT("request_id"),Action==TEXT("execution_start")?StartRequestId:FGuid::NewGuid().ToString());
    if(E){R->SetStringField(TEXT("execution_id"),Field(E,TEXT("execution_id")));R->SetNumberField(TEXT("control_version"),E->GetNumberField(TEXT("control_version")));}
    if((Action==TEXT("execution_start") && ScheduleEnabled->IsChecked()) || Action==TEXT("execution_reschedule")){
        FDateTime Local;if(!FDateTime::ParseIso8601(*(ScheduleInput->GetText().ToString()+TEXT("Z")),Local)){ErrorCode=TEXT("INVALID_SCHEDULE");return;}
        // Windows converts the selected civil date using that date's timezone/DST rules.
        FDateTime UTC=Local;
#if PLATFORM_WINDOWS
        SYSTEMTIME Civil{};Civil.wYear=Local.GetYear();Civil.wMonth=Local.GetMonth();Civil.wDay=Local.GetDay();Civil.wHour=Local.GetHour();Civil.wMinute=Local.GetMinute();Civil.wSecond=Local.GetSecond();SYSTEMTIME Universal{};
        if(!::TzSpecificLocalTimeToSystemTime(nullptr,&Civil,&Universal)){ErrorCode=TEXT("INVALID_SCHEDULE");return;}
        UTC=FDateTime(Universal.wYear,Universal.wMonth,Universal.wDay,Universal.wHour,Universal.wMinute,Universal.wSecond);
#else
        UTC=Local-(FDateTime::Now()-FDateTime::UtcNow());
#endif
        R->SetStringField(TEXT("scheduled_start_at"),UTC.ToString(TEXT("%Y-%m-%dT%H:%M:%SZ")));
    }
    bPending=true;ErrorCode.Empty();ExecutionErrorDetails=FText::GetEmpty();const auto Weak=TWeakObjectPtr<USecurityPlanWorkspaceWidget>(this);
    if(!Sync->SubmitPlan(R,[Weak](TSharedPtr<FJsonObject> Reply){if(!Weak.IsValid())return;Weak->bPending=false;Weak->ErrorCode=PlanUI::Error(Reply);Weak->ExecutionErrorDetails=ReservationError(Reply);const auto Params=Object(Reply,TEXT("params"));
        if(Params && Params->HasField(TEXT("minimum_separation_m")))Weak->ExecutionErrorDetails=FText::Format(T(TEXT("Group.Conflict")),User(Field(Params,TEXT("uav_a"))),User(Field(Params,TEXT("uav_b"))),FText::AsNumber(Params->GetNumberField(TEXT("minimum_separation_m"))),FText::AsNumber(Params->GetNumberField(TEXT("required_separation_m"))));
        if(Weak->ErrorCode.IsEmpty()){Weak->ExecutionIntent.Empty();Weak->bExecutionView=true;}Weak->Refresh();})){bPending=false;ErrorCode=TEXT("SYNC_OFFLINE");}Refresh();
}
