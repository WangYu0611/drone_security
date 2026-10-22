#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Map/MapDronePresentation.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapDronePresentationPolicy,"DroneOps.Fleet.ModelIconPolicy",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMapDronePresentationPolicy::RunTest(const FString&)
{
    using namespace MapDronePresentation;
    TestEqual(TEXT("Far icon keeps its established logical diameter"),IconDiameter,36.);
    TestFalse(TEXT("A small far model starts as an icon"),ShouldShowModel(false,true,true,47.9,10000.));
    TestTrue(TEXT("A 48px projected native model enters the close presentation"),ShouldShowModel(false,true,true,48.,10000.));
    bool Model=true;
    for(double Pixels:{47.,40.,33.,32.,35.,47.})
    {
        Model=ShouldShowModel(Model,true,true,Pixels,10000.);
        TestTrue(TEXT("Zoom jitter inside the dead band retains the model"),Model);
    }
    TestFalse(TEXT("Zooming below the lower threshold restores the icon"),ShouldShowModel(Model,true,true,31.9,10000.));
    for(double Pixels:{32.,40.,47.9})
        TestFalse(TEXT("The same dead band retains an existing icon"),ShouldShowModel(false,true,true,Pixels,10000.));
    TestTrue(TEXT("A tiny native model is revealed at the minimum 5m camera distance"),ShouldShowModel(false,true,true,1.,500.));
    TestTrue(TEXT("Near distance entry threshold is inclusive"),ShouldShowModel(false,true,true,8.,1200.));
    TestFalse(TEXT("A tiny model does not enter outside the near threshold"),ShouldShowModel(false,true,true,8.,1200.1));
    TestTrue(TEXT("Distance hysteresis holds the model through 18m"),ShouldShowModel(true,true,true,8.,1800.));
    TestFalse(TEXT("A small model exits beyond 18m"),ShouldShowModel(true,true,true,8.,1800.1));
    TestFalse(TEXT("2D always retains the fixed icon even when near and previously a model"),ShouldShowModel(true,false,true,200.,500.));
    TestFalse(TEXT("Missing model asset falls back to a visible icon"),ShouldShowModel(true,true,false,200.,500.));
    TestFalse(TEXT("Invalid projection cannot hide the fallback icon"),ShouldShowModel(true,true,true,std::numeric_limits<double>::quiet_NaN(),500.));
    TestFalse(TEXT("Invalid camera position cannot select a model"),ShouldShowModel(true,true,true,100.,std::numeric_limits<double>::infinity()));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapDronePresentationVisibility,"DroneOps.Fleet.ModelIconVisibility",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMapDronePresentationVisibility::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    if(!TestNotNull(TEXT("Transient world for model component lifecycle"),World))return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* Actor=World->SpawnActor<AActor>();
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    if(!TestNotNull(TEXT("Public engine mesh fixture"),Cube) || !TestNotNull(TEXT("Transient aircraft fixture"),Actor))return false;
    auto AddMesh=[&](const TCHAR* Name,bool Visible,bool Hidden){
        auto* Mesh=NewObject<UStaticMeshComponent>(Actor,FName(Name));Actor->AddInstanceComponent(Mesh);
        if(!Actor->GetRootComponent())Actor->SetRootComponent(Mesh);else Mesh->SetupAttachment(Actor->GetRootComponent());
        Mesh->SetStaticMesh(Cube);Mesh->SetVisibility(Visible);Mesh->SetHiddenInGame(Hidden);
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Mesh->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
        Mesh->RegisterComponent();return Mesh;
    };
    auto* Body=AddMesh(TEXT("NativeBody"),true,false);
    auto* Inactive=AddMesh(TEXT("InactiveModel"),false,false);
    auto* IntentionallyHidden=AddMesh(TEXT("OriginallyHiddenPart"),true,true);
    auto* Label=NewObject<UWidgetComponent>(Actor);Actor->AddInstanceComponent(Label);Label->SetupAttachment(Body);Label->SetVisibility(true);Label->RegisterComponent();
    auto* GroundTrail=NewObject<UProceduralMeshComponent>(Actor);Actor->AddInstanceComponent(GroundTrail);GroundTrail->SetupAttachment(Body);GroundTrail->RegisterComponent();
    Actor->SetActorTransform(FTransform(FRotator(5,10,15),FVector(100,200,300),FVector(.75,1.,1.25)));
    const FTransform OriginalTransform=Actor->GetActorTransform();
    const FTransform OriginalBody=Body->GetRelativeTransform();
    MapDronePresentation::FVisibilityOverride Visibility;
    ON_SCOPE_EXIT { Visibility.RestoreAll(); };
    FBox Before(ForceInit);
    TestTrue(TEXT("Native visible mesh has usable bounds"),Visibility.ActiveBounds(Actor,Before));
    Visibility.BeginFrame();Visibility.SetSuppressed(Actor,true);Visibility.EndFrame();
    TestTrue(TEXT("Far map presentation hides the native mesh in game"),Body->bHiddenInGame!=0);
    TestTrue(TEXT("Map leaves active model visibility ownership unchanged"),Body->GetVisibleFlag());
    TestFalse(TEXT("Inactive model stays inactive"),Inactive->GetVisibleFlag());
    TestFalse(TEXT("Widget label is not treated as a model mesh"),Label->bHiddenInGame!=0);
    TestTrue(TEXT("Far presentation retains existing ground trail suppression"),GroundTrail->bHiddenInGame!=0);
    FBox Suppressed(ForceInit);
    TestTrue(TEXT("Map's own override does not make the native model unavailable on the next frame"),Visibility.ActiveBounds(Actor,Suppressed));
    TestTrue(TEXT("Hidden and visible model bounds are identical"),Before.Equals(Suppressed));
    TestTrue(TEXT("Presentation leaves aircraft location, rotation and original scale unchanged"),Actor->GetActorTransform().Equals(OriginalTransform));
    TestTrue(TEXT("Presentation leaves the original model transform unchanged"),Body->GetRelativeTransform().Equals(OriginalBody));
    TestEqual(TEXT("Hidden mesh still retains its selection collision policy"),Body->GetCollisionEnabled(),ECollisionEnabled::QueryOnly);
    TestEqual(TEXT("Mesh selection channel remains blocked"),Body->GetCollisionResponseToChannel(ECC_Visibility),ECR_Block);
    Visibility.BeginFrame();Visibility.SetSuppressed(Actor,false);Visibility.EndFrame();
    TestFalse(TEXT("Close view reveals original body"),Body->bHiddenInGame!=0);
    TestTrue(TEXT("Close presentation does not revive the old procedural ground trail"),GroundTrail->bHiddenInGame!=0);
    TestTrue(TEXT("A deliberately hidden component is never forced visible"),IntentionallyHidden->bHiddenInGame!=0);

    // Changing model type while zoomed out must not resurrect the old type on
    // zoom-in or widget retirement; DroneVisualComponent owns Visible.
    Visibility.BeginFrame();Visibility.SetSuppressed(Actor,true);Visibility.EndFrame();
    Body->SetVisibility(false);auto* Replacement=AddMesh(TEXT("ReplacementModel"),true,false);
    Visibility.BeginFrame();Visibility.SetSuppressed(Actor,true);Visibility.EndFrame();
    TestTrue(TEXT("New model part inherits the current far presentation"),Replacement->bHiddenInGame!=0);
    Visibility.BeginFrame();Visibility.SetSuppressed(Actor,false);Visibility.EndFrame();
    TestFalse(TEXT("Old model stays disabled after a runtime type change"),Body->GetVisibleFlag());
    TestTrue(TEXT("Replacement keeps its model type visibility"),Replacement->GetVisibleFlag());
    TestFalse(TEXT("Replacement is displayed when near"),Replacement->bHiddenInGame!=0);

    Visibility.BeginFrame();Visibility.SetSuppressed(Actor,true);Visibility.EndFrame();
    Visibility.BeginFrame();Visibility.EndFrame();
    TestFalse(TEXT("Removing the registry actor restores its presentation override"),Replacement->bHiddenInGame!=0);
    TestTrue(TEXT("Removing an actor preserves its originally hidden part"),IntentionallyHidden->bHiddenInGame!=0);
    Visibility.BeginFrame();Visibility.SetSuppressed(Actor,true);Visibility.EndFrame();Visibility.RestoreAll();Visibility.RestoreAll();
    TestFalse(TEXT("Widget retirement restores native visibility idempotently"),Replacement->bHiddenInGame!=0);
    TestFalse(TEXT("Widget retirement restores the procedural component's own visibility"),GroundTrail->bHiddenInGame!=0);
    TestFalse(TEXT("Widget retirement cannot revive an obsolete model"),Body->GetVisibleFlag());
    TestTrue(TEXT("All presentation cycles retain original actor transform"),Actor->GetActorTransform().Equals(OriginalTransform));
    return true;
}
#endif
