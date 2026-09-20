import unreal,json
reg=unreal.AssetRegistryHelpers.get_asset_registry()
reg.search_all_assets(True)
opts=unreal.AssetRegistryDependencyOptions(True,True,True,True,True)
rows=[]
media=[]
for a in reg.get_assets_by_path('/Game',recursive=True):
 p=str(a.package_name); c=str(a.asset_class_path.asset_name)
 if any(x.lower() in c.lower() for x in ['Media','RenderTarget','Material']): media.append({'path':p,'class':c})
 if any(x in p for x in ['Video','DroneInfoPanel','DroneOpsHUD','BP_DroneOpsPlayerController','CesiumWorld','MainMenu']) or 'Media' in c:
  row={'path':p,'class':c,'dependencies':[str(x) for x in reg.get_dependencies(a.package_name,opts)],'referencers':[str(x) for x in reg.get_referencers(a.package_name,opts)]}
  if c in ['WidgetBlueprint','Blueprint']:
   try:
    cls=unreal.EditorAssetLibrary.load_blueprint_class(p)
    row['generated_class']=str(cls)
    obj=unreal.get_default_object(cls)
    row['defaults']={}
    for n in ['drone_ops_hud_widget_class','drone_video_window_widget_class','box_select_widget_class','player_controller_class']:
     try: row['defaults'][n]=str(obj.get_editor_property(n))
     except: pass
   except Exception as e: row['error']=str(e)
  rows.append(row)
with open(unreal.Paths.project_saved_dir()+'VideoQA/assets.json','w',encoding='utf-8') as f:json.dump({'assets':rows,'media_material_render_targets':media},f,ensure_ascii=False,indent=2)
unreal.log('TASK-V1 ASSET AUDIT COMPLETE')
