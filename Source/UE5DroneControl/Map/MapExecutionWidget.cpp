#include "Map/MapExecutionWidget.h"
#include "Map/FigmaMapStyle.h"
#include "Map/MapMoveSupport.h"
#include "Map/MapShellWidget.h"
#include "Map/PlanRouteVisualSet.h"
#include "PathEditor/DronePathActor.h"
#include "Shared/ExecutionPresentation.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "DroneOps/Core/ICoordinateService.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "Command/CommandScreenManager.h"
#include "Command/CommandMapInteractionService.h"
#include "Algo/BinarySearch.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/Slider.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/DrawElements.h"
#include "Layout/Clipping.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SWidget.h"
#include "RealTimeDroneReceiver.h"
#include "Map/MapPaintGeometry.h"
using namespace PlanUI;
namespace {
FString ExecutionGroupKey(const TSharedPtr<FJsonObject>& Execution)
{
    const FString Group=Field(Execution,TEXT("group_id"));
    return Group.IsEmpty()?Field(Execution,TEXT("execution_id")):TEXT("group/")+Group;
}
}

void UMapExecutionWidget::NativeOnInitialized(){
    Super::NativeOnInitialized();auto* Root=WidgetTree->ConstructWidget<UCanvasPanel>();WidgetTree->RootWidget=Root;
    Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    auto* Panel=WidgetTree->ConstructWidget<UBorder>();SummaryPanel=Panel;MapUI::Panel(Panel);Panel->SetPadding(FMargin(12));
    auto* Box=WidgetTree->ConstructWidget<UVerticalBox>();Panel->SetContent(Box);
    MapUI::Text(Label(WidgetTree,Box,T(TEXT("Execution.MonitorNeutral")),24),20,MapUI::PrimaryText,true);Summary=Label(WidgetTree,Box,FText::GetEmpty(),22);MapUI::Text(Summary,15);
    RunPicker=WidgetTree->ConstructWidget<UComboBoxString>();
    RunPicker->SetContentPadding(FMargin(8,4));
    auto ComboStyle=RunPicker->GetWidgetStyle();
    ComboStyle.ComboButtonStyle.ButtonStyle.SetNormal(FSlateRoundedBoxBrush(FigmaUI::Inset,5,FigmaUI::Outline,1));
    ComboStyle.ComboButtonStyle.ButtonStyle.SetHovered(FSlateRoundedBoxBrush(FigmaUI::Inset,5,FigmaUI::Muted,1));
    RunPicker->SetWidgetStyle(ComboStyle);
    RunPicker->SetToolTipText(SimulationUI::Label(TEXT("Select a completed rehearsal to replay."),TEXT("选择已结束的预演查看回放。")));
    RunPicker->OnSelectionChanged.AddDynamic(this,&UMapExecutionWidget::SelectRun);
    RunPicker->SetVisibility(ESlateVisibility::Collapsed);
    Box->AddChildToVerticalBox(RunPicker)->SetPadding(FMargin(0,8,0,0));

    ReplayControls=WidgetTree->ConstructWidget<UVerticalBox>();
    ReplayControls->SetVisibility(ESlateVisibility::Collapsed);
    Box->AddChildToVerticalBox(ReplayControls)->SetPadding(FMargin(0,6,0,0));
    ReplayTime=Label(WidgetTree,ReplayControls,FText::GetEmpty(),13);
    ReplaySlider=WidgetTree->ConstructWidget<USlider>();
    ReplaySlider->OnValueChanged.AddDynamic(this,&UMapExecutionWidget::ScrubReplay);
    ReplayControls->AddChildToVerticalBox(ReplaySlider)->SetPadding(FMargin(0,4));
    auto* Actions=WidgetTree->ConstructWidget<UHorizontalBox>();
    ReplayControls->AddChildToVerticalBox(Actions);
    PlayButton=WidgetTree->ConstructWidget<UButton>();
    PlayCaption=WidgetTree->ConstructWidget<UTextBlock>();
    MapUI::Text(PlayCaption,13);
    PlayButton->SetContent(PlayCaption);
    MapUI::Button(PlayButton,false,true);
    PlayButton->OnClicked.AddDynamic(this,&UMapExecutionWidget::PlayReplay);
    Actions->AddChildToHorizontalBox(PlayButton);
    auto* Locate=WidgetTree->ConstructWidget<UButton>();
    auto* LocateCaption=WidgetTree->ConstructWidget<UTextBlock>();
    MapUI::Text(LocateCaption,13);
    LocateCaption->SetText(SimulationUI::Label(TEXT("Locate route"),TEXT("定位航线")));
    Locate->SetContent(LocateCaption);
    MapUI::Button(Locate,false,true);
    Locate->OnClicked.AddDynamic(this,&UMapExecutionWidget::LocateExecution);
    Actions->AddChildToHorizontalBox(Locate);
    auto* Chrome=MapUI::Chrome(WidgetTree,Root);MapUI::Place(Chrome,Panel,FVector2D(45,230),FVector2D(380,270));
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UMapExecutionWidget::RefreshHistory(const TSharedPtr<FJsonObject>& State)
{
    TArray<TSharedPtr<FJsonObject>> Completed;
    TSet<FString> ActiveGroups;
    const auto All=Object(State,TEXT("executions"));
    if(All)for(const auto& Item:All->Values){
        const auto Run=Item.Value->AsObject();
        if(!ExecutionUI::IsRehearsal(Run))continue;
        if(ExecutionUI::Active(Run))ActiveGroups.Add(ExecutionGroupKey(Run));
        else Completed.Add(Run);
    }
    Completed.Sort([](const auto& A,const auto& B){
        const double CreatedA=A->GetNumberField(TEXT("created_at")),CreatedB=B->GetNumberField(TEXT("created_at"));
        return CreatedA!=CreatedB?CreatedA>CreatedB:Field(A,TEXT("execution_id"))<Field(B,TEXT("execution_id"));
    });
    const FString Live=SimulationUI::Label(TEXT("Current execution"),TEXT("当前执行")).ToString();
    TArray<FString> Options{Live};
    TSet<FString> Groups;
    FString Selected=Live;
    HistoryIds.Reset();
    for(const auto& Run:Completed){
        const FString Group=ExecutionGroupKey(Run);
        if(Groups.Contains(Group) || ActiveGroups.Contains(Group))continue;
        Groups.Add(Group);
        const FString Id=Field(Run,TEXT("execution_id"));
        const FString Name=Field(Run,TEXT("mission_name"))+TEXT(" · ")+T(*(TEXT("Execution.")+Field(Run,TEXT("state")))).ToString()+TEXT(" · ")+Id;
        Options.Add(Name);
        HistoryIds.Add(Name,Id);
        if(Id==ReplayId)Selected=Name;
    }
    if(!ReplayId.IsEmpty() && Selected==Live){
        ReplayId.Reset();
        LoadReplay(State,nullptr);
        bReplaySelectionChanged=true;
    }
    TGuardValue<bool> Guard(bChangingReplayUi,true);
    if(Options!=RunOptions){
        RunOptions=MoveTemp(Options);
        RunPicker->ClearOptions();
        for(const auto& Option:RunOptions)RunPicker->AddOption(Option);
    }
    if(RunPicker->GetSelectedOption()!=Selected)RunPicker->SetSelectedOption(Selected);
    RunPicker->SetVisibility(HistoryIds.IsEmpty()?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
}

void UMapExecutionWidget::LoadReplay(const TSharedPtr<FJsonObject>& State,const TSharedPtr<FJsonObject>& Run)
{
    ReplayTracks.Reset();
    ReplayDuration=ReplaySeconds=0;
    bReplayPlaying=false;
    const auto All=Object(State,TEXT("executions"));
    if(!All || !Run)return;
    const FString Group=ExecutionGroupKey(Run);
    for(const auto& Item:All->Values){
        const auto Member=Item.Value->AsObject();
        if(!ExecutionUI::IsRehearsal(Member) || ExecutionGroupKey(Member)!=Group)continue;
        const TArray<TSharedPtr<FJsonValue>>* Samples=nullptr;
        if(!Member->TryGetArrayField(TEXT("replay_samples"),Samples))continue;
        TArray<FReplaySample> Track;
        for(const auto& Value:*Samples){
            const auto Sample=Value->AsObject();
            const auto Position=Object(Sample,TEXT("position"));
            FReplaySample Point;
            if(!Position || !Sample->TryGetNumberField(TEXT("elapsed_seconds"),Point.Seconds)
                || !FMath::IsFinite(Point.Seconds) || Point.Seconds<0 || (!Track.IsEmpty() && Point.Seconds<Track.Last().Seconds)
                || !Position->TryGetNumberField(TEXT("longitude"),Point.Geographic.X)
                || !Position->TryGetNumberField(TEXT("latitude"),Point.Geographic.Y)
                || !Position->TryGetNumberField(TEXT("altitude"),Point.Geographic.Z)
                || Point.Geographic.ContainsNaN())continue;
            Sample->TryGetNumberField(TEXT("progress"),Point.Progress);
            Sample->TryGetNumberField(TEXT("current_waypoint"),Point.CurrentWaypoint);
            if(!Sample->TryGetNumberField(TEXT("completed_waypoints"),Point.CompletedWaypoints))
                Point.CompletedWaypoints=Point.Progress>=1.?int32(Member->GetNumberField(TEXT("total_waypoints"))):FMath::Max(0,Point.CurrentWaypoint-1);
            Track.Add(Point);
        }
        if(Track.IsEmpty())continue;
        ReplayDuration=FMath::Max(ReplayDuration,Track.Last().Seconds);
        ReplayTracks.Add(Field(Member,TEXT("execution_id")),MoveTemp(Track));
    }
}

bool UMapExecutionWidget::ReplaySample(const TSharedPtr<FJsonObject>& Current,FReplaySample& Result) const
{
    const auto* Track=ReplayTracks.Find(Field(Current,TEXT("execution_id")));
    if(!Track || Track->IsEmpty())return false;
    const int Next=Algo::UpperBoundBy(*Track,ReplaySeconds,&FReplaySample::Seconds);
    if(Next==0){Result=(*Track)[0];return true;}
    Result=(*Track)[Next-1];
    if(!Track->IsValidIndex(Next))return true;
    const auto& End=(*Track)[Next];
    const double Alpha=(ReplaySeconds-Result.Seconds)/(End.Seconds-Result.Seconds);
    const double Longitude=FMath::UnwindDegrees(Result.Geographic.X+
        FMath::FindDeltaAngleDegrees(Result.Geographic.X,End.Geographic.X)*Alpha);
    Result.Geographic=FMath::Lerp(Result.Geographic,End.Geographic,Alpha);
    Result.Geographic.X=Longitude;
    Result.Progress=FMath::Lerp(Result.Progress,End.Progress,Alpha);
    return true;
}

void UMapExecutionWidget::SelectRun(FString Option,ESelectInfo::Type Type)
{
    if(bChangingReplayUi || Type==ESelectInfo::Direct)return;
    const auto State=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetPlans();
    ReplayId=HistoryIds.FindRef(Option);
    LoadReplay(State,Find(State,TEXT("executions"),ReplayId));
    bReplaySelectionChanged=true;
    if(auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer());PC && PC->GetCommandScreenManager())
        if(auto* Shell=PC->GetCommandScreenManager()->GetMapShell()){Shell->Refresh();return;}
    Refresh();
}

void UMapExecutionWidget::PlayReplay()
{
    if(ReplayDuration<=0)return;
    if(ReplaySeconds>=ReplayDuration)ReplaySeconds=0;
    bReplayPlaying=!bReplayPlaying;
    RefreshSummary();
}

void UMapExecutionWidget::ScrubReplay(float Position)
{
    if(bChangingReplayUi || ReplayId.IsEmpty())return;
    bReplayPlaying=false;
    ReplaySeconds=FMath::Clamp(Position,0.f,1.f)*ReplayDuration;
    RefreshSummary();
    InvalidateLayoutAndVolatility();
}

void UMapExecutionWidget::NativeTick(const FGeometry& Geometry,float DeltaSeconds)
{
    Super::NativeTick(Geometry,DeltaSeconds);
    if(!bReplayPlaying)return;
    const int32 PreviousSecond=FMath::FloorToInt(ReplaySeconds);
    ReplaySeconds=FMath::Min(ReplayDuration,ReplaySeconds+DeltaSeconds);
    bReplayPlaying=ReplaySeconds<ReplayDuration;
    if(!bReplayPlaying || PreviousSecond!=FMath::FloorToInt(ReplaySeconds))RefreshSummary();
    TGuardValue<bool> Guard(bChangingReplayUi,true);
    ReplaySlider->SetValue(static_cast<float>(ReplaySeconds/ReplayDuration));
    if(TSharedPtr<SWidget> Cached=GetCachedWidget())Cached->Invalidate(EInvalidateWidgetReason::Paint);
}

void UMapExecutionWidget::BuildPresentedRoutes(const TSharedPtr<FJsonObject>& State){
    PresentedRoutes.Reset();
    TMap<FString,int32> RouteIndices;
    for(const auto& E:RenderedExecutions){
        const FString Plan=Field(E,TEXT("plan_id")),Mission=Field(E,TEXT("mission_id")),Group=Field(E,TEXT("group_id"));
        const bool bMissionGroup=!Plan.IsEmpty() && !Mission.IsEmpty() && !Group.IsEmpty()
            && Field(E,TEXT("control_kind"))!=TEXT("GROUP_MOVE");
        const FString Key=bMissionGroup?TEXT("mission/")+Plan+TEXT("/")+Mission+TEXT("/")+Group:Field(E,TEXT("execution_id"));
        int32* Index=RouteIndices.Find(Key);
        if(!Index){
            Index=&RouteIndices.Add(Key,PresentedRoutes.Num());
            auto& Presentation=PresentedRoutes.AddDefaulted_GetRef();
            Presentation.Key=Key;
            Presentation.bMissionGroup=bMissionGroup;
        }
        PresentedRoutes[*Index].Members.Add(E);
    }
    for(auto& Presentation:PresentedRoutes){
        Presentation.Members.Sort([](const auto& A,const auto& B){return Field(A,TEXT("uav_id"))<Field(B,TEXT("uav_id"));});
        const auto& First=Presentation.Members[0];
        if(Presentation.bMissionGroup){
            for(const auto& E:Presentation.Members){
                Presentation.Route=Object(E,TEXT("plan_route_snapshot"));
                if(Presentation.Route)break;
            }
            if(!Presentation.Route){
                const auto Deployment=Find(State,TEXT("deployments"),Field(First,TEXT("deployment_id")));
                const auto Snapshot=Object(Deployment,TEXT("snapshot"));
                const auto Mission=Find(Snapshot,TEXT("missions"),Field(First,TEXT("mission_id")));
                if(Mission && Field(Mission,TEXT("route_id"))==Field(First,TEXT("route_id")))
                    Presentation.Route=Find(Snapshot,TEXT("paths"),Field(First,TEXT("route_id")));
            }
        }
        if(!Presentation.Route)Presentation.Route=Object(First,TEXT("route_snapshot"));
    }
}

void UMapExecutionWidget::Refresh(){
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    auto* Registry=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
    auto* Coordinates=Registry->GetCoordinateService().GetObject();
    // Hydrate initial and terminal Mock positions even when no execution panel is
    // visible. Aircraft, selection and previews must use the same accepted source.
    if(Field(Sync->GetPlans(),TEXT("execution_adapter"))==TEXT("Mock") && Coordinates && ICoordinateService::Execute_IsCoordinateSystemReady(Coordinates)) {
        const auto Uavs=Object(Object(Sync->GetPlans(),TEXT("mock_execution")),TEXT("uavs"));
        if(Uavs)for(const auto& Item:Uavs->Values){const FString Uav(Item.Key.ToView());int32 Drone=0;LexTryParseString(Drone,*Uav.Mid(4));
            // Received provenance remains authoritative after GPS/liveness loss.
            // The map must not rewind its actor to a configured Mock home.
            FDroneTelemetrySnapshot Snapshot;
            if(Registry->GetTelemetry(Drone,Snapshot) && Snapshot.TelemetrySource==TEXT("received"))continue;
            const auto P=ExecutionUI::MockPosition(Sync->GetPlans(),Uav);
            if(auto* Mirror=Cast<ARealTimeDroneReceiver>(Registry->GetReceiverActor(Drone));Mirror && P)Mirror->ApplySimulationPosition(ICoordinateService::Execute_GeographicToWorld(Coordinates,P->GetNumberField(TEXT("latitude")),P->GetNumberField(TEXT("longitude")),P->GetNumberField(TEXT("altitude"))));}
    }
    const auto State=Sync->GetPlans();
    const auto SelectedPlan=Field(Sync->GetContext(),TEXT("active_security_plan_id"));
    RefreshHistory(State);
    const auto Replay=Find(State,TEXT("executions"),ReplayId);
    Execution=Replay;
    if(!Execution){
        Execution=ExecutionUI::Latest(State,SelectedPlan,true);
        if(!Execution)Execution=ExecutionUI::Latest(State,TEXT(""),true);
        if(!Execution)Execution=ExecutionUI::Latest(State,SelectedPlan);
        if(Execution && !ExecutionUI::Active(Execution)){
            const auto Plan=Find(State,TEXT("plans"),Field(Execution,TEXT("plan_id")));
            const bool Current=ExecutionUI::IsRehearsal(Execution)?ExecutionUI::MatchesCurrentRehearsalRoute(State,Execution)
                :!Plan || Field(Plan,TEXT("deployment_id"))==Field(Execution,TEXT("deployment_id"));
            if(!Current)Execution.Reset();
        }
    }
    RenderedExecutions=ExecutionUI::RouteOwners(State,SelectedPlan,Replay);
    BuildPresentedRoutes(State);
    if(!Execution && !RenderedExecutions.IsEmpty())Execution=RenderedExecutions[0];
    SetVisibility(Execution || !HistoryIds.IsEmpty()?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::Collapsed);
    const FString Id=ExecutionGroupKey(Execution);
    if(Sync->IsHydrated() && !bInitialExecutionSnapshotObserved){
        bInitialExecutionSnapshotObserved=true;
        // A persisted execution is history, not a newly requested camera move.
        // Initial framing belongs to the current accepted fleet positions.
        FocusedExecution=Id;
    }
    RefreshSummary();
    if(!Execution){VisualRoutes.Reset();bReplaySelectionChanged=false;return;}
    auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer());
    // A mission group presents its immutable centerline. The member snapshots
    // remain separate for progress, aircraft positions and manual takeover.
    if(Coordinates && ICoordinateService::Execute_IsCoordinateSystemReady(Coordinates)){
        TSet<FString> Live;
        const FString SelectedMission=Field(Sync->GetContext(),TEXT("active_mission_id"));
        const FString SelectedUav=FString::Printf(TEXT("UAV-%02d"),Registry->GetPrimarySelectedDrone());
        for(const auto& Presentation:PresentedRoutes){
            if(!Presentation.Route)continue;
            Live.Add(Presentation.Key);
            bool bRunning=false,bCompleted=true,bSelected=false;
            for(const auto& E:Presentation.Members){
                const FString MemberState=Field(E,TEXT("state"));
                bRunning|=ExecutionUI::Active(E) && MemberState!=TEXT("CREATED") && MemberState!=TEXT("SCHEDULED");
                bCompleted&=MemberState==TEXT("COMPLETED");
                bSelected|=SelectedMission.IsEmpty()?Field(E,TEXT("uav_id"))==SelectedUav
                    : Field(E,TEXT("plan_id"))==SelectedPlan && Field(E,TEXT("mission_id"))==SelectedMission;
            }
            const auto VisualState=bRunning?ERouteVisualState::Active:bCompleted?ERouteVisualState::Completed:ERouteVisualState::Confirmed;
            if(auto* Path=VisualRoutes.Put(GetWorld(),Coordinates,Presentation.Key,Presentation.Route,VisualState,bSelected))
                Path->SetActorHiddenInGame(IsPresentationSuppressed(Presentation));
        }
        VisualRoutes.Retain(Live);
    }
    if(bReplaySelectionChanged || Replay || ExecutionUI::IsRehearsal(Execution) || !ExecutionUI::Active(Execution))FocusedExecution=Id;
    bReplaySelectionChanged=false;
    // Consume automatic framing while editing; do not defer it until unlock.
    // Telemetry and execution presentation above keep refreshing normally.
    if(PC && PC->GetCommandScreenManager() && PC->GetCommandScreenManager()->GetMapService()->IsRouteEditCameraLocked())FocusedExecution=Id;
    if(bInitialExecutionSnapshotObserved && Id!=FocusedExecution && FocusExecution())FocusedExecution=Id;
    InvalidateLayoutAndVolatility();
}

void UMapExecutionWidget::RefreshSummary()
{
    const bool bReplay=!ReplayId.IsEmpty();
    ReplayControls->SetVisibility(bReplay || ExecutionUI::IsRehearsal(Execution)?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    const auto TimelineVisibility=bReplay?ESlateVisibility::Visible:ESlateVisibility::Collapsed;
    ReplayTime->SetVisibility(TimelineVisibility);
    ReplaySlider->SetVisibility(TimelineVisibility);
    PlayButton->SetVisibility(TimelineVisibility);
    if(bReplay){
        FNumberFormattingOptions SecondsFormat;
        SecondsFormat.SetMaximumFractionalDigits(1);
        const FText PlaybackTime=FText::Format(SimulationUI::Label(TEXT("{0} / {1} s"),TEXT("{0} / {1} 秒")),
            FText::AsNumber(ReplaySeconds,&SecondsFormat),FText::AsNumber(ReplayDuration,&SecondsFormat));
        ReplayTime->SetText(ReplayTracks.IsEmpty()?SimulationUI::Label(TEXT("No position samples were recorded."),TEXT("本次预演未记录位置采样。")):PlaybackTime);
        PlayCaption->SetText(bReplayPlaying?SimulationUI::Label(TEXT("Pause replay"),TEXT("暂停回放"))
            :SimulationUI::Label(TEXT("Play replay"),TEXT("播放回放")));
        PlayButton->SetIsEnabled(ReplayDuration>0);
        ReplaySlider->SetIsEnabled(ReplayDuration>0);
        TGuardValue<bool> Guard(bChangingReplayUi,true);
        ReplaySlider->SetValue(ReplayDuration>0?static_cast<float>(ReplaySeconds/ReplayDuration):0.f);
    }
    if(!Execution){Summary->SetText(T(TEXT("Execution.None")));return;}

    const auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    const auto Mode=bReplay?SimulationUI::Label(TEXT("Rehearsal replay"),TEXT("预演回放"))
        :T(Sync->IsReady()?(Execution->GetBoolField(TEXT("simulation"))?TEXT("Execution.Simulation"):TEXT("Execution.Real")):TEXT("Stage1.RECONNECTING"));
    auto Current=Execution;
    FReplaySample Sample;
    if(ReplaySample(Execution,Sample)){
        Current=MakeShared<FJsonObject>(*Execution);
        Current->SetNumberField(TEXT("current_waypoint"),Sample.CurrentWaypoint);
        Current->SetNumberField(TEXT("progress"),Sample.Progress);
    }
    FText Status=T(TEXT("Execution.ReadOnly"));
    if(ExecutionUI::IsRehearsal(Execution)){
        const FString Group=ExecutionGroupKey(Execution);
        const auto All=Object(Sync->GetPlans(),TEXT("executions"));
        int32 Count=0,Finished=0;
        FString Failure;
        if(All)for(const auto& Item:All->Values){
            const auto Member=Item.Value->AsObject();
            if(!ExecutionUI::IsRehearsal(Member) || ExecutionGroupKey(Member)!=Group)continue;
            ++Count;
            Finished+=Field(Member,TEXT("state"))==TEXT("COMPLETED");
            if(Failure.IsEmpty())Failure=Field(Member,TEXT("failure_reason"));
        }
        Status=FText::Format(SimulationUI::Label(TEXT("Aircraft complete {0}/{1}"),TEXT("已完成 {0}/{1} 架")),
            FText::AsNumber(Finished),FText::AsNumber(Count));
        if(!Failure.IsEmpty())Status=FText::Format(User(TEXT("{0} · {1}")),Status,T(*(TEXT("Errors.")+Failure)));
    }
    Summary->SetText(FText::Format(User(TEXT("{0}\n{1}\n{2}")),ExecutionUI::Summary(Current),Status,Mode));
}

void UMapExecutionWidget::LocateExecution()
{
    if(FocusExecution())FocusedExecution=ExecutionGroupKey(Execution);
}

bool UMapExecutionWidget::FocusExecution()
{
    auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer());
    auto* Manager=PC?PC->GetCommandScreenManager():nullptr;
    auto* Map=Manager?Manager->GetMapService():nullptr;
    if(!Execution || !Map || Map->IsRouteEditCameraLocked())return false;
    const auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    const auto* Registry=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
    auto* Coordinates=Registry->GetCoordinateService().GetObject();
    if(!Coordinates || !ICoordinateService::Execute_IsCoordinateSystemReady(Coordinates))return false;

    FBox Bounds(ForceInit);
    const FString Group=ExecutionGroupKey(Execution);
    auto Include=[&](const TSharedPtr<FJsonObject>& P){
        if(P)Bounds+=ICoordinateService::Execute_GeographicToWorld(Coordinates,
            P->GetNumberField(TEXT("latitude")),P->GetNumberField(TEXT("longitude")),EllipsoidAltitude(P));
    };
    const auto All=Object(Sync->GetPlans(),TEXT("executions"));
    if(All)for(const auto& Item:All->Values){
        const auto Member=Item.Value->AsObject();
        if(ExecutionGroupKey(Member)!=Group)continue;
        if(const auto Route=Object(Member,TEXT("route_snapshot")))
            for(const auto& Waypoint:Route->GetArrayField(TEXT("waypoints")))Include(Waypoint->AsObject());
        Include(Object(Member,TEXT("home_position")));
        Include(Object(Member,TEXT("position")));
    }
    // Free movement keeps the remaining fleet visible for the next selection.
    if(Field(Execution,TEXT("control_kind"))==TEXT("GROUP_MOVE"))
        for(const auto& Drone:Registry->GetFriendlyDroneDescriptors()){
            FVector Geographic=FVector::ZeroVector;
            if(MapMove::Position(Registry,Sync->GetPlans(),Drone.DroneId,Geographic))
                Bounds+=ICoordinateService::Execute_GeographicToWorld(Coordinates,Geographic.Y,Geographic.X,Geographic.Z);
        }
    return Map->FocusExecutionBounds(Bounds);
}
int32 UMapExecutionWidget::NativePaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Clip,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool Enabled) const {
    const int32 Base=Super::NativePaint(Args,G,Clip,Out,Layer,Style,Enabled);if(!Execution)return Base;
    const FGeometry Desktop=MapPaint::DesktopGeometry(G,Args);
    auto* C=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetCoordinateService().GetObject();if(!C || !ICoordinateService::Execute_IsCoordinateSystemReady(C))return Base;
    const auto* PaintPC=Cast<ADroneOpsPlayerController>(GetOwningPlayer());const auto* FrameShell=PaintPC && PaintPC->GetCommandScreenManager()?PaintPC->GetCommandScreenManager()->GetMapShell():nullptr;
    if(FrameShell)Out.PushClip(FSlateClippingZone(MapPaint::ClipGeometry(G,Desktop,FrameShell->GetMapViewportGeometry())));
    auto ProjectGeographic=[&](const FVector& Position,FVector2D& Screen){
        const auto World=ICoordinateService::Execute_GeographicToWorld(C,Position.Y,Position.X,Position.Z);
        FVector2D View;if(!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(),World,View,false))return false;
        // ProjectWorldLocationToWidgetPosition already returns DPI-adjusted viewport
        // coordinates. This widget fills that viewport; desktop window origins must
        // not be applied a second time (especially with side-by-side windows).
        Screen=View;const auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer());const auto* Shell=PC && PC->GetCommandScreenManager()?PC->GetCommandScreenManager()->GetMapShell():nullptr;
        return !Shell || Shell->IsPointOverMap(Desktop.LocalToAbsolute(Screen));
    };
    auto Project=[&](const TSharedPtr<FJsonObject>& P,FVector2D& Screen){
        return P && ProjectGeographic(FVector(P->GetNumberField(TEXT("longitude")),P->GetNumberField(TEXT("latitude")),EllipsoidAltitude(P)),Screen);
    };
    auto FleetOwnsHud=[this](const TSharedPtr<FJsonObject>& E){
        return !ManagedFleetGroup.IsEmpty() && Field(E,TEXT("group_id"))==ManagedFleetGroup;
    };
    for(const auto& Presentation:PresentedRoutes){
        if(!Presentation.Route || FleetOwnsHud(Presentation.Members[0]) || IsPresentationSuppressed(Presentation))continue;
        const auto& Points=Presentation.Route->GetArrayField(TEXT("waypoints"));
        if(Points.IsEmpty())continue;
        int32 Completed=Points.Num();
        TMap<int32,TArray<FString>> WaypointMembers;
        for(const auto& E:Presentation.Members){
            if(IsRouteSuppressed(E))continue;
            int32 MemberCompleted=0;
            int32 Waypoint=0;
            FReplaySample Sample;
            if(ReplaySample(E,Sample)){
                MemberCompleted=Sample.CompletedWaypoints;
                Waypoint=Sample.CurrentWaypoint;
            }else{
                E->TryGetNumberField(TEXT("completed_waypoints"),MemberCompleted);
                E->TryGetNumberField(TEXT("current_waypoint"),Waypoint);
            }
            Completed=FMath::Min(Completed,MemberCompleted);
            if(Presentation.bMissionGroup && Points.IsValidIndex(Waypoint-1)){
                const FString Uav=Field(E,TEXT("uav_id"));
                if(!Uav.IsEmpty())WaypointMembers.FindOrAdd(Waypoint-1).AddUnique(Uav);
            }
        }
        for(int32 I=0;I<Points.Num();++I){
            FVector2D P;if(!Project(Points[I]->AsObject(),P))continue;
            const FLinearColor Color=I<Completed?FLinearColor(.2f,1,.55f):I==Completed?FLinearColor(1,.75f,.1f):FLinearColor(.5f,.7f,1);
            FSlateDrawElement::MakeBox(Out,Base+2,G.ToPaintGeometry(FVector2D(12,12),FSlateLayoutTransform(P-FVector2D(6,6))),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,Color);
            const FString Caption=FString::Printf(TEXT("WP%d %s"),I+1,I<Completed?TEXT("✓"):I==Completed?TEXT("●"):TEXT("○"));
            FSlateDrawElement::MakeText(Out,Base+3,G.ToPaintGeometry(FVector2D(130,24),FSlateLayoutTransform(P+FVector2D(10,-12))),Caption,FCoreStyle::GetDefaultFontStyle("Bold",14),ESlateDrawEffect::None,Color);
            if(const auto* Uavs=WaypointMembers.Find(I)){
                const FString Members=FString::Join(*Uavs,TEXT(" / "));
                FSlateDrawElement::MakeText(Out,Base+4,G.ToPaintGeometry(FVector2D(240,22),FSlateLayoutTransform(P+FVector2D(10,8))),Members,FCoreStyle::GetDefaultFontStyle("Bold",12),ESlateDrawEffect::None,MapUI::Cyan);
            }
        }
    }
    // Registered aircraft keep their telemetry-driven fleet markers.
    auto* Registry=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
    const auto Plans=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetPlans();
    for(const auto& Current:RenderedExecutions){
        if(IsRouteSuppressed(Current) || FleetOwnsHud(Current))continue;
        const FString UAVId=Field(Current,TEXT("uav_id"));int32 RegisteredId=0;if(UAVId.StartsWith(TEXT("UAV-")))LexTryParseString(RegisteredId,*UAVId.Mid(4));
        FReplaySample Sample;
        const bool Historical=ReplaySample(Current,Sample);
        FVector RegisteredPosition=FVector::ZeroVector;
        const bool FleetOwnsLabel=!Historical && Registry->IsDroneRegistered(RegisteredId) && MapMove::Position(Registry,Plans,RegisteredId,RegisteredPosition);
        FVector2D UAV;
        if(!FleetOwnsLabel && (Historical?ProjectGeographic(Sample.Geographic,UAV):Project(Object(Current,TEXT("position")),UAV))){
            const auto Color=Historical?MapUI::Cyan:FLinearColor::Yellow;
            const FText Caption=Historical?FText::Format(SimulationUI::Label(TEXT("{0} · Replay"),TEXT("{0} · 回放")),User(UAVId)):User(UAVId);
            const TArray<FVector2D> Diamond{UAV+FVector2D(0,-13),UAV+FVector2D(13,0),UAV+FVector2D(0,13),UAV+FVector2D(-13,0),UAV+FVector2D(0,-13)};
            FSlateDrawElement::MakeLines(Out,Base+4,G.ToPaintGeometry(),Diamond,ESlateDrawEffect::None,Color,true,4);
            FSlateDrawElement::MakeText(Out,Base+4,G.ToPaintGeometry(FVector2D(220,24),FSlateLayoutTransform(UAV+FVector2D(16,12))),Caption,FCoreStyle::GetDefaultFontStyle("Bold",16),ESlateDrawEffect::None,Color);
        }
    }
    if(FrameShell)Out.PopClip();return Base+4;
}

void UMapExecutionWidget::NativeDestruct(){
    VisualRoutes.Reset();
    Super::NativeDestruct();
}
void UMapExecutionWidget::SetManagedFleetGroup(const FString& Group,const TArray<int32>& PreviewMembers){
    FleetPreviewUavs.Reset();for(int Id:PreviewMembers)FleetPreviewUavs.Add(FString::Printf(TEXT("UAV-%02d"),Id));
    ManagedFleetGroup=Group;if(SummaryPanel)SummaryPanel->SetVisibility(Execution && !Group.IsEmpty() && Field(Execution,TEXT("group_id"))==Group?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
    for(const auto& Presentation:PresentedRoutes)
        if(auto* Path=VisualRoutes.Find(Presentation.Key))Path->SetActorHiddenInGame(IsPresentationSuppressed(Presentation));
}

bool UMapExecutionWidget::IsRouteSuppressed(const TSharedPtr<FJsonObject>& E) const {
    // An explicit move preview owns this UAV's route until cancellation/ACK,
    // including a deployed mission being reviewed for takeover.
    if(FleetPreviewUavs.Contains(Field(E,TEXT("uav_id"))))return true;
    return !SuppressedPlan.IsEmpty() && Field(E,TEXT("plan_id"))==SuppressedPlan
        && (bPlanMoving || Field(E,TEXT("mission_id"))==SuppressedMission);
}
bool UMapExecutionWidget::OwnsMission(const FString& Plan,const FString& Mission) const {
    for(const auto& Presentation:PresentedRoutes){
        const auto& First=Presentation.Members[0];
        if(Field(First,TEXT("plan_id"))!=Plan || Field(First,TEXT("mission_id"))!=Mission)continue;
        const auto* Path=VisualRoutes.Find(Presentation.Key);
        if(IsValid(Path) && !Path->IsHidden())return true;
        for(const auto& E:Presentation.Members)
            if(FleetPreviewUavs.Contains(Field(E,TEXT("uav_id"))))return true;
    }
    // A retired execution still owns the decision to hide the original task's
    // route. Do not resurrect the saved route when its only UAV was taken over.
    return ExecutionUI::RetiredMissionOwnsRoute(GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetPlans(),Plan,Mission);
}

bool UMapExecutionWidget::IsPresentationSuppressed(const FPresentedRoute& Presentation) const {
    return !Presentation.Members.ContainsByPredicate([this](const auto& E){return !IsRouteSuppressed(E);});
}

bool UMapExecutionWidget::IsOverControls(const FVector2D& Screen) const {return IsVisible() && SummaryPanel && SummaryPanel->IsVisible() && SummaryPanel->GetCachedGeometry().IsUnderLocation(Screen);}
