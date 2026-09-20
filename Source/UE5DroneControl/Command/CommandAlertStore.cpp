#include "Command/CommandAlertStore.h"
#include "Shared/OperationalContextSubsystem.h"
#include "DroneOps/Network/DroneNetworkManager.h"
#include "Engine/GameInstance.h"

void UCommandAlertStore::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UDroneNetworkManager>();
    Network = GetGameInstance()->GetSubsystem<UDroneNetworkManager>();
    if (Network.IsValid())
        AlertHandle = Network->OnDroneWsAlert.AddUObject(this, &UCommandAlertStore::HandleNetworkAlert);
}

void UCommandAlertStore::Deinitialize()
{
    if (Network.IsValid()) Network->OnDroneWsAlert.Remove(AlertHandle);
    Network.Reset();
    Alerts.Empty();
    OnAlertAdded.Clear();
    OnAlertUpdated.Clear();
    Super::Deinitialize();
}

void UCommandAlertStore::HandleNetworkAlert(int32 DroneId, const FString& Type, int32 Value)
{
    if (GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->IsEnabled()) return;
    const FString Message = Type == TEXT("low_battery")
        ? FString::Printf(TEXT("低电量：%d%%"), Value)
        : (Type == TEXT("lost_connection") ? TEXT("遥测连接丢失") : Type);
    AddAlert(DroneId, Type, Message);
}

int32 UCommandAlertStore::AddAlert(int32 DroneId, const FString& Type, const FString& Message)
{
    FCommandAlert Alert;
    Alert.Id = NextId++;
    Alert.SharedId = TEXT("LOCAL-") + FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    Alert.ReceivedAt = FDateTime::UtcNow();
    Alert.DroneId = DroneId;
    Alert.Type = Type;
    Alert.Message = Message;
    // Bound local memory. UI explicitly labels this as recent session history.
    if (Alerts.Num() >= MaxAlerts) Alerts.RemoveAt(0);
    Alerts.Add(Alert);
    OnAlertAdded.Broadcast(Alert.Id);
    return Alert.Id;
}

bool UCommandAlertStore::ClearAlert(int32 AlertId)
{
    const int32 Removed = Alerts.RemoveAll([AlertId](const FCommandAlert& Alert) { return Alert.Id == AlertId; });
    if (Removed) OnAlertUpdated.Broadcast(AlertId);
    return Removed > 0;
}

bool UCommandAlertStore::MarkHandled(int32 AlertId)
{
    for (FCommandAlert& Alert : Alerts)
    {
        if (Alert.Id != AlertId) continue;
        if (Alert.bHandled) return true;
        Alert.bHandled = true;
        OnAlertUpdated.Broadcast(AlertId);
        return true;
    }
    return false;
}

int32 UCommandAlertStore::GetUnhandledCount() const
{
    int32 Count = 0;
    for (const FCommandAlert& Alert : Alerts) if (!Alert.bHandled) ++Count;
    return Count;
}

void UCommandAlertStore::AddSharedAlert(const FString& SharedId, int32 DroneId, const FString& Type, const FString& Message) {
    for (const auto& Alert : Alerts) if (Alert.SharedId == SharedId) return;
    const int32 Id = AddAlert(DroneId, Type, Message);
    for (auto& Alert : Alerts) if (Alert.Id == Id) Alert.SharedId = SharedId;
}
