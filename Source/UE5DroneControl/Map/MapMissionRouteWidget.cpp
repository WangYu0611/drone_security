#include "Map/MapMissionRouteWidget.h"
#include "Map/PlanRouteVisualSet.h"
#include "Components/CheckBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Shared/PlanWidgetSupport.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "DroneOps/Core/ICoordinateService.h"
#include "Command/CommandScreenManager.h"
#include "Command/CommandMapInteractionService.h"
#include "Serialization/JsonSerializer.h"
using namespace PlanUI;
void UMapMissionRouteWidget::NativeOnInitialized(){
    Super::NativeOnInitialized();Content=Root(WidgetTree);
    Label(WidgetTree,Content,T(TEXT("Workflow.RouteEditBanner")),20);
    Label(WidgetTree,Content,T(TEXT("Inspector.Title")),20);
    SelectedPointLabel=Label(WidgetTree,Content,FText::GetEmpty(),18);
    Label(WidgetTree,Content,T(TEXT("Inspector.Help")),13);
    Result=Label(WidgetTree,Content,FText::GetEmpty(),14);
    InspectorFields=WidgetTree->ConstructWidget<UVerticalBox>();Content->AddChild(InspectorFields);
    CoordinatesLabel=Label(WidgetTree,InspectorFields,FText::GetEmpty(),14);
    auto Input=[&](const TCHAR* Key){
        auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();InspectorFields->AddChild(Row);
        Label(WidgetTree,Row,T(Key),16);
        auto* Box=WidgetTree->ConstructWidget<UEditableTextBox>();CommandTheme::Input(Box);
        Row->AddChildToHorizontalBox(Box)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        Box->OnTextCommitted.AddDynamic(this,&UMapMissionRouteWidget::ParametersCommitted);return Box;
    };
    AltitudeInput=Input(TEXT("Mission3D.Altitude"));
    Label(WidgetTree,InspectorFields,T(TEXT("Inspector.Reference")),16);
    AltitudeReference=WidgetTree->ConstructWidget<UComboBoxString>();InspectorFields->AddChild(AltitudeReference);
    AltitudeReference->AddOption(TEXT("AGL"));AltitudeReference->AddOption(TEXT("Ellipsoid"));
    AltitudeReference->OnSelectionChanged.AddDynamic(this,&UMapMissionRouteWidget::ReferenceChanged);
    HoverInput=Input(TEXT("Mission3D.Hover"));SpeedInput=Input(TEXT("Inspector.Speed"));
    auto AddTo=[&](UPanelWidget* Parent,const TCHAR* A,const TCHAR* K){auto* B=Button(WidgetTree,Parent,A,K);B->OnAction.AddDynamic(this,&UMapMissionRouteWidget::Action);CommandTheme::Text(Cast<UTextBlock>(B->GetContent()),15);Actions.Add(A,B);};
    auto* PointActions=WidgetTree->ConstructWidget<UHorizontalBox>();InspectorFields->AddChild(PointActions);
    AddTo(PointActions,TEXT("waypoint_parameters"),TEXT("Mission3D.Apply"));AddTo(PointActions,TEXT("delete"),TEXT("Map.Delete"));
    auto* SaveActions=WidgetTree->ConstructWidget<UHorizontalBox>();Content->AddChild(SaveActions);
    AddTo(SaveActions,TEXT("save"),TEXT("Workflow.SaveDraft"));AddTo(SaveActions,TEXT("finish"),TEXT("Workflow.FinishRoute"));
    CommandTheme::Button(Actions[TEXT("finish")],true);
    Summary=Label(WidgetTree,Content,FText::GetEmpty(),14);
    UnsavedDialog=WidgetTree->ConstructWidget<UBorder>();CommandTheme::Panel(UnsavedDialog,CommandTheme::Elevated,CommandTheme::Warning);UnsavedDialog->SetPadding(FMargin(8));Content->AddChild(UnsavedDialog);
    auto* Guard=WidgetTree->ConstructWidget<UVerticalBox>();UnsavedDialog->SetContent(Guard);
    Label(WidgetTree,Guard,T(TEXT("Map.DirtyPrompt")),16);
    for(const auto& Entry:TArray<TPair<FString,FString>>{{TEXT("cancel"),TEXT("Workflow.ContinueEditing")},{TEXT("save"),TEXT("Workflow.SaveExit")},{TEXT("discard"),TEXT("Common.Discard")}}){auto* B=Button(WidgetTree,Guard,*Entry.Key,*Entry.Value);B->OnAction.AddDynamic(this,&UMapMissionRouteWidget::Action);}
    auto Add=[&](const TCHAR* A,const TCHAR* K){AddTo(Content,A,K);};
    Add(TEXT("begin"),TEXT("Map.Begin"));Actions[TEXT("begin")]->SetToolTipText(T(TEXT("Map.AdjustBeforeEdit")));Add(TEXT("undo"),TEXT("Workflow.Undo"));Add(TEXT("coordinates"),TEXT("Map.Add"));Add(TEXT("speed"),TEXT("Workflow.EditSpeed"));Add(TEXT("add"),TEXT("Map.Add"));Add(TEXT("clear"),TEXT("Map.Clear"));
    Add(TEXT("discard"),TEXT("Common.Discard"));Add(TEXT("cancel"),TEXT("Workflow.ContinueEditing"));Add(TEXT("exit"),TEXT("Common.Exit"));Add(TEXT("focus"),TEXT("Map.Focus"));Add(TEXT("video"),TEXT("Video.Assigned"));
    ClosedRoute=WidgetTree->ConstructWidget<UCheckBox>();Content->AddChild(ClosedRoute);Label(WidgetTree,ClosedRoute,T(TEXT("Geometry.ClosedRoute")),16);ClosedRoute->OnCheckStateChanged.AddDynamic(this,&UMapMissionRouteWidget::ClosedChanged);
    WaypointDetails=Label(WidgetTree,Content,FText::GetEmpty(),14);
    GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->OnMapRouteEditRequested.AddUObject(this,&UMapMissionRouteWidget::MapRequested);
}
int32 UMapMissionRouteWidget::GetAssignedUAV() const {auto* S=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();const auto ID=Field(Find(S->GetPlans(),TEXT("missions"),MissionId),TEXT("assigned_uav_id"));int32 N=0;if(ID.StartsWith(TEXT("UAV-")))LexTryParseString(N,*ID.Mid(4));return N;}
void UMapMissionRouteWidget::SetEditorEnabled(bool Enabled){
    if(!Enabled && !bDirty && !SessionId.IsEmpty()){bExitPending=true;Discard();return;}
    if(!Enabled && bDirty){bExitPending=true;bSelectionPending=false;PendingPlan.Empty();PendingMission.Empty();bPrompt=true;return;}
    bEditor=Enabled;SetVisibility(Enabled?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
}
TSharedRef<FJsonObject> UMapMissionRouteWidget::RouteJson() const {
    auto R=MakeShared<FJsonObject>();auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer());const auto Data=PC?PC->BuildEditingPathsData():TMap<int32,FDronePathSaveData>();
    TArray<TSharedPtr<FJsonValue>> Points;
    if(Data.Num()==1){const auto& Route=Data.CreateConstIterator().Value();R->SetNumberField(TEXT("pathId"),Route.PathId);R->SetBoolField(TEXT("bClosedLoop"),Route.bClosedLoop);
        auto* C=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetCoordinateService().GetObject();
        if(C)for(const auto& W:Route.Waypoints){const auto G=ICoordinateService::Execute_WorldToGeographic(C,W.Location);auto P=MakeShared<FJsonObject>();P->SetNumberField(TEXT("sequence"),Points.Num()+1);P->SetNumberField(TEXT("latitude"),G.Y);P->SetNumberField(TEXT("longitude"),G.X);P->SetNumberField(TEXT("altitude"),G.Z-W.AltitudeOffsetMeters);P->SetStringField(TEXT("altitude_reference"),W.AltitudeReference);
            if(W.AltitudeReference==TEXT("AGL"))P->SetNumberField(TEXT("terrain_ellipsoid_m"),W.AltitudeOffsetMeters);
            if(W.AltitudeReference==TEXT("MSL"))P->SetNumberField(TEXT("geoid_undulation_m"),W.AltitudeOffsetMeters);P->SetNumberField(TEXT("segmentSpeed"),W.SegmentSpeed);P->SetNumberField(TEXT("waitTime"),W.WaitTime);auto L=MakeShared<FJsonObject>();L->SetNumberField(TEXT("x"),W.Location.X);L->SetNumberField(TEXT("y"),W.Location.Y);L->SetNumberField(TEXT("z"),W.Location.Z);P->SetObjectField(TEXT("location"),L);Points.Add(MakeShared<FJsonValueObject>(P));}
    }R->SetArrayField(TEXT("waypoints"),Points);return R;
}
FString UMapMissionRouteWidget::Fingerprint() const {FString S;FJsonSerializer::Serialize(RouteJson(),TJsonWriterFactory<>::Create(&S));return S;}
bool UMapMissionRouteWidget::LoadSaved(){
    auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer());if(!PC)return false;auto* S=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();const auto M=Find(S->GetPlans(),TEXT("missions"),MissionId);if(!M){PC->ClearEditingPaths();LoadedRoute=TEXT("empty");return false;}
    auto* C=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetCoordinateService().GetObject();if(!C || !ICoordinateService::Execute_IsCoordinateSystemReady(C))return false;
    const auto R=Find(S->GetPlans(),TEXT("paths"),Field(M,TEXT("route_id")));
    const auto Data=FPlanRouteVisualSet::Decode(R,C,FMath::Max(1,GetAssignedUAV()));
    LoadedWaypoint=INDEX_NONE;PC->LoadMissionPath(Data,false);CleanFingerprint=Fingerprint();bDirty=false;bSaved=R.IsValid();LoadedRoute.Empty();if(R)FJsonSerializer::Serialize(R.ToSharedRef(),TJsonWriterFactory<>::Create(&LoadedRoute));else LoadedRoute=TEXT("empty");return true;
}
void UMapMissionRouteWidget::Submit(const TCHAR* Name,TFunction<void(TSharedPtr<FJsonObject>)> Complete,const TSharedPtr<FJsonObject>& Extra){
    auto* S=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();auto R=Extra?Extra.ToSharedRef():MakeShared<FJsonObject>();R->SetStringField(TEXT("action"),Name);R->SetStringField(TEXT("plan_id"),PlanId);R->SetStringField(TEXT("mission_id"),MissionId);if(!SessionId.IsEmpty())R->SetStringField(TEXT("edit_session_id"),SessionId);
    bPending=true;const auto Weak=TWeakObjectPtr<UMapMissionRouteWidget>(this);if(!S->SubmitPlan(R,[Weak,Complete](TSharedPtr<FJsonObject> Reply){if(!Weak.IsValid())return;Weak->bPending=false;Weak->ErrorCode=PlanUI::Error(Reply);Weak->ReservationDetails=ReservationError(Reply);Complete(Reply);})){bPending=false;ErrorCode=TEXT("SYNC_OFFLINE");Complete(nullptr);}
}
void UMapMissionRouteWidget::Begin(){
    if(bPending || !SessionId.IsEmpty())return;auto* S=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();const auto P=Find(S->GetPlans(),TEXT("plans"),PlanId);if(!P)return;
    auto* Coordinates=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetCoordinateService().GetObject();
    if(!Coordinates || !ICoordinateService::Execute_IsCoordinateSystemReady(Coordinates)){ErrorCode=TEXT("COORDINATES_NOT_READY");return;}
    if(bDirty && BaseRevision!=P->GetNumberField(TEXT("content_revision"))){ErrorCode=TEXT("VERSION_CONFLICT");return;}
    BaseRevision=P->GetNumberField(TEXT("content_revision"));auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("content_revision"),BaseRevision);R->SetBoolField(TEXT("workflow"),true);
    if(auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer());PC && PC->GetCommandScreenManager())PC->GetCommandScreenManager()->GetMapService()->SetRouteEditCameraLocked(true);
    bBeginning=true;
    Submit(TEXT("begin_route_edit"),[this](TSharedPtr<FJsonObject> Reply){bBeginning=false;if(!PlanUI::Error(Reply).IsEmpty()){if(auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer()))PC->SetMissionPathEditing(false);return;}SessionId=Field(Reply,TEXT("edit_session_id"));if(SessionId.IsEmpty()){if(auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer()))PC->SetMissionPathEditing(false);return;}if(bDirty)Submit(TEXT("mark_route_dirty"),[](TSharedPtr<FJsonObject>){});else LoadSaved();if(auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer()))PC->SetMissionPathEditing(true);},R);
}
void UMapMissionRouteWidget::Save(){
    if(bPending || SessionId.IsEmpty())return;
    auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer());if(!PC || !PC->RefreshMissionTerrainMetadata()){ErrorCode=TEXT("TERRAIN_REQUIRED");return;}
    bSaving=true;auto R=MakeShared<FJsonObject>();R->SetObjectField(TEXT("path"),RouteJson());R->SetBoolField(TEXT("keep_editing"),true);
    const auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();const double RequestedVersion=Sync->GetPlans()->GetNumberField(TEXT("version"));
    Submit(TEXT("save_route"),[this,RequestedVersion](TSharedPtr<FJsonObject> Reply){bSaving=false;if(!PlanUI::Error(Reply).IsEmpty()){
        bDirty=true;auto* S=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();const auto P=Find(S->GetPlans(),TEXT("plans"),PlanId);
        if(ErrorCode==TEXT("VERSION_CONFLICT") && RequestedVersion>0 && SaveRetries==0 && P && P->GetNumberField(TEXT("content_revision"))==BaseRevision && S->GetPlans()->GetNumberField(TEXT("version"))>RequestedVersion){++SaveRetries;Save();return;}bFinishAfterSave=false;SaveRetries=0;return;}
        bDirty=false;bSaved=true;SaveRetries=0;BaseRevision=Find(GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetPlans(),TEXT("plans"),PlanId)->GetNumberField(TEXT("content_revision"));CleanFingerprint=Fingerprint();if(bFinishAfterSave || bExitPending || bSelectionPending){bFinishAfterSave=false;Finish();}},R);
}
void UMapMissionRouteWidget::Finish(){
    if(bPending || SessionId.IsEmpty())return;
    if(RouteJson()->GetArrayField(TEXT("waypoints")).Num()<2){ErrorCode=TEXT("ROUTE_TOO_SHORT");return;}
    if(bDirty){bFinishAfterSave=true;Save();return;}
    Submit(TEXT("finish_route_edit"),[this](TSharedPtr<FJsonObject> Reply){
        if(!PlanUI::Error(Reply).IsEmpty())return;
        SessionId.Empty();bDirty=false;
        if(auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer()))PC->SetMissionPathEditing(false);
        const bool Switching=bSelectionPending;FinishLeave();if(!Switching)SetEditorEnabled(false);
    });
}
void UMapMissionRouteWidget::Discard(){
    if(bPending)return;
    auto Done=[this](TSharedPtr<FJsonObject> Reply){const auto C=PlanUI::Error(Reply);if(!C.IsEmpty() && C!=TEXT("EDIT_SESSION_EXPIRED"))return;
        SessionId.Empty();bDirty=false;bDiscardConfirm=false;ErrorCode.Empty();LoadSaved();if(auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer()))PC->SetMissionPathEditing(false);FinishLeave();};
    if(SessionId.IsEmpty()){Done(MakeShared<FJsonObject>());return;}Submit(TEXT("discard_route_edit"),Done);
}
void UMapMissionRouteWidget::FinishLeave(){
    if(bExitPending){bExitPending=false;bPrompt=false;SetEditorEnabled(false);return;}
    if(bSelectionPending){bSelectionPending=false;const auto P=PendingPlan,M=PendingMission;PendingPlan.Empty();PendingMission.Empty();const bool BeginNext=bBeginPending;bBeginPending=false;bPrompt=false;SwitchTo(P,M,BeginNext);}else bPrompt=false;
}
void UMapMissionRouteWidget::SwitchTo(const FString& P,const FString& M,bool Edit){
    if(P==PlanId && M==MissionId){if(Edit){SetEditorEnabled(true);Begin();}return;}
    if(bDirty || !SessionId.IsEmpty() || bSaving){PendingPlan=P;PendingMission=M;bSelectionPending=true;bBeginPending=Edit;bPrompt=true;return;}
    PlanId=P;MissionId=M;LoadedRoute.Empty();ErrorCode.Empty();LoadSaved();if(Edit){SetEditorEnabled(true);Begin();}
}
void UMapMissionRouteWidget::MapRequested(const TSharedPtr<FJsonObject>& R){if(!R || Field(R,TEXT("mode"))==TEXT("MOVE"))return;SwitchTo(Field(R,TEXT("plan_id")),Field(R,TEXT("mission_id")),true);SetVisibility(ESlateVisibility::Visible);}
void UMapMissionRouteWidget::Focus(){auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer());if(!PC || !PC->GetCommandScreenManager())return;const auto Data=PC->BuildEditingPathsData();if(Data.Num()==1 && !Data.CreateConstIterator().Value().Waypoints.IsEmpty())PC->GetCommandScreenManager()->GetMapService()->FocusLocation(Data.CreateConstIterator().Value().Waypoints[0].Location);}
void UMapMissionRouteWidget::Refresh(){
    if(!Summary)return;auto* S=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();const auto RemotePlan=Field(S->GetContext(),TEXT("active_security_plan_id")),RemoteMission=Field(S->GetContext(),TEXT("active_mission_id"));
    const auto Selection=RemotePlan+TEXT("/")+RemoteMission;if(Selection!=LastRemoteSelection && (RemotePlan.IsEmpty() || Find(S->GetPlans(),TEXT("plans"),RemotePlan))){LastRemoteSelection=Selection;SwitchTo(RemotePlan,RemoteMission,false);}
    const auto LiveSessions=Object(S->GetPlans(),TEXT("edit_sessions"));
    if(!SessionId.IsEmpty() && !bPending && !bSaving && S->IsReady() && LiveSessions && !LiveSessions->HasField(SessionId)){
        SessionId.Empty();ErrorCode=TEXT("EDIT_SESSION_EXPIRED");
    }
    // Hydrate again when descriptors/coordinates arrive late, and when another Map saved a route.
    // Never hydrate over a leased local draft, including a failed save.
    if(SessionId.IsEmpty() && !bDirty && !bPending && !bSaving){
        const auto Mission=Find(S->GetPlans(),TEXT("missions"),MissionId);
        const auto Route=Find(S->GetPlans(),TEXT("paths"),Field(Mission,TEXT("route_id")));
        FString RemoteRoute=TEXT("empty");if(Route){RemoteRoute.Empty();FJsonSerializer::Serialize(Route.ToSharedRef(),TJsonWriterFactory<>::Create(&RemoteRoute));}
        if(LoadedRoute!=RemoteRoute)LoadSaved();
    }
    if(!SessionId.IsEmpty() && !bDirty && !bPending && !bSaving && Fingerprint()!=CleanFingerprint){bDirty=true;Submit(TEXT("mark_route_dirty"),[](TSharedPtr<FJsonObject>){});}
    const auto P=Find(S->GetPlans(),TEXT("plans"),PlanId),M=Find(S->GetPlans(),TEXT("missions"),MissionId);const auto State=GetDraftState();
    TArray<FText> Lines{User(Field(P,TEXT("name"))),User(Field(M,TEXT("name"))),User(Field(M,TEXT("assigned_uav_id"))),ProductText::Get(TEXT("Plan.")+Field(P,TEXT("status"))),ProductText::Get(TEXT("Map.")+State),FText::Format(T(TEXT("Map.Count")),FText::AsNumber(RouteJson()->GetArrayField(TEXT("waypoints")).Num()))};
    if(bSaved && P)Lines.Add(FText::Format(T(TEXT("Workflow.LastSaved")),FText::AsDateTime(FDateTime::FromUnixTimestamp(P->GetNumberField(TEXT("updated_at"))))));
    const auto V=Object(P,TEXT("validation"));const TArray<TSharedPtr<FJsonValue>>* Issues;if(V && V->TryGetArrayField(TEXT("issues"),Issues))for(const auto& I:*Issues)if(Field(I->AsObject(),TEXT("mission_id"))==MissionId)Lines.Add(ProductText::Get(TEXT("Errors.")+Field(I->AsObject(),TEXT("code"))));
    TArray<FText> WaypointLines;const auto Geometry=RouteJson();const auto& Waypoints=Geometry->GetArrayField(TEXT("waypoints"));
    double Distance=0,Seconds=0;
    for(int I=0;I<Waypoints.Num();++I){const auto W=Waypoints[I]->AsObject();
        WaypointLines.Add(FText::Format(T(TEXT("Workflow.Waypoint")),FText::AsNumber(I+1),FText::AsNumber(W->GetNumberField(TEXT("altitude"))),FText::AsNumber(W->GetNumberField(TEXT("segmentSpeed")))));
        if(I>0){const auto Prev=Waypoints[I-1]->AsObject();const double Lat=FMath::DegreesToRadians((W->GetNumberField(TEXT("latitude"))+Prev->GetNumberField(TEXT("latitude")))*.5);
            const double Y=FMath::DegreesToRadians(W->GetNumberField(TEXT("latitude"))-Prev->GetNumberField(TEXT("latitude")))*6371000.;
            const double X=FMath::DegreesToRadians(W->GetNumberField(TEXT("longitude"))-Prev->GetNumberField(TEXT("longitude")))*6371000.*FMath::Cos(Lat);
            const double Z=EllipsoidAltitude(W)-EllipsoidAltitude(Prev);const double D=FMath::Sqrt(X*X+Y*Y+Z*Z);Distance+=D;Seconds+=D/FMath::Max(1.,W->GetNumberField(TEXT("segmentSpeed")));}
        Seconds+=W->GetNumberField(TEXT("waitTime"));
    }
    bool IsClosed=false;Geometry->TryGetBoolField(TEXT("bClosedLoop"),IsClosed);
    if(IsClosed && Waypoints.Num()>2){const auto Data=Cast<ADroneOpsPlayerController>(GetOwningPlayer())->BuildEditingPathsData();if(Data.Num()==1){const auto& W=Data.CreateConstIterator().Value().Waypoints;const double D=FVector::Distance(W.Last().Location,W[0].Location)/100.;Distance+=D;Seconds+=D/FMath::Max(1.f,W.Last().SegmentSpeed);}}
    WaypointLines.Insert(FText::Format(T(TEXT("Workflow.RouteStats")),FText::AsNumber(Distance),FText::AsNumber(Seconds/60)),0);
    WaypointDetails->SetText(FText::Join(User(TEXT("\n")),WaypointLines));
    Summary->SetText(FText::Join(User(TEXT("\n")),Lines));Result->SetText(bPrompt?T(TEXT("Map.DirtyPrompt")):bDiscardConfirm?T(TEXT("Map.DiscardPrompt")):ErrorCode.IsEmpty()?FText::GetEmpty():ErrorCode==TEXT("DRONE_ACTIVE_PLAN_CONFLICT") && !ReservationDetails.IsEmpty()?ReservationDetails:ProductText::Get(TEXT("Errors.")+ErrorCode));
    UnsavedDialog->SetVisibility(bPrompt?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    if(auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer()))PC->SetMissionPathEditing(!SessionId.IsEmpty() && !bPrompt && !bSaving, !SessionId.IsEmpty() || bBeginning);
    const bool CanEdit=!SessionId.IsEmpty() && !bPending && !bSaving && S->IsReady();
    for(const TCHAR* A:{TEXT("add"),TEXT("delete"),TEXT("clear"),TEXT("save"),TEXT("finish"),TEXT("undo"),TEXT("coordinates"),TEXT("speed")})Actions[A]->SetIsEnabled(CanEdit && !bPrompt);
    bool Closed=false;Geometry->TryGetBoolField(TEXT("bClosedLoop"),Closed);
    if(auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer())){
        FDroneWaypointSaveData W;int32 Index=INDEX_NONE;const bool Selected=PC->GetSelectedMissionWaypoint(W,Index);
        InspectorFields->SetIsEnabled(CanEdit && Selected && !bPrompt);
        SelectedPointLabel->SetText(Selected?FText::Format(T(TEXT("Mission3D.Selected")),User(FString::Printf(TEXT("%02d"),Index+1))):T(TEXT("Mission3D.SelectPoint")));
        if(Selected){
            SpeedInput->SetIsReadOnly(Index==0);
            auto* C=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetCoordinateService().GetObject();
            const auto Geo=ICoordinateService::Execute_WorldToGeographic(C,W.Location);
            CoordinatesLabel->SetText(FText::Format(T(TEXT("Inspector.Coordinates")),User(FString::Printf(TEXT("%.6f"),Geo.X)),User(FString::Printf(TEXT("%.6f"),Geo.Y))));
            if(LoadedWaypoint!=Index || (!AltitudeInput->HasKeyboardFocus() && !HoverInput->HasKeyboardFocus() && !SpeedInput->HasKeyboardFocus() && !W.Location.Equals(LoadedWaypointLocation,.1))){
                bLoadingParameters=true;LoadedWaypointLocation=W.Location;
                AltitudeInput->SetText(User(FString::Printf(TEXT("%.2f"),Geo.Z-W.AltitudeOffsetMeters)));
                HoverInput->SetText(User(FString::SanitizeFloat(W.WaitTime)));SpeedInput->SetText(User(FString::SanitizeFloat(W.SegmentSpeed)));
                AltitudeReference->ClearSelection();AltitudeReference->SetSelectedOption(W.AltitudeReference);bLoadingParameters=false;
                if(LoadedWaypoint!=Index)if(auto* Scroll=Cast<UScrollBox>(Content->GetParent()))Scroll->ScrollToStart();
            }
        }else CoordinatesLabel->SetText(FText::GetEmpty());
        LoadedWaypoint=Selected?Index:INDEX_NONE;
    }
    ClosedRoute->SetIsChecked(Closed);ClosedRoute->SetIsEnabled(CanEdit && !bPrompt && (Closed || Waypoints.Num()>=3));ClosedRoute->SetToolTipText(T(TEXT("Errors.CLOSED_ROUTE_TOO_SHORT")));
    Actions[TEXT("begin")]->SetIsEnabled(P && M && Field(P,TEXT("status"))!=TEXT("DEPLOYED") && SessionId.IsEmpty() && !bPending && S->IsReady());
    Actions[TEXT("coordinates")]->SetVisibility(ESlateVisibility::Collapsed);
    Actions[TEXT("focus")]->SetIsEnabled(SessionId.IsEmpty());
    Actions[TEXT("begin")]->SetVisibility(SessionId.IsEmpty()?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    Actions[TEXT("cancel")]->SetVisibility(bPrompt || bDiscardConfirm?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
}
void UMapMissionRouteWidget::Action(FName Name,int32){
    auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer());if(!PC)return;
    if(Name==TEXT("cancel")){bPrompt=false;bExitPending=false;bSelectionPending=false;PendingPlan.Empty();PendingMission.Empty();bDiscardConfirm=false;bBeginPending=false;return;}
    if(Name==TEXT("exit")){SetEditorEnabled(false);return;}if(Name==TEXT("focus")){Focus();return;}
    if(Name==TEXT("video")){const int ID=GetAssignedUAV();if(ID>0)GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->OpenVideo(FString::Printf(TEXT("UAV-%02d"),ID));return;}
    if(bPending)return;if(Name==TEXT("begin")){Begin();return;}if(Name==TEXT("save")){if(bPrompt || ApplyParameters())Save();return;}if(Name==TEXT("finish")){if(ApplyParameters())Finish();return;}
    if(Name==TEXT("discard")){if(bPrompt || bDiscardConfirm || !bDirty)Discard();else bDiscardConfirm=true;return;}
    if(SessionId.IsEmpty() || bSaving)return;
    if(Name==TEXT("waypoint_parameters")){
        ApplyParameters();return;
    }
    if(Name==TEXT("undo"))PC->UndoMissionRouteEdit();
    if(Name==TEXT("coordinates"))return;
    if(Name==TEXT("speed") && PC->GetCommandScreenManager())PC->GetCommandScreenManager()->ToggleSpeedPanel();
    if(Name==TEXT("add"))PC->SetMissionPathEditing(true);if(Name==TEXT("delete")){PC->DeleteCommandWaypoint();if(RouteJson()->GetArrayField(TEXT("waypoints")).Num()<3)PC->SetAllEditingPathsClosedLoop(false);}
    if(Name==TEXT("clear")){FDronePathSaveData Empty;Empty.PathId=FMath::Max(1,GetAssignedUAV());PC->LoadMissionPath(Empty,true);}
}


void UMapMissionRouteWidget::ClosedChanged(bool Checked){if(SessionId.IsEmpty() || bPending || bSaving)return;if(Checked && RouteJson()->GetArrayField(TEXT("waypoints")).Num()<3)return;if(auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer()))PC->SetAllEditingPathsClosedLoop(Checked);}


bool UMapMissionRouteWidget::ApplyParameters(){
    if(bLoadingParameters || LoadedWaypoint==INDEX_NONE)return true;
    if(SessionId.IsEmpty() || bPending || bSaving)return false;
    auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer());
    FDroneWaypointSaveData W;int32 Index=INDEX_NONE;
    if(!PC || !PC->GetSelectedMissionWaypoint(W,Index) || Index!=LoadedWaypoint)return false;
    double Height=0,Wait=0,Speed=0;
    if(!LexTryParseString(Height,*AltitudeInput->GetText().ToString()) || !LexTryParseString(Wait,*HoverInput->GetText().ToString()) || !LexTryParseString(Speed,*SpeedInput->GetText().ToString()) ||
       !FMath::IsFinite(Height) || !FMath::IsFinite(Wait) || !FMath::IsFinite(Speed) || Wait<0 || Speed<0 || Speed>15){ErrorCode=TEXT("WAYPOINT_PARAMETERS_INVALID");return false;}
    const auto Ref=AltitudeReference->GetSelectedOption();
    if((Ref!=TEXT("AGL") && Ref!=TEXT("Ellipsoid")) || !PC->SetSelectedMissionWaypointParameters(Height,Wait,Ref==TEXT("AGL"),Speed)){ErrorCode=TEXT("ALTITUDE_DATUM_UNRESOLVED");return false;}
    ErrorCode.Empty();LoadedWaypoint=INDEX_NONE;return true;
}
void UMapMissionRouteWidget::ParametersCommitted(const FText&,ETextCommit::Type){if(!bLoadingParameters)ApplyParameters();}
void UMapMissionRouteWidget::ReferenceChanged(FString,ESelectInfo::Type){if(!bLoadingParameters)ApplyParameters();}
