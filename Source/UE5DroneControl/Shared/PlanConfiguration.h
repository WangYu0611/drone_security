#pragma once
#include "Shared/PlanWidgetSupport.h"

// Configuration is an acknowledged Backend lease, never a widget visibility flag.
namespace PlanConfiguration {
inline TSharedPtr<FJsonObject> Session(const TSharedPtr<FJsonObject>& State) {
    const auto Lease=PlanUI::Object(State,TEXT("configuration_session"));double Expires=0;
    if(!Lease || PlanUI::Field(Lease,TEXT("owner_instance_id")).IsEmpty()
        || !Lease->TryGetNumberField(TEXT("lease_expires_at"),Expires) || !FMath::IsFinite(Expires)
        || Expires<=double(FDateTime::UtcNow().ToUnixTimestamp()))return nullptr;
    return Lease;
}
inline bool Active(const TSharedPtr<FJsonObject>& State) { return Session(State).IsValid(); }
inline bool IsOwner(const TSharedPtr<FJsonObject>& State,const FString& InstanceId) {
    const auto Lease=Session(State);return Lease && PlanUI::Field(Lease,TEXT("owner_instance_id"))==InstanceId;
}
inline FString PlanId(const TSharedPtr<FJsonObject>& State) { return PlanUI::Field(Session(State),TEXT("plan_id")); }
inline FText PlanName(const TSharedPtr<FJsonObject>& State) {
    const auto Id=PlanId(State);const auto Name=PlanUI::Field(PlanUI::Find(State,TEXT("plans"),Id),TEXT("name"));
    return Name.IsEmpty()?PlanUI::T(TEXT("Configuration.NewPlan")):PlanUI::User(Name);
}
}
