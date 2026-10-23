"""Read the actual merged UE settings; never access or modify the tile database."""
import json
from pathlib import Path
import unreal

out=Path(r'Saved\CesiumCacheCapacity\settings.json')
out.parent.mkdir(parents=True,exist_ok=True)
cls=unreal.load_class(None,'/Script/CesiumRuntime.CesiumRuntimeSettings')
assert cls is not None,'Cesium runtime settings class must load'
settings=unreal.get_default_object(cls)
result={'scope':'Read-only reflected Cesium CDO in fresh commandlet, no map/Backend/cache database access',
        'class':cls.get_path_name(),'object':settings.get_path_name(),
        'max_cache_items':settings.get_editor_property('max_cache_items'),
        'requests_per_cache_prune':settings.get_editor_property('requests_per_cache_prune'),
        'project_dir':unreal.Paths.project_dir(),
        'engine_version':unreal.SystemLibrary.get_engine_version()}
result['pass']=result['max_cache_items']==131072 and result['requests_per_cache_prune']==10000
out.write_text(json.dumps(result,indent=2),encoding='utf-8')
unreal.log('CESIUM_CACHE_CAPACITY '+json.dumps(result))
assert result['pass'],'Actual merged cache settings differ from expected production values'
