#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CommandCenterPanel.generated.h"

/** Command-only content. Existing Fleet/Alert widgets and Backend authority stay in the shell. */
UCLASS()
class UE5DRONECONTROL_API UCommandCenterPanel : public UUserWidget
{
    GENERATED_BODY()
public:
    void Refresh();
    FString GetHeaderText() const;
protected:
    virtual void NativeOnInitialized() override;
private:
    UPROPERTY() TObjectPtr<class UCommandTacticalMap> TacticalMap;
    UPROPERTY() TObjectPtr<class USecurityPlanWorkspaceWidget> PlanPanel;
    UPROPERTY() TObjectPtr<class UTextBlock> Status;
    UPROPERTY() TObjectPtr<class UVerticalBox> Content;
};
