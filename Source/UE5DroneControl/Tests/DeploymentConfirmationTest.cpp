#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Shared/DeploymentConfirmation.h"

namespace {
TSharedPtr<FJsonObject> DeploymentFixture(double Document,double PlanVersion,double Content=7,double Acquired=10,const FString& Owner=TEXT("command-a")) {
    auto State=MakeShared<FJsonObject>(),Plans=MakeShared<FJsonObject>(),Plan=MakeShared<FJsonObject>(),Lease=MakeShared<FJsonObject>();
    State->SetNumberField(TEXT("version"),Document);State->SetObjectField(TEXT("plans"),Plans);Plans->SetObjectField(TEXT("confirmed-plan"),Plan);
    Plan->SetNumberField(TEXT("version"),PlanVersion);Plan->SetNumberField(TEXT("content_revision"),Content);
    Lease->SetStringField(TEXT("plan_id"),TEXT("confirmed-plan"));Lease->SetStringField(TEXT("owner_instance_id"),Owner);
    Lease->SetNumberField(TEXT("acquired_version"),Acquired);Lease->SetNumberField(TEXT("lease_expires_at"),FDateTime::UtcNow().ToUnixTimestamp()+120);
    State->SetObjectField(TEXT("configuration_session"),Lease);return State;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeploymentConfirmationTest,"DroneOps.PlanConfiguration.DeploymentIntent",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDeploymentConfirmationTest::RunTest(const FString&) {
    const auto Initial=DeploymentFixture(12,4),Validated=DeploymentFixture(13,5),Reviewed=DeploymentFixture(15,6);
    const auto Intent=DeploymentConfirmation::Capture(Initial,TEXT("confirmed-plan"),TEXT("command-a"));
    TestTrue(TEXT("Initial confirmation captures the acknowledged identity"),Intent.Matches(Initial));
    const auto Validate=Intent.Request(TEXT("validate"),Initial),Review=Intent.Request(TEXT("review"),Validated),Deploy=Intent.Request(TEXT("deploy"),Reviewed);
    if(!TestTrue(TEXT("All three stages have guarded requests"),Validate && Review && Deploy))return false;
    TestEqual(TEXT("Validate uses the initially confirmed plan version"),Validate->GetNumberField(TEXT("expected_plan_version")),4.);
    TestEqual(TEXT("Review uses the validation ACK plan version"),Review->GetNumberField(TEXT("expected_plan_version")),5.);
    TestEqual(TEXT("Deploy uses the review ACK plan version"),Deploy->GetNumberField(TEXT("expected_plan_version")),6.);
    for(const auto& Request:{Validate,Review,Deploy}){
        TestEqual(TEXT("Every stage retains confirmed plan ID"),PlanUI::Field(Request,TEXT("plan_id")),FString(TEXT("confirmed-plan")));
        TestEqual(TEXT("Every stage retains confirmed content"),Request->GetNumberField(TEXT("expected_content_revision")),7.);
        TestEqual(TEXT("Every stage retains acquired lease token"),Request->GetNumberField(TEXT("configuration_acquired_version")),10.);
    }
    const auto Heartbeat=DeploymentFixture(16,6);
    TestTrue(TEXT("Heartbeat with unchanged content and lease remains the same confirmation"),Intent.Matches(Heartbeat));
    TestEqual(TEXT("Later heartbeat never replaces review ACK document version"),Deploy->GetNumberField(TEXT("expected_version")),15.);
    TestEqual(TEXT("Later heartbeat never replaces review ACK plan version"),Deploy->GetNumberField(TEXT("expected_plan_version")),6.);
    TestFalse(TEXT("New content invalidates a frozen confirmation"),Intent.Matches(DeploymentFixture(16,7,8)));
    TestFalse(TEXT("Release/reacquire by the same owner invalidates its old token"),Intent.Matches(DeploymentFixture(16,6,7,16)));
    TestFalse(TEXT("Another owner invalidates the confirmation"),Intent.Matches(DeploymentFixture(16,6,7,10,TEXT("command-b"))));
    TestFalse(TEXT("An ACK for changed content cannot supply a later stage"),Intent.Request(TEXT("deploy"),DeploymentFixture(16,7,8)).IsValid());
    TestFalse(TEXT("An ACK for a new lease cannot supply a later stage"),Intent.Request(TEXT("review"),DeploymentFixture(16,6,7,16)).IsValid());
    auto OtherPlan=DeploymentFixture(16,6);PlanUI::Object(OtherPlan,TEXT("configuration_session"))->SetStringField(TEXT("plan_id"),TEXT("other-plan"));
    TestFalse(TEXT("Lease plan drift invalidates confirmation"),Intent.Matches(OtherPlan));
    auto Expired=DeploymentFixture(16,6);PlanUI::Object(Expired,TEXT("configuration_session"))->SetNumberField(TEXT("lease_expires_at"),0);
    TestFalse(TEXT("Expired lease cannot continue confirmation"),Intent.Matches(Expired));
    auto Malformed=DeploymentFixture(16,6);PlanUI::Find(Malformed,TEXT("plans"),TEXT("confirmed-plan"))->SetNumberField(TEXT("version"),6.5);
    TestFalse(TEXT("Malformed ACK version never reaches Backend"),Intent.Request(TEXT("deploy"),Malformed).IsValid());
    TestFalse(TEXT("Guard helper cannot accidentally loosen route-save requests"),Intent.Request(TEXT("save_route"),Reviewed).IsValid());
    return true;
}
#endif
