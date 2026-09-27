"""UE Python commandlet. Creates only the new, depth-tested Route V2 material."""
import unreal

path = '/Game/Command/Materials/M_CommandPathV2'
assert not unreal.EditorAssetLibrary.does_asset_exist(path), 'Asset already exists; use update_route_material.py for shader-only changes'
m = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_CommandPathV2', '/Game/Command/Materials', unreal.Material, unreal.MaterialFactoryNew())
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property('two_sided', True)
m.set_editor_property('disable_depth_test', False)
m.set_editor_property('use_translucency_vertex_fog', False)
m.set_editor_property('used_with_spline_meshes', True)
lib = unreal.MaterialEditingLibrary
nodes = {}
for name, value in {'StartColor':(.015,.09,.85), 'EndColor':(1,.12,.015), 'HaloColor':(0,.8,1), 'StartWorld':(0,0,0), 'EndWorld':(100,0,0)}.items():
    n=lib.create_material_expression(m,unreal.MaterialExpressionVectorParameter)
    n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',unreal.LinearColor(*value,1));nodes[name]=n
for name,value in {'FlowRate':1,'FlowStrength':1,'FlowDensity':6,'IsHalo':0,'ConflictStrength':0,'SelectionStrength':0,'Completed':0,'IsRing':0}.items():
    n=lib.create_material_expression(m,unreal.MaterialExpressionScalarParameter)
    n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',value);nodes[name]=n
nodes['World']=lib.create_material_expression(m,unreal.MaterialExpressionWorldPosition)
nodes['Rim']=lib.create_material_expression(m,unreal.MaterialExpressionFresnel)
nodes['Clock']=lib.create_material_expression(m,unreal.MaterialExpressionTime)
nodes['Exposure']=lib.create_material_expression(m,unreal.MaterialExpressionEyeAdaptation)
custom=lib.create_material_expression(m,unreal.MaterialExpressionCustom)
custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
inputs=[]
for name in nodes:
    i=unreal.CustomInput();i.set_editor_property('input_name',name);inputs.append(i)
custom.set_editor_property('inputs',inputs)
custom.set_editor_property('code', '''
float3 delta = EndWorld.rgb - StartWorld.rgb;
float u = saturate(dot(World - StartWorld.rgb, delta) / max(dot(delta,delta),0.001));
// Phase travels only from Start to End; independent of cylinder UV orientation.
float phase = frac(u * FlowDensity - Clock * FlowRate);
float pulse = smoothstep(0.65,0.82,phase) * (1-smoothstep(0.88,1.0,phase));
float alarm = 0.5 + 0.5*sin(Clock*12.56637);
float3 base = lerp(StartColor.rgb,EndColor.rgb,u);
base = lerp(base,base*0.55,Completed);
float3 core = base*(0.45+0.25*SelectionStrength+pulse*FlowStrength*0.75);
// A dark silhouette preserves continuity on bright terrain without whitening altitude hues.
core = lerp(core,float3(0.008,0.012,0.02),smoothstep(0.2,0.65,Rim)*0.8*(1-IsRing));
core = lerp(core,lerp(float3(1,0,0),float3(1,0.8,0.8),alarm*0.65),ConflictStrength);
float3 halo = lerp(HaloColor.rgb, float3(1,0,0)*(0.55+alarm*0.65),ConflictStrength);
float edge = smoothstep(0.2,0.85,Rim);
halo = lerp(halo,float3(0.015,0.02,0.03),edge*0.85);
float3 rgb = lerp(core,halo,IsHalo);
float alpha = lerp(0.85+0.15*SelectionStrength,edge*(0.45+0.25*SelectionStrength),IsHalo);
alpha = lerp(alpha,smoothstep(0.12,0.6,Rim)*0.9,IsRing);
return float4(rgb/max(Exposure,0.00000001),alpha);
''')
for name,n in nodes.items():
    assert lib.connect_material_expressions(n,'',custom,name)
rgb=lib.create_material_expression(m,unreal.MaterialExpressionComponentMask)
for c in ['r','g','b']:rgb.set_editor_property(c,True)
rgb.set_editor_property('a',False)
alpha=lib.create_material_expression(m,unreal.MaterialExpressionComponentMask)
for c in ['r','g','b']:alpha.set_editor_property(c,False)
alpha.set_editor_property('a',True)
assert lib.connect_material_expressions(custom,'',rgb,'')
assert lib.connect_material_expressions(custom,'',alpha,'')
assert lib.connect_material_property(rgb,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
assert lib.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY)
lib.layout_material_expressions(m);lib.recompile_material(m)
assert unreal.EditorAssetLibrary.save_loaded_asset(m,only_if_is_dirty=False)
unreal.log('P55_MATERIAL_CREATED_DEPTH_TEST_ENABLED')
