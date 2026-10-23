#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace MapSelectionVideo {
inline bool SelectionAckMatches(const TSharedPtr<FJsonObject>& Ack,int64 CurrentVersion,int32 Primary,const TArray<int32>& Members) {
    double Version=-1;
    if(!Ack || !Ack->TryGetNumberField(TEXT("context_version"),Version) || Version!=CurrentVersion || CurrentVersion<0)return false;
    const auto Active=Ack->TryGetField(TEXT("active_uav_id"));
    if(!Active || (Primary<=0?Active->Type!=EJson::Null:
        Active->Type!=EJson::String || Active->AsString()!=FString::Printf(TEXT("UAV-%02d"),Primary)))return false;
    const TArray<TSharedPtr<FJsonValue>>* Selected=nullptr;
    if(!Ack->TryGetArrayField(TEXT("selected_uav_ids"),Selected) || Selected->Num()!=Members.Num())return false;
    for(int32 I=0;I<Members.Num();++I)
        if(!(*Selected)[I] || (*Selected)[I]->Type!=EJson::String || (*Selected)[I]->AsString()!=FString::Printf(TEXT("UAV-%02d"),Members[I]))return false;
    return true;
}
}
