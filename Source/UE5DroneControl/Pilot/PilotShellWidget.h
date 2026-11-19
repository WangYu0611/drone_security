#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "PilotShellWidget.generated.h"

class UBorder;
class UButton;
class UComboBoxString;
class UPilotControlSubsystem;
class UTextBlock;
class UWebBrowser;
struct FPilotControlAircraft;
struct FPilotControlAxes;

UCLASS()
class UE5DRONECONTROL_API UPilotShellWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    TSharedRef<SWidget> RebuildWidget() override;
    void NativeConstruct() override;
    void NativeDestruct() override;
    void NativeTick(const FGeometry& Geometry, float DeltaTime) override;

private:
    enum class EInputState { Disconnected, MissingAxes, OffCenter, Neutral, Mock };

    void BuildPresentation();
    UTextBlock* MakeLabel(const FText& Text, int32 Size, bool bBold = false);
    UButton* MakeButton(const FText& Text, bool bPrimary = false);
    void RefreshAircraft();
    void UpdateStatus(double Now, EInputState InputState, bool bWindowActive);
    const FPilotControlAircraft* GetSelectedAircraft() const;
    void OpenFeed(const FString& Url);
    void ProbeFeed(double Now);
    bool HasFreshFrames(double Now) const;
    bool IsWindowActive();
    EInputState SampleInput(FPilotControlAxes& Axes) const;
    void RequestStop(const FString& Reason);

    UFUNCTION() void AircraftSelected(FString Item, ESelectInfo::Type SelectInfo);
    UFUNCTION() void BeginControl();
    UFUNCTION() void StopControl();
    UFUNCTION() void MockForwardPressed();
    UFUNCTION() UWidget* MakeAircraftOption(FString Item);
    UFUNCTION() void FeedUrlChanged(const FText& Url);
    UFUNCTION() void FeedConsoleMessage(const FString& Message, const FString& Source, int32 Line);

    UPROPERTY(Transient) TObjectPtr<UPilotControlSubsystem> Control;
    UPROPERTY(Transient) TObjectPtr<UComboBoxString> AircraftPicker;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> LinkLabel;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> InputLabel;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> FrameLabel;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailLabel;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> FeedTitle;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> FeedPlaceholder;
    UPROPERTY(Transient) TObjectPtr<UBorder> FeedCover;
    UPROPERTY(Transient) TObjectPtr<UWebBrowser> Browser;
    UPROPERTY(Transient) TObjectPtr<UButton> BeginButton;
    UPROPERTY(Transient) TObjectPtr<UButton> StopButton;
    UPROPERTY(Transient) TObjectPtr<UButton> MockForwardButton;

    FString SelectedUavId;
    FString FeedUavId;
    FString SourceUrl;
    FString ProbePrefix;
    FString StopRequestedFor;
    TArray<FString> AircraftIds;
    TArray<FString> AxisNames{TEXT("Y"), TEXT("X"), TEXT("Z"), TEXT("R")};
    TArray<bool> InvertAxis{true, false, false, false};
    int32 JoystickIndex = 0;
    int32 FeedGeneration = 0;
    int32 ProbeSerial = 0;
    int32 LastCompletedProbe = 0;
    double FeedOpenedAt = 0.0;
    double LastProbeAt = 0.0;
    double LastFrameAt = 0.0;
    double LastDecodedFrame = -1.0;
    double NextInputAt = 0.0;
    double MockForwardUntil = 0.0;
    bool bPageReady = false;
    bool bUpdatingPicker = false;
    bool bMockInput = false;
};
