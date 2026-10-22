#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Shared/SecurityPlanWorkspaceWidget.h"
#include "Shared/PlanConfiguration.h"
#include "Shared/ActiveExecutionControlsWidget.h"
#include "Shared/ExecutionPresentation.h"
#include "Command/CommandScreenManager.h"
#include "Command/CommandShellWidget.h"
#include "Map/MapShellWidget.h"
#include "Map/MapFleetInteractionWidget.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/WidgetSwitcher.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "UObject/UObjectIterator.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "WebSocketsModule.h"
#include "IWebSocket.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "Widgets/SWindow.h"

using namespace PlanUI;

namespace {
UWorld* ConfigurationWorld() {
    for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE)return Context.World();
    return nullptr;
}
FString JsonText(const TSharedPtr<FJsonObject>& Value) {
    FString Text;if(Value)FJsonSerializer::Serialize(Value.ToSharedRef(),TJsonWriterFactory<>::Create(&Text));return Text;
}
bool DispatchWidgetAction(FAutomationTestBase* Test,UObject* Widget,const TCHAR* Function,FName Action,int32 Index=0) {
    auto* Method=Widget?Widget->FindFunction(Function):nullptr;
    if(!Test->TestNotNull(TEXT("existing reflected widget action is available"),Method))return false;
    struct FArguments { FName Action;int32 Index; } Arguments{Action,Index};
    Widget->ProcessEvent(Method,&Arguments);return true;
}
UCommandActionButton* ButtonCaption(UUserWidget* Widget,const FText& Caption) {
    TArray<UWidget*> Children;if(Widget && Widget->WidgetTree)Widget->WidgetTree->GetAllWidgets(Children);
    for(auto* Child:Children)if(auto* Button=Cast<UCommandActionButton>(Child)) {
        auto* Label=Cast<UTextBlock>(Button->GetContent());
        if(Label && Label->GetText().ToString()==Caption.ToString())return Button;
    }
    return nullptr;
}
bool EffectiveVisibility(UWidget* Widget) {
    if(!Widget)return false;
    for(auto* Ancestor=Widget;Ancestor;Ancestor=Ancestor->GetParent())if(!Ancestor->IsVisible())return false;
    return true;
}
bool SlateReachesControl(UWidget* Widget) {
    if(!EffectiveVisibility(Widget) || !Widget->GetIsEnabled())return false;
    auto* Client=Widget->GetWorld()?Widget->GetWorld()->GetGameViewport():nullptr;
    const auto Window=Client?Client->GetWindow():nullptr;const auto Cached=Widget->GetCachedWidget();
    const auto& Geometry=Widget->GetCachedGeometry();
    if(!Window || !Cached || Geometry.GetLocalSize().X<1 || Geometry.GetLocalSize().Y<1)return false;
    const TArray<TSharedRef<SWindow>> Windows{Window.ToSharedRef()};
    const auto Path=FSlateApplication::Get().LocateWindowUnderMouse(Geometry.LocalToAbsolute(Geometry.GetLocalSize()*.5),Windows);
    if(Path.IsValid())for(int32 Index=0;Index<Path.Widgets.Num();++Index)if(Path.Widgets[Index].Widget==Cached)return true;
    return false;
}

// This peer is an actual registered/subscribed Command client on the isolated
// test Backend. It never injects a local configuration snapshot into Map.
struct FConfigurationCommandPeer : TSharedFromThis<FConfigurationCommandPeer> {
    FString Instance=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    FString Http,Ws,Failure;
    FHttpRequestPtr Request;
    TSharedPtr<IWebSocket> Socket;
    TSharedPtr<FJsonObject> Reply;
    bool HttpDone=false,Ready=false;
    int32 Status=0;
    double LastPing=0;

    FConfigurationCommandPeer() {
        FParse::Value(FCommandLine::Get(),TEXT("P1Http="),Http);
        FParse::Value(FCommandLine::Get(),TEXT("P1Ws="),Ws);
    }
    void Post(const FString& Path,const TSharedRef<FJsonObject>& Body) {
        HttpDone=false;Status=0;Reply.Reset();Failure.Empty();
        Request=FHttpModule::Get().CreateRequest();Request->SetURL(Http+Path);Request->SetVerb(TEXT("POST"));Request->SetTimeout(10);
        Request->SetHeader(TEXT("Content-Type"),TEXT("application/json"));Request->SetContentAsString(JsonText(Body));
        const TWeakPtr<FConfigurationCommandPeer> Weak=AsShared();
        Request->OnProcessRequestComplete().BindLambda([Weak](FHttpRequestPtr,FHttpResponsePtr Response,bool Success){
            if(auto Self=Weak.Pin()) {
                Self->HttpDone=true;Self->Status=Response?Response->GetResponseCode():0;
                if(!Success || !Response || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Response->GetContentAsString()),Self->Reply))
                    Self->Failure=TEXT("Missing or invalid HTTP response from isolated Backend");
            }
        });
        if(!Request->ProcessRequest()){HttpDone=true;Failure=TEXT("Could not queue isolated HTTP request");}
    }
    void Register() {
        auto Identity=MakeShared<FJsonObject>();Identity->SetStringField(TEXT("instance_id"),Instance);
        Identity->SetStringField(TEXT("client_id"),TEXT("PlanConfiguration-Automation-Command"));
        Identity->SetStringField(TEXT("client_role"),TEXT("Command"));Identity->SetStringField(TEXT("hostname"),TEXT("ISOLATED-AUTOMATION"));
        Identity->SetStringField(TEXT("app_version"),TEXT("PlanConfiguration"));Post(TEXT("/api/context/clients"),Identity);
    }
    void Subscribe() {
        Socket=FWebSocketsModule::Get().CreateWebSocket(Ws);const TWeakPtr<FConfigurationCommandPeer> Weak=AsShared();
        Socket->OnConnected().AddLambda([Weak]{if(auto Self=Weak.Pin()){
            auto Message=MakeShared<FJsonObject>();Message->SetStringField(TEXT("type"),TEXT("subscribe_context"));
            Message->SetStringField(TEXT("instance_id"),Self->Instance);Self->Socket->Send(JsonText(Message));
        }});
        Socket->OnMessage().AddLambda([Weak](const FString& Text){if(auto Self=Weak.Pin()){
            TSharedPtr<FJsonObject> Message;if(FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Message)
                && Field(Message,TEXT("type"))==TEXT("context_subscribed"))Self->Ready=true;
        }});
        Socket->OnConnectionError().AddLambda([Weak](const FString& Error){if(auto Self=Weak.Pin())Self->Failure=Error;});Socket->Connect();
    }
    void Tick() {
        if(Ready && Socket && Socket->IsConnected() && FPlatformTime::Seconds()-LastPing>1.) {
            LastPing=FPlatformTime::Seconds();Socket->Send(TEXT("{\"type\":\"context_ping\",\"hydrated\":true}"));
        }
    }
    void Configuration(const TCHAR* Action,const TSharedPtr<FJsonObject>& State) {
        auto Body=MakeShared<FJsonObject>();Body->SetStringField(TEXT("instance_id"),Instance);
        Body->SetStringField(TEXT("action"),Action);Body->SetStringField(TEXT("plan_id"),TEXT(""));
        Body->SetNumberField(TEXT("expected_version"),State->GetNumberField(TEXT("version")));
        Post(TEXT("/api/security-plans"),Body);
    }
    void Close() {
        if(Request){Request->OnProcessRequestComplete().Unbind();Request->CancelRequest();Request.Reset();}
        if(Socket){Socket->OnConnected().Clear();Socket->OnMessage().Clear();Socket->OnConnectionError().Clear();Socket->Close();Socket.Reset();}
    }
};

void CheckIndependentGroups(FAutomationTestBase* Test) {
    auto State=MakeShared<FJsonObject>(),Executions=MakeShared<FJsonObject>();State->SetObjectField(TEXT("executions"),Executions);
    auto Add=[&](const TCHAR* Id,const TCHAR* Group,const TCHAR* Status,const TCHAR* Uav,double Created){
        auto E=MakeShared<FJsonObject>();E->SetStringField(TEXT("execution_id"),Id);E->SetStringField(TEXT("group_id"),Group);
        E->SetStringField(TEXT("state"),Status);E->SetStringField(TEXT("uav_id"),Uav);E->SetStringField(TEXT("plan_name"),Group);
        E->SetNumberField(TEXT("created_at"),Created);E->SetNumberField(TEXT("progress"),.25);Executions->SetObjectField(Id,E);
    };
    Add(TEXT("a-completed"),TEXT("a"),TEXT("COMPLETED"),TEXT("UAV-01"),1);
    Add(TEXT("a-returning"),TEXT("a"),TEXT("RETURNING"),TEXT("UAV-02"),1);
    Add(TEXT("a-paused"),TEXT("a"),TEXT("PAUSED"),TEXT("UAV-03"),1);
    Add(TEXT("b-executing"),TEXT("b"),TEXT("EXECUTING"),TEXT("UAV-04"),2);
    Add(TEXT("c-finished"),TEXT("c"),TEXT("COMPLETED"),TEXT("UAV-05"),3);
    Add(TEXT("d-scheduled"),TEXT("d"),TEXT("SCHEDULED"),TEXT("UAV-06"),4);
    const auto Groups=ExecutionControls::Groups(State);Test->TestEqual(TEXT("active rows retain each independent group and exclude completed history"),Groups.Num(),3);
    const auto A=ExecutionControls::Find(State,TEXT("a"));
    Test->TestEqual(TEXT("mixed group includes terminal members without hiding live members"),A.Members.Num(),3);
    Test->TestEqual(TEXT("mixed live members retain Mixed status"),A.State,FString(TEXT("Mixed")));
    Test->TestTrue(TEXT("RETURNING and PAUSED members expose appropriate actions"),A.CanPause && A.CanResume && A.CanAbort);
    Test->TestEqual(TEXT("pause targets returning member, not first completed member"),Field(ExecutionControls::Representative(A,TEXT("pause")),TEXT("execution_id")),FString(TEXT("a-returning")));
    Test->TestEqual(TEXT("resume targets paused member of the explicit group"),Field(ExecutionControls::Representative(A,TEXT("resume")),TEXT("execution_id")),FString(TEXT("a-paused")));
    const auto B=ExecutionControls::Find(State,TEXT("b"));
    Test->TestEqual(TEXT("another group remains independently addressable"),Field(ExecutionControls::Representative(B,TEXT("pause")),TEXT("execution_id")),FString(TEXT("b-executing")));
    const auto Scheduled=ExecutionControls::Find(State,TEXT("d"));
    Test->TestFalse(TEXT("scheduled task has no misleading pause or abort controls"),Scheduled.CanPause || Scheduled.CanResume || Scheduled.CanAbort);
    Test->TestFalse(TEXT("completed history has no live pause target"),ExecutionControls::Representative(ExecutionControls::Find(State,TEXT("c")),TEXT("pause")).IsValid());
    auto Lease=MakeShared<FJsonObject>();Lease->SetStringField(TEXT("owner_instance_id"),TEXT("owner"));
    Lease->SetNumberField(TEXT("lease_expires_at"),double(FDateTime::UtcNow().ToUnixTimestamp())-1);State->SetObjectField(TEXT("configuration_session"),Lease);
    Test->TestFalse(TEXT("expired configuration cannot keep local movement locked"),PlanConfiguration::Active(State));
    Test->AddInfo(TEXT("Mixed/RETURNING/group identity assertions are pure helper coverage, separate from live Backend checks."));
}
}

// Friend of the real workspace, so all actions use the production validation,
// request queue and ACK handlers rather than a duplicated test implementation.
class FPlanConfigurationWorkspace : public IAutomationLatentCommand {
    FAutomationTestBase* Test;
    TWeakObjectPtr<USecurityPlanWorkspaceWidget> Panel;
    double Started=0;
    int32 Step=0,InitialPlans=0,InitialExecutions=0;
    FString PlanId,VideoBefore;
public:
    explicit FPlanConfigurationWorkspace(FAutomationTestBase* InTest):Test(InTest){}
    bool Update() override {
        if(Started==0)Started=FPlatformTime::Seconds();
        if(FPlatformTime::Seconds()-Started>100){Test->AddError(FString::Printf(TEXT("Configuration workspace timed out at step %d"),Step));return true;}
        auto* World=ConfigurationWorld();if(!World)return false;
        auto* Sync=World->GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();if(!Sync->IsHydrated())return false;
        auto* PC=Cast<ADroneOpsPlayerController>(World->GetFirstPlayerController());auto* Manager=PC?PC->GetCommandScreenManager():nullptr;
        auto* Shell=Manager?Manager->GetShell():nullptr;
        if(!Shell){Test->AddError(TEXT("Workspace test requires the real Command client role"));return true;}
        if(!Panel.IsValid())for(TObjectIterator<USecurityPlanWorkspaceWidget> It;It;++It)if(It->GetWorld()==World){Panel=*It;break;}
        if(!Panel.IsValid())return false;auto* P=Panel.Get();P->Refresh();if(P->bPending)return false;
        const auto State=Sync->GetPlans();const auto Plans=Object(State,TEXT("plans")),Executions=Object(State,TEXT("executions"));
        if(!Plans || !Executions){Test->AddError(TEXT("Authoritative domain collections are missing"));return true;}
        if(Step==0) {
            InitialPlans=Plans->Values.Num();InitialExecutions=Executions->Values.Num();VideoBefore=JsonText(Sync->GetVideoView());
            if(!Test->TestFalse(TEXT("fresh runtime starts outside configuration"),PlanConfiguration::Active(State)))return true;
            P->Action(TEXT("new_plan"),0);
            Test->TestTrue(TEXT("new-plan entry waits for Backend ACK"),P->bPending);
            Test->TestFalse(TEXT("new form is not fabricated before ACK"),P->bCreating);++Step;return false;
        }
        if(Step==1) {
            if(!Test->TestTrue(TEXT("new form owns an acknowledged configuration lease"),P->bCreating && PlanConfiguration::IsOwner(State,Sync->GetInstanceId())))return true;
            Test->TestEqual(TEXT("opening new form creates no empty plan"),Plans->Values.Num(),InitialPlans);
            P->Action(TEXT("create_plan"),0);Test->TestEqual(TEXT("empty name is rejected before write"),P->ErrorCode,FString(TEXT("INVALID_NAME")));
            Test->TestFalse(TEXT("invalid form does not queue a request"),P->bPending);
            P->PlanName->SetText(User(TEXT("Unsaved cancelled plan")));P->Description->SetText(User(TEXT("This must not persist")));
            P->RequestFinishConfiguration();Test->TestTrue(TEXT("dirty new plan presents leave guard"),P->bConfigurationGuard);
            P->Action(TEXT("guard_keep"),0);Test->TestFalse(TEXT("keep-editing closes guard"),P->bConfigurationGuard);
            Test->TestEqual(TEXT("keep-editing retains original text"),P->PlanName->GetText().ToString(),FString(TEXT("Unsaved cancelled plan")));
            Test->TestTrue(TEXT("keep-editing retains Backend lease"),PlanConfiguration::IsOwner(Sync->GetPlans(),Sync->GetInstanceId()));
            P->RequestFinishConfiguration();P->Action(TEXT("guard_discard"),0);++Step;return false;
        }
        if(Step==2) {
            if(!Test->TestFalse(TEXT("discard leaves configuration only after release ACK"),PlanConfiguration::Active(State)))return true;
            Test->TestEqual(TEXT("discarded form leaves no plan behind"),Plans->Values.Num(),InitialPlans);
            Test->TestTrue(TEXT("discard returns to plan list"),P->bList && !P->bCreating);
            P->Action(TEXT("new_plan"),0);++Step;return false;
        }
        if(Step==3) {
            if(!Test->TestTrue(TEXT("second form reacquires lease"),P->bCreating && P->CanEditConfiguration()))return true;
            P->PlanName->SetText(User(TEXT("Configuration acceptance")));P->Description->SetText(User(TEXT("Saved original description")));
            P->Action(TEXT("create_plan"),0);++Step;return false;
        }
        if(Step==4) {
            PlanId=P->PlanId;const auto Plan=Find(State,TEXT("plans"),PlanId);
            if(!Test->TestTrue(TEXT("valid form creates one acknowledged plan"),Plan.IsValid() && Plans->Values.Num()==InitialPlans+1))return true;
            Test->TestEqual(TEXT("name persisted through actual Backend"),Field(Plan,TEXT("name")),FString(TEXT("Configuration acceptance")));
            Test->TestEqual(TEXT("lease follows the newly created plan"),PlanConfiguration::PlanId(State),PlanId);
            Test->TestEqual(TEXT("new plan contains its initial task"),Plan->GetArrayField(TEXT("mission_ids")).Num(),1);
            Test->TestFalse(TEXT("successful save leaves clean configuration"),P->HasUnsavedChanges());
            P->PlanName->SetText(User(TEXT("Discard this rename")));P->Description->SetText(User(TEXT("Keep while viewing alarms")));
            DispatchWidgetAction(Test,Shell,TEXT("Navigate"),TEXT("navigate"),3);Shell->Refresh();
            TArray<UWidget*> Children;Shell->WidgetTree->GetAllWidgets(Children);UWidgetSwitcher* Pages=nullptr;
            for(auto* Child:Children)if(auto* Switcher=Cast<UWidgetSwitcher>(Child)){Pages=Switcher;break;}
            if(Test->TestNotNull(TEXT("Command workspace page switcher"),Pages))Test->TestEqual(TEXT("Figma retains three main pages"),Pages->GetChildrenCount(),3);
            auto* AlarmModal=Shell->WidgetTree->FindWidget(TEXT("FigmaModalBackdrop"));
            if(Test->TestNotNull(TEXT("Alarm modal exists"),AlarmModal))Test->TestTrue(TEXT("alarm view remains accessible during configuration"),AlarmModal->IsVisible());
            DispatchWidgetAction(Test,Shell,TEXT("FigmaAction"),TEXT("close_dialog"),0);
            Test->TestEqual(TEXT("viewing alarms preserves unsaved input"),P->Description->GetText().ToString(),FString(TEXT("Keep while viewing alarms")));
            Test->TestTrue(TEXT("alarm viewing does not release configuration"),PlanConfiguration::IsOwner(Sync->GetPlans(),Sync->GetInstanceId()));
            Test->TestFalse(TEXT("alarm viewing does not send a plan mutation"),P->bPending);
            P->RequestFinishConfiguration();Test->TestTrue(TEXT("saved plan dirty leave also guards"),P->bConfigurationGuard);
            P->Action(TEXT("guard_discard"),0);++Step;return false;
        }
        if(Step==5) {
            if(!Test->TestFalse(TEXT("discarded edit releases lease"),PlanConfiguration::Active(State)))return true;
            Test->TestEqual(TEXT("discard preserves saved name"),Field(Find(State,TEXT("plans"),PlanId),TEXT("name")),FString(TEXT("Configuration acceptance")));
            Test->TestEqual(TEXT("discard preserves saved description"),Field(Find(State,TEXT("plans"),PlanId),TEXT("description")),FString(TEXT("Saved original description")));
            P->Action(TEXT("resume_configuration"),0);++Step;return false;
        }
        if(Step==6) {
            if(!Test->TestTrue(TEXT("existing plan resumes authoritative configuration"),P->CanEditConfiguration()))return true;
            P->PlanName->SetText(User(TEXT("Configuration saved on exit")));P->Description->SetText(User(TEXT("Confirmed before releasing lease")));
            P->RequestFinishConfiguration();Test->TestTrue(TEXT("save-and-leave starts from an explicit dirty guard"),P->bConfigurationGuard);
            P->Action(TEXT("guard_save"),0);++Step;return false;
        }
        const auto Plan=Find(State,TEXT("plans"),PlanId);
        Test->TestEqual(TEXT("save-and-leave persists edited name"),Field(Plan,TEXT("name")),FString(TEXT("Configuration saved on exit")));
        Test->TestEqual(TEXT("save-and-leave persists edited description"),Field(Plan,TEXT("description")),FString(TEXT("Confirmed before releasing lease")));
        Test->TestFalse(TEXT("save-and-leave releases acknowledged configuration"),PlanConfiguration::Active(State));
        Test->TestTrue(TEXT("successful finish leaves no dirty prompt"),P->bList && !P->bConfigurationGuard && !P->HasUnsavedChanges());
        Test->TestEqual(TEXT("configuration never starts flight or Mock execution"),Executions->Values.Num(),InitialExecutions);
        Test->TestEqual(TEXT("configuration and alert navigation preserve exact Video view"),JsonText(Sync->GetVideoView()),VideoBefore);
        Test->AddInfo(TEXT("Real Command widget and Backend ACK coverage; reflected actions are component evidence, not native mouse acceptance."));return true;
    }
};

class FPlanConfigurationMapControls : public IAutomationLatentCommand {
    struct FMoveReply { bool Done=false;TSharedPtr<FJsonObject> Reply; };
    FAutomationTestBase* Test;
    TSharedRef<FConfigurationCommandPeer> Peer=MakeShared<FConfigurationCommandPeer>();
    TSharedRef<FMoveReply> MoveReply=MakeShared<FMoveReply>();
    double Started=0,Next=0;
    int32 Step=0;
    bool bMonitorOpened=false;
    FString GroupId,VideoBefore;
public:
    explicit FPlanConfigurationMapControls(FAutomationTestBase* InTest):Test(InTest){}
    ~FPlanConfigurationMapControls() override { Peer->Close(); }
    bool Update() override {
        if(Started==0)Started=FPlatformTime::Seconds();Peer->Tick();
        if(FPlatformTime::Seconds()-Started>100){Test->AddError(FString::Printf(TEXT("Map configuration controls timed out at step %d"),Step));return true;}
        if(!Peer->Failure.IsEmpty()){Test->AddError(Peer->Failure);return true;}
        if(FPlatformTime::Seconds()<Next)return false;
        auto* World=ConfigurationWorld();if(!World)return false;auto* Sync=World->GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();if(!Sync->IsHydrated())return false;
        auto* PC=Cast<ADroneOpsPlayerController>(World->GetFirstPlayerController());auto* Manager=PC?PC->GetCommandScreenManager():nullptr;
        auto* Shell=Manager?Manager->GetMapShell():nullptr;auto* Fleet=Shell?Shell->GetFleetInteraction():nullptr;
        if(!Fleet){Test->AddError(TEXT("MapControls test requires the real Map role"));return true;}
        auto* Registry=World->GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();const auto State=Sync->GetPlans();
        Shell->Refresh();
        if(Step==0) {
            if(!Test->TestTrue(TEXT("Map fleet controls initially available"),Fleet->IsAvailable()))return true;
            VideoBefore=JsonText(Sync->GetVideoView());Sync->SetLocalSelection(1,{1,2});Peer->Register();++Step;return false;
        }
        if(Step==1) {
            if(!Peer->HttpDone)return false;if(!Test->TestEqual(TEXT("synthetic Command identity registered with actual Backend"),Peer->Status,200))return true;
            Peer->Subscribe();++Step;return false;
        }
        if(Step==2) {
            if(!Peer->Ready || Registry->GetMultiSelectedDrones().Num()!=2)return false;
            const auto Home=ExecutionUI::MockPosition(State,TEXT("UAV-01"));if(!Test->TestTrue(TEXT("isolated Mock position is hydrated"),Home.IsValid()))return true;
            auto Target=MakeShared<FJsonObject>();Target->SetNumberField(TEXT("latitude"),Home->GetNumberField(TEXT("latitude"))+.003);
            Target->SetNumberField(TEXT("longitude"),Home->GetNumberField(TEXT("longitude")));Target->SetNumberField(TEXT("altitude"),Home->GetNumberField(TEXT("altitude")));
            auto Request=MakeShared<FJsonObject>();Request->SetStringField(TEXT("action"),TEXT("execution_group_move"));Request->SetStringField(TEXT("request_id"),FGuid::NewGuid().ToString());
            Request->SetObjectField(TEXT("target"),Target);Request->SetNumberField(TEXT("target_area_radius_m"),10000.);
            Request->SetArrayField(TEXT("assigned_uav_ids"),{MakeShared<FJsonValueString>(TEXT("UAV-01")),MakeShared<FJsonValueString>(TEXT("UAV-02"))});
            const auto Result=MoveReply;if(!Sync->SubmitPlan(Request,[Result](TSharedPtr<FJsonObject> Reply){Result->Reply=Reply;Result->Done=true;})){
                Test->AddError(TEXT("Could not submit isolated Mock group fixture"));return true;
            }++Step;return false;
        }
        if(Step==3) {
            if(!MoveReply->Done)return false;if(!Test->TestTrue(TEXT("Map group fixture has actual Backend ACK"),PlanUI::Error(MoveReply->Reply).IsEmpty()))return true;
            const auto Execution=Find(State,TEXT("executions"),Field(MoveReply->Reply,TEXT("execution_id")));if(!Execution)return false;
            if(Field(Execution,TEXT("state"))!=TEXT("EXECUTING"))return false;GroupId=Field(Execution,TEXT("group_id"));
            Test->TestTrue(TEXT("Existing group can explicitly open its monitor"),Fleet->ShowExecutionControls());
            Test->TestTrue(TEXT("Operator can hide the existing execution monitor before configuration"),Fleet->CancelMode());
            Fleet->SetMode(EMapFleetMode::BoxSelect);Test->TestEqual(TEXT("selection mode can be armed before configuration"),Fleet->GetMode(),EMapFleetMode::BoxSelect);
            Peer->Configuration(TEXT("begin_configuration"),Sync->GetPlans());++Step;return false;
        }
        if(Step==4) {
            if(!Peer->HttpDone)return false;if(!Test->TestEqual(TEXT("Command entered configuration through HTTP and live presence"),Peer->Status,200))return true;
            if(!PlanConfiguration::IsOwner(State,Peer->Instance))return false;
            Test->TestFalse(TEXT("Map fleet entry is gated by remote acknowledged configuration"),Fleet->IsAvailable());
            Test->TestEqual(TEXT("configuration cancels a previously armed selection gesture"),Fleet->GetMode(),EMapFleetMode::Browse);
            Fleet->SetMode(EMapFleetMode::GroupMove);Test->TestEqual(TEXT("configuration prevents direct reentry into fleet movement"),Fleet->GetMode(),EMapFleetMode::Browse);
            Test->TestFalse(TEXT("configuration does not retain primary-pointer ownership"),Fleet->OwnsPrimaryInput());
            Test->TestEqual(TEXT("configuration does not hide the existing selected execution group"),Fleet->GetManagedGroup(),GroupId);
            auto* NewMove=ButtonCaption(Shell,ProductText::Source(TEXT("Fleet move")));
            if(Test->TestNotNull(TEXT("Map fleet toolbar button exists"),NewMove))Test->TestEqual(TEXT("new fleet move button is hidden during configuration"),NewMove->GetVisibility(),ESlateVisibility::Collapsed);
            auto* Pause=ButtonCaption(Fleet,T(TEXT("Execution.Pause")));
            if(!Test->TestNotNull(TEXT("existing group's pause control exists"),Pause))return true;
            if(!bMonitorOpened){
                Test->TestFalse(TEXT("Configuration does not automatically reopen the hidden fleet card"),EffectiveVisibility(Pause));
                auto* Monitor=Cast<UCommandActionButton>(Shell->WidgetTree->FindWidget(TEXT("execution_controls")));
                if(!Test->TestTrue(TEXT("Explicit execution-control entry is actually hit-testable during configuration"),SlateReachesControl(Monitor)))return true;
                Monitor->OnAction.Broadcast(TEXT("execution_controls"),0);bMonitorOpened=true;Next=FPlatformTime::Seconds()+.3;return false;
            }
            Test->TestEqual(TEXT("Explicit execution controls stay in monitor-only Browse mode"),Fleet->GetMode(),EMapFleetMode::Browse);
            Test->TestTrue(TEXT("Monitor entry never stages a destination"),Fleet->GetPreviewMembers().IsEmpty());
            if(!Test->TestTrue(TEXT("Pause is visible through every parent and reachable on the rendered Slate path"),SlateReachesControl(Pause)))return true;
            auto* Abort=ButtonCaption(Fleet,T(TEXT("Execution.Abort")));
            if(!Test->TestTrue(TEXT("Abort also remains actually reachable during configuration"),SlateReachesControl(Abort)))return true;
            Pause->OnAction.Broadcast(TEXT("pause"),0);++Step;return false;
        }
        if(Step==5) {
            const auto Group=ExecutionControls::Find(State,GroupId);if(Group.State!=TEXT("PAUSED"))return false;
            Test->TestEqual(TEXT("pause ACK affects both group members"),Group.Executions.Num(),2);
            Test->TestTrue(TEXT("pausing existing work does not end configuration"),PlanConfiguration::IsOwner(State,Peer->Instance));
            DispatchWidgetAction(Test,Fleet,TEXT("Action"),TEXT("abort"));
            Test->TestEqual(TEXT("opening abort confirmation never changes execution"),ExecutionControls::Find(Sync->GetPlans(),GroupId).State,FString(TEXT("PAUSED")));
            DispatchWidgetAction(Test,Fleet,TEXT("Action"),TEXT("abort_cancel"));
            DispatchWidgetAction(Test,Fleet,TEXT("Action"),TEXT("abort_confirm"));
            Test->TestEqual(TEXT("cancelled confirmation cannot subsequently abort the group"),ExecutionControls::Find(Sync->GetPlans(),GroupId).State,FString(TEXT("PAUSED")));
            DispatchWidgetAction(Test,Fleet,TEXT("Action"),TEXT("abort"));DispatchWidgetAction(Test,Fleet,TEXT("Action"),TEXT("abort_confirm"));++Step;return false;
        }
        if(Step==6) {
            if(ExecutionControls::Find(State,GroupId).State!=TEXT("ABORTED"))return false;
            Test->TestTrue(TEXT("abort ACK preserves configuration ownership"),PlanConfiguration::IsOwner(State,Peer->Instance));
            Peer->Configuration(TEXT("end_configuration"),State);++Step;return false;
        }
        if(!Peer->HttpDone)return false;if(!Test->TestEqual(TEXT("explicit configuration release acknowledged"),Peer->Status,200))return true;
        if(PlanConfiguration::Active(State))return false;
        Test->TestTrue(TEXT("Map movement becomes available only after release broadcast"),Fleet->IsAvailable());
        auto* NewMove=ButtonCaption(Shell,ProductText::Source(TEXT("Fleet move")));
        if(Test->TestNotNull(TEXT("Map fleet toolbar button restored"),NewMove))Test->TestEqual(TEXT("fleet move entry restored after configuration"),NewMove->GetVisibility(),ESlateVisibility::Visible);
        Test->TestEqual(TEXT("configuration created no extra execution"),Object(State,TEXT("executions"))->Values.Num(),2);
        Test->TestEqual(TEXT("group control and configuration preserve Video target"),JsonText(Sync->GetVideoView()),VideoBefore);
        Test->AddInfo(TEXT("Live isolated Mock Backend, subscribed Command peer and real Map widgets; this does not claim native mouse or real flight evidence."));return true;
    }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlanConfigurationWorkspaceTest,"DroneOps.PlanConfiguration.Workspace",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPlanConfigurationWorkspaceTest::RunTest(const FString&) {
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Tests/P55OfflineMap")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(8));
    ADD_LATENT_AUTOMATION_COMMAND(FPlanConfigurationWorkspace(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlanConfigurationMapControlsTest,"DroneOps.PlanConfiguration.MapControls",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPlanConfigurationMapControlsTest::RunTest(const FString&) {
    CheckIndependentGroups(this);
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Tests/P55OfflineMap")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(8));
    ADD_LATENT_AUTOMATION_COMMAND(FPlanConfigurationMapControls(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
