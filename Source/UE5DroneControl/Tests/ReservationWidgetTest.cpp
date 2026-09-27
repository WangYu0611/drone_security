#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Engine/Engine.h"
#include "Shared/SecurityPlanWorkspaceWidget.h"
#include "Shared/ExecutionPresentation.h"
#include "Components/CheckBox.h"
using namespace PlanUI;
class FP55ReservationWidgets:public IAutomationLatentCommand {
    FAutomationTestBase* Test;double Started=FPlatformTime::Seconds();int Step=0;FString Plan,Mission;
public:explicit FP55ReservationWidgets(FAutomationTestBase* T):Test(T){}
    bool Update() override {
        if(FPlatformTime::Seconds()-Started>60){Test->AddError(TEXT("Reservation UI hydration timeout"));return true;}
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();if(!W)return false;
        auto* S=W->GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();if(!S->IsReady())return false;
        USecurityPlanWorkspaceWidget* P=nullptr;for(TObjectIterator<USecurityPlanWorkspaceWidget> I;I;++I)if(I->GetWorld()==W){P=*I;break;}if(!P || P->bPending)return false;
        if(Step==0){for(const auto& I:Object(S->GetPlans(),TEXT("plans"))->Values)if(Field(I.Value->AsObject(),TEXT("name"))==TEXT("P55 FIX Route V2 Lifecycle")){Plan=I.Key;Mission=I.Value->AsObject()->GetArrayField(TEXT("mission_ids"))[0]->AsString();}
            if(Plan.IsEmpty()){Test->AddError(TEXT("Reservation UI fixture missing"));return true;}
            auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("action"),TEXT("select"));R->SetStringField(TEXT("plan_id"),Plan);R->SetStringField(TEXT("mission_id"),Mission);P->Submit(R);++Step;return false;}
        P->Refresh();if(P->PlanId!=Plan)return false;
        // Explicit client replica fixture tests presentation only; Backend authority is
        // separately exercised by transaction concurrency/restart and HTTP/native tests.
        auto Fixture=MakeShared<FJsonObject>(*S->GetPlans());auto All=MakeShared<FJsonObject>(*Object(Fixture,TEXT("executions")));
        auto E=MakeShared<FJsonObject>(*ExecutionUI::Latest(Fixture,TEXT(""),true));E->SetStringField(TEXT("execution_id"),TEXT("ui-reservation-fixture"));E->SetStringField(TEXT("uav_id"),TEXT("UAV-01"));E->SetStringField(TEXT("state"),TEXT("EXECUTING"));
        All->SetObjectField(TEXT("ui-reservation-fixture"),E);Fixture->SetObjectField(TEXT("executions"),All);Fixture->SetNumberField(TEXT("execution_version"),Fixture->GetNumberField(TEXT("execution_version"))+10000);S->ApplyPlans(Fixture);P->Refresh();
        auto* Check=P->AssignmentChecks.FindRef(TEXT("UAV-01")).Get();if(!Test->TestNotNull(TEXT("unavailable UAV remains in list"),Check))return true;
        Test->TestFalse(TEXT("executing UAV disabled"),Check->GetIsEnabled());Test->TestTrue(TEXT("old draft selection retained"),Check->IsChecked());
        const auto* Caption=Cast<UTextBlock>(Check->GetContent());Test->TestTrue(TEXT("occupying plan named"),Caption && Caption->GetText().ToString().Contains(Field(E,TEXT("plan_name"))));
        Test->TestTrue(TEXT("another UAV remains selectable"),P->AssignmentChecks.FindRef(TEXT("UAV-02"))->GetIsEnabled());
        FString Before;FJsonSerializer::Serialize(S->GetPlans().ToSharedRef(),TJsonWriterFactory<>::Create(&Before));
        P->Action(TEXT("clear_unavailable"),0);Test->TestFalse(TEXT("explicit clear removes unavailable local selection"),Check->IsChecked());
        FString After;FJsonSerializer::Serialize(S->GetPlans().ToSharedRef(),TJsonWriterFactory<>::Create(&After));Test->TestEqual(TEXT("clear requires Save and never mutates accepted assignment"),After,Before);
        auto Completed=MakeShared<FJsonObject>(*E);Completed->SetStringField(TEXT("state"),TEXT("COMPLETED"));All=MakeShared<FJsonObject>(*All);All->SetObjectField(TEXT("ui-reservation-fixture"),Completed);Fixture=MakeShared<FJsonObject>(*Fixture);Fixture->SetObjectField(TEXT("executions"),All);Fixture->SetNumberField(TEXT("execution_version"),Fixture->GetNumberField(TEXT("execution_version"))+1);S->ApplyPlans(Fixture);P->Refresh();
        Test->TestTrue(TEXT("completion immediately enables UAV without reopening"),Check->GetIsEnabled());return true;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP55ReservationWidgetTest,"DroneOps.P55.ReservationWidgets",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FP55ReservationWidgetTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Tests/P55OfflineMap")));ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(8));ADD_LATENT_AUTOMATION_COMMAND(FP55ReservationWidgets(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
