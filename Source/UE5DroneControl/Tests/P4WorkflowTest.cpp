#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Shared/OperationalContextSubsystem.h"
#include "Shared/OperationalEventStore.h"
#include "Shared/UILanguageSubsystem.h"
#include "Shared/ProductText.h"
#include "Shared/SecurityPlanWorkspaceWidget.h"
#include "Map/MapMissionRouteWidget.h"
#include "Command/CommandScreenManager.h"
#include "Command/CommandMapInteractionService.h"
#include "Camera/CameraActor.h"
#include "Video/VideoShellWidget.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Components/SplineMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SViewport.h"
#include "Internationalization/Internationalization.h"
#include "Serialization/JsonSerializer.h"
#include "Shared/PlanWidgetSupport.h"
#include "UObject/UObjectIterator.h"
#include "PathEditor/DronePathActor.h"
#include "Shared/ExecutionPresentation.h"
#include "Map/MapExecutionWidget.h"
#include "Map/MapPlanMoveWidget.h"
#include "DroneOps/Core/ICoordinateService.h"
namespace {
UWorld* World(){for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)return C.World();return nullptr;}
TSharedRef<FJsonObject> Context(int V,const FString& UAV){auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("context_version"),V);for(const TCHAR* K:{TEXT("active_alert_id"),TEXT("active_mission_id"),TEXT("active_security_plan_id"),TEXT("active_area_id")})O->SetField(K,MakeShared<FJsonValueNull>());O->SetStringField(TEXT("active_uav_id"),UAV);O->SetStringField(TEXT("operation_mode"),TEXT("MONITOR"));return O;}
TSharedRef<FJsonObject> Video(int V,const FString& Id){auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("video_view_version"),V);O->SetStringField(TEXT("video_target_uav_id"),Id);return O;}
TSharedRef<FJsonObject> Language(int V,const FString& L){auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("ui_preferences_version"),V);O->SetStringField(TEXT("language"),L);return O;}
class FP4Replica:public IAutomationLatentCommand {
    FAutomationTestBase* Test;bool IsVideo;
public:FP4Replica(FAutomationTestBase* T,bool V):Test(T),IsVideo(V){}
    bool Update() override {
        auto* W=World();if(!W){Test->AddError(TEXT("PIE world missing"));return true;}auto* GI=W->GetGameInstance();auto* S=GI->GetSubsystem<UOperationalContextSubsystem>();auto* Registry=GI->GetSubsystem<UDroneRegistrySubsystem>();
        for(int Id:{1,2}){FDroneDescriptor D;D.DroneId=Id;D.Name=TEXT("QA original name");Registry->RegisterDrone(D);}
        Test->TestTrue(TEXT("accept independent video snapshot"),S->ApplyVideoView(Video(90000,TEXT("UAV-01"))));
        S->ApplyContext(Context(90000,TEXT("UAV-02")));
        auto* PC=Cast<ADroneOpsPlayerController>(W->GetFirstPlayerController());auto* Shell=PC && PC->GetCommandScreenManager()?PC->GetCommandScreenManager()->GetVideoShell():nullptr;
        if(IsVideo){Test->TestNotNull(TEXT("real Video shell"),Shell);if(!Shell)return true;Shell->Refresh();Test->TestEqual(TEXT("selection did not switch video"),Shell->GetDisplayedDroneId(),1);}
        const int Generation=Shell?Shell->GetBrowserGeneration():0;const auto* Browser=Shell?Shell->GetBrowser():nullptr;
        auto ContextBefore=S->GetContext();const auto PlansBefore=S->GetPlans();
        Test->TestTrue(TEXT("no target catalog contains explicit instruction"),ProductText::Get(TEXT("Video.NoTargetHint")).ToString().Contains(TEXT("video")) || ProductText::Get(TEXT("Video.NoTargetHint")).ToString().Contains(TEXT("视频")));
        const FText LockedText=ProductText::Get(TEXT("Map.ViewLocked"));
        const FText Stable=ProductText::Get(TEXT("Plan.Title"));S->ApplyUIPreferences(Language(90000,TEXT("zh-Hans")));
        Test->TestEqual(TEXT("route lock status Chinese"),LockedText.ToString(),FString(TEXT("航线编辑中 | 地图视角已锁定")));
        Test->TestEqual(TEXT("formal localization changes retained FText"),Stable.ToString(),FString(TEXT("安保方案")));
        FOperationalEvent E;E.EventType=TEXT("UAV_ASSIGNED");E.Params=MakeShared<FJsonObject>();E.Params->SetStringField(TEXT("uav_id"),TEXT("UAV-01"));E.Params->SetStringField(TEXT("mission_name"),TEXT("East Perimeter"));
        Test->TestTrue(TEXT("structured event renders Chinese without changing input"),E.DisplayText().ToString().Contains(TEXT("已分配")) && E.DisplayText().ToString().Contains(TEXT("East Perimeter")));
        S->ApplyUIPreferences(Language(90001,TEXT("en")));Test->TestEqual(TEXT("route lock status English"),LockedText.ToString(),FString(TEXT("ROUTE EDITING | MAP VIEW LOCKED")));Test->TestEqual(TEXT("retained FText switches English"),Stable.ToString(),FString(TEXT("SECURITY PLAN")));
        Test->TestTrue(TEXT("structured event re-renders English"),E.DisplayText().ToString().Contains(TEXT("assigned to mission East Perimeter")));
        Test->TestFalse(TEXT("stale language rejected"),S->ApplyUIPreferences(Language(89999,TEXT("zh-Hans"))));
        Test->TestFalse(TEXT("stale video target rejected"),S->ApplyVideoView(Video(89999,TEXT("UAV-02"))));
        Test->TestTrue(TEXT("language retains context object"),ContextBefore==S->GetContext());Test->TestTrue(TEXT("language retains plans and review object"),PlansBefore==S->GetPlans());
        Test->TestEqual(TEXT("language keeps video target"),S->GetVideoView()->GetStringField(TEXT("video_target_uav_id")),FString(TEXT("UAV-01")));
        if(Shell){Shell->Refresh();Test->TestEqual(TEXT("language does not reload browser"),Shell->GetBrowserGeneration(),Generation);Test->TestTrue(TEXT("browser object retained"),Shell->GetBrowser()==Browser);Test->TestFalse(TEXT("no source never playing"),Shell->GetMediaState()==TEXT("PLAYING"));S->ApplyVideoView(Video(90001,TEXT("UAV-02")));Shell->Refresh();Test->TestEqual(TEXT("explicit video target changes source"),Shell->GetDisplayedDroneId(),2);}
        if(!IsVideo && PC){FDronePathSaveData D;D.PathId=1;FDroneWaypointSaveData A;A.Location=FVector(10,20,30);D.Waypoints.Add(A);PC->LoadMissionPath(D,true);S->ApplyUIPreferences(Language(90002,TEXT("zh-Hans")));Test->TestTrue(TEXT("language leaves route editor open"),PC->IsPathEditMode());Test->TestEqual(TEXT("language retains draft waypoint"),PC->BuildEditingPathsData().FindChecked(1).Waypoints.Num(),1);PC->ClearEditingPaths();S->ApplyUIPreferences(Language(90003,TEXT("en")));}
        return true;
    }
};
class FP4CommandLive:public IAutomationLatentCommand {
    FAutomationTestBase* Test;int Step=0;bool Waiting=false,Done=false;double Started=FPlatformTime::Seconds();FString Plan,Mission;
public:explicit FP4CommandLive(FAutomationTestBase* T):Test(T){}
    bool Update() override {
        if(Done)return true;if(FPlatformTime::Seconds()-Started>60){Test->AddError(TEXT("Live P4 workflow timed out"));return true;}if(Waiting)return false;
        auto* W=World();if(!W)return false;auto* S=W->GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();if(!S->IsReady())return false;
        const TCHAR* Actions[]={TEXT("create_plan"),TEXT("add_mission"),TEXT("rename_mission"),TEXT("assign"),TEXT("validate"),TEXT("save_route"),TEXT("delete_mission")};if(Step>=7)return true;
        auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("action"),Actions[Step]);R->SetStringField(TEXT("name"),TEXT("P4 UE live configuration"));R->SetStringField(TEXT("plan_id"),Plan);R->SetStringField(TEXT("mission_id"),Mission);R->SetStringField(TEXT("assigned_uav_id"),TEXT("UAV-01"));Waiting=true;
        if(!S->SubmitPlan(R,[this,S](TSharedPtr<FJsonObject> Reply){Waiting=false;if(!Reply){Test->AddError(TEXT("Live Backend response missing"));Done=true;return;}
            if(Step==5){Test->TestTrue(TEXT("Command route mutation rejected by Backend"),Reply->HasField(TEXT("error")) || Reply->HasField(TEXT("code")));++Step;return;}
            if(Reply->HasField(TEXT("error")) || Reply->HasField(TEXT("code"))){Test->AddError(TEXT("Authorized Command workflow failed"));Done=true;return;}
            if(Step==0)Plan=Reply->GetStringField(TEXT("plan_id"));if(Step==1)Mission=Reply->GetStringField(TEXT("mission_id"));
            const auto P=S->GetPlans()->GetObjectField(TEXT("plans"))->GetObjectField(Plan);Test->TestTrue(TEXT("Backend acknowledged live workflow"),P.IsValid());
            if(Step==4){Test->TestEqual(TEXT("missing route remains DRAFT"),P->GetStringField(TEXT("status")),FString(TEXT("DRAFT")));Test->TestFalse(TEXT("configuration is not flight ready"),P->GetObjectField(TEXT("validation"))->GetBoolField(TEXT("ready")));}++Step;
        })){Waiting=false;Test->AddError(TEXT("Live request could not be sent"));return true;}return false;
    }
};
}
// Exercise the real Map widget, actors, undo stack and real HTTP failure/ACK path.
// Requires p4_ue_route_fixture.mjs and the isolated P4 Backend endpoints.
class FP4MapDraft:public IAutomationLatentCommand {
    FAutomationTestBase* Test;int Step=0;double Started=FPlatformTime::Seconds();
    TWeakObjectPtr<UMapMissionRouteWidget> Panel;TWeakObjectPtr<ADronePathActor> Actor;
    int UndoCount=0,PointCount=0;FString Plan,Mission;int64 Version=0;
public:explicit FP4MapDraft(FAutomationTestBase* T):Test(T){}
    bool Update() override {
        if(FPlatformTime::Seconds()-Started>75){Test->AddError(TEXT("Map live draft test timeout"));return true;}
        auto* W=World();if(!W)return false;auto* S=W->GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
        auto* PC=Cast<ADroneOpsPlayerController>(W->GetFirstPlayerController());if(!PC || !S->IsReady() || !S->GetPlans())return false;
        if(!Panel.IsValid()){for(TObjectIterator<UMapMissionRouteWidget> I;I;++I)if(I->GetWorld()==W){Panel=*I;break;}if(!Panel.IsValid())return false;}
        auto* P=Panel.Get();P->Refresh();
        if(Step==0){
            auto Plans=PlanUI::Object(S->GetPlans(),TEXT("plans"));for(const auto& Entry:Plans->Values)if(PlanUI::Field(Entry.Value->AsObject(),TEXT("name"))==TEXT("P4 UE ROUTE GUARD")){Plan=FString(Entry.Key.ToView());Mission=Entry.Value->AsObject()->GetArrayField(TEXT("mission_ids"))[0]->AsString();}
            if(Plan.IsEmpty()){Test->AddError(TEXT("Route fixture missing"));return true;}
            PC->GetCommandScreenManager()->bFollowSelection=true;PC->GetCommandScreenManager()->bSelectionDirty=true;
            P->SwitchTo(Plan,Mission,true);Step=1;return false;
        }
        if(Step==1){
            if(P->bPending)return false;if(P->SessionId.IsEmpty()){Test->AddError(TEXT("Backend begin ACK missing"));return true;}
            Test->TestTrue(TEXT("editing begins after lease ACK"),PC->IsPathEditMode());
            Test->TestFalse(TEXT("RouteEditSuspendsFollowCamera"),PC->GetCommandScreenManager()->bFollowSelection);
            Test->TestFalse(TEXT("route entry discards queued selection follow"),PC->GetCommandScreenManager()->bSelectionDirty);
            Test->TestTrue(TEXT("RouteEditCameraLock follows acknowledged edit"),PC->GetCommandScreenManager()->GetMapService()->IsRouteEditCameraLocked());
            PC->AddGeographicWaypointInEditMode(EGeographicCoordinateSystem::WGS84,116.34,39.98,80);
            PC->AddGeographicWaypointInEditMode(EGeographicCoordinateSystem::WGS84,116.341,39.981,80);
            P->Refresh();Step=2;return false;
        }
        if(Step==2){
            if(P->bPending)return false;Test->TestTrue(TEXT("geometry marks local draft dirty"),P->bDirty);
            PointCount=P->RouteJson()->GetArrayField(TEXT("waypoints")).Num();Test->TestTrue(TEXT("real waypoint actors created"),PointCount>=2);
            // Inspector regression is supplementary component evidence, not native mouse acceptance.
            auto* Path=PC->EditingPaths[0].Get();
            PC->SetEditSelectedWaypoint(Path->GetWaypointHandleActors().Last());P->Refresh();
            const int32 SelectedIndex=Path->Waypoints.Num()-1;
            Test->TestEqual(TEXT("inspector binds selected waypoint"),P->LoadedWaypoint,SelectedIndex);
            const FVector BeforeHeight=Path->GetWaypointWorldLocation(SelectedIndex);
            const int OriginalCount=Path->Waypoints.Num();
            // Ground transaction driver; native screen ray is separately verified in Runtime.
            auto PendingGround=[&](){PC->bPendingWaypointAdd=true;PC->PendingWaypointScreen={100,100};PC->PendingWaypointLocation=BeforeHeight;};
            PendingGround();PC->UpdateMapGroundClick({180,140},true);PC->CompleteMapGroundClick({100,100},true);
            Test->TestEqual(TEXT("empty map drag and return cannot append a waypoint"),Path->Waypoints.Num(),OriginalCount);
            PendingGround();PC->UpdateMapGroundClick({100,100},false);PC->CompleteMapGroundClick({100,100},true);
            Test->TestEqual(TEXT("leaving viewport cancels pending point even on reentry"),Path->Waypoints.Num(),OriginalCount);
            PendingGround();PC->CompleteMapGroundClick({102,101},true);
            Test->TestEqual(TEXT("microdrag remains a click and adds exactly one waypoint"),Path->Waypoints.Num(),OriginalCount+1);
            PC->UndoMissionRouteEdit();PC->SetEditSelectedWaypoint(Path->GetWaypointHandleActors()[SelectedIndex]);P->Refresh();
            FVector2D BodyScreen;
            auto* Camera=PC->GetViewTarget();
            const auto BeforePointDrag=Camera->GetActorTransform();
            Test->TestTrue(TEXT("waypoint body projects to screen"),PC->ProjectWorldLocationToScreen(BeforeHeight,BodyScreen));
            Test->TestTrue(TEXT("Map body drag takes precedence over coincident gizmo axes"),PC->TryBeginMapWaypointBodyDrag(BodyScreen));
            PC->bNativeMapWaypointDrag=true;
            const FVector NativeStart=PC->EditSelectedWaypoint->GetActorLocation();
            PC->UpdateDraggedEditWaypoint();
            Test->TestTrue(TEXT("native point gesture does not reread stale polled mouse position"),PC->EditSelectedWaypoint->GetActorLocation()==NativeStart);
            PC->UpdateMapWaypointBodyDrag(BodyScreen+FVector2D(30,20));
            // Movement is presented by the handle during the gesture; the
            // existing deferred path update commits canonical data on release.
            PC->HandleEditModeReleased();
            Test->TestFalse(TEXT("native point capture clears on release"),PC->bNativeMapWaypointDrag);
            FVector2D MovedScreen;PC->ProjectWorldLocationToScreen(Path->GetWaypointWorldLocation(SelectedIndex),MovedScreen);
            Test->AddInfo(FString::Printf(TEXT("Waypoint body Input=(30,20) Before=%s Expected=%s Actual=%s Delta=%s"),*BodyScreen.ToString(),*(BodyScreen+FVector2D(30,20)).ToString(),*MovedScreen.ToString(),*(MovedScreen-BodyScreen).ToString()));
            Test->TestTrue(TEXT("point follows both screen axes within half a pixel"),MovedScreen.Equals(BodyScreen+FVector2D(30,20),.5));
            Test->TestTrue(TEXT("point drag does not pan camera"),Camera->GetActorTransform().Equals(BeforePointDrag,.0001));
            Test->TestTrue(TEXT("point planar drag preserves world height"),FMath::Abs(Path->GetWaypointWorldLocation(SelectedIndex).Z-BeforeHeight.Z)<.01);
            Path->UpdateWaypoint(SelectedIndex,BeforeHeight);
            P->AltitudeInput->SetText(PlanUI::User(TEXT("120")));P->HoverInput->SetText(PlanUI::User(TEXT("-5")));
            Test->TestFalse(TEXT("negative hover rejected by inspector"),P->ApplyParameters());
            Test->TestTrue(TEXT("invalid form leaves geometry unchanged"),Path->GetWaypointWorldLocation(SelectedIndex).Equals(BeforeHeight,.001));
            P->HoverInput->SetText(PlanUI::User(TEXT("10")));P->SpeedInput->SetText(PlanUI::User(TEXT("8")));
            Test->TestTrue(TEXT("inspector applies existing waypoint fields"),P->ApplyParameters());P->Refresh();
            FDroneWaypointSaveData Selected;int32 Index=INDEX_NONE;PC->GetSelectedMissionWaypoint(Selected,Index);
            Test->TestEqual(TEXT("inspector hover reaches route model"),Selected.WaitTime,10.f);
            auto* Coordinates=W->GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetCoordinateService().GetObject();
            Test->TestTrue(TEXT("inspector updates rendered altitude"),FMath::Abs(ICoordinateService::Execute_WorldToGeographic(Coordinates,Selected.Location).Z-Selected.AltitudeOffsetMeters-120)<.001);
            auto* Map=PC->GetCommandScreenManager()->GetMapService();Map->Enter3DMode();Map->Enter2DMode();
            Test->TestTrue(TEXT("RouteEditDisablesModeSwitch in live editor"),Map->GetMapMode()==ECommandMapMode::Map2D);
            Test->TestTrue(TEXT("blocked mode switch retains selected waypoint"),PC->GetSelectedMissionWaypoint(Selected,Index) && Index==SelectedIndex);
            Test->TestEqual(TEXT("mode switch retains hover"),Selected.WaitTime,10.f);
            // AGL datum drift (2D surface vs loaded 3D terrain) must not rewrite user altitude.
            double Ground=0;
            const auto Geo=ICoordinateService::Execute_WorldToGeographic(Coordinates,Selected.Location);
            // Deterministic component-only terrain fixture; native QA uses the actual map.
            auto* Terrain=W->SpawnActor<AActor>();auto* Surface=NewObject<UBoxComponent>(Terrain);
            Terrain->SetRootComponent(Surface);Surface->SetBoxExtent(FVector(50000,50000,50));
            Surface->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Surface->SetCollisionObjectType(ECC_WorldStatic);Surface->SetCollisionResponseToAllChannels(ECR_Block);Surface->RegisterComponent();
            Terrain->SetActorLocation(ICoordinateService::Execute_GeographicToWorld(Coordinates,Geo.Y,Geo.X,0));
            if(!Test->TestTrue(TEXT("test surface resolves terrain"),PC->TryGetMissionTerrainHeight(Geo.Y,Geo.X,Ground)))return true;
            Path->Waypoints[SelectedIndex].AltitudeReference=TEXT("AGL");
            Path->Waypoints[SelectedIndex].AltitudeOffsetMeters=Ground-20;
            Path->UpdateWaypoint(SelectedIndex,ICoordinateService::Execute_GeographicToWorld(Coordinates,Geo.Y,Geo.X,Ground+100));
            Test->TestTrue(TEXT("save resamples terrain atomically"),PC->RefreshMissionTerrainMetadata());
            PC->GetSelectedMissionWaypoint(Selected,Index);
            Test->TestTrue(TEXT("terrain refresh preserves entered 120m AGL"),FMath::Abs(ICoordinateService::Execute_WorldToGeographic(Coordinates,Selected.Location).Z-Selected.AltitudeOffsetMeters-120)<.001);
            Test->TestTrue(TEXT("terrain refresh renders corrected ellipsoid height"),FMath::Abs(ICoordinateService::Execute_WorldToGeographic(Coordinates,Selected.Location).Z-Ground-120)<.001);
            UndoCount=PC->EditUndoStack.Num();Actor=(PC->EditingPaths.IsEmpty()?nullptr:PC->EditingPaths[0].Get());
            P->SwitchTo(Plan,TEXT(""),false);Test->TestTrue(TEXT("remote empty mission prompts instead of replacing draft"),P->bPrompt);
            Test->TestEqual(TEXT("pinned mission survives remote selection"),P->MissionId,Mission);
            P->Action(TEXT("cancel"),0);Test->TestFalse(TEXT("cancel dismisses leave prompt"),P->bPrompt);
            Version=S->GetPlans()->GetNumberField(TEXT("version"));S->GetPlans()->SetNumberField(TEXT("version"),0);
            P->Save();P->Refresh();Test->TestTrue(TEXT("SaveDraft keeps camera locked while input is suspended"),PC->GetCommandScreenManager()->GetMapService()->IsRouteEditCameraLocked());Step=3;return false;
        }
        if(Step==3){
            if(P->bPending)return false;S->GetPlans()->SetNumberField(TEXT("version"),Version);
            Test->TestEqual(TEXT("real stale-version rejection"),P->ErrorCode,FString(TEXT("VERSION_CONFLICT")));
            Test->TestTrue(TEXT("failed save retains editor and dirty state"),PC->IsPathEditMode() && P->bDirty && !P->SessionId.IsEmpty());
            Test->TestEqual(TEXT("failed save retains waypoints"),P->RouteJson()->GetArrayField(TEXT("waypoints")).Num(),PointCount);
            Test->TestEqual(TEXT("failed save retains undo history"),PC->EditUndoStack.Num(),UndoCount);
            Test->TestTrue(TEXT("failed save retains original path actor"),Actor.IsValid() && (PC->EditingPaths.IsEmpty()?nullptr:PC->EditingPaths[0].Get())==Actor.Get());
            P->Save();Step=4;return false;
        }
        if(Step==4){
            if(P->bPending)return false;Test->TestTrue(TEXT("SaveDraft ACK keeps camera locked"),PC->GetCommandScreenManager()->GetMapService()->IsRouteEditCameraLocked());Test->TestTrue(TEXT("draft save ACK retains session and editor"),!P->SessionId.IsEmpty() && PC->IsPathEditMode() && !P->bDirty);
            Test->TestTrue(TEXT("successful save has no error"),P->ErrorCode.IsEmpty());
            const auto M=PlanUI::Find(S->GetPlans(),TEXT("missions"),Mission);const auto R=PlanUI::Find(S->GetPlans(),TEXT("paths"),PlanUI::Field(M,TEXT("route_id")));
            Test->TestTrue(TEXT("server saved route is hydrated"),R.IsValid() && R->GetArrayField(TEXT("waypoints")).Num()==PointCount);
            if(!R){Test->AddError(TEXT("Save did not produce a route: ")+P->ErrorCode);return true;}
            Test->TestEqual(TEXT("inspector hover survives Backend save"),R->GetArrayField(TEXT("waypoints")).Last()->AsObject()->GetNumberField(TEXT("waitTime")),10.0);
            P->Begin();Step=5;return false;
        }
        if(Step==5){
            if(P->bPending)return false;PC->AddGeographicWaypointInEditMode(EGeographicCoordinateSystem::WGS84,116.342,39.982,80);P->Refresh();Step=6;return false;
        }
        if(Step==6){if(P->bPending)return false;P->SwitchTo(Plan,TEXT(""),false);P->Discard();Step=7;return false;}
        if(P->bPending)return false;Test->TestFalse(TEXT("cancel does not resume selection follow"),PC->GetCommandScreenManager()->bFollowSelection);Test->TestFalse(TEXT("CancelEdit unlocks camera after ACK"),PC->GetCommandScreenManager()->GetMapService()->IsRouteEditCameraLocked());Test->TestTrue(TEXT("discard ACK permits pending empty mission selection"),P->MissionId.IsEmpty() && P->SessionId.IsEmpty() && !P->bDirty);return true;
    }
};

// Uses the real Command workspace and the deployed protocol fixture.
class FP4CommandReviewCopy:public IAutomationLatentCommand {
    FAutomationTestBase* Test;int Step=0;double Started=FPlatformTime::Seconds();
    TWeakObjectPtr<USecurityPlanWorkspaceWidget> Panel;FString Original,Copy,DeploymentBefore;int64 ContextVersion=0;
public:explicit FP4CommandReviewCopy(FAutomationTestBase* T):Test(T){}
    bool Update() override {
        if(FPlatformTime::Seconds()-Started>65){Test->AddError(TEXT("Command review/copy timeout"));return true;}
        auto* W=World();if(!W)return false;auto* S=W->GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();if(!S->IsReady()||!S->GetPlans())return false;
        if(!Panel.IsValid()){for(TObjectIterator<USecurityPlanWorkspaceWidget> I;I;++I)if(I->GetWorld()==W){Panel=*I;break;}if(!Panel.IsValid())return false;}
        auto* P=Panel.Get();P->Refresh();if(P->bPending)return false;
        auto Send=[&](const TCHAR* A,const FString& ID){auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("action"),A);R->SetStringField(TEXT("plan_id"),ID);P->Submit(R);};
        if(Step==0){for(const auto& E:PlanUI::Object(S->GetPlans(),TEXT("plans"))->Values)if(PlanUI::Field(E.Value->AsObject(),TEXT("status"))==TEXT("DEPLOYED")){Original=FString(E.Key.ToView());break;}if(Original.IsEmpty()){Test->AddError(TEXT("Run P4 protocol fixture before Command review/copy test"));return true;}
            FJsonSerializer::Serialize(PlanUI::Object(S->GetPlans(),TEXT("deployments")).ToSharedRef(),TJsonWriterFactory<>::Create(&DeploymentBefore));Send(TEXT("select"),Original);++Step;return false;}
        if(Step==1){Test->TestTrue(TEXT("deployed name is read only"),P->PlanName->GetIsReadOnly());Test->TestFalse(TEXT("deployed content action disabled"),P->Actions[TEXT("update_plan")]->GetIsEnabled());Test->TestEqual(TEXT("copy action visible"),P->Actions[TEXT("copy_plan")]->GetVisibility(),ESlateVisibility::Visible);
            ContextVersion=S->GetContext()->GetNumberField(TEXT("context_version"));P->Refresh();P->Refresh();Test->TestFalse(TEXT("ComboBox refresh does not send selection"),P->bPending);++Step;return false;}
        if(Step==2){Test->TestEqual(TEXT("ComboBox refresh leaves authoritative selection version"),int64(S->GetContext()->GetNumberField(TEXT("context_version"))),ContextVersion);CollectGarbage(RF_NoFlags);P->Action(TEXT("copy_plan"),0);++Step;return false;}
        if(Step==3){Copy=P->PlanId;Test->TestTrue(TEXT("copy selects new plan"),!Copy.IsEmpty()&&Copy!=Original);if(Copy.IsEmpty()||Copy==Original)return true;const auto C=PlanUI::Find(S->GetPlans(),TEXT("plans"),Copy);Test->TestEqual(TEXT("copy is DRAFT"),PlanUI::Field(C,TEXT("status")),FString(TEXT("DRAFT")));Test->TestFalse(TEXT("copy is editable"),P->PlanName->GetIsReadOnly());P->Action(TEXT("validate"),0);++Step;return false;}
        if(Step==4){P->Action(TEXT("review"),0);++Step;return false;}
        if(Step==5){Test->TestTrue(TEXT("review bound to copied revision"),PlanUI::Reviewed(PlanUI::Find(S->GetPlans(),TEXT("plans"),Copy)));P->PlanName->SetText(FText::AsCultureInvariant(TEXT("P4 UE changed after review")));P->Action(TEXT("update_plan"),0);++Step;return false;}
        const auto C=PlanUI::Find(S->GetPlans(),TEXT("plans"),Copy);Test->TestFalse(TEXT("content mutation invalidates review"),PlanUI::Reviewed(C));Test->TestEqual(TEXT("changed content is DRAFT"),PlanUI::Field(C,TEXT("status")),FString(TEXT("DRAFT")));Test->TestEqual(TEXT("deploy is hidden after invalidation"),P->Actions[TEXT("deploy_page")]->GetVisibility(),ESlateVisibility::Collapsed);
        FString After;FJsonSerializer::Serialize(PlanUI::Object(S->GetPlans(),TEXT("deployments")).ToSharedRef(),TJsonWriterFactory<>::Create(&After));Test->TestEqual(TEXT("copy and edit leave original deployment immutable"),After,DeploymentBefore);return true;
    }
};
class FP52ExecutionWidgets:public IAutomationLatentCommand {
    FAutomationTestBase* Test;int Step=0;double Started=FPlatformTime::Seconds();bool Command;
public:FP52ExecutionWidgets(FAutomationTestBase* T,bool C):Test(T),Command(C){}
    bool Update() override {
        if(FPlatformTime::Seconds()-Started>45){Test->AddError(TEXT("Execution widget hydration timeout"));return true;}
        auto* W=World();if(!W)return false;auto* S=W->GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();if(!S->IsHydrated())return false;
        const auto E=ExecutionUI::Latest(S->GetPlans(),TEXT(""),true);if(!E){Test->AddError(TEXT("P52 paused protocol fixture missing"));return true;}
        if(!Command){UMapExecutionWidget* Monitor=nullptr;for(TObjectIterator<UMapExecutionWidget> I;I;++I)if(I->GetWorld()==W){Monitor=*I;break;}
            int32 DroneId=0;LexTryParseString(DroneId,*PlanUI::Field(E,TEXT("uav_id")).Mid(4));
            auto* Registry=W->GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
            auto* Mirror=Registry->GetReceiverActor(DroneId);auto* Preview=Registry->GetSenderPawn(DroneId);
            if(!Mirror || !Preview)return false;
            Test->TestNotNull(TEXT("Map execution monitor exists"),Monitor);if(Monitor){Monitor->Refresh();Test->TestTrue(TEXT("Map execution monitor visible after hydration"),Monitor->IsVisible());
                auto* CameraService=CastChecked<ADroneOpsPlayerController>(W->GetFirstPlayerController())->GetCommandScreenManager()->GetMapService();
                CameraService->SetRouteEditCameraLocked(true);Monitor->FocusedExecution.Empty();Monitor->Refresh();
                Test->TestEqual(TEXT("editing consumes automatic execution framing without deferring it"),Monitor->FocusedExecution,PlanUI::Field(Monitor->Execution,TEXT("execution_id")));
                CameraService->SetRouteEditCameraLocked(false);Monitor->Refresh();
                Test->TestEqual(TEXT("unlock does not requeue execution focus"),Monitor->FocusedExecution,PlanUI::Field(Monitor->Execution,TEXT("execution_id")));
            }
            Test->TestTrue(TEXT("Mock execution suppresses duplicate local preview"),Preview->IsHidden() && !Preview->IsActorTickEnabled());
            Test->TestTrue(TEXT("Mock receiver retains visible aircraft presentation"),!Mirror->IsHidden());
            Test->TestTrue(TEXT("Preview logical position matches authoritative receiver"),Preview->GetActorLocation().Equals(Mirror->GetActorLocation(),.01));
            const auto Before=S->GetVideoView();Test->TestTrue(TEXT("execution retains deployed geometry"),PlanUI::Object(E,TEXT("route_snapshot"))->GetArrayField(TEXT("waypoints")).Num()>=4);
            Test->TestTrue(TEXT("Map UI refresh does not alter video target"),Before==S->GetVideoView());
            // Explicit replica fixture: no command or fabricated telemetry is sent to Backend.
            const auto Fixture=MakeShared<FJsonObject>(*S->GetPlans());
            const auto All=MakeShared<FJsonObject>(*PlanUI::Object(Fixture,TEXT("executions")));
            const auto Extra=MakeShared<FJsonObject>(*E);const int Other=DroneId==1?2:1;
            Extra->SetStringField(TEXT("execution_id"),TEXT("qa-second-group"));Extra->SetStringField(TEXT("group_id"),TEXT("qa-independent-group"));
            Extra->SetStringField(TEXT("uav_id"),FString::Printf(TEXT("UAV-%02d"),Other));Extra->SetBoolField(TEXT("simulation"),false);
            Extra->SetNumberField(TEXT("created_at"),E->GetNumberField(TEXT("created_at"))+1);
            FDroneTelemetrySnapshot ActualBefore;Registry->GetTelemetry(Other,ActualBefore);
            All->SetObjectField(TEXT("qa-second-group"),Extra);Fixture->SetObjectField(TEXT("executions"),All);
            Fixture->SetNumberField(TEXT("execution_version"),Fixture->GetNumberField(TEXT("execution_version"))+10000);
            S->ApplyPlans(Fixture);if(Monitor)Monitor->Refresh();FDroneTelemetrySnapshot ActualAfter;Registry->GetTelemetry(Other,ActualAfter);
            Test->TestTrue(TEXT("independent active groups are rendered together"),Monitor && Monitor->RenderedExecutions.Num()>=2);
            Test->TestTrue(TEXT("real execution snapshot does not fabricate registry telemetry"),ActualBefore.WorldLocation.Equals(ActualAfter.WorldLocation,.001));
            Test->TestEqual(TEXT("real execution snapshot does not fabricate ONLINE"),ActualBefore.Availability,ActualAfter.Availability);
            return true;}
        USecurityPlanWorkspaceWidget* P=nullptr;for(TObjectIterator<USecurityPlanWorkspaceWidget> I;I;++I)if(I->GetWorld()==W){P=*I;break;}if(!P)return false;
        if(Step==0){auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("action"),TEXT("select"));R->SetStringField(TEXT("plan_id"),PlanUI::Field(E,TEXT("plan_id")));R->SetStringField(TEXT("mission_id"),PlanUI::Field(E,TEXT("mission_id")));P->Submit(R);++Step;return false;}
        if(P->bPending)return false;P->bList=false;P->Refresh();
        if(Step==1){Test->TestTrue(TEXT("execution workspace restored"),P->ExecutionPage->IsVisible());Test->TestTrue(TEXT("paused execution displays Resume"),P->Actions[TEXT("execution_resume")]->IsVisible());P->ExecutionAction(TEXT("execution_resume"));++Step;return false;}
        if(Step==2){if(PlanUI::Field(E,TEXT("state"))!=TEXT("EXECUTING"))return false;Test->TestTrue(TEXT("executing displays Pause"),P->Actions[TEXT("execution_pause")]->IsVisible());P->ExecutionAction(TEXT("execution_pause"));++Step;return false;}
        if(PlanUI::Field(E,TEXT("state"))!=TEXT("PAUSED"))return false;Test->TestTrue(TEXT("backend ACK restored pause controls"),P->Actions[TEXT("execution_resume")]->IsVisible());
        P->bExecutionView=false;P->Refresh();Test->TestTrue(TEXT("recent execution is available from deployed page"),P->Actions[TEXT("execution_open")]->IsVisible());return true;
    }
};

class FP53GeometryUI:public IAutomationLatentCommand {
    FAutomationTestBase* Test;int Step=0;double Started=FPlatformTime::Seconds();FString Plan,Mission,Before;
    TArray<FVector> Original;const FVector Offset=FVector(15000,-25000,0);
public:explicit FP53GeometryUI(FAutomationTestBase* T):Test(T){}
    bool Update() override {
        if(FPlatformTime::Seconds()-Started>80){Test->AddError(TEXT("P53 geometry UI timeout"));return true;}
        auto* W=World();if(!W)return false;auto* S=W->GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();if(!S->IsHydrated())return false;
        auto* PC=Cast<ADroneOpsPlayerController>(W->GetFirstPlayerController());auto* C=W->GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetCoordinateService().GetObject();if(!PC || !C || !ICoordinateService::Execute_IsCoordinateSystemReady(C))return false;
        UMapMissionRouteWidget* P=nullptr;UMapPlanMoveWidget* M=nullptr;
        for(TObjectIterator<UMapMissionRouteWidget> I;I;++I)if(I->GetWorld()==W)P=*I;
        for(TObjectIterator<UMapPlanMoveWidget> I;I;++I)if(I->GetWorld()==W)M=*I;
        if(!P || !M)return false;if(P->bPending || M->bPending)return false;
        auto Route=[&](){return PlanUI::Find(S->GetPlans(),TEXT("paths"),PlanUI::Field(PlanUI::Find(S->GetPlans(),TEXT("missions"),Mission),TEXT("route_id")));};
        auto Move=[&](){auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("mode"),TEXT("MOVE"));R->SetStringField(TEXT("plan_id"),Plan);R->SetStringField(TEXT("mission_id"),Mission);M->Requested(R);};
        if(Step==0){for(const auto& E:PlanUI::Object(S->GetPlans(),TEXT("plans"))->Values)if(PlanUI::Field(E.Value->AsObject(),TEXT("name"))==TEXT("P4 UE ROUTE GUARD")){Plan=E.Key;Mission=E.Value->AsObject()->GetArrayField(TEXT("mission_ids"))[0]->AsString();break;}
            if(Plan.IsEmpty()){Test->AddError(TEXT("Route guard fixture missing"));return true;}auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("action"),TEXT("select"));R->SetStringField(TEXT("plan_id"),Plan);R->SetStringField(TEXT("mission_id"),Mission);S->SubmitPlan(R,[](TSharedPtr<FJsonObject>){});++Step;return false;}
        if(Step==1){P->SwitchTo(Plan,Mission,true);++Step;return false;}
        if(Step==2){if(P->SessionId.IsEmpty()){Test->AddError(TEXT("Route session rejected: ")+P->ErrorCode);return true;}FDronePathSaveData D;D.PathId=1;
            for(int I=0;I<4;++I){FDroneWaypointSaveData V;V.Location=ICoordinateService::Execute_GeographicToWorld(C,39.98+(I>=2?.001:0),116.34+(I==1 || I==2?.001:0),80);V.SegmentSpeed=8;D.Waypoints.Add(V);Original.Add(V.Location);}PC->LoadMissionPath(D,true);P->ClosedChanged(true);P->Refresh();++Step;return false;}
        if(Step==3){Test->TestTrue(TEXT("closed toggle changes existing path topology"),P->RouteJson()->GetBoolField(TEXT("bClosedLoop")));Test->TestEqual(TEXT("no first-point duplication"),P->RouteJson()->GetArrayField(TEXT("waypoints")).Num(),4);for(TActorIterator<ADronePathActor> I(W);I;++I)if(I->PathNumericId==1){TArray<USplineMeshComponent*> Meshes;I->GetComponents(Meshes);Test->TestEqual(TEXT("four real spline segments"),Meshes.Num(),4);const FVector First=I->GetWaypointWorldLocation(0);I->UpdateWaypoint(0,First+FVector(500,0,0));Meshes.Empty();I->GetComponents(Meshes);Test->TestEqual(TEXT("moving first waypoint keeps closure segments"),Meshes.Num(),4);I->UpdateWaypoint(0,First);I->AddWaypoint(First+FVector(0,500,0),8);Meshes.Empty();I->GetComponents(Meshes);Test->TestEqual(TEXT("closed add rebuilds five segments"),Meshes.Num(),5);I->RemoveWaypoint(4);Meshes.Empty();I->GetComponents(Meshes);Test->TestEqual(TEXT("closed delete reconnects four segments"),Meshes.Num(),4);}P->Save();++Step;return false;}
        if(Step==4){Test->TestTrue(TEXT("closed save acknowledged"),P->ErrorCode.IsEmpty());P->Finish();++Step;return false;}
        if(Step==5){if(!P->SessionId.IsEmpty())return false;Test->TestTrue(TEXT("closed flag survives Backend roundtrip"),Route()->GetBoolField(TEXT("bClosedLoop")));FJsonSerializer::Serialize(Route().ToSharedRef(),TJsonWriterFactory<>::Create(&Before));Move();++Step;return false;}
        if(Step==6){if(!M->IsMoving()){Test->AddError(TEXT("Move rejected: ")+M->ErrorCode);return true;}Test->TestEqual(TEXT("move includes route"),M->Routes.Num(),1);M->Delta=Offset;M->Action(TEXT("cancel"),0);++Step;return false;}
        if(Step==7){Test->TestFalse(TEXT("cancel exits move mode"),M->IsMoving());FString After;FJsonSerializer::Serialize(Route().ToSharedRef(),TJsonWriterFactory<>::Create(&After));Test->TestEqual(TEXT("cancel preserves exact saved JSON"),After,Before);Move();++Step;return false;}
        if(Step==8){if(!M->IsMoving()){Test->AddError(TEXT("Second move rejected: ")+M->ErrorCode);return true;}M->Delta=Offset;M->Action(TEXT("confirm"),0);++Step;return false;}
        if(M->IsMoving()){Test->AddError(TEXT("Move confirm rejected: ")+M->ErrorCode);return true;}
        const auto R=Route();Test->TestTrue(TEXT("translation retains closed route"),R->GetBoolField(TEXT("bClosedLoop")));const auto& Points=R->GetArrayField(TEXT("waypoints"));TArray<FVector> Restored;
        for(int I=0;I<Points.Num();++I){auto V=Points[I]->AsObject();auto WorldPoint=ICoordinateService::Execute_GeographicToWorld(C,V->GetNumberField(TEXT("latitude")),V->GetNumberField(TEXT("longitude")),V->GetNumberField(TEXT("altitude")));Restored.Add(WorldPoint);Test->TestTrue(TEXT("Cesium WGS84 roundtrip matches translated world position"),WorldPoint.Equals(Original[I]+Offset,.1));}
        for(int I=0;I<Original.Num();++I){const int J=(I+1)%Original.Num();Test->TestTrue(TEXT("all edge lengths including closure preserved within 1mm"),FMath::Abs(FVector::Distance(Restored[I],Restored[J])-FVector::Distance(Original[I],Original[J]))<.1);}
        P->ReloadGeometry();Test->TestEqual(TEXT("reloaded route still four logical waypoints"),PC->BuildEditingPathsData().CreateConstIterator().Value().Waypoints.Num(),4);return true;
    }
};


class FP54MapControls:public IAutomationLatentCommand {
    FAutomationTestBase* Test;
public: explicit FP54MapControls(FAutomationTestBase* T):Test(T){}
    bool Update() override {
        auto* W=World();auto* PC=W?Cast<ADroneOpsPlayerController>(W->GetFirstPlayerController()):nullptr;
        auto* Map=PC && PC->GetCommandScreenManager()?PC->GetCommandScreenManager()->GetMapService():nullptr;
        if(!Map || !Map->GetMapCamera()){Test->AddError(TEXT("P54 camera requires Map role"));return true;}
        auto* C=W->GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetCoordinateService().GetObject();
        if(!C || !ICoordinateService::Execute_IsCoordinateSystemReady(C)){Test->AddError(TEXT("Coordinate service unavailable"));return true;}
        const FVector Focus=ICoordinateService::Execute_GeographicToWorld(C,39.98,116.34,60.);
        // Component input driver: no OS cursor, UMG focus or game-frame polling.
        // Native Slate routing is verified separately in Stage1 Runtime.
        Test->AddInfo(FString::Printf(TEXT("Camera precondition mode=%d planning=%d distance=%.3f actual=%.3f"),int(Map->CurrentMapMode),Map->IsPlanning(),Map->ViewDistance,Map->ActualDistance));
        PC->SetMissionPathEditing(false);
        auto Reset=[&](ECommandMapMode Mode,float Yaw=0.f,float Pitch=-55.f){
            Map->CancelPointer();Map->bZoomAnchored=false;Map->CurrentMapMode=Mode;
            Map->ViewFocus=Map->ActualFocus=Focus;Map->ViewDistance=Map->ActualDistance=15000.;
            Map->ViewYaw=Yaw;Map->View3DPitch=Pitch;
            Map->ActualRotation=FRotator(Mode==ECommandMapMode::Map2D?-89.9f:Pitch,Yaw,0);
            Map->AdvanceCamera(1.f/60);
        };
        auto Settle=[&](){for(int I=0;I<240;++I)Map->AdvanceCamera(1.f/120);};
        Reset(ECommandMapMode::Map2D);
        for(int I=0;I<10;++I){Map->BeginPointer(EKeys::LeftMouseButton,{100,100},true,false);Map->EndPointer(EKeys::LeftMouseButton,{100,100},true);}
        Test->TestTrue(TEXT("ClickWithoutDragDoesNotPan: ten clicks, unchanged target and actual"),Map->ViewFocus==Focus && Map->ActualFocus==Focus);
        Map->BeginPointer(EKeys::LeftMouseButton,{100,100},true,false);Map->MovePointer({102,101},true);
        Test->TestFalse(TEXT("SmallMovementBelowThresholdDoesNotPan: no dragging"),Map->bLeftDragging);
        Map->EndPointer(EKeys::LeftMouseButton,{102,101},true);
        Test->TestTrue(TEXT("SmallMovementBelowThresholdDoesNotPan: target unchanged"),Map->ViewFocus==Focus);
        int Width=0,Height=0;PC->GetViewportSize(Width,Height);
        if(!Test->TestTrue(TEXT("nonzero viewport for pixel-to-world assertions"),Width>0 && Height>0))return true;
        const FVector2D Inputs[]={{80,0},{-80,0},{0,-80},{0,80}};
        const TCHAR* Names[]={TEXT("DragRight"),TEXT("DragLeft"),TEXT("DragUp"),TEXT("DragDown")};
        for(auto Mode:{ECommandMapMode::Map2D,ECommandMapMode::Map3D})for(float Yaw:{0.f,90.f,179.f,-90.f})for(int D=0;D<4;++D){
            Reset(Mode,Yaw);
            const auto Before=Map->GetMapCamera()->GetActorTransform();
            const FKey Button=Mode==ECommandMapMode::Map2D?EKeys::LeftMouseButton:EKeys::MiddleMouseButton;
            Map->BeginPointer(Button,{100,100},true,false);Map->EndPointer(Button,FVector2D(100,100)+Inputs[D],true);
            const double Scale=2.*15000.*FMath::Tan(FMath::DegreesToRadians(30.))/Width;
            const FRotationMatrix Basis(FRotator(0,Yaw,0));
            const FVector Expected=(-Basis.GetUnitAxis(EAxis::Y)*Inputs[D].X+Basis.GetUnitAxis(EAxis::X)*Inputs[D].Y/(-Map->ActualRotation.Vector().Z))*Scale;
            const FVector Delta=Map->ViewFocus-Focus;
            Test->AddInfo(FString::Printf(TEXT("%s mode=%d yaw=%.1f Input=%s Before=%s TargetAfter=%s ActualAfter=%s ExpectedDelta=%s ActualDelta=%s"),Names[D],int(Mode),Yaw,*Inputs[D].ToString(),*Focus.ToString(),*Map->ViewFocus.ToString(),*Map->ActualFocus.ToString(),*Expected.ToString(),*Delta.ToString()));
            Test->TestTrue(Names[D],Delta.Equals(Expected,.01)); // 0.1 mm, no direction masking.
            Test->TestTrue(TEXT("same-frame press/release changes target once, actual unchanged"),Map->GetMapCamera()->GetActorTransform().Equals(Before,.0001));
            const FVector Target=Map->ViewFocus;double Error=FVector::Distance(Map->ActualFocus,Target);
            for(int I=0;I<240;++I){Map->AdvanceCamera(1.f/120);const double NextError=FVector::Distance(Map->ActualFocus,Target);
                if(!Test->TestTrue(TEXT("FastReleaseDoesNotOvershoot: error decreases and target is fixed"),NextError<=Error+.000001 && Map->ViewFocus==Target))break;Error=NextError;}
            Test->TestTrue(TEXT("DampingConverges: settled within 0.1mm in two seconds"),Map->ActualFocus.Equals(Target,.01));
            const auto Settled=Map->GetMapCamera()->GetActorTransform();Settle();
            Test->TestTrue(TEXT("settled camera has no residual drift"),Map->GetMapCamera()->GetActorTransform().Equals(Settled,.0001));
        }
        Reset(ECommandMapMode::Map2D);
        Map->BeginPointer(EKeys::LeftMouseButton,{100,100},true,false);Map->MovePointer({140,100},true);const FVector Partial=Map->ViewFocus;
        Map->EndPointer(EKeys::LeftMouseButton,{140,100},true);
        Test->TestTrue(TEXT("release does not consume an already-consumed mouse delta"),Map->ViewFocus==Partial);
        Map->BeginPointer(EKeys::LeftMouseButton,{100,100},false,false);Map->MovePointer({300,300},true);
        Test->TestTrue(TEXT("UI-origin gesture cannot become map pan"),Map->ViewFocus==Partial);
        Map->BeginPointer(EKeys::MiddleMouseButton,{100,100},true,false);Map->MovePointer({200,200},false);Map->MovePointer({500,500},true);
        Test->TestTrue(TEXT("Map to Inspector to Map cancels capture without re-entry jump"),Map->ViewFocus==Partial);
        Map->BeginPointer(EKeys::LeftMouseButton,{100,100},true,false);Map->CancelPointer();Map->EndPointer(EKeys::LeftMouseButton,{900,900},true);
        Test->TestTrue(TEXT("focus loss drops stale release"),Map->ViewFocus==Partial);
        Reset(ECommandMapMode::Map2D);
        const auto ViewportWidget=W->GetGameViewport()->GetGameViewportWidget();
        Test->TestTrue(TEXT("wheel event viewport geometry available"),ViewportWidget.IsValid());
        if(ViewportWidget.IsValid()){
            const auto& Geometry=ViewportWidget->GetCachedGeometry();const FVector2D Size=Geometry.GetLocalSize();
            const FVector2D EventPosition=Geometry.LocalToAbsolute(Size*FVector2D(.7,.65));
            const double Tan=FMath::Tan(FMath::DegreesToRadians(30.));
            const FVector2D ExpectedSlope(.4*Tan,-.3*Size.Y/Size.X*Tan);
            Map->ZoomAtScreen(1,EventPosition);
            Test->TestTrue(TEXT("wheel anchor uses event position independently of polled cursor"),Map->ZoomCursor.Equals(ExpectedSlope,1.e-6));
            Settle();Map->ZoomAtScreen(-1,EventPosition);
            // Zoom's 0.20s time constant needs >2s for a 2250-unit distance
            // change to reach the 0.01-unit stop threshold (2250*exp(-10)=0.102).
            for(int I=0;I<480;++I)Map->AdvanceCamera(1.f/120);
            Test->AddInfo(FString::Printf(TEXT("Wheel screen=%s ExpectedFocus=%s Target=%s Actual=%s Delta=%s"),*EventPosition.ToString(),*Focus.ToString(),*Map->ViewFocus.ToString(),*Map->ActualFocus.ToString(),*(Map->ActualFocus-Focus).ToString()));
            Test->TestTrue(TEXT("first wheel at relocated cursor round-trips without drift"),Map->ActualFocus.Equals(Focus,.01));
        }
        Reset(ECommandMapMode::Map2D);
        const FVector2D CursorSlope(.25,-.15);const auto Anchor=Focus+Map->PlaneOffset(15000.,Map->ActualRotation,CursorSlope);
        Map->ZoomAt(1,&CursorSlope);Test->TestTrue(TEXT("ZoomDirection: wheel up decreases target distance"),Map->ViewDistance<15000.);
        Test->TestEqual(TEXT("ZoomDamping: immediate distance unchanged"),Map->ActualDistance,15000.);
        Map->AdvanceCamera(1.f/60);Test->TestTrue(TEXT("ZoomDamping: gradual actual distance"),Map->ActualDistance>Map->ViewDistance && Map->ActualDistance<15000.);
        Map->ZoomAt(-1,&CursorSlope);Map->ZoomAt(1,&CursorSlope);Map->ZoomAt(-1,&CursorSlope);
        for(int I=0;I<240;++I){Map->AdvanceCamera(1.f/120);
            if(!Test->TestTrue(TEXT("cursor ground anchor remains fixed during damping"),(Map->ActualFocus+Map->PlaneOffset(Map->ActualDistance,Map->ActualRotation,CursorSlope)).Equals(Anchor,.01)))break;}
        Test->TestTrue(TEXT("alternating zoom has no center drift"),Map->ActualFocus.Equals(Focus,.01) && FMath::Abs(Map->ActualDistance-15000.)<.01);
        for(auto Mode:{ECommandMapMode::Map2D,ECommandMapMode::Map3D}){
            Reset(Mode);Map->ZoomAt(100,&CursorSlope);Settle();const auto Low=Map->ViewFocus;
            Map->ZoomAt(100,&CursorSlope);Test->TestTrue(TEXT("minimum zoom clamps without hidden anchor movement"),Map->ViewDistance==Map->MinDistance && Map->ViewFocus==Low);
            Map->ZoomAt(-1,&CursorSlope);Test->TestTrue(TEXT("reverse wheel responds immediately at minimum"),Map->ViewDistance>Map->MinDistance);
            Map->ZoomAt(-100,&CursorSlope);Settle();const auto High=Map->ViewFocus;
            Map->ZoomAt(-100,&CursorSlope);Test->TestTrue(TEXT("maximum zoom clamps without hidden anchor movement"),Map->ViewDistance==Map->MaxDistance && Map->ViewFocus==High);
            Map->ZoomAt(1,&CursorSlope);Test->TestTrue(TEXT("reverse wheel responds immediately at maximum"),Map->ViewDistance<Map->MaxDistance);
        }
        Reset(ECommandMapMode::Map3D);
        Map->BeginPointer(EKeys::RightMouseButton,{100,100},true,false);Map->EndPointer(EKeys::RightMouseButton,{180,140},true);
        Test->TestTrue(TEXT("Orbit uses screen X for yaw and screen Y for pitch"),Map->ViewYaw>0 && Map->View3DPitch<-55);
        Map->RotateView({100000,100000});Test->TestEqual(TEXT("PitchClamp lower bound"),Map->View3DPitch,-Map->MaxPitch);
        Map->RotateView({-200000,-100000});Test->TestEqual(TEXT("PitchClamp upper bound"),Map->View3DPitch,-Map->MinPitch);
        const FVector NewFocus=Focus+FVector(1000,2000,300);const auto BeforeFocus=Map->GetMapCamera()->GetActorTransform();Map->FocusLocation(NewFocus);
        Test->TestTrue(TEXT("Focus changes pivot without teleporting"),Map->ViewFocus==NewFocus && Map->GetMapCamera()->GetActorTransform().Equals(BeforeFocus));Settle();
        Map->RotateView({80,20});Settle();Test->TestTrue(TEXT("orbit retains focused pivot"),Map->ViewFocus==NewFocus && Map->ActualFocus.Equals(NewFocus,.01));
        const auto Center=Map->ViewFocus;const auto Distance=Map->ViewDistance;Map->Enter2DMode();Settle();Map->Enter3DMode();Settle();Map->Enter2DMode();Settle();
        Test->TestTrue(TEXT("ModeSwitchPreservesRegion"),Map->ViewFocus==Center && Map->ViewDistance==Distance && Map->ActualFocus.Equals(Center,.01));
        // Sustained mixed input, 120 simulated seconds; component stress, not native time.
        for(int I=0;I<7200;++I){if(I%120==0){Map->Enter3DMode();Map->RotateView({75,20});}if(I%120==60)Map->Enter2DMode();
            if(I%30==0)Map->PanView({10,-5});if(I%60==0)Map->ZoomAt(1,nullptr);if(I%60==30)Map->ZoomAt(-1,nullptr);Map->AdvanceCamera(1.f/60);}
        Test->TestTrue(TEXT("mixed stress keeps finite camera and bounded positive distance"),!Map->GetMapCamera()->GetActorLocation().ContainsNaN() && Map->ViewDistance>=Map->MinDistance && Map->ViewDistance<=Map->MaxDistance);
        Settle();
        FDronePathSaveData Route;Route.PathId=1;const double Heights[]={40,80,120,60};
        for(int i=0;i<4;++i){FDroneWaypointSaveData P;P.Location=ICoordinateService::Execute_GeographicToWorld(C,39.98+i*.0001,116.34,Heights[i]);P.WaitTime=i==1?5:i==2?10:0;Route.Waypoints.Add(P);}
        PC->LoadMissionPath(Route,true);const auto Paths=PC->BuildEditingPathsData();
        if(!Test->TestTrue(TEXT("route hydration returns path"),Paths.Num()>0))return true;
        const auto RoundTrip=Paths.CreateConstIterator().Value();
        Test->TestEqual(TEXT("four 3D points survive actor hydration"),RoundTrip.Waypoints.Num(),4);
        for(int i=0;i<4;++i){const auto G=ICoordinateService::Execute_WorldToGeographic(C,RoundTrip.Waypoints[i].Location);Test->TestTrue(TEXT("WGS84 / Cesium ellipsoid height roundtrip"),FMath::Abs(G.Z-Heights[i])<.001);Test->TestEqual(TEXT("hover retained"),RoundTrip.Waypoints[i].WaitTime,Route.Waypoints[i].WaitTime);}
        return true;
    }
};

#define P4_TEST(Class,Name,Command) \
IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class,"DroneOps.P4." Name,EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter) \
bool Class::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Level/CesiumWorld")));ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(8));ADD_LATENT_AUTOMATION_COMMAND(Command);ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
P4_TEST(FP4Localization,"Localization.FTextAndDraftIsolation",FP4Replica(this,false))
P4_TEST(FP4VideoTarget,"VideoTarget.ExplicitSelectionAndBrowserIsolation",FP4Replica(this,true))
P4_TEST(FP4CommandWorkflow,"Workflow.CommandLiveCRUD",FP4CommandLive(this))
P4_TEST(FP4MapDraftTest,"Workflow.MapLiveDraftGuard",FP4MapDraft(this))
P4_TEST(FP4CommandReviewCopyTest,"Workflow.CommandReviewReadonlyCopyAndCombo",FP4CommandReviewCopy(this))
P4_TEST(FP52CommandExecution,"P52.CommandExecutionControls",FP52ExecutionWidgets(this,true))
P4_TEST(FP52MapExecution,"P52.MapExecutionMonitor",FP52ExecutionWidgets(this,false))
P4_TEST(FP53GeometryUITest,"P53.GeometryUIAndCesiumRoundtrip",FP53GeometryUI(this))
P4_TEST(FP54MapControlsTest,"P54.CameraDampingAnd3DRoute",FP54MapControls(this))
#undef P4_TEST
#endif
