#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Command/CommandShellWidget.h"
#include "Command/CommandScreenManager.h"
#include "Command/CommandCenterPanel.h"
#include "Command/CommandTacticalMap.h"
#include "Command/CommandActionButton.h"
#include "Command/FigmaFleetWidget.h"
#include "Command/GroupControlWidget.h"
#include "Command/CommandAlertStore.h"
#include "Blueprint/WidgetTree.h"
#include "Shared/SecurityPlanWorkspaceWidget.h"
#include "Shared/OperationalContextSubsystem.h"
#include "Shared/OperationalEventStore.h"
#include "Shared/PlanWidgetSupport.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "Components/WidgetSwitcher.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Editor.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Settings/LevelEditorPlaySettings.h"

DEFINE_LATENT_AUTOMATION_COMMAND(FFigmaStartPIE);
bool FFigmaStartPIE::Update(){FRequestPlaySessionParams P;P.EditorPlaySettings=DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(),GetTransientPackage());P.EditorPlaySettings->NewWindowWidth=1920;P.EditorPlaySettings->NewWindowHeight=1080;P.EditorPlaySettings->CenterNewWindow=true;GUnrealEd->RequestPlaySession(P);return true;}

// Exercises widget actions against the isolated Mock Backend. Captures are
// rendered automation evidence; this does not claim native mouse interaction.
class FFigmaCommandChecks : public IAutomationLatentCommand {
    FAutomationTestBase* Test;int32 Phase=0;double Start=0,Next=0;FString VideoBefore,LabelBefore,Out;int32 DroneId=0;int64 SelectionVersion=-1;
    void CheckGroupMoveEnabled(UFigmaFleetWidget* Fleet,bool Expected){
        UCommandActionButton* Move=nullptr;int32 Matches=0;
        if(Fleet->GroupControls && Fleet->GroupControls->WidgetTree)
            Fleet->GroupControls->WidgetTree->ForEachWidget([&](UWidget* Widget){
                auto* Button=Cast<UCommandActionButton>(Widget);
                auto* Caption=Button?Cast<UTextBlock>(Button->GetContent()):nullptr;
                if(Caption && Caption->GetText().EqualTo(ProductText::Get(TEXT("Group.Move")))){Move=Button;++Matches;}
            });
        Test->TestEqual(TEXT("Fleet has one real group move button"),Matches,1);
        if(Move)Test->TestEqual(Expected?TEXT("Acknowledged singleton enables Fleet group move"):TEXT("Acknowledged empty selection disables Fleet group move"),Move->GetIsEnabled(),Expected);
    }
    void Capture(const TCHAR* Name){
        FSlateApplication::Get().InvalidateAllWidgets(false);
        TArray<FColor> Pixels;FIntVector Size=FIntVector::ZeroValue;
        auto V=GEngine->GameViewport?GEngine->GameViewport->GetGameViewportWidget():nullptr;
        if(!V || !FSlateApplication::Get().TakeScreenshot(V.ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Rendered Figma screenshot unavailable"));return;}
        TArray64<uint8> Bytes;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Bytes);
        Test->TestTrue(TEXT("Screenshot persisted"),FFileHelper::SaveArrayToFile(Bytes,*(Out/(FString(Name)+TEXT(".png")))));
        Test->AddInfo(FString::Printf(TEXT("Rendered screenshot %s %dx%d"),Name,Size.X,Size.Y));
    }
public:
    explicit FFigmaCommandChecks(FAutomationTestBase* T):Test(T){Out=FPaths::ProjectSavedDir()/TEXT("FigmaQA")/FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S"));IFileManager::Get().MakeDirectory(*Out,true);Test->AddInfo(TEXT("Figma evidence: ")+Out);}
    bool Update() override {
        const double Now=FPlatformTime::Seconds();if(Start==0)Start=Now;if(Now-Start>70){Test->AddError(TEXT("Figma interaction timed out"));return true;}if(Now<Next)return false;
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();if(!W)return false;
        auto* PC=Cast<ADroneOpsPlayerController>(W->GetFirstPlayerController());auto* M=PC?PC->GetCommandScreenManager():nullptr;auto* Shell=M?M->GetShell():nullptr;if(!Shell)return false;
        auto* GI=W->GetGameInstance();auto* Sync=GI->GetSubsystem<UOperationalContextSubsystem>();auto* R=GI->GetSubsystem<UDroneRegistrySubsystem>();if(!Sync->IsHydrated())return false;
        auto* Plan=Shell->CenterPanel->GetPlanWorkspace();auto* Fleet=Shell->FigmaFleet.Get();Shell->Refresh();
        if(Phase==0){Test->TestEqual(TEXT("Three design pages"),Shell->WorkspacePages->GetChildrenCount(),3);Test->TestEqual(TEXT("Three primary navigation choices"),Shell->PrimaryNavigationButtons.Num(),3);Test->TestEqual(TEXT("Registry count projected"),Shell->StatisticValues[0]->GetText().ToString(),FText::AsNumber(R->GetFriendlyDroneDescriptors().Num()).ToString());VideoBefore=PlanUI::Field(Sync->GetVideoView(),TEXT("video_target_uav_id"));Shell->Navigate(TEXT("navigate"),0);Sync->SetLanguage(TEXT("zh-Hans"));}
        else if(Phase==1){Capture(TEXT("01-overview"));const int Z=Shell->SituationMap->GetZoom();Shell->FigmaAction(TEXT("zoom_in"),0);Test->TestEqual(TEXT("Zoom action operates map"),Shell->SituationMap->GetZoom(),FMath::Min(Z+1,19));Shell->FigmaAction(TEXT("zoom_out"),0);Shell->FigmaAction(TEXT("devices"),0);}
        else if(Phase==2){Capture(TEXT("02-devices"));Test->TestEqual(TEXT("Device table uses Registry"),Shell->DialogRows->GetChildrenCount(),FMath::Max(1,R->GetFriendlyDroneDescriptors().Num()));Shell->FigmaAction(TEXT("alerts"),0);}
        else if(Phase==3){Capture(TEXT("03-alerts"));Shell->FigmaAction(TEXT("logs"),0);}
        else if(Phase==4){Capture(TEXT("04-logs"));Test->TestTrue(TEXT("Dialog blocks page"),Shell->FigmaDialog->IsVisible());Shell->FigmaAction(TEXT("close_dialog"),0);Test->TestEqual(TEXT("Closing releases overlay"),Shell->FigmaDialog->GetVisibility(),ESlateVisibility::Collapsed);Shell->Navigate(TEXT("navigate"),1);}
        else if(Phase==5){Capture(TEXT("05-plans"));Plan->Action(TEXT("new_plan"),0);}
        else if(Phase==6){if(!Plan->bCreating)return false;Capture(TEXT("06-new-plan"));Plan->PlanName->SetText(FText::FromString(TEXT("   ")));Plan->Refresh();Test->TestFalse(TEXT("Whitespace-only name cannot create"),Plan->Actions[TEXT("create_plan")]->GetIsEnabled());Plan->PlanName->SetText(FText::GetEmpty());
            Plan->DescriptionEditor->SetText(FText::FromString(TEXT("First line\nSecond line")));
            // SetText updates a field programmatically; emulate the native edit notification.
            Plan->DescriptionEditor->OnTextChanged.Broadcast(Plan->DescriptionEditor->GetText());
            Test->TestEqual(TEXT("Multiline description reaches configuration draft"),Plan->Description->GetText().ToString(),FString(TEXT("First line\nSecond line")));
            Test->TestTrue(TEXT("Multiline edit participates in unsaved guard"),Plan->HasUnsavedChanges());Plan->Action(TEXT("cancel_new"),0);
            Test->TestTrue(TEXT("Cancel opens unsaved guard above modal"),Plan->bConfigurationGuard);Plan->Action(TEXT("guard_keep"),0);
            Test->TestEqual(TEXT("Keep retains multiline draft"),Plan->DescriptionEditor->GetText().ToString(),FString(TEXT("First line\nSecond line")));
            Plan->Action(TEXT("cancel_new"),0);Plan->Action(TEXT("guard_discard"),0);}
        else if(Phase==7){if(Plan->IsConfiguring())return false;Shell->Navigate(TEXT("navigate"),2);}
        else if(Phase==8){Capture(TEXT("07-fleet"));const auto Ds=R->GetFriendlyDroneDescriptors();if(Ds.IsEmpty()){Test->AddError(TEXT("Mock fixture has no UAVs"));return true;}DroneId=Ds[0].DroneId;FDroneLabelSettings Settings;R->GetDroneLabelSettings(DroneId,Settings);LabelBefore=Settings.DisplayName;Fleet->OpenLabelDialog(DroneId);}
        else if(Phase==9){Capture(TEXT("08-label"));Fleet->LabelName->SetText(FText::FromString(TEXT("unsaved label test")));Fleet->Action(TEXT("label_cancel"),0);FDroneLabelSettings Settings;R->GetDroneLabelSettings(DroneId,Settings);Test->TestEqual(TEXT("Cancel retains persisted label"),Settings.DisplayName,LabelBefore);Fleet->Action(TEXT("primary"),DroneId);}
        else if(Phase==10){if(R->GetPrimarySelectedDrone()!=DroneId)return false;Test->TestEqual(TEXT("Primary selection leaves video target independent"),PlanUI::Field(Sync->GetVideoView(),TEXT("video_target_uav_id")),VideoBefore);Sync->SetLanguage(TEXT("en"));}
        else if(Phase==11){Capture(TEXT("09-fleet-en"));Test->TestEqual(TEXT("Three pages remain after localization"),Shell->WorkspacePages->GetChildrenCount(),3);
            SelectionVersion=Sync->GetVersion();if(!Test->TestTrue(TEXT("Request empty Fleet selection through Backend"),Sync->SetLocalSelection(0,{})))return true;}
        else if(Phase==12){
            if(Sync->GetVersion()<=SelectionVersion || R->GetPrimarySelectedDrone()!=0 || !R->GetMultiSelectedDrones().IsEmpty() || Fleet->bSelectionPending)return false;
            CheckGroupMoveEnabled(Fleet,false);
            auto* Row=Fleet->Rows.Find(DroneId);if(!Test->TestTrue(TEXT("Singleton selection checkbox exists and is enabled"),Row && Row->Toggle && Row->Toggle->GetIsEnabled()))return true;
            SelectionVersion=Sync->GetVersion();Row->Toggle->OnClicked.Broadcast();
            Test->TestTrue(TEXT("Fleet checkbox waits for selection acknowledgement"),Fleet->bSelectionPending);
            Test->TestTrue(TEXT("Pending singleton does not replace acknowledged empty selection"),R->GetMultiSelectedDrones().IsEmpty());
        }
        else if(Phase==13){
            if(Sync->GetVersion()<=SelectionVersion || R->GetPrimarySelectedDrone()!=DroneId || R->GetMultiSelectedDrones()!=TArray<int32>{DroneId} || Fleet->bSelectionPending)return false;
            Test->TestTrue(TEXT("Singleton move fixture is synchronized"),Sync->IsReady() && Sync->IsHydrated());
            Test->TestFalse(TEXT("Singleton move fixture has no configuration lease"),Plan->IsConfiguring());
            CheckGroupMoveEnabled(Fleet,true);
            Test->TestEqual(TEXT("Empty and singleton selection keep Video independent"),PlanUI::Field(Sync->GetVideoView(),TEXT("video_target_uav_id")),VideoBefore);
            return true;
        }
        ++Phase;Next=Now+1.5;return false;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFigmaCommandPresentation,"DroneOps.Figma.CommandPresentation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFigmaCommandPresentation::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Tests/P55OfflineMap")));ADD_LATENT_AUTOMATION_COMMAND(FFigmaStartPIE());ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(8));ADD_LATENT_AUTOMATION_COMMAND(FFigmaCommandChecks(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}

// Deliberately a client-replica presentation fixture. Backend execution/occupancy
// is exercised separately; these rows must remain complete as snapshots change.
class FOverviewTasksChecks : public IAutomationLatentCommand {
    FAutomationTestBase* Test;double Start=FPlatformTime::Seconds(),Next=0;int Phase=0,OldTarget=0;FString VideoBefore,Out;
    TSharedPtr<FJsonObject> Fixture,Executions;
    void Add(const TCHAR* Id,const TCHAR* Group,const TCHAR* State,const TCHAR* UAV,double Created,bool Mission=true){
        auto E=MakeShared<FJsonObject>();E->SetStringField(TEXT("execution_id"),Id);E->SetStringField(TEXT("group_id"),Group);
        E->SetStringField(TEXT("state"),State);E->SetStringField(TEXT("uav_id"),UAV);E->SetNumberField(TEXT("created_at"),Created);E->SetNumberField(TEXT("progress"),.25);
        E->SetBoolField(TEXT("simulation"),true);E->SetNumberField(TEXT("control_version"),1);E->SetNumberField(TEXT("completed_waypoints"),0);E->SetNumberField(TEXT("current_waypoint"),1);E->SetNumberField(TEXT("total_waypoints"),3);
        if(Mission){E->SetStringField(TEXT("plan_id"),TEXT("presentation-plan"));E->SetStringField(TEXT("mission_id"),Group);E->SetStringField(TEXT("plan_name"),TEXT("园区安保方案"));E->SetStringField(TEXT("mission_name"),FString(Group)==TEXT("scheduled")?TEXT("夜间巡逻"):TEXT("周界巡检"));}
        Executions->SetObjectField(Id,E);
    }
    void Apply(UOperationalContextSubsystem* Sync){Fixture=MakeShared<FJsonObject>(*Fixture);Fixture->SetObjectField(TEXT("executions"),Executions);Fixture->SetNumberField(TEXT("execution_version"),Fixture->GetNumberField(TEXT("execution_version"))+1);Sync->ApplyPlans(Fixture);}
public:
    explicit FOverviewTasksChecks(FAutomationTestBase* T):Test(T){Out=FPaths::ProjectSavedDir()/TEXT("UIFollowupQA")/FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S"));IFileManager::Get().MakeDirectory(*Out,true);Test->AddInfo(TEXT("Overview presentation evidence: ")+Out);}
    bool Update() override {
        const double Now=FPlatformTime::Seconds();if(Now-Start>45){Test->AddError(TEXT("Overview presentation timed out"));return true;}if(Now<Next)return false;
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();if(!W)return false;
        auto* PC=Cast<ADroneOpsPlayerController>(W->GetFirstPlayerController());auto* Manager=PC?PC->GetCommandScreenManager():nullptr;auto* Shell=Manager?Manager->GetShell():nullptr;if(!Shell)return false;
        auto* Sync=W->GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();if(!Sync->IsHydrated())return false;
        if(Phase==0){
            VideoBefore=PlanUI::Field(Sync->GetVideoView(),TEXT("video_target_uav_id"));Shell->Navigate(TEXT("navigate"),0);
            Fixture=MakeShared<FJsonObject>(*Sync->GetPlans());Fixture->SetNumberField(TEXT("execution_version"),Fixture->GetNumberField(TEXT("execution_version"))+10000);Executions=MakeShared<FJsonObject>();
            Add(TEXT("m1"),TEXT("patrol"),TEXT("EXECUTING"),TEXT("UAV-01"),10);Add(TEXT("m2"),TEXT("patrol"),TEXT("PAUSED"),TEXT("UAV-02"),10);
            Add(TEXT("free"),TEXT("free-move"),TEXT("EXECUTING"),TEXT("UAV-03"),20,false);Add(TEXT("night"),TEXT("scheduled"),TEXT("SCHEDULED"),TEXT("UAV-04"),30);
            Add(TEXT("old"),TEXT("completed"),TEXT("COMPLETED"),TEXT("UAV-04"),40);Apply(Sync);Shell->Refresh();
            Test->TestEqual(TEXT("Overview lists every active task group, excludes terminal history"),Shell->RunningTaskRows->GetChildrenCount(),3);
            if(Shell->RunningGroupIds.Num()!=3 || Shell->RunningTaskNames.Num()!=3 || Shell->RunningTaskButtons.Num()!=3)return true;
            Test->TestEqual(TEXT("Newest active task first, not completed history"),Shell->RunningGroupIds[0],FString(TEXT("scheduled")));
            Test->TestTrue(TEXT("Grouped task shows both members"),Shell->RunningTaskDetails[2]->GetText().ToString().Contains(TEXT("UAV-01, UAV-02")));
            Test->TestTrue(TEXT("Mixed execution states visible"),Shell->RunningTaskNames[2]->GetText().ToString().Contains(ProductText::Get(TEXT("Execution.Mixed")).ToString()));
            Test->TestEqual(TEXT("Free movement has no misleading mission navigation"),Shell->RunningTaskButtons[1]->GetVisibility(),ESlateVisibility::Collapsed);
            for(const auto& Target:Shell->RunningTaskTargets)if(Target.Value==TEXT("scheduled"))OldTarget=Target.Key;
            Test->TestTrue(TEXT("Scheduled mission has stable navigation identity"),OldTarget>0);
        } else if(Phase==1){
            Test->TestTrue(TEXT("Map remains usable with three active tasks and global execution controls"),Shell->SituationMap->GetCachedGeometry().GetLocalSize().Y>=220.f);
            Test->TestTrue(TEXT("All three task rows have room in the side panel"),Shell->RunningTaskRows->GetCachedGeometry().GetLocalSize().Y>=144.f);
            FSlateApplication::Get().InvalidateAllWidgets(false);TArray<FColor> Pixels;FIntVector Size=FIntVector::ZeroValue;auto V=GEngine->GameViewport?GEngine->GameViewport->GetGameViewportWidget():nullptr;
            if(V && FSlateApplication::Get().TakeScreenshot(V.ToSharedRef(),Pixels,Size)){TArray64<uint8> Bytes;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Bytes);Test->TestTrue(TEXT("Active task list rendered screenshot saved"),FFileHelper::SaveArrayToFile(Bytes,*(Out/TEXT("01-overview-active-tasks.png"))));}else Test->AddError(TEXT("Overview rendered screenshot unavailable"));
            Executions=MakeShared<FJsonObject>(*Executions);auto Updated=MakeShared<FJsonObject>(*Executions->GetObjectField(TEXT("m1")));Updated->SetNumberField(TEXT("progress"),.75);Executions->SetObjectField(TEXT("m1"),Updated);Apply(Sync);Shell->Refresh();
            Test->TestTrue(TEXT("Progress refreshes within the existing group"),Shell->RunningTaskNames[2]->GetText().ToString().Contains(FText::AsPercent(.5).ToString()));
            // All rows disappear, then a different mission occupies row zero.
            Executions=MakeShared<FJsonObject>();Apply(Sync);Shell->Refresh();Test->TestEqual(TEXT("Completed/removed tasks leave no stale rows"),Shell->RunningTaskRows->GetChildrenCount(),0);Test->TestEqual(TEXT("Empty state restored"),Shell->RunningTaskEmpty->GetVisibility(),ESlateVisibility::Visible);
            Add(TEXT("replacement"),TEXT("replacement"),TEXT("PAUSED"),TEXT("UAV-01"),50);Apply(Sync);Shell->Refresh();Shell->OpenRunningTask(TEXT("open_task"),OldTarget);
            Test->TestEqual(TEXT("Queued stale row action cannot open replacement task"),Shell->WorkspacePages->GetActiveWidgetIndex(),0);
            Test->TestFalse(TEXT("Removed action identity never retargeted"),Shell->RunningTaskTargets.Contains(OldTarget));
            Test->TestEqual(TEXT("Overview presentation preserves independent video target"),PlanUI::Field(Sync->GetVideoView(),TEXT("video_target_uav_id")),VideoBefore);
            Test->AddInfo(TEXT("Presentation fixture only; no native mouse or real UAV execution claimed."));return true;
        }
        ++Phase;Next=Now+2.;return false;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverviewActiveTasks,"DroneOps.Figma.ActiveTaskOverview",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOverviewActiveTasks::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Tests/P55OfflineMap")));ADD_LATENT_AUTOMATION_COMMAND(FFigmaStartPIE());ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(8));ADD_LATENT_AUTOMATION_COMMAND(FOverviewTasksChecks(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
