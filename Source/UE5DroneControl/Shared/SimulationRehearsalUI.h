#pragma once

#include "Shared/PlanWidgetSupport.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"

namespace SimulationUI
{
inline FText Label(const TCHAR* English, const TCHAR* Chinese)
{
    const bool bChinese = FInternationalization::Get().GetCurrentLanguage()->GetTwoLetterISOLanguageName() == TEXT("zh");
    return FText::FromString(bChinese ? Chinese : English);
}

inline bool Supported(const TSharedPtr<FJsonObject>& State)
{
    return PlanUI::Object(State, TEXT("simulation_uavs")).IsValid();
}

inline bool Aircraft(const TSharedPtr<FJsonObject>& State, const FString& Id)
{
    return PlanUI::Find(State, TEXT("simulation_uavs"), Id).IsValid();
}

inline FText AircraftCaption(const TSharedPtr<FJsonObject>& State, const FString& Id)
{
    const auto AircraftRecord = PlanUI::Find(State, TEXT("simulation_uavs"), Id);
    if (!AircraftRecord) return PlanUI::User(Id);
    const auto Name = PlanUI::Field(AircraftRecord, TEXT("name"));
    return FText::Format(Label(TEXT("{0} · Simulated"), TEXT("{0} · 模拟")), PlanUI::User(Name.IsEmpty() ? Id : Name));
}
}
