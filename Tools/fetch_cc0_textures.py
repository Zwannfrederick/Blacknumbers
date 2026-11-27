"""Fetches 100% CC0 1.0 Public Domain 1K PBR textures from PolyHaven (https://polyhaven.com/license),
caches them locally under Saved/CC0Cache/, and writes UE5-ready 1024x1024 seamless PNGs into
Saved/GeneratedTextures/ (preserving procedural fallback automatically if offline).

Also composites Karamara's dark daytime glass + NightFactor warm-lit window grid onto real CC0
brick (red_brick_03) and stucco plaster (clay_plaster) facades so buildings keep full day/night
emissive life with photorealistic wall materials.
"""
import hashlib
import io
import json
import os
import sys
import urllib.request

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CACHE_DIR = os.path.join(PROJECT_DIR, "Saved", "CC0Cache")
OUT_DIR = os.path.join(PROJECT_DIR, "Saved", "GeneratedTextures")
STAMP_PATH = os.path.join(OUT_DIR, ".cc0_manifest_hash")
TEX_SIZE = 1024

# All assets below are CC0 1.0 Universal (Public Domain) from PolyHaven.
# Tuple: (target_prefix, polyhaven_id, mode)
#   mode="direct"     -> writes T_<prefix>_{D,N,R}.png directly
#   mode="neutralize" -> normalizes diffuse brightness to ~0.78 neutral grey so UE MI BaseColor tints cleanly
#   mode="facade"     -> composites Karamara architectural windows + emissive onto the CC0 wall maps
CC0_SURFACE_MAP = [
    ("Asphalt",     "asphalt_02",           "direct"),
    ("Concrete",    "concrete_floor_02",    "direct"),
    ("Sidewalk",    "precast_stone_paving", "direct"),
    ("Cobblestone", "cobblestone_02",       "direct"),
    ("Dirt",        "dirt_aerial_02",       "direct"),
    ("Grass",       "sparse_grass",         "direct"),
    ("Rock",        "aerial_rocks_04",      "direct"),
    ("Container",   "corrugated_iron_02",   "neutralize"),
    ("BridgeSteel", "rusty_metal_02",       "neutralize"),
    ("WoodCrate",   "wood_planks",          "direct"),
    ("Bark",        "bark_brown_02",        "direct"),
    ("Brick",             "red_brick_03",             "facade"),
    ("Plaster",           "clay_plaster",             "facade"),
    ("Cladding",          "exterior_wall_cladding_02","facade"),
    ("ConcreteTower",     "concrete_tile_facade",     "facade"),
    ("WallBrick",         "red_brick_03",             "direct"),
    ("WallPlaster",       "clay_plaster",             "direct"),
    ("WallCladding",      "exterior_wall_cladding_02","direct"),
    ("WallConcretePanel", "concrete_tile_facade",     "direct"),
    ("PitAsphalt",    "asphalt_pit_lane",         "direct"),
    ("CliffRock",     "cliff_side",               "direct"),
    ("MountainGrass", "aerial_grass_rock",        "direct"),
    ("RoofTiles",     "clay_roof_tiles_02",       "direct"),
]

# Photographic building facades (real window frames, sills, night-lit window emission), CC0 1.0 from
# ambientCG (https://ambientcg.com/). These replace the composited window grids on city buildings.
# Tuple: (target_prefix, ambientcg_id)
AMBIENTCG_FACADES = [
    ("Brick",         "Facade018B"),  # brick apartment block
    ("Plaster",       "Facade018A"),  # brick/stone apartment block, darker glazing
    ("Cladding",      "Facade019A"),  # modern banded office
    ("ConcreteTower", "Facade020A"),  # brick-pier mid-rise tower
    ("GlassSteel",    "Facade006"),   # glass curtain-wall office
]
AMBIENTCG_CACHE_DIR = os.path.join(CACHE_DIR, "ambientcg")

UA = {"User-Agent": "BlackNumbers-UE5-CC0Fetcher/1.0"}


def _manifest_signature():
    payload = json.dumps([CC0_SURFACE_MAP, AMBIENTCG_FACADES], sort_keys=True) + "|v4_arch_walls"
    return hashlib.sha256(payload.encode("utf-8")).hexdigest()[:16]


def _download_bytes(url, timeout=20):
    req = urllib.request.Request(url, headers=UA)
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return resp.read()


def _pick_1k_url(file_tree, map_key):
    entry = file_tree.get(map_key, {}).get("1k", {})
    # Poly Haven roughness PNGs can be 16-bit, which Pillow clips when converting to L.
    formats = ("jpg", "png") if map_key == "Rough" else ("png", "jpg")
    for fmt in formats:
        if fmt in entry and "url" in entry[fmt]:
            return entry[fmt]["url"], fmt
    return None, None


def _ensure_cached_asset(asset_id):
    asset_dir = os.path.join(CACHE_DIR, asset_id)
    os.makedirs(asset_dir, exist_ok=True)
    needed = ("Diffuse", "nor_dx", "Rough")
    paths = {}
    missing = []
    for k in needed:
        found = None
        for ext in ("png", "jpg"):
            cand = os.path.join(asset_dir, "%s.%s" % (k, ext))
            if os.path.isfile(cand) and os.path.getsize(cand) > 1024:
                found = cand
                break
        if found:
            paths[k] = found
        else:
            missing.append(k)

    if not missing:
        return paths

    meta_bytes = _download_bytes("https://api.polyhaven.com/files/%s" % asset_id, timeout=15)
    meta = json.loads(meta_bytes.decode("utf-8"))
    for k in missing:
        url, fmt = _pick_1k_url(meta, k)
        if not url:
            raise RuntimeError("PolyHaven asset %s missing 1k %s" % (asset_id, k))
        raw = _download_bytes(url, timeout=25)
        out_path = os.path.join(asset_dir, "%s.%s" % (k, fmt))
        with open(out_path, "wb") as f:
            f.write(raw)
        paths[k] = out_path
    return paths


def _load_rgb_f32(path, size=TEX_SIZE):
    import numpy as np
    from PIL import Image

    img = Image.open(path).convert("RGB")
    if img.size != (size, size):
        img = img.resize((size, size), Image.Resampling.LANCZOS)
    return np.asarray(img, dtype=np.float32) / 255.0


def _load_gray_f32(path, size=TEX_SIZE):
    import numpy as np
    from PIL import Image

    img = Image.open(path).convert("L")
    if img.size != (size, size):
        img = img.resize((size, size), Image.Resampling.LANCZOS)
    return np.asarray(img, dtype=np.float32) / 255.0


def _save_rgb(name, arr):
    import numpy as np
    from PIL import Image

    rgb = np.clip(arr * 255.0, 0, 255).astype(np.uint8)
    Image.fromarray(rgb, mode="RGB").save(os.path.join(OUT_DIR, name + ".png"), optimize=False)


def _save_gray(name, arr):
    import numpy as np
    from PIL import Image

    g = np.clip(arr * 255.0, 0, 255).astype(np.uint8)
    rgb = np.stack([g, g, g], axis=-1)
    Image.fromarray(rgb, mode="RGB").save(os.path.join(OUT_DIR, name + ".png"), optimize=False)


def _composite_karamara_windows(wall_d, wall_n, wall_r, is_brick):
    """Overlays crisp Karamara apartment windows, recessed frames, concrete sills, and
    NightFactor warm/cool lit interior emissive masks onto a real CC0 masonry/plaster wall."""
    import numpy as np

    N = TEX_SIZE
    rng = np.random.default_rng(301 if is_brick else 302)
    albedo = wall_d.copy()
    normal = wall_n.copy()
    rough = wall_r.copy()
    emissive = np.zeros((N, N, 3), dtype=np.float32)

    cols, rows = 4, 4
    win_w_frac = 0.50 if is_brick else 0.48
    win_h_frac = 0.56 if is_brick else 0.54
    frame_color = np.array((0.14, 0.13, 0.12) if is_brick else (0.19, 0.16, 0.13), dtype=np.float32)
    glass_color = np.array((0.05, 0.08, 0.11) if is_brick else (0.06, 0.09, 0.12), dtype=np.float32)
    lit_prob = 0.54 if is_brick else 0.58
    flat_n = np.array((0.5, 0.5, 1.0), dtype=np.float32)

    cw, ch = N // cols, N // rows
    pad_x = int(cw * (1.0 - win_w_frac) * 0.5)
    pad_y = int(ch * (1.0 - win_h_frac) * 0.5)
    frame_px = max(5, cw // 34)
    sill_h = max(9, ch // 26)

    # Subtle plaster floor cornice band
    if not is_brick:
        for r in range(rows):
            by0 = r * ch
            albedo[by0:by0 + 10, :] *= 0.88
            normal[by0:by0 + 10, :] = np.array((0.5, 0.34, 0.92), dtype=np.float32)

    for r in range(rows):
        for c in range(cols):
            x0 = c * cw + pad_x
            x1 = (c + 1) * cw - pad_x
            y0 = r * ch + pad_y
            y1 = (r + 1) * ch - pad_y

            # Concrete sill & top lintel shadow
            sill_y1 = min(N, y1 + sill_h)
            sx0, sx1 = max(0, x0 - 7), min(N, x1 + 7)
            albedo[y1:sill_y1, sx0:sx1] = (0.50, 0.48, 0.44)
            normal[y1:sill_y1, sx0:sx1] = (0.50, 0.28, 0.90)
            rough[y1:sill_y1, sx0:sx1] = 0.78

            lintel_y0 = max(0, y0 - 7)
            albedo[lintel_y0:y0, sx0:sx1] *= 0.68
            normal[lintel_y0:y0, sx0:sx1] = (0.50, 0.72, 0.90)

            # Outer recessed window frame
            albedo[y0:y1, x0:x1] = frame_color
            normal[y0:y1, x0:x1] = flat_n
            rough[y0:y1, x0:x1] = 0.46

            # Reflective dark daytime glass pane
            gx0, gx1 = x0 + frame_px, x1 - frame_px
            gy0, gy1 = y0 + frame_px, y1 - frame_px
            pane_h = gy1 - gy0
            grad = np.linspace(0.70, 1.20, pane_h, dtype=np.float32)[:, None]
            albedo[gy0:gy1, gx0:gx1] = glass_color * grad[:, :, None]
            normal[gy0:gy1, gx0:gx1] = flat_n
            rough[gy0:gy1, gx0:gx1] = 0.06

            # Center vertical mullion & upper transom bar
            mx = (gx0 + gx1) // 2
            ty = gy0 + pane_h // 3
            albedo[gy0:gy1, mx - 2:mx + 2] = frame_color
            rough[gy0:gy1, mx - 2:mx + 2] = 0.44
            albedo[ty - 2:ty + 2, gx0:gx1] = frame_color
            rough[ty - 2:ty + 2, gx0:gx1] = 0.44

            if rng.random() < lit_prob:
                intensity = rng.uniform(0.48, 1.0)
                if rng.random() < 0.14:
                    col = np.array([0.76, 0.88, 1.0], dtype=np.float32) * intensity
                else:
                    col = np.array([1.0, rng.uniform(0.68, 0.85), rng.uniform(0.32, 0.52)], dtype=np.float32) * intensity
                blind_cut = rng.uniform(0.14, 0.40)
                y_norm = np.linspace(0.0, 1.0, pane_h, dtype=np.float32)[:, None]
                t = np.clip((y_norm - blind_cut) / 0.16, 0.0, 1.0)
                room_grad = 0.35 + 0.65 * (t * t * (3.0 - 2.0 * t))
                emissive[gy0:gy1, gx0:gx1] = col[None, None, :] * room_grad[:, :, None]
                emissive[gy0:gy1, mx - 2:mx + 2] = 0.0
                emissive[ty - 2:ty + 2, gx0:gx1] = 0.0

    return albedo, normal, rough, emissive


def _fetch_puresky_clouds():
    """Downloads Poly Haven CC0 autumn_field_puresky tonemapped sky panorama and extracts a clean
    upper-hemisphere cloud layer into T_SkyClouds_D.png for M_BN_SkyClouds."""
    import numpy as np
    from PIL import Image

    cache_path = os.path.join(CACHE_DIR, "autumn_field_puresky_tonemapped.jpg")
    if not (os.path.isfile(cache_path) and os.path.getsize(cache_path) > 10000):
        meta = json.loads(_download_bytes("https://api.polyhaven.com/files/autumn_field_puresky", timeout=15).decode("utf-8"))
        url = meta.get("tonemapped", {}).get("url")
        if not url:
            raise RuntimeError("autumn_field_puresky missing tonemapped url")
        raw = _download_bytes(url, timeout=30)
        with open(cache_path, "wb") as f:
            f.write(raw)

    img = Image.open(cache_path).convert("RGB")
    w, h = img.size
    # Upper 50% of equirectangular panorama is the pure sky & sunset clouds (above horizon)
    sky = img.crop((0, 0, w, h // 2)).resize((TEX_SIZE, TEX_SIZE // 2), Image.Resampling.LANCZOS)
    arr = np.asarray(sky, dtype=np.float32) / 255.0
    # Extract soft cloud luminosity above clear sky blue so clouds blend additively without greying SkyAtmosphere
    lum = 0.299 * arr[:, :, 0] + 0.587 * arr[:, :, 1] + 0.114 * arr[:, :, 2]
    blue_excess = np.maximum(0.0, arr[:, :, 2] - 0.5 * (arr[:, :, 0] + arr[:, :, 1]))
    cloud_mask = np.clip((lum - 0.42 - blue_excess * 1.4) * 2.2, 0.0, 1.0)
    # Soft fade near bottom horizon seam so it blends invisibly into ExponentialHeightFog
    rows = np.linspace(0.0, 1.0, arr.shape[0], dtype=np.float32)[:, None]
    horizon_fade = np.clip((0.96 - rows) / 0.24, 0.0, 1.0)
    clouds = arr * (cloud_mask * horizon_fade)[:, :, None]
    # Pad/mirror to 1024x1024 square for clean spherical dome sampling
    full = np.zeros((TEX_SIZE, TEX_SIZE, 3), dtype=np.float32)
    full[:TEX_SIZE // 2, :, :] = clouds
    full[TEX_SIZE // 2:, :, :] = clouds[::-1, :, :] * 0.25
    _save_rgb("T_SkyClouds_D", full)


def _apply_ambientcg_facade(prefix, asset_id):
    """Writes T_<prefix>_{D,N,R,E} from a cached/downloaded ambientCG 1K-JPG facade zip."""
    import zipfile
    import numpy as np

    os.makedirs(AMBIENTCG_CACHE_DIR, exist_ok=True)
    zip_path = os.path.join(AMBIENTCG_CACHE_DIR, asset_id + ".zip")
    if not os.path.isfile(zip_path):
        data = _download_bytes("https://ambientcg.com/get?file=%s_1K-JPG.zip" % asset_id, timeout=120)
        with open(zip_path, "wb") as f:
            f.write(data)
    with zipfile.ZipFile(zip_path) as z:
        names = z.namelist()

        def member(suffix):
            hit = [n for n in names if n.endswith("_%s.jpg" % suffix)]
            return io.BytesIO(z.read(hit[0])) if hit else None

        color, normal, rough, emission = member("Color"), member("NormalDX"), member("Roughness"), member("Emission")
        if color is None or normal is None or rough is None:
            raise RuntimeError("incomplete ambientCG zip: %s" % names)
        diff = _load_rgb_f32(color)
        rgh = _load_gray_f32(rough)
        emis = _load_rgb_f32(emission) if emission is not None else np.zeros_like(diff)
        if float((emis.max(axis=-1) > 0.2).mean()) < 0.05:
            emis = _night_windows(diff, rgh, seed=sum(map(ord, asset_id)))
        _save_rgb("T_%s_D" % prefix, diff)
        _save_rgb("T_%s_N" % prefix, _load_rgb_f32(normal))
        _save_gray("T_%s_R" % prefix, rgh)
        _save_rgb("T_%s_E" % prefix, emis)


def _intervals(profile, thresh):
    """Runs of indices where profile > thresh, as (start, end) pairs (end exclusive)."""
    runs, start = [], None
    for i, v in enumerate(profile):
        if v > thresh and start is None:
            start = i
        elif v <= thresh and start is not None:
            runs.append((start, i))
            start = None
    if start is not None:
        runs.append((start, len(profile)))
    return [r for r in runs if r[1] - r[0] >= 4]


def _night_windows(diff, rgh, seed, lit_fraction=0.42):
    """Facade has no usable emission map: light ~42% of its window cells warm/cool at night.
    Window cells come from the glass mask's row/column profiles (works for regular grids)."""
    import numpy as np

    lum = diff.mean(axis=-1)
    glass = rgh < 0.3
    if glass.mean() > 0.8:  # all-glass curtain wall: panes are the darker pixels
        glass = lum < np.percentile(lum, 55)
    rows = _intervals(glass.mean(axis=1), 0.25)
    cols = _intervals(glass.mean(axis=0), 0.25)
    rng = np.random.default_rng(seed)
    emis = np.zeros_like(diff)
    warm, cool = np.array([1.0, 0.78, 0.45]), np.array([0.72, 0.86, 1.0])
    for r0, r1 in rows:
        for c0, c1 in cols:
            if rng.random() < lit_fraction:
                tint = warm if rng.random() < 0.75 else cool
                cell = glass[r0:r1, c0:c1, None] * tint * rng.uniform(0.55, 1.0)
                emis[r0:r1, c0:c1] = np.maximum(emis[r0:r1, c0:c1], cell)
    return emis


def fetch_and_apply(force=False):
    """Downloads CC0 PolyHaven textures (or uses local cache) and updates Saved/GeneratedTextures/."""
    import numpy as np

    os.makedirs(CACHE_DIR, exist_ok=True)
    os.makedirs(OUT_DIR, exist_ok=True)

    sig = _manifest_signature()
    if not force and os.path.isfile(STAMP_PATH):
        with open(STAMP_PATH, "r", encoding="utf-8") as f:
            if f.read().strip() == sig and os.path.isfile(os.path.join(OUT_DIR, "T_SkyClouds_D.png")):
                print("BN: CC0 PBR textures already up to date (%d sets + PureSky)" % len(CC0_SURFACE_MAP))
                return 0

    applied = 0
    failed = 0
    for prefix, asset_id, mode in CC0_SURFACE_MAP:
        try:
            files = _ensure_cached_asset(asset_id)
            diff = _load_rgb_f32(files["Diffuse"])
            nrm = _load_rgb_f32(files["nor_dx"])
            rgh = _load_gray_f32(files["Rough"])

            if mode == "neutralize":
                # Convert metal albedo to tintable luminance while retaining surface wear.
                lum = 0.299 * diff[:, :, 0] + 0.587 * diff[:, :, 1] + 0.114 * diff[:, :, 2]
                mean_l = max(float(lum.mean()), 0.15)
                scaled = np.clip(lum * (0.78 / mean_l), 0.22, 0.98)
                diff = np.stack([scaled, scaled, scaled], axis=-1)
                _save_rgb("T_%s_D" % prefix, diff)
                _save_rgb("T_%s_N" % prefix, nrm)
                _save_gray("T_%s_R" % prefix, rgh)
            elif mode == "facade":
                # Tile CC0 masonry/plaster 2x2 across the 1024px 4-story facade so brick courses have realistic scale
                tile_d = np.tile(diff[::2, ::2, :], (2, 2, 1))
                tile_n = np.tile(nrm[::2, ::2, :], (2, 2, 1))
                tile_r = np.tile(rgh[::2, ::2], (2, 2))
                bd, bn, br, be = _composite_karamara_windows(tile_d, tile_n, tile_r, is_brick=(prefix in ("Brick", "Cladding")))
                _save_rgb("T_%s_D" % prefix, bd)
                _save_rgb("T_%s_N" % prefix, bn)
                _save_gray("T_%s_R" % prefix, br)
                _save_rgb("T_%s_E" % prefix, be)
            else:
                _save_rgb("T_%s_D" % prefix, diff)
                _save_rgb("T_%s_N" % prefix, nrm)
                _save_gray("T_%s_R" % prefix, rgh)
            applied += 1
        except Exception as exc:
            failed += 1
            print("BN: CC0 fallback to procedural for %s (%s): %s" % (prefix, asset_id, exc), file=sys.stderr)

    for prefix, asset_id in AMBIENTCG_FACADES:
        try:
            _apply_ambientcg_facade(prefix, asset_id)
            applied += 1
        except Exception as exc:
            failed += 1
            print("BN: ambientCG facade fallback for %s (%s): %s" % (prefix, asset_id, exc), file=sys.stderr)

    try:
        _fetch_puresky_clouds()
    except Exception as exc:
        failed += 1
        print("BN: CC0 fallback to procedural clouds (autumn_field_puresky): %s" % exc, file=sys.stderr)

    if failed == 0:
        with open(STAMP_PATH, "w", encoding="utf-8") as f:
            f.write(sig + "\n")
    print("BN: applied %d/%d CC0 1.0 PolyHaven + ambientCG texture sets + PureSky into %s" % (applied, len(CC0_SURFACE_MAP) + len(AMBIENTCG_FACADES), OUT_DIR))
    return applied


if __name__ == "__main__":
    fetch_and_apply(force="--force" in sys.argv)
