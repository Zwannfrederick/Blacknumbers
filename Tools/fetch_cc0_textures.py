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
    ("WoodCrate",   "wood_planks",          "direct"),
    ("Bark",        "bark_brown_02",        "direct"),
    ("Brick",       "red_brick_03",         "facade"),
    ("Plaster",     "clay_plaster",         "facade"),
]

UA = {"User-Agent": "BlackNumbers-UE5-CC0Fetcher/1.0"}


def _manifest_signature():
    payload = json.dumps(CC0_SURFACE_MAP, sort_keys=True) + "|v1"
    return hashlib.sha256(payload.encode("utf-8")).hexdigest()[:16]


def _download_bytes(url, timeout=20):
    req = urllib.request.Request(url, headers=UA)
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return resp.read()


def _pick_1k_url(file_tree, map_key):
    entry = file_tree.get(map_key, {}).get("1k", {})
    for fmt in ("png", "jpg"):
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


def fetch_and_apply(force=False):
    """Downloads CC0 PolyHaven textures (or uses local cache) and updates Saved/GeneratedTextures/."""
    import numpy as np

    os.makedirs(CACHE_DIR, exist_ok=True)
    os.makedirs(OUT_DIR, exist_ok=True)

    sig = _manifest_signature()
    if not force and os.path.isfile(STAMP_PATH):
        with open(STAMP_PATH, "r", encoding="utf-8") as f:
            if f.read().strip() == sig and os.path.isfile(os.path.join(OUT_DIR, "T_Rock_D.png")):
                print("BN: CC0 PBR textures already up to date (%d sets)" % len(CC0_SURFACE_MAP))
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
                # Convert corrugated steel to clean tintable luminance with subtle rust retained
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
                bd, bn, br, be = _composite_karamara_windows(tile_d, tile_n, tile_r, is_brick=(prefix == "Brick"))
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

    if failed == 0:
        with open(STAMP_PATH, "w", encoding="utf-8") as f:
            f.write(sig + "\n")
    print("BN: applied %d/%d CC0 1.0 PolyHaven PBR texture sets into %s" % (applied, len(CC0_SURFACE_MAP), OUT_DIR))
    return applied


if __name__ == "__main__":
    fetch_and_apply(force="--force" in sys.argv)
