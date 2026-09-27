#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Map/MapShellWidget.h"
#include "Map/MapMissionRouteWidget.h"
#include "Map/MapPlanMoveWidget.h"
#include "Map/MapExecutionWidget.h"
#include "Shared/ExecutionPresentation.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "DroneOps/Core/ICoordinateService.h"
#include "Command/CommandScreenManager.h"
#include "Command/CommandMapInteractionService.h"
#include "Shared/PlanWidgetSupport.h"
#include "Components/SplineMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
using namespace PlanUI;
class FP55SecurityPlanVisual:public IAutomationLatentCommand {
    FAutomationTestBase* Test;int Step=0,HydratedOthers=0;double Started=FPlatformTime::Seconds();FString Plan,Mission,Before;
    FVector Original,Delta=FVector(2500,3500,0);FLinearColor Color;float Rate=0;TWeakObjectPtr<ADronePathActor> Preview;
public:explicit FP55SecurityPlanVisual(FAutomationTestBase* T):Test(T){}
    bool Update() override {
        if(FPlatformTime::Seconds()-Started>100){Test->AddError(TEXT("Security Plan visual lifecycle timeout"));return true;}
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();if(!W)return false;
        auto* PC=Cast<ADroneOpsPlayerController>(W->GetFirstPlayerController());auto* S=W->GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
        auto* Shell=PC && PC->GetCommandScreenManager()?PC->GetCommandScreenManager()->GetMapShell():nullptr;if(!Shell || !S->IsReady())return false;
        auto* P=Shell->PlanPanel.Get();auto* M=Shell->MovePanel.Get();if(!P || !M || P->bPending || M->bPending)return false;
        auto* C=W->GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetCoordinateService().GetObject();if(!C || !ICoordinateService::Execute_IsCoordinateSystemReady(C))return false;
        Shell->Refresh();
        auto Route=[&](){return Find(S->GetPlans(),TEXT("paths"),Field(Find(S->GetPlans(),TEXT("missions"),Mission),TEXT("route_id")));};
        auto Move=[&](){auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("mode"),TEXT("MOVE"));R->SetStringField(TEXT("plan_id"),Plan);R->SetStringField(TEXT("mission_id"),Mission);M->Requested(R);};
        auto Visible=[&](){int Count=0;for(TActorIterator<ADronePathActor> I(W);I;++I)if(!I->IsHidden() && !I->GetSegmentVisuals().IsEmpty())++Count;return Count;};
        if(Step==0){for(const auto& I:Object(S->GetPlans(),TEXT("plans"))->Values)if(Field(I.Value->AsObject(),TEXT("name"))==TEXT("P55 FIX Route V2 Lifecycle")){Plan=I.Key;Mission=I.Value->AsObject()->GetArrayField(TEXT("mission_ids"))[0]->AsString();}
            if(Plan.IsEmpty()){Test->AddError(TEXT("Three Mission Backend fixture missing"));return true;}
            auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("action"),TEXT("select"));R->SetStringField(TEXT("plan_id"),Plan);R->SetStringField(TEXT("mission_id"),Mission);S->SubmitPlan(R,[](TSharedPtr<FJsonObject>){});++Step;return false;}
        if(Step==1){if(P->PlanId!=Plan)return false;Test->TestEqual(TEXT("all saved Missions have V2 owners"),Shell->SavedRouteVisuals.Num(),3);
            auto* A=Shell->SavedRouteVisuals.Find(Plan+TEXT("/")+Mission);if(!Test->TestNotNull(TEXT("saved selected owner"),A))return true;
            Test->TestEqual(TEXT("closed route has five V2 segments"),A->GetSegmentVisuals().Num(),5);Test->TestTrue(TEXT("background presentations have no waypoint handles"),A->GetWaypointHandleActors().IsEmpty());
            Test->TestTrue(TEXT("selected route emphasized"),A->IsVisualSelected());Test->TestTrue(TEXT("cached editor is hidden"),PC->GetMissionRouteVisual()->IsHidden());
            const auto Data=FPlanRouteVisualSet::Decode(Route(),C);Test->TestTrue(TEXT("same geographic decoder as editor"),A->GetWaypointWorldLocation(1).Equals(Data.Waypoints[1].Location,.01));
            P->SetEditorEnabled(true);P->Begin();++Step;return false;}
        if(Step==2){if(!Test->TestTrue(TEXT("real edit lease acquired"),!P->SessionId.IsEmpty()))return true;
            Test->TestEqual(TEXT("editing replaces exactly one saved owner"),Shell->SavedRouteVisuals.Num(),2);Test->TestFalse(TEXT("editing owner visible"),PC->GetMissionRouteVisual()->IsHidden());
            auto* A=PC->GetMissionRouteVisual();A->UpdateWaypointSegmentSpeed(2,13);P->Save();++Step;return false;}
        if(Step==3){Test->TestTrue(TEXT("save acknowledged and camera stays locked"),P->ErrorCode.IsEmpty() && PC->GetCommandScreenManager()->GetMapService()->IsRouteEditCameraLocked());P->Finish();++Step;return false;}
        // Fixture routes begin as geographic-only API data. Save each through the real
        // Map editor so all three carry canonical CoordinateService world coordinates,
        // as required by the unchanged Backend rigid-translation validator.
        if(Step==10){P->Save();Step=11;return false;}
        if(Step==11){Test->TestTrue(TEXT("other Mission save ACK"),P->ErrorCode.IsEmpty());P->Finish();Step=12;return false;}
        if(Step==12){if(!P->SessionId.IsEmpty())return false;++HydratedOthers;P->SwitchTo(Plan,Mission,false);Step=4;return false;}
        if(Step==4){if(!P->SessionId.IsEmpty())return false;
            if(HydratedOthers<2){const auto Other=Find(S->GetPlans(),TEXT("plans"),Plan)->GetArrayField(TEXT("mission_ids"))[HydratedOthers+1]->AsString();P->SwitchTo(Plan,Other,true);Step=10;return false;}
Test->TestEqual(TEXT("finish restores all saved owners"),Shell->SavedRouteVisuals.Num(),3);Test->TestFalse(TEXT("finish unlocks camera"),PC->GetCommandScreenManager()->GetMapService()->IsRouteEditCameraLocked());
            FJsonSerializer::Serialize(Route().ToSharedRef(),TJsonWriterFactory<>::Create(&Before));Original=FPlanRouteVisualSet::Decode(Route(),C).Waypoints[0].Location;Move();++Step;return false;}
        if(Step==5 || Step==7){if(!Test->TestTrue(TEXT("move lease acknowledged"),M->IsMoving()))return true;
            Test->TestEqual(TEXT("three move V2 previews"),M->PreviewVisuals.Num(),3);Test->TestEqual(TEXT("move removes saved owners"),Shell->SavedRouteVisuals.Num(),0);
            auto* A=M->PreviewVisuals.Find(Field(Find(S->GetPlans(),TEXT("missions"),Mission),TEXT("route_id")));if(!Test->TestNotNull(TEXT("move owner"),A))return true;Preview=A;
            Color=A->GetSegmentVisuals()[0].StartColor;Rate=A->GetSegmentVisuals()[1].FlowRate;M->Delta=Delta;M->Refresh();static_cast<AActor*>(PC)->Tick(.016f);A->RefreshRouteVisualPreview();
            Test->TestTrue(TEXT("preview world position is Original plus Delta"),A->GetWaypointWorldLocation(0).Equals(Original+Delta,.01));
            Test->TestTrue(TEXT("preview preserves altitude color"),A->GetSegmentVisuals()[0].StartColor==Color);Test->TestEqual(TEXT("preview preserves incoming speed flow"),A->GetSegmentVisuals()[1].FlowRate,Rate);
            Test->TestEqual(TEXT("edited incoming speed retained"),A->GetSegmentVisuals()[1].EffectiveSpeed,13.f);
            Test->TestEqual(TEXT("closing speed remains last waypoint speed"),A->GetSegmentVisuals().Last().EffectiveSpeed,10.f);
            FString During;FJsonSerializer::Serialize(Route().ToSharedRef(),TJsonWriterFactory<>::Create(&During));Test->TestEqual(TEXT("drag leaves Backend route unchanged"),During,Before);
            TArray<USplineMeshComponent*> Meshes;A->GetComponents(Meshes);for(auto* Mesh:Meshes)Test->TestTrue(TEXT("move keeps camera-scaled visible route width"),Mesh->GetStartScale().X>.05f);bool Material=false;for(auto* Mesh:Meshes)if(auto* MID=Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0))){Material|=MID->Parent->GetPathName().Contains(TEXT("M_CommandPathV2"));}
            Test->TestTrue(TEXT("real V2 material on move core and halo"),Material);
            M->Action(Step==5?TEXT("cancel"):TEXT("confirm"),0);++Step;return false;}
        if(Step==6){Test->TestFalse(TEXT("cancel releases preview"),M->IsMoving());Test->TestFalse(TEXT("cancel destroys preview actors"),Preview.IsValid());
            FString After;FJsonSerializer::Serialize(Route().ToSharedRef(),TJsonWriterFactory<>::Create(&After));Test->TestEqual(TEXT("cancel retains exact Backend JSON"),After,Before);Test->TestEqual(TEXT("cancel restores three owners"),Shell->SavedRouteVisuals.Num(),3);Move();++Step;return false;}
        Test->AddInfo(TEXT("Move confirm error: ")+M->ErrorCode);Test->TestFalse(TEXT("confirm releases move lease"),M->IsMoving());Test->TestFalse(TEXT("confirm destroys preview actor"),Preview.IsValid());
        Test->TestTrue(TEXT("confirm ACK retains translated geographic route"),FPlanRouteVisualSet::Decode(Route(),C).Waypoints[0].Location.Equals(Original+Delta,.1));Test->TestEqual(TEXT("confirm restores all saved owners"),Shell->SavedRouteVisuals.Num(),3);
        Test->TestTrue(TEXT("closed topology retained after confirm"),Route()->GetBoolField(TEXT("bClosedLoop")));
        // Count only this plan's owners: other independent active execution groups may exist.
        int Owners=Shell->SavedRouteVisuals.Num()+M->PreviewVisuals.Num()+(PC->GetMissionRouteVisual() && !PC->GetMissionRouteVisual()->IsHidden()?1:0);
        Test->TestEqual(TEXT("no duplicate plan ownership"),Owners,3);
        if(const auto E=ExecutionUI::Latest(S->GetPlans(),TEXT(""),true)){
            const FString EP=Field(E,TEXT("plan_id")),EM=Field(E,TEXT("mission_id"));
            P->SwitchTo(EP,EM,false);Shell->Refresh();
            Test->TestTrue(TEXT("acknowledged execution owns its Mission"),Shell->ExecutionMonitor->OwnsMission(EP,EM));
            Test->TestEqual(TEXT("execution removes overlapping saved owner"),Shell->SavedRouteVisuals.Num(),0);
            auto Context=MakeShared<FJsonObject>();Context->Values=S->GetContext()->Values;
            Context->SetNumberField(TEXT("context_version"),S->GetContext()->GetNumberField(TEXT("context_version"))+10000);
            Context->SetStringField(TEXT("active_security_plan_id"),EP);Context->SetStringField(TEXT("active_mission_id"),EM);S->ApplyContext(Context);Shell->Refresh();
            int SelectedOwners=0;for(TActorIterator<ADronePathActor> I(W);I;++I)if(!I->IsHidden() && I->IsVisualSelected() && !I->GetSegmentVisuals().IsEmpty())++SelectedOwners;
            Test->TestEqual(TEXT("selected Mission emphasis follows execution owner"),SelectedOwners,1);
            P->SwitchTo(Plan,Mission,false);Shell->Refresh();Test->TestEqual(TEXT("switch back rebuilds only three saved owners"),Shell->SavedRouteVisuals.Num(),3);
        }else Test->AddError(TEXT("Expected acknowledged execution fixture"));
        Test->AddInfo(FString::Printf(TEXT("Total visible world routes (including independent execution groups): %d"),Visible()));return true;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP55SecurityPlanVisualTest,"DroneOps.P55.SecurityPlanRouteVisual",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FP55SecurityPlanVisualTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(FParse::Param(FCommandLine::Get(),TEXT("P55OfflineVisualQA"))?TEXT("/Game/Tests/P55OfflineMap"):TEXT("/Game/Level/CesiumWorld")));ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(8));ADD_LATENT_AUTOMATION_COMMAND(FP55SecurityPlanVisual(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
