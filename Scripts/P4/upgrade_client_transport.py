from pathlib import Path
p=Path('Source/UE5DroneControl/Shared/OperationalContextSubsystem.h');s=p.read_text(encoding='utf-8-sig')
s=s.replace('    void ApplyPlans(const TSharedPtr<FJsonObject>& Incoming);','''    void ApplyPlans(const TSharedPtr<FJsonObject>& Incoming);
    TSharedPtr<FJsonObject> GetUIPreferences() const { return UIPreferences; }
    TSharedPtr<FJsonObject> GetVideoView() const { return VideoView; }
    FString GetInstanceId() const { return InstanceId; }
    bool SetLanguage(const FString& Language);
    bool OpenVideo(const FString& UAV);
    bool ApplyUIPreferences(const TSharedPtr<FJsonObject>& Incoming);
    bool ApplyVideoView(const TSharedPtr<FJsonObject>& Incoming);
    FOperationalContextChanged OnUIPreferencesChanged, OnVideoViewChanged, OnMapRouteEditRequested;
    FString ViewSyncError;
''')
s=s.replace('    TSharedPtr<FJsonObject> Plans;', '    TSharedPtr<FJsonObject> Plans;\n    TSharedPtr<FJsonObject> UIPreferences,VideoView;')
p.write_text(s,encoding='utf-8')
p=Path('Source/UE5DroneControl/Shared/OperationalContextSubsystem.cpp');s=p.read_text(encoding='utf-8-sig')
s=s.replace('#include "Shared/OperationalEventStore.h"','#include "Shared/OperationalEventStore.h"\n#include "Shared/UILanguageSubsystem.h"')
s=s.replace('    if (Type == TEXT("SecurityPlansChanged")) {','''    if(Type==TEXT("UIPreferencesChanged") || Type==TEXT("VideoViewChanged") || Type==TEXT("MapRouteEditRequested")) {
        const TSharedPtr<FJsonObject>* Payload;if(Json->TryGetObjectField(TEXT("payload"),Payload)) {
            if(Type==TEXT("UIPreferencesChanged"))ApplyUIPreferences(*Payload);
            else if(Type==TEXT("VideoViewChanged"))ApplyVideoView(*Payload);
            else OnMapRouteEditRequested.Broadcast(*Payload);
        }return;
    }
    if (Type == TEXT("SecurityPlansChanged")) {''')
s=s.replace('''    if (Type == TEXT("context_subscribed"))
        Request(TEXT("GET"),TEXT("/api/security-plans"),nullptr,[this](TSharedPtr<FJsonObject> Reply){ApplyPlans(Reply);});''','''    if (Type == TEXT("context_subscribed")) {
        Request(TEXT("GET"),TEXT("/api/security-plans"),nullptr,[this](TSharedPtr<FJsonObject> Reply){ApplyPlans(Reply);});
        Request(TEXT("GET"),TEXT("/api/ui-preferences"),nullptr,[this](TSharedPtr<FJsonObject> Reply){ApplyUIPreferences(Reply);});
        Request(TEXT("GET"),TEXT("/api/video-view"),nullptr,[this](TSharedPtr<FJsonObject> Reply){ApplyVideoView(Reply);});
    }''')
s+='''
namespace {
bool NewDomainVersion(const TSharedPtr<FJsonObject>& Incoming,const TSharedPtr<FJsonObject>& Current,const TCHAR* Key) {
    double N,Old=-1;if(!Incoming || !Incoming->TryGetNumberField(Key,N) || !FMath::IsFinite(N) || N<0 || N!=FMath::FloorToDouble(N))return false;
    if(Current)Current->TryGetNumberField(Key,Old);return N>Old;
}
}
bool UOperationalContextSubsystem::ApplyUIPreferences(const TSharedPtr<FJsonObject>& Incoming) {
    if(!NewDomainVersion(Incoming,UIPreferences,TEXT("ui_preferences_version")))return false;
    FString Language;if(!Incoming->TryGetStringField(TEXT("language"),Language) || (Language!=TEXT("en") && Language!=TEXT("zh-Hans")))return false;
    UIPreferences=Incoming;GetGameInstance()->GetSubsystem<UUILanguageSubsystem>()->ApplyConfirmed(Language);
    OnUIPreferencesChanged.Broadcast(Incoming);return true;
}
bool UOperationalContextSubsystem::ApplyVideoView(const TSharedPtr<FJsonObject>& Incoming) {
    if(!NewDomainVersion(Incoming,VideoView,TEXT("video_view_version")))return false;
    const auto V=Incoming->TryGetField(TEXT("video_target_uav_id"));if(!V || (V->Type!=EJson::Null && V->Type!=EJson::String))return false;
    VideoView=Incoming;OnVideoViewChanged.Broadcast(Incoming);return true;
}
bool UOperationalContextSubsystem::SetLanguage(const FString& Language) {
    if(!IsReady()){ViewSyncError=TEXT("SYNC_OFFLINE");return false;}
    auto Body=MakeShared<FJsonObject>();Body->SetStringField(TEXT("instance_id"),InstanceId);Body->SetStringField(TEXT("language"),Language);
    Request(TEXT("PATCH"),TEXT("/api/ui-preferences"),Body,[this](TSharedPtr<FJsonObject> R){ViewSyncError=R?TEXT(""):TEXT("SYNC_FAILED");ApplyUIPreferences(R);});return true;
}
bool UOperationalContextSubsystem::OpenVideo(const FString& UAV) {
    if(!IsReady()){ViewSyncError=TEXT("SYNC_OFFLINE");return false;}
    auto Body=MakeShared<FJsonObject>();Body->SetStringField(TEXT("instance_id"),InstanceId);
    if(UAV.IsEmpty())Body->SetField(TEXT("video_target_uav_id"),MakeShared<FJsonValueNull>());else Body->SetStringField(TEXT("video_target_uav_id"),UAV);
    Request(TEXT("PATCH"),TEXT("/api/video-view"),Body,[this](TSharedPtr<FJsonObject> R){ViewSyncError=R?TEXT(""):TEXT("SYNC_FAILED");ApplyVideoView(R);});return true;
}
'''
p.write_text(s,encoding='utf-8')
