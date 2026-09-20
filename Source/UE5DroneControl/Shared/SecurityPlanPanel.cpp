#include "Shared/SecurityPlanPanel.h"
#include "Shared/OperationalContextSubsystem.h"
#include "Command/CommandScreenManager.h"
#include "Command/CommandMapInteractionService.h"
#include "Command/CommandActionButton.h"
#include "Command/CommandTheme.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "DroneOps/Core/ICoordinateService.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Engine/GameInstance.h"

namespace {
FString Field(const TSharedPtr<FJsonObject>& O,const TCHAR* Key) {FString S;if(O)O->TryGetStringField(Key,S);return S;}
TSharedPtr<FJsonObject> FindObject(const TSharedPtr<FJsonObject>& Root,const TCHAR* Collection,const FString& Id) {
    const TSharedPtr<FJsonObject>* Items; const TSharedPtr<FJsonObject>* Item;
    return Root && Root->TryGetObjectField(Collection,Items) && (*Items)->TryGetObjectField(Id,Item)?*Item:nullptr;
}
}
void USecurityPlanPanel::NativeOnInitialized() {
    Super::NativeOnInitialized();bMap=UCommandScreenManager::ResolveClientRole()==EDroneClientRole::Map;
    auto* Border=WidgetTree->ConstructWidget<UBorder>();CommandTheme::Panel(Border);Border->SetPadding(FMargin(10));WidgetTree->RootWidget=Border;
    auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>();Border->SetContent(Scroll);
    Content=WidgetTree->ConstructWidget<UVerticalBox>();Scroll->AddChild(Content);
    auto Label=[&](const TCHAR* Value){auto* W=WidgetTree->ConstructWidget<UTextBlock>();CommandTheme::Text(W,14);W->SetText(FText::FromString(Value));W->SetAutoWrapText(true);Content->AddChild(W);return W;};
    Label(TEXT("SECURITY PLAN"));
    auto Combo=[&](UVerticalBox* Parent){auto* Box=WidgetTree->ConstructWidget<UComboBoxString>();auto Style=Box->GetItemStyle();Style.SetEvenRowBackgroundBrush(FSlateRoundedBoxBrush(FLinearColor(.7f,.7f,.7f),0.f));Style.SetOddRowBackgroundBrush(FSlateRoundedBoxBrush(FLinearColor(.7f,.7f,.7f),0.f));Style.SetTextColor(FLinearColor::Black);Box->SetItemStyle(Style);auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetMinDesiredHeight(30);Size->SetContent(Box);Parent->AddChild(Size);return Box;};
    Label(TEXT("PLAN"));Plans=Combo(Content);Plans->OnSelectionChanged.AddDynamic(this,&USecurityPlanPanel::PlanSelected);
    Label(TEXT("MISSION"));Missions=Combo(Content);Missions->OnSelectionChanged.AddDynamic(this,&USecurityPlanPanel::MissionSelected);
    Summary=Label(TEXT("No active plan"));
    Editing=WidgetTree->ConstructWidget<UVerticalBox>();Content->AddChild(Editing);
    auto Entry=[&](const TCHAR* Hint){auto* W=WidgetTree->ConstructWidget<UEditableTextBox>();W->SetHintText(FText::FromString(Hint));CommandTheme::Input(W);W->WidgetStyle.TextStyle.SetFont(FCoreStyle::GetDefaultFontStyle("Regular",14));W->SetWidgetStyle(W->WidgetStyle);Editing->AddChild(W);return W;};
    PlanName=Entry(TEXT("PLAN NAME"));Description=Entry(TEXT("DESCRIPTION"));MissionName=Entry(TEXT("MISSION NAME"));
    auto* UavLabel=WidgetTree->ConstructWidget<UTextBlock>();CommandTheme::Text(UavLabel,14);UavLabel->SetText(FText::FromString(TEXT("ASSIGNMENT UAV")));Editing->AddChild(UavLabel);UAVs=Combo(Editing);
    auto Buttons=[&](UVerticalBox* Parent,std::initializer_list<TPair<const TCHAR*,const TCHAR*>> Items){
        auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();Parent->AddChild(Row);
        for(auto Item:Items){auto* B=WidgetTree->ConstructWidget<UCommandActionButton>();B->Configure(Item.Key);B->OnAction.AddDynamic(this,&USecurityPlanPanel::Action);
            CommandTheme::Button(B,false,true);auto* T=WidgetTree->ConstructWidget<UTextBlock>();CommandTheme::Text(T,12);T->SetText(FText::FromString(Item.Value));B->SetContent(T);Row->AddChild(B);}
    };
    Buttons(Editing,{{TEXT("create_plan"),TEXT("NEW PLAN")},{TEXT("update_plan"),TEXT("SAVE PLAN")}});
    Buttons(Editing,{{TEXT("add_mission"),TEXT("ADD MISSION")},{TEXT("rename_mission"),TEXT("RENAME")},{TEXT("delete_mission"),TEXT("DELETE")}});
    Buttons(Editing,{{TEXT("assign"),TEXT("ASSIGN UAV")},{TEXT("unassign"),TEXT("UNASSIGN")}});
    Buttons(Editing,{{TEXT("edit_route"),TEXT("ADD / MOVE WP")},{TEXT("delete_wp"),TEXT("DELETE WP")}});
    Buttons(Editing,{{TEXT("clear_route"),TEXT("CLEAR ROUTE")},{TEXT("save_route"),TEXT("SAVE ROUTE")}});
    Buttons(Editing,{{TEXT("validate"),TEXT("VALIDATE")},{TEXT("exit"),TEXT("EXIT EDITOR")}});
    if(!bMap)Buttons(Content,{{TEXT("validate"),TEXT("VALIDATE")},{TEXT("deploy"),TEXT("DEPLOY PLAN")}});
    if(!bMap)Buttons(Content,{{TEXT("confirm"),TEXT("CONFIRM DEPLOY")},{TEXT("cancel"),TEXT("CANCEL")}});
    Result=Label(TEXT("Deployment records business state only."));
    Editing->SetVisibility(ESlateVisibility::Collapsed);
    Refresh();
}
void USecurityPlanPanel::SetEditorEnabled(bool Enabled) {
    bEditor=Enabled && bMap;Editing->SetVisibility(bEditor?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    SetVisibility(bMap && !bEditor?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
    if(auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer()))PC->SetMissionPathEditing(false);
    LoadRoute(true);
}
TSharedPtr<FJsonObject> USecurityPlanPanel::CurrentMission() const {
    return FindObject(GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetPlans(),TEXT("missions"),SelectedMission);
}
int32 USecurityPlanPanel::GetAssignedUAV() const {int32 Id=0;const auto Uav=Field(CurrentMission(),TEXT("assigned_uav_id"));if(Uav.StartsWith(TEXT("UAV-")))LexTryParseString(Id,*Uav.Mid(4));return Id;}
void USecurityPlanPanel::Refresh() {
    if(!Summary)return;
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();auto State=Sync->GetPlans();
    if(State){const int64 Version=static_cast<int64>(State->GetNumberField(TEXT("version")));if(Version!=ViewedVersion){if(ViewedVersion>=0 && !bPending){bConfirm=false;Result->SetText(FText::FromString(TEXT("Backend state updated")));}ViewedVersion=Version;}}
    SelectedPlan=Field(Sync->GetContext(),TEXT("active_security_plan_id"));SelectedMission=Field(Sync->GetContext(),TEXT("active_mission_id"));
    auto Plan=FindObject(State,TEXT("plans"),SelectedPlan);auto Mission=CurrentMission();
    bRefreshing=true;
    auto Fill=[&](UComboBoxString* Box,TArray<FString>& Old,TArray<FString>& Ids,const TArray<FString>& Labels,const TArray<FString>& NewIds,const FString& Selected){
        if(Old!=Labels){Box->ClearOptions();for(const auto& L:Labels)Box->AddOption(L);Old=Labels;}
        Ids=NewIds;int32 Index=Ids.IndexOfByKey(Selected);if(Index!=INDEX_NONE)Box->SetSelectedOption(Labels[Index]);else Box->ClearSelection();
    };
    TArray<FString> Labels,Ids;const TSharedPtr<FJsonObject>* All;
    if(State && State->TryGetObjectField(TEXT("plans"),All))for(const auto& Pair:(*All)->Values){Ids.Add(FString(Pair.Key.ToView()));Labels.Add(Field(Pair.Value->AsObject(),TEXT("name"))+TEXT(" / ")+Field(Pair.Value->AsObject(),TEXT("status")));}
    Fill(Plans,PlanLabels,PlanIds,Labels,Ids,SelectedPlan);Labels.Empty();Ids.Empty();
    const TArray<TSharedPtr<FJsonValue>>* List;
    if(Plan && Plan->TryGetArrayField(TEXT("mission_ids"),List))for(const auto& Id:*List){auto M=FindObject(State,TEXT("missions"),Id->AsString());Ids.Add(Id->AsString());Labels.Add(Field(M,TEXT("name"))+TEXT(" / ")+Field(M,TEXT("assigned_uav_id"))+TEXT(" / ")+Field(M,TEXT("status")));}
    Fill(Missions,MissionLabels,MissionIds,Labels,Ids,SelectedMission);
    Labels.Empty();Ids.Empty();for(const auto& D:GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetFriendlyDroneDescriptors()) {auto Id=FString::Printf(TEXT("UAV-%02d"),D.DroneId);Ids.Add(Id);Labels.Add(Id+TEXT(" / ")+D.Name);}
    const FString Previous=UAVs->GetSelectedOption();
    if(UavLabels!=Labels){UAVs->ClearOptions();for(const auto& L:Labels)UAVs->AddOption(L);UavLabels=Labels;UavIds=Ids;if(Labels.Contains(Previous))UAVs->SetSelectedOption(Previous);}
    bRefreshing=false;
    FString Text=Plan?Field(Plan,TEXT("name"))+TEXT("\nSTATUS: ")+Field(Plan,TEXT("status")):TEXT("No active plan");
    const TSharedPtr<FJsonObject>* V;
    if(Plan && Plan->TryGetObjectField(TEXT("validation"),V)) {
        Text+=FString::Printf(TEXT("\nMISSIONS %.0f  |  UAV ASSIGNED %.0f / %.0f\nROUTES READY %.0f / %.0f"),(*V)->GetNumberField(TEXT("mission_count")),(*V)->GetNumberField(TEXT("assigned_count")),(*V)->GetNumberField(TEXT("mission_count")),(*V)->GetNumberField(TEXT("routes_ready")),(*V)->GetNumberField(TEXT("mission_count")));
        Text+=Field(Plan,TEXT("status"))==TEXT("READY")?TEXT("\nREADY TO DEPLOY"):Field(Plan,TEXT("status"))==TEXT("DEPLOYED")?TEXT("\nDEPLOYED"):TEXT("\nNOT READY");
        if((*V)->TryGetArrayField(TEXT("issues"),List))for(const auto& I:*List){auto Issue=I->AsObject();auto M=FindObject(State,TEXT("missions"),Field(Issue,TEXT("mission_id")));Text+=TEXT("\n")+Field(M,TEXT("name"))+TEXT(": ")+Field(Issue,TEXT("code"));}
    }
    if(Mission)Text+=TEXT("\nACTIVE MISSION: ")+Field(Mission,TEXT("name"))+TEXT("\nASSIGNED UAV: ")+Field(Mission,TEXT("assigned_uav_id"));
    Summary->SetText(FText::FromString(Text));
    FString Selection=SelectedPlan+TEXT("/")+SelectedMission;
    // Context selection can arrive before its domain snapshot after reconnect.
    // Only consume the selection once both referenced objects are available.
    if(Selection!=LastSelection && (SelectedPlan.IsEmpty() || Plan.IsValid()) && (SelectedMission.IsEmpty() || Mission.IsValid())){LastSelection=Selection;bConfirm=false;PlanName->SetText(FText::FromString(Field(Plan,TEXT("name"))));Description->SetText(FText::FromString(Field(Plan,TEXT("description"))));MissionName->SetText(FText::FromString(Field(Mission,TEXT("name"))));
        int32 I=UavIds.IndexOfByKey(Field(Mission,TEXT("assigned_uav_id")));if(I!=INDEX_NONE)UAVs->SetSelectedOption(UavLabels[I]);}
    LoadRoute();
}
void USecurityPlanPanel::Submit(const TSharedRef<FJsonObject>& Request) {
    if(bPending)return;bPending=true;Result->SetText(FText::FromString(TEXT("Waiting for Backend...")));
    const auto Weak=TWeakObjectPtr<USecurityPlanPanel>(this);const bool Deploy=Field(Request,TEXT("action"))==TEXT("deploy");
    if(!GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->SubmitPlan(Request,[Weak,Deploy](TSharedPtr<FJsonObject> Reply){
        if(!Weak.IsValid())return;auto* Self=Weak.Get();Self->bPending=false;
        const bool Error=!Reply || Reply->HasField(TEXT("code")) || Reply->HasField(TEXT("error"));
        FString Message=Reply?Field(Reply,TEXT("message")):TEXT("Backend unavailable");
        if(Message.IsEmpty() && Reply)Message=Field(Reply,TEXT("error"));
        Self->Result->SetText(FText::FromString(Error?(Deploy?TEXT("DEPLOY FAILED: "):TEXT("REQUEST FAILED: "))+Message:TEXT("Backend confirmed")));Self->Refresh();
    })) {bPending=false;Result->SetText(FText::FromString(TEXT("Backend not connected")));}
}
void USecurityPlanPanel::PlanSelected(FString Item,ESelectInfo::Type) {if(bRefreshing)return;int32 I=PlanLabels.IndexOfByKey(Item);if(I==INDEX_NONE)return;
    auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("action"),TEXT("select"));R->SetStringField(TEXT("plan_id"),PlanIds[I]);Submit(R);}
void USecurityPlanPanel::MissionSelected(FString Item,ESelectInfo::Type) {if(bRefreshing)return;int32 I=MissionLabels.IndexOfByKey(Item);if(I==INDEX_NONE)return;
    auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("action"),TEXT("select"));R->SetStringField(TEXT("plan_id"),SelectedPlan);R->SetStringField(TEXT("mission_id"),MissionIds[I]);Submit(R);}
void USecurityPlanPanel::LoadRoute(bool Force) {
    if(!bMap)return;auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer());if(!PC)return;
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();auto Mission=CurrentMission();
    if(!Mission){if(PC->IsMissionPathMode())PC->ClearEditingPaths();LoadedRoute.Empty();return;}
    auto Path=FindObject(Sync->GetPlans(),TEXT("paths"),Field(Mission,TEXT("route_id")));
    const FString Key=SelectedMission+TEXT("/")+Field(Mission,TEXT("route_id"));
    // Keep an unsaved local editing draft until explicit save/reload/selection change.
    const int64 Version=Sync->GetPlans()?static_cast<int64>(Sync->GetPlans()->GetNumberField(TEXT("version"))):-1;
    const bool Deployed=Field(FindObject(Sync->GetPlans(),TEXT("plans"),SelectedPlan),TEXT("status"))==TEXT("DEPLOYED");
    if(!Force && LoadedRoute==Key && ((PC->IsPathEditMode() && !Deployed) || LoadedVersion==Version))return;
    LoadedVersion=Version;
    LoadedRoute=Key;
    auto* Coordinate=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetCoordinateService().GetObject();
    if(!Coordinate || !ICoordinateService::Execute_IsCoordinateSystemReady(Coordinate)){LoadedRoute.Empty();return;}
    FDronePathSaveData Data;int32 Drone=0;FString Uav=Field(Mission,TEXT("assigned_uav_id"));LexTryParseString(Drone,*Uav.Mid(4));Data.PathId=Drone>0?Drone:1;
    const TArray<TSharedPtr<FJsonValue>>* Points;
    if(Path && Path->TryGetArrayField(TEXT("waypoints"),Points)){Path->TryGetBoolField(TEXT("bClosedLoop"),Data.bClosedLoop);for(const auto& P:*Points){auto O=P->AsObject();FDroneWaypointSaveData W;
        W.Location=ICoordinateService::Execute_GeographicToWorld(Coordinate,O->GetNumberField(TEXT("latitude")),O->GetNumberField(TEXT("longitude")),O->GetNumberField(TEXT("altitude")));
        W.SegmentSpeed=O->GetNumberField(TEXT("segmentSpeed"));W.WaitTime=O->GetNumberField(TEXT("waitTime"));Data.Waypoints.Add(W);}}
    PC->LoadMissionPath(Data,false);
    if(!Data.Waypoints.IsEmpty() && PC->GetCommandScreenManager())PC->GetCommandScreenManager()->GetMapService()->FocusLocation(Data.Waypoints[0].Location);
}
void USecurityPlanPanel::Action(FName Name,int32) {
    if(bPending)return;auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer());auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    if(Name==TEXT("exit")){SetEditorEnabled(false);return;}
    if(Name==TEXT("cancel")){bConfirm=false;Result->SetText(FText::FromString(TEXT("Deployment cancelled")));return;}
    auto Plan=FindObject(Sync->GetPlans(),TEXT("plans"),SelectedPlan);
    if(Name==TEXT("deploy")) {bConfirm=Plan && Field(Plan,TEXT("status"))==TEXT("READY");Result->SetText(FText::FromString(bConfirm?TEXT("DEPLOY SECURITY PLAN?\n")+Field(Plan,TEXT("name"))+TEXT("\nReview mission / UAV totals above.\nCONFIRM DEPLOY or CANCEL"):TEXT("NOT READY: validate and resolve blocking issues")));return;}
    if(Name==TEXT("confirm")){if(!bConfirm)return;bConfirm=false;Name=TEXT("deploy");}
    if(bMap && (!bEditor || (Plan && Field(Plan,TEXT("status"))==TEXT("DEPLOYED") && Name!=TEXT("create_plan")))){Result->SetText(FText::FromString(TEXT("Plan is read-only")));return;}
    if(Name==TEXT("edit_route")){if(!CurrentMission())return;LoadRoute(true);PC->SetMissionPathEditing(true);Result->SetText(FText::FromString(TEXT("Click map to add; select waypoint and drag its axes to move.")));return;}
    if(Name==TEXT("delete_wp")){PC->DeleteCommandWaypoint();return;}
    if(Name==TEXT("clear_route")){FDronePathSaveData Empty;Empty.PathId=FMath::Max(1,GetAssignedUAV());PC->LoadMissionPath(Empty,true);Result->SetText(FText::FromString(TEXT("Route cleared locally. SAVE ROUTE to persist.")));return;}
    auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("action"),Name==TEXT("unassign")?TEXT("assign"):Name.ToString());R->SetStringField(TEXT("plan_id"),SelectedPlan);R->SetStringField(TEXT("mission_id"),SelectedMission);
    if(Name==TEXT("create_plan") || Name==TEXT("update_plan")){R->SetStringField(TEXT("name"),PlanName->GetText().ToString());R->SetStringField(TEXT("description"),Description->GetText().ToString());}
    if(Name==TEXT("add_mission") || Name==TEXT("rename_mission"))R->SetStringField(TEXT("name"),MissionName->GetText().ToString());
    if(Name==TEXT("assign")){int32 I=UavLabels.IndexOfByKey(UAVs->GetSelectedOption());if(I==INDEX_NONE)return;R->SetStringField(TEXT("assigned_uav_id"),UavIds[I]);}
    if(Name==TEXT("save_route")) {
        if(!PC || !CurrentMission())return;
        const auto Data=PC->BuildEditingPathsData();if(Data.Num()!=1)return;
        auto* Coordinate=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetCoordinateService().GetObject();if(!Coordinate)return;
        auto Path=MakeShared<FJsonObject>();const auto& Route=Data.CreateConstIterator().Value();Path->SetNumberField(TEXT("pathId"),Route.PathId);Path->SetBoolField(TEXT("bClosedLoop"),Route.bClosedLoop);
        TArray<TSharedPtr<FJsonValue>> Points;for(const auto& W:Route.Waypoints){const FVector Geo=ICoordinateService::Execute_WorldToGeographic(Coordinate,W.Location);auto P=MakeShared<FJsonObject>();
            P->SetNumberField(TEXT("sequence"),Points.Num()+1);P->SetNumberField(TEXT("latitude"),Geo.Y);P->SetNumberField(TEXT("longitude"),Geo.X);P->SetNumberField(TEXT("altitude"),Geo.Z);P->SetNumberField(TEXT("segmentSpeed"),W.SegmentSpeed);P->SetNumberField(TEXT("waitTime"),W.WaitTime);
            auto L=MakeShared<FJsonObject>();L->SetNumberField(TEXT("x"),W.Location.X);L->SetNumberField(TEXT("y"),W.Location.Y);L->SetNumberField(TEXT("z"),W.Location.Z);P->SetObjectField(TEXT("location"),L);Points.Add(MakeShared<FJsonValueObject>(P));}
        Path->SetArrayField(TEXT("waypoints"),Points);R->SetObjectField(TEXT("path"),Path);PC->SetMissionPathEditing(false);
    }
    Submit(R);
}
