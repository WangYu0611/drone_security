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
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/IInputProcessor.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SViewport.h"
#include "Layout/WidgetPath.h"
#include "PathEditor/DronePathActor.h"

// UMG retains ownership. A hit on an editable waypoint body owns its complete
// native gesture, so deferred gameplay input cannot re-read its release as press.
// All buttons use native screen coordinates; polling MouseY has opposite signs
// and can lose an entire press/move/release sequence between game ticks.
class FMapPointerInput final : public IInputProcessor
{
    TWeakObjectPtr<UCommandMapInteractionService> Service;
    bool bWaypointDrag=false,bGroundClick=false;
    bool WaypointPosition(const FVector2D& Screen,FVector2D& Local) const {
        if(!Service.IsValid() || !Service->Controller.IsValid())return false;
        auto* PC=Service->Controller.Get();auto* Viewport=PC->GetWorld()->GetGameViewport();
        const auto Widget=Viewport?Viewport->GetGameViewportWidget():nullptr;
        if(!Widget.IsValid())return false;
        const auto& Geometry=Widget->GetCachedGeometry();const FVector2D Size=Geometry.GetLocalSize();
        int W=0,H=0;PC->GetViewportSize(W,H);if(Size.X<=0 || Size.Y<=0 || W<=0 || H<=0)return false;
        Local=Geometry.AbsoluteToLocal(Screen)*FVector2D(W/Size.X,H/Size.Y);return true;
    }
    void CancelWaypoint(){
        if((bWaypointDrag || bGroundClick) && Service.IsValid() && Service->Controller.IsValid())Service->Controller->CancelMissionGesture();
        bWaypointDrag=false;bGroundClick=false;
    }
public:
    explicit FMapPointerInput(UCommandMapInteractionService* In):Service(In){}
    void Tick(float,FSlateApplication& App,TSharedRef<ICursor>) override {
        if((bWaypointDrag || bGroundClick) && (!App.IsActive() || !App.GetPressedMouseButtons().Contains(EKeys::LeftMouseButton)))CancelWaypoint();
        if(Service.IsValid() && (!App.IsActive() ||
            !App.GetPressedMouseButtons().Contains(Service->GestureButton)))Service->CancelPointer();
    }
    bool HandleMouseButtonDownEvent(FSlateApplication&,const FPointerEvent& E) override {
        FVector2D Position;
        if(Service.IsValid() && E.GetEffectingButton()==EKeys::LeftMouseButton
            && Service->IsPlanning() && Service->CanUsePointer(E.GetScreenSpacePosition())
            && WaypointPosition(E.GetScreenSpacePosition(),Position)
            && Service->Controller->TryBeginMapWaypointBodyDrag(Position)){
            bWaypointDrag=true;Service->Controller->bNativeMapWaypointDrag=true;return true;
        }
        if(Service.IsValid() && E.GetEffectingButton()==EKeys::LeftMouseButton
            && Service->IsPlanning() && Service->CanUsePointer(E.GetScreenSpacePosition())
            && WaypointPosition(E.GetScreenSpacePosition(),Position)
            && Service->Controller->BeginMapGroundClick(Position)){bGroundClick=true;return true;}
        if(Service.IsValid())Service->BeginPointer(E.GetEffectingButton(),E.GetScreenSpacePosition(),
            Service->CanUsePointer(E.GetScreenSpacePosition()),E.IsShiftDown());
        return false;
    }
    bool HandleMouseMoveEvent(FSlateApplication&,const FPointerEvent& E) override {
        if(bGroundClick){
            FVector2D Position;
            const bool Over=Service.IsValid() && Service->CanUsePointer(E.GetScreenSpacePosition()) && WaypointPosition(E.GetScreenSpacePosition(),Position);
            if(Service.IsValid() && Service->Controller.IsValid())Service->Controller->UpdateMapGroundClick(Position,Over);
            return true;
        }
        if(bWaypointDrag){
            FVector2D Position;
            if(Service.IsValid() && Service->CanUsePointer(E.GetScreenSpacePosition()) && WaypointPosition(E.GetScreenSpacePosition(),Position))Service->Controller->UpdateMapWaypointBodyDrag(Position);
            return true;
        }
        if(Service.IsValid() && Service->GestureButton.IsValid())Service->MovePointer(E.GetScreenSpacePosition(),Service->CanUsePointer(E.GetScreenSpacePosition()));
        return false;
    }
    bool HandleMouseButtonUpEvent(FSlateApplication&,const FPointerEvent& E) override {
        if(bGroundClick && E.GetEffectingButton()==EKeys::LeftMouseButton){
            FVector2D Position;
            const bool Over=Service.IsValid() && Service->CanUsePointer(E.GetScreenSpacePosition()) && WaypointPosition(E.GetScreenSpacePosition(),Position);
            if(Service.IsValid() && Service->Controller.IsValid())Service->Controller->CompleteMapGroundClick(Position,Over);
            bGroundClick=false;return true;
        }
        if(bWaypointDrag && E.GetEffectingButton()==EKeys::LeftMouseButton){
            FVector2D Position;
            if(Service.IsValid() && Service->CanUsePointer(E.GetScreenSpacePosition()) && WaypointPosition(E.GetScreenSpacePosition(),Position)){
                Service->Controller->UpdateMapWaypointBodyDrag(Position);Service->Controller->HandleEditModeReleased();bWaypointDrag=false;
            }else if(Service.IsValid() && Service->Controller.IsValid()){Service->Controller->HandleEditModeReleased();bWaypointDrag=false;}
            return true;
        }
        if(Service.IsValid())Service->EndPointer(E.GetEffectingButton(),E.GetScreenSpacePosition(),
            Service->CanUsePointer(E.GetScreenSpacePosition()));
        return false;
    }
    bool HandleMouseWheelOrGestureEvent(FSlateApplication&,const FPointerEvent& E,const FPointerEvent*) override {
        if(Service.IsValid() && Service->CanUsePointer(E.GetScreenSpacePosition()))Service->ZoomAtScreen(E.GetWheelDelta(),E.GetScreenSpacePosition());
        return false;
    }
};

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
            if(FSlateApplication::IsInitialized() && !PointerInput.IsValid()){
                PointerInput=MakeShared<FMapPointerInput>(this);
                FSlateApplication::Get().RegisterInputPreProcessor(PointerInput);
            }
        }
    }
}

void UCommandMapInteractionService::Unbind()
{
    if(PointerInput.IsValid() && FSlateApplication::IsInitialized())FSlateApplication::Get().UnregisterInputPreProcessor(PointerInput);
    PointerInput.Reset();CancelPointer();
    if (MapWorld.IsValid() && OwnsCamera())
        for (TActorIterator<ADronePathActor> It(MapWorld.Get()); It; ++It) It->SetMapDisplayRadius(0.f);
    if (Controller.IsValid() && OwnsCamera()) Controller->HitResultTraceDistance = OriginalHitResultTraceDistance;
    if (MapCamera.IsValid()) MapCamera->Destroy();
    MapCamera.Reset();
    PathPanel.Reset(); Registry.Reset(); Controller.Reset();
}

void UCommandMapInteractionService::SetRouteEditCameraLocked(bool Locked)
{
    if(bRouteEditCameraLocked==Locked)return;
    CancelPointer();
    bZoomAnchored=false;
    if(Locked){
        // Freeze the currently displayed pose once; discard outstanding damping
        // targets without teleporting or changing the browsing integrator.
        ViewFocus=ActualFocus;ViewDistance=ActualDistance;
        ViewYaw=ActualRotation.Yaw;
        if(CurrentMapMode==ECommandMapMode::Map3D)View3DPitch=ActualRotation.Pitch;
        if(Controller.IsValid() && Controller->GetCommandScreenManager())
            Controller->GetCommandScreenManager()->StopCameraFollow();
    }
    bRouteEditCameraLocked=Locked;
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
    if(bRouteEditCameraLocked)return false;
    return Controller.IsValid() && Controller->FocusCommandDrone(DroneId);
}

bool UCommandMapInteractionService::FocusLocation(const FVector& WorldLocation)
{
    if(bRouteEditCameraLocked)return false;
    if (WorldLocation.ContainsNaN()) return false;
    if (OwnsCamera())
    {
        CancelPointer(); bZoomAnchored=false; ViewFocus = WorldLocation;
        UpdateCameraTransform();
        return true;
    }
    return Controller.IsValid() && Controller->FocusCommandLocation(WorldLocation);
}

bool UCommandMapInteractionService::FocusExecutionBounds(const FBox& Bounds)
{
    if(bRouteEditCameraLocked)return false;
    if(!Bounds.IsValid || !OwnsCamera())return false;
    // Frame the immutable mission snapshot without changing coordinates or selection.
    ViewDistance=FMath::Clamp(FMath::Max(15000.,Bounds.GetExtent().Size()*5.),double(MinDistance),double(MaxDistance));
    return FocusLocation(Bounds.GetCenter());
}

bool UCommandMapInteractionService::FocusAlertOnMap(int32 AlertId)
{
    if(bRouteEditCameraLocked)return false;
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
    auto ConfigFloat=[&](const TCHAR* Key,float& Value,float Min,float Max) {
        GConfig->GetFloat(TEXT("CommandMapCamera"),Key,Value,GGameIni);
        Value=FMath::Clamp(FMath::IsFinite(Value)?Value:Min,Min,Max);
    };
    ConfigFloat(TEXT("PanDampingSeconds"),PanDamping,.01f,1.f);
    ConfigFloat(TEXT("RotationDampingSeconds"),RotationDamping,.01f,1.f);
    ConfigFloat(TEXT("ZoomDampingSeconds"),ZoomDamping,.01f,1.f);
    ConfigFloat(TEXT("MinPitchDegrees"),MinPitch,1.f,80.f);
    ConfigFloat(TEXT("MaxPitchDegrees"),MaxPitch,MinPitch,89.f);
    ConfigFloat(TEXT("RotationSensitivity"),RotateSensitivity,.01f,2.f);
    ConfigFloat(TEXT("DragThresholdPixels"),DragThreshold,2.f,20.f);
    ConfigFloat(TEXT("MinDistance"),MinDistance,1.f,2000000.f);
    ConfigFloat(TEXT("MaxDistance"),MaxDistance,MinDistance,100000000.f);
    ConfigFloat(TEXT("PanSensitivity"),PanSensitivity,.1f,4.f);
    ConfigFloat(TEXT("ZoomRatio"),ZoomRatio,.1f,.99f);
    ViewDistance=FMath::Clamp(ViewDistance,double(MinDistance),double(MaxDistance));
    View3DPitch=FMath::Clamp(View3DPitch,-MaxPitch,-MinPitch);
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
    if(bRouteEditCameraLocked)return;
    if (Mode != ECommandMapMode::Map2D && Mode != ECommandMapMode::Map3D) return;
    bModeRequested = true;
    if (CurrentMapMode == Mode) { ApplyPendingMapMode(); return; }
    CancelPointer(); bZoomAnchored=false;
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
    if (!bCameraInitialized) {
        ActualFocus=ViewFocus; ActualDistance=ViewDistance;
        ActualRotation=FRotator(CurrentMapMode==ECommandMapMode::Map2D?-89.9f:View3DPitch,ViewYaw,0);
        bCameraInitialized=true;
        MapCamera->SetActorLocationAndRotation(ActualFocus-ActualRotation.Vector()*ActualDistance,ActualRotation);
    }
}
bool UCommandMapInteractionService::CanUsePointer(const FVector2D& Cursor) const
{
    if(!Controller.IsValid() || !OwnsCamera() || !FSlateApplication::IsInitialized())return false;
    auto* Viewport=Controller->GetWorld()->GetGameViewport();
    const auto Widget=Viewport?Viewport->GetGameViewportWidget():nullptr;
    if(!Widget.IsValid())return false;
    auto& App=FSlateApplication::Get();
    if(!App.IsActive())return false;
    // Hit testing respects popup menus, other windows and UMG consumption,
    // whereas testing only a panel rectangle cannot see a dropdown popup.
    const auto Path=App.LocateWindowUnderMouse(Cursor,App.GetInteractiveTopLevelWindows());
    if(!Path.IsValid() || Path.Widgets.Last().Widget!=Widget)return false;
    const auto* Manager=Controller->GetCommandScreenManager();
    return Manager && Manager->IsCursorOverMap();
}
void UCommandMapInteractionService::CancelPointer()
{
    GestureButton=FKey();bLeftGesture=false;bLeftDragging=false;
    LeftStart=PointerLast=FVector2D::ZeroVector;
}
void UCommandMapInteractionService::BeginPointer(const FKey& Button,const FVector2D& Cursor,bool bOverMap,bool bShift)
{
    CancelPointer();
    if(bRouteEditCameraLocked || !bOverMap || Cursor.ContainsNaN())return;
    const bool Primary=Button==EKeys::LeftMouseButton;
    if(Primary && (IsPlanning() || bShift || CurrentMapMode!=ECommandMapMode::Map2D))return;
    if(!Primary && Button!=EKeys::MiddleMouseButton && Button!=EKeys::RightMouseButton)return;
    GestureButton=Button;LeftStart=PointerLast=Cursor;bLeftGesture=Primary;
}
void UCommandMapInteractionService::MovePointer(const FVector2D& Cursor,bool bOverMap)
{
    if(!GestureButton.IsValid())return;
    if(bRouteEditCameraLocked || !bOverMap || Cursor.ContainsNaN() || (bLeftGesture && IsPlanning())){CancelPointer();return;}
    if(!bLeftDragging){
        if(FVector2D::Distance(Cursor,LeftStart)<=DragThreshold)return;
        bLeftDragging=true;
    }
    const FVector2D Delta=Cursor-PointerLast;PointerLast=Cursor;
    if(GestureButton==EKeys::RightMouseButton && CurrentMapMode==ECommandMapMode::Map3D)RotateView(Delta);
    else PanView(Delta);
}
void UCommandMapInteractionService::EndPointer(const FKey& Button,const FVector2D& Cursor,bool bOverMap)
{
    if(Button!=GestureButton)return;
    MovePointer(Cursor,bOverMap);CancelPointer();
}
void UCommandMapInteractionService::BeginPrimaryGesture(const FVector2D& Cursor)
{
    BeginPointer(EKeys::LeftMouseButton,Cursor,CanUsePointer(Cursor),
        FSlateApplication::IsInitialized() && FSlateApplication::Get().GetModifierKeys().IsShiftDown());
}
void UCommandMapInteractionService::EndPrimaryGesture(const FVector2D& Cursor)
{
    EndPointer(EKeys::LeftMouseButton,Cursor,CanUsePointer(Cursor));
}
void UCommandMapInteractionService::PanView(const FVector2D& Delta)
{
    if(bRouteEditCameraLocked)return;
    if(!Controller.IsValid() || !OwnsCamera() || Delta.ContainsNaN())return;
    int32 W=0,H=0;Controller->GetViewportSize(W,H);if(W<=0)return;
    const double Scale=2.*ActualDistance*FMath::Tan(FMath::DegreesToRadians(MapCamera->GetCameraComponent()->FieldOfView*.5))/W;
    const FRotationMatrix Basis(FRotator(0,ActualRotation.Yaw,0));
    const double Down=FMath::Max(.01,-ActualRotation.Vector().Z);
    // Screen X grows right, Y down. Moving the camera opposite the ray-plane
    // displacement makes ground content follow the pointer at every yaw/pitch.
    ViewFocus+=(-Basis.GetUnitAxis(EAxis::Y)*Delta.X+Basis.GetUnitAxis(EAxis::X)*(Delta.Y/Down))*Scale*PanSensitivity;
    bZoomAnchored=false;
}
FVector UCommandMapInteractionService::PlaneOffset(double Distance,const FRotator& Rotation,const FVector2D& RaySlope) const
{
    const FRotationMatrix Basis(Rotation);
    const FVector Forward=Basis.GetUnitAxis(EAxis::X);
    const FVector Ray=Forward+Basis.GetUnitAxis(EAxis::Y)*RaySlope.X+Basis.GetUnitAxis(EAxis::Z)*RaySlope.Y;
    if(Ray.Z>=-.001)return FVector::ZeroVector;
    return -Forward*Distance+Ray*(Forward.Z*Distance/Ray.Z);
}
void UCommandMapInteractionService::ZoomAt(float Steps,const FVector2D* RaySlope)
{
    if(bRouteEditCameraLocked)return;
    if(!FMath::IsFinite(Steps) || Steps==0)return;
    const double Next=FMath::Clamp(ViewDistance*FMath::Pow(double(ZoomRatio),double(FMath::Clamp(Steps,-100.f,100.f))),double(MinDistance),double(MaxDistance));
    if(Next==ViewDistance)return; // No hidden target movement at either limit.
    if(CurrentMapMode==ECommandMapMode::Map2D && RaySlope && !RaySlope->ContainsNaN()){
        if(!bZoomAnchored || !ZoomCursor.Equals(*RaySlope,1.e-6)){
            ZoomCursor=*RaySlope;
            ZoomAnchor=ActualFocus+PlaneOffset(ActualDistance,ActualRotation,ZoomCursor);
        }
        bZoomAnchored=true;
        const FRotator Target(-89.9f,ViewYaw,0);
        ViewFocus=ZoomAnchor-PlaneOffset(Next,Target,ZoomCursor);
    }else bZoomAnchored=false; // 3D dolly retains its focus/orbit pivot.
    ViewDistance=Next;
}
void UCommandMapInteractionService::ZoomAtScreen(float Steps,const FVector2D& ScreenPosition)
{
    if(!Controller.IsValid() || !OwnsCamera() || ScreenPosition.ContainsNaN())return;
    auto* Viewport=Controller->GetWorld()->GetGameViewport();
    const auto Widget=Viewport?Viewport->GetGameViewportWidget():nullptr;
    if(!Widget.IsValid())return;
    const auto& Geometry=Widget->GetCachedGeometry();
    const FVector2D Size=Geometry.GetLocalSize(),Local=Geometry.AbsoluteToLocal(ScreenPosition);
    if(Size.X<=0 || Size.Y<=0)return;
    const double Tan=FMath::Tan(FMath::DegreesToRadians(MapCamera->GetCameraComponent()->FieldOfView*.5));
    const FVector2D Slope((2.*Local.X/Size.X-1.)*Tan,(Size.Y-2.*Local.Y)/Size.X*Tan);
    // PlayerController cursor polling can still hold the preceding mouse-move
    // position while Slate dispatches this wheel event. Use its own position.
    ZoomAt(Steps,&Slope);
}
void UCommandMapInteractionService::ZoomView(float Steps)
{
    FVector2D Slope;
    if(Controller.IsValid() && OwnsCamera()){
        double X=0,Y=0;int32 W=0,H=0;Controller->GetViewportSize(W,H);
        if(W>0 && H>0 && Controller->GetMousePosition(X,Y)){
            const double Tan=FMath::Tan(FMath::DegreesToRadians(MapCamera->GetCameraComponent()->FieldOfView*.5));
            Slope=FVector2D((2.*X/W-1.)*Tan,(H-2.*Y)/W*Tan);
            ZoomAt(Steps,&Slope);return;
        }
    }
    ZoomAt(Steps,nullptr);
}
void UCommandMapInteractionService::RotateView(const FVector2D& Delta)
{
    if(bRouteEditCameraLocked)return;
    if(CurrentMapMode!=ECommandMapMode::Map3D || Delta.ContainsNaN())return;
    bZoomAnchored=false;
    ViewYaw=FMath::UnwindDegrees(ViewYaw+Delta.X*RotateSensitivity);
    View3DPitch=FMath::Clamp(View3DPitch-Delta.Y*RotateSensitivity,-MaxPitch,-MinPitch);
}
void UCommandMapInteractionService::AdvanceCamera(float DeltaSeconds)
{
    if(bRouteEditCameraLocked || !OwnsCamera() || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds<=0)return;
    const double Dt=DeltaSeconds;
    ActualFocus=FMath::Lerp(ActualFocus,ViewFocus,1.-FMath::Exp(-Dt/PanDamping));
    ActualDistance=FMath::Lerp(ActualDistance,ViewDistance,1.-FMath::Exp(-Dt/ZoomDamping));
    const FRotator Target(CurrentMapMode==ECommandMapMode::Map2D?-89.9f:View3DPitch,ViewYaw,0);
    ActualRotation=FQuat::Slerp(ActualRotation.Quaternion(),Target.Quaternion(),1.-FMath::Exp(-Dt/RotationDamping)).Rotator();
    if(bZoomAnchored)ActualFocus=ZoomAnchor-PlaneOffset(ActualDistance,ActualRotation,ZoomCursor);
    // Sub-millimetre positional / sub-pixel angular tail ends exactly at target.
    if(ActualFocus.Equals(ViewFocus,.01))ActualFocus=ViewFocus;
    if(FMath::Abs(ActualDistance-ViewDistance)<.01)ActualDistance=ViewDistance;
    if(ActualRotation.Equals(Target,.0001))ActualRotation=Target;
    MapCamera->SetActorLocationAndRotation(ActualFocus-ActualRotation.Vector()*ActualDistance,ActualRotation);
}
void UCommandMapInteractionService::TickView(float DeltaSeconds)
{
    if(!Controller.IsValid() || !OwnsCamera())return;
    if(APawn* Pawn=Controller->GetPawn();Pawn && Pawn->InputEnabled())Pawn->DisableInput(Controller.Get());
    if(Controller->GetViewTarget()!=MapCamera.Get())Controller->SetViewTarget(MapCamera.Get());
    AdvanceCamera(DeltaSeconds);
    Controller->HitResultTraceDistance=FMath::Max(OriginalHitResultTraceDistance,static_cast<float>(ActualDistance*4.));
    int32 Width=0,Height=0;Controller->GetViewportSize(Width,Height);
    if(Width>0){const double Scale=2.*ActualDistance*FMath::Tan(FMath::DegreesToRadians(MapCamera->GetCameraComponent()->FieldOfView*.5))/Width;
        for(TActorIterator<ADronePathActor> It(Controller->GetWorld());It;++It)It->SetMapDisplayRadius(static_cast<float>(Scale*1.5));}
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
    if(PointerInput.IsValid() && FSlateApplication::IsInitialized())FSlateApplication::Get().UnregisterInputPreProcessor(PointerInput);
    PointerInput.Reset();
    OnCesiumRasterOverlayLoadFailure.RemoveAll(this);
    OnCesium3DTilesetLoadFailure.RemoveAll(this);
    Super::BeginDestroy();
}
