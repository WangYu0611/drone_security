#pragma once
#include "CoreMinimal.h"

class AActor;
class APlayerController;
class UMeshComponent;

/** Map-only visibility override. Never changes model size, transforms or collision. */
namespace MapDronePresentation
{
    inline constexpr double IconDiameter = 36.;
    inline constexpr double ModelEnterPixels = 48.;
    inline constexpr double ModelExitPixels = 32.;
    // A small native model must still be revealed at the camera's 5m zoom limit.
    inline constexpr double ModelEnterDistanceCm = 1200.;
    inline constexpr double ModelExitDistanceCm = 1800.;

    bool ShouldShowModel(bool WasModel, bool Is3D, bool ModelReady,
        double ProjectedDiameter, double CameraDistanceCm);
    bool ProjectBounds(APlayerController* Player, const FBox& Bounds, FBox2D& ScreenBounds);

    class FVisibilityOverride
    {
    public:
        bool ActiveBounds(const AActor* Actor, FBox& Bounds) const;
        void BeginFrame();
        void SetSuppressed(AActor* Actor, bool Suppressed);
        void EndFrame();
        void RestoreAll();
    private:
        // HiddenInGame is our presentation override; Visible remains owned by
        // DroneVisualComponent, including runtime Original/static model changes.
        TMap<TWeakObjectPtr<UMeshComponent>, bool> OriginalHidden;
        TSet<TWeakObjectPtr<UMeshComponent>> Touched;
        bool WasOriginallyHidden(UMeshComponent* Mesh) const;
    };
}
