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
    // Only explicit Map hits call this: the clicked video target may differ
    // from the resulting primary/members after Shift or Ctrl selection.
    bool SetMapSelection(int32 Id, const TArray<int32>& Multi, int32 ClickedId);
    bool IsMapSelectionPending() const { return bMapSelectionPending; }
    void CancelPendingLocalSelection() { ++SelectionSerial; bMapSelectionPending=false; }
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
    bool IsVideoSwitchPending() const { return bVideoSwitchPending; }
    FString GetPendingVideoTarget() const { return bVideoSwitchPending ? RequestedVideoTarget : FString(); }
    bool IsLanguageChangePending() const { return bLanguageChangePending; }
    FString GetLanguageSyncError() const { return LanguageSyncError; }
    bool ApplyUIPreferences(const TSharedPtr<FJsonObject>& Incoming);
    bool ApplyVideoView(const TSharedPtr<FJsonObject>& Incoming);
    FOperationalContextChanged OnUIPreferencesChanged, OnVideoViewChanged, OnMapRouteEditRequested;
    FString ViewSyncError;
    bool IsHydrated() const { return IsReady() && bPlansHydrated && bLanguageHydrated && bVideoHydrated; }
    FString GetSystemState() const;
    bool IsRoleOnline(const FString& ClientRole) const;


private:
    friend class FVideoRequestFeedbackTest;
    bool bVideoSwitchPending = false, bLanguageChangePending = false;
    int64 VideoSwitchSerial = 0, LanguageChangeSerial = 0;
    FString RequestedVideoTarget, RequestedLanguage, LanguageSyncError;
    void CancelPendingViewRequests(bool ReportFailure);
    void CompleteVideoSwitch(int64 Serial, const TSharedPtr<FJsonObject>& Reply);
    void CompleteLanguageChange(int64 Serial, const TSharedPtr<FJsonObject>& Reply);
    void ReconcileVideoFeedback();
    void ReconcileLanguageFeedback();
    bool bEnabled = false, bApplying = false, bFollowGlobalSelection = true;
    bool bStopped = false, bConnecting = false;
    bool bPlansHydrated=false, bLanguageHydrated=false, bVideoHydrated=false;
    int64 Version = -1;
    int32 Generation = 0;
    int64 SelectionSerial = 0;
    bool bMapSelectionPending = false;
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
        TFunction<void(TSharedPtr<FJsonObject>)> Complete, bool IncludeErrors=false);
};
