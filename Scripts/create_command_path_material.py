"""Run once outside PIE in the UE Editor Python console to create the A3 display asset."""
import unreal

asset_path = "/Game/Command/Materials/M_CommandPath"
assert not unreal.EditorAssetLibrary.does_asset_exist(asset_path), "Asset already exists; do not overwrite"
material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    "M_CommandPath", "/Game/Command/Materials", unreal.Material, unreal.MaterialFactoryNew())
material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
material.set_editor_property("used_with_spline_meshes", True)
library = unreal.MaterialEditingLibrary
color = library.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -500, 0)
color.set_editor_property("parameter_name", "Color")
color.set_editor_property("default_value", unreal.LinearColor(0.15, 0.60, 0.75, 1.0))
exposure = library.create_material_expression(material, unreal.MaterialExpressionEyeAdaptation, -500, 180)
divide = library.create_material_expression(material, unreal.MaterialExpressionDivide, -220, 0)
assert library.connect_material_expressions(color, "", divide, "A")
assert library.connect_material_expressions(exposure, "", divide, "B")
assert library.connect_material_property(divide, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
library.recompile_material(material)
assert unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
unreal.log("TASK-A3: saved exposure-compensated spline display material " + asset_path)
