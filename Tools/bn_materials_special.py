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
    """Creates/updates /Game/World/Materials/MPC_BN_World with NightFactor, Wetness, and RainIntensity."""
    mpc = load_or_create(
        MPC_NAME,
        mat_dir,
        unreal.MaterialParameterCollection,
        unreal.MaterialParameterCollectionFactoryNew(),
    )
    existing = [str(n) for n in mpc.get_scalar_parameter_names()]
    params = list(mpc.get_editor_property("scalar_parameters"))
    changed = False
    for pname, default_val in (("NightFactor", 0.0), ("Wetness", 0.42), ("RainIntensity", 0.0)):
        if pname not in existing:
            param = unreal.CollectionScalarParameter()
            try:
                param.set_editor_property("parameter_name", pname)
                param.set_editor_property("default_value", float(default_val))
            except Exception:
                param.import_text('(DefaultValue=%f,ParameterName="%s")' % (float(default_val), pname))
            params.append(param)
            changed = True
    if changed:
        mpc.set_editor_property("scalar_parameters", params)
    return mpc


def add_mpc_scalar_node(mat, mel, mpc, param_name, x, y):
    """Creates a CollectionParameter node reading MPC_BN_World.<param_name>."""
    node = mel.create_material_expression(mat, unreal.MaterialExpressionCollectionParameter, x, y)
    node.set_editor_property("collection", mpc)
    node.set_editor_property("parameter_name", param_name)
    return node


def add_night_factor_node(mat, mel, mpc, x, y):
    """Creates a CollectionParameter node reading MPC_BN_World.NightFactor."""
    return add_mpc_scalar_node(mat, mel, mpc, "NightFactor", x, y)


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

    base_col = h["param"](mat, unreal.MaterialExpressionVectorParameter, "BaseColor", unreal.LinearColor(0.012, 0.044, 0.068, 1.0), -120, -240)
    sky_tint = h["param"](mat, unreal.MaterialExpressionVectorParameter, "SkyTint", unreal.LinearColor(0.16, 0.24, 0.34, 1.0), -120, -100)
    fres = mel.create_material_expression(mat, unreal.MaterialExpressionFresnel, -120, 40)
    try:
        fres.set_editor_property("exponent", 3.4)
        fres.set_editor_property("base_reflect_fraction", 0.05)
    except Exception:
        pass
    water_col = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, 120, -180)
    h["connect"](base_col, "", water_col, "A")
    h["connect"](sky_tint, "", water_col, "B")
    h["connect"](fres, "", water_col, "Alpha")

    rough = h["param"](mat, unreal.MaterialExpressionScalarParameter, "Roughness", 0.065, 120, 60)
    spec = h["param"](mat, unreal.MaterialExpressionScalarParameter, "Specular", 0.92, 120, -20)
    metal = h["param"](mat, unreal.MaterialExpressionScalarParameter, "Metallic", 0.08, 120, -90)

    h["connect_prop"](water_col, "", unreal.MaterialProperty.MP_BASE_COLOR)
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

    col = h["param"](mat, unreal.MaterialExpressionVectorParameter, "Color", unreal.LinearColor(0.25, 1.8, 2.8, 1.0), -280, -240)
    uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -820, 60)
    v_ch = h["mask"](mat, uv, g=True, x=-620, y=60)
    inv_v = mel.create_material_expression(mat, unreal.MaterialExpressionOneMinus, -440, 60)
    h["connect"](v_ch, "", inv_v, "")
    # Smooth exponential fade completely to 0.0 at top edge (eliminates solid plastic cylinder look)
    grad = mel.create_material_expression(mat, unreal.MaterialExpressionPower, -240, 60)
    grad.set_editor_property("const_exponent", 2.3)
    h["connect"](inv_v, "", grad, "Base")

    # Subtle animated horizontal scanlines + grazing Fresnel rim highlight
    tm = mel.create_material_expression(mat, unreal.MaterialExpressionTime, -820, 260)
    v_scan = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -620, 220)
    v_scan.set_editor_property("const_b", 10.0)
    h["connect"](v_ch, "", v_scan, "A")
    t_scan = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -620, 340)
    t_scan.set_editor_property("const_b", 0.65)
    h["connect"](tm, "", t_scan, "A")
    scan_ph = mel.create_material_expression(mat, unreal.MaterialExpressionSubtract, -440, 260)
    h["connect"](v_scan, "", scan_ph, "A")
    h["connect"](t_scan, "", scan_ph, "B")
    scan_sin = mel.create_material_expression(mat, unreal.MaterialExpressionSine, -280, 260)
    h["connect"](scan_ph, "", scan_sin, "")
    scan_band = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -100, 260)
    scan_band.set_editor_property("const_a", 0.72)
    scan_band.set_editor_property("const_b", 1.12)
    h["connect"](scan_sin, "", scan_band, "Alpha")

    fres = mel.create_material_expression(mat, unreal.MaterialExpressionFresnel, -280, -80)
    try:
        fres.set_editor_property("exponent", 2.2)
        fres.set_editor_property("base_reflect_fraction", 0.12)
    except Exception:
        pass
    rim_fac = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -100, -80)
    rim_fac.set_editor_property("const_a", 0.34)
    rim_fac.set_editor_property("const_b", 1.05)
    h["connect"](fres, "", rim_fac, "Alpha")

    fac1 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 80, 80)
    h["connect"](grad, "", fac1, "A")
    h["connect"](scan_band, "", fac1, "B")
    fac2 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 220, 20)
    h["connect"](fac1, "", fac2, "A")
    h["connect"](rim_fac, "", fac2, "B")

    em = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 380, -60)
    h["connect"](col, "", em, "A")
    h["connect"](fac2, "", em, "B")
    h["connect_prop"](em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(mat)
    return mat


def _build_terrain_slope_master(mat_dir, textures, h):
    """World-space slope-blended coastal mountain terrain: cliff rock on steep faces, mountain grass on flat crests."""
    mel = unreal.MaterialEditingLibrary
    mat = h["load_or_create"]("M_BN_TerrainSlope", mat_dir, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(mat)
    mat.set_editor_property("used_with_instanced_static_meshes", True)

    world = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1420, 120)
    xy = h["mask"](mat, world, r=True, g=True, x=-1220, y=120)
    rock_tile = h["param"](mat, unreal.MaterialExpressionScalarParameter, "RockTileScale", 0.00055, -1220, 260)
    grass_tile = h["param"](mat, unreal.MaterialExpressionScalarParameter, "GrassTileScale", 0.00075, -1220, 380)

    rock_uv = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -980, 80)
    h["connect"](xy, "", rock_uv, "A")
    h["connect"](rock_tile, "", rock_uv, "B")

    grass_uv = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -980, 280)
    h["connect"](xy, "", grass_uv, "A")
    h["connect"](grass_tile, "", grass_uv, "B")

    # Slope mask from VertexNormalWS.Z: smooth grass on gentle tops (Nz > 0.72), raw rock on cliffs
    vnorm = mel.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -980, -280)
    nz = h["mask"](mat, vnorm, b=True, x=-780, y=-280)
    sub = mel.create_material_expression(mat, unreal.MaterialExpressionSubtract, -600, -280)
    sub.set_editor_property("const_b", 0.68)
    h["connect"](nz, "", sub, "A")
    gain = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -440, -280)
    gain.set_editor_property("const_b", 4.5)
    h["connect"](sub, "", gain, "A")
    slope_alpha = mel.create_material_expression(mat, unreal.MaterialExpressionClamp, -260, -280)
    slope_alpha.set_editor_property("min_default", 0.0)
    slope_alpha.set_editor_property("max_default", 1.0)
    h["connect"](gain, "", slope_alpha, "")

    r_alb = h["tex_param"](mat, "RockAlbedo", textures["T_CliffRock_D"], unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, rock_uv, -640, -40)
    r_nrm = h["tex_param"](mat, "RockNormal", textures["T_CliffRock_N"], unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, rock_uv, -640, 180)
    r_rgh = h["tex_param"](mat, "RockRough", textures["T_CliffRock_R"], unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, rock_uv, -640, 400)

    g_alb = h["tex_param"](mat, "GrassAlbedo", textures["T_MountainGrass_D"], unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, grass_uv, -640, 620)
    g_nrm = h["tex_param"](mat, "GrassNormal", textures["T_MountainGrass_N"], unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, grass_uv, -640, 840)
    g_rgh = h["tex_param"](mat, "GrassRough", textures["T_MountainGrass_R"], unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, grass_uv, -640, 1060)

    rock_tint = h["param"](mat, unreal.MaterialExpressionVectorParameter, "RockTint", unreal.LinearColor(0.84, 0.80, 0.76, 1.0), -360, -80)
    grass_tint = h["param"](mat, unreal.MaterialExpressionVectorParameter, "GrassTint", unreal.LinearColor(0.76, 0.80, 0.64, 1.0), -360, 560)
    r_col = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -140, -40)
    h["connect"](r_alb, "RGB", r_col, "A")
    h["connect"](rock_tint, "", r_col, "B")
    g_col = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -140, 580)
    h["connect"](g_alb, "RGB", g_col, "A")
    h["connect"](grass_tint, "", g_col, "B")

    alb_lerp = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, 140, -40)
    h["connect"](r_col, "", alb_lerp, "A")
    h["connect"](g_col, "", alb_lerp, "B")
    h["connect"](slope_alpha, "", alb_lerp, "Alpha")

    nrm_lerp = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, 140, 220)
    h["connect"](r_nrm, "RGB", nrm_lerp, "A")
    h["connect"](g_nrm, "RGB", nrm_lerp, "B")
    h["connect"](slope_alpha, "", nrm_lerp, "Alpha")

    rgh_lerp = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, 140, 440)
    h["connect"](r_rgh, "R", rgh_lerp, "A")
    h["connect"](g_rgh, "R", rgh_lerp, "B")
    h["connect"](slope_alpha, "", rgh_lerp, "Alpha")
    rgh_out = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, 320, 440)
    rgh_out.set_editor_property("const_a", 0.68)
    rgh_out.set_editor_property("const_b", 0.96)
    h["connect"](rgh_lerp, "", rgh_out, "Alpha")

    spec = h["param"](mat, unreal.MaterialExpressionScalarParameter, "Specular", 0.28, 320, 120)
    h["connect_prop"](alb_lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)
    h["connect_prop"](spec, "", unreal.MaterialProperty.MP_SPECULAR)
    h["connect_prop"](rgh_out, "", unreal.MaterialProperty.MP_ROUGHNESS)
    h["connect_prop"](nrm_lerp, "", unreal.MaterialProperty.MP_NORMAL)

    mel.recompile_material(mat)
    return mat


def _build_sky_clouds_master(mat_dir, textures, mpc, h):
    """Additive unlit sky cloud dome fading cleanly into the horizon fog, tinted warm amber at dusk and indigo at night."""
    mel = unreal.MaterialEditingLibrary
    mat = h["load_or_create"]("M_BN_SkyClouds", mat_dir, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(mat)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property("two_sided", True)
    try:
        mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    except Exception:
        pass

    uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -860, 40)
    pan = mel.create_material_expression(mat, unreal.MaterialExpressionPanner, -660, 40)
    pan.set_editor_property("speed_x", 0.0024)
    pan.set_editor_property("speed_y", 0.0005)
    h["connect"](uv, "", pan, "Coordinate")

    cloud_tex = h["tex_param"](mat, "CloudMap", textures["T_SkyClouds_D"], unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, pan, -420, 20)

    # Smooth horizon fade (V=0 horizon -> 0, V>=0.22 full dome) so sky merges seamlessly with ExponentialHeightFog
    v_ch = h["mask"](mat, uv, g=True, x=-620, y=280)
    v_sub = mel.create_material_expression(mat, unreal.MaterialExpressionSubtract, -440, 280)
    v_sub.set_editor_property("const_b", 0.05)
    h["connect"](v_ch, "", v_sub, "A")
    v_mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -280, 280)
    v_mul.set_editor_property("const_b", 5.5)
    h["connect"](v_sub, "", v_mul, "A")
    horizon_fade = mel.create_material_expression(mat, unreal.MaterialExpressionClamp, -100, 280)
    horizon_fade.set_editor_property("min_default", 0.0)
    horizon_fade.set_editor_property("max_default", 1.0)
    h["connect"](v_mul, "", horizon_fade, "")

    dusk_col = h["param"](mat, unreal.MaterialExpressionVectorParameter, "DuskTint", unreal.LinearColor(1.05, 0.66, 0.38, 1.0), -420, -280)
    night_col = h["param"](mat, unreal.MaterialExpressionVectorParameter, "NightTint", unreal.LinearColor(0.14, 0.22, 0.42, 1.0), -420, -140)
    night_f = add_night_factor_node(mat, mel, mpc, -420, -20)
    sky_tint = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -120, -180)
    h["connect"](dusk_col, "", sky_tint, "A")
    h["connect"](night_col, "", sky_tint, "B")
    h["connect"](night_f, "", sky_tint, "Alpha")

    opacity = h["param"](mat, unreal.MaterialExpressionScalarParameter, "CloudOpacity", 0.38, 40, 220)
    col_mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 100, -40)
    h["connect"](cloud_tex, "RGB", col_mul, "A")
    h["connect"](sky_tint, "", col_mul, "B")

    mask_mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 100, 180)
    h["connect"](horizon_fade, "", mask_mul, "A")
    h["connect"](opacity, "", mask_mul, "B")

    em = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 280, 40)
    h["connect"](col_mul, "", em, "A")
    h["connect"](mask_mul, "", em, "B")
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
    # Parked-car paints (HISM material slot 0 override) so kerbside cars are not all one colour.
    for pname, pcol in (("Red", (0.42, 0.04, 0.03)), ("Blue", (0.04, 0.10, 0.30)), ("White", (0.82, 0.82, 0.80)),
                        ("Black", (0.02, 0.02, 0.025)), ("Green", (0.05, 0.16, 0.09)), ("Taxi", (0.80, 0.55, 0.04))):
        make_mi("MI_Parked_" + pname, m_mesh, {"Roughness": 0.26, "Specular": 0.62, "Metallic": 0.55}, {"BaseColor": pcol})
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
    make_mi("MI_CarCarbon", m_mesh,
            {"Roughness": 0.22, "Specular": 0.68, "Metallic": 0.20, "NormalStrength": 0.85},
            {"BaseColor": (0.90, 0.92, 0.96)},
            {"AlbedoMap": "T_CarbonWeave_D", "NormalMap": "T_CarbonWeave_N"})
    make_mi("MI_CarRim", m_mesh,
            {"Roughness": 0.22, "Specular": 0.78, "Metallic": 0.86},
            {"BaseColor": (0.74, 0.76, 0.80)})

    # 6. Slope-Blended Coastal Mountain Terrain (Cliff Rock on Steep Slopes + Mountain Grass on Crests)
    m_slope = _build_terrain_slope_master(mat_dir, textures, helpers)
    make_mi("MI_MountainRock_Slope", m_slope,
            {"RockTileScale": 0.00055, "GrassTileScale": 0.00075, "Specular": 0.28},
            {"RockTint": (0.84, 0.80, 0.76), "GrassTint": (0.76, 0.80, 0.64)},
            {"RockAlbedo": "T_CliffRock_D", "RockNormal": "T_CliffRock_N", "RockRough": "T_CliffRock_R",
             "GrassAlbedo": "T_MountainGrass_D", "GrassNormal": "T_MountainGrass_N", "GrassRough": "T_MountainGrass_R"})

    # 7. Dynamic Cloud Sky Dome (Horizon-Faded Additive Panning Clouds)
    m_sky = _build_sky_clouds_master(mat_dir, textures, mpc, helpers)
    make_mi("MI_SkyClouds", m_sky,
            {"CloudOpacity": 0.36},
            {"DuskTint": (1.06, 0.68, 0.40), "NightTint": (0.14, 0.22, 0.42)},
            {"CloudMap": "T_SkyClouds_D"})

    return instances
