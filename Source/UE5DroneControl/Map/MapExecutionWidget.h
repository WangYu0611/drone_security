#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Dom/JsonObject.h"
#include "MapExecutionWidget.generated.h"
UCLASS()
class UE5DRONECONTROL_API UMapExecutionWidget:public UUserWidget {
    GENERATED_BODY()
public:void Refresh();
protected:
    virtual void NativeOnInitialized() override;
    virtual int32 NativePaint(const FPaintArgs&,const FGeometry&,const FSlateRect&,FSlateWindowElementList&,int32,const FWidgetStyle&,bool) const override;
private:
    UPROPERTY() TObjectPtr<class UTextBlock> Summary;
    TSharedPtr<FJsonObject> Execution;
    FString FocusedExecution;
};
