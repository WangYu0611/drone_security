#pragma once
#include "Map/FigmaMapStyle.h"

// MAP fleet-only styling. Shared Command/Video controls keep their existing theme.
namespace FleetPanelStyle {
inline void Card(UBorder* W) {
    W->SetBrush(FSlateRoundedBoxBrush(MapUI::Surface,12.f,MapUI::Border,1.f));
    W->SetBrushColor(FLinearColor::White);W->SetPadding(FMargin(16));
}
inline void Button(UButton* W,bool Primary=false,bool Danger=false) {
    MapUI::Button(W,false,false);
    auto S=W->GetStyle();const auto Accent=Danger?MapUI::Critical:MapUI::Cyan;
    S.SetNormal(FSlateRoundedBoxBrush(Primary?Accent:MapUI::Elevated,8.f,Primary?Accent:MapUI::Border,1.f));
    S.SetHovered(FSlateRoundedBoxBrush(Primary?Accent*1.1f:MapUI::Hover,8.f,Accent,1.f));
    S.SetPressed(FSlateRoundedBoxBrush(MapUI::Selected,8.f,Accent,2.f));
    S.SetNormalForeground(Primary?MapUI::Background:MapUI::PrimaryText);
    S.SetNormalPadding(FMargin(10,7)).SetPressedPadding(FMargin(10,7));W->SetStyle(S);
    if(auto* Label=Cast<UTextBlock>(W->GetContent())){
        MapUI::Text(Label,14,Primary?MapUI::Background:Danger?MapUI::Critical:MapUI::PrimaryText,Primary);
        Label->SetJustification(ETextJustify::Center);
    }
}
}
