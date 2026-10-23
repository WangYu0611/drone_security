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
#include "Blueprint/WidgetTree.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "CesiumGeoreference.h"
#include "CesiumCreditSystem.h"
#include "Components/RichTextBlock.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Widgets/SOverlay.h"
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
#include "UObject/UnrealType.h"

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
    double OverviewObservedAt=0,MinimumOverviewDistance=MAX_dbl;
    double InitialDetailReadyAt=0,RestoredDetailReadyAt=0;
    int32 InitialDetailFrames=0,RestoredDetailFrames=0;
    int32 Phase=0,LocateId=0,InitialPrimary=0,ClickId=1;
    double ActionAt=0;
    bool WaitingForClick=false;
    TArray<int32> InitialSelection;
    TWeakObjectPtr<UUserWidget> InitialCreditsWidget;
    FString Out,InitialVideo,AfterClickVideo;
    TSharedRef<FJsonObject> Report=MakeShared<FJsonObject>();

    struct FTilesState
    {
        int32 GoogleActors=0,GoogleMeshes=0,GoogleVisibleMeshes=0,GoogleRenderedMeshes=0;
        int32 DuplicateVisibleMeshes=0,DuplicateRenderedMeshes=0;
        double GoogleLoadProgress=100.;
        bool GoogleActive=true,GoogleCredits=true,GoogleDetail=true,GoogleDisabled=true,DuplicatesDisabled=true;
        TArray<TSharedPtr<FJsonValue>> Layers;
        bool HasRenderedGoogle() const { return GoogleActors>0 && GoogleActive && GoogleRenderedMeshes>0; }
        bool HasLoadedGoogleDetail() const { return HasRenderedGoogle() && GoogleLoadProgress>=99.; }
    };
    static FTilesState TilesState(UWorld* World)
    {
        FTilesState Result;
        for(TActorIterator<ACesium3DTileset> It(World);It;++It)
        {
            auto* Tileset=*It;const int64 AssetId=Tileset->GetIonAssetID();
            if(AssetId!=2275207 && AssetId!=96188 && AssetId!=1)continue;
            auto Layer=MakeShared<FJsonObject>();Result.Layers.Add(MakeShared<FJsonValueObject>(Layer));
            Layer->SetNumberField(TEXT("ion_asset_id"),static_cast<double>(AssetId));
            Layer->SetNumberField(TEXT("source"),static_cast<int32>(Tileset->GetTilesetSource()));
            Layer->SetBoolField(TEXT("hidden"),Tileset->IsHidden());
            Layer->SetBoolField(TEXT("suspended"),Tileset->SuspendUpdate);
            Layer->SetBoolField(TEXT("collision"),Tileset->GetActorEnableCollision());
            Layer->SetBoolField(TEXT("show_credits"),Tileset->ShowCreditsOnScreen);
            Layer->SetNumberField(TEXT("maximum_screen_space_error"),Tileset->GetMaximumScreenSpaceError());
            Layer->SetNumberField(TEXT("load_progress"),Tileset->GetLoadProgress());
            int32 Loaded=0,Visible=0,Rendered=0;TArray<UStaticMeshComponent*> Meshes;
            // Public Engine components include Cesium's generated glTF primitives;
            // an actor or a successful root.json request alone is not rendered data.
            Tileset->GetComponents(Meshes);
            for(const auto* Mesh:Meshes)
            {
                if(!IsValid(Mesh) || !Mesh->IsRegistered() || !Mesh->GetStaticMesh())continue;
                ++Loaded;
                if(Tileset->IsHidden() || !Mesh->IsVisible() || Mesh->bHiddenInGame)continue;
                ++Visible;if(Mesh->WasRecentlyRendered(1.f))++Rendered;
            }
            Layer->SetNumberField(TEXT("loaded_static_meshes"),Loaded);
            Layer->SetNumberField(TEXT("visible_static_meshes"),Visible);
            Layer->SetNumberField(TEXT("recently_rendered_static_meshes"),Rendered);
            const bool Disabled=Tileset->IsHidden() && Tileset->SuspendUpdate && !Tileset->GetActorEnableCollision()
                && Tileset->GetTilesetSource()==ETilesetSource::FromEllipsoid;
            if(AssetId==2275207)
            {
                ++Result.GoogleActors;Result.GoogleMeshes+=Loaded;
                Result.GoogleLoadProgress=FMath::Min(Result.GoogleLoadProgress,static_cast<double>(Tileset->GetLoadProgress()));
                Result.GoogleVisibleMeshes+=Visible;Result.GoogleRenderedMeshes+=Rendered;
                Result.GoogleActive=Result.GoogleActive && !Tileset->IsHidden() && !Tileset->SuspendUpdate
                    && Tileset->GetTilesetSource()==ETilesetSource::FromCesiumIon;
                Result.GoogleCredits=Result.GoogleCredits && Tileset->ShowCreditsOnScreen;
                Result.GoogleDetail=Result.GoogleDetail && FMath::IsNearlyEqual(Tileset->GetMaximumScreenSpaceError(),4.,.001);
                Result.GoogleDisabled=Result.GoogleDisabled && Disabled;
            }
            else
            {
                Result.DuplicatesDisabled=Result.DuplicatesDisabled && Disabled;
                Result.DuplicateVisibleMeshes+=Visible;Result.DuplicateRenderedMeshes+=Rendered;
            }
        }
        return Result;
    }
    bool DetailSettled(const FTilesState& State,double Now,double& ReadySince,int32& ReadyFrames) const
    {
        if(!State.HasLoadedGoogleDetail()){ReadySince=0;ReadyFrames=0;return false;}
        if(ReadySince==0)ReadySince=Now;
        ++ReadyFrames;
        return ReadyFrames>=3 && Now-ReadySince>=1.;
    }
    void RecordTiles(const FTilesState& State,const TCHAR* Name,double WaitSeconds,double DetailSeconds=0.,int32 DetailFrames=0)
    {
        auto Data=MakeShared<FJsonObject>();Report->SetObjectField(Name,Data);
        Data->SetNumberField(TEXT("wait_seconds"),WaitSeconds);Data->SetArrayField(TEXT("layers"),State.Layers);
        Data->SetBoolField(TEXT("google_render_ready"),State.HasRenderedGoogle());
        const bool DetailReady=State.HasLoadedGoogleDetail() && DetailSeconds>=1. && DetailFrames>=3;
        Data->SetNumberField(TEXT("google_minimum_load_progress"),State.GoogleActors>0?State.GoogleLoadProgress:0.);
        Data->SetNumberField(TEXT("google_detail_stable_seconds"),DetailSeconds);
        Data->SetNumberField(TEXT("google_detail_stable_frames"),DetailFrames);
        Data->SetBoolField(TEXT("google_detail_ready"),DetailReady);
        Data->SetBoolField(TEXT("google_load_timeout"),WaitSeconds>=120. && !DetailReady);
        Data->SetNumberField(TEXT("google_actors"),State.GoogleActors);
        Data->SetNumberField(TEXT("google_loaded_meshes"),State.GoogleMeshes);
        Data->SetNumberField(TEXT("google_visible_meshes"),State.GoogleVisibleMeshes);
        Data->SetNumberField(TEXT("google_recently_rendered_meshes"),State.GoogleRenderedMeshes);
        Data->SetNumberField(TEXT("duplicate_visible_meshes"),State.DuplicateVisibleMeshes);
        Data->SetNumberField(TEXT("duplicate_recently_rendered_meshes"),State.DuplicateRenderedMeshes);
        Test->AddInfo(FString::Printf(TEXT("%s: Google actors=%d loaded=%d visible=%d rendered=%d progress=%.2f%% detail-stable=%.1fs/%d frames duplicate-visible=%d wait=%.1fs"),
            Name,State.GoogleActors,State.GoogleMeshes,State.GoogleVisibleMeshes,State.GoogleRenderedMeshes,State.GoogleLoadProgress,
            DetailSeconds,DetailFrames,State.DuplicateVisibleMeshes,WaitSeconds));
    }
    void CheckPhotorealistic3D(const FTilesState& State,const TCHAR* Name)
    {
        Test->TestEqual(*FString::Printf(TEXT("%s: exactly one Google 2275207 tileset"),Name),State.GoogleActors,1);
        Test->TestTrue(*FString::Printf(TEXT("%s: Google ion source is active"),Name),State.GoogleActive && State.GoogleActors>0);
        Test->TestTrue(*FString::Printf(TEXT("%s: Google generated static meshes are visible and recently rendered"),Name),State.HasRenderedGoogle());
        Test->TestTrue(*FString::Printf(TEXT("%s: Google detail for the current view is at least 99 percent loaded"),Name),State.HasLoadedGoogleDetail());
        Test->TestTrue(*FString::Printf(TEXT("%s: Google attribution enabled"),Name),State.GoogleCredits && State.GoogleActors>0);
        Test->TestTrue(*FString::Printf(TEXT("%s: Google screen-space error is 4"),Name),State.GoogleDetail && State.GoogleActors>0);
        Test->TestTrue(*FString::Printf(TEXT("%s: OSM 96188 and Terrain 1 are hidden, suspended, collision disabled and source detached"),Name),State.DuplicatesDisabled);
        Test->TestEqual(*FString::Printf(TEXT("%s: no visible OSM or Terrain mesh overlaps Google"),Name),State.DuplicateVisibleMeshes,0);
        Test->TestEqual(*FString::Printf(TEXT("%s: no recently rendered OSM or Terrain mesh overlaps Google"),Name),State.DuplicateRenderedMeshes,0);
    }
    struct FViewportLayer
    {
        TSharedPtr<SWidget> Parent;
        int32 ZOrder=INDEX_NONE;
    };
    FViewportLayer ViewportLayer(UUserWidget* Widget) const
    {
        // Inspect the actual viewport overlay, not UGameViewportSubsystem's
        // stored slot: SetWidgetSlot can update that value without reordering
        // the outer host. This would detect the previous hidden-credit bug.
        auto Child=Widget?Widget->GetCachedWidget():nullptr;
        while(Child.IsValid())
        {
            auto Parent=Child->GetParentWidget();
            if(!Parent.IsValid())break;
            if(Parent->GetType()==FName(TEXT("SOverlay")))
            {
                auto* Children=Parent->GetChildren();
                for(int32 Index=0;Index<Children->Num();++Index)
                    if(Children->GetChildAt(Index)==Child.ToSharedRef())
                        return {Parent,static_cast<const SOverlay::FOverlaySlot&>(Children->GetSlotAt(Index)).GetZOrder()};
            }
            Child=Parent;
        }
        return {};
    }
    void CheckAttribution(UWorld* World,UMapShellWidget* Shell,const TCHAR* Name)
    {
        auto Data=MakeShared<FJsonObject>();Report->SetObjectField(Name,Data);
        const auto ShellLayer=ViewportLayer(Shell);
        const auto* Property=FindFProperty<FObjectPropertyBase>(ACesiumCreditSystem::StaticClass(),TEXT("CreditsWidget"));
        UUserWidget* Widget=nullptr;
        for(TActorIterator<ACesiumCreditSystem> It(World);It && !Widget;++It)
            if(Property)Widget=Cast<UUserWidget>(Property->GetObjectPropertyValue_InContainer(*It));
        const auto CreditLayer=ViewportLayer(Widget);
        const bool InViewport=Widget && Widget->IsInViewport() && Widget->GetWorld()==World;
        const bool AboveShell=CreditLayer.Parent.IsValid() && CreditLayer.Parent==ShellLayer.Parent &&
            CreditLayer.ZOrder>ShellLayer.ZOrder;
        auto* Text=Widget && Widget->WidgetTree?Cast<URichTextBlock>(Widget->WidgetTree->FindWidget(TEXT("RichTextOnScreen"))):nullptr;
        const int32 TextLength=Text?Text->GetText().ToString().Len():0;
        Data->SetBoolField(TEXT("official_widget_in_game_viewport"),InViewport);
        Data->SetNumberField(TEXT("shell_outer_overlay_z"),ShellLayer.ZOrder);
        Data->SetNumberField(TEXT("credits_outer_overlay_z"),CreditLayer.ZOrder);
        Data->SetBoolField(TEXT("credits_above_shell_in_same_overlay"),AboveShell);
        Data->SetNumberField(TEXT("official_on_screen_credit_text_length"),TextLength);
        Test->TestTrue(*FString::Printf(TEXT("%s: official Cesium widget is in the current game viewport"),Name),InViewport);
        Test->TestTrue(*FString::Printf(TEXT("%s: actual outer viewport layer places Cesium credits above MapShell"),Name),AboveShell);
        Test->TestTrue(*FString::Printf(TEXT("%s: official on-screen attribution contains live provider content"),Name),TextLength>0);
        if(InitialCreditsWidget.IsValid())
            Test->TestTrue(TEXT("Mode cycling retains the same official attribution widget and its dynamic state"),InitialCreditsWidget.Get()==Widget);
        else InitialCreditsWidget=Widget;
    }
    void CheckCityOverview(UWorld* World,UCommandMapInteractionService* Map,UDroneRegistrySubsystem* Registry)
    {
        Test->TestTrue(TEXT("Configured city startup view is enabled"),Map->HasConfiguredStartupView());
        Test->TestEqual(TEXT("Production startup defaults to 3D"),Map->GetMapMode(),ECommandMapMode::Map3D);
        TArray<TSharedPtr<FJsonValue>> Origins;int32 Georeferences=0;bool CorrectOrigin=true;
        for(TActorIterator<ACesiumGeoreference> It(World);It;++It)
        {
            ++Georeferences;auto Origin=MakeShared<FJsonObject>();Origins.Add(MakeShared<FJsonValueObject>(Origin));
            const FVector Geographic=It->GetOriginLongitudeLatitudeHeight();Vector(Origin,TEXT("longitude_latitude_ellipsoid_m"),Geographic);
            CorrectOrigin=CorrectOrigin && FMath::IsNearlyEqual(Geographic.X,103.8585,1.e-6)
                && FMath::IsNearlyEqual(Geographic.Y,1.2834,1.e-6) && FMath::IsNearlyEqual(Geographic.Z,50.,.01);
        }
        Report->SetArrayField(TEXT("cesium_georeferences"),Origins);
        Test->TestTrue(TEXT("Actual Cesium georeference is Marina Bay 103.8585 / 1.2834 / ellipsoid 50 m"),Georeferences>0 && CorrectOrigin);
        Test->TestTrue(TEXT("Configured city overview target distance is 2400 m"),FMath::IsNearlyEqual(Map->ViewDistance,240000.,1.));
        Test->TestTrue(TEXT("Settled city overview actual camera distance is at least 2000 m"),Map->ActualDistance>=200000.);
        Test->TestTrue(TEXT("Hydrated fleet refresh did not replace the city overview with a close fleet fit"),
            OverviewObservedAt>0 && MinimumOverviewDistance>=200000.);
        Report->SetNumberField(TEXT("minimum_overview_distance_after_hydration_cm"),OverviewObservedAt>0?MinimumOverviewDistance:0.);
        if(const auto* Camera=Map->GetMapCamera())
        {
            Test->TestTrue(TEXT("City overview pitch is -50 degrees"),FMath::Abs(FMath::FindDeltaAngleDegrees(Camera->GetActorRotation().Pitch,-50.))<.25);
            Test->TestTrue(TEXT("City overview yaw is -35 degrees"),FMath::Abs(FMath::FindDeltaAngleDegrees(Camera->GetActorRotation().Yaw,-35.))<.25);
        }
        auto* Coordinates=Registry->GetCoordinateService().GetObject();
        if(Coordinates && ICoordinateService::Execute_IsCoordinateSystemReady(Coordinates))
            Test->TestTrue(TEXT("City overview focuses the configured Singapore geographic anchor"),
                Map->ViewFocus.Equals(ICoordinateService::Execute_GeographicToWorld(Coordinates,1.2834,103.8585,50.),100.));
    }

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
            if(Id==1)
                Test->TestTrue(TEXT("Production Mock formation starts at Marina Bay, ellipsoid 50 m"),
                    Valid && FMath::IsNearlyEqual(G.X,103.8585,1.e-6) && FMath::IsNearlyEqual(G.Y,1.2834,1.e-6)
                    && FMath::IsNearlyEqual(G.Z,50.,.01));
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
    bool RenderedModel(UMapFleetInteractionWidget* Fleet,int32 Id) const
    {
        AActor* Actor=Fleet->GetDisplayedModelActor(Id);
        if(!IsValid(Actor) || Actor->IsHidden())return false;
        TArray<UMeshComponent*> Meshes;Actor->GetComponents(Meshes);
        for(const auto* Mesh:Meshes)
            if(IsValid(Mesh) && Mesh->IsVisible() && !Mesh->bHiddenInGame && Mesh->WasRecentlyRendered(.5f))return true;
        return false;
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
        Data->SetStringField(TEXT("map_mode"),Map->GetMapMode()==ECommandMapMode::Map3D?TEXT("3D"):TEXT("2D"));
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
            D->SetBoolField(TEXT("model_presentation"),Fleet->IsModelPresentation(Id));
            D->SetBoolField(TEXT("model_recently_rendered"),RenderedModel(Fleet,Id));
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
            {
                const bool Model=Fleet->IsModelPresentation(Id);
                Test->TestTrue(*FString::Printf(TEXT("%s: UAV %d has an actually drawn icon or recently rendered visible model"),Name,Id),
                    Model?RenderedModel(Fleet,Id):Fleet->PaintedDroneIds.Contains(Id));
                Test->TestFalse(*FString::Printf(TEXT("%s: UAV %d model is not covered by the opaque icon"),Name,Id),
                    Model && Fleet->PaintedDroneIds.Contains(Id));
            }
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
                Test->TestEqual(TEXT("Explicit Locate retains all four nearby aircraft for independent selection"),Visible,4);
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
        Report->SetStringField(TEXT("startup_acceptance"),TEXT("Untouched Singapore city overview with rendered Google meshes; overlapping 2 m fleet symbols are allowed only in this initial overview. Explicit Locate and empty-selection Fit retain separate 36 logical pixel hit footprints."));
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
            // Wait through several hydrated refreshes before judging automatic
            // framing. A 2 m formation intentionally overlaps at the city scale.
            if(Ready && Now-ReadyAt>=3.)
            {
                if(OverviewObservedAt==0)OverviewObservedAt=Now;
                MinimumOverviewDistance=FMath::Min(MinimumOverviewDistance,FMath::Min(Map->ViewDistance,Map->ActualDistance));
            }
            const auto Tiles=TilesState(World);
            const bool DetailReady=DetailSettled(Tiles,Now,InitialDetailReadyAt,InitialDetailFrames);
            const bool Settled=Ready && Now-ReadyAt>=6.;
            if((!Settled || !DetailReady) && Now-Started<120.)return false;
            Test->TestTrue(TEXT("Production startup backend, coordinates, descriptors and positions became ready"),Ready);
            Test->TestTrue(TEXT("Production city overview remains observable through hydrated refreshes"),Settled);
            Test->TestTrue(TEXT("Actual startup world is CesiumWorld"),World->GetMapName().EndsWith(TEXT("CesiumWorld")));
            Test->TestEqual(TEXT("Actual startup client role is Map"),Manager->GetClientRole(),EDroneClientRole::Map);
            InitialPrimary=Registry->GetPrimarySelectedDrone();InitialSelection=Registry->GetMultiSelectedDrones();InitialVideo=Video(Sync);
            LocateId=InitialPrimary>=1 && InitialPrimary<=4?InitialPrimary:1;
            RecordTiles(Tiles,TEXT("initial_tiles"),Now-Started,InitialDetailReadyAt>0?Now-InitialDetailReadyAt:0.,InitialDetailFrames);
            Test->TestTrue(TEXT("Initial Google detail remains rendered and at least 99 percent loaded for one second and three frames"),DetailReady);
            CheckPhotorealistic3D(Tiles,TEXT("Initial city overview"));
            CheckCityOverview(World,Map,Registry);
            CheckAttribution(World,Shell,TEXT("initial_attribution"));
            Capture(TEXT("01-startup"));
            Observe(World,PC,Shell,Map,Registry,Sync,TEXT("initial"),true);
            CheckFormation(Registry,Sync);
            // Continue after failures and retain them in the automation report.
            // Mode cycling also verifies real mesh restoration, not only actor flags.
            Map->SetMapMode(ECommandMapMode::Map2D);
            Phase=6;ActionAt=Now;return false;
        }
        if(Phase==6)
        {
            if(Now-ActionAt<3.)return false;
            const auto Tiles=TilesState(World);RecordTiles(Tiles,TEXT("mode_2d_tiles"),Now-ActionAt);
            Test->TestEqual(TEXT("Explicit mode switch enters 2D"),Map->GetMapMode(),ECommandMapMode::Map2D);
            Test->TestTrue(TEXT("2D detaches, hides and suspends the Google layer"),Tiles.GoogleActors>0 && Tiles.GoogleDisabled);
            Test->TestEqual(TEXT("2D displays no Google mesh"),Tiles.GoogleVisibleMeshes,0);
            Test->TestEqual(TEXT("2D has no recently rendered Google mesh"),Tiles.GoogleRenderedMeshes,0);
            Test->TestEqual(TEXT("2D preserves primary selection"),Registry->GetPrimarySelectedDrone(),InitialPrimary);
            Test->TestTrue(TEXT("2D preserves group selection"),Registry->GetMultiSelectedDrones()==InitialSelection);
            Test->TestEqual(TEXT("2D preserves explicit video target"),Video(Sync),InitialVideo);
            Capture(TEXT("01a-mode-2d"));Save();
            Map->SetMapMode(ECommandMapMode::Map3D);
            Phase=7;ActionAt=Now;return false;
        }
        if(Phase==7)
        {
            const auto Tiles=TilesState(World);
            const bool DetailReady=DetailSettled(Tiles,Now,RestoredDetailReadyAt,RestoredDetailFrames);
            if((Now-ActionAt<3. || !DetailReady) && Now-ActionAt<120.)return false;
            Test->TestEqual(TEXT("Mode restore enters 3D"),Map->GetMapMode(),ECommandMapMode::Map3D);
            RecordTiles(Tiles,TEXT("restored_3d_tiles"),Now-ActionAt,RestoredDetailReadyAt>0?Now-RestoredDetailReadyAt:0.,RestoredDetailFrames);
            Test->TestTrue(TEXT("Restored Google detail remains rendered and at least 99 percent loaded for one second and three frames"),DetailReady);
            CheckPhotorealistic3D(Tiles,TEXT("Restored 3D"));
            CheckCityOverview(World,Map,Registry);
            CheckAttribution(World,Shell,TEXT("restored_3d_attribution"));
            Test->TestEqual(TEXT("Mode cycling preserves primary selection"),Registry->GetPrimarySelectedDrone(),InitialPrimary);
            Test->TestTrue(TEXT("Mode cycling preserves group selection"),Registry->GetMultiSelectedDrones()==InitialSelection);
            Test->TestEqual(TEXT("Mode cycling preserves explicit video target"),Video(Sync),InitialVideo);
            Capture(TEXT("01b-restored-3d"));Save();
            Report->SetNumberField(TEXT("explicit_locate_id"),LocateId);
            // The real Locate action requests a close frame; automatic selection
            // uses the default false and must not zoom away from the city view.
            Test->TestTrue(TEXT("Explicit production Locate request accepted"),Map->FocusDrone(LocateId,true));
            Phase=1;LocateAt=Now;return false;
        }
        if(Phase==1)
        {
            if(Now-LocateAt<3.)return false;
            Capture(TEXT("02-explicit-locate"));
            Observe(World,PC,Shell,Map,Registry,Sync,TEXT("after_explicit_locate"),true);
            CheckSymbolSeparation(Shell);
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

            if(WaitingForClick)
            {
                const bool Ack=!Sync->IsMapSelectionPending() && Registry->GetPrimarySelectedDrone()==ClickId && Registry->GetMultiSelectedDrones()==TArray<int32>{ClickId}
                    && PlanUI::Field(Sync->GetVideoView(),TEXT("video_target_uav_id"))==FString::Printf(TEXT("UAV-%02d"),ClickId);
                if(!Ack && Now-ActionAt<12.)return false;
                Test->TestTrue(*FString::Printf(TEXT("Independent UAV %d click acknowledged as exactly that aircraft"),ClickId),Ack);
                Test->TestEqual(TEXT("Explicit geometry click switches Video to the actual hit"),PlanUI::Field(Sync->GetVideoView(),TEXT("video_target_uav_id")),FString::Printf(TEXT("UAV-%02d"),ClickId));
                AfterClickVideo=Video(Sync);WaitingForClick=false;++ClickId;Fleet->Refresh();
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
                const auto BeforeClickVideo=Video(Sync);
                Fleet->EndPointer(Screen,true);
                Test->TestEqual(TEXT("Geometry click cannot optimistically replace the Video snapshot"),Video(Sync),BeforeClickVideo);
                WaitingForClick=true;ActionAt=Now;return false;
            }
            Test->TestTrue(TEXT("Original selection restore requested"),Sync->SetLocalSelection(InitialPrimary,InitialSelection));
            Phase=5;ActionAt=Now;return false;
        }
        if(Phase==8)
        {
            const bool Model=Fleet->IsModelPresentation(LocateId) && RenderedModel(Fleet,LocateId);
            if((Now-ActionAt<3. || !Model) && Now-ActionAt<15.)return false;
            Test->TestTrue(TEXT("Maximum user zoom shows an actual recently rendered aircraft model"),Model);
            Test->TestFalse(TEXT("Near aircraft model has no opaque map icon on top"),Fleet->PaintedDroneIds.Contains(LocateId));
            Report->SetBoolField(TEXT("near_model_rendered"),Model);
            Report->SetNumberField(TEXT("near_model_camera_distance_cm"),Map->ActualDistance);
            Capture(TEXT("04-near-aircraft-model"));Save();
            Test->TestTrue(TEXT("Return to the configured city overview is accepted"),Map->ApplyConfiguredStartupView());
            Phase=9;ActionAt=Now;return false;
        }
        if(Phase==9)
        {
            if(Now-ActionAt<3.)return false;
            for(int32 Id=1;Id<=4;++Id)
            {
                Test->TestFalse(TEXT("Zooming back out removes model presentation"),Fleet->IsModelPresentation(Id));
                Test->TestTrue(TEXT("Zooming back out restores an actually painted aircraft icon"),Fleet->PaintedDroneIds.Contains(Id));
            }
            Test->TestEqual(TEXT("Zoom transitions preserve the last clicked video target"),Video(Sync),AfterClickVideo);
            Test->TestEqual(TEXT("Zoom transitions preserve primary selection"),Registry->GetPrimarySelectedDrone(),InitialPrimary);
            Test->TestTrue(TEXT("Zoom transitions preserve group selection"),Registry->GetMultiSelectedDrones()==InitialSelection);
            Report->SetBoolField(TEXT("far_icons_restored"),Fleet->PaintedDroneIds.Num()==4);
            Capture(TEXT("05-far-icons-restored"));Save();return true;
        }
        const bool Restored=Registry->GetPrimarySelectedDrone()==InitialPrimary && Registry->GetMultiSelectedDrones()==InitialSelection;
        if(!Restored && Now-ActionAt<12.)return false;
        Test->TestEqual(TEXT("Post-click primary selection restored"),Registry->GetPrimarySelectedDrone(),InitialPrimary);
        Test->TestTrue(TEXT("Post-click group selection restored"),Registry->GetMultiSelectedDrones()==InitialSelection);
        Test->TestEqual(TEXT("Generic selection restore preserves the last clicked video target"),Video(Sync),AfterClickVideo);
        Report->SetBoolField(TEXT("selection_preserved"),Registry->GetPrimarySelectedDrone()==InitialPrimary && Registry->GetMultiSelectedDrones()==InitialSelection);
        Report->SetBoolField(TEXT("map_click_video_followed"),!AfterClickVideo.IsEmpty() && Video(Sync)==AfterClickVideo);
        Test->TestTrue(TEXT("Near model check focuses the accepted aircraft position"),Map->FocusDrone(LocateId,true));
        Map->ZoomView(100);Phase=8;ActionAt=Now;Save();return false;
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
