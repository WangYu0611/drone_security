#pragma once
#include "Command/CommandTheme.h"

// MAP fleet-only styling. Shared Command/Video controls keep their existing theme.
namespace FleetPanelStyle {
inline void Card(UBorder* W) {
    W->SetBrush(FSlateRoundedBoxBrush(CommandTheme::Surface,12.f,CommandTheme::Border,1.f));
    W->SetBrushColor(FLinearColor::White);W->SetPadding(FMargin(20));
}
inline void Button(UButton* W,bool Primary=false,bool Danger=false) {
    CommandTheme::Button(W,false,false);
    auto S=W->GetStyle();const auto Accent=Danger?CommandTheme::Critical:CommandTheme::Cyan;
    S.SetNormal(FSlateRoundedBoxBrush(Primary?Accent:CommandTheme::Elevated,8.f,Primary?Accent:CommandTheme::Border,1.f));
    S.SetHovered(FSlateRoundedBoxBrush(Primary?Accent*1.1f:CommandTheme::Hover,8.f,Accent,1.f));
    S.SetPressed(FSlateRoundedBoxBrush(CommandTheme::Selected,8.f,Accent,2.f));
    S.SetNormalForeground(Primary?CommandTheme::Background:CommandTheme::PrimaryText);
    S.SetNormalPadding(FMargin(12,10)).SetPressedPadding(FMargin(12,10));W->SetStyle(S);
    if(auto* Label=Cast<UTextBlock>(W->GetContent())){
        CommandTheme::Text(Label,16,Primary?CommandTheme::Background:Danger?CommandTheme::Critical:CommandTheme::PrimaryText,Primary);
        Label->SetJustification(ETextJustify::Center);
    }
}
}
