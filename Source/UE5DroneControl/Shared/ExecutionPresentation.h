#pragma once
#include "Shared/PlanWidgetSupport.h"
namespace ExecutionUI {
inline bool Active(const TSharedPtr<FJsonObject>& E) {
    const auto S=PlanUI::Field(E,TEXT("state"));
    return E && S!=TEXT("COMPLETED") && S!=TEXT("ABORTED") && S!=TEXT("FAILED");
}
inline TSharedPtr<FJsonObject> Latest(const TSharedPtr<FJsonObject>& State,const FString& Plan=TEXT(""),bool OnlyActive=false) {
    const auto All=PlanUI::Object(State,TEXT("executions"));TSharedPtr<FJsonObject> Result;double Time=-1;
    if(All)for(const auto& Entry:All->Values){const auto E=Entry.Value->AsObject();
        if((Plan.IsEmpty() || PlanUI::Field(E,TEXT("plan_id"))==Plan) && (!OnlyActive || Active(E)) && E->GetNumberField(TEXT("created_at"))>=Time){Result=E;Time=E->GetNumberField(TEXT("created_at"));}}
    return Result;
}
inline FText Summary(const TSharedPtr<FJsonObject>& E) {
    if(!E)return ProductText::Get(TEXT("Execution.None"));
    return FText::Format(ProductText::Get(TEXT("Execution.Summary")),PlanUI::User(PlanUI::Field(E,TEXT("plan_name"))),PlanUI::User(PlanUI::Field(E,TEXT("mission_name"))),
        PlanUI::User(PlanUI::Field(E,TEXT("uav_id"))),ProductText::Get(TEXT("Execution.")+PlanUI::Field(E,TEXT("state"))),
        FText::AsNumber(E->GetNumberField(TEXT("current_waypoint"))),FText::AsNumber(E->GetNumberField(TEXT("total_waypoints"))),FText::AsPercent(E->GetNumberField(TEXT("progress"))));
}
}
