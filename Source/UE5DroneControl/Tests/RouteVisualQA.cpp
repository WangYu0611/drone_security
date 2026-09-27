#if WITH_DEV_AUTOMATION_TESTS
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Components/SplineMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PathEditor/DronePathActor.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "DroneOps/Core/ICoordinateService.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "Command/CommandScreenManager.h"
#include "Command/CommandMapInteractionService.h"

// Explicit development fixture only. No Backend writes or fabricated execution progress.
static FAutoConsoleCommand P55VisualQA(TEXT("P55.QA"),TEXT("Local Route V2 visual fixture: overview, joint, vertical, conflict, clear, active, completed, perf, inspect, remove"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args){
    if(!GEngine)return;
    for(const auto& Context:GEngine->GetWorldContexts()){
        auto* W=Context.World();if(!W || !W->IsGameWorld() || !W->GetGameInstance())continue;
        auto* PC=Cast<ADroneOpsPlayerController>(W->GetFirstPlayerController());if(!PC || !PC->GetCommandScreenManager())continue;
        auto* C=W->GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetCoordinateService().GetObject();
        if(!C || !ICoordinateService::Execute_IsCoordinateSystemReady(C))return;
        const FString Mode=Args.IsEmpty()?TEXT("overview"):Args[0];
        if(Mode==TEXT("inspect")){
            for(TActorIterator<ADronePathActor> It(W);It;++It){TArray<USplineMeshComponent*> Meshes;It->GetComponents(Meshes);
                for(auto* M:Meshes){auto* MID=Cast<UMaterialInstanceDynamic>(M->GetMaterial(0));if(MID)UE_LOG(LogTemp,Display,TEXT("P55 MATERIAL %s visible=%d radius=%s start=%s end=%s halo=%f color=%s flow=%f"),*MID->Parent->GetPathName(),M->IsVisible(),*M->GetStartScale().ToString(),*MID->K2_GetVectorParameterValue(TEXT("StartWorld")).ToString(),*MID->K2_GetVectorParameterValue(TEXT("EndWorld")).ToString(),MID->K2_GetScalarParameterValue(TEXT("IsHalo")),*MID->K2_GetVectorParameterValue(TEXT("StartColor")).ToString(),MID->K2_GetScalarParameterValue(TEXT("FlowRate")));}}
            return;
        }
        TArray<ADronePathActor*> Existing;for(TActorIterator<ADronePathActor> It(W);It;++It)if(It->Tags.Contains(TEXT("P55VisualFixture")))Existing.Add(*It);
        if(Mode==TEXT("conflict") || Mode==TEXT("clear") || Mode==TEXT("active") || Mode==TEXT("completed")){
            for(auto* P:Existing){if(Mode==TEXT("conflict"))P->MarkConflictSegment(1,2);
                if(Mode==TEXT("clear"))P->ClearConflictVisualization();
                if(Mode==TEXT("active") || Mode==TEXT("completed"))P->SetRoutePresentation(Mode==TEXT("active")?ERouteVisualState::Active:ERouteVisualState::Completed,P==Existing[0]);
                P->RefreshConflictVisualization();}return;
        }
        for(auto* P:Existing)P->Destroy();
        for(TActorIterator<AStaticMeshActor> It(W);It;++It)if(It->Tags.Contains(TEXT("P55VisualOccluder")))It->Destroy();
        if(Mode==TEXT("remove"))return;
        FBox Bounds(ForceInit);
        const double Heights[]{40,60,100,140,80};const float Speeds[]{0,1,6,10,15};
        for(int R=0;R<((Mode==TEXT("perf") || Mode==TEXT("dark") || Mode==TEXT("occlusion"))?1:3);++R){
            auto* P=W->SpawnActor<ADronePathActor>();P->Tags.Add(TEXT("P55VisualFixture"));P->PathNumericId=91+R;
            P->bParticipatesInConflictChecks=false;P->bClosedLoop=true;
            P->bSpawnWaypointHandlesAtRuntime=Mode!=TEXT("joint");
            const int Count=Mode==TEXT("perf")?100:5;
            for(int I=0;I<Count;++I){FDroneWaypoint V;
                const double Angle=I*2*PI/Count;
                const double Lat=39.9827+FMath::Sin(Mode==TEXT("vertical") && I==1?0.:Angle)*.0006+R*.0015;
                const double Lon=116.3466+FMath::Cos(Mode==TEXT("vertical") && I==1?0.:Angle)*.001;
                V.Location=ICoordinateService::Execute_GeographicToWorld(C,Lat,Lon,Heights[I%5]);
                if(Mode==TEXT("dark"))V.Location+=FVector(40000,0,0);
                V.SegmentSpeed=Speeds[I%5];P->Waypoints.Add(V);Bounds+=V.Location;
            }
            P->RefreshPath();P->SetRoutePresentation(R==0?ERouteVisualState::Planning:ERouteVisualState::Confirmed,R==0);
            if(Mode==TEXT("occlusion")){
                auto* Box=W->SpawnActor<AStaticMeshActor>();Box->Tags.Add(TEXT("P55VisualOccluder"));
                auto* Mesh=Box->GetStaticMeshComponent();Mesh->SetMobility(EComponentMobility::Movable);
                Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));
                Mesh->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Tests/M_P55QADark")));
                Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                Box->SetActorLocation((P->Waypoints[0].Location+P->Waypoints[1].Location)*.5);Box->SetActorScale3D(FVector(40,40,100));
            }
        }
        PC->GetCommandScreenManager()->GetMapService()->FocusExecutionBounds(Bounds);
        UE_LOG(LogTemp,Display,TEXT("P55 LOCAL VISUAL FIXTURE %s: not Backend execution or native state transition evidence"),*Mode);return;
    }
}));
#endif
