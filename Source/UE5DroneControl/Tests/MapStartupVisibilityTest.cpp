#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Map/MapShellWidget.h"
#include "Map/MapFleetInteractionWidget.h"
#include "Map/MapMoveSupport.h"
#include "Command/CommandScreenManager.h"
#include "Command/CommandMapInteractionService.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "DroneOps/Core/ICoordinateService.h"
#include "Shared/OperationalContextSubsystem.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Editor.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Settings/LevelEditorPlaySettings.h"

DEFINE_LATENT_AUTOMATION_COMMAND(FMapStartupStartPIE);
bool FMapStartupStartPIE::Update()
{
    FRequestPlaySessionParams Params;
    Params.EditorPlaySettings=DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(),GetTransientPackage());
    Params.EditorPlaySettings->NewWindowWidth=1280;
    Params.EditorPlaySettings->NewWindowHeight=800;
    Params.EditorPlaySettings->CenterNewWindow=true;
    GUnrealEd->RequestPlaySession(Params);
    return true;
}

// Actual CesiumWorld startup, isolated Backend and rendered Slate evidence.
// No fixture actor, camera positioning, registry mutation, selection, or native
// mouse synthesis precedes the first observation. External Cesium errors remain errors.
class FMapStartupVisibility : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Started=FPlatformTime::Seconds(),ReadyAt=0,LocateAt=0;
    int32 Phase=0,LocateId=0,InitialPrimary=0,ClickId=1;
    double ActionAt=0;
    bool WaitingForClick=false;
    TArray<int32> InitialSelection;
    FString Out,InitialVideo;
    TSharedRef<FJsonObject> Report=MakeShared<FJsonObject>();

    static void Vector(const TSharedRef<FJsonObject>& Json,const TCHAR* Name,const FVector& Value)
    {
        auto V=MakeShared<FJsonObject>();V->SetNumberField(TEXT("x"),Value.X);
        V->SetNumberField(TEXT("y"),Value.Y);V->SetNumberField(TEXT("z"),Value.Z);
        Json->SetObjectField(Name,V);
    }
    static void Point(const TSharedRef<FJsonObject>& Json,const TCHAR* Name,const FVector2D& Value)
    {
        auto V=MakeShared<FJsonObject>();V->SetNumberField(TEXT("x"),Value.X);
        V->SetNumberField(TEXT("y"),Value.Y);Json->SetObjectField(Name,V);
    }
    static FString Video(UOperationalContextSubsystem* Sync)
    {
        FString Result;if(Sync && Sync->GetVideoView())
            FJsonSerializer::Serialize(Sync->GetVideoView().ToSharedRef(),TJsonWriterFactory<>::Create(&Result));
        return Result;
    }
    void CheckFormation(UDroneRegistrySubsystem* Registry,UOperationalContextSubsystem* Sync)
    {
        const auto Baseline=PlanUI::Object(Sync->GetPlans(),TEXT("mock_position_baseline"));
        const auto Positions=PlanUI::Object(Baseline,TEXT("positions"));
        double Epoch=0;const bool HasEpoch=Baseline && Baseline->TryGetNumberField(TEXT("epoch"),Epoch) && Epoch>0;
        Test->TestTrue(TEXT("Launcher Mock startup has an authoritative reset epoch"),HasEpoch);
        Report->SetNumberField(TEXT("mock_reset_epoch"),Epoch);
        Test->TestTrue(TEXT("Startup reset contains four aircraft"),Positions && Positions->Values.Num()==4);
        FVector Previous=FVector::ZeroVector;
        for(int32 Id=1;Id<=4;++Id)
        {
            FVector G=FVector::ZeroVector;const bool Valid=MapMove::Position(Registry,Sync->GetPlans(),Id,G);
            Test->TestTrue(*FString::Printf(TEXT("Reset UAV %d has accepted position"),Id),Valid);
            const auto Position=PlanUI::Find(Baseline,TEXT("positions"),MapMove::UAV(Id));double Lon=0,Lat=0,Alt=0;
            const bool HasReset=Position && Position->TryGetNumberField(TEXT("longitude"),Lon)
                && Position->TryGetNumberField(TEXT("latitude"),Lat) && Position->TryGetNumberField(TEXT("altitude"),Alt);
            Test->TestTrue(*FString::Printf(TEXT("Reset UAV %d uses the new baseline, not historical execution"),Id),
                Valid && HasReset && G.Equals(FVector(Lon,Lat,Alt),.000001));
            if(Id>1 && Valid)
            {
                constexpr double MetresPerDegree=6371000.*3.14159265358979323846/180.;
                const double EastMetres=(G.X-Previous.X)*MetresPerDegree*FMath::Cos(FMath::DegreesToRadians(G.Y));
                Test->TestTrue(*FString::Printf(TEXT("UAV %d follows numeric ID order east at 2 m spacing"),Id),
                    FMath::IsNearlyEqual(EastMetres,2.,.005) && FMath::Abs(G.Y-Previous.Y)<1.e-8 && FMath::Abs(G.Z-Previous.Z)<.01);
            }
            Previous=G;
        }
    }
    bool MapHit(UMapShellWidget* Shell,UMapFleetInteractionWidget* Fleet,const FVector2D& Local) const
    {
        const FVector2D Screen=Fleet->GetCachedGeometry().LocalToAbsolute(Local);
        if(!Shell->IsPointOverMap(Screen))return false;
        auto* Client=Fleet->GetWorld()->GetGameViewport();const auto View=Client?Client->GetGameViewportWidget():nullptr;
        const auto Window=Client?Client->GetWindow():nullptr;if(!View || !Window)return false;
        const TArray<TSharedRef<SWindow>> Windows{Window.ToSharedRef()};
        const auto Path=FSlateApplication::Get().LocateWindowUnderMouse(Screen,Windows);
        return Path.IsValid() && Path.Widgets.Last().Widget==View;
    }
    void CheckSymbolSeparation(UMapShellWidget* Shell)
    {
        auto* Fleet=Shell->GetFleetInteraction();TArray<FVector2D> Points;
        for(int32 Id=1;Id<=4;++Id)
        {
            FVector2D P=FVector2D::ZeroVector;
            const bool Projects=Fleet->Project(Id,P);
            Test->TestTrue(*FString::Printf(TEXT("UAV %d center reaches actual Slate viewport without UI interception"),Id),
                Projects && MapHit(Shell,Fleet,P));
            if(Projects)Points.Add(P);
        }
        for(int32 A=0;A<Points.Num();++A)for(int32 B=A+1;B<Points.Num();++B)
            Test->TestTrue(TEXT("Fixed 28 logical pixel UAV symbols and 36 pixel rings have independent screen footprints"),
                FVector2D::Distance(Points[A],Points[B])>=36.);
    }
    void Save()
    {
        FString Json;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));
        Test->TestTrue(TEXT("Startup diagnostic JSON saved"),FFileHelper::SaveStringToFile(Json,*(Out/TEXT("startup.json"))));
    }
    void Capture(const TCHAR* Name)
    {
        const auto View=GEngine->GameViewport?GEngine->GameViewport->GetGameViewportWidget():nullptr;
        TArray<FColor> Pixels;FIntVector Size=FIntVector::ZeroValue;
        if(!View || !FSlateApplication::Get().TakeScreenshot(View.ToSharedRef(),Pixels,Size))
        {Test->AddError(TEXT("Production startup screenshot unavailable"));return;}
        TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);
        Test->TestTrue(TEXT("Production startup screenshot saved"),
            FFileHelper::SaveArrayToFile(Png,*(Out/(FString(Name)+TEXT(".png")))));
    }
    TSharedRef<FJsonObject> Observe(UWorld* World,ADroneOpsPlayerController* PC,
        UMapShellWidget* Shell,UCommandMapInteractionService* Map,UDroneRegistrySubsystem* Registry,
        UOperationalContextSubsystem* Sync,const TCHAR* Name,bool AssertVisibility)
    {
        auto Data=MakeShared<FJsonObject>();Report->SetObjectField(Name,Data);
        Data->SetStringField(TEXT("world"),World->GetMapName());
        Data->SetBoolField(TEXT("hydrated"),Sync->IsHydrated());
        Data->SetNumberField(TEXT("primary"),Registry->GetPrimarySelectedDrone());
        TArray<TSharedPtr<FJsonValue>> Selected;
        for(int32 Id:Registry->GetMultiSelectedDrones())Selected.Add(MakeShared<FJsonValueNumber>(Id));
        Data->SetArrayField(TEXT("selection"),Selected);
        const auto Frame=Shell->GetMapViewportGeometry();
        Point(Data,TEXT("map_screen_origin"),Frame.GetAbsolutePosition());
        Point(Data,TEXT("map_local_size"),Frame.GetLocalSize());
        Point(Data,TEXT("map_screen_size"),Frame.LocalToAbsolute(Frame.GetLocalSize())-Frame.GetAbsolutePosition());
        Point(Data,TEXT("viewport_pixels"),UWidgetLayoutLibrary::GetViewportSize(World));
        Data->SetNumberField(TEXT("viewport_scale"),UWidgetLayoutLibrary::GetViewportScale(World));
        Vector(Data,TEXT("camera_focus_target"),Map->ViewFocus);
        Vector(Data,TEXT("camera_focus_actual"),Map->ActualFocus);
        Data->SetNumberField(TEXT("camera_distance_target"),Map->ViewDistance);
        Data->SetNumberField(TEXT("camera_distance_actual"),Map->ActualDistance);
        if(auto* Camera=Map->GetMapCamera())
        {
            Vector(Data,TEXT("camera_location"),Camera->GetActorLocation());
            Vector(Data,TEXT("camera_rotation_pyr"),FVector(Camera->GetActorRotation().Pitch,Camera->GetActorRotation().Yaw,Camera->GetActorRotation().Roll));
            Data->SetNumberField(TEXT("camera_fov"),Camera->GetCameraComponent()->FieldOfView);
        }
        auto* Fleet=Shell->GetFleetInteraction();auto* Coordinates=Registry->GetCoordinateService().GetObject();
        TArray<TSharedPtr<FJsonValue>> PaintedIds;
        for(int32 Id:Fleet->PaintedDroneIds)PaintedIds.Add(MakeShared<FJsonValueNumber>(Id));
        Data->SetArrayField(TEXT("painted_uav_ids"),PaintedIds);
        Data->SetNumberField(TEXT("painted_count"),Fleet->PaintedDroneIds.Num());
        const bool CoordsReady=Coordinates && ICoordinateService::Execute_IsCoordinateSystemReady(Coordinates);
        Data->SetBoolField(TEXT("coordinates_ready"),CoordsReady);
        FVector2D FocusPoint=FVector2D::ZeroVector;
        Data->SetBoolField(TEXT("camera_focus_projects"),UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC,Map->ActualFocus,FocusPoint,true));
        Point(Data,TEXT("camera_focus_projected"),FocusPoint);
        int32 ValidPositions=0,Projected=0,Visible=0;bool PrimaryVisible=false,LocateVisible=false;
        FBox Bounds(ForceInit);TArray<TSharedPtr<FJsonValue>> Drones;
        for(int32 Id=1;Id<=4;++Id)
        {
            auto D=MakeShared<FJsonObject>();Drones.Add(MakeShared<FJsonValueObject>(D));D->SetNumberField(TEXT("id"),Id);
            D->SetBoolField(TEXT("painted"),Fleet->PaintedDroneIds.Contains(Id));
            FDroneTelemetrySnapshot Telemetry;const bool HasTelemetry=Registry->GetTelemetry(Id,Telemetry);
            D->SetBoolField(TEXT("telemetry_present"),HasTelemetry);
            D->SetStringField(TEXT("telemetry_source"),Telemetry.TelemetrySource);
            D->SetBoolField(TEXT("simulation"),Telemetry.bSimulation);D->SetBoolField(TEXT("gps_fix"),Telemetry.bGpsFix);
            D->SetNumberField(TEXT("availability"),static_cast<int32>(Telemetry.Availability));
            FVector Geographic=FVector::ZeroVector,WorldPosition=FVector::ZeroVector;
            const bool HasPosition=MapMove::Position(Registry,Sync->GetPlans(),Id,Geographic) && CoordsReady;
            D->SetBoolField(TEXT("accepted_position"),HasPosition);Vector(D,TEXT("accepted_geo_lon_lat_alt"),Geographic);
            if(HasPosition)
            {
                ++ValidPositions;WorldPosition=ICoordinateService::Execute_GeographicToWorld(Coordinates,Geographic.Y,Geographic.X,Geographic.Z);
                Bounds+=WorldPosition;Vector(D,TEXT("accepted_world"),WorldPosition);
                D->SetNumberField(TEXT("distance_to_focus_target_cm"),FVector::Distance(WorldPosition,Map->ViewFocus));
                FVector2D RawPoint=FVector2D::ZeroVector;
                D->SetBoolField(TEXT("raw_projection"),UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC,WorldPosition,RawPoint,true));
                Point(D,TEXT("raw_widget_point"),RawPoint);
            }
            for(int32 Kind=0;Kind<2;++Kind)
            {
                AActor* Actor=Kind==0?static_cast<AActor*>(Registry->GetSenderPawn(Id)):Registry->GetReceiverActor(Id);
                const TCHAR* Key=Kind==0?TEXT("sender"):TEXT("receiver");
                auto A=MakeShared<FJsonObject>();D->SetObjectField(Key,A);A->SetBoolField(TEXT("exists"),IsValid(Actor));
                if(IsValid(Actor))
                {
                    Vector(A,TEXT("world"),Actor->GetActorLocation());
                    if(HasPosition)A->SetNumberField(TEXT("distance_from_accepted_cm"),FVector::Distance(Actor->GetActorLocation(),WorldPosition));
                    FVector2D ActorPoint=FVector2D::ZeroVector;
                    A->SetBoolField(TEXT("projects"),UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC,Actor->GetActorLocation(),ActorPoint,true));
                    Point(A,TEXT("widget_point"),ActorPoint);
                }
            }
            FVector2D P=FVector2D::ZeroVector;const bool Projects=Fleet->Project(Id,P);
            D->SetBoolField(TEXT("fleet_projects"),Projects);Point(D,TEXT("fleet_point"),P);
            const FVector2D Absolute=Fleet->GetCachedGeometry().LocalToAbsolute(P);
            const FVector2D InFrame=Frame.AbsoluteToLocal(Absolute);
            const bool Inside=Projects && InFrame.X>=14 && InFrame.Y>=14 && InFrame.X<=Frame.GetLocalSize().X-14 && InFrame.Y<=Frame.GetLocalSize().Y-14;
            const bool Unobstructed=Inside && Shell->IsPointOverMap(Absolute);
            D->SetBoolField(TEXT("whole_marker_inside_map"),Inside);D->SetBoolField(TEXT("visible_outside_controls"),Unobstructed);
            if(Projects)++Projected;if(Unobstructed)++Visible;
            if(Id==InitialPrimary)PrimaryVisible=Unobstructed;if(Id==LocateId)LocateVisible=Unobstructed;
        }
        Data->SetArrayField(TEXT("uavs"),Drones);Data->SetNumberField(TEXT("accepted_count"),ValidPositions);
        Data->SetNumberField(TEXT("projected_count"),Projected);Data->SetNumberField(TEXT("visible_count"),Visible);
        const double Spread=Bounds.IsValid?Bounds.GetSize().Length():0.;
        Data->SetNumberField(TEXT("accepted_spread_cm"),Spread);
        if(AssertVisibility)
        {
            // Projection and pointer hit tests cannot detect a bad OnPaint
            // desktop/window transform. NativePaint records IDs only immediately
            // before the actual symbol draw, after its clipping/hit guards.
            for(int32 Id=1;Id<=4;++Id)
                Test->TestTrue(*FString::Printf(TEXT("%s: UAV %d actually submitted for symbol drawing"),Name,Id),
                    Fleet->PaintedDroneIds.Contains(Id));
            Test->TestEqual(*FString::Printf(TEXT("%s: four accepted Mock positions"),Name),ValidPositions,4);
            if(Phase==0)
            {
                Test->TestTrue(TEXT("Initial production map displays selected UAV, or at least one UAV when selection is empty"),
                    InitialPrimary>=1 && InitialPrimary<=4?PrimaryVisible:Visible>0);
                Test->TestEqual(TEXT("Initial reset Mock fleet has all four visible symbols"),Visible,4);
            }
            else if(Phase==1)
            {
                Test->TestTrue(TEXT("Explicit Locate brings accepted UAV position into visible map"),LocateVisible);
                FVector Geo=FVector::ZeroVector;
                if(CoordsReady && MapMove::Position(Registry,Sync->GetPlans(),LocateId,Geo))
                    Test->TestTrue(TEXT("Explicit Locate focuses accepted geographic position rather than stale actor location"),
                        Map->ViewFocus.Equals(ICoordinateService::Execute_GeographicToWorld(Coordinates,Geo.Y,Geo.X,Geo.Z),100.));
            }
            else Test->TestEqual(TEXT("Empty-selection Locate fits all four accepted aircraft"),Visible,4);
        }
        Test->AddInfo(FString::Printf(TEXT("%s: accepted=%d projected=%d visible=%d painted=%d primary=%d spread_cm=%.1f"),Name,ValidPositions,Projected,Visible,Fleet->PaintedDroneIds.Num(),Registry->GetPrimarySelectedDrone(),Spread));
        Save();return Data;
    }
public:
    explicit FMapStartupVisibility(FAutomationTestBase* InTest):Test(InTest)
    {
        Out=FPaths::ProjectSavedDir()/TEXT("MapStartupQA")/FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S"));
        IFileManager::Get().MakeDirectory(*Out,true);
        Report->SetStringField(TEXT("evidence_boundary"),TEXT("Production CesiumWorld RHI, actual Slate hit geometry, component Locate and left-click ACK; isolated Mock Backend; no OS mouse or physical flight verification"));
        Report->SetNumberField(TEXT("requested_window_width"),1280);Report->SetNumberField(TEXT("requested_window_height"),800);
        Test->AddInfo(TEXT("Startup evidence: ")+Out);
    }
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();UWorld* World=nullptr;
        for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();
        auto* PC=World?Cast<ADroneOpsPlayerController>(World->GetFirstPlayerController()):nullptr;
        auto* Manager=PC?PC->GetCommandScreenManager():nullptr;
        auto* Shell=Manager?Manager->GetMapShell():nullptr;auto* Map=Manager?Manager->GetMapService():nullptr;
        auto* Registry=Manager?Manager->GetRegistry():nullptr;
        auto* Sync=PC?PC->GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>():nullptr;
        if(!World || !PC || !Manager || !Shell || !Shell->GetFleetInteraction() || !Map || !Registry || !Sync)
        {
            if(Now-Started<45.)return false;
            Test->AddError(TEXT("Production Map startup infrastructure timeout"));
            Report->SetStringField(TEXT("failure"),TEXT("infrastructure_timeout"));Save();Capture(TEXT("00-timeout"));return true;
        }
        if(Phase==0)
        {
            auto* Coordinates=Registry->GetCoordinateService().GetObject();
            bool Ready=Sync->IsHydrated() && Map->GetMapCamera() && Coordinates &&
                ICoordinateService::Execute_IsCoordinateSystemReady(Coordinates) && Shell->GetMapViewportGeometry().GetLocalSize().X>100;
            for(int32 Id=1;Id<=4;++Id)
            {
                FDroneDescriptor D;FVector G=FVector::ZeroVector;
                Ready=Ready && Registry->GetDroneDescriptor(Id,D) && MapMove::Position(Registry,Sync->GetPlans(),Id,G);
            }
            if(Ready && ReadyAt==0)ReadyAt=Now;
            if(!Ready)ReadyAt=0;
            if((!Ready || Now-ReadyAt<3.) && Now-Started<60.)return false;
            Test->TestTrue(TEXT("Production startup backend, coordinates, descriptors and positions became ready"),Ready);
            Test->TestTrue(TEXT("Actual startup world is CesiumWorld"),World->GetMapName().EndsWith(TEXT("CesiumWorld")));
            Test->TestEqual(TEXT("Actual startup client role is Map"),Manager->GetClientRole(),EDroneClientRole::Map);
            InitialPrimary=Registry->GetPrimarySelectedDrone();InitialSelection=Registry->GetMultiSelectedDrones();InitialVideo=Video(Sync);
            LocateId=InitialPrimary>=1 && InitialPrimary<=4?InitialPrimary:1;
            Capture(TEXT("01-startup"));
            Observe(World,PC,Shell,Map,Registry,Sync,TEXT("initial"),true);
            CheckFormation(Registry,Sync);
            CheckSymbolSeparation(Shell);
            // Continue after visibility failures: this comparison is the diagnostic,
            // and the original failed assertions remain in the automation report.
            Report->SetNumberField(TEXT("explicit_locate_id"),LocateId);
            Test->TestTrue(TEXT("Explicit production Locate request accepted"),Map->FocusDrone(LocateId));
            Phase=1;LocateAt=Now;return false;
        }
        if(Phase==1)
        {
            if(Now-LocateAt<3.)return false;
            Capture(TEXT("02-explicit-locate"));
            Observe(World,PC,Shell,Map,Registry,Sync,TEXT("after_explicit_locate"),true);
            Test->TestEqual(TEXT("Initial observation and Locate preserve primary"),Registry->GetPrimarySelectedDrone(),InitialPrimary);
            Test->TestTrue(TEXT("Initial observation and Locate preserve group"),Registry->GetMultiSelectedDrones()==InitialSelection);
            Test->TestEqual(TEXT("Initial observation and Locate preserve video"),Video(Sync),InitialVideo);
            // Only after the untouched startup evidence do component selection
            // actions begin. They use the isolated Backend and are restored below.
            Test->TestTrue(TEXT("Empty selection request submitted"),Sync->SetLocalSelection(0,{}));
            Phase=2;ActionAt=Now;return false;
        }
        auto* Fleet=Shell->GetFleetInteraction();
        if(Phase==2)
        {
            const bool Empty=Registry->GetPrimarySelectedDrone()==0 && Registry->GetMultiSelectedDrones().IsEmpty();
            if(!Empty && Now-ActionAt<12.)return false;
            Test->TestTrue(TEXT("Empty selection acknowledged"),Empty);
            Test->TestTrue(TEXT("Empty-selection FocusDrone(0) fits accepted fleet"),Map->FocusDrone(0));
            Phase=3;ActionAt=Now;return false;
        }
        if(Phase==3)
        {
            if(Now-ActionAt<3.)return false;
            Capture(TEXT("03-empty-selection-fit-all"));
            Observe(World,PC,Shell,Map,Registry,Sync,TEXT("empty_selection_fit_all"),true);
            CheckSymbolSeparation(Shell);
            Phase=4;ActionAt=Now;return false;
        }
        if(Phase==4)
        {
            Test->TestEqual(TEXT("Independent aircraft clicks never switch video"),Video(Sync),InitialVideo);
            if(WaitingForClick)
            {
                const bool Ack=Registry->GetPrimarySelectedDrone()==ClickId && Registry->GetMultiSelectedDrones()==TArray<int32>{ClickId};
                if(!Ack && Now-ActionAt<12.)return false;
                Test->TestTrue(*FString::Printf(TEXT("Independent UAV %d click acknowledged as exactly that aircraft"),ClickId),Ack);
                WaitingForClick=false;++ClickId;Fleet->Refresh();
            }
            if(ClickId<=4)
            {
                FVector2D P=FVector2D::ZeroVector;
                const bool Hit=Fleet->Project(ClickId,P) && MapHit(Shell,Fleet,P);
                Test->TestTrue(*FString::Printf(TEXT("Independent UAV %d actual geometry allows left click"),ClickId),Hit);
                if(!Hit){++ClickId;return false;}
                const FVector2D Screen=Fleet->GetCachedGeometry().LocalToAbsolute(P);
                const bool Began=Fleet->BeginPointer(Screen,false,false);
                Test->TestTrue(TEXT("Aircraft left click begins through production fleet input"),Began);
                if(!Began){++ClickId;return false;}
                Fleet->EndPointer(Screen,true);WaitingForClick=true;ActionAt=Now;return false;
            }
            Test->TestTrue(TEXT("Original selection restore requested"),Sync->SetLocalSelection(InitialPrimary,InitialSelection));
            Phase=5;ActionAt=Now;return false;
        }
        const bool Restored=Registry->GetPrimarySelectedDrone()==InitialPrimary && Registry->GetMultiSelectedDrones()==InitialSelection;
        if(!Restored && Now-ActionAt<12.)return false;
        Test->TestEqual(TEXT("Post-click primary selection restored"),Registry->GetPrimarySelectedDrone(),InitialPrimary);
        Test->TestTrue(TEXT("Post-click group selection restored"),Registry->GetMultiSelectedDrones()==InitialSelection);
        Test->TestEqual(TEXT("All startup, Locate and selection actions preserve explicit video target"),Video(Sync),InitialVideo);
        Report->SetBoolField(TEXT("selection_preserved"),Registry->GetPrimarySelectedDrone()==InitialPrimary && Registry->GetMultiSelectedDrones()==InitialSelection);
        Report->SetBoolField(TEXT("video_preserved"),Video(Sync)==InitialVideo);
        Save();return true;
    }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapStartupVisibilityTest,"DroneOps.Map.StartupVisibility",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMapStartupVisibilityTest::RunTest(const FString&)
{
    if(FParse::Param(FCommandLine::Get(),TEXT("NullRHI")) || FParse::Param(FCommandLine::Get(),TEXT("P55OfflineVisualQA")))
    {AddError(TEXT("StartupVisibility requires RHI production CesiumWorld without P55OfflineVisualQA"));return false;}
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Level/CesiumWorld")));
    ADD_LATENT_AUTOMATION_COMMAND(FMapStartupStartPIE());
    ADD_LATENT_AUTOMATION_COMMAND(FMapStartupVisibility(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
