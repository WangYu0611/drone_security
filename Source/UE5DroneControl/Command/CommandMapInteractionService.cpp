#include "Command/CommandMapInteractionService.h"
#include "Shared/DroneMissionService.h"
#include "Command/CommandAlertStore.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "DroneOps/Network/DroneNetworkManager.h"
#include "DroneOps/Network/DroneWebSocketClient.h"
#include "UI/SequenceDispatchPanelWidget.h"
#include "Engine/GameInstance.h"
#include "Command/CommandScreenManager.h"
#include "DroneOps/Core/ICoordinateService.h"
#include "CesiumRasterOverlay.h"
#include "CesiumIonRasterOverlay.h"
#include "CesiumUrlTemplateRasterOverlay.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "Misc/ConfigCacheIni.h"
#include "InputCoreTypes.h"
#include "Engine/World.h"
#include "PathEditor/DronePathActor.h"

UCommandMapInteractionService* UCommandMapInteractionService::GetOrCreateForWorld(UWorld* World)
{
    if (!World) return nullptr;
    for (UObject* Object : World->ExtraReferencedObjects)
        if (auto* Service = Cast<UCommandMapInteractionService>(Object)) return Service;
    auto* Service = NewObject<UCommandMapInteractionService>(World);
    World->ExtraReferencedObjects.Add(Service);
    Service->InitializeWorld(World);
    return Service;
}

void UCommandMapInteractionService::Initialize(ADroneOpsPlayerController* InController)
{
    Controller = InController;
    Registry = InController && InController->GetGameInstance()
        ? InController->GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>() : nullptr;
    if (InController && UCommandScreenManager::ResolveClientRole() == EDroneClientRole::Map)
    {
        InitializeWorld(InController->GetWorld());
        OriginalHitResultTraceDistance = InController->HitResultTraceDistance;
        MapCamera = InController->GetWorld()->SpawnActor<ACameraActor>();
        if (MapCamera.IsValid())
        {
            MapCamera->GetCameraComponent()->bConstrainAspectRatio = false;
            MapCamera->GetCameraComponent()->SetFieldOfView(60.f);
            UpdateCameraTransform();
            InController->SetViewTarget(MapCamera.Get());
            InController->bShowMouseCursor = true;
            FInputModeGameAndUI Input;
            Input.SetHideCursorDuringCapture(false);
            Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            InController->SetInputMode(Input);
        }
    }
}

void UCommandMapInteractionService::Unbind()
{
    if (MapWorld.IsValid() && OwnsCamera())
        for (TActorIterator<ADronePathActor> It(MapWorld.Get()); It; ++It) It->SetMapDisplayRadius(0.f);
    if (Controller.IsValid() && OwnsCamera()) Controller->HitResultTraceDistance = OriginalHitResultTraceDistance;
    if (MapCamera.IsValid()) MapCamera->Destroy();
    MapCamera.Reset();
    PathPanel.Reset(); Registry.Reset(); Controller.Reset();
}

void UCommandMapInteractionService::SetPathPanel(USequenceDispatchPanelWidget* Panel) { PathPanel = Panel; }

bool UCommandMapInteractionService::SelectDrone(int32 DroneId)
{
    if (!Registry.IsValid() || (DroneId > 0 && !Registry->IsDroneRegistered(DroneId))) return false;
    if (DroneId <= 0) { Registry->ClearSelection(); return true; }
    Registry->SetPrimarySelectedDrone(DroneId);
    Registry->SetMultiSelectedDrones({DroneId});
    return true;
}

bool UCommandMapInteractionService::FocusDrone(int32 DroneId)
{
    return Controller.IsValid() && Controller->FocusCommandDrone(DroneId);
}

bool UCommandMapInteractionService::FocusLocation(const FVector& WorldLocation)
{
    if (WorldLocation.ContainsNaN()) return false;
    if (OwnsCamera())
    {
        ViewFocus = WorldLocation;
        UpdateCameraTransform();
        return true;
    }
    return Controller.IsValid() && Controller->FocusCommandLocation(WorldLocation);
}

bool UCommandMapInteractionService::FocusExecutionBounds(const FBox& Bounds)
{
    if(!Bounds.IsValid || !OwnsCamera())return false;
    // Frame the immutable mission snapshot without changing coordinates or selection.
    ViewDistance=FMath::Max(15000.,Bounds.GetExtent().Size()*5.);
    return FocusLocation(Bounds.GetCenter());
}

bool UCommandMapInteractionService::FocusAlertOnMap(int32 AlertId)
{
    if (!Controller.IsValid()) return false;
    const UCommandAlertStore* Store = Controller->GetGameInstance()->GetSubsystem<UCommandAlertStore>();
    if (!Store) return false;
    for (const FCommandAlert& Alert : Store->GetAlerts())
    {
        if (Alert.Id != AlertId) continue;
        // TODO: route an explicit protocol position to FocusLocation once alerts carry a frame/GPS.
        // Today's alert is DroneId/type/value only. Never substitute fabricated coordinates.
        return Alert.DroneId > 0 && SelectDrone(Alert.DroneId) && FocusDrone(Alert.DroneId);
    }
    return false;
}

bool UCommandMapInteractionService::BeginPathPlanning()
{
    return PathPanel.IsValid() && PathPanel->BeginCommandPlanning();
}
bool UCommandMapInteractionService::ConfirmPath()
{
    return PathPanel.IsValid() && PathPanel->ConfirmCommandPath();
}
bool UCommandMapInteractionService::CancelPathPlanning()
{
    return PathPanel.IsValid() && PathPanel->CancelCommandPlanning();
}
bool UCommandMapInteractionService::DeleteSelectedWaypoint()
{
    return Controller.IsValid() && Controller->DeleteCommandWaypoint();
}
bool UCommandMapInteractionService::IsPlanning() const
{
    return Controller.IsValid() && Controller->IsPathEditMode();
}
bool UCommandMapInteractionService::CanPauseSelected() const
{
    return Controller.IsValid() && Controller->GetGameInstance()->GetSubsystem<UDroneMissionService>()->CanPauseSelected();
}
bool UCommandMapInteractionService::SetSelectedPaused(bool bPaused)
{
    return Controller.IsValid() && Controller->GetGameInstance()->GetSubsystem<UDroneMissionService>()->SetSelectedPaused(bPaused);
}

void UCommandMapInteractionService::InitializeWorld(UWorld* World)
{
    if (bWorldInitialized || !World) return;
    bWorldInitialized = true;
    MapWorld = World;
    FString Mode;
    GConfig->GetString(TEXT("CommandMap"), TEXT("DefaultMapMode"), Mode, GGameIni);
    if (!bModeRequested) CurrentMapMode = Mode.Equals(TEXT("3D"), ESearchCase::IgnoreCase) ? ECommandMapMode::Map3D : ECommandMapMode::Map2D;
    FString BasemapUrl;
    GConfig->GetString(TEXT("CommandMap"), TEXT("BasemapUrl"), BasemapUrl, GGameIni);
    BasemapUrl.ReplaceInline(TEXT("$z"), TEXT("{z}"));
    BasemapUrl.ReplaceInline(TEXT("$x"), TEXT("{x}"));
    BasemapUrl.ReplaceInline(TEXT("$y"), TEXT("{y}"));
    const bool bValidUrl = BasemapUrl.IsEmpty() || ((BasemapUrl.StartsWith(TEXT("https://")) || BasemapUrl.StartsWith(TEXT("http://")))
        && BasemapUrl.Contains(TEXT("{z}")) && BasemapUrl.Contains(TEXT("{x}")) && BasemapUrl.Contains(TEXT("{y}")));
    if (!bValidUrl) UE_LOG(LogTemp, Error, TEXT("COMMAND FUNCTION ERROR: CommandMap.BasemapUrl must be an HTTP(S) WGS84/WebMercator XYZ template; keeping existing raster."));
    int32 IonAssetId = 0;
    GConfig->GetInt(TEXT("CommandMap"), TEXT("BasemapIonAssetId"), IonAssetId, GGameIni);
    for (TActorIterator<ACesium3DTileset> It(World); It; ++It)
    {
        ACesium3DTileset* Tileset = *It;
        TArray<UCesiumRasterOverlay*> Rasters;
        Tileset->GetComponents(Rasters);
        if (Tileset->ActorHasTag(TEXT("CommandSharedLayer"))) continue;
        const bool bBasemap = !Tileset->ActorHasTag(TEXT("Command3DOnly")) && (!Rasters.IsEmpty() || Tileset->ActorHasTag(TEXT("CommandBasemapSurface")));
        if (!bBasemap && Tileset->GetTilesetSource() == ETilesetSource::FromUrl && Tileset->GetUrl().IsEmpty()) continue;
        FLayer& Layer = Layers.AddDefaulted_GetRef();
        Layer.Actor = Tileset;
        Layer.OriginalSource = Tileset->GetTilesetSource();
        Layer.bBasemap = bBasemap;
        Layer.bHidden = Tileset->IsHidden();
        Layer.bSuspended = Tileset->SuspendUpdate;
        Layer.bCollision = Tileset->GetActorEnableCollision();
        if (!bBasemap) continue;
        for (UCesiumRasterOverlay* Raster : Rasters)
        {
            // PreInitializeComponents runs before auto activation; preserve the configured intent as well.
            Layer.RasterActivation.Add(Raster, Raster->IsActive() || Raster->bAutoActivate);
            if (UCesiumIonRasterOverlay* Ion = Cast<UCesiumIonRasterOverlay>(Raster); Ion && IonAssetId > 0)
            {
                if (Ion->IonAssetID != IonAssetId) { Ion->IonAssetID = IonAssetId; Ion->Refresh(); }
            }
        }
        if (bValidUrl && !BasemapUrl.IsEmpty())
        {
            for (UCesiumRasterOverlay* Raster : Rasters) { Raster->SetAutoActivate(false); Raster->Deactivate(); Layer.RasterActivation[Raster] = false; }
            UCesiumUrlTemplateRasterOverlay* Raster = NewObject<UCesiumUrlTemplateRasterOverlay>(Tileset, TEXT("CommandBasemapRaster"));
            Raster->SetAutoActivate(false);
            Raster->TemplateUrl = BasemapUrl;
            Raster->Projection = ECesiumUrlTemplateRasterOverlayProjection::WebMercator;
            Raster->MaterialLayerKey = TEXT("Overlay0");
            Tileset->AddInstanceComponent(Raster);
            Raster->RegisterComponent();
            Raster->Activate(true);
            Layer.RasterActivation.Add(Raster, true);
        }
    }
    OnCesiumRasterOverlayLoadFailure.AddUObject(this, &UCommandMapInteractionService::RasterFailed);
    OnCesium3DTilesetLoadFailure.AddUObject(this, &UCommandMapInteractionService::TilesetFailed);
    ApplyLayers();
}

void UCommandMapInteractionService::ApplyLayers()
{
    bool bLocal = false, bPlane = false;
    GConfig->GetBool(TEXT("CesiumTileServer"), TEXT("UseLocalTileServer"), bLocal, GEngineIni);
    GConfig->GetBool(TEXT("CesiumTileServer"), TEXT("UseOfflineRasterPlane"), bPlane, GEngineIni);
    const bool bOfflinePlane = bLocal && bPlane;
    for (const FLayer& Layer : Layers)
    {
        ACesium3DTileset* Actor = Layer.Actor.Get();
        if (!Actor) continue;
        const bool b2D = CurrentMapMode == ECommandMapMode::Map2D;
        // SuspendUpdate alone does not prevent Cesium BeginPlay/OnConstruction from loading a source.
        // A disabled heavy layer gets a non-network ellipsoid source before BeginPlay, then restores its exact source in 3D.
        Actor->SuspendUpdate = bOfflinePlane || (b2D ? !Layer.bBasemap : Layer.bSuspended);
        const ETilesetSource Source = (b2D || bOfflinePlane) ? ETilesetSource::FromEllipsoid : Layer.OriginalSource;
        if (Actor->GetTilesetSource() != Source) Actor->SetTilesetSource(Source);
        Actor->SetActorHiddenInGame(bOfflinePlane || (b2D ? !Layer.bBasemap : Layer.bHidden));
        Actor->SetActorEnableCollision(!bOfflinePlane && (b2D ? Layer.bBasemap : Layer.bCollision));
        for (const auto& Pair : Layer.RasterActivation)
            if (auto* Raster = Pair.Key.Get(); Raster && Raster->IsActive() != (!bOfflinePlane && Pair.Value))
                Raster->SetActive(!bOfflinePlane && Pair.Value);
    }
    LogLayerState();
}

void UCommandMapInteractionService::SetMapMode(ECommandMapMode Mode)
{
    if (Mode != ECommandMapMode::Map2D && Mode != ECommandMapMode::Map3D) return;
    bModeRequested = true;
    if (CurrentMapMode == Mode) { ApplyPendingMapMode(); return; }
    CurrentMapMode = Mode;
    ApplyPendingMapMode();
    OnMapModeChanged.Broadcast(CurrentMapMode);
}
void UCommandMapInteractionService::ApplyPendingMapMode()
{
    if (MapWorld.IsValid()) ApplyLayers();
    UpdateCameraTransform();
}
void UCommandMapInteractionService::UpdateCameraTransform()
{
    if (!MapCamera.IsValid()) return;
    const FRotator Rotation(CurrentMapMode == ECommandMapMode::Map2D ? -89.9f : View3DPitch, ViewYaw, 0.f);
    MapCamera->SetActorLocationAndRotation(ViewFocus - Rotation.Vector() * ViewDistance, Rotation);
    // The default 1 km cursor ray cannot reach the map at the supported GIS zoom distances.
    if (Controller.IsValid()) Controller->HitResultTraceDistance = FMath::Max(OriginalHitResultTraceDistance, static_cast<float>(ViewDistance * 4.0));
}
void UCommandMapInteractionService::PanView(const FVector2D& Delta)
{
    if (!OwnsCamera() || Delta.ContainsNaN()) return;
    const FRotationMatrix Rotation(FRotator(0.f, ViewYaw, 0.f));
    ViewFocus += (Rotation.GetUnitAxis(EAxis::Y) * Delta.X + Rotation.GetUnitAxis(EAxis::X) * Delta.Y) * ViewDistance * 0.002;
    UpdateCameraTransform();
}
void UCommandMapInteractionService::ZoomView(float Steps)
{
    if (!FMath::IsFinite(Steps)) return;
    ViewDistance = FMath::Clamp(ViewDistance * FMath::Pow(0.85, Steps), 500.0, 2000000.0);
    UpdateCameraTransform();
}
void UCommandMapInteractionService::RotateView(const FVector2D& Delta)
{
    if (CurrentMapMode != ECommandMapMode::Map3D || Delta.ContainsNaN()) return;
    ViewYaw = FMath::UnwindDegrees(ViewYaw + Delta.X * 0.25f);
    View3DPitch = FMath::Clamp(View3DPitch - Delta.Y * 0.25f, -89.f, -10.f);
    UpdateCameraTransform();
}
void UCommandMapInteractionService::TickView(float DeltaSeconds)
{
    if (!Controller.IsValid() || !OwnsCamera()) return;
    // Existing spline components only: no RefreshPath, waypoint mutation or playback restart.
    int32 Width = 0, Height = 0;
    Controller->GetViewportSize(Width, Height);
    if (Width > 0)
    {
        const double UnitsPerPixel = 2.0 * ViewDistance * FMath::Tan(FMath::DegreesToRadians(MapCamera->GetCameraComponent()->FieldOfView * .5)) / Width;
        for (TActorIterator<ADronePathActor> It(Controller->GetWorld()); It; ++It)
            It->SetMapDisplayRadius(static_cast<float>(UnitsPerPixel * 1.5));
    }
    if (APawn* Pawn = Controller->GetPawn(); Pawn && Pawn->InputEnabled()) Pawn->DisableInput(Controller.Get());
    if (Controller->GetViewTarget() != MapCamera.Get()) Controller->SetViewTarget(MapCamera.Get());
    const auto* Manager = Controller->GetCommandScreenManager();
    if (!Manager || !Manager->IsCursorOverMap()) return;
    float X = 0, Y = 0;
    Controller->GetInputMouseDelta(X, Y);
    if (Controller->IsInputKeyDown(EKeys::MiddleMouseButton)) PanView(FVector2D(-X, Y));
    else if (Controller->IsInputKeyDown(EKeys::RightMouseButton)) RotateView(FVector2D(X, Y));
    if (Controller->WasInputKeyJustPressed(EKeys::MouseScrollUp)) ZoomView(1.f);
    if (Controller->WasInputKeyJustPressed(EKeys::MouseScrollDown)) ZoomView(-1.f);
}
bool UCommandMapInteractionService::FocusGeographicLocation(double Latitude, double Longitude, double EllipsoidHeightMeters)
{
    if (!Registry.IsValid() || !FMath::IsFinite(Latitude) || !FMath::IsFinite(Longitude) || !FMath::IsFinite(EllipsoidHeightMeters)
        || FMath::Abs(Latitude) > 90 || FMath::Abs(Longitude) > 180) return false;
    const auto Service = Registry->GetCoordinateService();
    UObject* Object = Service.GetObject();
    if (!Object || !ICoordinateService::Execute_IsCoordinateSystemReady(Object) || !ICoordinateService::Execute_IsGeographicSupported(Object)) return false;
    return FocusLocation(ICoordinateService::Execute_GeographicToWorld(Object, Latitude, Longitude, EllipsoidHeightMeters));
}
void UCommandMapInteractionService::LogLayerState() const
{
    for (const FLayer& Layer : Layers)
        if (const ACesium3DTileset* Actor = Layer.Actor.Get())
            UE_LOG(LogTemp, Log, TEXT("[CommandMap] Mode=%s layer=%s sharedBasemap=%d source=%d suspended=%d hidden=%d ionAsset=%lld"),
                CurrentMapMode == ECommandMapMode::Map2D ? TEXT("2D") : TEXT("3D"), *Actor->GetName(), Layer.bBasemap,
                static_cast<int32>(Actor->GetTilesetSource()), Actor->SuspendUpdate, Actor->IsHidden(), Actor->GetIonAssetID());
    for (const FLayer& Layer : Layers)
        for (const auto& Pair : Layer.RasterActivation)
            if (const auto* Raster = Pair.Key.Get())
                UE_LOG(LogTemp, Log, TEXT("[CommandMap] raster=%s class=%s active=%d carrier=%s"),
                    *Raster->GetName(), *Raster->GetClass()->GetName(), Raster->IsActive(), *GetNameSafe(Raster->GetOwner()));
}
void UCommandMapInteractionService::RasterFailed(const FCesiumRasterOverlayLoadFailureDetails& Details)
{
    if (Details.Overlay.IsValid() && Details.Overlay->GetWorld() == MapWorld.Get())
        UE_LOG(LogTemp, Warning, TEXT("BASEMAP CONNECTIVITY ERROR: overlay=%s HTTP=%d; inspect original Cesium error; Command state retained"), *Details.Overlay->GetName(), Details.HttpStatusCode);
}
void UCommandMapInteractionService::TilesetFailed(const FCesium3DTilesetLoadFailureDetails& Details)
{
    if (Details.Tileset.IsValid() && Details.Tileset->GetWorld() == MapWorld.Get())
        UE_LOG(LogTemp, Warning, TEXT("BASEMAP CONNECTIVITY ERROR: tileset=%s HTTP=%d; inspect original Cesium error; Command state retained"), *Details.Tileset->GetName(), Details.HttpStatusCode);
}
void UCommandMapInteractionService::BeginDestroy()
{
    OnCesiumRasterOverlayLoadFailure.RemoveAll(this);
    OnCesium3DTilesetLoadFailure.RemoveAll(this);
    Super::BeginDestroy();
}
