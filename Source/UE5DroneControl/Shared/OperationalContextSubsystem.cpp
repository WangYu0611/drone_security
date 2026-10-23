#include "Shared/OperationalContextSubsystem.h"
#include "Shared/ExecutionPresentation.h"
#include "Shared/MapSelectionVideo.h"
#include "RealTimeDroneReceiver.h"
#include "DroneOps/Core/ICoordinateService.h"
#include "Shared/ProductText.h"
#include "Shared/OperationalEventStore.h"
#include "Shared/UILanguageSubsystem.h"
#include "Command/CommandAlertStore.h"
#include "Command/CommandScreenManager.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "Engine/GameInstance.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "WebSocketsModule.h"
#include "IWebSocket.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformProcess.h"

namespace {
FString Encode(const TSharedRef<FJsonObject>& Json) {
    FString Result; auto Writer = TJsonWriterFactory<>::Create(&Result);
    FJsonSerializer::Serialize(Json, Writer); return Result;
}
}
void UOperationalContextSubsystem::Initialize(FSubsystemCollectionBase& Collection) {
    Super::Initialize(Collection);
    Collection.InitializeDependency<UDroneRegistrySubsystem>();
    const auto ClientRole = UCommandScreenManager::ResolveClientRole();
    bEnabled = ClientRole != EDroneClientRole::Standalone && !FParse::Param(FCommandLine::Get(), TEXT("P1DisableSync"));
    if (!bEnabled) return;
    Role = StaticEnum<EDroneClientRole>()->GetNameStringByValue(static_cast<int64>(ClientRole));
    InstanceId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    FParse::Value(FCommandLine::Get(), TEXT("P5Instance="), InstanceId);
    ClientId = TEXT("CLIENT-") + Role.ToUpper() + TEXT("-") + InstanceId.Left(8);
    HttpUrl = TEXT("http://127.0.0.1:8080"); WsUrl = TEXT("ws://127.0.0.1:8081/ws");
    GConfig->GetString(TEXT("/Script/UE5DroneControl.DroneNetworkManager"), TEXT("BackendBaseUrl"), HttpUrl, GGameIni);
    GConfig->GetString(TEXT("/Script/UE5DroneControl.DroneNetworkManager"), TEXT("WebSocketUrl"), WsUrl, GGameIni);
    GConfig->GetString(TEXT("OperationalContext"), TEXT("BackendBaseUrl"), HttpUrl, GGameIni);
    GConfig->GetString(TEXT("OperationalContext"), TEXT("WebSocketUrl"), WsUrl, GGameIni);
    FParse::Value(FCommandLine::Get(), TEXT("P1Http="), HttpUrl);
    FParse::Value(FCommandLine::Get(), TEXT("P1Ws="), WsUrl);
    // Registry remains the authoritative context replica; Video follow belongs to the media view.
    TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float) {
        const double Now = FPlatformTime::Seconds();
        if ((IsReady() || bConnecting) && Now - LastMessage > 12) Offline();
        if (IsReady() && Socket && Now - LastPing > 3) { LastPing = Now; Socket->Send(IsHydrated()?TEXT("{\"type\":\"context_ping\",\"hydrated\":true}"):TEXT("{\"type\":\"context_ping\",\"hydrated\":false}")); }
        if (!bConnecting && !IsReady() && Now >= NextAttempt) Connect();
        // Descriptors may arrive after the authoritative context.
        if (IsReady() && Context) ApplySelection();
        return !bStopped;
    }), 0.25f);
}
void UOperationalContextSubsystem::Deinitialize() {
    bStopped = true; ++Generation; bEnabled = false; CancelPendingLocalSelection();
    FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    if (Socket) {
        Socket->OnConnected().Clear(); Socket->OnConnectionError().Clear(); Socket->OnClosed().Clear(); Socket->OnMessage().Clear();
        Socket->Close(); Socket.Reset();
    }
    Super::Deinitialize();
}
void UOperationalContextSubsystem::Request(const FString& Verb, const FString& Path, const TSharedPtr<FJsonObject>& Body,
    TFunction<void(TSharedPtr<FJsonObject>)> Complete, bool IncludeErrors) {
    auto Req = FHttpModule::Get().CreateRequest(); Req->SetURL(HttpUrl + Path); Req->SetVerb(Verb); Req->SetTimeout(5);
    Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    if (Body) Req->SetContentAsString(Encode(Body.ToSharedRef()));
    const int32 Attempt = Generation;
    Req->OnProcessRequestComplete().BindWeakLambda(this, [this, Attempt, Complete, IncludeErrors](FHttpRequestPtr, FHttpResponsePtr Response, bool Ok) {
        if (bStopped || Generation != Attempt) return;
        TSharedPtr<FJsonObject> Json;
        if (Ok && Response && (IncludeErrors || (Response->GetResponseCode() >= 200 && Response->GetResponseCode() < 300)))
            FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Response->GetContentAsString()), Json);
        Complete(Json);
    });
    if (!Req->ProcessRequest()) Complete(nullptr);
}
void UOperationalContextSubsystem::Connect() {
    LastMessage = FPlatformTime::Seconds(); bConnecting = true; State = TEXT("RECONNECTING"); const int32 Attempt = ++Generation;
    auto Identity = MakeShared<FJsonObject>();
    Identity->SetStringField(TEXT("client_id"), ClientId); Identity->SetStringField(TEXT("client_role"), Role);
    Identity->SetStringField(TEXT("instance_id"), InstanceId); Identity->SetStringField(TEXT("hostname"), FPlatformProcess::ComputerName());
    Identity->SetStringField(TEXT("app_version"), TEXT("P5-Stage1"));
    Request(TEXT("POST"), TEXT("/api/context/clients"), Identity, [this, Attempt](TSharedPtr<FJsonObject> Registered) {
        if (!Registered) { Offline(); return; }
        Request(TEXT("GET"), TEXT("/api/context"), nullptr, [this, Attempt](TSharedPtr<FJsonObject> Snapshot) {
            if (!Snapshot || !Snapshot->HasTypedField<EJson::Number>(TEXT("context_version"))) { Offline(); return; }
            ApplyContext(Snapshot); OpenSocket(Attempt);
        });
    });
}
void UOperationalContextSubsystem::OpenSocket(int32 Attempt) {
    Socket = FWebSocketsModule::Get().CreateWebSocket(WsUrl);
    Socket->OnConnected().AddWeakLambda(this, [this, Attempt] {
        if (Generation != Attempt) return;
        auto Subscribe = MakeShared<FJsonObject>(); Subscribe->SetStringField(TEXT("type"), TEXT("subscribe_context"));
        Subscribe->SetStringField(TEXT("instance_id"), InstanceId); Socket->Send(Encode(Subscribe));
    });
    Socket->OnMessage().AddWeakLambda(this, [this, Attempt](const FString& Text) { if (Generation == Attempt) Receive(Text); });
    Socket->OnConnectionError().AddWeakLambda(this, [this, Attempt](const FString&) { if (Generation == Attempt) Offline(); });
    Socket->OnClosed().AddWeakLambda(this, [this, Attempt](int32, const FString&, bool) { if (Generation == Attempt) Offline(); });
    Socket->Connect();
}
void UOperationalContextSubsystem::Offline() {
    if(bMapSelectionPending)ViewSyncError=TEXT("MAP_VIDEO_SYNC_FAILED");
    CancelPendingLocalSelection();
    if (auto* Events=GetGameInstance()->GetSubsystem<UOperationalEventStore>()) Events->ProjectConnectivity(false);
    ++Generation; bPlansHydrated=bLanguageHydrated=bVideoHydrated=false; Clients.Empty(); bConnecting = false; State = TEXT("DISCONNECTED"); NextAttempt = FPlatformTime::Seconds() + 2;
    if (Socket) {
        Socket->OnConnected().Clear(); Socket->OnConnectionError().Clear(); Socket->OnClosed().Clear(); Socket->OnMessage().Clear();
        Socket->Close(); Socket.Reset();
    }
    UE_LOG(LogTemp, Log, TEXT("[P1][%s] BACKEND OFFLINE / SYNC OFFLINE"), *Role);
}
void UOperationalContextSubsystem::Receive(const FString& Message) {
    TSharedPtr<FJsonObject> Json;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Message), Json) || !Json) return;
    FString Type; Json->TryGetStringField(TEXT("type"), Type);
    if(Type==TEXT("UIPreferencesChanged") || Type==TEXT("VideoViewChanged") || Type==TEXT("MapRouteEditRequested")) {
        const TSharedPtr<FJsonObject>* Payload;if(Json->TryGetObjectField(TEXT("payload"),Payload)) {
            if(Type==TEXT("UIPreferencesChanged"))ApplyUIPreferences(*Payload);
            else if(Type==TEXT("VideoViewChanged"))ApplyVideoView(*Payload);
            else OnMapRouteEditRequested.Broadcast(*Payload);
        }return;
    }
    if (Type == TEXT("SecurityPlansChanged")) {
        const TSharedPtr<FJsonObject>* Payload;
        if (Json->TryGetObjectField(TEXT("payload"),Payload)) ApplyPlans(*Payload);
        return;
    }
    if (Type == TEXT("context_subscribed")) {
        Request(TEXT("GET"),TEXT("/api/security-plans"),nullptr,[this](TSharedPtr<FJsonObject> Reply){if(!Reply){Offline();return;} ApplyPlans(Reply);bPlansHydrated=true;});
        Request(TEXT("GET"),TEXT("/api/ui-preferences"),nullptr,[this](TSharedPtr<FJsonObject> Reply){if(!Reply){Offline();return;} ApplyUIPreferences(Reply);bLanguageHydrated=true;});
        Request(TEXT("GET"),TEXT("/api/video-view"),nullptr,[this](TSharedPtr<FJsonObject> Reply){if(!Reply){Offline();return;} ApplyVideoView(Reply);bVideoHydrated=true;});
    }
    if (Type == TEXT("alert")) {
        FString Id, AlertType; double DroneId = 0;
        if (Json->TryGetStringField(TEXT("alert_id"), Id) && Json->TryGetStringField(TEXT("alert_type"), AlertType) &&
            Json->TryGetNumberField(TEXT("drone_id"), DroneId))
            GetGameInstance()->GetSubsystem<UCommandAlertStore>()->AddSharedAlert(Id, static_cast<int32>(DroneId), AlertType, AlertType);
    }
    if (Type == TEXT("context_pong")) { LastMessage = FPlatformTime::Seconds(); return; }
    if (Type == TEXT("context_subscribed") || Type == TEXT("OperationalContextChanged")) {
        const TSharedPtr<FJsonObject>* Payload;
        if (!Json->TryGetObjectField(TEXT("payload"), Payload) || !(*Payload)->HasTypedField<EJson::Number>(TEXT("context_version"))) return;
        ApplyContext(*Payload); LastMessage = FPlatformTime::Seconds();
        if (Type == TEXT("context_subscribed")) { bConnecting = false; State = TEXT("CONNECTED");
            if(auto* Events=GetGameInstance()->GetSubsystem<UOperationalEventStore>()) Events->ProjectConnectivity(true); }
    }
    auto ApplyClient = [this](const TSharedPtr<FJsonObject>& Client) {
        FString Id; double IncomingVersion;
        if (!Client->TryGetStringField(TEXT("instance_id"), Id) || !Client->TryGetNumberField(TEXT("client_version"), IncomingVersion)) return;
        const auto* Existing = Clients.Find(Id); double PreviousVersion = -1;
        if (Existing) (*Existing)->TryGetNumberField(TEXT("client_version"), PreviousVersion);
        if (IncomingVersion > PreviousVersion) {
            Clients.Add(Id, Client);
            FString ClientRole, ClientState; Client->TryGetStringField(TEXT("client_role"),ClientRole); Client->TryGetStringField(TEXT("state"),ClientState);
            if(auto* Events=GetGameInstance()->GetSubsystem<UOperationalEventStore>()) Events->ProjectPresence(Id,ClientRole,ClientState);
        }
    };
    if (Type == TEXT("context_subscribed")) {
        const TArray<TSharedPtr<FJsonValue>>* Items;
        if (Json->TryGetArrayField(TEXT("clients"), Items)) {
            for (const auto& Item : *Items) if (Item->Type == EJson::Object) {
                auto Client = Item->AsObject(); FString Id;
                ApplyClient(Client);
            }
        }
    }
    if (Type == TEXT("client_connected") || Type == TEXT("client_disconnected")) {
        const TSharedPtr<FJsonObject>* Client; FString Id;
        if (Json->TryGetObjectField(TEXT("payload"), Client)) ApplyClient(*Client);
    }
    if (Type == TEXT("error")) Offline();
}
bool UOperationalContextSubsystem::ApplyContext(const TSharedPtr<FJsonObject>& Incoming) {
    if (!Incoming) return false;
    double Number; if (!Incoming->TryGetNumberField(TEXT("context_version"), Number) || Number <= Version) return false;
    if (!FMath::IsFinite(Number) || Number < 0 || Number != FMath::FloorToDouble(Number) || Number > 9007199254740991.0) return false;
    for (const TCHAR* Field : {TEXT("active_uav_id"), TEXT("active_alert_id"), TEXT("active_mission_id"), TEXT("active_security_plan_id"), TEXT("active_area_id")}) {
        const auto Value = Incoming->TryGetField(Field);
        if (!Value || (Value->Type != EJson::String && Value->Type != EJson::Null)) return false;
    }
    FString Mode;
    if (!Incoming->TryGetStringField(TEXT("operation_mode"), Mode) ||
        (Mode != TEXT("MONITOR") && Mode != TEXT("PLAN_EDIT") && Mode != TEXT("MISSION_EXECUTION") && Mode != TEXT("ALERT_RESPONSE"))) return false;
    auto Previous = Context;
    Version = static_cast<int64>(Number); Context = Incoming; ApplySelection();
    auto Notify = [&](const TCHAR* Field, FOperationalFieldChanged& Event) {
        FString Before, After;
        if (Previous) Previous->TryGetStringField(Field, Before);
        Context->TryGetStringField(Field, After);
        if (Before != After) Event.Broadcast(After);
    };
    Notify(TEXT("active_uav_id"), OnActiveUAVChanged); Notify(TEXT("active_alert_id"), OnActiveAlertChanged);
    Notify(TEXT("active_mission_id"), OnActiveMissionChanged); Notify(TEXT("active_security_plan_id"), OnActiveSecurityPlanChanged);
    Notify(TEXT("active_area_id"), OnActiveAreaChanged); Notify(TEXT("operation_mode"), OnOperationModeChanged);
    UE_LOG(LogTemp, Log, TEXT("[P1][%s] ApplyRemote ActiveUAV=%d v%lld"), *Role,
        GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetPrimarySelectedDrone(), Version);
    OnContextChanged.Broadcast(Context); return true;
}
void UOperationalContextSubsystem::ApplySelection() {
    if (!Context || !bFollowGlobalSelection) return;
    FString Id; Context->TryGetStringField(TEXT("active_uav_id"), Id);
    int32 DroneId = 0;
    if (Id.StartsWith(TEXT("UAV-"))) LexTryParseString(DroneId, *Id.Mid(4));
    auto* Registry = GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
    if (DroneId > 0 && !Registry->IsDroneRegistered(DroneId)) return;
    TArray<int32> Selected;const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;
    if(Context->TryGetArrayField(TEXT("selected_uav_ids"),Values))for(const auto& V:*Values){int32 N=0;const auto S=V->AsString();if(S.StartsWith(TEXT("UAV-")) && LexTryParseString(N,*S.Mid(4)) && Registry->IsDroneRegistered(N))Selected.Add(N);}
    else if(DroneId>0)Selected.Add(DroneId);
    if(Registry->GetPrimarySelectedDrone()==DroneId && Registry->GetMultiSelectedDrones()==Selected)return;
    TGuardValue<bool> Guard(bApplying, true);
    if (DroneId == 0) Registry->ClearSelection(); else {Registry->SetMultiSelectedDrones(Selected);Registry->SetPrimarySelectedDrone(DroneId);}
}
bool UOperationalContextSubsystem::UpdateContext(const TSharedRef<FJsonObject>& Patch) {
    if (!IsReady()) return false;
    if (Patch->HasField(TEXT("active_uav_id")) || Patch->HasField(TEXT("selected_uav_ids"))) {
        if(bMapSelectionPending)return false;
        ++SelectionSerial;
    }
    auto Body = MakeShared<FJsonObject>(); Body->SetStringField(TEXT("instance_id"), InstanceId); Body->SetObjectField(TEXT("patch"), Patch);
    Request(TEXT("PATCH"), TEXT("/api/context"), Body, [this](TSharedPtr<FJsonObject> Reply) {
        if (Reply) ApplyContext(Reply); else Offline();
    }); return true;
}
bool UOperationalContextSubsystem::SetActiveUAV(int32 Id) {
    auto* Registry = GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
    TArray<int32> Multi = Registry->GetMultiSelectedDrones();
    if (Id <= 0) Multi.Empty();
    else if (!Multi.Contains(Id)) Multi = {Id};
    return SetLocalSelection(Id, Multi);
}
bool UOperationalContextSubsystem::SetLocalSelection(int32 Id, const TArray<int32>& Multi) {
    UE_LOG(LogTemp, Log, TEXT("[P1][%s] SetActiveUAV UAV-%02d"), *Role, Id);
    if (!IsReady()) return false;
    if(bMapSelectionPending)return false;
    const int64 Serial = ++SelectionSerial;
    auto Patch = MakeShared<FJsonObject>();
    if (Id > 0) Patch->SetStringField(TEXT("active_uav_id"), FString::Printf(TEXT("UAV-%02d"), Id));
    else Patch->SetField(TEXT("active_uav_id"), MakeShared<FJsonValueNull>());
    TArray<TSharedPtr<FJsonValue>> Selected;for(int32 N:Multi)Selected.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("UAV-%02d"),N)));
    Patch->SetArrayField(TEXT("selected_uav_ids"),Selected);
    auto Body = MakeShared<FJsonObject>(); Body->SetStringField(TEXT("instance_id"), InstanceId); Body->SetObjectField(TEXT("patch"), Patch);
    Request(TEXT("PATCH"), TEXT("/api/context"), Body, [this, Serial, Id, Multi](TSharedPtr<FJsonObject> Reply) {
        if (!Reply) { Offline(); return; }
        ApplyContext(Reply);
        // Commit the shared selection only after
        // the matching primary ACK, and never let an older ACK undo a later local action.
        double AckVersion = -1; Reply->TryGetNumberField(TEXT("context_version"), AckVersion);
        auto* Registry = GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
        if (Serial != SelectionSerial || AckVersion != Version || Registry->GetPrimarySelectedDrone() != Id) return;
        TGuardValue<bool> Guard(bApplying, true);
        Registry->SetMultiSelectedDrones(Multi);
    });
    return true;
}
bool UOperationalContextSubsystem::SetMapSelection(int32 Id,const TArray<int32>& Multi,int32 ClickedId) {
    if(Role!=TEXT("Map") || !IsHydrated() || bMapSelectionPending)return false;
    auto* Registry=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
    if(ClickedId<=0 || !Registry->IsDroneRegistered(ClickedId))return false;
    const int64 Serial=++SelectionSerial;
    const FString Target=FString::Printf(TEXT("UAV-%02d"),ClickedId);
    auto Patch=MakeShared<FJsonObject>();
    if(Id>0)Patch->SetStringField(TEXT("active_uav_id"),FString::Printf(TEXT("UAV-%02d"),Id));
    else Patch->SetField(TEXT("active_uav_id"),MakeShared<FJsonValueNull>());
    TArray<TSharedPtr<FJsonValue>> Members;
    for(int32 N:Multi)Members.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("UAV-%02d"),N)));
    Patch->SetArrayField(TEXT("selected_uav_ids"),Members);
    auto Body=MakeShared<FJsonObject>();Body->SetStringField(TEXT("instance_id"),InstanceId);Body->SetObjectField(TEXT("patch"),Patch);
    Body->SetNumberField(TEXT("map_selection_serial"),Serial);Body->SetStringField(TEXT("map_video_target_uav_id"),Target);
    bMapSelectionPending=true;ViewSyncError.Empty();
    Request(TEXT("PATCH"),TEXT("/api/context"),Body,[this,Serial,Id,Multi,Target](TSharedPtr<FJsonObject> Reply){
        if(Serial!=SelectionSerial)return;
        ApplyContext(Reply);
        // The full accepted selection must match, including same-primary
        // group -> singleton and no-op clicks. Never use local intent as ACK.
        if(!MapSelectionVideo::SelectionAckMatches(Reply,Version,Id,Multi)
            || !MapSelectionVideo::SelectionAckMatches(Context,Version,Id,Multi)) {
            bMapSelectionPending=false;
            ViewSyncError=PlanUI::Field(Reply,TEXT("code"))==TEXT("VIDEO_SELECTION_STALE")?TEXT("VIDEO_SELECTION_STALE"):TEXT("MAP_VIDEO_SYNC_FAILED");
            if(!Reply)Offline();return;
        }
        auto Video=MakeShared<FJsonObject>();Video->SetStringField(TEXT("instance_id"),InstanceId);
        Video->SetStringField(TEXT("video_target_uav_id"),Target);
        Video->SetNumberField(TEXT("expected_context_version"),Version);Video->SetNumberField(TEXT("map_selection_serial"),Serial);
        Request(TEXT("PATCH"),TEXT("/api/video-view"),Video,[this,Serial,Target](TSharedPtr<FJsonObject> Ack){
            if(Serial!=SelectionSerial)return;
            bMapSelectionPending=false;
            const bool Accepted=Ack && Ack->HasTypedField<EJson::Number>(TEXT("video_view_version")) && PlanUI::Field(Ack,TEXT("video_target_uav_id"))==Target;
            ViewSyncError=Accepted?TEXT(""):PlanUI::Field(Ack,TEXT("code"))==TEXT("VIDEO_SELECTION_STALE")?TEXT("VIDEO_SELECTION_STALE"):TEXT("MAP_VIDEO_SYNC_FAILED");
            // A newer WebSocket snapshot always wins over an older HTTP ACK.
            if(Accepted)ApplyVideoView(Ack);
        },true);
    },true);
    return true;
}
bool UOperationalContextSubsystem::SelectAlert(const FString& AlertId, int32 DroneId) {
    auto Patch = MakeShared<FJsonObject>(); Patch->SetStringField(TEXT("active_alert_id"), AlertId);
    Patch->SetStringField(TEXT("active_uav_id"), FString::Printf(TEXT("UAV-%02d"), DroneId));
    Patch->SetStringField(TEXT("operation_mode"), TEXT("ALERT_RESPONSE")); return UpdateContext(Patch);
}
FString UOperationalContextSubsystem::GetStatusText() const {
    FString Id; if (Context) Context->TryGetStringField(TEXT("active_uav_id"), Id);
    const FString Status=FText::Format(ProductText::Get(TEXT("Common.Sync")),ProductText::Source(IsReady()?TEXT("ONLINE"):TEXT("OFFLINE")),ProductText::Source(State),FText::AsCultureInvariant(Id),FText::AsNumber(Version)).ToString();
    return ViewSyncError.IsEmpty()?Status:Status+TEXT(" | ")+ProductText::Get(TEXT("Errors.")+ViewSyncError).ToString();
}

void UOperationalContextSubsystem::ApplyPlans(const TSharedPtr<FJsonObject>& Incoming) {
    double Next=-1,Previous=-1;
    if(!Incoming || !Incoming->TryGetNumberField(TEXT("version"),Next))return;
    if(Plans)Plans->TryGetNumberField(TEXT("version"),Previous);
    double E=-1,OldE=-1;Incoming->TryGetNumberField(TEXT("execution_version"),E);
    if(Plans)Plans->TryGetNumberField(TEXT("execution_version"),OldE);
    if(Next<=Previous && E<=OldE)return;
    auto Merged=MakeShared<FJsonObject>();Merged->Values=Incoming->Values;
    if(Plans && Next<Previous)Merged->Values=Plans->Values;
    if(Plans && E<OldE) {
        for(const TCHAR* K:{TEXT("execution_version"),TEXT("executions"),TEXT("execution_requests")})if(Plans->HasField(K))Merged->SetField(K,Plans->TryGetField(K));
        if(Plans->HasField(TEXT("mock_position_baseline")))Merged->SetField(TEXT("mock_position_baseline"),Plans->TryGetField(TEXT("mock_position_baseline")));
        else Merged->RemoveField(TEXT("mock_position_baseline"));
    } else if(Next<Previous) {
        for(const TCHAR* K:{TEXT("execution_version"),TEXT("executions"),TEXT("execution_requests")})if(Incoming->HasField(K))Merged->SetField(K,Incoming->TryGetField(K));
        if(Incoming->HasField(TEXT("mock_position_baseline")))Merged->SetField(TEXT("mock_position_baseline"),Incoming->TryGetField(TEXT("mock_position_baseline")));
        else Merged->RemoveField(TEXT("mock_position_baseline"));
    }
    if(Plans && Plans->GetArrayField(TEXT("events")).Num()>Incoming->GetArrayField(TEXT("events")).Num())Merged->SetArrayField(TEXT("events"),Plans->GetArrayField(TEXT("events")));
    Plans=Merged;
    {
        TSet<FString> Uavs;const auto All=PlanUI::Object(Plans,TEXT("executions"));
        if(All)for(const auto& Entry:All->Values)if(const auto Item=Entry.Value->AsObject())Uavs.Add(PlanUI::Field(Item,TEXT("uav_id")));
        const auto BaselinePositions=PlanUI::Object(PlanUI::Object(Plans,TEXT("mock_position_baseline")),TEXT("positions"));
        if(BaselinePositions)for(const auto& Entry:BaselinePositions->Values)Uavs.Add(FString(Entry.Key.ToView()));
        auto* Registry=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();auto* C=Registry->GetCoordinateService().GetObject();
        for(const auto& UAV:Uavs){
            const auto Item=ExecutionUI::PositionExecution(Plans,UAV);
            bool Simulation=false;if(Item && (!Item->TryGetBoolField(TEXT("simulation"),Simulation) || !Simulation))continue;
            const auto P=Item?PlanUI::Object(Item,TEXT("position")):ExecutionUI::MockBaselinePosition(Plans,UAV);
            int32 Id=0;LexTryParseString(Id,*UAV.Mid(4));if(!P || Id<=0)continue;
            FDroneTelemetrySnapshot T;Registry->GetTelemetry(Id,T);
            // An acknowledged Mock execution may position its simulated marker,
            // but must never replace received telemetry or invent hardware liveness.
            if(T.TelemetrySource==TEXT("received"))continue;
            T.DroneId=Id;T.bSimulation=true;T.TelemetrySource=Item?TEXT("mock_execution"):TEXT("mock_reset");T.bLocalPositionValid=false;
            T.GpsLatitude=P->GetNumberField(TEXT("latitude"));T.GpsLongitude=P->GetNumberField(TEXT("longitude"));T.GpsAltitude=P->GetNumberField(TEXT("altitude"));
            const auto Route=PlanUI::Object(Item,TEXT("route_snapshot"));const TArray<TSharedPtr<FJsonValue>>* Points=nullptr;
            if(Route && Route->TryGetArrayField(TEXT("waypoints"),Points) && !Points->IsEmpty()){
                const int Target=FMath::Clamp(int(Item->GetNumberField(TEXT("completed_waypoints"))),0,Points->Num()-1);const auto To=(*Points)[Target]->AsObject();
                const double North=To->GetNumberField(TEXT("latitude"))-T.GpsLatitude,East=(To->GetNumberField(TEXT("longitude"))-T.GpsLongitude)*FMath::Cos(FMath::DegreesToRadians(T.GpsLatitude));
                if(FMath::Abs(North)+FMath::Abs(East)>1e-10)T.Attitude.Yaw=FMath::RadiansToDegrees(FMath::Atan2(East,North));}
            T.GeographicLocation=FVector(T.GpsLatitude,T.GpsLongitude,T.GpsAltitude);T.Altitude=T.GpsAltitude;T.bGpsFix=true;
            if(C && ICoordinateService::Execute_IsCoordinateSystemReady(C)){
                T.WorldLocation=ICoordinateService::Execute_GeographicToWorld(C,T.GpsLatitude,T.GpsLongitude,T.GpsAltitude);
                if(auto* Mirror=Cast<ARealTimeDroneReceiver>(Registry->GetReceiverActor(Id)))Mirror->ApplySimulationPosition(T.WorldLocation);
            }
            Registry->UpdateTelemetry(Id,T);
        }
    }
    GetGameInstance()->GetSubsystem<UOperationalEventStore>()->ProjectPlanEvents(Plans);
    OnPlansChanged.Broadcast(Plans);
}
bool UOperationalContextSubsystem::SubmitPlan(const TSharedRef<FJsonObject>& Body,TFunction<void(TSharedPtr<FJsonObject>)> Complete) {
    if(!IsReady())return false;
    Body->SetStringField(TEXT("instance_id"),InstanceId);
    if(Plans && !Body->HasField(TEXT("expected_version")))Body->SetNumberField(TEXT("expected_version"),Plans->GetNumberField(TEXT("version")));
    auto Req=FHttpModule::Get().CreateRequest();Req->SetURL(HttpUrl+TEXT("/api/security-plans"));Req->SetVerb(TEXT("POST"));Req->SetTimeout(10);
    Req->SetHeader(TEXT("Content-Type"),TEXT("application/json"));Req->SetContentAsString(Encode(Body));
    const int32 Attempt=Generation;
    Req->OnProcessRequestComplete().BindWeakLambda(this,[this,Attempt,Complete](FHttpRequestPtr,FHttpResponsePtr Response,bool Ok){
        if(bStopped || Generation!=Attempt){Complete(nullptr);return;}
        TSharedPtr<FJsonObject> Reply;
        if(Ok && Response)FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Response->GetContentAsString()),Reply);
        if(Response && (Response->GetResponseCode()<200 || Response->GetResponseCode()>=300)){
            if(!Reply)Reply=MakeShared<FJsonObject>();
            if(!Reply->HasField(TEXT("code")))Reply->SetStringField(TEXT("code"),Response->GetResponseCode()==403?TEXT("ROLE_FORBIDDEN"):TEXT("REQUEST_REJECTED"));
        }
        if(Reply) {
            const TSharedPtr<FJsonObject>* State;
            if(Reply->TryGetObjectField(TEXT("state"),State))ApplyPlans(*State);
            if(Reply->TryGetObjectField(TEXT("context"),State))ApplyContext(*State);
        }
        Complete(Reply);
    });
    return Req->ProcessRequest();
}
namespace {
bool NewDomainVersion(const TSharedPtr<FJsonObject>& Incoming,const TSharedPtr<FJsonObject>& Current,const TCHAR* Key) {
    double N,Old=-1;if(!Incoming || !Incoming->TryGetNumberField(Key,N) || !FMath::IsFinite(N) || N<0 || N!=FMath::FloorToDouble(N))return false;
    if(Current)Current->TryGetNumberField(Key,Old);return N>Old;
}
}
bool UOperationalContextSubsystem::ApplyUIPreferences(const TSharedPtr<FJsonObject>& Incoming) {
    if(!NewDomainVersion(Incoming,UIPreferences,TEXT("ui_preferences_version")))return false;
    FString Language;if(!Incoming->TryGetStringField(TEXT("language"),Language) || (Language!=TEXT("en") && Language!=TEXT("zh-Hans")))return false;
    UIPreferences=Incoming;GetGameInstance()->GetSubsystem<UUILanguageSubsystem>()->ApplyConfirmed(Language);
    OnUIPreferencesChanged.Broadcast(Incoming);return true;
}
bool UOperationalContextSubsystem::ApplyVideoView(const TSharedPtr<FJsonObject>& Incoming) {
    if(!NewDomainVersion(Incoming,VideoView,TEXT("video_view_version")))return false;
    const auto V=Incoming->TryGetField(TEXT("video_target_uav_id"));if(!V || (V->Type!=EJson::Null && V->Type!=EJson::String))return false;
    VideoView=Incoming;OnVideoViewChanged.Broadcast(Incoming);return true;
}
bool UOperationalContextSubsystem::SetLanguage(const FString& Language) {
    if(!IsReady()){ViewSyncError=TEXT("SYNC_OFFLINE");return false;}
    auto Body=MakeShared<FJsonObject>();Body->SetStringField(TEXT("instance_id"),InstanceId);Body->SetStringField(TEXT("language"),Language);
    Request(TEXT("PATCH"),TEXT("/api/ui-preferences"),Body,[this](TSharedPtr<FJsonObject> R){ViewSyncError=R && R->HasField(TEXT("ui_preferences_version"))?TEXT(""):TEXT("SYNC_FAILED");ApplyUIPreferences(R);});return true;
}
bool UOperationalContextSubsystem::OpenVideo(const FString& UAV) {
    if(bMapSelectionPending)return false;
    if(!IsReady()){ViewSyncError=TEXT("SYNC_OFFLINE");return false;}
    auto Body=MakeShared<FJsonObject>();Body->SetStringField(TEXT("instance_id"),InstanceId);
    if(UAV.IsEmpty())Body->SetField(TEXT("video_target_uav_id"),MakeShared<FJsonValueNull>());else Body->SetStringField(TEXT("video_target_uav_id"),UAV);
    Request(TEXT("PATCH"),TEXT("/api/video-view"),Body,[this](TSharedPtr<FJsonObject> R){ViewSyncError=R && R->HasField(TEXT("video_view_version"))?TEXT(""):TEXT("SYNC_FAILED");ApplyVideoView(R);});return true;
}

bool UOperationalContextSubsystem::IsRoleOnline(const FString& ClientRole) const {
    if(!IsHydrated())return false;
    const double Now=FDateTime::UtcNow().ToUnixTimestamp();
    for(const auto& E:Clients) {
        FString R,S;bool Ready=false;double Seen=0;
        E.Value->TryGetStringField(TEXT("client_role"),R);E.Value->TryGetStringField(TEXT("state"),S);
        E.Value->TryGetBoolField(TEXT("hydrated"),Ready);E.Value->TryGetNumberField(TEXT("last_seen"),Seen);
        if(R==ClientRole && S==TEXT("ONLINE") && Ready && Now-Seen<12)return true;
    }return false;
}
FString UOperationalContextSubsystem::GetSystemState() const {
    if(!IsReady())return TEXT("OFFLINE");
    return IsRoleOnline(TEXT("Command")) && IsRoleOnline(TEXT("Map")) && IsRoleOnline(TEXT("Video"))?TEXT("READY"):TEXT("DEGRADED");
}
