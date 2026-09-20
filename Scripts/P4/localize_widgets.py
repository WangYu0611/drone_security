from pathlib import Path
import json
files=['Command/CommandShellWidget.cpp','Command/CommandAlarmPanel.cpp','Command/CommandCenterPanel.cpp','Map/MapShellWidget.cpp','Video/VideoShellWidget.cpp','Command/OperationalLogWidget.cpp','UI/DroneListItemWidget.cpp']
root=Path('Source/UE5DroneControl')
for f in files:
 p=root/f;s=p.read_text(encoding='utf-8-sig')
 if '#include "Shared/ProductText.h"' not in s:s='#include "Shared/ProductText.h"\n'+s
 s=s.replace('FText::FromString(', 'ProductText::Source(')
 p.write_text(s,encoding='utf-8')
p=root/'Command/CommandCenterPanel.cpp';s=p.read_text();a=s.index('    Status->SetText(');b=s.index('\n}',a)
s=s[:a]+'''    Status->SetText(FText::Format(ProductText::Get(TEXT("Command.Header")),ProductText::Source(Mode),FText::AsCultureInvariant(Sync->GetStatusText()),FText::AsNumber(OnlineRoles.Num())));'''+s[b:];p.write_text(s)
p=root/'Command/CommandShellWidget.cpp';s=p.read_text();a=s.index('    if(CenterPanel) Overview->SetText(');b=s.index('\n',a)
s=s[:a]+'''    if(CenterPanel) Overview->SetText(FText::Format(ProductText::Get(TEXT("Command.Count")),FText::AsNumber(Drones.Num()),FText::AsNumber(Online),FText::AsNumber(Store?Store->GetUnhandledCount():0)));'''+s[b:]
s=s.replace('Metrics = FString::Printf(TEXT("ALT %s   SPD %s   BAT %s"), *Altitude, *Speed, *Battery);','Metrics=FText::Format(ProductText::Get(TEXT("Command.Metrics")),FText::AsCultureInvariant(Altitude),FText::AsCultureInvariant(Speed),FText::AsCultureInvariant(Battery)).ToString();')
s=s.replace('''Mission = FString::Printf(TEXT("LINK  %s\\nMISSION  %s"),
            bHasTelemetry ? *AvailabilityText(Telemetry.Availability) : TEXT("N/A"),
            bHasTask ? *TaskText(Task) : TEXT("N/A"));''','''Mission=FText::Format(ProductText::Get(TEXT("Command.Link")),ProductText::Source(bHasTelemetry?AvailabilityText(Telemetry.Availability):TEXT("N/A")),ProductText::Source(bHasTask?TaskText(Task):TEXT("N/A"))).ToString();''')
p.write_text(s)
p=root/'Video/VideoShellWidget.cpp';s=p.read_text();a=s.index('    return FString::Printf(TEXT("UAV-%02d%s');b=s.index('\n}',a)
s=s[:a]+'''    return FText::Format(ProductText::Get(TEXT("Video.Feed")),FText::AsCultureInvariant(FString::Printf(TEXT("UAV-%02d"),Id)),ProductText::Source(Link),ProductText::Source(D.VideoUrl.IsEmpty()?TEXT("NO SOURCE"):TEXT("SOURCE SET")),Secondary?ProductText::Source(TEXT("VIDEO SLOT\\nNO ACTIVE STREAM")):FText::GetEmpty()).ToString();'''+s[b:]
a=s.index('    Heading->SetText(',s.index('void UVideoShellWidget::Refresh'));b=s.index('\n',a)
s=s[:a]+'''    Heading->SetText(FText::Format(ProductText::Get(TEXT("Video.Heading")),bValid?FText::AsCultureInvariant(FString::Printf(TEXT("UAV-%02d"),Id)):ProductText::Source(TEXT("NO TARGET"))));'''+s[b:]
a=s.index('    Details->SetText(');b=s.index('\n',s.index('*MediaState',a))
s=s[:a]+'''    Details->SetText(FText::Format(ProductText::Get(TEXT("Video.Details")),FText::AsCultureInvariant(DisplayedDroneId>0?FString::Printf(TEXT("UAV-%02d"),DisplayedDroneId):TEXT("")),FText::AsCultureInvariant(Resolution),FText::AsCultureInvariant(Rate),ProductText::Source(MediaState)));'''+s[b:]
p.write_text(s)
p=root/'Shared/OperationalContextSubsystem.cpp';s=p.read_text();s='#include "Shared/ProductText.h"\n'+s;a=s.index('    return FString::Printf(TEXT("BACKEND');b=s.index('\n}',a)
s=s[:a]+'''    return FText::Format(ProductText::Get(TEXT("Common.Sync")),ProductText::Source(IsReady()?TEXT("ONLINE"):TEXT("OFFLINE")),ProductText::Source(State),FText::AsCultureInvariant(Id),FText::AsNumber(Version)).ToString();'''+s[b:];p.write_text(s)
p=root/'Shared/OperationalEventStore.h';s=p.read_text();s=s.replace('FString Source, EventType, TargetId, Message;','FString Source, EventType, TargetId, Message;\n    TSharedPtr<FJsonObject> Params;\n    FText DisplayText() const;');s=s.replace('FDelegateHandle ContextHandle;','FDelegateHandle ContextHandle,LanguageHandle;');p.write_text(s)
p=root/'Shared/OperationalEventStore.cpp';s=p.read_text();s='#include "Shared/ProductText.h"\n#include "Shared/UILanguageSubsystem.h"\n'+s
s=s.replace('    if(Sync->GetContext())','    LanguageHandle=GetGameInstance()->GetSubsystem<UUILanguageSubsystem>()->OnLanguageChanged.AddWeakLambda(this,[this]{OnChanged.Broadcast();});\n    if(Sync->GetContext())')
s=s.replace('    Super::Deinitialize();','    GetGameInstance()->GetSubsystem<UUILanguageSubsystem>()->OnLanguageChanged.Remove(LanguageHandle);\n    Super::Deinitialize();')
s=s.replace('        if(!Events.IsEmpty())Events.Last().Timestamp=', '        const TSharedPtr<FJsonObject>* Params;if(!Events.IsEmpty() && E->TryGetObjectField(TEXT("params"),Params))Events.Last().Params=*Params;\n        if(!Events.IsEmpty())Events.Last().Timestamp=')
s+='''
FText FOperationalEvent::DisplayText() const {
    if(!Params)return FText::AsCultureInvariant(Message);
    FFormatNamedArguments Args;for(const auto& E:Params->Values){FString Value;if(E.Value->TryGetString(Value))Args.Add(FString(E.Key.ToView()),FText::AsCultureInvariant(Value));else if(E.Value->Type==EJson::Number)Args.Add(FString(E.Key.ToView()),FText::AsNumber(E.Value->AsNumber()));}
    return FText::Format(ProductText::Get(TEXT("Events.")+EventType),Args);
}
''';p.write_text(s)
p=root/'Command/OperationalLogWidget.cpp';s=p.read_text();s=s.replace('*Category,*E.Message','*ProductText::Source(Category).ToString(),*E.DisplayText().ToString()')
a=s.index('    Counts->SetText(');b=s.index('\n',a);s=s[:a]+'''    Counts->SetText(FText::Format(ProductText::Get(TEXT("Log.Count")),FText::AsNumber(Critical),FText::AsNumber(Warning)));'''+s[b:];p.write_text(s)
p=root/'UE5DroneControl.cpp';s=p.read_text();s='#include "Misc/ConfigCacheIni.h"\n#include "Misc/Paths.h"\n'+s;s=s.replace('        FDefaultGameModuleImpl::StartupModule();','''        FDefaultGameModuleImpl::StartupModule();
        // UE5.8 GatherText schedules preloaded config branches. Load the product target explicitly.
        if(IsRunningCommandlet() && GConfig)GConfig->LoadFile(FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir()/TEXT("Localization/DroneOps.ini")));''');p.write_text(s)
p=Path('Scripts/P4/localization.json');d=json.loads(p.read_text(encoding='utf-8-sig'));d['Common.Sync']=['BACKEND {0} | SYNC {1} | ACTIVE {2} | v{3}','后端 {0} | 同步 {1} | 当前 {2} | v{3}'];d['Common.Connected']=['CONNECTED','已连接'];d['Common.Disconnected']=['DISCONNECTED','已断开'];d['Common.Reconnecting']=['RECONNECTING','重连中'];p.write_text(json.dumps(d,ensure_ascii=False,indent=2),encoding='utf-8')
