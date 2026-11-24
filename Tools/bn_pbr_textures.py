"""Generates 100% original seamless 1024x1024 PBR texture maps for BLACK NUMBERS
under Saved/GeneratedTextures/ using Python3 + PIL + numpy, and imports them
into UE5 under /Game/World/Textures/ via AssetTools.import_asset_tasks.

Can be run standalone with system python3 (PNG generation only):
    python3 Tools/bn_pbr_textures.py --generate-only
Or imported inside UE5 Python (auto-delegates PNG generation to system python3
if PIL/numpy are absent from Unreal's bundled Python, then imports into UE5):
    import bn_pbr_textures
    textures = bn_pbr_textures.ensure_ue_textures()
"""
import math
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bn_pbr_extra_textures  # noqa: E402

TEX_SIZE = 1024
UTIL_SIZE = 64
UE_TEX_DIR = "/Game/World/Textures"

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(PROJECT_DIR, "Saved", "GeneratedTextures")

# Map key -> (asset_name, kind) where kind in {"albedo", "normal", "roughness", "emissive"}
TEXTURE_MANIFEST = [
    ("T_White_D", "albedo"),
    ("T_Flat_N", "normal"),
    ("T_White_R", "roughness"),
    ("T_Black_E", "emissive"),
    ("T_Asphalt_D", "albedo"),
    ("T_Asphalt_N", "normal"),
    ("T_Asphalt_R", "roughness"),
    ("T_Concrete_D", "albedo"),
    ("T_Concrete_N", "normal"),
    ("T_Concrete_R", "roughness"),
    ("T_Sidewalk_D", "albedo"),
    ("T_Sidewalk_N", "normal"),
    ("T_Sidewalk_R", "roughness"),
    ("T_Brick_D", "albedo"),
    ("T_Brick_N", "normal"),
    ("T_Brick_R", "roughness"),
    ("T_Brick_E", "emissive"),
    ("T_Plaster_D", "albedo"),
    ("T_Plaster_N", "normal"),
    ("T_Plaster_R", "roughness"),
    ("T_Plaster_E", "emissive"),
    ("T_GlassSteel_D", "albedo"),
    ("T_GlassSteel_N", "normal"),
    ("T_GlassSteel_R", "roughness"),
    ("T_GlassSteel_E", "emissive"),
    ("T_Container_D", "albedo"),
    ("T_Container_N", "normal"),
    ("T_Container_R", "roughness"),
    ("T_Dirt_D", "albedo"),
    ("T_Dirt_N", "normal"),
    ("T_Dirt_R", "roughness"),
    ("T_Grass_D", "albedo"),
    ("T_Grass_N", "normal"),
    ("T_Grass_R", "roughness"),
    ("T_WoodCrate_D", "albedo"),
    ("T_WoodCrate_N", "normal"),
    ("T_WoodCrate_R", "roughness"),
    ("T_HazardMetal_D", "albedo"),
    ("T_HazardMetal_N", "normal"),
    ("T_HazardMetal_R", "roughness"),
    ("T_Water_N", "normal"),
    ("T_Foliage_D", "albedo"),
    ("T_Foliage_N", "normal"),
    ("T_Foliage_R", "roughness"),
    ("T_Bark_D", "albedo"),
    ("T_Bark_N", "normal"),
    ("T_Bark_R", "roughness"),
] + bn_pbr_extra_textures.EXTRA_TEXTURE_MANIFEST


# ---------------------------------------------------------------------------
# Pure Python3 + numpy + PIL procedural seamless PBR texture generation
# ---------------------------------------------------------------------------
def _generate_all_pngs_impl(force=False):
    import numpy as np
    from PIL import Image

    os.makedirs(OUT_DIR, exist_ok=True)
    if not force and all(os.path.isfile(os.path.join(OUT_DIR, name + ".png")) for name, _ in TEXTURE_MANIFEST):
        return len(TEXTURE_MANIFEST)

    N = TEX_SIZE

    def save_rgb(name, arr):
        rgb = np.clip(arr * 255.0, 0, 255).astype(np.uint8)
        Image.fromarray(rgb, mode="RGB").save(os.path.join(OUT_DIR, name + ".png"), optimize=False)

    def save_gray(name, arr):
        g = np.clip(arr * 255.0, 0, 255).astype(np.uint8)
        rgb = np.stack([g, g, g], axis=-1)
        Image.fromarray(rgb, mode="RGB").save(os.path.join(OUT_DIR, name + ".png"), optimize=False)

    def save_normal(name, height, strength=3.0):
        # Seamless central differences with periodic wrap; UE5 DirectX-style tangent normal (+X right, -Y up)
        dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * 0.5
        dy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * 0.5
        nx = -dx * strength
        ny = -dy * strength
        nz = np.ones_like(height, dtype=np.float32)
        inv = 1.0 / np.sqrt(nx * nx + ny * ny + nz * nz + 1e-8)
        nrm = np.stack([(nx * inv) * 0.5 + 0.5, (ny * inv) * 0.5 + 0.5, (nz * inv) * 0.5 + 0.5], axis=-1)
        save_rgb(name, nrm)

    def periodic_noise(rng, size, grid):
        """Seamless smooth periodic value noise by bicubic-like cosine interpolation on a toroidal grid."""
        g = rng.random((grid, grid), dtype=np.float32)
        coords = np.linspace(0.0, float(grid), size, endpoint=False, dtype=np.float32)
        i0 = np.floor(coords).astype(np.int32) % grid
        i1 = (i0 + 1) % grid
        f = coords - np.floor(coords)
        f = f * f * (3.0 - 2.0 * f)
        fx = f[None, :]
        fy = f[:, None]
        c00 = g[np.ix_(i0, i0)]
        c10 = g[np.ix_(i0, i1)]
        c01 = g[np.ix_(i1, i0)]
        c11 = g[np.ix_(i1, i1)]
        top = c00 * (1.0 - fx) + c10 * fx
        bot = c01 * (1.0 - fx) + c11 * fx
        return top * (1.0 - fy) + bot * fy

    def fbm(rng, size, octaves):
        """Weighted sum of periodic_noise layers: octaves = [(grid, weight), ...]."""
        acc = np.zeros((size, size), dtype=np.float32)
        wsum = 0.0
        for grid, w in octaves:
            acc += periodic_noise(rng, size, grid) * w
            wsum += w
        return acc / max(wsum, 1e-6)

    def smoothstep(edge0, edge1, x):
        t = np.clip((x - edge0) / max(edge1 - edge0, 1e-6), 0.0, 1.0)
        return t * t * (3.0 - 2.0 * t)

    # 0. Utility textures
    save_rgb("T_White_D", np.ones((UTIL_SIZE, UTIL_SIZE, 3), dtype=np.float32))
    flat_n = np.zeros((UTIL_SIZE, UTIL_SIZE, 3), dtype=np.float32)
    flat_n[:, :, 0] = 0.5
    flat_n[:, :, 1] = 0.5
    flat_n[:, :, 2] = 1.0
    save_rgb("T_Flat_N", flat_n)
    save_gray("T_White_R", np.ones((UTIL_SIZE, UTIL_SIZE), dtype=np.float32))
    save_rgb("T_Black_E", np.zeros((UTIL_SIZE, UTIL_SIZE, 3), dtype=np.float32))

    # 1. Asphalt (cracked/patched dark tarmac + subtle damp puddle roughness variation for SSR)
    rng = np.random.default_rng(101)
    fine_grain = rng.random((N, N), dtype=np.float32)
    med_grain = fbm(rng, N, [(32, 0.35), (64, 0.35), (128, 0.30)])
    macro_patch = fbm(rng, N, [(4, 0.55), (8, 0.30), (16, 0.15)])
    puddles = fbm(rng, N, [(3, 0.50), (6, 0.35), (12, 0.15)])

    # Seamless random-walk tar cracks wrapped toroidally
    crack_mask = np.zeros((N, N), dtype=np.float32)
    for _ in range(9):
        cx, cy = rng.integers(0, N), rng.integers(0, N)
        ang = rng.uniform(0.0, 2.0 * math.pi)
        for _step in range(380):
            ang += rng.uniform(-0.35, 0.35)
            cx = (cx + math.cos(ang) * 2.1) % N
            cy = (cy + math.sin(ang) * 2.1) % N
            ix, iy = int(cx), int(cy)
            for oy in (-1, 0, 1):
                for ox in (-1, 0, 1):
                    w = 1.0 if (ox == 0 and oy == 0) else 0.45
                    px, py = (ix + ox) % N, (iy + oy) % N
                    if w > crack_mask[py, px]:
                        crack_mask[py, px] = w

    # Subtle bitumen repair patch
    yy, xx = np.mgrid[0:N, 0:N]
    patch_rect = ((xx > 180) & (xx < 470) & (yy > 560) & (yy < 840)).astype(np.float32)
    patch_rect = np.clip(
        (np.roll(patch_rect, 2, 0) + np.roll(patch_rect, -2, 0) +
         np.roll(patch_rect, 2, 1) + np.roll(patch_rect, -2, 1) + patch_rect) / 5.0,
        0.0, 1.0,
    )

    # Subtle aggregate & road wear only; macro-scale puddles are driven by world-space noise in M_BN_Surface
    # so damp reflections span 20-45 m naturally instead of repeating every 5.5 m.
    stone = 0.088 + 0.052 * med_grain + 0.042 * (fine_grain - 0.5)
    stone = stone * (0.91 + 0.16 * macro_patch) * (1.0 - 0.14 * patch_rect)
    stone = stone * (1.0 - 0.50 * crack_mask)
    asphalt_d = np.stack([stone * 0.96, stone * 0.98, stone * 1.04], axis=-1)
    asphalt_h = 0.45 * med_grain + 0.35 * fine_grain - 0.55 * crack_mask - 0.06 * patch_rect
    track_band = 0.5 + 0.5 * np.cos((yy / N) * 4.0 * math.pi)
    asphalt_r = 0.88 - 0.12 * med_grain - 0.09 * track_band - 0.25 * crack_mask
    asphalt_r = np.clip(asphalt_r, 0.52, 0.98)
    save_rgb("T_Asphalt_D", asphalt_d)
    save_normal("T_Asphalt_N", asphalt_h, strength=4.2)
    save_gray("T_Asphalt_R", asphalt_r)

    # 2. Concrete (cast concrete panels with expansion joints, formwork tie holes, weathering)
    rng = np.random.default_rng(202)
    c_grain = fbm(rng, N, [(8, 0.35), (24, 0.30), (64, 0.20), (128, 0.15)])
    c_fine = rng.random((N, N), dtype=np.float32)
    joint_x = ((xx % 512) < 5) | ((xx % 512) > 507)
    joint_y = ((yy % 512) < 5) | ((yy % 512) > 507)
    joints = (joint_x | joint_y).astype(np.float32)
    panel_id = (xx // 512) * 2 + (yy // 512)
    panel_tint = np.take(np.array([0.96, 1.02, 1.00, 0.94], dtype=np.float32), panel_id)
    base_c = (0.44 + 0.14 * c_grain + 0.04 * (c_fine - 0.5)) * panel_tint
    base_c *= (1.0 - 0.38 * joints)
    save_rgb("T_Concrete_D", np.stack([base_c * 1.01, base_c * 0.99, base_c * 0.95], axis=-1))
    save_normal("T_Concrete_N", 0.35 * c_grain + 0.15 * c_fine - 0.65 * joints, strength=3.8)
    save_gray("T_Concrete_R", np.clip(0.84 + 0.12 * c_grain - 0.12 * joints, 0.35, 0.98))

    # 3. Sidewalk slabs (4x4 urban paving blocks with crisp bevel joints and grime)
    rng = np.random.default_rng(203)
    sw_n = fbm(rng, N, [(8, 0.35), (32, 0.35), (128, 0.30)])
    sw_fine = rng.random((N, N), dtype=np.float32)
    cell = N // 4  # 256 px per slab
    ux = xx % cell
    uy = yy % cell
    edge_dist = np.minimum(np.minimum(ux, cell - 1 - ux), np.minimum(uy, cell - 1 - uy))
    seam = 1.0 - smoothstep(2.0, 7.0, edge_dist.astype(np.float32))
    slab_tones = rng.uniform(0.90, 1.08, size=(4, 4)).astype(np.float32)
    tone_map = slab_tones[yy // cell, xx // cell]
    sw_val = (0.48 + 0.14 * sw_n + 0.04 * (sw_fine - 0.5)) * tone_map * (1.0 - 0.42 * seam)
    save_rgb("T_Sidewalk_D", np.stack([sw_val * 1.02, sw_val * 1.00, sw_val * 0.95], axis=-1))
    save_normal("T_Sidewalk_N", 0.25 * sw_n + 0.12 * sw_fine - 0.70 * seam, strength=4.5)
    save_gray("T_Sidewalk_R", np.clip(0.82 + 0.12 * sw_n - 0.16 * seam, 0.35, 0.98))

    # Helper to stamp architectural windows + sills + warm night emissive onto building facades
    def apply_windows(rng_w, wall_rgb, wall_h, wall_r, cols, rows, win_w_frac, win_h_frac,
                      frame_color, glass_color, lit_prob, cool_mix=0.18):
        albedo = wall_rgb.copy()
        height = wall_h.copy()
        rough = wall_r.copy()
        emissive = np.zeros((N, N, 3), dtype=np.float32)

        cw = N // cols
        ch = N // rows
        pad_x = int(cw * (1.0 - win_w_frac) * 0.5)
        pad_y = int(ch * (1.0 - win_h_frac) * 0.5)
        frame_px = max(5, cw // 36)
        sill_h = max(8, ch // 28)

        for r in range(rows):
            for c in range(cols):
                x0 = c * cw + pad_x
                x1 = (c + 1) * cw - pad_x
                y0 = r * ch + pad_y
                y1 = (r + 1) * ch - pad_y

                # Concrete sill & lintel
                sill_y1 = min(N, y1 + sill_h)
                albedo[y1:sill_y1, max(0, x0 - 6):min(N, x1 + 6)] = (0.48, 0.46, 0.43)
                height[y1:sill_y1, max(0, x0 - 6):min(N, x1 + 6)] = 0.85

                # Outer window frame
                albedo[y0:y1, x0:x1] = frame_color
                height[y0:y1, x0:x1] = 0.20
                rough[y0:y1, x0:x1] = 0.48

                # Inner glass pane + cross mullion
                gx0, gx1 = x0 + frame_px, x1 - frame_px
                gy0, gy1 = y0 + frame_px, y1 - frame_px
                pane_h = gy1 - gy0
                pane_w = gx1 - gx0
                grad = np.linspace(0.75, 1.15, pane_h, dtype=np.float32)[:, None]
                albedo[gy0:gy1, gx0:gx1] = np.array(glass_color, dtype=np.float32) * grad[:, :, None]
                height[gy0:gy1, gx0:gx1] = 0.05
                rough[gy0:gy1, gx0:gx1] = 0.07

                # Center vertical mullion
                mx = (gx0 + gx1) // 2
                albedo[gy0:gy1, mx - 2:mx + 2] = frame_color
                height[gy0:gy1, mx - 2:mx + 2] = 0.22
                rough[gy0:gy1, mx - 2:mx + 2] = 0.45

                if rng_w.random() < lit_prob:
                    intensity = rng_w.uniform(0.45, 1.0)
                    if rng_w.random() < cool_mix:
                        col = np.array([0.75, 0.86, 1.0], dtype=np.float32) * intensity
                    else:
                        col = np.array([1.0, rng_w.uniform(0.68, 0.86), rng_w.uniform(0.32, 0.54)], dtype=np.float32) * intensity
                    # Subtle top-down blind/curtain gradient inside lit room
                    blind_cut = rng_w.uniform(0.12, 0.42)
                    y_norm = np.linspace(0.0, 1.0, pane_h, dtype=np.float32)[:, None]
                    room_grad = (0.35 + 0.65 * smoothstep(blind_cut, blind_cut + 0.15, y_norm))
                    em_pane = col[None, None, :] * room_grad[:, :, None]
                    # Keep mullion dark over lit window; daytime albedo stays dark reflective glass
                    # while NightFactor in M_BN_Surface drives EmissiveMap at dusk/night.
                    emissive[gy0:gy1, gx0:gx1] = em_pane
                    emissive[gy0:gy1, mx - 2:mx + 2] = 0.0

        return albedo, height, rough, emissive

    # 4. Brick Building Facade + warm night windows
    rng = np.random.default_rng(301)
    brick_h_px = 16
    brick_w_px = 48
    row_idx = yy // brick_h_px
    x_shift = (xx + (row_idx % 2) * (brick_w_px // 2)) % N
    col_idx = x_shift // brick_w_px
    mortar_y = (yy % brick_h_px) < 2
    mortar_x = (x_shift % brick_w_px) < 2
    mortar = (mortar_x | mortar_y).astype(np.float32)
    b_rows = N // brick_h_px
    b_cols = (N // brick_w_px) + 1
    brick_var = rng.uniform(0.76, 1.18, size=(b_rows, b_cols)).astype(np.float32)
    b_tone = brick_var[row_idx % b_rows, col_idx % b_cols]
    b_noise = fbm(rng, N, [(16, 0.4), (64, 0.4), (128, 0.2)])
    brick_rgb = np.stack([
        0.38 * b_tone * (0.85 + 0.25 * b_noise),
        0.16 * b_tone * (0.85 + 0.25 * b_noise),
        0.11 * b_tone * (0.85 + 0.25 * b_noise),
    ], axis=-1)
    mortar_rgb = np.array([0.32, 0.30, 0.27], dtype=np.float32)
    brick_rgb = brick_rgb * (1.0 - mortar[:, :, None]) + mortar_rgb * mortar[:, :, None]
    brick_h = (0.65 + 0.20 * b_noise) * (1.0 - mortar) + 0.22 * mortar
    brick_r = np.clip(0.84 + 0.10 * mortar + 0.08 * b_noise, 0.5, 0.98)
    bd, bh, br, be = apply_windows(
        rng, brick_rgb, brick_h, brick_r, cols=4, rows=4,
        win_w_frac=0.52, win_h_frac=0.58,
        frame_color=(0.14, 0.13, 0.12), glass_color=(0.06, 0.09, 0.12),
        lit_prob=0.52, cool_mix=0.12,
    )
    save_rgb("T_Brick_D", bd)
    save_normal("T_Brick_N", bh, strength=4.2)
    save_gray("T_Brick_R", br)
    save_rgb("T_Brick_E", be)

    # 5. Plaster Building Facade + warm night windows
    rng = np.random.default_rng(302)
    p_noise = fbm(rng, N, [(6, 0.40), (24, 0.30), (64, 0.20), (128, 0.10)])
    p_fine = rng.random((N, N), dtype=np.float32)
    floor_band = ((yy % 256) < 12).astype(np.float32)
    p_val = 0.56 + 0.12 * p_noise + 0.03 * (p_fine - 0.5) - 0.06 * floor_band
    plaster_rgb = np.stack([p_val * 1.05, p_val * 0.96, p_val * 0.82], axis=-1)
    plaster_h = 0.50 + 0.18 * p_noise + 0.08 * p_fine + 0.25 * floor_band
    plaster_r = np.clip(0.86 + 0.08 * p_noise, 0.6, 0.98)
    pd, ph, pr, pe = apply_windows(
        rng, plaster_rgb, plaster_h, plaster_r, cols=4, rows=4,
        win_w_frac=0.50, win_h_frac=0.56,
        frame_color=(0.20, 0.16, 0.13), glass_color=(0.07, 0.10, 0.13),
        lit_prob=0.56, cool_mix=0.15,
    )
    save_rgb("T_Plaster_D", pd)
    save_normal("T_Plaster_N", ph, strength=3.4)
    save_gray("T_Plaster_R", pr)
    save_rgb("T_Plaster_E", pe)

    # 6. Glass-Steel Modern Curtain Wall Facade + office night windows
    rng = np.random.default_rng(303)
    gs_noise = fbm(rng, N, [(8, 0.5), (32, 0.5)])
    spandrel = np.stack([
        0.16 + 0.04 * gs_noise,
        0.19 + 0.04 * gs_noise,
        0.23 + 0.05 * gs_noise,
    ], axis=-1)
    spandrel_h = 0.60 + 0.08 * gs_noise
    spandrel_r = np.full((N, N), 0.44, dtype=np.float32)
    gd, gh, gr, ge = apply_windows(
        rng, spandrel, spandrel_h, spandrel_r, cols=4, rows=4,
        win_w_frac=0.84, win_h_frac=0.72,
        frame_color=(0.10, 0.12, 0.15), glass_color=(0.08, 0.15, 0.22),
        lit_prob=0.62, cool_mix=0.45,
    )
    save_rgb("T_GlassSteel_D", gd)
    save_normal("T_GlassSteel_N", gh, strength=3.5)
    save_gray("T_GlassSteel_R", gr)
    save_rgb("T_GlassSteel_E", ge)

    # 7. Corrugated Shipping Container Metal (neutral tintable albedo + crisp vertical normal ribs)
    rng = np.random.default_rng(401)
    rib_freq = 24.0
    rib_wave = np.clip(np.sin((xx / N) * 2.0 * math.pi * rib_freq) * 1.45, -1.0, 1.0) * 0.5 + 0.5
    # Horizontal top/bottom structural header rails
    rail = ((yy < 46) | (yy > N - 46)).astype(np.float32)
    rib_h = rib_wave * (1.0 - rail) + 0.92 * rail
    wear = fbm(rng, N, [(8, 0.45), (32, 0.35), (128, 0.20)])
    rust_mask = smoothstep(0.66, 0.80, wear) * (0.3 + 0.7 * smoothstep(0.55, 0.98, yy / N))
    base_m = (0.76 + 0.10 * rib_h + 0.08 * (wear - 0.5))
    cont_d = np.stack([
        base_m * (1.0 - 0.35 * rust_mask) + 0.34 * rust_mask,
        base_m * (1.0 - 0.62 * rust_mask) + 0.16 * rust_mask,
        base_m * (1.0 - 0.72 * rust_mask) + 0.09 * rust_mask,
    ], axis=-1)
    cont_r = np.clip(0.42 + 0.16 * wear + 0.35 * rust_mask - 0.06 * rib_h, 0.24, 0.94)
    save_rgb("T_Container_D", cont_d)
    save_normal("T_Container_N", rib_h + 0.08 * wear, strength=6.5)
    save_gray("T_Container_R", cont_r)

    # 8. Dirt (packed earth, stones, subtle ruts)
    rng = np.random.default_rng(501)
    d_base = fbm(rng, N, [(6, 0.35), (16, 0.30), (48, 0.20), (128, 0.15)])
    pebbles = smoothstep(0.68, 0.86, periodic_noise(rng, N, 96))
    d_fine = rng.random((N, N), dtype=np.float32)
    dirt_d = np.stack([
        0.26 + 0.14 * d_base + 0.10 * pebbles + 0.03 * (d_fine - 0.5),
        0.18 + 0.10 * d_base + 0.08 * pebbles + 0.02 * (d_fine - 0.5),
        0.11 + 0.06 * d_base + 0.06 * pebbles + 0.02 * (d_fine - 0.5),
    ], axis=-1)
    save_rgb("T_Dirt_D", dirt_d)
    save_normal("T_Dirt_N", 0.50 * d_base + 0.38 * pebbles + 0.12 * d_fine, strength=5.0)
    save_gray("T_Dirt_R", np.clip(0.86 + 0.12 * d_base - 0.15 * pebbles, 0.55, 0.98))

    # 9. Grass (layered blades, rich olive/green variation, soil undertones)
    rng = np.random.default_rng(502)
    g_macro = fbm(rng, N, [(6, 0.45), (18, 0.35), (48, 0.20)])
    # Anisotropic high-frequency tuft texture
    g_blades = periodic_noise(rng, N, 128) * 0.6 + rng.random((N, N), dtype=np.float32) * 0.4
    dry_patch = smoothstep(0.62, 0.82, g_macro)
    grass_d = np.stack([
        0.09 + 0.07 * g_blades + 0.11 * dry_patch,
        0.16 + 0.11 * g_blades + 0.04 * g_macro - 0.03 * dry_patch,
        0.04 + 0.03 * g_blades + 0.02 * dry_patch,
    ], axis=-1)
    save_rgb("T_Grass_D", grass_d)
    save_normal("T_Grass_N", 0.35 * g_macro + 0.65 * g_blades, strength=4.5)
    save_gray("T_Grass_R", np.clip(0.80 + 0.14 * g_macro - 0.08 * g_blades, 0.58, 0.98))

    # 10. Wood Crate (planks, wood grain, outer frame battens, nails)
    rng = np.random.default_rng(601)
    grain_x = periodic_noise(rng, N, 16)
    grain_y = periodic_noise(rng, N, 128)
    wood_grain = 0.5 + 0.5 * np.sin((yy * 0.18 + grain_x * 10.0) * 1.0) * 0.45 + 0.35 * (grain_y - 0.5)
    plank_seam = ((yy % 128) < 5).astype(np.float32)
    plank_tone = rng.uniform(0.86, 1.14, size=8).astype(np.float32)[yy // 128]
    border = ((xx < 72) | (xx > N - 72) | (yy < 72) | (yy > N - 72)).astype(np.float32)
    diag = (np.abs(xx - yy) < 38).astype(np.float32) * (1.0 - border)
    batten = np.maximum(border, diag)
    w_tone = (0.50 + 0.14 * wood_grain) * plank_tone * (1.0 - 0.45 * plank_seam * (1.0 - batten))
    w_tone = w_tone * (1.0 - 0.10 * batten) + 0.08 * batten
    wood_d = np.stack([w_tone * 0.92, w_tone * 0.66, w_tone * 0.38], axis=-1)
    wood_h = 0.35 * wood_grain - 0.55 * plank_seam * (1.0 - batten) + 0.45 * batten
    save_rgb("T_WoodCrate_D", wood_d)
    save_normal("T_WoodCrate_N", wood_h, strength=4.5)
    save_gray("T_WoodCrate_R", np.clip(0.76 + 0.14 * wood_grain + 0.08 * plank_seam, 0.48, 0.96))

    # 11. Hazard Metal (diagonal amber/charcoal chevrons + brushed steel scratches)
    rng = np.random.default_rng(701)
    brushed = periodic_noise(rng, N, 64) * 0.5 + fbm(rng, N, [(12, 0.5), (48, 0.5)]) * 0.5
    stripe_phase = ((xx + yy) % 256) < 128
    stripe_edge = (np.abs(((xx + yy) % 128) - 2) < 3).astype(np.float32)
    scratches = smoothstep(0.72, 0.86, fbm(rng, N, [(16, 0.4), (64, 0.6)]))
    yellow = np.array([0.88, 0.64, 0.05], dtype=np.float32)
    charcoal = np.array([0.07, 0.07, 0.08], dtype=np.float32)
    haz_d = np.where(stripe_phase[:, :, None], yellow, charcoal) * (0.88 + 0.20 * brushed[:, :, None])
    haz_d = haz_d * (1.0 - 0.45 * scratches[:, :, None]) + np.array([0.45, 0.46, 0.48]) * 0.45 * scratches[:, :, None]
    save_rgb("T_HazardMetal_D", haz_d)
    save_normal("T_HazardMetal_N", 0.28 * brushed - 0.20 * stripe_edge + 0.15 * scratches, strength=3.5)
    save_gray("T_HazardMetal_R", np.clip(0.42 + 0.18 * brushed - 0.14 * scratches, 0.22, 0.85))

    # 12. Harbor Water Normal (layered periodic swells + fine ripples)
    rng = np.random.default_rng(801)
    u = (xx / N) * 2.0 * math.pi
    v = (yy / N) * 2.0 * math.pi
    waves = (
        0.35 * np.sin(3.0 * u + 2.0 * v)
        + 0.25 * np.sin(5.0 * u - 4.0 * v + 1.1)
        + 0.20 * np.sin(9.0 * u + 7.0 * v + 2.3)
        + 0.20 * fbm(rng, N, [(16, 0.4), (48, 0.35), (96, 0.25)])
    )
    save_normal("T_Water_N", waves, strength=3.2)

    # 13. Autumn Foliage (layered russet/amber/gold/olive leaf clusters)
    rng = np.random.default_rng(901)
    f_macro = fbm(rng, N, [(8, 0.45), (24, 0.35), (64, 0.20)])
    f_leaves = periodic_noise(rng, N, 64) * 0.55 + periodic_noise(rng, N, 128) * 0.45
    gold = smoothstep(0.35, 0.72, f_macro)
    russet = smoothstep(0.55, 0.85, f_leaves)
    fol_d = np.stack([
        0.34 + 0.36 * gold + 0.14 * russet,
        0.16 + 0.24 * gold - 0.05 * russet + 0.08 * f_leaves,
        0.04 + 0.04 * f_leaves,
    ], axis=-1) * (0.65 + 0.50 * f_leaves[:, :, None])
    save_rgb("T_Foliage_D", fol_d)
    save_normal("T_Foliage_N", 0.35 * f_macro + 0.65 * f_leaves, strength=5.0)
    save_gray("T_Foliage_R", np.clip(0.56 + 0.24 * f_macro - 0.10 * f_leaves, 0.38, 0.90))

    # 14. Tree Bark (deep vertical ridges)
    rng = np.random.default_rng(902)
    bark_x = periodic_noise(rng, N, 64)
    bark_warp = fbm(rng, N, [(8, 0.5), (24, 0.5)])
    ridges = 0.5 + 0.5 * np.sin((xx / N) * 2.0 * math.pi * 22.0 + bark_warp * 6.0)
    bark_h = 0.62 * ridges + 0.28 * bark_x + 0.10 * bark_warp
    bark_d = np.stack([
        0.13 + 0.11 * bark_h,
        0.10 + 0.08 * bark_h,
        0.08 + 0.06 * bark_h,
    ], axis=-1)
    save_rgb("T_Bark_D", bark_d)
    save_normal("T_Bark_N", bark_h, strength=5.8)
    save_gray("T_Bark_R", np.clip(0.84 + 0.12 * bark_warp - 0.08 * ridges, 0.62, 0.98))

    bn_pbr_extra_textures.generate_extra_pngs(
        OUT_DIR, save_rgb, save_gray, save_normal, periodic_noise, fbm, smoothstep, force=force
    )

    return len(TEXTURE_MANIFEST)


def generate_all_pngs(force=False):
    """Generates all seamless PBR PNGs under Saved/GeneratedTextures/.
    Falls back to system /usr/bin/python3 when running inside UE5's embedded Python."""
    try:
        import numpy  # noqa: F401
        from PIL import Image  # noqa: F401
        return _generate_all_pngs_impl(force=force)
    except ImportError:
        py = "/usr/bin/python3" if os.path.isfile("/usr/bin/python3") else "python3"
        env = os.environ.copy()
        env.pop("PYTHONHOME", None)
        env.pop("PYTHONPATH", None)
        cmd = [py, os.path.abspath(__file__), "--generate-only"]
        if force:
            cmd.append("--force")
        subprocess.run(cmd, check=True, env=env)
        return len(TEXTURE_MANIFEST)


def ensure_ue_textures(force_reimport=False):
    """Generates missing PNGs and imports all PBR maps into /Game/World/Textures/.
    Returns {asset_name: unreal.Texture2D}."""
    import unreal

    generate_all_pngs(force=False)

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    assets = unreal.EditorAssetLibrary
    if not assets.does_directory_exist(UE_TEX_DIR):
        assets.make_directory(UE_TEX_DIR)

    tasks = []
    to_configure = []
    for name, kind in TEXTURE_MANIFEST:
        ue_path = "%s/%s" % (UE_TEX_DIR, name)
        png_path = os.path.join(OUT_DIR, name + ".png")
        if not os.path.isfile(png_path):
            raise RuntimeError("Missing generated texture PNG: " + png_path)
        if force_reimport or not assets.does_asset_exist(ue_path):
            task = unreal.AssetImportTask()
            task.filename = png_path
            task.destination_path = UE_TEX_DIR
            task.destination_name = name
            task.replace_existing = True
            task.automated = True
            task.save = False
            tasks.append(task)
            to_configure.append((name, kind))

    if tasks:
        tools.import_asset_tasks(tasks)

    result = {}
    for name, kind in TEXTURE_MANIFEST:
        ue_path = "%s/%s" % (UE_TEX_DIR, name)
        tex = unreal.load_asset(ue_path)
        if tex is None:
            raise RuntimeError("Failed to import/load texture: " + ue_path)
        dirty = (name, kind) in to_configure
        if kind == "normal":
            if tex.get_editor_property("compression_settings") != unreal.TextureCompressionSettings.TC_NORMALMAP:
                tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
                dirty = True
            if tex.get_editor_property("srgb"):
                tex.set_editor_property("srgb", False)
                dirty = True
            if tex.get_editor_property("lod_group") != unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP:
                tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
                dirty = True
        elif kind == "roughness":
            if tex.get_editor_property("compression_settings") != unreal.TextureCompressionSettings.TC_MASKS:
                tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
                dirty = True
            if tex.get_editor_property("srgb"):
                tex.set_editor_property("srgb", False)
                dirty = True
        else:
            if tex.get_editor_property("compression_settings") != unreal.TextureCompressionSettings.TC_DEFAULT:
                tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
                dirty = True
            if not tex.get_editor_property("srgb"):
                tex.set_editor_property("srgb", True)
                dirty = True
        if dirty:
            assets.save_loaded_asset(tex, only_if_is_dirty=False)
        result[name] = tex

    unreal.log("BN: ensured %d PBR textures under %s" % (len(result), UE_TEX_DIR))
    return result


if __name__ == "__main__":
    if "--generate-only" in sys.argv:
        count = generate_all_pngs(force="--force" in sys.argv)
        print("BN: generated %d PBR textures in %s" % (count, OUT_DIR))
    else:
        try:
            import unreal  # noqa: F401
            ensure_ue_textures(force_reimport="--force" in sys.argv)
        except ImportError:
            count = generate_all_pngs(force="--force" in sys.argv)
            print("BN: generated %d PBR textures in %s (outside UE)" % (count, OUT_DIR))
