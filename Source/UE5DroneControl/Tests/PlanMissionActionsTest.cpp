#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Shared/SecurityPlanWorkspaceWidget.h"
#include "Shared/PlanWidgetSupport.h"
#include "Shared/PlanConfiguration.h"
#include "Command/CommandScreenManager.h"
#include "Command/CommandShellWidget.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "UObject/UObjectIterator.h"
#include "Serialization/JsonSerializer.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "WebSocketsModule.h"
#include "IWebSocket.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "ImageUtils.h"
#include "Editor.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Settings/LevelEditorPlaySettings.h"

using namespace PlanUI;
namespace {
FString MissionJson(const TSharedPtr<FJsonObject>& Value){FString Text;if(Value)FJsonSerializer::Serialize(Value.ToSharedRef(),TJsonWriterFactory<>::Create(&Text));return Text;}
// The start gate requires a live Map subscription. This peer is explicitly a
// synthetic client, and does not impersonate native Map rendering/interaction.
struct FMissionMapPeer : TSharedFromThis<FMissionMapPeer> {
    FString Id=FGuid::NewGuid().ToString(),Http,Ws,Failure;bool Ready=false,Hydrated=false;double LastPing=0;FHttpRequestPtr Request;TSharedPtr<IWebSocket> Socket;
    TArray<FHttpRequestPtr> HydrationRequests;TMap<FString,TSharedPtr<FJsonObject>> Snapshots;
    void Hydrate(){
        const TWeakPtr<FMissionMapPeer> Weak=AsShared();
        for(const FString Path:{TEXT("/api/security-plans"),TEXT("/api/ui-preferences"),TEXT("/api/video-view")}){
            auto Get=FHttpModule::Get().CreateRequest();Get->SetURL(Http+Path);Get->SetVerb(TEXT("GET"));Get->SetTimeout(10);HydrationRequests.Add(Get);
            Get->OnProcessRequestComplete().BindLambda([Weak,Path](FHttpRequestPtr,FHttpResponsePtr Reply,bool Success){if(auto Self=Weak.Pin()){
                TSharedPtr<FJsonObject> Snapshot;
                if(!Success || !Reply || Reply->GetResponseCode()!=200 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Reply->GetContentAsString()),Snapshot)){Self->Failure=TEXT("Synthetic Map hydration failed: ")+Path;return;}
                Self->Snapshots.Add(Path,Snapshot);Self->Hydrated=Self->Snapshots.Num()==3;
            }});if(!Get->ProcessRequest())Failure=TEXT("Could not queue synthetic Map hydration: ")+Path;
        }
    }
    void Start(){
        FParse::Value(FCommandLine::Get(),TEXT("P1Http="),Http);FParse::Value(FCommandLine::Get(),TEXT("P1Ws="),Ws);
        auto Identity=MakeShared<FJsonObject>();Identity->SetStringField(TEXT("instance_id"),Id);Identity->SetStringField(TEXT("client_id"),TEXT("MissionActions-Synthetic-Map"));Identity->SetStringField(TEXT("client_role"),TEXT("Map"));Identity->SetStringField(TEXT("hostname"),TEXT("ISOLATED-AUTOMATION"));Identity->SetStringField(TEXT("app_version"),TEXT("MissionActions"));
        Request=FHttpModule::Get().CreateRequest();Request->SetURL(Http+TEXT("/api/context/clients"));Request->SetVerb(TEXT("POST"));Request->SetHeader(TEXT("Content-Type"),TEXT("application/json"));Request->SetContentAsString(MissionJson(Identity));Request->SetTimeout(10);
        const TWeakPtr<FMissionMapPeer> Weak=AsShared();Request->OnProcessRequestComplete().BindLambda([Weak](FHttpRequestPtr,FHttpResponsePtr Reply,bool Success){if(auto Self=Weak.Pin()){
            if(!Success || !Reply || Reply->GetResponseCode()!=200){Self->Failure=TEXT("Synthetic Map peer registration failed");return;}
            Self->Socket=FWebSocketsModule::Get().CreateWebSocket(Self->Ws);
            Self->Socket->OnConnected().AddLambda([Weak]{if(auto Peer=Weak.Pin()){auto Message=MakeShared<FJsonObject>();Message->SetStringField(TEXT("type"),TEXT("subscribe_context"));Message->SetStringField(TEXT("instance_id"),Peer->Id);Peer->Socket->Send(MissionJson(Message));}});
            Self->Socket->OnMessage().AddLambda([Weak](const FString& Text){if(auto Peer=Weak.Pin()){TSharedPtr<FJsonObject> Value;if(FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Value) && Field(Value,TEXT("type"))==TEXT("context_subscribed")){Peer->Ready=true;Peer->Hydrate();}}});
            Self->Socket->OnConnectionError().AddLambda([Weak](const FString& Error){if(auto Peer=Weak.Pin())Peer->Failure=Error;});Self->Socket->Connect();
        }});if(!Request->ProcessRequest())Failure=TEXT("Could not queue synthetic Map registration");
    }
    void Tick(){const double Now=FPlatformTime::Seconds();if(Ready && Socket && Socket->IsConnected() && Now-LastPing>1){LastPing=Now;Socket->Send(Hydrated?TEXT("{\"type\":\"context_ping\",\"hydrated\":true}"):TEXT("{\"type\":\"context_ping\",\"hydrated\":false}"));}}
    void Close(){if(Request){Request->OnProcessRequestComplete().Unbind();Request->CancelRequest();}for(const auto& Get:HydrationRequests){Get->OnProcessRequestComplete().Unbind();Get->CancelRequest();}if(Socket){Socket->OnConnected().Clear();Socket->OnMessage().Clear();Socket->OnConnectionError().Clear();Socket->Close();}}
};
}

DEFINE_LATENT_AUTOMATION_COMMAND(FPlanTasksStartPIE);
bool FPlanTasksStartPIE::Update(){FRequestPlaySessionParams Params;Params.EditorPlaySettings=DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(),GetTransientPackage());Params.EditorPlaySettings->NewWindowWidth=1920;Params.EditorPlaySettings->NewWindowHeight=1080;Params.EditorPlaySettings->CenterNewWindow=true;GUnrealEd->RequestPlaySession(Params);return true;}

// Exercises the shipped widget handlers against the isolated Backend fixture.
// Reflected/component actions are deliberately not reported as mouse evidence.
class FPlanMissionActions : public IAutomationLatentCommand {
    FAutomationTestBase* Test;int32 Step=0;double Started=FPlatformTime::Seconds();
    FString Original,First,Second,Copy,CopyFirst,CopySecond,DeploymentsBefore,VideoBefore,Out;
    TSharedRef<FMissionMapPeer> MapPeer=MakeShared<FMissionMapPeer>();double CaptureAt=0;TSet<FString> Captured;
    int32 ExecutionsBefore=0;
    bool Capture(const TCHAR* Name){
        if(FParse::Param(FCommandLine::Get(),TEXT("NullRHI")) || Captured.Contains(Name))return true;
        if(CaptureAt==0){CaptureAt=FPlatformTime::Seconds()+.7;return false;}if(FPlatformTime::Seconds()<CaptureAt)return false;
        FSlateApplication::Get().InvalidateAllWidgets(false);TArray<FColor> Pixels;FIntVector Size=FIntVector::ZeroValue;auto View=GEngine->GameViewport?GEngine->GameViewport->GetGameViewportWidget():nullptr;
        if(View && FSlateApplication::Get().TakeScreenshot(View.ToSharedRef(),Pixels,Size)){TArray64<uint8> Bytes;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Bytes);Test->TestTrue(TEXT("Task workflow screenshot persisted"),FFileHelper::SaveArrayToFile(Bytes,*(Out/(FString(Name)+TEXT(".png")))));}else Test->AddError(TEXT("Task workflow rendered screenshot unavailable"));
        CaptureAt=0;Captured.Add(Name);return true;
    }
    bool CheckSelectionHelper(USecurityPlanWorkspaceWidget* P){
        auto Fixture=MakeShared<FJsonObject>(),Executions=MakeShared<FJsonObject>();Fixture->SetObjectField(TEXT("executions"),Executions);
        auto Add=[&](const TCHAR* Id,const FString& Mission,const TCHAR* Status,double Time){auto E=MakeShared<FJsonObject>();E->SetStringField(TEXT("execution_id"),Id);E->SetStringField(TEXT("plan_id"),Original);E->SetStringField(TEXT("mission_id"),Mission);E->SetStringField(TEXT("state"),Status);E->SetNumberField(TEXT("created_at"),Time);Executions->SetObjectField(Id,E);};
        Add(TEXT("north-active"),First,TEXT("PAUSED"),1);Add(TEXT("south-newer"),Second,TEXT("EXECUTING"),3);Add(TEXT("north-history"),First,TEXT("COMPLETED"),2);
        Test->TestEqual(TEXT("selected task keeps its active run despite newer sibling and terminal history"),Field(P->SelectedExecution(Fixture),TEXT("execution_id")),FString(TEXT("north-active")));
        P->MissionId=Second;Test->TestEqual(TEXT("second task resolves its own execution"),Field(P->SelectedExecution(Fixture),TEXT("execution_id")),FString(TEXT("south-newer")));
        P->MissionId=TEXT("missing");Test->TestFalse(TEXT("missing task never falls back to plan latest"),P->SelectedExecution(Fixture).IsValid());P->MissionId=First;
        Test->AddInfo(TEXT("Execution-selection helper checks use a local JSON fixture only; navigation/copy/guards below use actual Backend ACKs."));return true;
    }
public:
    explicit FPlanMissionActions(FAutomationTestBase* InTest):Test(InTest){Out=FPaths::ProjectSavedDir()/TEXT("MissionActionsQA")/FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S"));IFileManager::Get().MakeDirectory(*Out,true);Test->AddInfo(TEXT("Task workflow evidence: ")+Out);}
    ~FPlanMissionActions() override {MapPeer->Close();}
    bool Update() override {
        MapPeer->Tick();if(!MapPeer->Failure.IsEmpty()){Test->AddError(MapPeer->Failure);return true;}
        if(FPlatformTime::Seconds()-Started>120){Test->AddError(FString::Printf(TEXT("MissionActions timed out at step %d (Map subscribed=%d hydrated=%d)"),Step,MapPeer->Ready,MapPeer->Hydrated));return true;}
        UWorld* World=nullptr;for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE){World=Context.World();break;}if(!World)return false;
        auto* Sync=World->GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();if(!Sync->IsHydrated())return false;
        USecurityPlanWorkspaceWidget* P=nullptr;for(TObjectIterator<USecurityPlanWorkspaceWidget> It;It;++It)if(It->GetWorld()==World){P=*It;break;}if(!P)return false;
        P->Refresh();if(P->bPending)return false;const auto State=Sync->GetPlans();
        auto Row=[&](const TCHAR* Action,int32 Index){if(!Test->TestTrue(TEXT("task action row exists"),P->MissionList->MissionActionIds.IsValidIndex(Index)))return false;P->MissionAction(Action,P->MissionList->MissionActionIds[Index]);return true;};
        if(Step==0){
            MapPeer->Start();auto* PC=Cast<ADroneOpsPlayerController>(World->GetFirstPlayerController());auto* Manager=PC?PC->GetCommandScreenManager():nullptr;auto* Shell=Manager?Manager->GetShell():nullptr;
            if(Shell){struct FNavigation {FName Action;int32 Index;} Args{TEXT("navigate"),1};Shell->ProcessEvent(Shell->FindFunction(TEXT("Navigate")),&Args);}
            const auto Plans=Object(State,TEXT("plans"));if(Plans)for(const auto& Entry:Plans->Values)if(Field(Entry.Value->AsObject(),TEXT("name"))==TEXT("Mission actions deployed fixture"))Original=Entry.Key;
            const auto Plan=Find(State,TEXT("plans"),Original);if(!Test->TestTrue(TEXT("real deployed two-task fixture exists"),Plan.IsValid() && Plan->GetArrayField(TEXT("mission_ids")).Num()==2))return true;
            First=Plan->GetArrayField(TEXT("mission_ids"))[0]->AsString();Second=Plan->GetArrayField(TEXT("mission_ids"))[1]->AsString();
            DeploymentsBefore=MissionJson(Object(State,TEXT("deployments")));VideoBefore=MissionJson(Sync->GetVideoView());ExecutionsBefore=Object(State,TEXT("executions"))->Values.Num();
            const FString Before=Field(Sync->GetContext(),TEXT("active_mission_id"));P->OpenMissionExecution(Original,First);
            Test->TestTrue(TEXT("overview task navigation waits for selection ACK"),P->bPending);Test->TestEqual(TEXT("task selection is not optimistic"),Field(Sync->GetContext(),TEXT("active_mission_id")),Before);++Step;return false;
        }
        if(Step==1){
            P->WorkspaceScroll->ScrollToStart();if(!Capture(TEXT("01-deployed-task-list")))return false;
            if(!Test->TestEqual(TEXT("overview navigation selects requested first task exactly"),P->MissionId,First))return true;
            Test->TestTrue(TEXT("open execution opens details without a start intent"),P->bExecutionView && P->ExecutionIntent.IsEmpty());
            Test->TestTrue(TEXT("deployed tasks remain visible outside the editor step"),P->MissionList->GetParent()==P->Workspace && P->MissionList->IsVisible());CheckSelectionHelper(P);
            const int32 Stale=P->MissionList->MissionActionIds[0];P->MissionList->Refresh(State,Find(State,TEXT("plans"),Original),Second);
            P->MissionAction(TEXT("select_mission"),Stale);Test->TestFalse(TEXT("stale row generation is ignored instead of retargeting"),P->bPending);P->Refresh();
            if(!Row(TEXT("edit_mission"),1))return true;++Step;return false;
        }
        if(Step==2){
            if(!Capture(TEXT("02-second-task-version-editor")))return false;
            Copy=P->PlanId;const auto Plan=Find(State,TEXT("plans"),Copy);
            if(!Test->TestTrue(TEXT("deployed edit creates a separate editable version"),Copy!=Original && Field(Plan,TEXT("status"))==TEXT("DRAFT")))return true;
            Test->TestEqual(TEXT("new version records its source plan"),Field(Plan,TEXT("source_plan_id")),Original);
            CopyFirst=Plan->GetArrayField(TEXT("mission_ids"))[0]->AsString();CopySecond=Plan->GetArrayField(TEXT("mission_ids"))[1]->AsString();
            Test->TestEqual(TEXT("editing second deployed task retains second-task identity after copy"),P->MissionId,CopySecond);Test->TestEqual(TEXT("copied task name is preserved"),P->MissionName->GetText().ToString(),FString(TEXT("South task")));
            Test->TestTrue(TEXT("copied task editor owns acknowledged lease"),P->CanEditConfiguration() && P->CurrentStep==1);
            Test->TestEqual(TEXT("copy leaves immutable deployments unchanged"),MissionJson(Object(State,TEXT("deployments"))),DeploymentsBefore);
            P->MissionName->SetText(User(TEXT("South task revised")));if(!Row(TEXT("select_mission"),0))return true;
            Test->TestTrue(TEXT("switching a dirty task asks before leaving"),P->bConfigurationGuard);Test->TestEqual(TEXT("dirty switch leaves Backend selection untouched"),Field(Sync->GetContext(),TEXT("active_mission_id")),CopySecond);
            P->Action(TEXT("guard_keep"),0);Test->TestEqual(TEXT("keep editing retains task input"),P->MissionName->GetText().ToString(),FString(TEXT("South task revised")));
            if(!Row(TEXT("select_mission"),0))return true;P->Action(TEXT("guard_save"),0);++Step;return false;
        }
        if(Step==3){
            if(!Test->TestEqual(TEXT("save and continue selects the requested sibling after ACK"),P->MissionId,CopyFirst))return true;
            Test->TestEqual(TEXT("previous task rename persisted before switching"),Field(Find(State,TEXT("missions"),CopySecond),TEXT("name")),FString(TEXT("South task revised")));
            Test->TestEqual(TEXT("same-plan task switch keeps configuration lease"),PlanConfiguration::PlanId(State),Copy);
            if(!Row(TEXT("execute_mission"),1))return true;++Step;return false;
        }
        if(Step==4){
            Test->TestEqual(TEXT("draft execution entry retains selected second task"),P->MissionId,CopySecond);Test->TestEqual(TEXT("draft execution entry stops at pre-deployment review"),P->CurrentStep,3);
            Test->TestEqual(TEXT("actual Backend validates the copied plan"),Field(Find(State,TEXT("plans"),Copy),TEXT("status")),FString(TEXT("READY")));
            Test->TestTrue(TEXT("task rows remain usable during review"),P->MissionList->IsVisible() && P->MissionList->GetIsEnabled());
            Test->TestEqual(TEXT("check does not deploy"),MissionJson(Object(State,TEXT("deployments"))),DeploymentsBefore);
            if(!Row(TEXT("edit_mission"),0))return true;++Step;return false;
        }
        if(Step==5){
            Test->TestTrue(TEXT("review edit entry returns to chosen task editor"),P->CurrentStep==1 && P->MissionId==CopyFirst && !P->MissionName->GetIsReadOnly());
            P->MissionName->SetText(User(TEXT("Discard this task change")));P->OpenMissionExecution(Original,Second);
            Test->TestTrue(TEXT("overview navigation obeys existing unsaved guard"),P->bConfigurationGuard);P->Action(TEXT("guard_keep"),0);
            Test->TestEqual(TEXT("cancelled overview navigation keeps original selected task"),P->MissionId,CopyFirst);
            P->OpenMissionExecution(Original,Second);
            Test->TestTrue(TEXT("pending dirty decision blocks another mission navigation"),P->IsMissionNavigationBlocked());
            P->OpenMissionExecution(Original,First);
            Test->TestTrue(TEXT("repeated navigation preserves original unsaved decision"),P->bConfigurationGuard && !P->bPending);
            P->Action(TEXT("guard_discard"),0);++Step;return false;
        }
        if(Step==6){
            if(!Test->TestTrue(TEXT("discarded cross-plan navigation selects exact deployed task"),P->PlanId==Original && P->MissionId==Second))return true;
            Test->TestFalse(TEXT("cross-plan navigation releases own configuration lease before selection"),PlanConfiguration::Active(State));
            Test->TestEqual(TEXT("discard never persisted task edits"),Field(Find(State,TEXT("missions"),CopyFirst),TEXT("name")),FString(TEXT("North task")));
            Test->TestTrue(TEXT("execution detail navigation never starts or confirms a task"),P->bExecutionView && P->ExecutionIntent.IsEmpty());
            if(!Row(TEXT("execute_mission"),1))return true;++Step;return false;
        }
        if(Step==7){
            Test->TestEqual(TEXT("execute opens explicit selected-task start confirmation"),P->ExecutionIntent,FString(TEXT("start")));Test->TestEqual(TEXT("confirmation belongs to the requested second task"),P->MissionId,Second);
            P->Action(TEXT("execution_cancel"),0);Test->TestTrue(TEXT("cancelled execution intent is cleared"),P->ExecutionIntent.IsEmpty());
            Test->TestEqual(TEXT("navigation, editing, checking and cancelled execution create no runtime execution"),Object(State,TEXT("executions"))->Values.Num(),ExecutionsBefore);
            if(!Row(TEXT("execute_mission"),1))return true;++Step;return false;
        }
        if(Step==8){
            if(!MapPeer->Ready || !Sync->IsRoleOnline(TEXT("Map")))return false;
            if(!Test->TestTrue(TEXT("explicit start confirmation is available with the live synthetic Map peer"),P->Actions[TEXT("execution_confirm")]->GetIsEnabled()))return true;
            P->Action(TEXT("execution_confirm"),0);++Step;return false;
        }
        if(Step==9){
            const auto E=P->SelectedExecution(State);if(!E){if(!P->ErrorCode.IsEmpty()){Test->AddError(TEXT("Execution start rejected: ")+P->ErrorCode);return true;}return false;}
            if(Field(E,TEXT("state"))!=TEXT("EXECUTING"))return false;
            if(!Capture(TEXT("03-second-task-executing")))return false;
            Test->TestEqual(TEXT("one confirmed task creates its one assigned UAV execution"),Object(State,TEXT("executions"))->Values.Num(),ExecutionsBefore+1);
            for(const auto& Entry:Object(State,TEXT("executions"))->Values){const auto Execution=Entry.Value->AsObject();Test->TestEqual(TEXT("Backend execution belongs to selected original plan"),Field(Execution,TEXT("plan_id")),Original);Test->TestEqual(TEXT("Backend execution belongs to second task, never first"),Field(Execution,TEXT("mission_id")),Second);}
            P->Action(TEXT("execution_abort_prompt"),0);P->Action(TEXT("execution_confirm"),0);++Step;return false;
        }
        if(Field(P->SelectedExecution(State),TEXT("state"))!=TEXT("ABORTED"))return false;
        Test->TestEqual(TEXT("explicit abort cleans up the test execution"),Field(P->SelectedExecution(State),TEXT("state")),FString(TEXT("ABORTED")));
        Test->TestEqual(TEXT("deployed snapshots remain immutable through the complete workflow"),MissionJson(Object(State,TEXT("deployments"))),DeploymentsBefore);
        Test->TestEqual(TEXT("task selection does not change Video source"),MissionJson(Sync->GetVideoView()),VideoBefore);
        Test->AddInfo(TEXT("Actual Backend ACK and real Command widget flow verified; no native mouse or hardware flight claim."));return true;
    }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlanMissionActionsTest,"DroneOps.PlanConfiguration.MissionActions",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPlanMissionActionsTest::RunTest(const FString&){
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Tests/P55OfflineMap")));ADD_LATENT_AUTOMATION_COMMAND(FPlanTasksStartPIE());ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(8));
    ADD_LATENT_AUTOMATION_COMMAND(FPlanMissionActions(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
