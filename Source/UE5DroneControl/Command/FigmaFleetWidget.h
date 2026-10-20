#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DroneOps/Core/DroneOpsTypes.h"
#include "FigmaFleetWidget.generated.h"

/** Command fleet presentation. Selection, execution and video keep independent authorities. */
UCLASS()
class UE5DRONECONTROL_API UFigmaFleetWidget : public UUserWidget
{
    GENERATED_BODY()
    friend class FFigmaCommandChecks;
public:
    void Refresh();
protected:
    virtual void NativeOnInitialized() override;
private:
    struct FRow
    {
        class UBorder* Card = nullptr;
        class UBorder* IconCard = nullptr;
        class UBorder* Badge = nullptr;
        class UBorder* CheckBox = nullptr;
        class UTextBlock* Name = nullptr;
        class UTextBlock* State = nullptr;
        class UTextBlock* Detail = nullptr;
        class UTextBlock* PrimaryLabel = nullptr;
        class UCommandActionButton* Toggle = nullptr;
        class UCommandActionButton* Primary = nullptr;
        class UImage* Check = nullptr;
    };
    TMap<int32, FRow> Rows;
    TArray<int32> DroneIds;
    TMap<FString, int32> VideoOptions;
    TMap<int32, FString> VideoDisplayNames;
    int32 EditingId = 0;
    int32 PendingPrimary = 0;
    int32 SelectionBeforePrimary = 0;
    int64 SelectionRequestVersion = -1;
    double SelectionRequestedAt = 0;
    TArray<int32> PendingSelection;
    TArray<int32> SelectionBefore;
    bool bSelectionPending = false;
    FDroneLabelSettings DraftLabel;
    UPROPERTY() TObjectPtr<class UOverlay> RootOverlay;
    UPROPERTY() TObjectPtr<class UScrollBox> DroneScroll;
    UPROPERTY() TObjectPtr<class UTextBlock> EmptyList;
    UPROPERTY() TObjectPtr<class UGroupControlWidget> GroupControls;
    UPROPERTY() TObjectPtr<class UTextBlock> SelectionStatus;
    UPROPERTY() TObjectPtr<class UTextBlock> TelemetryState;
    UPROPERTY() TArray<TObjectPtr<class UTextBlock>> TelemetryValues;
    UPROPERTY() TObjectPtr<class UComboBoxString> VideoChoice;
    UPROPERTY() TObjectPtr<class UCommandActionButton> VideoOpen;
    UPROPERTY() TObjectPtr<class UTextBlock> VideoTitle;
    UPROPERTY() TObjectPtr<class UTextBlock> VideoHint;
    UPROPERTY() TObjectPtr<class UImage> VideoEmptyIcon;
    UPROPERTY() TObjectPtr<class UBorder> LabelLayer;
    UPROPERTY() TObjectPtr<class UEditableTextBox> LabelName;
    UPROPERTY() TObjectPtr<class UTextBlock> LabelPreview;
    UPROPERTY() TObjectPtr<class UTextBlock> LabelSizeValue;
    UPROPERTY() TObjectPtr<class USlider> LabelSize;
    UPROPERTY() TObjectPtr<class UProgressBar> LabelSizeProgress;
    UPROPERTY() TObjectPtr<class UCommandActionButton> LabelConfirm;
    UPROPERTY() TArray<TObjectPtr<class UImage>> ColorRings;
    UPROPERTY() TArray<TObjectPtr<class UImage>> ColorChecks;
    class UCommandActionButton* MakeAction(FName Action, const TCHAR* TextKey, int32 Id = 0, const FString& Icon = FString());
    class UTextBlock* MakeText(const FText& Text, int32 Size = 14, bool Bold = false);
    void BuildRows(const TArray<FDroneDescriptor>& Descriptors);
    void RefreshVideoOptions(const TArray<FDroneDescriptor>& Descriptors);
    void SetSelectionStatus(const FText& Text);
    void BuildLabelDialog();
    void OpenLabelDialog(int32 Id);
    void RefreshLabelPreview();
    UFUNCTION() void Action(FName Name, int32 Id);
    UFUNCTION() void LabelNameChanged(const FText& Value);
    UFUNCTION() void LabelSizeChanged(float Value);
    UFUNCTION() UWidget* GenerateVideoOption(FString Option);
};
