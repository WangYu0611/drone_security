from pathlib import Path
for file in ['Source/UE5DroneControl/Command/CommandCenterPanel.cpp','Source/UE5DroneControl/Command/CommandCenterPanel.h']:
 p=Path(file);s=p.read_text(encoding='utf-8-sig').replace('USecurityPlanPanel','USecurityPlanWorkspaceWidget').replace('Shared/SecurityPlanPanel.h','Shared/SecurityPlanWorkspaceWidget.h');p.write_text(s,encoding='utf-8')
for file in ['Source/UE5DroneControl/Map/MapShellWidget.cpp','Source/UE5DroneControl/Map/MapShellWidget.h']:
 p=Path(file);s=p.read_text(encoding='utf-8-sig').replace('USecurityPlanPanel','UMapMissionRouteWidget').replace('Shared/SecurityPlanPanel.h','Map/MapMissionRouteWidget.h');p.write_text(s,encoding='utf-8')
p=Path('Source/UE5DroneControl/Map/MapShellWidget.cpp');s=p.read_text(encoding='utf-8-sig')
a=s.index('    Button(TEXT("plan"), TEXT("Plan"));');b=s.index('    ActionStatus =',a)
s=s[:a]+'''    Button(TEXT("security_plan"), TEXT("MISSION ROUTE"));
    Button(TEXT("open_video"), TEXT("OPEN VIDEO"));
    Button(TEXT("zh-Hans"), TEXT("中文"));Button(TEXT("en"),TEXT("English"));
'''+s[b:]
s=s.replace('''    if(Action==TEXT("security_plan"))''','''    if(Action==TEXT("en") || Action==TEXT("zh-Hans")){GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->SetLanguage(Action.ToString());return;}
    if(Action==TEXT("open_video")){const int Id=Manager->GetRegistry()->GetPrimarySelectedDrone();if(Id>0)GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->OpenVideo(FString::Printf(TEXT("UAV-%02d"),Id));return;}
    if(Action==TEXT("security_plan"))''')
p.write_text(s,encoding='utf-8')
p=Path('Source/UE5DroneControl/Video/VideoShellWidget.h');s=p.read_text(encoding='utf-8-sig')
s=s.replace('    int32 GetViewMode() const', '    int32 GetBrowserGeneration() const {return Generation;}\n    int32 GetViewMode() const')
s=s.replace('    UFUNCTION() void ToggleFollow()', '    UFUNCTION() void Chinese();\n    UFUNCTION() void English();\n    UFUNCTION() void ToggleFollow()')
p.write_text(s,encoding='utf-8')
p=Path('Source/UE5DroneControl/Video/VideoShellWidget.cpp');s=p.read_text(encoding='utf-8-sig')
s=s.replace('''        FollowButton=Action(TEXT("FOLLOW TARGET"),true); FollowButton->OnClicked.AddDynamic(this,&UVideoShellWidget::ToggleFollow);
        FollowLabel=Cast<UTextBlock>(FollowButton->GetContent());''','''        auto* Zh=Action(TEXT("中文"),true);Zh->OnClicked.AddDynamic(this,&UVideoShellWidget::Chinese);
        auto* En=Action(TEXT("English"),true);En->OnClicked.AddDynamic(this,&UVideoShellWidget::English);''')
a=s.index('    LocalSourceId = Id;',s.index('void UVideoShellWidget::SelectDrone'));b=s.index('    Refresh();',a)
s=s[:a]+'''    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    if(Sync->IsEnabled())Sync->OpenVideo(FString::Printf(TEXT("UAV-%02d"),Id));
    else LocalSourceId=Id;
'''+s[b:]
a=s.index('    int32 Id = ContextSync->IsEnabled()');b=s.index('    FDroneDescriptor Selected;',a)
s=s[:a]+'''    int32 Id=LocalSourceId;
    if(ContextSync->IsEnabled()) {
        FString Target;if(ContextSync->GetVideoView())ContextSync->GetVideoView()->TryGetStringField(TEXT("video_target_uav_id"),Target);
        Id=0;if(Target.StartsWith(TEXT("UAV-")))LexTryParseString(Id,*Target.Mid(4));
    }
'''+s[b:]
a=s.index('    if (!bValid && !Drones.IsEmpty() && !bFollowTarget)');b=s.index('    if (!bValid) Id=0;',a);s=s[:a]+s[b:]
a=s.index('            SystemHeader->SetText(FText::FromString(FString::Printf(TEXT("%s  |  CURRENT VIDEO:');b=s.index('    const double Now',a)
s=s[:a]+'''            SystemHeader->SetText(FText::Format(ProductText::Get(TEXT("Video.Header")),FText::AsCultureInvariant(Sync->GetStatusText()),FText::AsNumber(DisplayedDroneId),FText::AsCultureInvariant(Global)));
        }
'''+s[b:]
s=s.replace('void UVideoShellWidget::SetFollowTarget(bool Enabled) { bFollowTarget=Enabled; Refresh(); }','void UVideoShellWidget::SetFollowTarget(bool Enabled) { bFollowTarget=false; }\nvoid UVideoShellWidget::Chinese(){GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->SetLanguage(TEXT("zh-Hans"));}\nvoid UVideoShellWidget::English(){GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->SetLanguage(TEXT("en"));}')
s=s.replace('MediaState = Url.IsEmpty() ? TEXT("OFFLINE / NO SOURCE") : TEXT("CONNECTING");','MediaState = Id<=0?TEXT("NO TARGET"):Url.IsEmpty()?TEXT("NO SOURCE"):TEXT("CONNECTING");')
s='#include "Shared/ProductText.h"\n'+s;p.write_text(s,encoding='utf-8')
p=Path('Source/UE5DroneControl/Command/CommandAlarmPanel.cpp');s=p.read_text(encoding='utf-8-sig')
s=s.replace('        AddButton(TEXT("focus"),', '        AddButton(TEXT("open_video"), TEXT("OPEN VIDEO"), Alert.DroneId > 0);\n        AddButton(TEXT("focus"),')
s=s.replace('''    if (Action == TEXT("focus"))''','''    if(Action==TEXT("open_video") && Store.IsValid()){for(const auto& Alert:Store->GetAlerts())if(Alert.Id==Id){GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->OpenVideo(FString::Printf(TEXT("UAV-%02d"),Alert.DroneId));break;}return;}
    if (Action == TEXT("focus"))''')
p.write_text(s,encoding='utf-8')
