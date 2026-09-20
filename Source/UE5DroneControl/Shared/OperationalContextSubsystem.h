#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Dom/JsonObject.h"
#include "Containers/Ticker.h"
#include "OperationalContextSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOperationalFieldChanged, const FString&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOperationalContextChanged, const TSharedPtr<FJsonObject>&);

/** Per-process replica. All shared writes pass through Backend; remote apply never echoes. */
UCLASS()
class UE5DRONECONTROL_API UOperationalContextSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    bool IsEnabled() const { return bEnabled; }
    bool ShouldRouteSelection() const { return bEnabled && !bApplying && bFollowGlobalSelection; }
    bool IsReady() const { return State == TEXT("CONNECTED"); }
    bool SetActiveUAV(int32 Id);
    bool SetLocalSelection(int32 Id, const TArray<int32>& Multi);
    void CancelPendingLocalSelection() { ++SelectionSerial; }
    bool SelectAlert(const FString& AlertId, int32 DroneId);
    bool UpdateContext(const TSharedRef<FJsonObject>& Patch);
    bool ApplyContext(const TSharedPtr<FJsonObject>& Incoming);
    FString GetStatusText() const;
    int64 GetVersion() const { return Version; }
    TSharedPtr<FJsonObject> GetContext() const { return Context; }
    FOperationalContextChanged OnContextChanged;
    FOperationalFieldChanged OnActiveUAVChanged, OnActiveAlertChanged, OnActiveMissionChanged;
    FOperationalFieldChanged OnActiveSecurityPlanChanged, OnActiveAreaChanged, OnOperationModeChanged;
    TMap<FString, TSharedPtr<FJsonObject>> Clients;
    TSharedPtr<FJsonObject> GetPlans() const { return Plans; }
    FOperationalContextChanged OnPlansChanged;
    bool SubmitPlan(const TSharedRef<FJsonObject>& Body, TFunction<void(TSharedPtr<FJsonObject>)> Complete);
    void ApplyPlans(const TSharedPtr<FJsonObject>& Incoming);
    TSharedPtr<FJsonObject> GetUIPreferences() const { return UIPreferences; }
    TSharedPtr<FJsonObject> GetVideoView() const { return VideoView; }
    FString GetInstanceId() const { return InstanceId; }
    bool SetLanguage(const FString& Language);
    bool OpenVideo(const FString& UAV);
    bool ApplyUIPreferences(const TSharedPtr<FJsonObject>& Incoming);
    bool ApplyVideoView(const TSharedPtr<FJsonObject>& Incoming);
    FOperationalContextChanged OnUIPreferencesChanged, OnVideoViewChanged, OnMapRouteEditRequested;
    FString ViewSyncError;
    bool IsHydrated() const { return IsReady() && bPlansHydrated && bLanguageHydrated && bVideoHydrated; }
    FString GetSystemState() const;
    bool IsRoleOnline(const FString& ClientRole) const;


private:
    bool bEnabled = false, bApplying = false, bFollowGlobalSelection = true;
    bool bStopped = false, bConnecting = false;
    bool bPlansHydrated=false, bLanguageHydrated=false, bVideoHydrated=false;
    int64 Version = -1;
    int32 Generation = 0;
    int32 SelectionSerial = 0;
    FString Role, InstanceId, ClientId, HttpUrl, WsUrl, State = TEXT("DISCONNECTED");
    TSharedPtr<FJsonObject> Context;
    TSharedPtr<FJsonObject> Plans;
    TSharedPtr<FJsonObject> UIPreferences,VideoView;
    TSharedPtr<class IWebSocket> Socket;
    FTSTicker::FDelegateHandle TickHandle;
    double NextAttempt = 0, LastMessage = 0, LastPing = 0;
    void Connect();
    void OpenSocket(int32 Attempt);
    void Offline();
    void Receive(const FString& Message);
    void ApplySelection();
    void Request(const FString& Verb, const FString& Path, const TSharedPtr<FJsonObject>& Body,
        TFunction<void(TSharedPtr<FJsonObject>)> Complete);
};
