"""Explicit synthetic QA map: keeps georeference/UI, replaces unavailable online tiles.
Never changes the production map or credential-free Cesium server asset.
"""
import unreal

target='/Game/Tests/P55OfflineMap'
assert not unreal.EditorAssetLibrary.does_asset_exist(target)
assert unreal.EditorAssetLibrary.duplicate_asset('/Game/Level/CesiumWorld',target)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level(target)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for a in actors.get_all_level_actors():
    if isinstance(a,unreal.Cesium3DTileset):
        actors.destroy_actor(a)
geo=next(a for a in actors.get_all_level_actors() if isinstance(a,unreal.CesiumGeoreference))
origin=geo.transform_longitude_latitude_height_position_to_unreal(unreal.Vector(116.3466,39.9815,0))
lib=unreal.MaterialEditingLibrary
for index,(name,color) in enumerate([('Bright',(.72,.72,.65)),('White',(.97,.97,.97)),('Dark',(.035,.06,.08))]):
    m=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_P55QA'+name,'/Game/Tests',unreal.Material,unreal.MaterialFactoryNew())
    m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    c=lib.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);c.set_editor_property('constant',unreal.LinearColor(*color,1))
    e=lib.create_material_expression(m,unreal.MaterialExpressionEyeAdaptation)
    div=lib.create_material_expression(m,unreal.MaterialExpressionDivide)
    lib.connect_material_expressions(c,'',div,'A');lib.connect_material_expressions(e,'',div,'B')
    lib.connect_material_property(div,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);lib.recompile_material(m)
    assert unreal.EditorAssetLibrary.save_loaded_asset(m)
    a=actors.spawn_actor_from_class(unreal.StaticMeshActor,origin+unreal.Vector((index-1)*40000,0,-50))
    a.set_actor_label('P55 SYNTHETIC '+name+' surface')
    mesh=a.static_mesh_component;mesh.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
    mesh.set_material(0,m);a.set_actor_scale3d(unreal.Vector(400,1600,1))
assert levels.save_current_level()
unreal.log('P55_OFFLINE_FIXTURE_SAVED: synthetic surfaces, no satellite imagery, original map untouched')
