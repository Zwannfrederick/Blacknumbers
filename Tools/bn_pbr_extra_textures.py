"""Extra procedural textures for BLACK NUMBERS:
- Masked foliage leaf & pine bough cards (albedo, alpha mask, normal)
- Soft alpha particle VFX (smoke puff, exhaust flame, collision spark, rubber skidmark)
- Readable Turkish highway gantry signs and Karamara ground-floor storefronts
- Road crosswalk, lane arrow, and old-town cobblestone PBR maps

Invoked by bn_pbr_textures.py inside system python3 (with numpy + PIL).
"""
import math
import os


EXTRA_TEXTURE_MANIFEST = [
    # Foliage cards (Mesh UV0)
    ("T_LeafCluster_D", "albedo"),
    ("T_LeafCluster_A", "roughness"),  # linear mask (sRGB=False, TC_MASKS)
    ("T_LeafCluster_N", "normal"),
    ("T_PineNeedle_D", "albedo"),
    ("T_PineNeedle_A", "roughness"),
    ("T_PineNeedle_N", "normal"),
    # Particle VFX (RGBA soft alpha)
    ("T_VFX_SmokePuff", "albedo"),
    ("T_VFX_Flame", "albedo"),
    ("T_VFX_Spark", "albedo"),
    ("T_VFX_SkidMark", "albedo"),
    # Readable Turkish highway gantry signs & storefronts
    ("T_Sign_Merkez", "albedo"),
    ("T_Sign_EskiLiman", "albedo"),
    ("T_Sign_KuzeyGecidi", "albedo"),
    ("T_Sign_DoguSanayi", "albedo"),
    ("T_Storefront_Karamara_D", "albedo"),
    ("T_Storefront_Karamara_E", "emissive"),
    # Road details & cobblestone
    ("T_Crosswalk_D", "albedo"),
    ("T_RoadArrow_D", "albedo"),
    ("T_Cobblestone_D", "albedo"),
    ("T_Cobblestone_N", "normal"),
    ("T_Cobblestone_R", "roughness"),
]


def _load_font(size, bold=True):
    from PIL import ImageFont

    candidates = [
        "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf" if bold else "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSansCondensed-Bold.ttf",
        "/usr/share/fonts/Adwaita/AdwaitaSans-Regular.ttf",
    ]
    for path in candidates:
        if os.path.isfile(path):
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


def generate_extra_pngs(out_dir, save_rgb, save_gray, save_normal, periodic_noise, fbm, smoothstep, force=False):
    """Generates all extra textures into out_dir."""
    import numpy as np
    from PIL import Image, ImageDraw, ImageFilter

    if not force and all(os.path.isfile(os.path.join(out_dir, name + ".png")) for name, _ in EXTRA_TEXTURE_MANIFEST):
        return len(EXTRA_TEXTURE_MANIFEST)

    def save_rgba(name, rgba_arr):
        arr = np.clip(rgba_arr * 255.0, 0, 255).astype(np.uint8)
        Image.fromarray(arr, mode="RGBA").save(os.path.join(out_dir, name + ".png"), optimize=False)

    # ------------------------------------------------------------------
    # 1. Autumn Leaf Cluster (512x512) — branching twigs + layered leaves
    # ------------------------------------------------------------------
    F = 512
    rng = np.random.default_rng(1101)
    leaf_img = Image.new("RGBA", (F, F), (0, 0, 0, 0))
    height_img = Image.new("L", (F, F), 0)
    draw_c = ImageDraw.Draw(leaf_img)
    draw_h = ImageDraw.Draw(height_img)

    # Dark woody central twig & side stems
    stem_col = (52, 38, 28, 255)
    draw_c.line([(F * 0.50, F * 0.96), (F * 0.49, F * 0.52), (F * 0.51, F * 0.14)], fill=stem_col, width=7)
    draw_h.line([(F * 0.50, F * 0.96), (F * 0.49, F * 0.52), (F * 0.51, F * 0.14)], fill=120, width=7)
    branches = [
        ((0.49, 0.76), (0.20, 0.56)),
        ((0.50, 0.72), (0.80, 0.54)),
        ((0.49, 0.52), (0.18, 0.30)),
        ((0.50, 0.48), (0.82, 0.28)),
        ((0.50, 0.30), (0.30, 0.14)),
        ((0.51, 0.28), (0.72, 0.14)),
    ]
    for (x0, y0), (x1, y1) in branches:
        draw_c.line([(x0 * F, y0 * F), (x1 * F, y1 * F)], fill=stem_col, width=4)
        draw_h.line([(x0 * F, y0 * F), (x1 * F, y1 * F)], fill=100, width=4)

    # Palette: rich autumn maple/oak russet, burnt amber, goldenrod, olive accents
    palette = [
        (168, 78, 26),
        (194, 118, 32),
        (142, 54, 22),
        (208, 148, 42),
        (118, 84, 30),
        (96, 102, 38),
    ]

    def draw_pointed_leaf(cx, cy, length, width, angle_rad, rgb, h_val):
        ca, sa = math.cos(angle_rad), math.sin(angle_rad)
        pts = []
        steps = 12
        for k in range(steps):
            t = (k / float(steps)) * 2.0 * math.pi
            # Almond / maple-like pointed silhouette
            lx = math.cos(t) * length * 0.5
            ly = math.sin(t) * width * 0.5 * (0.75 + 0.25 * math.cos(t))
            px = cx + lx * ca - ly * sa
            py = cy + lx * sa + ly * ca
            pts.append((px, py))
        draw_c.polygon(pts, fill=(rgb[0], rgb[1], rgb[2], 255))
        draw_h.polygon(pts, fill=h_val)
        # Subtle midrib highlight/vein
        x_a = cx - length * 0.42 * ca
        y_a = cy - length * 0.42 * sa
        x_b = cx + length * 0.42 * ca
        y_b = cy + length * 0.42 * sa
        vein_c = (min(255, int(rgb[0] * 1.15)), min(255, int(rgb[1] * 1.12)), min(255, int(rgb[2] * 1.05)), 255)
        draw_c.line([(x_a, y_a), (x_b, y_b)], fill=vein_c, width=2)
        draw_h.line([(x_a, y_a), (x_b, y_b)], fill=min(255, h_val + 30), width=2)

    # Scatter 175 clustered leaves around the twig skeleton for a lush autumn canopy
    for i in range(175):
        r = rng.uniform(0.0, 0.40)
        th = rng.uniform(0.0, 2.0 * math.pi)
        cx = F * (0.50 + r * math.cos(th) * rng.uniform(0.85, 1.10))
        cy = F * (0.48 + r * math.sin(th) * rng.uniform(0.85, 1.05))
        length = rng.uniform(40.0, 80.0)
        width = length * rng.uniform(0.44, 0.64)
        angle = math.atan2(cy - F * 0.60, cx - F * 0.50) + rng.uniform(-0.85, 0.85)
        base_col = palette[int(rng.integers(0, len(palette)))]
        shade = rng.uniform(0.76, 1.14)
        col = tuple(int(np.clip(c * shade, 18, 245)) for c in base_col)
        h_val = int(110 + (i / 175.0) * 125)
        draw_pointed_leaf(cx, cy, length, width, angle, col, h_val)

    leaf_arr = np.asarray(leaf_img, dtype=np.float32) / 255.0
    alpha = leaf_arr[:, :, 3]
    # Color-bleed background with average autumn leaf tone so mipmaps never get black/white halo fringes
    bg_leaf = np.array([0.48, 0.24, 0.08], dtype=np.float32)
    leaf_rgb = leaf_arr[:, :, :3] * alpha[:, :, None] + bg_leaf[None, None, :] * (1.0 - alpha[:, :, None])
    leaf_h = np.asarray(height_img.filter(ImageFilter.GaussianBlur(radius=1.2)), dtype=np.float32) / 255.0
    save_rgb("T_LeafCluster_D", leaf_rgb)
    save_gray("T_LeafCluster_A", alpha)
    save_normal("T_LeafCluster_N", leaf_h, strength=4.0)

    # ------------------------------------------------------------------
    # 2. Pine Bough Needle Cluster (512x512) — evergreen mountain trees
    # ------------------------------------------------------------------
    rng = np.random.default_rng(1102)
    pine_img = Image.new("RGBA", (F, F), (0, 0, 0, 0))
    pine_h_img = Image.new("L", (F, F), 0)
    dp = ImageDraw.Draw(pine_img)
    dph = ImageDraw.Draw(pine_h_img)

    pine_branches = [
        ((0.50, 0.94), (0.50, 0.14)),
        ((0.50, 0.78), (0.16, 0.48)),
        ((0.50, 0.78), (0.84, 0.48)),
        ((0.50, 0.56), (0.18, 0.28)),
        ((0.50, 0.56), (0.82, 0.28)),
        ((0.50, 0.36), (0.28, 0.16)),
        ((0.50, 0.36), (0.72, 0.16)),
    ]
    for (x0, y0), (x1, y1) in pine_branches:
        dp.line([(x0 * F, y0 * F), (x1 * F, y1 * F)], fill=(48, 36, 26, 255), width=5)
        dph.line([(x0 * F, y0 * F), (x1 * F, y1 * F)], fill=120, width=5)
        b_ang = math.atan2((y1 - y0), (x1 - x0))
        for k in range(42):
            t = rng.uniform(0.08, 0.98)
            bx = (x0 + (x1 - x0) * t) * F
            by = (y0 + (y1 - y0) * t) * F
            for side in (-1.0, 1.0):
                ang = b_ang + side * rng.uniform(0.45, 1.15)
                nlen = rng.uniform(28.0, 62.0) * (1.0 - 0.35 * t)
                nx = bx + math.cos(ang) * nlen
                ny = by + math.sin(ang) * nlen
                g = int(rng.uniform(52, 98))
                r_c = int(g * rng.uniform(0.36, 0.52))
                b_c = int(g * rng.uniform(0.32, 0.48))
                dp.line([(bx, by), (nx, ny)], fill=(r_c, g, b_c, 255), width=3)
                dph.line([(bx, by), (nx, ny)], fill=int(130 + rng.uniform(0, 110)), width=3)

    pine_arr = np.asarray(pine_img, dtype=np.float32) / 255.0
    p_alpha = pine_arr[:, :, 3]
    bg_pine = np.array([0.11, 0.22, 0.10], dtype=np.float32)
    pine_rgb = pine_arr[:, :, :3] * p_alpha[:, :, None] + bg_pine[None, None, :] * (1.0 - p_alpha[:, :, None])
    pine_h = np.asarray(pine_h_img.filter(ImageFilter.GaussianBlur(radius=1.0)), dtype=np.float32) / 255.0
    save_rgb("T_PineNeedle_D", pine_rgb)
    save_gray("T_PineNeedle_A", p_alpha)
    save_normal("T_PineNeedle_N", pine_h, strength=3.6)

    # ------------------------------------------------------------------
    # 3. Soft Alpha Particle VFX (256x256 RGBA)
    # ------------------------------------------------------------------
    P = 256
    py_g, px_g = np.mgrid[0:P, 0:P].astype(np.float32)
    u = (px_g / (P - 1)) * 2.0 - 1.0  # -1..+1
    v = (py_g / (P - 1)) * 2.0 - 1.0
    rad = np.sqrt(u * u + v * v)

    # 3a. Tire Smoke Puff: multi-octave cloudy silhouette with smooth zero border
    rng = np.random.default_rng(1201)
    cloud = fbm(rng, P, [(4, 0.45), (8, 0.30), (16, 0.17), (32, 0.08)])
    radial_falloff = 1.0 - smoothstep(0.18, 0.94, rad)
    smoke_a = np.clip(radial_falloff * (0.45 + 0.85 * cloud), 0.0, 1.0) * (1.0 - smoothstep(0.86, 0.99, rad))
    smoke_lit = np.clip(0.82 + 0.18 * cloud - 0.18 * v, 0.55, 1.0)
    save_rgba("T_VFX_SmokePuff", np.stack([smoke_lit, smoke_lit, smoke_lit, smoke_a], axis=-1))

    # 3b. Exhaust / NOS Flame Plume (vertical teardrop: hot core at bottom, wispy tip at top)
    rng = np.random.default_rng(1202)
    f_turb = fbm(rng, P, [(4, 0.4), (8, 0.35), (16, 0.25)])
    vt = py_g / (P - 1)  # 0 at top tip, 1 at exhaust nozzle base
    width_env = np.maximum(np.sin(np.clip(vt, 0.0, 1.0) * math.pi), 0.0) ** 0.72 * (0.35 + 0.65 * vt)
    dx_n = np.abs(u) / np.maximum(width_env, 0.04)
    flame_body = (1.0 - smoothstep(0.15, 1.0, dx_n)) * smoothstep(0.02, 0.25, vt) * (1.0 - smoothstep(0.88, 1.0, vt))
    flame_a = np.clip(flame_body * (0.65 + 0.55 * f_turb), 0.0, 1.0)
    core = np.clip(1.0 - smoothstep(0.0, 0.55, dx_n) * (1.0 - 0.4 * vt), 0.0, 1.0)
    # White-hot neutral mask so Tint in C++ tints NOS blue or Backfire orange cleanly!
    flame_r = np.clip(0.85 + 0.15 * core, 0.0, 1.0) * flame_a
    flame_g = np.clip(0.72 + 0.28 * core, 0.0, 1.0) * flame_a
    flame_b = np.clip(0.65 + 0.35 * core, 0.0, 1.0) * flame_a
    save_rgba("T_VFX_Flame", np.stack([flame_r, flame_g, flame_b, flame_a], axis=-1))

    # 3c. Collision Spark Streak (bright center elongated horizontally along U)
    spark_dist = np.sqrt((u * 1.05) ** 2 + (v * 3.8) ** 2)
    spark_a = np.clip(np.exp(-spark_dist * spark_dist * 3.2) * (1.0 - smoothstep(0.75, 1.0, rad)), 0.0, 1.0)
    spark_c = np.clip(0.75 + 0.25 * spark_a, 0.0, 1.0) * spark_a
    save_rgba("T_VFX_Spark", np.stack([spark_c, spark_c, spark_c, spark_a], axis=-1))

    # 3d. Tire Skidmark Strip (subtle rubber tread + soft tapered ends)
    rng = np.random.default_rng(1203)
    skid_grain = fbm(rng, P, [(8, 0.4), (32, 0.35), (64, 0.25)])
    edge_y = 1.0 - smoothstep(0.55, 0.96, np.abs(v))
    edge_x = 1.0 - smoothstep(0.75, 0.98, np.abs(u))
    grooves = 0.72 + 0.28 * np.cos(v * math.pi * 6.0)
    skid_a = np.clip(edge_y * edge_x * grooves * (0.72 + 0.38 * skid_grain), 0.0, 1.0)
    skid_v = np.full((P, P), 0.06, dtype=np.float32)
    save_rgba("T_VFX_SkidMark", np.stack([skid_v, skid_v, skid_v, skid_a], axis=-1))

    # ------------------------------------------------------------------
    # 4. Readable Turkish Highway Gantry Signs (1024x512)
    # ------------------------------------------------------------------
    def fit_font(draw_obj, text, max_w, start_size, bold=True):
        size = start_size
        while size > 12:
            f = _load_font(size, bold=bold)
            bbox = draw_obj.textbbox((0, 0), text, font=f)
            if (bbox[2] - bbox[0]) <= max_w:
                return f
            size -= 2
        return _load_font(12, bold=bold)

    def make_highway_sign(filename, bg_rgb, badge_text, badge_bg, line1, line2, footer):
        W, H = 1024, 512
        img = Image.new("RGB", (W, H), bg_rgb)
        d = ImageDraw.Draw(img)
        # Double crisp reflective white highway border
        d.rounded_rectangle([18, 18, W - 18, H - 18], radius=24, outline=(240, 244, 240), width=10)
        d.rounded_rectangle([36, 36, W - 36, H - 36], radius=16, outline=(200, 214, 204), width=3)
        # Highway route shield badge (top right) with centered text
        bx0, by0, bx1, by1 = W - 252, 56, W - 58, 146
        d.rounded_rectangle([bx0, by0, bx1, by1], radius=14, fill=badge_bg, outline=(245, 245, 245), width=5)
        f_badge = fit_font(d, badge_text, (bx1 - bx0) - 24, 44, bold=True)
        bb = d.textbbox((0, 0), badge_text, font=f_badge)
        d.text(((bx0 + bx1 - (bb[2] - bb[0])) * 0.5, (by0 + by1 - (bb[3] - bb[1])) * 0.5 - 4),
               badge_text, fill=(250, 250, 245), font=f_badge)

        f_big = fit_font(d, line1, (bx0 - 92), 62, bold=True)
        f_med = fit_font(d, line2, W - 132, 48, bold=True)
        f_sub = fit_font(d, footer, W - 132, 34, bold=False)

        d.text((64, 70), line1, fill=(248, 250, 246), font=f_big)
        d.line([(60, 178), (W - 60, 178)], fill=(215, 230, 220), width=4)
        d.text((64, 214), line2, fill=(242, 246, 240), font=f_med)
        d.text((64, 382), footer, fill=(244, 206, 86), font=f_sub)
        # Subtle weathering grain
        arr = np.asarray(img, dtype=np.float32) / 255.0
        rng_s = np.random.default_rng(abs(hash(filename)) % 99999)
        grime = rng_s.uniform(0.93, 1.03, size=(H, W, 1)).astype(np.float32)
        arr = np.clip(arr * grime, 0.0, 1.0)
        save_rgb(filename, arr)

    make_highway_sign(
        "T_Sign_Merkez", (14, 96, 52), "D-400", (20, 74, 152),
        "↑ MERKEZ BULVAR", "→ ESKİ LİMAN   ← YAMAÇ MAH.", "KARAMARA ŞEHİR MERKEZİ  •  60 KM/S",
    )
    make_highway_sign(
        "T_Sign_EskiLiman", (16, 72, 128), "LİMAN", (196, 114, 18),
        "↑ ESKİ LİMAN", "↗ ESKİ LİMAN KÖPRÜSÜ", "MARA RIHTIM  •  DOĞU SANAYİ ÇIKIŞI",
    )
    make_highway_sign(
        "T_Sign_KuzeyGecidi", (16, 86, 48), "O-5", (184, 52, 24),
        "↑ KUZEY GEÇİDİ", "↗ SEYİR TEPESİ   ÇEVRE YOLU", "DİKKAT: SERT DAĞ VİRAJLARI  •  %9 EĞİM",
    )
    make_highway_sign(
        "T_Sign_DoguSanayi", (22, 66, 98), "SAN-2", (190, 128, 22),
        "↑ DOĞU SANAYİ", "→ TARIK OTO TAMİR   ARDA GALERİ", "AĞIR VASITA & KONTEYNER BÖLGESİ",
    )

    # ------------------------------------------------------------------
    # 5. Karamara Storefront Strip (1024x512 Albedo + Night Emissive)
    # ------------------------------------------------------------------
    W, H = 1024, 512
    sf_d = Image.new("RGB", (W, H), (36, 35, 34))
    sf_e = Image.new("RGB", (W, H), (0, 0, 0))
    dd = ImageDraw.Draw(sf_d)
    de = ImageDraw.Draw(sf_e)

    shops = [
        ("TARIK OTO TAMİR", "PERFORMANCE & TURBO", (176, 62, 22), (255, 165, 64)),
        ("ARDA GALERİ", "KARAMARA MOTORS", (24, 92, 138), (110, 210, 255)),
        ("GECE KAHVESİ", "24 SAAT AÇIK • BÜFE", (142, 86, 26), (255, 200, 108)),
        ("MARA YEDEK PARÇA", "LASTİK • EGZOZ • JANT", (118, 34, 34), (255, 115, 72)),
    ]
    sw = W // 4
    for idx, (title, sub, sign_col, neon_col) in enumerate(shops):
        x0 = idx * sw
        x1 = x0 + sw
        f_shop = fit_font(dd, title, sw - 34, 24, bold=True)
        f_sub = fit_font(dd, sub, sw - 34, 14, bold=False)
        # Concrete pillar frame
        dd.rectangle([x0, 0, x1, H], fill=(48, 46, 44))
        # Upper signboard fascia
        dd.rectangle([x0 + 8, 22, x1 - 8, 128], fill=sign_col, outline=(22, 22, 24), width=3)
        tb = dd.textbbox((0, 0), title, font=f_shop)
        tx = x0 + (sw - (tb[2] - tb[0])) * 0.5
        dd.text((tx, 44), title, fill=(246, 244, 236), font=f_shop)
        sb = dd.textbbox((0, 0), sub, font=f_sub)
        sx = x0 + (sw - (sb[2] - sb[0])) * 0.5
        dd.text((sx, 88), sub, fill=(230, 216, 184), font=f_sub)
        # Night emissive signboard text + soft backlit fascia
        de.rectangle([x0 + 12, 26, x1 - 12, 124], fill=tuple(int(c * 0.28) for c in neon_col))
        de.text((tx, 44), title, fill=neon_col, font=f_shop)
        de.text((sx, 88), sub, fill=tuple(int(c * 0.75) for c in neon_col), font=f_sub)
        # Shopfront glass showcase window (dark blue-grey in daytime, warm lit interior at night)
        dd.rectangle([x0 + 16, 150, x1 - 16, 435], fill=(18, 24, 30), outline=(28, 28, 30), width=5)
        # Interior shelving silhouette
        dd.rectangle([x0 + 26, 320, x1 - 26, 425], fill=(24, 28, 34))
        # Night interior glow
        warm_win = (235, 168, 82) if idx != 1 else (165, 205, 240)
        de.rectangle([x0 + 22, 158, x1 - 22, 428], fill=tuple(int(c * 0.65) for c in warm_win))
        de.rectangle([x0 + 22, 340, x1 - 22, 428], fill=tuple(int(c * 0.30) for c in warm_win))
        # Vertical mullion bar stays dark
        mx = (x0 + x1) // 2
        dd.rectangle([mx - 4, 150, mx + 4, 435], fill=(26, 26, 28))
        de.rectangle([mx - 4, 150, mx + 4, 435], fill=(0, 0, 0))
        # Bottom tiled plinth
        dd.rectangle([x0 + 8, 442, x1 - 8, H], fill=(64, 62, 58))

    save_rgb("T_Storefront_Karamara_D", np.asarray(sf_d, dtype=np.float32) / 255.0)
    save_rgb("T_Storefront_Karamara_E", np.asarray(sf_e, dtype=np.float32) / 255.0)

    # ------------------------------------------------------------------
    # 6. Crosswalk & Road Arrow (512x512 RGBA) + Cobblestone (1024x1024)
    # ------------------------------------------------------------------
    cw = np.zeros((F, F, 4), dtype=np.float32)
    rng = np.random.default_rng(1301)
    wear = fbm(rng, F, [(8, 0.5), (24, 0.3), (64, 0.2)])
    stripe_period = F // 6
    for k in range(6):
        y0 = k * stripe_period + 14
        y1 = (k + 1) * stripe_period - 14
        cw[y0:y1, 24:F - 24, 0:3] = 0.84
        cw[y0:y1, 24:F - 24, 3] = np.clip(0.82 + 0.24 * (wear[y0:y1, 24:F - 24] - 0.5), 0.45, 0.96)
    save_rgba("T_Crosswalk_D", cw)

    arrow_img = Image.new("RGBA", (F, F), (0, 0, 0, 0))
    da = ImageDraw.Draw(arrow_img)
    da.polygon([(256, 48), (372, 200), (300, 200), (300, 464), (212, 464), (212, 200), (140, 200)], fill=(224, 224, 218, 225))
    arr_a = np.asarray(arrow_img, dtype=np.float32) / 255.0
    arr_a[:, :, 3] *= np.clip(0.84 + 0.25 * (wear - 0.5), 0.55, 1.0)
    save_rgba("T_RoadArrow_D", arr_a)

    # Cobblestone (1024x1024 seamless paving stones for Merkez Carsi & Yamac)
    C = 1024
    rng = np.random.default_rng(1302)
    yy, xx = np.mgrid[0:C, 0:C]
    stone_h_px = 32
    stone_w_px = 64
    r_idx = yy // stone_h_px
    x_sh = (xx + (r_idx % 2) * (stone_w_px // 2)) % C
    c_idx = x_sh // stone_w_px
    ux = (x_sh % stone_w_px).astype(np.float32) / stone_w_px
    uy = (yy % stone_h_px).astype(np.float32) / stone_h_px
    dx_e = np.minimum(ux, 1.0 - ux) * stone_w_px
    dy_e = np.minimum(uy, 1.0 - uy) * stone_h_px
    bevel = smoothstep(2.0, 8.0, np.minimum(dx_e, dy_e))
    s_tones = rng.uniform(0.74, 1.18, size=(C // stone_h_px, C // stone_w_px)).astype(np.float32)
    tone = s_tones[r_idx, c_idx]
    c_noise = fbm(rng, C, [(12, 0.45), (32, 0.35), (96, 0.20)])
    val = (0.28 + 0.14 * c_noise) * tone * (0.45 + 0.55 * bevel)
    cob_d = np.stack([val * 1.04, val * 1.00, val * 0.94], axis=-1)
    cob_h = 0.25 * c_noise + 0.75 * bevel
    cob_r = np.clip(0.72 + 0.16 * (1.0 - bevel) + 0.10 * c_noise, 0.36, 0.96)
    save_rgb("T_Cobblestone_D", cob_d)
    save_normal("T_Cobblestone_N", cob_h, strength=5.0)
    save_gray("T_Cobblestone_R", cob_r)

    return len(EXTRA_TEXTURE_MANIFEST)
