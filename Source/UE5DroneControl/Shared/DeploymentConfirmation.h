#pragma once
#include "Shared/PlanConfiguration.h"

// One explicit confirmation owns this identity across validate -> review -> deploy.
// Heartbeats may advance the document, but never replace the confirmed content
// or the exact configuration lease. Plan versions come only from stage ACKs.
namespace DeploymentConfirmation {
inline bool Version(const TSharedPtr<FJsonObject>& Object,const TCHAR* Key,double& Value) {
    return Object && Object->TryGetNumberField(Key,Value) && FMath::IsFinite(Value)
        && Value>=0 && Value<=9007199254740991. && Value==FMath::FloorToDouble(Value);
}
struct FIntent {
    FString PlanId,OwnerInstance;
    double ContentRevision=-1,AcquiredVersion=-1;
    bool Matches(const TSharedPtr<FJsonObject>& State) const {
        const auto Lease=PlanConfiguration::Session(State),Plan=PlanUI::Find(State,TEXT("plans"),PlanId);
        double Content=-1,Acquired=-1;
        return !PlanId.IsEmpty() && !OwnerInstance.IsEmpty() && Plan && Lease
            && PlanUI::Field(Lease,TEXT("plan_id"))==PlanId && PlanUI::Field(Lease,TEXT("owner_instance_id"))==OwnerInstance
            && Version(Plan,TEXT("content_revision"),Content) && Content==ContentRevision
            && Version(Lease,TEXT("acquired_version"),Acquired) && Acquired==AcquiredVersion;
    }
    TSharedPtr<FJsonObject> Request(const TCHAR* Action,const TSharedPtr<FJsonObject>& Acknowledged) const {
        const FString Stage(Action);double DocumentVersion=-1,PlanVersion=-1;
        if((Stage!=TEXT("validate") && Stage!=TEXT("review") && Stage!=TEXT("deploy")) || !Matches(Acknowledged)
            || !Version(Acknowledged,TEXT("version"),DocumentVersion)
            || !Version(PlanUI::Find(Acknowledged,TEXT("plans"),PlanId),TEXT("version"),PlanVersion))return nullptr;
        auto Body=MakeShared<FJsonObject>();Body->SetStringField(TEXT("action"),Stage);Body->SetStringField(TEXT("plan_id"),PlanId);
        Body->SetNumberField(TEXT("expected_version"),DocumentVersion);Body->SetNumberField(TEXT("expected_plan_version"),PlanVersion);
        Body->SetNumberField(TEXT("expected_content_revision"),ContentRevision);Body->SetNumberField(TEXT("configuration_acquired_version"),AcquiredVersion);
        return Body;
    }
};
inline FIntent Capture(const TSharedPtr<FJsonObject>& State,const FString& PlanId,const FString& OwnerInstance) {
    FIntent Intent;Intent.PlanId=PlanId;Intent.OwnerInstance=OwnerInstance;
    Version(PlanUI::Find(State,TEXT("plans"),PlanId),TEXT("content_revision"),Intent.ContentRevision);
    Version(PlanConfiguration::Session(State),TEXT("acquired_version"),Intent.AcquiredVersion);
    return Intent;
}
}
