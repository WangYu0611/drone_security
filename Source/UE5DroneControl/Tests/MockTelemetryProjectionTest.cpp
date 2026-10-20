#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "DroneOps/Network/DroneNetworkManager.h"
#include "Shared/OperationalContextSubsystem.h"
#include "Map/MapExecutionWidget.h"
#include "Map/MapMoveSupport.h"
#include "DroneOps/Core/ICoordinateService.h"
#include "RealTimeDroneReceiver.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace {
class FRunMockTelemetryCheck : public IAutomationLatentCommand {
    TFunction<void()> Check;
public:
    explicit FRunMockTelemetryCheck(TFunction<void()> InCheck):Check(MoveTemp(InCheck)){}
    bool Update() override {Check();return true;}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMockTelemetryProjectionTest,"DroneOps.Mock.TelemetrySource",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMockTelemetryProjectionTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Tests/P55OfflineMap")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2));
    ADD_LATENT_AUTOMATION_COMMAND(FRunMockTelemetryCheck([this]{
        UWorld* World=nullptr;
        for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE)World=Context.World();
        if(!World){AddError(TEXT("PIE world missing"));return;}
        auto* GI=World->GetGameInstance();auto* Registry=GI->GetSubsystem<UDroneRegistrySubsystem>();
        auto* Network=GI->GetSubsystem<UDroneNetworkManager>();auto* Sync=GI->GetSubsystem<UOperationalContextSubsystem>();
        const bool WasIsolated=Network->IsStrictLocalPreviewIsolation();Network->SetStrictLocalPreviewIsolation(true);
        constexpr int32 Id=9001;FDroneDescriptor Desc;Desc.DroneId=Id;Desc.Name=TEXT("Mock provenance fixture");Registry->RegisterDrone(Desc);
        FDroneTelemetrySnapshot Initial;Initial.DroneId=Id;Initial.LastUpdateTime=123.;Registry->UpdateTelemetry(Id,Initial);
        auto Packet=MakeShared<FJsonObject>();Packet->SetStringField(TEXT("type"),TEXT("telemetry"));Packet->SetNumberField(TEXT("drone_id"),Id);
        Packet->SetStringField(TEXT("telemetry_source"),TEXT("mock_home"));Packet->SetBoolField(TEXT("simulation"),true);
        Packet->SetStringField(TEXT("status"),TEXT("offline"));Packet->SetBoolField(TEXT("gps_fix"),true);
        Packet->SetNumberField(TEXT("gps_lat"),39.9807);Packet->SetNumberField(TEXT("gps_lon"),116.34703);Packet->SetNumberField(TEXT("gps_alt"),30.);
        auto Deliver=[&]{FString Json;FJsonSerializer::Serialize(Packet,TJsonWriterFactory<>::Create(&Json));Network->OnWsMessage(Json);};
        Deliver();FDroneTelemetrySnapshot Actual;Registry->GetTelemetry(Id,Actual);
        TestTrue(TEXT("Mock WS provenance mapped"),Actual.bSimulation && Actual.TelemetrySource==TEXT("mock_home"));
        TestTrue(TEXT("Mock location valid without local flight frame"),Actual.bGpsFix && !Actual.bLocalPositionValid);
        TestEqual(TEXT("Mock WS leaves hardware offline"),Actual.Availability,EDroneAvailability::Offline);
        TestEqual(TEXT("Mock WS does not create a heartbeat"),Actual.LastUpdateTime,123.);
        TestEqual(TEXT("Mock GPS latitude mapped"),Actual.GpsLatitude,39.9807);
        Packet->SetStringField(TEXT("status"),TEXT("online"));Deliver();Registry->GetTelemetry(Id,Actual);
        TestEqual(TEXT("Mock cannot upgrade hardware to online"),Actual.Availability,EDroneAvailability::Offline);
        Packet->SetStringField(TEXT("status"),TEXT("offline"));Deliver();

        // Hydrate the same packet through REST while retaining all descriptors.
        TArray<TSharedPtr<FJsonObject>> Rows;
        for(const auto& D:Registry->GetAllDroneDescriptors()){
            if(D.bIsEnemyTarget)continue;
            auto Row=D.DroneId==Id?MakeShared<FJsonObject>(*Packet):MakeShared<FJsonObject>();
            Row->SetNumberField(TEXT("id"),D.DroneId);Row->SetStringField(TEXT("id_str"),D.BackendIdString);
            Row->SetStringField(TEXT("name"),D.Name);Row->SetNumberField(TEXT("slot"),D.Slot);Row->SetStringField(TEXT("ip"),D.IpAddress);
            Row->SetNumberField(TEXT("port"),D.ControlPort);Row->SetNumberField(TEXT("mavlink_system_id"),D.MavlinkSystemId);
            Row->SetNumberField(TEXT("bit_index"),D.BitIndex);Row->SetNumberField(TEXT("ue_receive_port"),D.UEReceivePort);
            Row->SetStringField(TEXT("topic_prefix"),D.TopicPrefix);Row->SetStringField(TEXT("video_url"),D.VideoUrl);
            if(D.DroneId==Id)Row->SetStringField(TEXT("task_state"),TEXT("STANDBY"));Rows.Add(Row);
        }
        Network->SyncDroneListToRegistry(Rows);Registry->GetTelemetry(Id,Actual);
        TestEqual(TEXT("REST shares Mock source mapping"),Actual.TelemetrySource,FString(TEXT("mock_home")));
        TestEqual(TEXT("REST task hydration is not a Mock heartbeat"),Actual.LastUpdateTime,123.);
        TestEqual(TEXT("REST leaves hardware offline"),Actual.Availability,EDroneAvailability::Offline);

        const auto PlansAtMockRequest=Sync->GetPlans();const auto AtMockRequest=Network->WsTelemetryGeneration;
        const auto Previous=PlansAtMockRequest;auto Plans=Previous?MakeShared<FJsonObject>(*Previous):MakeShared<FJsonObject>();double Version=0,ExecutionVersion=0;
        if(Previous){Previous->TryGetNumberField(TEXT("version"),Version);Previous->TryGetNumberField(TEXT("execution_version"),ExecutionVersion);}
        for(const TCHAR* Key:{TEXT("plans"),TEXT("missions"),TEXT("paths"),TEXT("deployments")})if(!Plans->HasField(Key))Plans->SetObjectField(Key,MakeShared<FJsonObject>());
        Plans->RemoveField(TEXT("mock_position_baseline"));Plans->SetStringField(TEXT("execution_adapter"),TEXT("Mock"));
        Plans->SetNumberField(TEXT("version"),Version+1000);Plans->SetNumberField(TEXT("execution_version"),ExecutionVersion+1000);
        Plans->SetArrayField(TEXT("events"),{});auto Executions=MakeShared<FJsonObject>();auto Execution=MakeShared<FJsonObject>();
        Execution->SetStringField(TEXT("uav_id"),TEXT("UAV-9001"));Execution->SetStringField(TEXT("state"),TEXT("COMPLETED"));
        Execution->SetBoolField(TEXT("simulation"),true);Execution->SetNumberField(TEXT("created_at"),1);Execution->SetNumberField(TEXT("updated_at"),2);
        auto Position=MakeShared<FJsonObject>();Position->SetNumberField(TEXT("latitude"),39.99);Position->SetNumberField(TEXT("longitude"),116.35);Position->SetNumberField(TEXT("altitude"),50.);
        Execution->SetObjectField(TEXT("position"),Position);Executions->SetObjectField(TEXT("completed"),Execution);Plans->SetObjectField(TEXT("executions"),Executions);
        Sync->ApplyPlans(Plans);Registry->GetTelemetry(Id,Actual);
        TestEqual(TEXT("Acknowledged execution position applied"),Actual.GpsLatitude,39.99);
        TestEqual(TEXT("Execution projection remains offline"),Actual.Availability,EDroneAvailability::Offline);
        TestEqual(TEXT("Execution projection preserves heartbeat"),Actual.LastUpdateTime,123.);
        TestEqual(TEXT("Execution provenance explicit"),Actual.TelemetrySource,FString(TEXT("mock_execution")));
        Network->SyncDroneListToRegistry(Rows,&AtMockRequest,&PlansAtMockRequest);Registry->GetTelemetry(Id,Actual);
        TestEqual(TEXT("REST started before execution ACK cannot restore old Mock home"),Actual.GpsLatitude,39.99);
        TestEqual(TEXT("Rejected old Mock REST preserves execution source"),Actual.TelemetrySource,FString(TEXT("mock_execution")));
        TestEqual(TEXT("Rejected old Mock REST preserves heartbeat"),Actual.LastUpdateTime,123.);

        auto Scheduled=MakeShared<FJsonObject>(*Execution);Scheduled->SetStringField(TEXT("state"),TEXT("SCHEDULED"));
        Scheduled->SetNumberField(TEXT("created_at"),100);Scheduled->SetNumberField(TEXT("updated_at"),100);
        auto FuturePosition=MakeShared<FJsonObject>(*Position);FuturePosition->SetNumberField(TEXT("latitude"),42.);
        Scheduled->SetObjectField(TEXT("position"),FuturePosition);Executions->SetObjectField(TEXT("future"),Scheduled);
        Plans=MakeShared<FJsonObject>(*Plans);Plans->SetNumberField(TEXT("execution_version"),ExecutionVersion+1001);Sync->ApplyPlans(Plans);
        Registry->GetTelemetry(Id,Actual);TestEqual(TEXT("Future schedule cannot move the current marker"),Actual.GpsLatitude,39.99);
        auto MoreRecent=MakeShared<FJsonObject>(*Execution);MoreRecent->SetNumberField(TEXT("created_at"),0);MoreRecent->SetNumberField(TEXT("updated_at"),3);
        auto RecentPosition=MakeShared<FJsonObject>(*Position);RecentPosition->SetNumberField(TEXT("latitude"),40.02);
        MoreRecent->SetObjectField(TEXT("position"),RecentPosition);Executions->SetObjectField(TEXT("more-recent-update"),MoreRecent);
        Plans=MakeShared<FJsonObject>(*Plans);Plans->SetNumberField(TEXT("execution_version"),ExecutionVersion+1002);Sync->ApplyPlans(Plans);
        Registry->GetTelemetry(Id,Actual);TestEqual(TEXT("Last confirmed update wins over creation order"),Actual.GpsLatitude,40.02);

        TestEqual(TEXT("Without a startup baseline historical position still restores"),
            ExecutionUI::MockPosition(Sync->GetPlans(),TEXT("UAV-9001"))->GetNumberField(TEXT("latitude")),40.02);
        double NextExecutionVersion=ExecutionVersion+1002;
        auto ApplyNextPlans=[&]{Plans=MakeShared<FJsonObject>(*Plans);Plans->SetNumberField(TEXT("execution_version"),++NextExecutionVersion);Sync->ApplyPlans(Plans);};
        auto BaselinePosition=MakeShared<FJsonObject>(*Position);BaselinePosition->SetNumberField(TEXT("latitude"),39.97);
        auto Baseline=MakeShared<FJsonObject>();auto BaselinePositions=MakeShared<FJsonObject>();
        BaselinePositions->SetObjectField(TEXT("UAV-9001"),BaselinePosition);
        Baseline->SetNumberField(TEXT("epoch"),7);Baseline->SetNumberField(TEXT("reset_at"),200);
        Baseline->SetStringField(TEXT("request_id"),TEXT("mock-baseline-fixture"));Baseline->SetObjectField(TEXT("positions"),BaselinePositions);
        Plans=MakeShared<FJsonObject>(*Plans);Plans->SetObjectField(TEXT("mock_position_baseline"),Baseline);
        Executions=MakeShared<FJsonObject>(*Executions);
        auto OldTerminal=MakeShared<FJsonObject>(*MoreRecent);OldTerminal->SetNumberField(TEXT("mock_epoch"),6);
        OldTerminal->SetNumberField(TEXT("updated_at"),100000);Executions->SetObjectField(TEXT("old-terminal"),OldTerminal);
        auto CurrentScheduled=MakeShared<FJsonObject>(*Scheduled);CurrentScheduled->SetNumberField(TEXT("mock_epoch"),7);
        CurrentScheduled->SetNumberField(TEXT("updated_at"),200000);Executions->SetObjectField(TEXT("current-scheduled"),CurrentScheduled);
        auto CurrentReal=MakeShared<FJsonObject>(*OldTerminal);CurrentReal->SetNumberField(TEXT("mock_epoch"),7);
        CurrentReal->SetBoolField(TEXT("simulation"),false);CurrentReal->SetNumberField(TEXT("updated_at"),300000);
        Executions->SetObjectField(TEXT("real-record"),CurrentReal);
        auto OtherAircraft=MakeShared<FJsonObject>(*MoreRecent);OtherAircraft->SetStringField(TEXT("uav_id"),TEXT("UAV-9002"));
        Executions->SetObjectField(TEXT("not-reset"),OtherAircraft);Plans->SetObjectField(TEXT("executions"),Executions);ApplyNextPlans();
        Registry->GetTelemetry(Id,Actual);
        TestEqual(TEXT("Old terminal timestamp cannot resurrect a previous startup position"),Actual.GpsLatitude,39.97);
        TestEqual(TEXT("Reset provenance is explicit"),Actual.TelemetrySource,FString(TEXT("mock_reset")));
        TestTrue(TEXT("Reset remains simulated without claiming a local flight frame"),Actual.bSimulation && !Actual.bLocalPositionValid);
        TestEqual(TEXT("Reset does not fabricate hardware online"),Actual.Availability,EDroneAvailability::Offline);
        TestEqual(TEXT("Reset preserves the heartbeat"),Actual.LastUpdateTime,123.);
        TestTrue(TEXT("Reset retains old execution history"),PlanUI::Object(Sync->GetPlans(),TEXT("executions"))->HasField(TEXT("old-terminal")));
        TestEqual(TEXT("Only aircraft listed in the baseline change position rules"),
            ExecutionUI::MockPosition(Sync->GetPlans(),TEXT("UAV-9002"))->GetNumberField(TEXT("latitude")),40.02);
        Actual.GpsLatitude=42.;Actual.TelemetrySource=TEXT("mock_execution");Registry->UpdateTelemetry(Id,Actual);
        FVector AcceptedPosition;
        TestTrue(TEXT("Map reads the reset baseline despite an old cached simulated GPS fix"),
            MapMove::Position(Registry,Sync->GetPlans(),Id,AcceptedPosition) && FMath::IsNearlyEqual(AcceptedPosition.Y,39.97));
        TestTrue(TEXT("Move validation uses the same reset position while hardware remains offline"),
            MapMove::Position(Registry,Sync->GetPlans(),Id,AcceptedPosition,true) && FMath::IsNearlyEqual(AcceptedPosition.Y,39.97));

        auto CurrentExecution=MakeShared<FJsonObject>(*Execution);CurrentExecution->SetNumberField(TEXT("mock_epoch"),7);
        CurrentExecution->SetNumberField(TEXT("updated_at"),4);
        auto CurrentPosition=MakeShared<FJsonObject>(*Position);CurrentPosition->SetNumberField(TEXT("latitude"),40.03);
        CurrentExecution->SetObjectField(TEXT("position"),CurrentPosition);
        Executions=MakeShared<FJsonObject>(*Executions);Executions->SetObjectField(TEXT("current-epoch"),CurrentExecution);
        Plans=MakeShared<FJsonObject>(*Plans);Plans->SetObjectField(TEXT("executions"),Executions);ApplyNextPlans();
        Registry->GetTelemetry(Id,Actual);
        TestEqual(TEXT("Current epoch execution wins even with an older timestamp than old history"),Actual.GpsLatitude,40.03);
        TestEqual(TEXT("Current epoch execution retains execution provenance"),Actual.TelemetrySource,FString(TEXT("mock_execution")));
        TestTrue(TEXT("Map follows the current epoch execution"),
            MapMove::Position(Registry,Sync->GetPlans(),Id,AcceptedPosition) && FMath::IsNearlyEqual(AcceptedPosition.Y,40.03));

        // Plan and execution revisions can arrive in either order. A newer plan
        // response with old execution state must not discard the reset boundary.
        auto StaleExecutionState=MakeShared<FJsonObject>(*Plans);
        StaleExecutionState->SetNumberField(TEXT("version"),Version+1001);
        StaleExecutionState->SetNumberField(TEXT("execution_version"),ExecutionVersion+1000);
        StaleExecutionState->RemoveField(TEXT("mock_position_baseline"));Sync->ApplyPlans(StaleExecutionState);
        TestTrue(TEXT("New plan revision retains the newer execution baseline"),
            ExecutionUI::MockBaselinePosition(Sync->GetPlans(),TEXT("UAV-9001")).IsValid());
        Registry->GetTelemetry(Id,Actual);TestEqual(TEXT("Stale execution payload cannot restore old history"),Actual.GpsLatitude,40.03);
        auto NextBaseline=MakeShared<FJsonObject>(*Baseline);NextBaseline->SetNumberField(TEXT("epoch"),8);
        Plans=MakeShared<FJsonObject>(*Sync->GetPlans());Plans->SetNumberField(TEXT("version"),Version+1000);
        Plans->SetObjectField(TEXT("mock_position_baseline"),NextBaseline);ApplyNextPlans();
        Registry->GetTelemetry(Id,Actual);TestEqual(TEXT("New execution baseline merges with an older plan revision"),Actual.GpsLatitude,39.97);
        TestEqual(TEXT("Reset preserves the newer plan revision"),Sync->GetPlans()->GetNumberField(TEXT("version")),Version+1001);
        Plans=MakeShared<FJsonObject>(*Sync->GetPlans());

        const auto AtRequest=Network->WsTelemetryGeneration;
        auto OldReceived=MakeShared<FJsonObject>(*Packet);OldReceived->SetStringField(TEXT("telemetry_source"),TEXT("received"));OldReceived->SetBoolField(TEXT("simulation"),false);
        Packet->SetStringField(TEXT("telemetry_source"),TEXT("received"));Packet->SetBoolField(TEXT("simulation"),false);
        Packet->SetStringField(TEXT("status"),TEXT("online"));Packet->SetNumberField(TEXT("gps_lat"),40.1);Deliver();
        Registry->GetTelemetry(Id,Actual);const double ReceivedAt=Actual.LastUpdateTime;
        TestTrue(TEXT("Received source replaces Mock"),!Actual.bSimulation && Actual.TelemetrySource==TEXT("received") && Actual.GpsLatitude==40.1);
        for(auto& Row:Rows)if(Row->GetNumberField(TEXT("id"))==Id){OldReceived->Values.Append(Row->Values);OldReceived->SetStringField(TEXT("telemetry_source"),TEXT("received"));OldReceived->SetBoolField(TEXT("simulation"),false);Row=OldReceived;}
        Network->SyncDroneListToRegistry(Rows,&AtRequest);Registry->GetTelemetry(Id,Actual);
        TestEqual(TEXT("REST started before new WS cannot replace its GPS"),Actual.GpsLatitude,40.1);
        TestEqual(TEXT("Rejected old REST preserves received heartbeat"),Actual.LastUpdateTime,ReceivedAt);
        ApplyNextPlans();
        Registry->GetTelemetry(Id,Actual);TestEqual(TEXT("Plan replica cannot override received GPS"),Actual.GpsLatitude,40.1);
        TestEqual(TEXT("Plan replica retains received heartbeat"),Actual.LastUpdateTime,ReceivedAt);
        TestTrue(TEXT("Received GPS remains the map position even with an active reset baseline"),
            MapMove::Position(Registry,Sync->GetPlans(),Id,AcceptedPosition) && FMath::IsNearlyEqual(AcceptedPosition.Y,40.1));

        // A restarted backend has forgotten its received cache. It may downgrade
        // liveness using a home/unavailable packet, but not replace the real fix.
        Packet->SetStringField(TEXT("telemetry_source"),TEXT("mock_home"));Packet->SetBoolField(TEXT("simulation"),true);
        Packet->SetStringField(TEXT("status"),TEXT("offline"));Packet->SetNumberField(TEXT("gps_lat"),39.9807);Deliver();
        Registry->GetTelemetry(Id,Actual);
        TestEqual(TEXT("Backend restart downgrades received hardware to offline"),Actual.Availability,EDroneAvailability::Offline);
        TestTrue(TEXT("Backend restart retains received source and position"),Actual.TelemetrySource==TEXT("received") && !Actual.bSimulation && Actual.GpsLatitude==40.1);
        TestEqual(TEXT("Backend restart does not create a heartbeat"),Actual.LastUpdateTime,ReceivedAt);
        Packet->SetStringField(TEXT("status"),TEXT("online"));Deliver();Registry->GetTelemetry(Id,Actual);
        TestEqual(TEXT("Mock cannot re-upgrade retained received hardware"),Actual.Availability,EDroneAvailability::Offline);
        for(auto& Row:Rows)if(Row->GetNumberField(TEXT("id"))==Id){Row->SetStringField(TEXT("telemetry_source"),TEXT("unavailable"));Row->SetBoolField(TEXT("simulation"),false);Row->SetStringField(TEXT("status"),TEXT("lost"));}
        Network->SyncDroneListToRegistry(Rows);Registry->GetTelemetry(Id,Actual);
        TestEqual(TEXT("Unavailable REST can downgrade received hardware to lost"),Actual.Availability,EDroneAvailability::Lost);
        TestEqual(TEXT("Unavailable REST task hydration preserves received heartbeat"),Actual.LastUpdateTime,ReceivedAt);
        TestTrue(TEXT("Unavailable REST retains received provenance and location"),Actual.TelemetrySource==TEXT("received") && !Actual.bSimulation && Actual.GpsLatitude==40.1);
        Packet->SetStringField(TEXT("telemetry_source"),TEXT("received"));Packet->SetBoolField(TEXT("simulation"),false);Packet->SetNumberField(TEXT("gps_lat"),40.1);Deliver();
        Registry->GetTelemetry(Id,Actual);TestEqual(TEXT("New received packet restores online state"),Actual.Availability,EDroneAvailability::Online);
        Packet->SetStringField(TEXT("status"),TEXT("lost"));Packet->SetBoolField(TEXT("gps_fix"),false);Deliver();
        Packet->SetStringField(TEXT("telemetry_source"),TEXT("mock_home"));Packet->SetBoolField(TEXT("simulation"),true);Packet->SetBoolField(TEXT("gps_fix"),true);Packet->SetNumberField(TEXT("gps_lat"),39.9807);Deliver();
        for(auto& Row:Rows)if(Row->GetNumberField(TEXT("id"))==Id){Row->SetStringField(TEXT("telemetry_source"),TEXT("mock_home"));Row->SetBoolField(TEXT("simulation"),true);Row->SetStringField(TEXT("status"),TEXT("lost"));}
        Network->SyncDroneListToRegistry(Rows);Registry->GetTelemetry(Id,Actual);
        TestTrue(TEXT("Lost real source never falls back on WS or REST Mock"),Actual.TelemetrySource==TEXT("received") && !Actual.bSimulation && !Actual.bGpsFix && Actual.GpsLatitude==40.1 && Actual.Availability==EDroneAvailability::Lost);
        TestFalse(TEXT("Received GPS loss never falls back to the reset baseline"),MapMove::Position(Registry,Sync->GetPlans(),Id,AcceptedPosition));
        // Exercise the separate map refresh path: it must obey the same source
        // priority as REST/WS even when a received aircraft loses its GPS fix.
        auto* Coordinates=Registry->GetCoordinateService().GetObject();
        if(TestTrue(TEXT("Actor projection fixture has a ready coordinate system"),
            Coordinates && ICoordinateService::Execute_IsCoordinateSystemReady(Coordinates))){
            const FTransform SpawnTransform(FVector(12345.,67890.,4321.));
            auto* Mirror=World->SpawnActorDeferred<ARealTimeDroneReceiver>(ARealTimeDroneReceiver::StaticClass(),
                SpawnTransform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            if(TestNotNull(TEXT("Isolated telemetry actor created"),Mirror)){
                Mirror->DroneId=Id;Mirror->DroneName=TEXT("Telemetry actor fixture");Mirror->bUseWebSocket=true;
                Mirror->FinishSpawning(SpawnTransform);Mirror->SetActorTickEnabled(false);
                auto ActorPlans=MakeShared<FJsonObject>(*Sync->GetPlans());
                ActorPlans->SetNumberField(TEXT("execution_version"),++NextExecutionVersion);
                ActorPlans->RemoveField(TEXT("mock_position_baseline"));
                ActorPlans->SetStringField(TEXT("execution_adapter"),TEXT("Mock"));
                ActorPlans->SetObjectField(TEXT("executions"),MakeShared<FJsonObject>());
                const TSharedPtr<FJsonObject>* ExistingConfig=nullptr;
                ActorPlans->TryGetObjectField(TEXT("mock_execution"),ExistingConfig);
                auto Config=ExistingConfig && ExistingConfig->IsValid()?MakeShared<FJsonObject>(**ExistingConfig):MakeShared<FJsonObject>();
                auto Uavs=MakeShared<FJsonObject>();auto Uav=MakeShared<FJsonObject>();
                Uav->SetObjectField(TEXT("home_position"),Position);Uavs->SetObjectField(TEXT("UAV-9001"),Uav);
                Config->SetObjectField(TEXT("uavs"),Uavs);ActorPlans->SetObjectField(TEXT("mock_execution"),Config);
                Sync->ApplyPlans(ActorPlans);
                auto* Monitor=CreateWidget<UMapExecutionWidget>(World->GetFirstPlayerController());
                if(TestNotNull(TEXT("Map execution presentation created"),Monitor)){
                    Actual.bGpsFix=true;Actual.Availability=EDroneAvailability::Online;
                    Registry->UpdateTelemetry(Id,Actual);Mirror->SetActorLocation(SpawnTransform.GetLocation());
                    Monitor->Refresh();
                    TestTrue(TEXT("Map refresh preserves received actor with GPS"),
                        Mirror->GetActorLocation().Equals(SpawnTransform.GetLocation(),.01));
                    Actual.bGpsFix=false;Actual.Availability=EDroneAvailability::Lost;
                    Registry->UpdateTelemetry(Id,Actual);Mirror->SetActorLocation(SpawnTransform.GetLocation());
                    Monitor->Refresh();
                    TestTrue(TEXT("Map refresh never substitutes Mock after received GPS loss"),
                        Mirror->GetActorLocation().Equals(SpawnTransform.GetLocation(),.01));
                    Actual.TelemetrySource=TEXT("mock_home");Actual.bSimulation=true;
                    Actual.Availability=EDroneAvailability::Offline;Registry->UpdateTelemetry(Id,Actual);
                    Monitor->Refresh();
                    const FVector Home=ICoordinateService::Execute_GeographicToWorld(Coordinates,
                        Position->GetNumberField(TEXT("latitude")),Position->GetNumberField(TEXT("longitude")),
                        Position->GetNumberField(TEXT("altitude")));
                    TestTrue(TEXT("Pure Mock actor still initializes from configured home"),
                        Mirror->GetActorLocation().Equals(Home,.01));
                    Registry->GetTelemetry(Id,Actual);
                    TestEqual(TEXT("Map actor initialization leaves Mock hardware offline"),Actual.Availability,EDroneAvailability::Offline);
                }
                Mirror->Destroy();
            }
        }
        Registry->UnregisterDrone(Id);Network->SetStrictLocalPreviewIsolation(WasIsolated);
        AddInfo(TEXT("Synthetic component projection checks; no native mouse or real aircraft evidence."));
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
