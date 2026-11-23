"""
Generates and imports authentic NFS Most Wanted (2005)-inspired UI textures for BLACK NUMBERS
into /Game/UI/Textures/, plus imports the 4 original Karamara concept artworks.

Two modes:
  1. System python3 (with PIL + numpy):
       python3 Tools/bn_ui_textures.py --generate-only [--force]
     Generates crisp RGBA PNGs in Saved/GeneratedUITextures/.
  2. Unreal Editor Python:
       UnrealEditor-Cmd ... -run=pythonscript -script=Tools/bn_ui_textures.py
     Imports all PNGs as crisp non-streaming UI UTexture2Ds under /Game/UI/Textures/.
"""
import math
import os
import shutil
import subprocess
import sys

PROJECT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OUT_DIR = os.path.join(PROJECT_DIR, "Saved", "GeneratedUITextures")
CONCEPT_DIR = os.path.join(PROJECT_DIR, "docs", "images", "concept")
UE_TEX_DIR = "/Game/UI/Textures"

# (asset_name, wrap_mode) where wrap_mode is "wrap" or "clamp"
UI_TEXTURE_MANIFEST = [
    ("T_UI_Art_HeroCar", "clamp"),
    ("T_UI_Art_Boss01", "clamp"),
    ("T_UI_Art_Pursuit", "clamp"),
    ("T_UI_Art_Garage", "clamp"),
    ("T_UI_HazardStripe", "wrap"),
    ("T_UI_BrushedMetal", "wrap"),
    ("T_UI_PanelBevel", "clamp"),
    ("T_UI_VignetteLeft", "clamp"),
    ("T_UI_VignetteFull", "clamp"),
    ("T_UI_TachRing", "clamp"),
    ("T_UI_RadarRing", "clamp"),
]

CONCEPT_SOURCES = {
    "T_UI_Art_HeroCar": "hero_car_kurogane.png",
    "T_UI_Art_Boss01": "boss01_nocturne.png",
    "T_UI_Art_Pursuit": "karamara_pursuit.png",
    "T_UI_Art_Garage": "tarik_garage_tier6.png",
}


def _all_pngs_exist():
    return all(os.path.isfile(os.path.join(OUT_DIR, name + ".png")) for name, _ in UI_TEXTURE_MANIFEST)


def _save_rgba(name, arr):
    from PIL import Image
    path = os.path.join(OUT_DIR, name + ".png")
    clipped = arr.clip(0.0, 255.0).astype("uint8")
    Image.fromarray(clipped, mode="RGBA").save(path, format="PNG")


def _generate_with_pil(force=False):
    import numpy as np
    from PIL import Image, ImageDraw, ImageFont

    os.makedirs(OUT_DIR, exist_ok=True)
    if not force and _all_pngs_exist():
        return len(UI_TEXTURE_MANIFEST)

    # 1. Copy & normalize the 4 original concept artworks (max 1280px wide for fast VRAM-friendly UI)
    for asset_name, src_file in CONCEPT_SOURCES.items():
        src_path = os.path.join(CONCEPT_DIR, src_file)
        dst_path = os.path.join(OUT_DIR, asset_name + ".png")
        if not os.path.isfile(src_path):
            raise RuntimeError("Missing concept art source: " + src_path)
        with Image.open(src_path) as im:
            im = im.convert("RGBA")
            if im.width > 1280:
                new_h = max(1, int(round(im.height * (1280.0 / im.width))))
                im = im.resize((1280, new_h), Image.LANCZOS)
            im.save(dst_path, format="PNG")

    rng = np.random.default_rng(2005)

    # 2. T_UI_HazardStripe (512x64): seamless 45-deg distressed amber & asphalt caution stripe
    w, h = 512, 64
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    period = 32.0
    phase = ((xx + yy) % period) / period
    # Anti-aliased stripe mask
    edge = 0.045
    stripe = np.clip((phase - 0.0) / edge, 0.0, 1.0) * np.clip((0.48 - phase) / edge, 0.0, 1.0)
    grit = rng.normal(0.0, 1.0, (h, w)).astype(np.float32)
    streak = rng.normal(0.0, 1.0, (h, 1)).astype(np.float32)
    wear = np.clip(0.90 + 0.07 * grit + 0.05 * streak, 0.72, 1.12)
    amber = np.array([242.0, 168.0, 20.0], dtype=np.float32)
    asphalt = np.array([17.0, 18.0, 21.0], dtype=np.float32)
    rgb = (asphalt[None, None, :] * (1.0 - stripe[:, :, None]) + amber[None, None, :] * stripe[:, :, None]) * wear[:, :, None]
    # Top highlight & bottom shadow bevel
    rgb[0:2, :, :] *= 1.18
    rgb[h - 2:h, :, :] *= 0.45
    alpha = np.full((h, w, 1), 255.0, dtype=np.float32)
    _save_rgba("T_UI_HazardStripe", np.concatenate([rgb, alpha], axis=-1))

    # 3. T_UI_BrushedMetal (512x512): seamless dark gunmetal brushed aluminum + carbon micro-grain
    n = 512
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float32)
    horiz = rng.normal(0.0, 1.0, (n, 16)).astype(np.float32)
    horiz_up = np.repeat(horiz, n // 16, axis=1)
    # Smooth horizontal brush streaks
    brush = np.roll(horiz_up, 3, axis=1) * 0.25 + horiz_up * 0.5 + np.roll(horiz_up, -3, axis=1) * 0.25
    fine = rng.normal(0.0, 1.0, (n, n)).astype(np.float32)
    diag = np.sin((xx + yy) * (2.0 * math.pi / 16.0)) * 0.5 + 0.5
    base = 22.0 + 3.2 * brush + 1.8 * fine + 2.2 * diag
    metal = np.stack([base * 1.03, base * 1.01, base * 0.98, np.full((n, n), 245.0, dtype=np.float32)], axis=-1)
    _save_rgba("T_UI_BrushedMetal", metal)

    # 4. T_UI_PanelBevel (256x256): 9-slice tactical Most Wanted framed dark steel panel
    s = 256
    yy, xx = np.mgrid[0:s, 0:s].astype(np.float32)
    # Subtle vertical gunmetal sheen + horizontal micro-grain
    grad_y = 1.0 - 0.28 * (yy / (s - 1.0))
    grain = rng.normal(0.0, 1.4, (s, 1)).astype(np.float32)
    p = np.zeros((s, s, 4), dtype=np.float32)
    p[:, :, 0] = 20.0 * grad_y + grain
    p[:, :, 1] = 21.0 * grad_y + grain
    p[:, :, 2] = 24.0 * grad_y + grain
    p[:, :, 3] = 238.0

    # Outer 2px crisp warm steel border
    border_col = np.array([78.0, 74.0, 66.0, 255.0], dtype=np.float32)
    p[0:2, :, :] = border_col
    p[s - 2:s, :, :] = np.array([38.0, 36.0, 34.0, 255.0], dtype=np.float32)
    p[:, 0:2, :] = border_col
    p[:, s - 2:s, :] = np.array([46.0, 44.0, 41.0, 255.0], dtype=np.float32)
    # Inner top highlight edge (Machined lip)
    p[2:3, 2:s - 2, :] = np.array([150.0, 134.0, 96.0, 155.0], dtype=np.float32)
    # Amber corner L-brackets (crisp 14px industrial ticks at all 4 corners)
    corner_amber = np.array([240.0, 165.0, 20.0, 255.0], dtype=np.float32)
    arm = 14
    p[0:3, 0:arm, :] = corner_amber
    p[0:arm, 0:3, :] = corner_amber
    p[0:3, s - arm:s, :] = corner_amber
    p[0:arm, s - 3:s, :] = corner_amber
    p[s - 3:s, 0:arm, :] = corner_amber
    p[s - arm:s, 0:3, :] = corner_amber
    p[s - 3:s, s - arm:s, :] = corner_amber
    p[s - arm:s, s - 3:s, :] = corner_amber
    _save_rgba("T_UI_PanelBevel", p)

    # 5. T_UI_VignetteLeft (1024x64): smooth cinema fade dark-left -> transparent-right
    vw, vh = 1024, 64
    u = np.linspace(0.0, 1.0, vw, dtype=np.float32)[None, :]
    # Solid dark under sidebar (0..0.34), smooth S-curve feather out to 0.94
    t = np.clip((u - 0.34) / 0.60, 0.0, 1.0)
    smooth = t * t * (3.0 - 2.0 * t)
    alpha_1d = 248.0 * np.power(1.0 - smooth, 1.35)
    vleft = np.zeros((vh, vw, 4), dtype=np.float32)
    vleft[:, :, 0] = 9.0
    vleft[:, :, 1] = 10.0
    vleft[:, :, 2] = 12.0
    vleft[:, :, 3] = np.repeat(alpha_1d, vh, axis=0)
    _save_rgba("T_UI_VignetteLeft", vleft)

    # 6. T_UI_VignetteFull (512x512): smooth cinematic radial edge vignette + bottom anchor shade
    vs = 512
    yn, xn = np.mgrid[0:vs, 0:vs].astype(np.float32)
    nx = (xn / (vs - 1.0)) * 2.0 - 1.0
    ny = (yn / (vs - 1.0)) * 2.0 - 1.0
    rad = np.sqrt(nx * nx * 0.85 + ny * ny * 1.15)
    v_alpha = np.clip((rad - 0.52) / 0.72, 0.0, 1.0)
    v_alpha = v_alpha * v_alpha * 185.0
    # Slight warm top/bottom cinema band darkening
    edge_y = np.clip((np.abs(ny) - 0.78) / 0.22, 0.0, 1.0) * 110.0
    vfull = np.zeros((vs, vs, 4), dtype=np.float32)
    vfull[:, :, 0] = 8.0
    vfull[:, :, 1] = 9.0
    vfull[:, :, 2] = 11.0
    vfull[:, :, 3] = np.clip(v_alpha + edge_y, 0.0, 225.0)
    _save_rgba("T_UI_VignetteFull", vfull)

    # Load a bold sans font if present on Linux for crisp gauge numerals
    font_candidates = [
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
    ]
    font_path = next((fp for fp in font_candidates if os.path.isfile(fp)), None)

    def load_font(px):
        return ImageFont.truetype(font_path, px) if font_path else ImageFont.load_default()

    # 7. T_UI_TachRing (512x512, 2x supersampled from 1024x1024): NFS Most Wanted circular tachometer bezel
    hi = 1024
    cx = cy = hi * 0.5
    yy, xx = np.mgrid[0:hi, 0:hi].astype(np.float32)
    dx = xx - cx
    dy = yy - cy
    dist = np.sqrt(dx * dx + dy * dy)
    ang = (np.degrees(np.arctan2(dy, dx)) + 360.0) % 360.0
    # Sweep from 135 deg clockwise by 270 deg -> ends at 45 deg
    sweep_deg = (ang - 135.0) % 360.0

    tach = np.zeros((hi, hi, 4), dtype=np.float32)
    # Recessed dark smoked dial core
    core = dist <= 452.0
    rad_n = np.clip(dist / 452.0, 0.0, 1.0)
    tach[core, 0] = 11.0 + 10.0 * (rad_n[core] ** 2)
    tach[core, 1] = 12.0 + 9.0 * (rad_n[core] ** 2)
    tach[core, 2] = 15.0 + 8.0 * (rad_n[core] ** 2)
    tach[core, 3] = 222.0

    # Inner dark chrome separator ring
    inner_ring = (dist >= 306.0) & (dist <= 314.0)
    tach[inner_ring] = [95.0, 88.0, 72.0, 180.0]

    # Outer metallic bezel (452..494) with directional specular sheen
    bezel = (dist > 452.0) & (dist <= 494.0)
    sheen = 0.55 + 0.45 * np.cos(np.radians(ang + 45.0))
    bezel_t = (dist - 452.0) / 42.0
    bezel_curve = np.sin(bezel_t * math.pi)
    bval = 42.0 + 88.0 * sheen * (0.45 + 0.55 * bezel_curve)
    tach[bezel, 0] = bval[bezel] * 1.06
    tach[bezel, 1] = bval[bezel] * 0.98
    tach[bezel, 2] = bval[bezel] * 0.84
    tach[bezel, 3] = 250.0

    # Razor amber accent rim on outer sweep (486..494)
    amber_rim = (dist >= 484.0) & (dist <= 494.0) & (sweep_deg <= 270.0)
    tach[amber_rim] = [242.0, 168.0, 24.0, 245.0]

    # Redline hazard band (82%..100% of 270 deg = 221..270 deg)
    red_arc = (dist >= 420.0) & (dist <= 448.0) & (sweep_deg >= 221.0) & (sweep_deg <= 270.0)
    tach[red_arc] = [215.0, 34.0, 28.0, 215.0]

    # Subtle ambient amber arc track (0..221 deg)
    amber_arc = (dist >= 434.0) & (dist <= 444.0) & (sweep_deg <= 221.0)
    tach[amber_arc] = [145.0, 112.0, 52.0, 95.0]

    # Soft outer edge AA fade
    outer_aa = (dist > 494.0) & (dist <= 502.0)
    tach[outer_aa, 0] = 24.0
    tach[outer_aa, 1] = 22.0
    tach[outer_aa, 2] = 20.0
    tach[outer_aa, 3] = (1.0 - (dist[outer_aa] - 494.0) / 8.0) * 200.0

    tach_img = Image.fromarray(tach.clip(0.0, 255.0).astype("uint8"), mode="RGBA")
    draw = ImageDraw.Draw(tach_img)
    num_font = load_font(54)

    # Draw 8 major ticks (0..8 -> 9 marks) + 16 minor half-ticks
    for i in range(17):
        frac = i / 16.0
        deg = 135.0 + frac * 270.0
        rad = math.radians(deg)
        c_a, s_a = math.cos(rad), math.sin(rad)
        is_major = (i % 2 == 0)
        is_red = frac >= 0.81
        r0 = 382.0 if is_major else 404.0
        r1 = 446.0
        col = (245, 58, 42, 255) if is_red else ((248, 240, 222, 245) if is_major else (165, 160, 150, 185))
        draw.line((cx + c_a * r0, cy + s_a * r0, cx + c_a * r1, cy + s_a * r1), fill=col, width=10 if is_major else 5)
        if is_major:
            label = str(i // 2)
            nr = 338.0
            nx_t = cx + c_a * nr
            ny_t = cy + s_a * nr
            bbox = draw.textbbox((0, 0), label, font=num_font)
            tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
            tcol = (255, 68, 52, 255) if is_red else (242, 236, 224, 240)
            draw.text((nx_t - tw * 0.5, ny_t - th * 0.58), label, fill=tcol, font=num_font)

    tach_small = tach_img.resize((512, 512), Image.LANCZOS)
    tach_small.save(os.path.join(OUT_DIR, "T_UI_TachRing.png"), format="PNG")

    # 8. T_UI_RadarRing (512x512, 2x supersampled): matching Most Wanted circular GPS radar frame
    rad_arr = np.zeros((hi, hi, 4), dtype=np.float32)
    r_core = dist <= 458.0
    r_n = np.clip(dist / 458.0, 0.0, 1.0)
    rad_arr[r_core, 0] = 10.0 + 12.0 * (r_n[r_core] ** 2)
    rad_arr[r_core, 1] = 12.0 + 11.0 * (r_n[r_core] ** 2)
    rad_arr[r_core, 2] = 15.0 + 10.0 * (r_n[r_core] ** 2)
    rad_arr[r_core, 3] = 205.0

    # Concentric tactical GPS range rings (blended softly onto dark glass without punching alpha holes)
    for ring_r in (160.0, 310.0):
        ring_mask = np.abs(dist - ring_r) <= 2.5
        rad_arr[ring_mask, 0] = rad_arr[ring_mask, 0] * 0.72 + 175.0 * 0.28
        rad_arr[ring_mask, 1] = rad_arr[ring_mask, 1] * 0.72 + 148.0 * 0.28
        rad_arr[ring_mask, 2] = rad_arr[ring_mask, 2] * 0.72 + 92.0 * 0.28
        rad_arr[ring_mask, 3] = 212.0
    # Subtle crosshair axes
    axis_mask = ((np.abs(dx) <= 2.0) | (np.abs(dy) <= 2.0)) & (dist <= 450.0) & (dist >= 45.0)
    rad_arr[axis_mask, 0] = rad_arr[axis_mask, 0] * 0.84 + 140.0 * 0.16
    rad_arr[axis_mask, 1] = rad_arr[axis_mask, 1] * 0.84 + 132.0 * 0.16
    rad_arr[axis_mask, 2] = rad_arr[axis_mask, 2] * 0.84 + 112.0 * 0.16
    rad_arr[axis_mask, 3] = 208.0

    # Metallic outer bezel + amber crown tick
    r_bezel = (dist > 458.0) & (dist <= 496.0)
    r_t = (dist - 458.0) / 38.0
    r_val = 46.0 + 82.0 * sheen * (0.45 + 0.55 * np.sin(r_t * math.pi))
    rad_arr[r_bezel, 0] = r_val[r_bezel] * 1.04
    rad_arr[r_bezel, 1] = r_val[r_bezel] * 0.96
    rad_arr[r_bezel, 2] = r_val[r_bezel] * 0.84
    rad_arr[r_bezel, 3] = 248.0

    r_rim = (dist >= 488.0) & (dist <= 496.0)
    rad_arr[r_rim] = [240.0, 168.0, 26.0, 230.0]

    rad_img = Image.fromarray(rad_arr.clip(0.0, 255.0).astype("uint8"), mode="RGBA")
    rdraw = ImageDraw.Draw(rad_img)
    # 12 perimeter degree ticks + top forward chevron
    for k in range(12):
        rad = math.radians(k * 30.0 - 90.0)
        c_a, s_a = math.cos(rad), math.sin(rad)
        r0 = 432.0 if k % 3 == 0 else 442.0
        col = (242, 168, 24, 245) if k == 0 else (180, 174, 160, 185)
        rdraw.line((cx + c_a * r0, cy + s_a * r0, cx + c_a * 458.0, cy + s_a * 458.0), fill=col, width=8 if k % 3 == 0 else 4)
    rdraw.polygon([(cx, 444.0 - 418.0), (cx - 18.0, 444.0 - 388.0), (cx + 18.0, 444.0 - 388.0)], fill=(242, 168, 24, 255))

    rad_small = rad_img.resize((512, 512), Image.LANCZOS)
    rad_small.save(os.path.join(OUT_DIR, "T_UI_RadarRing.png"), format="PNG")

    return len(UI_TEXTURE_MANIFEST)


def generate_all_pngs(force=False):
    if not force and _all_pngs_exist():
        return len(UI_TEXTURE_MANIFEST)
    try:
        return _generate_with_pil(force=force)
    except ImportError:
        env = os.environ.copy()
        env.pop("PYTHONHOME", None)
        env.pop("PYTHONPATH", None)
        cmd = ["/usr/bin/python3", os.path.abspath(__file__), "--generate-only"]
        if force:
            cmd.append("--force")
        subprocess.run(cmd, check=True, env=env)
        return len(UI_TEXTURE_MANIFEST)


def ensure_ue_ui_textures(force_reimport=False):
    """Generates missing PNGs and imports all UI textures into /Game/UI/Textures/."""
    import unreal

    generate_all_pngs(force=force_reimport)

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    assets = unreal.EditorAssetLibrary
    if not assets.does_directory_exist(UE_TEX_DIR):
        assets.make_directory(UE_TEX_DIR)

    tasks = []
    to_configure = []
    for name, wrap in UI_TEXTURE_MANIFEST:
        ue_path = "%s/%s" % (UE_TEX_DIR, name)
        png_path = os.path.join(OUT_DIR, name + ".png")
        if not os.path.isfile(png_path):
            raise RuntimeError("Missing UI texture PNG: " + png_path)
        if force_reimport or not assets.does_asset_exist(ue_path):
            task = unreal.AssetImportTask()
            task.filename = png_path
            task.destination_path = UE_TEX_DIR
            task.destination_name = name
            task.replace_existing = True
            task.automated = True
            task.save = False
            tasks.append(task)
            to_configure.append((name, wrap))

    if tasks:
        tools.import_asset_tasks(tasks)

    ui_compression = getattr(
        unreal.TextureCompressionSettings,
        "TC_EDITOR_ICON",
        unreal.TextureCompressionSettings.TC_DEFAULT,
    )
    no_mips = getattr(unreal.TextureMipGenSettings, "NO_MIPMAPS", getattr(unreal.TextureMipGenSettings, "TMGS_NO_MIPS", None))
    addr_wrap = getattr(unreal.TextureAddress, "TA_WRAP", getattr(unreal.TextureAddress, "WRAP", None))
    addr_clamp = getattr(unreal.TextureAddress, "TA_CLAMP", getattr(unreal.TextureAddress, "CLAMP", None))
    count = 0
    for name, wrap in UI_TEXTURE_MANIFEST:
        ue_path = "%s/%s" % (UE_TEX_DIR, name)
        tex = unreal.load_asset(ue_path)
        if tex is None:
            raise RuntimeError("Failed to import/load UI texture: " + ue_path)
        dirty = (name, wrap) in to_configure
        if tex.get_editor_property("compression_settings") != ui_compression:
            tex.set_editor_property("compression_settings", ui_compression)
            dirty = True
        if tex.get_editor_property("lod_group") != unreal.TextureGroup.TEXTUREGROUP_UI:
            tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
            dirty = True
        if not tex.get_editor_property("srgb"):
            tex.set_editor_property("srgb", True)
            dirty = True
        if not tex.get_editor_property("never_stream"):
            tex.set_editor_property("never_stream", True)
            dirty = True
        if no_mips is not None and tex.get_editor_property("mip_gen_settings") != no_mips:
            tex.set_editor_property("mip_gen_settings", no_mips)
            dirty = True
        addr = addr_wrap if wrap == "wrap" else addr_clamp
        if addr is not None:
            if tex.get_editor_property("address_x") != addr:
                tex.set_editor_property("address_x", addr)
                dirty = True
            if tex.get_editor_property("address_y") != addr:
                tex.set_editor_property("address_y", addr)
                dirty = True
        if dirty:
            assets.save_loaded_asset(tex, only_if_is_dirty=False)
        count += 1

    unreal.log("BN: ensured %d UI textures under %s" % (count, UE_TEX_DIR))
    return count


if __name__ == "__main__":
    if "--generate-only" in sys.argv:
        n = generate_all_pngs(force="--force" in sys.argv)
        print("BN: generated %d UI textures in %s" % (n, OUT_DIR))
    else:
        try:
            import unreal  # noqa: F401
            ensure_ue_ui_textures(force_reimport="--force" in sys.argv)
        except ImportError:
            n = generate_all_pngs(force="--force" in sys.argv)
            print("BN: generated %d UI textures in %s (outside UE)" % (n, OUT_DIR))
