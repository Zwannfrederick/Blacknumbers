"""Special-purpose master materials & MaterialParameterCollection for BLACK NUMBERS:
- MPC_BN_World (scalar NightFactor 0..1 updated by ABNDayNightCycle)
- M_BN_Foliage (Two-Sided Masked leaf/pine clusters using Mesh UV0)
- M_BN_Water (Dual-layer scrolling normal waves for harbor & open sea)
- M_BN_Particle_Translucent & M_BN_Particle_Additive (soft alpha VFX & road markings)
- M_BN_RaceHolo (pulsing additive holographic race start rings & checkpoint chevrons)
- M_BN_MeshUV (Mesh UV0 PBR + MPC NightFactor emissive for Turkish signs, storefronts, vehicles)
"""
import unreal


MPC_NAME = "MPC_BN_World"


def ensure_world_mpc(mat_dir, load_or_create):
    """Creates/updates /Game/World/Materials/MPC_BN_World with scalar NightFactor (default 0.0)."""
    mpc = load_or_create(
        MPC_NAME,
        mat_dir,
        unreal.MaterialParameterCollection,
        unreal.MaterialParameterCollectionFactoryNew(),
    )
    existing = [str(n) for n in mpc.get_scalar_parameter_names()]
    if "NightFactor" not in existing:
        param = unreal.CollectionScalarParameter()
        try:
            param.set_editor_property("parameter_name", "NightFactor")
            param.set_editor_property("default_value", 0.0)
        except Exception:
            param.import_text('(DefaultValue=0.0,ParameterName="NightFactor")')
        params = list(mpc.get_editor_property("scalar_parameters"))
        params.append(param)
        mpc.set_editor_property("scalar_parameters", params)
    return mpc


def add_night_factor_node(mat, mel, mpc, x, y):
    """Creates a CollectionParameter node reading MPC_BN_World.NightFactor."""
    node = mel.create_material_expression(mat, unreal.MaterialExpressionCollectionParameter, x, y)
    node.set_editor_property("collection", mpc)
    node.set_editor_property("parameter_name", "NightFactor")
    return node


def _build_foliage_master(mat_dir, textures, h):
    mel = unreal.MaterialEditingLibrary
    mat = h["load_or_create"]("M_BN_Foliage", mat_dir, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(mat)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    mat.set_editor_property("opacity_mask_clip_value", 0.42)
    try:
        mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    except Exception:
        pass

    uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -760, 0)
    base_col = h["param"](mat, unreal.MaterialExpressionVectorParameter, "BaseColor", unreal.LinearColor(1, 1, 1, 1), -420, -320)
    rough = h["param"](mat, unreal.MaterialExpressionScalarParameter, "Roughness", 0.64, 80, 140)
    spec = h["param"](mat, unreal.MaterialExpressionScalarParameter, "Specular", 0.35, 80, 40)

    alb = h["tex_param"](mat, "AlbedoMap", textures["T_LeafCluster_D"], unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, uv, -460, -140)
    msk = h["tex_param"](mat, "MaskMap", textures["T_LeafCluster_A"], unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, uv, -460, 140)
    nrm = h["tex_param"](mat, "NormalMap", textures["T_LeafCluster_N"], unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, uv, -460, 400)

    tinted = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -120, -220)
    h["connect"](alb, "RGB", tinted, "A")
    h["connect"](base_col, "", tinted, "B")

    # Subtle subsurface glow for sunlit autumn canopy leaves
    sss = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 80, 260)
    sss.set_editor_property("const_b", 0.42)
    h["connect"](tinted, "", sss, "A")

    h["connect_prop"](tinted, "", unreal.MaterialProperty.MP_BASE_COLOR)
    h["connect_prop"](spec, "", unreal.MaterialProperty.MP_SPECULAR)
    h["connect_prop"](rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    h["connect_prop"](nrm, "RGB", unreal.MaterialProperty.MP_NORMAL)
    h["connect_prop"](msk, "R", unreal.MaterialProperty.MP_OPACITY_MASK)
    try:
        h["connect_prop"](sss, "", unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    except Exception:
        pass

    mel.recompile_material(mat)
    return mat


def _build_water_master(mat_dir, textures, h):
    mel = unreal.MaterialEditingLibrary
    mat = h["load_or_create"]("M_BN_Water", mat_dir, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(mat)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)

    world = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1400, 120)
    xy = h["mask"](mat, world, r=True, g=True, x=-1180, y=120)
    tile = h["param"](mat, unreal.MaterialExpressionScalarParameter, "TileScale", 0.00042, -1180, 280)
    uv1_base = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -960, 120)
    h["connect"](xy, "", uv1_base, "A")
    h["connect"](tile, "", uv1_base, "B")

    uv2_base = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -960, 320)
    uv2_base.set_editor_property("const_b", 1.73)
    h["connect"](uv1_base, "", uv2_base, "A")

    pan1 = mel.create_material_expression(mat, unreal.MaterialExpressionPanner, -740, 120)
    pan1.set_editor_property("speed_x", 0.024)
    pan1.set_editor_property("speed_y", 0.014)
    h["connect"](uv1_base, "", pan1, "Coordinate")

    pan2 = mel.create_material_expression(mat, unreal.MaterialExpressionPanner, -740, 320)
    pan2.set_editor_property("speed_x", -0.019)
    pan2.set_editor_property("speed_y", 0.027)
    h["connect"](uv2_base, "", pan2, "Coordinate")

    n1 = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -480, 100)
    n1.set_editor_property("texture", textures["T_Water_N"])
    n1.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    h["connect"](pan1, "", n1, "UVs")

    n2 = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -480, 340)
    n2.set_editor_property("texture", textures["T_Water_N"])
    n2.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    h["connect"](pan2, "", n2, "UVs")

    nblend = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -200, 220)
    nblend.set_editor_property("const_alpha", 0.5)
    h["connect"](n1, "RGB", nblend, "A")
    h["connect"](n2, "RGB", nblend, "B")

    flat = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -200, 420)
    flat.set_editor_property("constant", unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
    nstr = h["param"](mat, unreal.MaterialExpressionScalarParameter, "NormalStrength", 0.45, -200, 560)
    final_n = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, 60, 320)
    h["connect"](flat, "", final_n, "A")
    h["connect"](nblend, "", final_n, "B")
    h["connect"](nstr, "", final_n, "Alpha")

    base_col = h["param"](mat, unreal.MaterialExpressionVectorParameter, "BaseColor", unreal.LinearColor(0.012, 0.044, 0.068, 1.0), 60, -200)
    rough = h["param"](mat, unreal.MaterialExpressionScalarParameter, "Roughness", 0.075, 60, 60)
    spec = h["param"](mat, unreal.MaterialExpressionScalarParameter, "Specular", 0.88, 60, -40)
    metal = h["param"](mat, unreal.MaterialExpressionScalarParameter, "Metallic", 0.06, 60, -120)

    h["connect_prop"](base_col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    h["connect_prop"](metal, "", unreal.MaterialProperty.MP_METALLIC)
    h["connect_prop"](spec, "", unreal.MaterialProperty.MP_SPECULAR)
    h["connect_prop"](rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    h["connect_prop"](final_n, "", unreal.MaterialProperty.MP_NORMAL)

    mel.recompile_material(mat)
    return mat


def _build_particle_master(mat_dir, name, textures, additive, h):
    mel = unreal.MaterialEditingLibrary
    mat = h["load_or_create"](name, mat_dir, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(mat)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE if additive else unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    try:
        mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    except Exception:
        pass

    uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -640, 0)
    tex = h["tex_param"](mat, "ParticleTex", textures["T_VFX_SmokePuff"], unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, uv, -380, -80)
    col = h["param"](mat, unreal.MaterialExpressionVectorParameter, "Color", unreal.LinearColor(1, 1, 1, 1), -380, 200)

    rgb_mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -60, -60)
    h["connect"](tex, "RGB", rgb_mul, "A")
    h["connect"](col, "", rgb_mul, "B")

    a_mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -60, 180)
    h["connect"](tex, "A", a_mul, "A")
    h["connect"](col, "A", a_mul, "B")

    if additive:
        final_em = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 160, 40)
        h["connect"](rgb_mul, "", final_em, "A")
        h["connect"](a_mul, "", final_em, "B")
        h["connect_prop"](final_em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        h["connect_prop"](rgb_mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        h["connect_prop"](a_mul, "", unreal.MaterialProperty.MP_OPACITY)

    mel.recompile_material(mat)
    return mat


def _build_holo_master(mat_dir, h):
    mel = unreal.MaterialEditingLibrary
    mat = h["load_or_create"]("M_BN_RaceHolo", mat_dir, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(mat)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property("two_sided", True)
    try:
        mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    except Exception:
        pass

    col = h["param"](mat, unreal.MaterialExpressionVectorParameter, "Color", unreal.LinearColor(0.25, 1.8, 2.8, 1.0), -280, -180)
    uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -640, 80)
    v_ch = h["mask"](mat, uv, g=True, x=-440, y=80)
    inv_v = mel.create_material_expression(mat, unreal.MaterialExpressionOneMinus, -260, 80)
    h["connect"](v_ch, "", inv_v, "")
    grad = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -80, 80)
    grad.set_editor_property("const_a", 0.25)
    grad.set_editor_property("const_b", 1.0)
    h["connect"](inv_v, "", grad, "Alpha")

    tm = mel.create_material_expression(mat, unreal.MaterialExpressionTime, -640, 280)
    tm_mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -460, 280)
    tm_mul.set_editor_property("const_b", 0.55)
    h["connect"](tm, "", tm_mul, "A")
    sn = mel.create_material_expression(mat, unreal.MaterialExpressionSine, -280, 280)
    h["connect"](tm_mul, "", sn, "")
    pulse = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -80, 280)
    pulse.set_editor_property("const_a", 0.78)
    pulse.set_editor_property("const_b", 1.18)
    h["connect"](sn, "", pulse, "Alpha")

    fac = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 100, 160)
    h["connect"](grad, "", fac, "A")
    h["connect"](pulse, "", fac, "B")

    em = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 260, 0)
    h["connect"](col, "", em, "A")
    h["connect"](fac, "", em, "B")
    h["connect_prop"](em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(mat)
    return mat


def _build_mesh_uv_master(mat_dir, textures, mpc, h):
    mel = unreal.MaterialEditingLibrary
    mat = h["load_or_create"]("M_BN_MeshUV", mat_dir, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(mat)
    mat.set_editor_property("used_with_instanced_static_meshes", True)

    uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -760, 120)
    base_col = h["param"](mat, unreal.MaterialExpressionVectorParameter, "BaseColor", unreal.LinearColor(1, 1, 1, 1), -200, -460)
    rough = h["param"](mat, unreal.MaterialExpressionScalarParameter, "Roughness", 0.45, 40, 80)
    spec = h["param"](mat, unreal.MaterialExpressionScalarParameter, "Specular", 0.55, 40, -20)
    metal = h["param"](mat, unreal.MaterialExpressionScalarParameter, "Metallic", 0.0, 40, -120)
    nstr = h["param"](mat, unreal.MaterialExpressionScalarParameter, "NormalStrength", 0.0, -200, 380)
    em_const = h["param"](mat, unreal.MaterialExpressionVectorParameter, "Emissive", unreal.LinearColor(0, 0, 0, 1), -200, 560)
    em_night = h["param"](mat, unreal.MaterialExpressionVectorParameter, "EmissiveMaskColor", unreal.LinearColor(0, 0, 0, 1), -200, 740)

    alb = h["tex_param"](mat, "AlbedoMap", textures["T_White_D"], unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, uv, -460, -280)
    nrm = h["tex_param"](mat, "NormalMap", textures["T_Flat_N"], unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, uv, -460, 220)
    em_tex = h["tex_param"](mat, "EmissiveMap", textures["T_Black_E"], unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, uv, -460, 620)
    night_f = add_night_factor_node(mat, mel, mpc, -200, 920)

    tinted = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 80, -340)
    h["connect"](alb, "RGB", tinted, "A")
    h["connect"](base_col, "", tinted, "B")

    flat = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -40, 280)
    flat.set_editor_property("constant", unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
    final_n = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, 180, 260)
    h["connect"](flat, "", final_n, "A")
    h["connect"](nrm, "RGB", final_n, "B")
    h["connect"](nstr, "", final_n, "Alpha")

    em_m1 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 40, 680)
    h["connect"](em_tex, "RGB", em_m1, "A")
    h["connect"](em_night, "", em_m1, "B")
    em_m2 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 200, 760)
    h["connect"](em_m1, "", em_m2, "A")
    h["connect"](night_f, "", em_m2, "B")
    final_em = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, 360, 620)
    h["connect"](em_const, "", final_em, "A")
    h["connect"](em_m2, "", final_em, "B")

    h["connect_prop"](tinted, "", unreal.MaterialProperty.MP_BASE_COLOR)
    h["connect_prop"](metal, "", unreal.MaterialProperty.MP_METALLIC)
    h["connect_prop"](spec, "", unreal.MaterialProperty.MP_SPECULAR)
    h["connect_prop"](rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    h["connect_prop"](final_n, "", unreal.MaterialProperty.MP_NORMAL)
    h["connect_prop"](final_em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(mat)
    return mat


def build_special_materials(mat_dir, textures, mpc, helpers):
    """Creates all specialized masters and MaterialInstanceConstants under mat_dir."""
    mel = unreal.MaterialEditingLibrary
    load_or_create = helpers["load_or_create"]
    instances = {}

    def make_mi(name, parent, scalars=None, vectors=None, tex_params=None):
        mi = load_or_create(name, mat_dir, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, parent)
        mel.clear_all_material_instance_parameters(mi)
        for k, v in (scalars or {}).items():
            mel.set_material_instance_scalar_parameter_value(mi, k, float(v))
        for k, v in (vectors or {}).items():
            c = unreal.LinearColor(v[0], v[1], v[2], v[3] if len(v) > 3 else 1.0)
            mel.set_material_instance_vector_parameter_value(mi, k, c)
        for k, tname in (tex_params or {}).items():
            mel.set_material_instance_texture_parameter_value(mi, k, textures[tname])
        mel.update_material_instance(mi)
        instances[name] = mi
        return mi

    # 1. Foliage
    m_foliage = _build_foliage_master(mat_dir, textures, helpers)
    make_mi("MI_Foliage_AutumnA", m_foliage, {"Roughness": 0.62}, {"BaseColor": (1.06, 0.96, 0.84)},
            {"AlbedoMap": "T_LeafCluster_D", "MaskMap": "T_LeafCluster_A", "NormalMap": "T_LeafCluster_N"})
    make_mi("MI_Foliage_AutumnB", m_foliage, {"Roughness": 0.66}, {"BaseColor": (0.94, 0.66, 0.56)},
            {"AlbedoMap": "T_LeafCluster_D", "MaskMap": "T_LeafCluster_A", "NormalMap": "T_LeafCluster_N"})
    make_mi("MI_Foliage_Pine", m_foliage, {"Roughness": 0.72}, {"BaseColor": (0.86, 0.96, 0.86)},
            {"AlbedoMap": "T_PineNeedle_D", "MaskMap": "T_PineNeedle_A", "NormalMap": "T_PineNeedle_N"})
    make_mi("MI_Foliage_Bush", m_foliage, {"Roughness": 0.70}, {"BaseColor": (0.58, 0.82, 0.44)},
            {"AlbedoMap": "T_LeafCluster_D", "MaskMap": "T_LeafCluster_A", "NormalMap": "T_LeafCluster_N"})

    # 2. Animated Harbor & Ocean Water
    m_water = _build_water_master(mat_dir, textures, helpers)
    make_mi("MI_WaterHarbor", m_water, {"TileScale": 0.00048, "Roughness": 0.075, "Specular": 0.88, "NormalStrength": 0.48},
            {"BaseColor": (0.012, 0.048, 0.068)})
    make_mi("MI_WaterOcean", m_water, {"TileScale": 0.00028, "Roughness": 0.09, "Specular": 0.85, "NormalStrength": 0.55},
            {"BaseColor": (0.009, 0.032, 0.058)})

    # 3. Soft Alpha Particles & Road Markings
    m_part_trans = _build_particle_master(mat_dir, "M_BN_Particle_Translucent", textures, False, helpers)
    m_part_add = _build_particle_master(mat_dir, "M_BN_Particle_Additive", textures, True, helpers)
    make_mi("MI_VFX_Smoke", m_part_trans, vectors={"Color": (0.78, 0.79, 0.82, 0.55)}, tex_params={"ParticleTex": "T_VFX_SmokePuff"})
    make_mi("MI_VFX_Skid", m_part_trans, vectors={"Color": (0.03, 0.03, 0.035, 0.75)}, tex_params={"ParticleTex": "T_VFX_SkidMark"})
    make_mi("MI_VFX_Flame", m_part_add, vectors={"Color": (0.25, 0.85, 1.5, 0.9)}, tex_params={"ParticleTex": "T_VFX_Flame"})
    make_mi("MI_VFX_Spark", m_part_add, vectors={"Color": (2.2, 1.4, 0.4, 0.95)}, tex_params={"ParticleTex": "T_VFX_Spark"})
    make_mi("MI_Crosswalk", m_part_trans, vectors={"Color": (0.86, 0.86, 0.82, 0.90)}, tex_params={"ParticleTex": "T_Crosswalk_D"})
    make_mi("MI_RoadArrow", m_part_trans, vectors={"Color": (0.86, 0.86, 0.82, 0.88)}, tex_params={"ParticleTex": "T_RoadArrow_D"})

    # 4. Holographic Race / Garage / Cooldown Rings
    m_holo = _build_holo_master(mat_dir, helpers)
    make_mi("MI_RaceHolo_Start", m_holo, vectors={"Color": (0.20, 1.65, 2.60, 1.0)})
    make_mi("MI_RaceHolo_Gate", m_holo, vectors={"Color": (2.60, 1.05, 0.14, 1.0)})
    make_mi("MI_RaceHolo_Garage", m_holo, vectors={"Color": (0.18, 2.20, 0.95, 1.0)})
    make_mi("MI_RaceHolo_Cooldown", m_holo, vectors={"Color": (0.22, 1.05, 3.00, 1.0)})

    # 5. Mesh UV0 PBR (Turkish Signs, Storefronts, Traffic Vehicles, Police Gear, Car Light Strips)
    m_mesh = _build_mesh_uv_master(mat_dir, textures, mpc, helpers)
    for sign_key in ("Merkez", "EskiLiman", "KuzeyGecidi", "DoguSanayi"):
        make_mi(
            "MI_Sign_" + sign_key, m_mesh,
            {"Roughness": 0.34, "Specular": 0.60, "Metallic": 0.15},
            {"BaseColor": (0.95, 0.95, 0.95), "EmissiveMaskColor": (0.45, 0.48, 0.45)},
            {"AlbedoMap": "T_Sign_" + sign_key, "EmissiveMap": "T_Sign_" + sign_key},
        )
    make_mi(
        "MI_Storefront_Karamara", m_mesh,
        {"Roughness": 0.36, "Specular": 0.62, "Metallic": 0.12},
        {"BaseColor": (0.96, 0.94, 0.90), "EmissiveMaskColor": (1.85, 1.55, 1.15)},
        {"AlbedoMap": "T_Storefront_Karamara_D", "EmissiveMap": "T_Storefront_Karamara_E"},
    )
    # Civilian traffic & police vehicle slots
    make_mi("MI_Traffic_Paint", m_mesh, {"Roughness": 0.22, "Specular": 0.65, "Metallic": 0.68}, {"BaseColor": (0.78, 0.78, 0.80)})
    make_mi("MI_Traffic_Glass", m_mesh, {"Roughness": 0.08, "Specular": 0.82, "Metallic": 0.20}, {"BaseColor": (0.04, 0.06, 0.08)})
    make_mi("MI_Traffic_Trim", m_mesh, {"Roughness": 0.76, "Specular": 0.32, "Metallic": 0.10}, {"BaseColor": (0.06, 0.06, 0.07)})
    make_mi("MI_Traffic_Headlight", m_mesh, {"Roughness": 0.12, "Specular": 0.80},
            {"BaseColor": (0.96, 0.94, 0.86), "Emissive": (1.2, 1.1, 0.85), "EmissiveMaskColor": (6.0, 5.4, 4.0)},
            {"EmissiveMap": "T_White_D"})
    make_mi("MI_Traffic_Taillight", m_mesh, {"Roughness": 0.14, "Specular": 0.75},
            {"BaseColor": (0.65, 0.04, 0.03), "Emissive": (1.8, 0.08, 0.04), "EmissiveMaskColor": (3.2, 0.14, 0.06)},
            {"EmissiveMap": "T_White_D"})
    make_mi("MI_Police_LightRed", m_mesh, {"Roughness": 0.15}, {"BaseColor": (0.9, 0.06, 0.06), "Emissive": (8.5, 0.35, 0.2)})
    make_mi("MI_Police_LightBlue", m_mesh, {"Roughness": 0.15}, {"BaseColor": (0.06, 0.25, 0.95), "Emissive": (0.3, 1.8, 9.5)})
    make_mi("MI_Police_Chrome", m_mesh, {"Roughness": 0.28, "Specular": 0.75, "Metallic": 0.88}, {"BaseColor": (0.22, 0.24, 0.26)})
    make_mi("MI_Vehicle_HeadlightEmissive", m_mesh, {"Roughness": 0.10},
            {"BaseColor": (1.0, 0.97, 0.88), "Emissive": (0.4, 0.38, 0.32), "EmissiveMaskColor": (7.5, 6.8, 5.2)},
            {"EmissiveMap": "T_White_D"})
    make_mi("MI_Vehicle_BrakeLightEmissive", m_mesh, {"Roughness": 0.12},
            {"BaseColor": (0.55, 0.03, 0.02), "Emissive": (0.8, 0.04, 0.02)})

    return instances
