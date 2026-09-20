#include "Shared/OperationalEventStore.h"
#include "Shared/ProductText.h"
#include "Shared/UILanguageSubsystem.h"
#include "Shared/OperationalContextSubsystem.h"
#include "Command/CommandAlertStore.h"
#include "Engine/GameInstance.h"

void UOperationalEventStore::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UOperationalContextSubsystem>();
    Collection.InitializeDependency<UCommandAlertStore>();
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    ContextHandle=Sync->OnContextChanged.AddUObject(this,&UOperationalEventStore::ProjectContext);
    GetGameInstance()->GetSubsystem<UCommandAlertStore>()->OnAlertAdded.AddDynamic(this,&UOperationalEventStore::AlertAdded);
    LanguageHandle=GetGameInstance()->GetSubsystem<UUILanguageSubsystem>()->OnLanguageChanged.AddWeakLambda(this,[this]{OnChanged.Broadcast();});
    if(Sync->GetContext())ProjectContext(Sync->GetContext());
}
void UOperationalEventStore::Deinitialize()
{
    GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->OnContextChanged.Remove(ContextHandle);
    GetGameInstance()->GetSubsystem<UCommandAlertStore>()->OnAlertAdded.RemoveDynamic(this,&UOperationalEventStore::AlertAdded);
    GetGameInstance()->GetSubsystem<UUILanguageSubsystem>()->OnLanguageChanged.Remove(LanguageHandle);
    Super::Deinitialize();
}
void UOperationalEventStore::Add(EOperationalCategory Category,EOperationalLevel Level,const FString& Source,const FString& Type,const FString& Target,const FString& Message)
{
    FOperationalEvent E;E.Sequence=++Sequence;E.Timestamp=FDateTime::UtcNow();E.Category=Category;E.Level=Level;
    E.Source=Source;E.EventType=Type;E.TargetId=Target;E.Message=Message;
    static const TSet<FString> StructuredTypes={TEXT("backend_connected"),TEXT("backend_reconnected"),TEXT("backend_disconnected"),TEXT("client_connected"),TEXT("client_disconnected"),TEXT("active_uav_changed"),TEXT("alert_selected"),TEXT("operation_mode_changed"),TEXT("enter_alert_response"),TEXT("active_mission_id_changed"),TEXT("active_security_plan_id_changed"),TEXT("active_area_id_changed")};
    if(StructuredTypes.Contains(Type)){E.Params=MakeShared<FJsonObject>();E.Params->SetStringField(TEXT("source"),Source);E.Params->SetStringField(TEXT("target"),Target);}
    if(Events.Num()>=500)Events.RemoveAt(0);
    Events.Add(MoveTemp(E));OnChanged.Broadcast();
}
void UOperationalEventStore::ProjectContext(const TSharedPtr<FJsonObject>& Context)
{
    if(!Context)return;
    FString Source;Context->TryGetStringField(TEXT("updated_by"),Source);
    for(const TCHAR* Field:{TEXT("active_uav_id"),TEXT("active_alert_id"),TEXT("operation_mode"),TEXT("active_mission_id"),TEXT("active_security_plan_id"),TEXT("active_area_id")})
    {
        FString Value;Context->TryGetStringField(Field,Value);
        if(PreviousFields.FindRef(Field)==Value)continue;
        PreviousFields.Add(Field,Value);
        const FString F(Field);
        EOperationalCategory Category=F==TEXT("active_alert_id")?EOperationalCategory::ALERT:F==TEXT("active_mission_id")?EOperationalCategory::MISSION:
            F==TEXT("active_security_plan_id") || F==TEXT("active_area_id")?EOperationalCategory::PLAN:EOperationalCategory::OPERATION;
        FString Type=F==TEXT("active_alert_id")?TEXT("alert_selected"):F==TEXT("active_uav_id")?TEXT("active_uav_changed"):F==TEXT("operation_mode")?TEXT("operation_mode_changed"):F+TEXT("_changed");
        FString Label=F==TEXT("active_alert_id")?TEXT("Alert selected"):F==TEXT("active_uav_id")?TEXT("Active UAV changed"):F==TEXT("operation_mode")?TEXT("Operation mode changed"):F;
        Add(Category,EOperationalLevel::INFO,Source,Type,Value,Label+TEXT(": ")+(Value.IsEmpty()?TEXT("NONE"):Value));
        if(F==TEXT("operation_mode") && Value==TEXT("ALERT_RESPONSE"))Add(EOperationalCategory::ALERT,EOperationalLevel::WARNING,Source,TEXT("enter_alert_response"),Value,TEXT("Enter ALERT_RESPONSE"));
    }
}
void UOperationalEventStore::ProjectPresence(const FString& Id,const FString& Role,const FString& State)
{
    if(!Presence.Contains(Id) && State!=TEXT("ONLINE")) {Presence.Add(Id,State);return;}
    if(Presence.FindRef(Id)==State)return;
    Presence.Add(Id,State);
    Add(EOperationalCategory::SYSTEM,State==TEXT("ONLINE")?EOperationalLevel::INFO:EOperationalLevel::WARNING,Role,
        State==TEXT("ONLINE")?TEXT("client_connected"):TEXT("client_disconnected"),Id,Role+TEXT(" Client ")+(State==TEXT("ONLINE")?TEXT("Connected"):TEXT("Disconnected")));
}
void UOperationalEventStore::ProjectConnectivity(bool Connected)
{
    if(Connected==bWasConnected)return;
    Add(EOperationalCategory::SYSTEM,Connected?EOperationalLevel::INFO:EOperationalLevel::WARNING,TEXT("Backend"),
        Connected?(bEverConnected?TEXT("backend_reconnected"):TEXT("backend_connected")):TEXT("backend_disconnected"),TEXT(""),
        Connected?(bEverConnected?TEXT("Backend Reconnected"):TEXT("Backend Connected")):TEXT("Backend Disconnected"));
    bWasConnected=Connected;bEverConnected|=Connected;
}
void UOperationalEventStore::AlertAdded(int32 Id)
{
    for(const auto& A:GetGameInstance()->GetSubsystem<UCommandAlertStore>()->GetAlerts())if(A.Id==Id)
    {
        const auto Level=A.Type.Contains(TEXT("lost"))?EOperationalLevel::CRITICAL:A.Type==TEXT("info")?EOperationalLevel::INFO:EOperationalLevel::WARNING;
        Add(EOperationalCategory::ALERT,Level,TEXT("AlertStore"),TEXT("alert_raised"),FString::Printf(TEXT("UAV-%02d"),A.DroneId),
            FString::Printf(TEXT("UAV-%02d: %s"),A.DroneId,*A.Message));break;
    }
}

void UOperationalEventStore::ProjectPlanEvents(const TSharedPtr<FJsonObject>& State) {
    const TArray<TSharedPtr<FJsonValue>>* Items;
    if(!State || !State->TryGetArrayField(TEXT("events"),Items))return;
    for(const auto& Item:*Items) {
        const auto E=Item->AsObject();if(!E)continue;
        const int64 Serial=static_cast<int64>(E->GetNumberField(TEXT("sequence")));if(Serial<=LastPlanSequence)continue;
        LastPlanSequence=Serial;
        if(E->GetStringField(TEXT("event_type"))==TEXT("MISSION_FAILED")){
            const auto Params=E->GetObjectField(TEXT("params"));const FString U=Params->GetStringField(TEXT("uav_id"));int32 Id=0;LexTryParseString(Id,*U.Mid(4));
            GetGameInstance()->GetSubsystem<UCommandAlertStore>()->AddSharedAlert(FString::Printf(TEXT("execution-%lld"),Serial),Id,TEXT("execution_failed"),ProductText::Get(TEXT("Errors.")+Params->GetStringField(TEXT("reason"))).ToString());
        }
        Add(E->GetStringField(TEXT("category"))==TEXT("PLAN")?EOperationalCategory::PLAN:EOperationalCategory::MISSION,
            EOperationalLevel::INFO,E->GetStringField(TEXT("source")),E->GetStringField(TEXT("event_type")),E->GetStringField(TEXT("target_id")),E->GetStringField(TEXT("message")));
        const TSharedPtr<FJsonObject>* Params;if(!Events.IsEmpty() && E->TryGetObjectField(TEXT("params"),Params))Events.Last().Params=*Params;
        if(!Events.IsEmpty())Events.Last().Timestamp=FDateTime::FromUnixTimestamp(static_cast<int64>(E->GetNumberField(TEXT("timestamp"))));
    }
    OnChanged.Broadcast();
}
FText FOperationalEvent::DisplayText() const {
    if(!Params)return FText::AsCultureInvariant(Message);
    FFormatNamedArguments Args;for(const auto& E:Params->Values){FString Value;if(E.Value->TryGetString(Value))Args.Add(FString(E.Key.ToView()),FText::AsCultureInvariant(Value));else if(E.Value->Type==EJson::Number)Args.Add(FString(E.Key.ToView()),FText::AsNumber(E.Value->AsNumber()));}
    return FText::Format(ProductText::Get(TEXT("Events.")+EventType),Args);
}
