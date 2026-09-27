"""Update only the existing custom shader and tactical fog policy; preserve graph."""
import unreal, ast
from pathlib import Path
source=Path(__file__).with_name('create_route_material.py').read_text()
code=next(ast.literal_eval(n.args[1]) for n in ast.walk(ast.parse(source)) if isinstance(n,ast.Call) and isinstance(n.func,ast.Attribute) and n.func.attr=='set_editor_property' and len(n.args)>1 and isinstance(n.args[0],ast.Constant) and n.args[0].value=='code')
m=unreal.load_asset('/Game/Command/Materials/M_CommandPathV2')
custom=[n for n in unreal.MaterialEditingLibrary.get_material_expressions(m) if isinstance(n,unreal.MaterialExpressionCustom)]
assert len(custom)==1
custom[0].set_editor_property('code',code)
m.set_editor_property('use_translucency_vertex_fog',False)
assert not m.get_editor_property('disable_depth_test')
unreal.MaterialEditingLibrary.recompile_material(m)
assert unreal.EditorAssetLibrary.save_loaded_asset(m,only_if_is_dirty=False)
unreal.log('P55_SHADER_UPDATED_WITH_DEPTH_TEST')
