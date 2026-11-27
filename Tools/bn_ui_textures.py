"""
Generates and imports authentic NFS Most Wanted (2005)-inspired grunge, India-ink splatter,
stencil icon, and motorsport HUD textures for BLACK NUMBERS into /Game/UI/Textures/.

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
    ("T_UI_VignetteLeft", "clamp"),
    ("T_UI_VignetteFull", "clamp"),
    ("T_UI_Icon_Race", "clamp"),
    ("T_UI_Icon_Milestone", "clamp"),
    ("T_UI_Icon_Bounty", "clamp"),
    ("T_UI_Icon_Duel", "clamp"),
    ("T_UI_Icon_Evidence", "clamp"),
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
    from PIL import Image, ImageDraw

    os.makedirs(OUT_DIR, exist_ok=True)
    if not force and _all_pngs_exist():
        return len(UI_TEXTURE_MANIFEST)

    # 1. Copy & normalize the 4 original concept artworks
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


    # 4. T_UI_VignetteLeft (640x1080 portrait): clean smoothstep ink falloff behind the left menu column (no splatter).
    vw, vh = 640, 1080
    vu = np.tile(np.linspace(0.0, 1.0, vw, dtype=np.float32), (vh, 1))
    t = np.clip((vu - 0.38) / 0.62, 0.0, 1.0)
    v_alpha = (1.0 - t * t * (3.0 - 2.0 * t)) * 236.0
    vleft = np.zeros((vh, vw, 4), dtype=np.float32)
    vleft[:, :, 0:3] = (10.0, 11.0, 13.0)
    vleft[:, :, 3] = v_alpha
    _save_rgba("T_UI_VignetteLeft", vleft)

    # 5. Stencil street icons (256x256, 2x supersampled from 512x512, pure white RGBA -> tintable in Slate!)
    def make_stencil_icon(name, draw_fn):
        im = Image.new("RGBA", (512, 512), (0, 0, 0, 0))
        d = ImageDraw.Draw(im)
        draw_fn(d)
        im.resize((256, 256), Image.LANCZOS).save(os.path.join(OUT_DIR, name + ".png"), format="PNG")

    def draw_icon_race(d):
        # Tilted MW checkered racing flag card (exact silhouette from Izzy reference photo!)
        tmp = Image.new("RGBA", (512, 512), (0, 0, 0, 0))
        td = ImageDraw.Draw(tmp)
        td.rounded_rectangle((96, 128, 416, 384), radius=24, fill=(255, 255, 255, 255))
        # Cutout checkered grid inside
        cols, rows = 5, 3
        cw, ch = 50, 52
        ox, oy = 130, 178
        for r in range(rows):
            for c in range(cols):
                if (r + c) % 2 == 0:
                    td.rectangle((ox + c * cw, oy + r * ch, ox + (c + 1) * cw - 4, oy + (r + 1) * ch - 4), fill=(0, 0, 0, 0))
        # Top folder tab notch
        td.rectangle((140, 96, 310, 140), fill=(255, 255, 255, 255))
        td.rectangle((160, 114, 290, 134), fill=(0, 0, 0, 0))
        rot = tmp.rotate(-22.0, resample=Image.BICUBIC)
        d._image.alpha_composite(rot)

    def draw_icon_milestone(d):
        # Rounded triangle with bold cutout checkmark (exact match to MW Milestones icon in photo!)
        pts = [(256, 78), (434, 396), (78, 396)]
        d.polygon(pts, fill=(255, 255, 255, 255))
        d.ellipse((230, 62, 282, 114), fill=(255, 255, 255, 255))
        d.ellipse((404, 366, 456, 418), fill=(255, 255, 255, 255))
        d.ellipse((56, 366, 108, 418), fill=(255, 255, 255, 255))
        # Inner dark triangle cutout
        d.polygon([(256, 152), (370, 354), (142, 354)], fill=(0, 0, 0, 0))
        # Bold white checkmark rising across the center
        d.line([(182, 284), (242, 344), (358, 212)], fill=(255, 255, 255, 255), width=36, joint="curve")

    def draw_icon_bounty(d):
        # Bold Police Shield / Rap Sheet Badge (exact match to MW Bounty shield icon in photo!)
        shield = [(256, 76), (396, 116), (382, 298), (256, 434), (130, 298), (116, 116)]
        d.polygon(shield, fill=(255, 255, 255, 255))
        inner = [(256, 116), (358, 146), (348, 282), (256, 386), (164, 282), (154, 146)]
        d.polygon(inner, fill=(0, 0, 0, 0))
        # Center badge banner + star
        d.rectangle((164, 194, 348, 248), fill=(255, 255, 255, 255))
        d.rectangle((188, 212, 324, 230), fill=(0, 0, 0, 0))
        # Five-point star underneath banner
        spts = []
        for k in range(10):
            ang_s = math.radians(k * 36.0 - 90.0)
            rr = 44.0 if k % 2 == 0 else 20.0
            spts.append((256 + math.cos(ang_s) * rr, 314 + math.sin(ang_s) * rr))
        d.polygon(spts, fill=(255, 255, 255, 255))

    def draw_icon_duel(d):
        # Iconic crossed street-duel double flags / lightning bolt crest
        d.line([(116, 412), (372, 108)], fill=(255, 255, 255, 255), width=26)
        d.line([(396, 412), (140, 108)], fill=(255, 255, 255, 255), width=26)
        # Crown / lightning bolt at center
        d.polygon([(274, 88), (190, 266), (258, 266), (228, 424), (336, 224), (266, 224)], fill=(255, 255, 255, 255))

    def draw_icon_evidence(d):
        # Dossier folder with horizontal intel stripes
        d.polygon([(88, 130), (226, 130), (256, 164), (424, 164), (424, 396), (88, 396)], fill=(255, 255, 255, 255))
        for y_bar in (224, 276, 328):
            d.rectangle((132, y_bar, 380, y_bar + 22), fill=(0, 0, 0, 0))

    make_stencil_icon("T_UI_Icon_Race", draw_icon_race)
    make_stencil_icon("T_UI_Icon_Milestone", draw_icon_milestone)
    make_stencil_icon("T_UI_Icon_Bounty", draw_icon_bounty)
    make_stencil_icon("T_UI_Icon_Duel", draw_icon_duel)
    make_stencil_icon("T_UI_Icon_Evidence", draw_icon_evidence)

    # 6. T_UI_VignetteFull (512x512): neutral cinema vignette (cool ink, no colour cast).
    vs = 512
    yn, xn = np.mgrid[0:vs, 0:vs].astype(np.float32)
    nx = (xn / (vs - 1.0)) * 2.0 - 1.0
    ny = (yn / (vs - 1.0)) * 2.0 - 1.0
    rad = np.sqrt(nx * nx * 0.85 + ny * ny * 1.15)
    v_alpha = np.clip((rad - 0.55) / 0.75, 0.0, 1.0)
    vfull = np.zeros((vs, vs, 4), dtype=np.float32)
    vfull[:, :, 0:3] = (10.0, 11.0, 13.0)
    vfull[:, :, 3] = v_alpha * v_alpha * 210.0
    _save_rgba("T_UI_VignetteFull", vfull)

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
        uasset_path = os.path.join(PROJECT_DIR, "Content", "UI", "Textures", name + ".uasset")
        is_stale = os.path.isfile(uasset_path) and os.path.getmtime(png_path) > os.path.getmtime(uasset_path)
        if force_reimport or is_stale or not assets.does_asset_exist(ue_path):
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
