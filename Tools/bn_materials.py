"""Creates (or rebuilds) BLACK NUMBERS surface materials and physical materials under /Game/World.

No external textures: one master material tiles the engine's own noise texture in world space,
so huge road slabs never stretch. Physical material friction feeds Chaos tyre grip directly
(UChaosWheeledVehicleSimulation::ApplyWheelFrictionForces -> SetSurfaceFriction).
"""
import unreal

ROOT = "/Game/World"
MAT_DIR = ROOT + "/Materials"
PHYS_DIR = ROOT + "/Physics"
MASTER = "M_BN_Surface"
NOISE = "/Engine/EngineMaterials/Good64x64TilingNoiseHighFreq.Good64x64TilingNoiseHighFreq"

# name: friction. Tuned relative to asphalt = 1.0; the vehicle drive test runs on asphalt.
PHYSICAL = {
    "PM_Asphalt": 1.0,
    "PM_Concrete": 0.85,
    "PM_Dirt": 0.6,
    "PM_Grass": 0.45,
}

# name: (base colour, roughness, noise tiles per metre, noise strength, emissive)
SURFACES = {
    "MI_Asphalt": ((0.045, 0.045, 0.05), 0.85, 1.6, 0.55, None),
    "MI_Concrete": ((0.32, 0.31, 0.29), 0.9, 0.8, 0.35, None),
    "MI_Dirt": ((0.22, 0.15, 0.09), 0.95, 0.9, 0.6, None),
    "MI_Grass": ((0.07, 0.13, 0.04), 0.95, 1.2, 0.6, None),
    "MI_LineWhite": ((0.75, 0.75, 0.72), 0.6, 2.0, 0.2, None),
    "MI_LineYellow": ((0.8, 0.55, 0.05), 0.6, 2.0, 0.2, None),
    "MI_Building_Brick": ((0.26, 0.1, 0.07), 0.9, 0.5, 0.35, None),
    "MI_Building_Plaster": ((0.45, 0.4, 0.32), 0.9, 0.5, 0.25, None),
    "MI_Building_Steel": ((0.16, 0.18, 0.2), 0.7, 0.5, 0.3, None),
    "MI_Metal_Dark": ((0.03, 0.03, 0.03), 0.5, 1.0, 0.2, None),
    "MI_Barrier_Red": ((0.5, 0.04, 0.03), 0.7, 1.0, 0.2, None),
    "MI_LampGlow": ((1.0, 0.6, 0.25), 0.5, 1.0, 0.0, (40.0, 18.0, 5.0)),
}

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def _load_or_create(name, path, cls, factory):
    full = "%s/%s" % (path, name)
    if assets.does_asset_exist(full):
        return unreal.load_asset(full)
    asset = tools.create_asset(name, path, cls, factory)
    if asset is None:
        raise RuntimeError("Could not create " + full)
    return asset


def _sampler_for(texture):
    """The sampler type must match the texture's compression or the material fails to compile."""
    compression = texture.get_editor_property("compression_settings")
    srgb = texture.get_editor_property("srgb")
    if compression == unreal.TextureCompressionSettings.TC_GRAYSCALE:
        return unreal.MaterialSamplerType.SAMPLERTYPE_GRAYSCALE if srgb else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE
    if compression == unreal.TextureCompressionSettings.TC_MASKS:
        return unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
    if compression == unreal.TextureCompressionSettings.TC_ALPHA:
        return unreal.MaterialSamplerType.SAMPLERTYPE_ALPHA
    return unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if srgb else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR


def _noise_sample(mat, uv, scale_factor, x):
    """World-XY noise sample; scale_factor multiplies the NoiseScale parameter (two octaves)."""
    texture = unreal.load_asset(NOISE)
    scaled = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, x, 0)
    scaled.set_editor_property("const_b", scale_factor)
    mel.connect_material_expressions(uv, "", scaled, "A")
    sample = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, x + 200, 0)
    sample.set_editor_property("texture", texture)
    sample.set_editor_property("sampler_type", _sampler_for(texture))
    mel.connect_material_expressions(scaled, "", sample, "UVs")
    return sample


def _build_master():
    mat = _load_or_create(MASTER, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(mat)

    def param(cls, name, default, y):
        p = mel.create_material_expression(mat, cls, -1200, y)
        p.set_editor_property("parameter_name", name)
        p.set_editor_property("default_value", default)
        return p

    base = param(unreal.MaterialExpressionVectorParameter, "BaseColor", unreal.LinearColor(0.5, 0.5, 0.5, 1), -300)
    rough = param(unreal.MaterialExpressionScalarParameter, "Roughness", 0.8, -100)
    tiles = param(unreal.MaterialExpressionScalarParameter, "NoiseTilesPerMetre", 1.0, 100)
    strength = param(unreal.MaterialExpressionScalarParameter, "NoiseStrength", 0.4, 300)
    emissive = param(unreal.MaterialExpressionVectorParameter, "Emissive", unreal.LinearColor(0, 0, 0, 1), 500)

    # UV = world XY (cm) / 100 * tiles-per-metre
    world = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1200, 700)
    xy = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -1000, 700)
    xy.set_editor_property("r", True)
    xy.set_editor_property("g", True)
    mel.connect_material_expressions(world, "", xy, "")
    per_metre = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -800, 700)
    per_metre.set_editor_property("const_b", 0.01)
    mel.connect_material_expressions(xy, "", per_metre, "A")
    uv = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -600, 700)
    mel.connect_material_expressions(per_metre, "", uv, "A")
    mel.connect_material_expressions(tiles, "", uv, "B")

    fine = _noise_sample(mat, uv, 1.0, -400)
    coarse = _noise_sample(mat, uv, 0.08, -400)
    noise = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 0, 0)
    mel.connect_material_expressions(fine, "R", noise, "A")
    mel.connect_material_expressions(coarse, "R", noise, "B")

    # colour * lerp(1 - strength, 1, noise)
    dark = mel.create_material_expression(mat, unreal.MaterialExpressionOneMinus, 0, 300)
    mel.connect_material_expressions(strength, "", dark, "")
    shade = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, 200, 200)
    shade.set_editor_property("const_b", 1.0)
    mel.connect_material_expressions(dark, "", shade, "A")
    mel.connect_material_expressions(noise, "", shade, "Alpha")
    colour = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 400, -200)
    mel.connect_material_expressions(base, "", colour, "A")
    mel.connect_material_expressions(shade, "", colour, "B")

    mel.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    return mat


def build():
    """Returns ({instance name: MaterialInstanceConstant}, {physical name: PhysicalMaterial})."""
    physical = {}
    for name, friction in PHYSICAL.items():
        pm = _load_or_create(name, PHYS_DIR, unreal.PhysicalMaterial, unreal.PhysicalMaterialFactoryNew())
        pm.set_editor_property("friction", friction)
        physical[name] = pm

    master = _build_master()
    instances = {}
    for name, (colour, roughness, tiles, strength, glow) in SURFACES.items():
        mi = _load_or_create(name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, master)
        mel.set_material_instance_vector_parameter_value(mi, "BaseColor", unreal.LinearColor(*colour, 1.0))
        mel.set_material_instance_scalar_parameter_value(mi, "Roughness", roughness)
        mel.set_material_instance_scalar_parameter_value(mi, "NoiseTilesPerMetre", tiles)
        mel.set_material_instance_scalar_parameter_value(mi, "NoiseStrength", strength)
        if glow:
            mel.set_material_instance_vector_parameter_value(mi, "Emissive", unreal.LinearColor(*glow, 1.0))
        mel.update_material_instance(mi)
        instances[name] = mi

    if not assets.save_directory(ROOT, only_if_is_dirty=False, recursive=True):
        raise RuntimeError("Could not save " + ROOT)
    return instances, physical
