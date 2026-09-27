#pragma once
#include "Shared/PlanWidgetSupport.h"
namespace ExecutionUI {
inline bool Active(const TSharedPtr<FJsonObject>& E) {
    const auto S=PlanUI::Field(E,TEXT("state"));
    return E && S!=TEXT("COMPLETED") && S!=TEXT("ABORTED") && S!=TEXT("FAILED") && S!=TEXT("CANCELLED");
}
inline TSharedPtr<FJsonObject> Latest(const TSharedPtr<FJsonObject>& State,const FString& Plan=TEXT(""),bool OnlyActive=false) {
    const auto All=PlanUI::Object(State,TEXT("executions"));TSharedPtr<FJsonObject> Result;double Time=-1;
    if(All)for(const auto& Entry:All->Values){const auto E=Entry.Value->AsObject();
        if((Plan.IsEmpty() || PlanUI::Field(E,TEXT("plan_id"))==Plan) && (!OnlyActive || Active(E)) && E->GetNumberField(TEXT("created_at"))>=Time){Result=E;Time=E->GetNumberField(TEXT("created_at"));}}
    return Result;
}
// One latest execution group owns each Mission; retain its completed snapshots.
// A multi-UAV group is one presentation set with one immutable trajectory per UAV.
inline TArray<TSharedPtr<FJsonObject>> RouteOwners(const TSharedPtr<FJsonObject>& State,const FString& SelectedPlan){
    TArray<TSharedPtr<FJsonObject>> Result;const auto All=PlanUI::Object(State,TEXT("executions"));if(!All)return Result;
    TMap<FString,TSharedPtr<FJsonObject>> LatestByMission;
    auto Key=[](const TSharedPtr<FJsonObject>& E){const FString P=PlanUI::Field(E,TEXT("plan_id")),M=PlanUI::Field(E,TEXT("mission_id"));return P.IsEmpty() || M.IsEmpty()?PlanUI::Field(E,TEXT("execution_id")):P+TEXT("/")+M;};
    for(const auto& Item:All->Values){const auto E=Item.Value->AsObject();if(!E)continue;
        const FString P=PlanUI::Field(E,TEXT("plan_id"));if(!Active(E) && P!=SelectedPlan)continue;
        const auto Plan=PlanUI::Find(State,TEXT("plans"),P);
        if(!Active(E) && Plan && PlanUI::Field(Plan,TEXT("deployment_id"))!=PlanUI::Field(E,TEXT("deployment_id")))continue;
        const FString K=Key(E);auto& Old=LatestByMission.FindOrAdd(K);
        if(!Old || (Active(E) && !Active(Old)) || (Active(E)==Active(Old) && E->GetNumberField(TEXT("created_at"))>Old->GetNumberField(TEXT("created_at"))))Old=E;
    }
    for(const auto& Item:All->Values){const auto E=Item.Value->AsObject();if(!E)continue;const auto* Latest=LatestByMission.Find(Key(E));if(!Latest)continue;
        const FString Group=PlanUI::Field(*Latest,TEXT("group_id"));
        if(E==*Latest || (!Group.IsEmpty() && PlanUI::Field(E,TEXT("group_id"))==Group))Result.Add(E);
    }return Result;
}
inline FText Summary(const TSharedPtr<FJsonObject>& E) {
    if(!E)return ProductText::Get(TEXT("Execution.None"));
    return FText::Format(ProductText::Get(TEXT("Execution.Summary")),PlanUI::User(PlanUI::Field(E,TEXT("plan_name"))),PlanUI::User(PlanUI::Field(E,TEXT("mission_name"))),
        PlanUI::User(PlanUI::Field(E,TEXT("uav_id"))),ProductText::Get(TEXT("Execution.")+PlanUI::Field(E,TEXT("state"))),
        FText::AsNumber(E->GetNumberField(TEXT("current_waypoint"))),FText::AsNumber(E->GetNumberField(TEXT("total_waypoints"))),FText::AsPercent(E->GetNumberField(TEXT("progress"))));
}
}
