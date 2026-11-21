#include "Shared/PilotControlSubsystem.h"

#include "Command/CommandScreenManager.h"
#include "Dom/JsonObject.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "IWebSocket.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Serialization/JsonSerializer.h"
#include "WebSocketsModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogPilotControl, Log, All);

namespace
{
const TCHAR* ControlPath = TEXT("/api/pilot-control");

// Local send bounds apply to the Mock adapter. The server also fences expired windows.
constexpr double MockSendInterval = 0.05;
constexpr double MockInputAge = 0.20;
constexpr double MockWindowAge = 0.35;

const TCHAR* ControlActionName(EPilotControlAction Action)
{
    switch (Action)
    {
        case EPilotControlAction::Takeoff: return TEXT("takeoff");
        case EPilotControlAction::Land: return TEXT("land");
        case EPilotControlAction::Hold: return TEXT("hold");
        case EPilotControlAction::GoHome: return TEXT("gohome");
        case EPilotControlAction::EmergencyStop: return TEXT("estop");
        default: return nullptr;
    }
}

FString PilotString(const TSharedPtr<FJsonObject>& Json, const TCHAR* Key)
{
    FString Value;
    if (Json) Json->TryGetStringField(Key, Value);
    return Value;
}

bool ReadPilotCounter(const TSharedPtr<FJsonObject>& Json, const TCHAR* Key, int64& Out)
{
    double Value = 0.;
    if (!Json || !Json->TryGetNumberField(Key, Value) || !FMath::IsFinite(Value) || Value < 0. ||
        Value > 9007199254740991. || Value != FMath::FloorToDouble(Value)) return false;
    Out = static_cast<int64>(Value);
    return true;
}

FString EncodePilotEnvelope(const TSharedRef<FJsonObject>& Json)
{
    FString Text;
    FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Text));
    return Text;
}

void WritePilotCounter(const TSharedRef<FJsonObject>& Json, const FString& Key, int64 Value)
{
    Json->SetField(Key, MakeShared<FJsonValueNumberString>(LexToString(Value)));
}
}

void UPilotControlSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    const EDroneClientRole ClientRole = UCommandScreenManager::ResolveClientRole();
    if (ClientRole != EDroneClientRole::Command && ClientRole != EDroneClientRole::Pilot) return;

    Role = ClientRole == EDroneClientRole::Pilot ? TEXT("Pilot") : TEXT("Command");
    FParse::Value(FCommandLine::Get(), TEXT("PilotControlReadyFile="), ReadyFile);
    WriteReadyMarker(false);
    FString Identity;
    FParse::Value(FCommandLine::Get(), TEXT("PilotControlIdentity="), Identity);
    bEnabled = ClientRole == EDroneClientRole::Pilot || !Identity.IsEmpty();
    bConfigured = LoadCredentials(Identity);
    if (!bConfigured) return;

    HttpUrl = TEXT("http://127.0.0.1:8080");
    WsUrl = TEXT("ws://127.0.0.1:8081/ws");
    GConfig->GetString(TEXT("/Script/UE5DroneControl.DroneNetworkManager"), TEXT("BackendBaseUrl"), HttpUrl, GGameIni);
    GConfig->GetString(TEXT("/Script/UE5DroneControl.DroneNetworkManager"), TEXT("WebSocketUrl"), WsUrl, GGameIni);
    GConfig->GetString(TEXT("OperationalContext"), TEXT("BackendBaseUrl"), HttpUrl, GGameIni);
    GConfig->GetString(TEXT("OperationalContext"), TEXT("WebSocketUrl"), WsUrl, GGameIni);
    FParse::Value(FCommandLine::Get(), TEXT("P1Http="), HttpUrl);
    FParse::Value(FCommandLine::Get(), TEXT("P1Ws="), WsUrl);
    const FString HttpHost = FGenericPlatformHttp::GetUrlDomain(HttpUrl);
    if ((!HttpUrl.StartsWith(TEXT("http://")) && !HttpUrl.StartsWith(TEXT("https://"))) ||
        (!WsUrl.StartsWith(TEXT("ws://")) && !WsUrl.StartsWith(TEXT("wss://"))) ||
        HttpHost.IsEmpty() || !HttpHost.Equals(FGenericPlatformHttp::GetUrlDomain(WsUrl), ESearchCase::IgnoreCase))
    {
        StatusCode = TEXT("INVALID_ENDPOINTS");
        bConfigured = false;
        Authorization.Reset();
        return;
    }
    HttpUrl = FGenericPlatformHttp::GetUrlBase(HttpUrl);
    WsUrl = FGenericPlatformHttp::GetUrlBase(WsUrl) + TEXT("/ws/pilot-control");
    TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UPilotControlSubsystem::Tick), 0.02f);
}

void UPilotControlSubsystem::Deinitialize()
{
    bStopped = true;
    ++Generation;
    if (TickHandle.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    WriteReadyMarker(false);
    ClearControlInput();
    CloseSocket();
    Authorization.Reset();
    Super::Deinitialize();
}

bool UPilotControlSubsystem::LoadCredentials(const FString& Identity)
{
    const FString Path = FPlatformMisc::GetEnvironmentVariable(TEXT("DRONE_PILOT_CONTROL_CREDENTIALS"));
    if (Identity.IsEmpty() || Path.IsEmpty()) return false;

    FString Raw;
    TSharedPtr<FJsonObject> Document;
    const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
    StatusCode = TEXT("CREDENTIALS_INVALID");
    if (!FFileHelper::LoadFileToString(Raw, *Path) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), Document) || !Document ||
        !Document->TryGetArrayField(TEXT("credentials"), Entries)) return false;

    for (const auto& Entry : *Entries)
    {
        if (!Entry || Entry->Type != EJson::Object) continue;
        const auto Item = Entry->AsObject();
        if (PilotString(Item, TEXT("id")) != Identity || PilotString(Item, TEXT("role")) != Role) continue;
        const FString Token = PilotString(Item, TEXT("token"));
        if (Token.IsEmpty() || Token.Len() > 512 || Token.Contains(TEXT(" ")) || Token.Contains(TEXT("\r")) ||
            Token.Contains(TEXT("\n")) || Token.Contains(TEXT("\t"))) return false;
        Authorization = TEXT("Bearer ") + Token;
        return true;
    }
    return false;
}

bool UPilotControlSubsystem::Tick(float)
{
    if (bStopped) return false;
    if (!bConfigured) return true;
    const double Now = FPlatformTime::Seconds();
    if (bConnecting && Now >= ConnectDeadline) Disconnect(TEXT("BACKEND_UNAVAILABLE"));
    if (bReady && (!Socket || !Socket->IsConnected())) Disconnect(TEXT("BACKEND_UNAVAILABLE"));
    if (bReady && PendingVersion >= 0 && Now >= PendingDeadline) Disconnect(TEXT("REQUEST_UNCERTAIN"));
    if (!bReady && !bConnecting && Now >= NextConnect) Connect();
    if (bReady) SendInput(Now);
    return true;
}

void UPilotControlSubsystem::Request(const TCHAR* Verb, const FString& Path, const TSharedPtr<FJsonObject>& Body,
    bool bAuthenticate, FResponse Complete)
{
    auto Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(HttpUrl + Path);
    Req->SetVerb(Verb);
    Req->SetTimeout(3.f);
    if (bAuthenticate) Req->SetHeader(TEXT("Authorization"), Authorization);
    if (Body)
    {
        Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
        Req->SetContentAsString(EncodePilotEnvelope(Body.ToSharedRef()));
    }
    const int32 Attempt = Generation;
    Req->OnProcessRequestComplete().BindWeakLambda(this,
        [this, Attempt, Complete](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
        {
            if (bStopped || Generation != Attempt) return;
            TSharedPtr<FJsonObject> Reply;
            if (bConnected && Response)
            {
                FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Response->GetContentAsString()), Reply);
                Complete(Response->GetResponseCode(), Reply);
            }
            else Complete(0, nullptr);
        });
    if (!Req->ProcessRequest() && Generation == Attempt) Complete(0, nullptr);
}

void UPilotControlSubsystem::Connect()
{
    bConnecting = true;
    ConnectDeadline = FPlatformTime::Seconds() + 10.;
    const int32 Attempt = ++Generation;
    StatusCode = TEXT("CONNECTING");
    OnChanged.Broadcast();
    Request(TEXT("GET"), TEXT("/"), nullptr, false,
        [this, Attempt](int32 Status, const TSharedPtr<FJsonObject>& Response)
        {
            if (Status != 200 || !Response)
            {
                Disconnect(TEXT("BACKEND_UNAVAILABLE"));
                return;
            }
            const TArray<TSharedPtr<FJsonValue>>* Capabilities = nullptr;
            Response->TryGetArrayField(TEXT("capabilities"), Capabilities);
            const auto HasCapability = [Capabilities](const TCHAR* Name)
            {
                return Capabilities && Capabilities->ContainsByPredicate([Name](const TSharedPtr<FJsonValue>& Value)
                {
                    return Value && Value->Type == EJson::String && Value->AsString() == Name;
                });
            };
            if (!HasCapability(TEXT("pilot_control_mock")))
            {
                Disconnect(TEXT("BACKEND_NOT_MOCK"));
                return;
            }
            bControlActions = HasCapability(TEXT("pilot_control_actions"));
            Request(TEXT("GET"), ControlPath, nullptr, true,
                [this, Attempt](int32 ControlStatus, const TSharedPtr<FJsonObject>& Snapshot)
                {
                    if (ControlStatus == 401 || ControlStatus == 403) Disconnect(TEXT("CREDENTIALS_INVALID"));
                    else if (ControlStatus != 200 || !Snapshot) Disconnect(TEXT("BACKEND_UNAVAILABLE"));
                    else OpenSocket(Attempt);
                });
        });
}

void UPilotControlSubsystem::OpenSocket(int32 Attempt)
{
    const TMap<FString, FString> Headers{{TEXT("Authorization"), Authorization}};
    Socket = FWebSocketsModule::Get().CreateWebSocket(WsUrl, FString(), Headers);
    Socket->OnConnected().AddWeakLambda(this, [this, Attempt]
    {
        if (Generation == Attempt && Socket) Socket->Send(TEXT("{\"type\":\"subscribe_pilot_control\"}"));
    });
    Socket->OnMessage().AddWeakLambda(this, [this, Attempt](const FString& Message)
    {
        if (Generation == Attempt) Receive(Message);
    });
    Socket->OnConnectionError().AddWeakLambda(this, [this, Attempt](const FString&)
    {
        if (Generation == Attempt) Disconnect(TEXT("BACKEND_UNAVAILABLE"));
    });
    Socket->OnClosed().AddWeakLambda(this, [this, Attempt](int32, const FString&, bool)
    {
        if (Generation == Attempt) Disconnect(TEXT("BACKEND_UNAVAILABLE"));
    });
    Socket->Connect();
}

void UPilotControlSubsystem::CloseSocket()
{
    if (!Socket) return;
    const auto Closing = MoveTemp(Socket);
    Closing->OnConnected().Clear();
    Closing->OnConnectionError().Clear();
    Closing->OnClosed().Clear();
    Closing->OnMessage().Clear();
    Closing->Close();
}

void UPilotControlSubsystem::Disconnect(const TCHAR* Code)
{
    const bool bWasReady = bReady;
    ++Generation;
    bReady = bConnecting = false;
    bControlActions = false;
    ConnectionId.Reset();
    ServerEpoch.Reset();
    ActiveUavId.Reset();
    ActiveSessionId.Reset();
    SuppressedSessionId.Reset();
    for (auto& Row : Aircraft) Row.ControlSessionId.Reset();
    WindowId = Sequence = 0;
    ClearControlInput();
    PendingBody.Reset();
    Version = PendingVersion = -1;
    StatusCode = Code;
    NextConnect = FPlatformTime::Seconds() + 1.5;
    CloseSocket();
    WriteReadyMarker(false);
    if (bWasReady) UE_LOG(LogPilotControl, Log, TEXT("[PilotControl] %s disconnected"), *Role);
    OnChanged.Broadcast();
}

void UPilotControlSubsystem::WriteReadyMarker(bool bPresent) const
{
    if (ReadyFile.IsEmpty()) return;
    if (!bPresent)
    {
        IFileManager::Get().Delete(*ReadyFile, false, true, true);
        return;
    }
    if (!FFileHelper::SaveStringToFile(TEXT("ready\n"), *ReadyFile))
        UE_LOG(LogPilotControl, Warning, TEXT("[PilotControl] Unable to update launcher readiness marker"));
}

void UPilotControlSubsystem::Receive(const FString& Text)
{
    TSharedPtr<FJsonObject> Message;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Message)) return;
    const FString Type = PilotString(Message, TEXT("type"));
    const bool bInitial = Type == TEXT("pilot_control_subscribed");
    if (bInitial || Type == TEXT("pilot_control_changed"))
    {
        if (bInitial && !bConnecting) return;
        if (!bInitial && !bReady) return;
        const TSharedPtr<FJsonObject>* Payload = nullptr;
        if (bInitial) ConnectionId = PilotString(Message, TEXT("connection_id"));
        if (ConnectionId.IsEmpty() || !Message->TryGetObjectField(TEXT("payload"), Payload) ||
            !ApplySnapshot(*Payload, bInitial))
        {
            Disconnect(TEXT("INVALID_SNAPSHOT"));
        }
        return;
    }
    if (Type == TEXT("pilot_input_window"))
    {
        int64 OfferedWindow = 0;
        if (bReady && !ActiveSessionId.IsEmpty() && PilotString(Message, TEXT("control_session_id")) == ActiveSessionId &&
            ReadPilotCounter(Message, TEXT("window_id"), OfferedWindow) && OfferedWindow > WindowId)
        {
            WindowId = OfferedWindow;
            WindowReceivedAt = FPlatformTime::Seconds();
        }
        return;
    }
    if (Type == TEXT("pilot_input_rejected") && !ActiveSessionId.IsEmpty() && !IsRequestPending() &&
        StatusCode != TEXT("INPUT_REJECTED"))
    {
        StatusCode = TEXT("INPUT_REJECTED");
        OnChanged.Broadcast();
    }
}

bool UPilotControlSubsystem::ApplySnapshot(const TSharedPtr<FJsonObject>& Snapshot, bool bInitial)
{
    const FString Epoch = PilotString(Snapshot, TEXT("server_epoch"));
    int64 IncomingVersion = -1;
    const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* PilotRows = nullptr;
    if (Epoch.IsEmpty() || !ReadPilotCounter(Snapshot, TEXT("version"), IncomingVersion) ||
        !Snapshot->TryGetArrayField(TEXT("aircraft"), Rows) ||
        !Snapshot->TryGetArrayField(TEXT("pilots"), PilotRows) ||
        (!bInitial && ServerEpoch != Epoch)) return false;
    if (!bInitial && IncomingVersion <= Version) return true;

    TArray<FPilotControlAircraft> NewAircraft;
    FString NewActiveUav;
    FString NewSession;
    for (const auto& Value : *Rows)
    {
        if (!Value || Value->Type != EJson::Object) return false;
        const auto Row = Value->AsObject();
        FPilotControlAircraft AircraftRow;
        AircraftRow.UavId = PilotString(Row, TEXT("uav_id"));
        AircraftRow.PilotId = PilotString(Row, TEXT("pilot_id"));
        AircraftRow.GrantId = PilotString(Row, TEXT("grant_id"));
        AircraftRow.ControlState = PilotString(Row, TEXT("control_state"));
        AircraftRow.AircraftState = PilotString(Row, TEXT("aircraft_state"));
        AircraftRow.Reason = PilotString(Row, TEXT("reason"));
        AircraftRow.ActivationReason = PilotString(Row, TEXT("activation_reason"));
        AircraftRow.VideoUrl = PilotString(Row, TEXT("video_url"));
        AircraftRow.ControlSessionId = PilotString(Row, TEXT("control_session_id"));
        Row->TryGetBoolField(TEXT("input_frozen"), AircraftRow.bInputFrozen);
        if (AircraftRow.UavId.IsEmpty() || !ReadPilotCounter(Row, TEXT("revision"), AircraftRow.Revision) ||
            !Row->TryGetBoolField(TEXT("available"), AircraftRow.bAvailable)) return false;
        if (AircraftRow.ControlState == TEXT("ACTIVE") && !AircraftRow.ControlSessionId.IsEmpty())
        {
            if (Role != TEXT("Pilot") || !NewSession.IsEmpty()) return false;
            NewActiveUav = AircraftRow.UavId;
            NewSession = AircraftRow.ControlSessionId;
        }
        NewAircraft.Add(MoveTemp(AircraftRow));
    }

    TArray<FPilotControlPilot> NewPilots;
    for (const auto& Value : *PilotRows)
    {
        if (!Value || Value->Type != EJson::Object) return false;
        const auto Row = Value->AsObject();
        FPilotControlPilot Pilot;
        Pilot.PilotId = PilotString(Row, TEXT("pilot_id"));
        if (Pilot.PilotId.IsEmpty() || !Row->TryGetBoolField(TEXT("online"), Pilot.bOnline)) return false;
        NewPilots.Add(MoveTemp(Pilot));
    }

    ServerEpoch = Epoch;
    Version = IncomingVersion;
    Aircraft = MoveTemp(NewAircraft);
    Pilots = MoveTemp(NewPilots);
    ActiveUavId = MoveTemp(NewActiveUav);
    if (NewSession != ActiveSessionId)
    {
        ActiveSessionId = MoveTemp(NewSession);
        SuppressedSessionId.Reset();
        WindowId = Sequence = 0;
        ClearControlInput();
    }
    const TSharedPtr<FJsonObject>* Exit = nullptr;
    LastExitReason = Snapshot->TryGetObjectField(TEXT("last_exit"), Exit) ? PilotString(*Exit, TEXT("reason")) : FString();
    if (bInitial)
    {
        bReady = true;
        bConnecting = false;
        StatusCode = TEXT("READY");
        WriteReadyMarker(true);
        UE_LOG(LogPilotControl, Log, TEXT("[PilotControl] %s ready"), *Role);
    }
    FinalizeOperationIfSynchronized();
    OnChanged.Broadcast();
    return true;
}

const FPilotControlAircraft* UPilotControlSubsystem::FindAircraft(const FString& UavId) const
{
    return Aircraft.FindByPredicate([&UavId](const FPilotControlAircraft& Row) { return Row.UavId == UavId; });
}

bool UPilotControlSubsystem::Grant(const FString& PilotId, const TArray<FString>& UavIds)
{
    if (Role != TEXT("Command") || PilotId.IsEmpty() || UavIds.IsEmpty() || UavIds.Num() > 64) return false;
    auto Body = MakeShared<FJsonObject>();
    auto Revisions = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Ids;
    for (const FString& UavId : UavIds)
    {
        const auto* Row = FindAircraft(UavId);
        if (!Row || Revisions->HasField(UavId)) return false;
        WritePilotCounter(Revisions, UavId, Row->Revision);
        Ids.Add(MakeShared<FJsonValueString>(UavId));
    }
    Body->SetStringField(TEXT("pilot_id"), PilotId);
    Body->SetArrayField(TEXT("uav_ids"), MoveTemp(Ids));
    Body->SetObjectField(TEXT("expected_revisions"), Revisions);
    return BeginOperation(TEXT("grant"), Body);
}

bool UPilotControlSubsystem::Revoke(const FString& UavId)
{
    const auto* Row = FindAircraft(UavId);
    if (Role != TEXT("Command") || !Row || Row->GrantId.IsEmpty()) return false;
    auto Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("uav_id"), UavId);
    Body->SetStringField(TEXT("grant_id"), Row->GrantId);
    return BeginOperation(TEXT("revoke"), Body);
}

bool UPilotControlSubsystem::Activate(const FString& UavId)
{
    const auto* Row = FindAircraft(UavId);
    if (Role != TEXT("Pilot") || !Row || Row->GrantId.IsEmpty() || ConnectionId.IsEmpty() ||
        !ActiveSessionId.IsEmpty() || !Row->ActivationReason.IsEmpty()) return false;
    auto Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("uav_id"), UavId);
    Body->SetStringField(TEXT("grant_id"), Row->GrantId);
    Body->SetStringField(TEXT("connection_id"), ConnectionId);
    WritePilotCounter(Body, TEXT("expected_revision"), Row->Revision);
    return BeginOperation(TEXT("activate"), Body);
}

bool UPilotControlSubsystem::Stop(const FString& Reason)
{
    if (Role != TEXT("Pilot")) return false;
    ClearControlInput();
    if (ActiveSessionId.IsEmpty()) return false;
    SuppressedSessionId = ActiveSessionId;
    auto Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("control_session_id"), ActiveSessionId);
    Body->SetStringField(TEXT("reason"), Reason.IsEmpty() ? TEXT("OPERATOR_STOP") : Reason.Left(128));
    return BeginOperation(TEXT("stop"), Body);
}

bool UPilotControlSubsystem::BeginOperation(const TCHAR* Action, const TSharedRef<FJsonObject>& Body)
{
    if (!bReady || IsRequestPending()) return false;
    Body->SetStringField(TEXT("action"), Action);
    Body->SetStringField(TEXT("request_id"), FGuid::NewGuid().ToString(EGuidFormats::Digits));
    PendingBody = Body;
    PendingVersion = -1;
    RetryCount = 0;
    LastResults.Reset();
    StatusCode = TEXT("REQUEST_PENDING");
    OnChanged.Broadcast();
    SendOperation();
    return true;
}

void UPilotControlSubsystem::SendOperation()
{
    if (!PendingBody || !bReady) return;
    Request(TEXT("POST"), ControlPath, PendingBody, true,
        [this](int32 Code, const TSharedPtr<FJsonObject>& Reply) { CompleteOperation(Code, Reply); });
}

void UPilotControlSubsystem::CompleteOperation(int32 ResponseCode, const TSharedPtr<FJsonObject>& Reply)
{
    if (!PendingBody) return;
    if (ResponseCode == 0 || ResponseCode >= 500)
    {
        if (RetryCount++ == 0) SendOperation();  // Same request_id reconciles uncertain writes.
        else Disconnect(TEXT("REQUEST_UNCERTAIN"));
        return;
    }
    if (ResponseCode < 200 || ResponseCode >= 300)
    {
        StatusCode = PilotString(Reply, TEXT("code"));
        if (StatusCode.IsEmpty()) StatusCode = TEXT("REQUEST_REJECTED");
        PendingBody.Reset();
        OnChanged.Broadcast();
        return;
    }
    const TArray<TSharedPtr<FJsonValue>>* Results = nullptr;
    if (!ReadPilotCounter(Reply, TEXT("version"), PendingVersion) ||
        PilotString(Reply, TEXT("server_epoch")) != ServerEpoch ||
        PilotString(Reply, TEXT("request_id")) != PilotString(PendingBody, TEXT("request_id")) ||
        !Reply->TryGetArrayField(TEXT("results"), Results))
    {
        Disconnect(TEXT("REQUEST_UNCERTAIN"));
        return;
    }

    LastResults.Reset();
    for (const auto& Value : *Results)
    {
        if (!Value || Value->Type != EJson::Object) continue;
        const auto Row = Value->AsObject();
        FPilotControlResult Result;
        Result.UavId = PilotString(Row, TEXT("uav_id"));
        Result.Code = PilotString(Row, TEXT("code"));
        Row->TryGetBoolField(TEXT("ok"), Result.bOk);
        LastResults.Add(MoveTemp(Result));
    }
    const auto* Failure = LastResults.FindByPredicate([](const FPilotControlResult& Row) { return !Row.bOk; });
    PendingStatusCode = Failure ? Failure->Code : LastResults.IsEmpty() ? TEXT("READY") : LastResults[0].Code;
    PendingDeadline = FPlatformTime::Seconds() + 3.;
    FinalizeOperationIfSynchronized();
    OnChanged.Broadcast();
}

void UPilotControlSubsystem::FinalizeOperationIfSynchronized()
{
    if (!PendingBody || PendingVersion < 0 || Version < PendingVersion) return;
    UE_LOG(LogPilotControl, Log, TEXT("[PilotControl] %s operation %s: %s"), *Role,
        *PilotString(PendingBody, TEXT("action")), *PendingStatusCode);
    PendingBody.Reset();
    PendingVersion = -1;
    StatusCode = MoveTemp(PendingStatusCode);
}

void UPilotControlSubsystem::SetControlInput(const FPilotControlAxes& Axes)
{
    if (Role != TEXT("Pilot") || !bReady || ActiveSessionId.IsEmpty() || ActiveSessionId == SuppressedSessionId) return;
    if (!FMath::IsFinite(Axes.Forward) || !FMath::IsFinite(Axes.Right) ||
        !FMath::IsFinite(Axes.Vertical) || !FMath::IsFinite(Axes.Yaw))
    {
        ClearControlInput();
        return;
    }
    Sample = {
        FMath::Clamp(Axes.Forward, -1.f, 1.f), FMath::Clamp(Axes.Right, -1.f, 1.f),
        FMath::Clamp(Axes.Vertical, -1.f, 1.f), FMath::Clamp(Axes.Yaw, -1.f, 1.f)};
    SampleTime = FPlatformTime::Seconds();
}

void UPilotControlSubsystem::ClearControlInput()
{
    Sample = {};
    SampleTime = 0.;
    PendingAction = EPilotControlAction::None;
    PendingActionGuard.Unbind();
    PendingActionAt = 0.;
}

bool UPilotControlSubsystem::QueueControlAction(EPilotControlAction Action, FPilotControlActionGuard Guard)
{
    const auto* Row = FindAircraft(ActiveUavId);
    const double Now = FPlatformTime::Seconds();
    if (!SupportsControlActions() || Role != TEXT("Pilot") || IsRequestPending() || !Row || Row->bInputFrozen ||
        ActiveSessionId.IsEmpty() || ActiveSessionId == SuppressedSessionId || !ControlActionName(Action) || !Guard.IsBound()) return false;
    if (Action != EPilotControlAction::EmergencyStop && PendingAction != EPilotControlAction::None &&
        Now - PendingActionAt <= MockInputAge) return false;
    PendingAction = Action;
    PendingActionGuard = MoveTemp(Guard);
    PendingActionAt = Now;
    return true;
}

void UPilotControlSubsystem::SendInput(double Now)
{
    if (Role != TEXT("Pilot") || ActiveSessionId.IsEmpty() || ActiveSessionId == SuppressedSessionId || WindowId <= 0 ||
        SampleTime <= 0. || Now - SampleTime > MockInputAge || Now - WindowReceivedAt > MockWindowAge || Now < NextInput) return;
    NextInput = Now + MockSendInterval;
    auto Input = MakeShared<FJsonObject>();
    auto Axes = MakeShared<FJsonObject>();
    const auto* Row = FindAircraft(ActiveUavId);
    const bool bFrozen = Row && Row->bInputFrozen;
    const auto* Action = ControlActionName(PendingAction);
    const bool bSendAction = bControlActions && !bFrozen && Action && Now - PendingActionAt <= MockInputAge &&
        PendingActionGuard.IsBound() && PendingActionGuard.Execute(Now);
    const FPilotControlAxes WireAxes = bFrozen || (bSendAction && PendingAction == EPilotControlAction::EmergencyStop)
        ? FPilotControlAxes{} : Sample;
    Axes->SetNumberField(TEXT("forward"), WireAxes.Forward);
    Axes->SetNumberField(TEXT("right"), WireAxes.Right);
    Axes->SetNumberField(TEXT("vertical"), WireAxes.Vertical);
    Axes->SetNumberField(TEXT("yaw"), WireAxes.Yaw);
    Input->SetStringField(TEXT("type"), TEXT("pilot_input"));
    Input->SetStringField(TEXT("uav_id"), ActiveUavId);
    Input->SetStringField(TEXT("control_session_id"), ActiveSessionId);
    WritePilotCounter(Input, TEXT("window_id"), WindowId);
    WritePilotCounter(Input, TEXT("seq"), ++Sequence);
    Input->SetObjectField(TEXT("axes"), Axes);
    if (bSendAction) Input->SetStringField(TEXT("action"), Action);
    PendingAction = EPilotControlAction::None;
    PendingActionGuard.Unbind();
    PendingActionAt = 0.;
    Socket->Send(EncodePilotEnvelope(Input));
}
