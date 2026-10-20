#pragma once
#include "Shared/ExecutionPresentation.h"
#include "Shared/PlanConfiguration.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"

/** Both map clients use accepted positions and the same authoritative execution action. */
namespace MapMove {
inline FString UAV(int32 Id){return FString::Printf(TEXT("UAV-%02d"),Id);}
inline TArray<int32> Selection(const UDroneRegistrySubsystem* Registry){
    auto Members=Registry->GetMultiSelectedDrones();Members.Sort();
    const int32 Primary=Registry->GetPrimarySelectedDrone();
    if(Members.Remove(Primary))Members.Insert(Primary,0);return Members;
}
inline bool MockFallbackAllowed(const TSharedPtr<FJsonObject>& State,int32 Id){
    const auto Owner=ExecutionUI::PositionExecution(State,UAV(Id));
    bool Simulation=false;return !Owner || (Owner->TryGetBoolField(TEXT("simulation"),Simulation) && Simulation);
}
inline bool TelemetrySourceAllowed(const TSharedPtr<FJsonObject>& State,int32 Id,const FDroneTelemetrySnapshot& Telemetry){
    return !Telemetry.bSimulation || (PlanUI::Field(State,TEXT("execution_adapter"))==TEXT("Mock") && MockFallbackAllowed(State,Id));
}
inline bool Position(const UDroneRegistrySubsystem* Registry,const TSharedPtr<FJsonObject>& State,int32 Id,FVector& Geographic,bool ForControl=false){
    FDroneTelemetrySnapshot Telemetry;
    const bool Mock=PlanUI::Field(State,TEXT("execution_adapter"))==TEXT("Mock");
    const bool AllowedMock=Mock && MockFallbackAllowed(State,Id);
    const bool HasTelemetry=Registry->GetTelemetry(Id,Telemetry);
    // A cached simulated GPS fix can belong to the previous startup epoch.
    // With a reset baseline, use the acknowledged Plans position instead.
    const bool UseBaseline=AllowedMock && ExecutionUI::MockBaselinePosition(State,UAV(Id)).IsValid() &&
        (!HasTelemetry || Telemetry.TelemetrySource!=TEXT("received"));
    if(HasTelemetry && !UseBaseline && Telemetry.bGpsFix && TelemetrySourceAllowed(State,Id,Telemetry) &&
        (!ForControl || (Mock && Telemetry.bSimulation) || Telemetry.Availability==EDroneAvailability::Online)){
        Geographic=FVector(Telemetry.GpsLongitude,Telemetry.GpsLatitude,Telemetry.GpsAltitude);
        if(!Geographic.ContainsNaN() && FMath::Abs(Geographic.X)<=180 && FMath::Abs(Geographic.Y)<=85.05112878)return true;
    }
    if(!AllowedMock || (HasTelemetry && Telemetry.TelemetrySource==TEXT("received")))return false;
    const auto P=ExecutionUI::MockPosition(State,UAV(Id));double Lat=0,Lon=0,Alt=0;
    if(!P || !P->TryGetNumberField(TEXT("latitude"),Lat) || !P->TryGetNumberField(TEXT("longitude"),Lon) || !P->TryGetNumberField(TEXT("altitude"),Alt))return false;
    Geographic=FVector(Lon,Lat,Alt);
    return !Geographic.ContainsNaN() && FMath::Abs(Lon)<=180 && FMath::Abs(Lat)<=85.05112878;
}
inline FText Unavailable(const UDroneRegistrySubsystem* Registry,UOperationalContextSubsystem* Sync,const TArray<int32>& Members){
    using namespace PlanUI;
    if(PlanConfiguration::Active(Sync->GetPlans()))return T(TEXT("Configuration.MoveBlocked"));
    if(!Sync->IsReady() || !Sync->IsHydrated())return T(TEXT("Errors.SYNC_OFFLINE"));
    if(Members.IsEmpty())return T(TEXT("MapMove.SelectAircraft"));
    for(int32 Id:Members){FDroneDescriptor Descriptor;
        if(!Registry->GetDroneDescriptor(Id,Descriptor) || Descriptor.bIsEnemyTarget)return T(TEXT("Errors.INVALID_UAV"));
        if(const auto Owner=UAVReservation(Sync->GetPlans(),TEXT(""),UAV(Id)))
            return FText::Format(T(TEXT("Fleet.Occupied")),User(UAV(Id)),Field(Owner,TEXT("control_kind"))==TEXT("GROUP_MOVE")?T(TEXT("MapMove.Title")):User(Field(Owner,TEXT("plan_name"))));
        FVector P;if(!Position(Registry,Sync->GetPlans(),Id,P,true))return FText::Format(T(TEXT("Fleet.TelemetryRequired")),User(UAV(Id)));
    }return FText::GetEmpty();
}
inline TSharedRef<FJsonObject> Request(const TArray<int32>& Members,const FVector& Target){
    auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("request_id"),FGuid::NewGuid().ToString());
    R->SetStringField(TEXT("action"),TEXT("execution_group_move"));
    TArray<TSharedPtr<FJsonValue>> Ids;for(int32 Id:Members)Ids.Add(MakeShared<FJsonValueString>(UAV(Id)));R->SetArrayField(TEXT("assigned_uav_ids"),Ids);
    auto P=MakeShared<FJsonObject>();P->SetNumberField(TEXT("longitude"),Target.X);P->SetNumberField(TEXT("latitude"),Target.Y);P->SetNumberField(TEXT("altitude"),Target.Z);R->SetObjectField(TEXT("target"),P);
    R->SetNumberField(TEXT("target_area_radius_m"),10000.);return R;
}
}
