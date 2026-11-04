"""
Border Run - builds the blood puddle decal material for you.

How to run (in the Unreal editor):  Tools -> Execute Python Script...  -> pick this file.
It creates  /Game/FX/M_BloodPool  (dark red, wet-looking, soft splattered edge).
Then: Project Settings -> Border Run -> Effects -> Blood Pool Decal = M_BloodPool.
"""
import unreal

FOLDER = "/Game/FX"
NAME = "M_BloodPool"
FULL = FOLDER + "/" + NAME

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary

if not eal.does_directory_exist(FOLDER):
    eal.make_directory(FOLDER)
if eal.does_asset_exist(FULL):
    eal.delete_asset(FULL)

tools = unreal.AssetToolsHelpers.get_asset_tools()
mat = tools.create_asset(NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())

# A decal that's see-through around the edges.
mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

# Colour: very dark red.
color = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -600, -250)
color.set_editor_property("constant", unreal.LinearColor(0.15, 0.0, 0.0, 1.0))
mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)

# Wet shine.
rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -600, -50)
rough.set_editor_property("r", 0.15)
mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

# Round, soft puddle shape.
grad = mel.create_material_expression(mat, unreal.MaterialExpressionMaterialFunctionCall, -650, 150)
grad.set_editor_property("material_function",
    unreal.load_asset("/Engine/Functions/Engine_MaterialFunctions01/Gradient/RadialGradientExponential"))
radius = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -950, 170)
radius.set_editor_property("r", 0.45)
density = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -950, 260)
density.set_editor_property("r", 3.0)
mel.connect_material_expressions(radius, "", grad, "Radius")
mel.connect_material_expressions(density, "", grad, "Density")

# Splattered, uneven edge (noise), multiplied into the shape.
noise = mel.create_material_expression(mat, unreal.MaterialExpressionNoise, -650, 400)
noise.set_editor_property("scale", 0.04)
noise.set_editor_property("output_min", 0.35)
noise.set_editor_property("output_max", 1.25)
mult = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -350, 250)
mel.connect_material_expressions(grad, "", mult, "A")
mel.connect_material_expressions(noise, "", mult, "B")
mel.connect_material_property(mult, "", unreal.MaterialProperty.MP_OPACITY)

mel.recompile_material(mat)
eal.save_asset(FULL)
unreal.log("Border Run: made " + FULL + " - now set Project Settings -> Border Run -> Effects -> Blood Pool Decal to it.")
