#include "Command/GroupControlWidget.h"
#include "Shared/PlanWidgetSupport.h"
#include "Shared/ExecutionPresentation.h"
#include "Shared/ActiveExecutionControlsWidget.h"
#include "Shared/PlanConfiguration.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "Command/FigmaCommandStyle.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBoxSlot.h"
using namespace PlanUI;
namespace {
ExecutionControls::FGroup SelectedGroup(const TSharedPtr<FJsonObject>& State,const TArray<int32>& Selected) {
    TArray<FString> Members;for(int32 Id:Selected)Members.Add(FString::Printf(TEXT("UAV-%02d"),Id));Members.Sort();
    for(const auto& Group:ExecutionControls::Groups(State,false)){
        const auto E=ExecutionControls::Representative(Group);
        if(Group.Members==Members && Field(E,TEXT("control_kind"))==TEXT("GROUP_MOVE"))return Group;
    }return {};
}
}
void UGroupControlWidget::NativeOnInitialized(){
    Super::NativeOnInitialized();auto* Content=Root(WidgetTree);
    Label(WidgetTree,Content,T(TEXT("Group.Title")),20);Summary=Label(WidgetTree,Content,FText::GetEmpty(),16);
    MoveInputs=WidgetTree->ConstructWidget<UVerticalBox>();Content->AddChild(MoveInputs);
    auto Input=[&](const TCHAR* Key){Label(WidgetTree,MoveInputs,T(Key),14);auto* E=WidgetTree->ConstructWidget<UEditableTextBox>();CommandTheme::Input(E);MoveInputs->AddChild(E);return E;};
    Latitude=Input(TEXT("Group.Latitude"));Longitude=Input(TEXT("Group.Longitude"));Altitude=Input(TEXT("Group.Altitude"));AreaRadius=Input(TEXT("Group.Area"));AreaRadius->SetText(User(TEXT("100")));
    auto* Move=Button(WidgetTree,MoveInputs,TEXT("move"),TEXT("Group.Move"));Move->OnAction.AddDynamic(this,&UGroupControlWidget::Action);Buttons.Add(TEXT("move"),Move);
    for(const auto& Pair:TArray<TPair<FString,FString>>{{TEXT("pause"),TEXT("Execution.Pause")},{TEXT("resume"),TEXT("Execution.Resume")},{TEXT("abort"),TEXT("Execution.Abort")},{TEXT("abort_confirm"),TEXT("ExecutionControls.ConfirmAbort")},{TEXT("abort_cancel"),TEXT("Common.Cancel")}}){auto* B=Button(WidgetTree,Content,*Pair.Key,*Pair.Value);B->OnAction.AddDynamic(this,&UGroupControlWidget::Action);Buttons.Add(*Pair.Key,B);}
    Result=Label(WidgetTree,Content,FText::GetEmpty(),14);
}
void UGroupControlWidget::ConfigureFigmaLayout(){
    if(!Summary || !WidgetTree)return;
    auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>();WidgetTree->RootWidget=Scroll;
    auto* Content=WidgetTree->ConstructWidget<UVerticalBox>();Scroll->AddChild(Content);
    auto* Title=Label(WidgetTree,Content,ProductText::Get(TEXT("Figma.Fleet.Group")),19);FigmaUI::Text(Title,19,FigmaUI::PrimaryText,true);
    Summary->RemoveFromParent();FigmaUI::Text(Summary,11,FigmaUI::Muted);Content->AddChildToVerticalBox(Summary)->SetPadding(FMargin(0,5,0,12));
    auto* Inputs=WidgetTree->ConstructWidget<UVerticalBox>();Content->AddChild(Inputs);
    auto* Grid=WidgetTree->ConstructWidget<UUniformGridPanel>();Grid->SetSlotPadding(FMargin(0,0,12,12));Inputs->AddChild(Grid);
    UEditableTextBox* Fields[]={Latitude,Longitude,Altitude,AreaRadius};
    const TCHAR* Keys[]={TEXT("Group.Latitude"),TEXT("Group.Longitude"),TEXT("Group.Altitude"),TEXT("Group.Area")};
    for(int32 I=0;I<4;++I){
        auto* Cell=WidgetTree->ConstructWidget<UVerticalBox>();Grid->AddChildToUniformGrid(Cell,I/2,I%2);
        auto* Caption=Label(WidgetTree,Cell,T(Keys[I]),12);FigmaUI::Text(Caption,12,FigmaUI::PrimaryText,true);
        Fields[I]->RemoveFromParent();FigmaUI::Input(Fields[I]);Cell->AddChildToVerticalBox(Fields[I])->SetPadding(FMargin(0,5,0,0));
    }
    Buttons[TEXT("move")]->RemoveFromParent();FigmaUI::Button(Buttons[TEXT("move")],false,true);
    if(auto* LabelWidget=Cast<UTextBlock>(Buttons[TEXT("move")]->GetContent()))FigmaUI::Text(LabelWidget,12);
    Inputs->AddChild(Buttons[TEXT("move")]);MoveInputs=Inputs;
    auto* Controls=WidgetTree->ConstructWidget<UHorizontalBox>();Content->AddChildToVerticalBox(Controls)->SetPadding(FMargin(0,5,0,0));
    for(const TCHAR* Name:{TEXT("pause"),TEXT("resume"),TEXT("abort"),TEXT("abort_confirm"),TEXT("abort_cancel")}){
        auto* ButtonWidget=Buttons[Name].Get();ButtonWidget->RemoveFromParent();FigmaUI::Button(ButtonWidget,false,true);
        if(auto* LabelWidget=Cast<UTextBlock>(ButtonWidget->GetContent()))FigmaUI::Text(LabelWidget,12);
        Controls->AddChildToHorizontalBox(ButtonWidget)->SetPadding(FMargin(0,0,6,0));
    }
    Result->RemoveFromParent();FigmaUI::Text(Result,11,FigmaUI::Muted);Content->AddChildToVerticalBox(Result)->SetPadding(FMargin(0,5,0,0));
}
void UGroupControlWidget::SetSelectionPending(bool bInSelectionPending){
    if(bSelectionPending==bInSelectionPending)return;
    bSelectionPending=bInSelectionPending;Refresh();
}
void UGroupControlWidget::Refresh(){
    if(!Summary)return;auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();auto* Registry=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();const auto Selected=Registry->GetMultiSelectedDrones();
    const bool Configuring=PlanConfiguration::Active(Sync->GetPlans());MoveInputs->SetVisibility(Configuring?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
    double Separation=0;const auto Config=Object(Sync->GetPlans(),TEXT("mock_execution"));if(Config)Config->TryGetNumberField(TEXT("safety_separation_m"),Separation);
    TArray<FText> Lines{T(Field(Sync->GetPlans(),TEXT("execution_adapter"))==TEXT("Real")?TEXT("Execution.Real"):TEXT("Execution.Simulation")),FText::Format(T(TEXT("Group.Summary")),FText::AsNumber(Selected.Num()),FText::AsNumber(Separation))};
    if(Configuring)Lines.Add(T(TEXT("Configuration.MoveBlocked")));
    if(bSelectionPending)Lines.Add(T(TEXT("Fleet.SelectionPending")));
    if(Latitude->GetText().IsEmpty() && !Selected.IsEmpty()){FDroneTelemetrySnapshot Telemetry;if(Registry->GetTelemetry(Selected[0],Telemetry) && Telemetry.bGpsFix){Latitude->SetText(User(FString::Printf(TEXT("%.8f"),Telemetry.GpsLatitude)));Longitude->SetText(User(FString::Printf(TEXT("%.8f"),Telemetry.GpsLongitude)));Altitude->SetText(User(FString::SanitizeFloat(Telemetry.GpsAltitude)));}}
    const auto Group=SelectedGroup(Sync->GetPlans(),Selected);ManagedGroup=Group.Id;ExecutionId=Field(ExecutionControls::Representative(Group),TEXT("execution_id"));
    if(ManagedGroup.IsEmpty() || AbortGroup!=ManagedGroup || !Group.CanAbort)bAbortConfirm=false;
    if(!ManagedGroup.IsEmpty())Lines.Add(FText::Format(T(TEXT("ExecutionControls.Group")),Group.PlanName.IsEmpty()?T(TEXT("Group.Title")):ProductText::Source(Group.PlanName),Group.MissionName.IsEmpty()?T(TEXT("Group.Title")):ProductText::Source(Group.MissionName),User(FString::Join(Group.Members,TEXT(", "))),T(*(TEXT("Execution.")+Group.State)),FText::AsPercent(Group.Progress),User(Group.Id)));
    Summary->SetText(FText::Join(User(TEXT("\n")),Lines));
    const bool Online=Sync->IsReady() && Sync->IsHydrated();
    Buttons[TEXT("move")]->SetIsEnabled(!Configuring && !bPending && !bSelectionPending && Online && !Selected.IsEmpty());
    auto Set=[&](const TCHAR* Name,bool Visible,bool Enabled){Buttons[Name]->SetVisibility(Visible?ESlateVisibility::Visible:ESlateVisibility::Collapsed);Buttons[Name]->SetIsEnabled(Enabled && !bPending && Online);};
    Set(TEXT("pause"),!bAbortConfirm && Group.CanPause,Group.CanPause);Set(TEXT("resume"),!bAbortConfirm && Group.CanResume,Group.CanResume);Set(TEXT("abort"),!bAbortConfirm && Group.CanAbort,Group.CanAbort);
    Set(TEXT("abort_confirm"),bAbortConfirm,Group.CanAbort);Set(TEXT("abort_cancel"),bAbortConfirm,true);
    Buttons[TEXT("abort_cancel")]->SetIsEnabled(!bPending);
}
void UGroupControlWidget::Action(FName Name,int32){
    if(bPending)return;auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    if(Name==TEXT("abort_cancel")){bAbortConfirm=false;Refresh();return;}
    if(Name==TEXT("move") && bSelectionPending){Refresh();return;}
    if(!Sync->IsReady() || !Sync->IsHydrated()){Result->SetText(T(TEXT("Errors.SYNC_OFFLINE")));Refresh();return;}
    auto* Registry=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();const auto Group=SelectedGroup(Sync->GetPlans(),Registry->GetMultiSelectedDrones());
    if(Name==TEXT("abort")){if(Group.CanAbort){bAbortConfirm=true;AbortGroup=Group.Id;}Refresh();return;}
    if(Name==TEXT("abort_confirm")){if(!bAbortConfirm || AbortGroup!=Group.Id)return;bAbortConfirm=false;Name=TEXT("abort");}
    auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("request_id"),FGuid::NewGuid().ToString());
    if(Name==TEXT("move")){
        if(PlanConfiguration::Active(Sync->GetPlans())){Result->SetText(T(TEXT("Configuration.MoveBlocked")));Refresh();return;}
        double Lat=0,Lon=0,Alt=0,Radius=0;if(!LexTryParseString(Lat,*Latitude->GetText().ToString()) || !LexTryParseString(Lon,*Longitude->GetText().ToString()) || !LexTryParseString(Alt,*Altitude->GetText().ToString()) || !LexTryParseString(Radius,*AreaRadius->GetText().ToString()) || !FMath::IsFinite(Lat) || !FMath::IsFinite(Lon) || !FMath::IsFinite(Alt) || !FMath::IsFinite(Radius)){Result->SetText(T(TEXT("Errors.INVALID_WAYPOINT")));return;}
        R->SetStringField(TEXT("action"),TEXT("execution_group_move"));auto Target=MakeShared<FJsonObject>();Target->SetNumberField(TEXT("latitude"),Lat);Target->SetNumberField(TEXT("longitude"),Lon);Target->SetNumberField(TEXT("altitude"),Alt);R->SetObjectField(TEXT("target"),Target);R->SetNumberField(TEXT("target_area_radius_m"),Radius);
        TArray<TSharedPtr<FJsonValue>> Members;for(int32 Id:Registry->GetMultiSelectedDrones())Members.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("UAV-%02d"),Id)));R->SetArrayField(TEXT("assigned_uav_ids"),Members);
    }else{
        if(Name!=TEXT("pause") && Name!=TEXT("resume") && Name!=TEXT("abort"))return;
        // Re-resolve against the current selected members; a different latest group is never a target.
        const auto E=ExecutionControls::Representative(Group,Name);if(!E || Group.Id!=ManagedGroup){Result->SetText(T(TEXT("Errors.EXECUTION_STATE_INVALID")));Refresh();return;}
        R->SetStringField(TEXT("action"),TEXT("execution_")+Name.ToString());R->SetStringField(TEXT("execution_id"),Field(E,TEXT("execution_id")));R->SetNumberField(TEXT("control_version"),E->GetNumberField(TEXT("control_version")));
    }
    bPending=true;Result->SetText(T(TEXT("Common.Waiting")));const auto Weak=TWeakObjectPtr<UGroupControlWidget>(this);
    if(!Sync->SubmitPlan(R,[Weak](TSharedPtr<FJsonObject> Reply){if(!Weak.IsValid())return;Weak->bPending=false;const auto Code=PlanUI::Error(Reply);FText Text=Code.IsEmpty()?T(TEXT("Group.Accepted")):ProductText::Get(TEXT("Errors.")+Code);
        const auto Params=Object(Reply,TEXT("params"));if(Params && Params->HasField(TEXT("minimum_separation_m")))Text=FText::Format(T(TEXT("Group.Conflict")),User(Field(Params,TEXT("uav_a"))),User(Field(Params,TEXT("uav_b"))),FText::AsNumber(Params->GetNumberField(TEXT("minimum_separation_m"))),FText::AsNumber(Params->GetNumberField(TEXT("required_separation_m"))));
        const auto Reservation=ReservationError(Reply);if(!Reservation.IsEmpty())Text=Reservation;
        Weak->Result->SetText(Text);Weak->Refresh();})){bPending=false;Result->SetText(T(TEXT("Errors.SYNC_OFFLINE")));}Refresh();
}
