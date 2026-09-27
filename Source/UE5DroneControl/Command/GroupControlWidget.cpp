#include "Command/GroupControlWidget.h"
#include "Shared/PlanWidgetSupport.h"
#include "Shared/ExecutionPresentation.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
using namespace PlanUI;
void UGroupControlWidget::NativeOnInitialized(){
    Super::NativeOnInitialized();auto* Content=Root(WidgetTree);
    Label(WidgetTree,Content,T(TEXT("Group.Title")),20);Summary=Label(WidgetTree,Content,FText::GetEmpty(),16);
    auto Input=[&](const TCHAR* Key){Label(WidgetTree,Content,T(Key),14);auto* E=WidgetTree->ConstructWidget<UEditableTextBox>();CommandTheme::Input(E);Content->AddChild(E);return E;};
    Latitude=Input(TEXT("Group.Latitude"));Longitude=Input(TEXT("Group.Longitude"));Altitude=Input(TEXT("Group.Altitude"));AreaRadius=Input(TEXT("Group.Area"));AreaRadius->SetText(User(TEXT("100")));
    for(const auto& Pair:TArray<TPair<FString,FString>>{{TEXT("move"),TEXT("Group.Move")},{TEXT("pause"),TEXT("Execution.Pause")},{TEXT("resume"),TEXT("Execution.Resume")},{TEXT("abort"),TEXT("Execution.Abort")}}){auto* B=Button(WidgetTree,Content,*Pair.Key,*Pair.Value);B->OnAction.AddDynamic(this,&UGroupControlWidget::Action);Buttons.Add(*Pair.Key,B);}
    Result=Label(WidgetTree,Content,FText::GetEmpty(),14);
}
void UGroupControlWidget::Refresh(){
    if(!Summary)return;auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();auto* Registry=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();const auto Selected=Registry->GetMultiSelectedDrones();
    double Separation=0;const auto Config=Object(Sync->GetPlans(),TEXT("mock_execution"));if(Config)Config->TryGetNumberField(TEXT("safety_separation_m"),Separation);
    Summary->SetText(FText::Format(User(TEXT("{0}\n{1}")),T(Field(Sync->GetPlans(),TEXT("execution_adapter"))==TEXT("Real")?TEXT("Execution.Real"):TEXT("Execution.Simulation")),FText::Format(T(TEXT("Group.Summary")),FText::AsNumber(Selected.Num()),FText::AsNumber(Separation))));
    if(Latitude->GetText().IsEmpty() && !Selected.IsEmpty()){FDroneTelemetrySnapshot Telemetry;if(Registry->GetTelemetry(Selected[0],Telemetry) && Telemetry.bGpsFix){Latitude->SetText(User(FString::Printf(TEXT("%.8f"),Telemetry.GpsLatitude)));Longitude->SetText(User(FString::Printf(TEXT("%.8f"),Telemetry.GpsLongitude)));Altitude->SetText(User(FString::SanitizeFloat(Telemetry.GpsAltitude)));}}
    TSharedPtr<FJsonObject> Latest;double Time=-1;const auto All=Object(Sync->GetPlans(),TEXT("executions"));if(All)for(const auto& E:All->Values){const auto X=E.Value->AsObject();if(Field(X,TEXT("control_kind"))==TEXT("GROUP_MOVE") && X->GetNumberField(TEXT("created_at"))>Time){Latest=X;Time=X->GetNumberField(TEXT("created_at"));}}
    ExecutionId=Field(Latest,TEXT("execution_id"));const auto State=Field(Latest,TEXT("state"));
    Buttons[TEXT("move")]->SetIsEnabled(!bPending && Sync->IsReady() && Selected.Num()>1);
    Buttons[TEXT("pause")]->SetIsEnabled(!bPending && State==TEXT("EXECUTING"));Buttons[TEXT("resume")]->SetIsEnabled(!bPending && State==TEXT("PAUSED"));Buttons[TEXT("abort")]->SetIsEnabled(!bPending && (State==TEXT("EXECUTING") || State==TEXT("PAUSED")));
}
void UGroupControlWidget::Action(FName Name,int32){
    if(bPending)return;auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();auto R=MakeShared<FJsonObject>();
    R->SetStringField(TEXT("request_id"),FGuid::NewGuid().ToString());
    if(Name==TEXT("move")){
        double Lat=0,Lon=0,Alt=0,Radius=0;if(!LexTryParseString(Lat,*Latitude->GetText().ToString()) || !LexTryParseString(Lon,*Longitude->GetText().ToString()) || !LexTryParseString(Alt,*Altitude->GetText().ToString()) || !LexTryParseString(Radius,*AreaRadius->GetText().ToString())){Result->SetText(T(TEXT("Errors.INVALID_WAYPOINT")));return;}
        R->SetStringField(TEXT("action"),TEXT("execution_group_move"));auto Target=MakeShared<FJsonObject>();Target->SetNumberField(TEXT("latitude"),Lat);Target->SetNumberField(TEXT("longitude"),Lon);Target->SetNumberField(TEXT("altitude"),Alt);R->SetObjectField(TEXT("target"),Target);R->SetNumberField(TEXT("target_area_radius_m"),Radius);
        TArray<TSharedPtr<FJsonValue>> Members;for(int32 Id:GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetMultiSelectedDrones())Members.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("UAV-%02d"),Id)));R->SetArrayField(TEXT("assigned_uav_ids"),Members);
    }else{const auto E=Find(Sync->GetPlans(),TEXT("executions"),ExecutionId);if(!E)return;R->SetStringField(TEXT("action"),TEXT("execution_")+Name.ToString());R->SetStringField(TEXT("execution_id"),ExecutionId);R->SetNumberField(TEXT("control_version"),E->GetNumberField(TEXT("control_version")));}
    bPending=true;Result->SetText(T(TEXT("Common.Waiting")));const auto Weak=TWeakObjectPtr<UGroupControlWidget>(this);
    if(!Sync->SubmitPlan(R,[Weak](TSharedPtr<FJsonObject> Reply){if(!Weak.IsValid())return;Weak->bPending=false;const auto Code=PlanUI::Error(Reply);FText Text=Code.IsEmpty()?T(TEXT("Group.Accepted")):ProductText::Get(TEXT("Errors.")+Code);
        const auto Params=Object(Reply,TEXT("params"));if(Params && Params->HasField(TEXT("minimum_separation_m")))Text=FText::Format(T(TEXT("Group.Conflict")),User(Field(Params,TEXT("uav_a"))),User(Field(Params,TEXT("uav_b"))),FText::AsNumber(Params->GetNumberField(TEXT("minimum_separation_m"))),FText::AsNumber(Params->GetNumberField(TEXT("required_separation_m"))));
        Weak->Result->SetText(Text);Weak->Refresh();})){bPending=false;Result->SetText(T(TEXT("Errors.SYNC_OFFLINE")));}
}
