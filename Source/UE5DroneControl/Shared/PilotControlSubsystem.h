#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PilotControlSubsystem.generated.h"

class FJsonObject;
class IWebSocket;

struct FPilotControlAircraft
{
    FString UavId;
    FString PilotId;
    FString GrantId;
    FString ControlState;
    FString AircraftState;
    FString Reason;
    FString ActivationReason;
    FString VideoUrl;
    FString ControlSessionId;
    int64 Revision = 0;
    bool bAvailable = false;
    bool bInputFrozen = false;
};

struct FPilotControlPilot
{
    FString PilotId;
    bool bOnline = false;
};

struct FPilotControlResult
{
    FString UavId;
    FString Code;
    bool bOk = false;
};

struct FPilotControlAxes
{
    float Forward = 0.f;
    float Right = 0.f;
    float Vertical = 0.f;
    float Yaw = 0.f;
};

enum class EPilotControlAction : uint8 { None, Takeoff, Land, Hold, GoHome, EmergencyStop };

DECLARE_MULTICAST_DELEGATE(FPilotControlChanged);
DECLARE_DELEGATE_RetVal_OneParam(bool, FPilotControlActionGuard, double);

/** Role-scoped replica and input sender for an explicitly enabled Mock Backend. */
UCLASS()
class UE5DRONECONTROL_API UPilotControlSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    bool IsEnabled() const { return bEnabled; }
    bool IsReady() const { return bReady; }
    bool SupportsControlActions() const { return bReady && bControlActions; }
    bool IsRequestPending() const { return PendingBody.IsValid(); }
    const FString& GetStatusCode() const { return StatusCode; }
    const TArray<FPilotControlAircraft>& GetAircraft() const { return Aircraft; }
    const TArray<FPilotControlPilot>& GetPilots() const { return Pilots; }
    const TArray<FPilotControlResult>& GetLastResults() const { return LastResults; }
    const FString& GetActiveUavId() const { return ActiveUavId; }
    const FString& GetLastExitReason() const { return LastExitReason; }

    bool Grant(const FString& PilotId, const TArray<FString>& UavIds);
    bool Revoke(const FString& UavId);
    bool Activate(const FString& UavId);
    bool Stop(const FString& Reason = TEXT("OPERATOR_STOP"));
    bool QueueControlAction(EPilotControlAction Action, FPilotControlActionGuard Guard);
    void SetControlInput(const FPilotControlAxes& Axes);
    void ClearControlInput();

    FPilotControlChanged OnChanged;

private:
    using FResponse = TFunction<void(int32, const TSharedPtr<FJsonObject>&)>;

    bool Tick(float DeltaTime);
    bool LoadCredentials(const FString& Identity);
    void Connect();
    void OpenSocket(int32 Attempt);
    void Receive(const FString& Text);
    void Disconnect(const TCHAR* Code);
    void CloseSocket();
    bool ApplySnapshot(const TSharedPtr<FJsonObject>& Snapshot, bool bInitial);
    void WriteReadyMarker(bool bPresent) const;
    const FPilotControlAircraft* FindAircraft(const FString& UavId) const;
    void Request(const TCHAR* Verb, const FString& Path, const TSharedPtr<FJsonObject>& Body,
        bool bAuthenticate, FResponse Complete);
    bool BeginOperation(const TCHAR* Action, const TSharedRef<FJsonObject>& Body);
    void SendOperation();
    void CompleteOperation(int32 ResponseCode, const TSharedPtr<FJsonObject>& Reply);
    void FinalizeOperationIfSynchronized();
    void SendInput(double Now);

    TSharedPtr<IWebSocket> Socket;
    FTSTicker::FDelegateHandle TickHandle;
    FString Role, HttpUrl, WsUrl, Authorization, ReadyFile, ConnectionId, ServerEpoch;
    FString StatusCode = TEXT("NOT_CONFIGURED");
    FString ActiveUavId, ActiveSessionId, SuppressedSessionId, LastExitReason;
    TArray<FPilotControlAircraft> Aircraft;
    TArray<FPilotControlPilot> Pilots;
    TArray<FPilotControlResult> LastResults;
    TSharedPtr<FJsonObject> PendingBody;
    FString PendingStatusCode;
    FPilotControlAxes Sample;
    EPilotControlAction PendingAction = EPilotControlAction::None;
    FPilotControlActionGuard PendingActionGuard;
    int64 Version = -1, PendingVersion = -1, WindowId = 0, Sequence = 0;
    int32 Generation = 0, RetryCount = 0;
    double ConnectDeadline = 0., NextConnect = 0., PendingDeadline = 0.;
    double NextInput = 0., WindowReceivedAt = 0., SampleTime = 0.;
    double PendingActionAt = 0.;
    bool bEnabled = false, bConfigured = false, bReady = false, bConnecting = false, bStopped = false;
    bool bControlActions = false;
};
