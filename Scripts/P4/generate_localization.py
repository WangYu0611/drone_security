import json
from pathlib import Path
data=json.loads(Path('Scripts/P4/localization.json').read_text(encoding='utf-8-sig'))
quote=lambda s:json.dumps(s,ensure_ascii=False)
cpp=['#include "Shared/ProductText.h"','namespace {','const TMap<FString,FText>& Catalog(){static const TMap<FString,FText> C={']
for key,(en,zh) in data.items():cpp.append('{TEXT('+quote(key)+'),NSLOCTEXT("DroneOps",'+quote(key)+','+quote(en)+')},')
cpp+=['};return C;}','}','FText ProductText::Get(const FString& Key){if(const auto* T=Catalog().Find(Key))return *T;return NSLOCTEXT("DroneOps","Errors.Request","Request failed. Confirm the current selection and retry.");}',
 'FText ProductText::Source(const FString& Source){']
for key,(en,zh) in data.items():cpp.append('if(Source==TEXT('+quote(en)+'))return Get(TEXT('+quote(key)+'));')
cpp+=['return FText::AsCultureInvariant(Source);','}']
aliases=Path('Scripts/P4/source_aliases.json')
if aliases.exists():
    cpp[-2:-2]=['if(Source==TEXT('+quote(source)+'))return Get(TEXT('+quote(key)+'));' for source,key in json.loads(aliases.read_text(encoding='utf-8-sig')).items()]
Path('Source/UE5DroneControl/Shared/ProductText.cpp').write_text('\n'.join(cpp),encoding='utf-8')
root=Path('Content/Localization/DroneOps');root.mkdir(parents=True,exist_ok=True)
for lang in ['en','zh-Hans']:
    d=root/lang;d.mkdir(exist_ok=True)
    lines=['msgid ""','msgstr ""',quote('Content-Type: text/plain; charset=UTF-8\n'),quote('Language: '+lang+'\n'),'']
    for key,(en,zh) in data.items():lines+=['msgctxt '+quote('DroneOps,'+key),'msgid '+quote(en),'msgstr '+quote(en if lang=='en' else zh),'']
    (d/'DroneOps.po').write_text('\n'.join(lines),encoding='utf-8')
config=Path('Config/Localization');config.mkdir(exist_ok=True)
(config/'DroneOps.ini').write_text('''[CommonSettings]
SourcePath=Content/Localization/DroneOps
DestinationPath=Content/Localization/DroneOps
ManifestName=DroneOps.manifest
ArchiveName=DroneOps.archive
PortableObjectName=DroneOps.po
NativeCulture=en
CulturesToGenerate=en
CulturesToGenerate=zh-Hans

[GatherTextStep0]
CommandletClass=GatherTextFromSource
SearchDirectoryPaths=Source/UE5DroneControl/Shared
FileNameFilters=ProductText.cpp

[GatherTextStep1]
CommandletClass=GenerateGatherManifest

[GatherTextStep2]
CommandletClass=GenerateGatherArchive

[GatherTextStep3]
CommandletClass=InternationalizationExport
bImportLoc=true

[GatherTextStep4]
CommandletClass=GenerateTextLocalizationResource
ResourceName=DroneOps.locres
''',encoding='utf-8')
