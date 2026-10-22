#include "Map/MapDronePresentation.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Actor.h"

namespace MapDronePresentation
{
    namespace
    {
        bool IsModelMesh(const UMeshComponent* Mesh)
        {
            return Mesh && (Mesh->IsA<UStaticMeshComponent>() || Mesh->IsA<USkinnedMeshComponent>());
        }
        bool HasModelAsset(const UMeshComponent* Mesh)
        {
            if (const auto* Static = Cast<UStaticMeshComponent>(Mesh)) return Static->GetStaticMesh() != nullptr;
            if (const auto* Skinned = Cast<USkinnedMeshComponent>(Mesh)) return Skinned->GetSkinnedAsset() != nullptr;
            return false;
        }
    }

    bool ShouldShowModel(bool WasModel, bool Is3D, bool ModelReady,
        double ProjectedDiameter, double CameraDistanceCm)
    {
        if (!Is3D || !ModelReady || !FMath::IsFinite(ProjectedDiameter) || ProjectedDiameter <= 0.
            || !FMath::IsFinite(CameraDistanceCm) || CameraDistanceCm < 0.) return false;
        return WasModel
            ? ProjectedDiameter >= ModelExitPixels || CameraDistanceCm <= ModelExitDistanceCm
            : ProjectedDiameter >= ModelEnterPixels || CameraDistanceCm <= ModelEnterDistanceCm;
    }

    bool ProjectBounds(APlayerController* Player, const FBox& Bounds, FBox2D& ScreenBounds)
    {
        ScreenBounds = FBox2D(ForceInit);
        if (!Player || !Bounds.IsValid) return false;
        for (int32 X = 0; X < 2; ++X) for (int32 Y = 0; Y < 2; ++Y) for (int32 Z = 0; Z < 2; ++Z)
        {
            const FVector Corner(X ? Bounds.Max.X : Bounds.Min.X, Y ? Bounds.Max.Y : Bounds.Min.Y, Z ? Bounds.Max.Z : Bounds.Min.Z);
            FVector2D Point;
            if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(Player, Corner, Point, true) || Point.ContainsNaN()) return false;
            ScreenBounds += Point;
        }
        return ScreenBounds.bIsValid && ScreenBounds.GetSize().GetMax() > 0.;
    }

    bool FVisibilityOverride::WasOriginallyHidden(UMeshComponent* Mesh) const
    {
        const bool* Hidden = OriginalHidden.Find(Mesh);
        return Hidden ? *Hidden : Mesh->bHiddenInGame;
    }

    bool FVisibilityOverride::ActiveBounds(const AActor* Actor, FBox& Bounds) const
    {
        Bounds = FBox(ForceInit);
        if (!IsValid(Actor) || Actor->IsHidden()) return false;
        TArray<UMeshComponent*> Meshes; Actor->GetComponents(Meshes);
        for (auto* Mesh : Meshes)
        {
            if (!IsValid(Mesh) || !Mesh->IsRegistered() || !IsModelMesh(Mesh) || !HasModelAsset(Mesh)
                || !Mesh->GetVisibleFlag() || WasOriginallyHidden(Mesh)) continue;
            const FBox Box = Mesh->Bounds.GetBox();
            if (Box.IsValid && !Box.Min.ContainsNaN() && !Box.Max.ContainsNaN() && Box.GetExtent().GetMax() > 0.) Bounds += Box;
        }
        return Bounds.IsValid != 0;
    }

    void FVisibilityOverride::BeginFrame() { Touched.Reset(); }

    void FVisibilityOverride::SetSuppressed(AActor* Actor, bool Suppressed)
    {
        if (!IsValid(Actor)) return;
        TArray<UMeshComponent*> Meshes; Actor->GetComponents(Meshes);
        for (auto* Mesh : Meshes)
        {
            if (!IsValid(Mesh) || Mesh->IsA<UWidgetComponent>()) continue;
            Touched.Add(Mesh);
            const bool Hidden = OriginalHidden.FindOrAdd(Mesh, Mesh->bHiddenInGame);
            // Preserve the existing map suppression of procedural ground trails;
            // their large bounds must never drive the model-distance decision.
            const bool Hide = Hidden || Suppressed || !IsModelMesh(Mesh);
            if (Mesh->bHiddenInGame != Hide) Mesh->SetHiddenInGame(Hide);
        }
    }

    void FVisibilityOverride::EndFrame()
    {
        for (auto It = OriginalHidden.CreateIterator(); It; ++It)
        {
            if (Touched.Contains(It.Key()) && It.Key().IsValid()) continue;
            if (It.Key().IsValid()) It.Key()->SetHiddenInGame(It.Value());
            It.RemoveCurrent();
        }
    }

    void FVisibilityOverride::RestoreAll()
    {
        for (const auto& Item : OriginalHidden) if (Item.Key.IsValid()) Item.Key->SetHiddenInGame(Item.Value);
        OriginalHidden.Reset(); Touched.Reset();
    }
}
