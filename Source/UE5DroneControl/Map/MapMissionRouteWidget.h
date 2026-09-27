#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Dom/JsonObject.h"
#include "MapMissionRouteWidget.generated.h"
UCLASS()
class UE5DRONECONTROL_API UMapMissionRouteWidget:public UUserWidget {
    GENERATED_BODY()
    bool bBeginning=false;
    friend class FP53GeometryUI;
    friend class FP4MapDraft;
public:
    void ReloadGeometry(){LoadedRoute.Empty();LoadSaved();}
    void Refresh();void SetEditorEnabled(bool Enabled);
    bool IsEditorEnabled() const {return bEditor;}
    int32 GetAssignedUAV() const;
    bool HasDirtyDraft() const {return bDirty;}
    FString GetDraftState() const {return bSaving?TEXT("SAVING"):bDirty?TEXT("DIRTY"):SessionId.IsEmpty()?TEXT("CLEAN"):bSaved?TEXT("SAVED"):TEXT("EDITING");}
protected:virtual void NativeOnInitialized() override;
private:
    UPROPERTY() TObjectPtr<class UCheckBox> ClosedRoute;
    UPROPERTY() TObjectPtr<class UComboBoxString> AltitudeReference;
    UPROPERTY() TObjectPtr<class UVerticalBox> InspectorFields;
    UPROPERTY() TObjectPtr<class UTextBlock> CoordinatesLabel;
    UPROPERTY() TObjectPtr<class UEditableTextBox> SpeedInput;
    bool bLoadingParameters=false;
    bool ApplyParameters();
    UFUNCTION() void ParametersCommitted(const FText& Text,ETextCommit::Type Method);
    UFUNCTION() void ReferenceChanged(FString Value,ESelectInfo::Type Method);
    UPROPERTY() TObjectPtr<class UEditableTextBox> AltitudeInput;
    UPROPERTY() TObjectPtr<class UEditableTextBox> HoverInput;
    UPROPERTY() TObjectPtr<class UTextBlock> SelectedPointLabel;
    int32 LoadedWaypoint=INDEX_NONE;
    FVector LoadedWaypointLocation=FVector::ZeroVector;

    UFUNCTION() void ClosedChanged(bool Checked);
    UPROPERTY() TObjectPtr<class UVerticalBox> Content;
    UPROPERTY() TObjectPtr<class UTextBlock> Summary;
    UPROPERTY() TObjectPtr<class UTextBlock> Result;
    UPROPERTY() TObjectPtr<class UTextBlock> WaypointDetails;
    UPROPERTY() TObjectPtr<class UBorder> UnsavedDialog;
    bool bSaved=false;
    int SaveRetries=0;
    UPROPERTY() TMap<FName,TObjectPtr<class UCommandActionButton>> Actions;
    FString PlanId,MissionId,SessionId,CleanFingerprint,ErrorCode,PendingPlan,PendingMission,LastRemoteSelection;
    bool bEditor=false,bDirty=false,bSaving=false,bPending=false,bPrompt=false,bDiscardConfirm=false,bExitPending=false,bBeginPending=false,bSelectionPending=false;
    FString LoadedRoute;
    double BaseRevision=-1;
    UFUNCTION() void Action(FName Name,int32 Id);
    void MapRequested(const TSharedPtr<FJsonObject>& Request);
    void SwitchTo(const FString& Plan,const FString& Mission,bool Begin);
    void Begin();void Save();void Finish();void Discard();void FinishLeave();
    bool bFinishAfterSave=false;
    bool LoadSaved();void Focus();
    TSharedRef<FJsonObject> RouteJson() const;
    FString Fingerprint() const;
    void Submit(const TCHAR* Name,TFunction<void(TSharedPtr<FJsonObject>)> Complete,const TSharedPtr<FJsonObject>& Extra=nullptr);
};
