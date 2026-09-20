#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Dom/JsonObject.h"
#include "OperationalEventStore.generated.h"

UENUM()
enum class EOperationalCategory : uint8 { SYSTEM, OPERATION, ALERT, MISSION, PLAN };
UENUM()
enum class EOperationalLevel : uint8 { INFO, WARNING, CRITICAL };
USTRUCT()
struct FOperationalEvent
{
    GENERATED_BODY()
    uint64 Sequence = 0;
    FDateTime Timestamp;
    EOperationalCategory Category = EOperationalCategory::SYSTEM;
    EOperationalLevel Level = EOperationalLevel::INFO;
    FString Source, EventType, TargetId, Message;
    TSharedPtr<FJsonObject> Params;
    FText DisplayText() const;
};
DECLARE_MULTICAST_DELEGATE(FOperationalEventsChanged);

/** Session projection of authoritative business events; bounded memory, never deletes Backend data. */
UCLASS()
class UE5DRONECONTROL_API UOperationalEventStore : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    const TArray<FOperationalEvent>& GetEvents() const { return Events; }
    uint64 GetLatestSequence() const { return Sequence; }
    FOperationalEventsChanged OnChanged;
    void ProjectContext(const TSharedPtr<FJsonObject>& Context);
    void ProjectPresence(const FString& Id,const FString& Role,const FString& State);
    void ProjectConnectivity(bool Connected);
    void ProjectPlanEvents(const TSharedPtr<FJsonObject>& State);
private:
    TArray<FOperationalEvent> Events;
    TMap<FString,FString> PreviousFields, Presence;
    uint64 Sequence = 0;
    int64 LastPlanSequence = 0;
    bool bWasConnected = false, bEverConnected = false;
    FDelegateHandle ContextHandle,LanguageHandle;
    void Add(EOperationalCategory Category,EOperationalLevel Level,const FString& Source,const FString& Type,const FString& Target,const FString& Message);
    UFUNCTION() void AlertAdded(int32 Id);
};
