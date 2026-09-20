#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UILanguageSubsystem.generated.h"
DECLARE_MULTICAST_DELEGATE(FUILanguageChanged);
UCLASS()
class UE5DRONECONTROL_API UUILanguageSubsystem:public UGameInstanceSubsystem {
    GENERATED_BODY()
public:
    void ApplyConfirmed(const FString& Language);
    FString GetLanguage() const {return ConfirmedLanguage;}
    FUILanguageChanged OnLanguageChanged;
private:
    FString ConfirmedLanguage=TEXT("en");
};
