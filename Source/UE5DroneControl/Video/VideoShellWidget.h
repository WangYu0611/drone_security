#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "VideoShellWidget.generated.h"

/** Routes both aircraft rows and reserved feed cards to the shared selection. */
UCLASS()
class UE5DRONECONTROL_API UVideoFeedButton : public UButton
{
    GENERATED_BODY()
public:
    int32 DroneId = 0;
    TWeakObjectPtr<class UVideoShellWidget> Owner;
    UPROPERTY() TObjectPtr<class UTextBlock> Label;
    UFUNCTION() void ActivateFeed();
};

/** One media browser; remaining feed cards are explicit placeholders over the Registry. */
UCLASS()
class UE5DRONECONTROL_API UVideoShellWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void InitializeRegistry(class UDroneRegistrySubsystem* InRegistry);
    void Refresh();
    void Shutdown();
    void Resume();
    int32 GetDisplayedDroneId() const { return DisplayedDroneId; }
    FString GetSourceUrl() const { return SourceUrl; }
    FString GetMediaState() const { return MediaState; }
    void SelectDrone(int32 Id);
    void SetViewMode(int32 Channels);
    void SetFollowTarget(bool Enabled);
    bool IsFollowingTarget() const { return bFollowTarget; }
    bool SetAsActiveUAV();
    int32 GetBrowserGeneration() const {return Generation;}
    int32 GetViewMode() const { return ViewMode; }
    int32 GetSecondaryDroneId(int32 Index) const { return SecondaryIds.IsValidIndex(Index) ? SecondaryIds[Index] : 0; }
    class UVideoFeedButton* GetAircraftButton(int32 Id) const;
    class UVideoFeedButton* GetSecondaryButton(int32 Index) const;
    class UWebBrowser* GetBrowser() const { return Browser; }
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeDestruct() override;
private:
    TWeakObjectPtr<class UDroneRegistrySubsystem> Registry;
    UPROPERTY() TObjectPtr<class UVerticalBox> AircraftList;
    UPROPERTY() TObjectPtr<class UHorizontalBox> SecondaryArea;
    UPROPERTY() TObjectPtr<class UTextBlock> SystemHeader;
    UPROPERTY() TObjectPtr<class UTextBlock> EmptyVideo;
    UPROPERTY() TObjectPtr<class UBorder> MediaCover;
    UPROPERTY() TArray<TObjectPtr<UVideoFeedButton>> AircraftButtons;
    UPROPERTY() TArray<TObjectPtr<UVideoFeedButton>> SecondaryButtons;
    UPROPERTY() TArray<TObjectPtr<UButton>> ModeButtons;
    UPROPERTY() TObjectPtr<class UWebBrowser> Browser;
    UPROPERTY() TObjectPtr<class UTextBlock> Heading;
    UPROPERTY() TObjectPtr<class UTextBlock> Status;
    UPROPERTY() TObjectPtr<class UTextBlock> Details;
    UPROPERTY() TObjectPtr<class UButton> Retry;
    // Presentation order only; descriptors, selection and telemetry remain in Registry.
    TArray<int32> AircraftIds, SecondaryIds;
    int32 ViewMode = 6;
    FString SourceUrl, MediaState = TEXT("OFFLINE"), ProbeToken;
    int32 DisplayedDroneId = 0, Generation = 0, Width = 0, Height = 0;
    double LastProbe = 0, LastProgress = 0, LastSample = 0, LastMediaTime = -1, LastFrames = -1, Fps = -1;
    bool bStopped = false, bPageReady = false;
    bool bFollowTarget = false;
    int32 LocalSourceId = 0;
    UPROPERTY() TObjectPtr<class UButton> FollowButton;
    UPROPERTY() TObjectPtr<class UTextBlock> FollowLabel;
    UFUNCTION() void Chinese();
    UFUNCTION() void English();
    UFUNCTION() void ToggleFollow() { SetFollowTarget(!bFollowTarget); }
    UFUNCTION() void PublishActive() { SetAsActiveUAV(); }
    class UTextBlock* MakeText(const FString& Value, int32 Size);
    UVideoFeedButton* MakeFeedButton();
    void UpdateFeeds();
    FString FeedDescription(int32 Id, bool Secondary) const;
    void SetSource(int32 Id, const FString& Url);
    void UpdateLabels();
    UFUNCTION() void ViewFour() { SetViewMode(4); }
    UFUNCTION() void ViewSix() { SetViewMode(6); }
    UFUNCTION() void ViewFocus() { SetViewMode(1); }
    UFUNCTION() void UrlChanged(const FText& Url);
    UFUNCTION() void ConsoleMessage(const FString& Message, const FString& Source, int32 Line);
    UFUNCTION() void RetrySource();
};
