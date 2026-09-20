#include "Command/CommandActionButton.h"
#include "Command/CommandTheme.h"
UCommandActionButton::UCommandActionButton(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    InitIsFocusable(false);
}
void UCommandActionButton::Configure(FName InAction, int32 InItemId)
{
    CommandTheme::Button(this);
    Action = InAction;
    ItemId = InItemId;
    OnClicked.AddUniqueDynamic(this, &UCommandActionButton::Clicked);
}
void UCommandActionButton::Clicked() { OnAction.Broadcast(Action, ItemId); }
