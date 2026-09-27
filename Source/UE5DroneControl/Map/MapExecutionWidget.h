#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Dom/JsonObject.h"
#include "Map/PlanRouteVisualSet.h"
#include "MapExecutionWidget.generated.h"
UCLASS()
class UE5DRONECONTROL_API UMapExecutionWidget:public UUserWidget {
    GENERATED_BODY()
    friend class FP52ExecutionWidgets;
public:void Refresh();
    void SetRouteSuppression(const FString& Plan,const FString& Mission,bool Moving) { SuppressedPlan=Plan;SuppressedMission=Mission;bPlanMoving=Moving; }
    bool OwnsMission(const FString& Plan,const FString& Mission) const;
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;
    virtual int32 NativePaint(const FPaintArgs&,const FGeometry&,const FSlateRect&,FSlateWindowElementList&,int32,const FWidgetStyle&,bool) const override;
private:
    UPROPERTY() TObjectPtr<class UTextBlock> Summary;
    TSharedPtr<FJsonObject> Execution;
    TArray<TSharedPtr<FJsonObject>> RenderedExecutions;
    FString FocusedExecution;
    FString SuppressedPlan,SuppressedMission;
    bool bPlanMoving=false;
    bool IsRouteSuppressed(const TSharedPtr<FJsonObject>& E) const;
    FPlanRouteVisualSet VisualRoutes;
};
