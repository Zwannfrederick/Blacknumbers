"""Generates 9 true-3D multi-material modular architectural building meshes (glTF 2.0 .glb)
for BLACK NUMBERS' 6 Karamara districts, replacing flat stretched BasicShapes/Cube boxes with
real 3D recessed windows, sills, lintels, mullions, protruding balconies & iron railings,
ground-floor shopfront arcades & awnings, hollow parapet roofs, clay-tile hip roofs with eaves,
and industrial monitor-roof factories — all HISM-instanced with Nanite OFF for 4 GB VRAM safety.

Invoked automatically via Tools/bn_meshes.py or standalone:
    python3 Tools/bn_building_meshes.py
"""
import math
import os


def _box_uv(b, mat, cx, cy, cz, sx, sy, sz):
    """Adds an axis-aligned solid box centered at (cx, cy, cz) with size (sx, sy, sz) in metres."""
    b.add_box(mat, (cx, cy, cz), (sx, sy, sz))


def _add_wall_with_windows(b, wall_mat, trim_mat, metal_mat, glass_mat,
                           face, w, d, z0, z1, n_cols, win_w, win_h, recess=0.20,
                           add_shutters=False):
    """Builds ONE exterior wall band (height z0..z1) along face in {'+X','-X','+Y','-Y'}
    with TRUE 3D recessed window pockets (-recess deep), protruding stone sills & lintels,
    dark-metal frames & mullions, and recessed reflective/night-glowing glass panes."""
    h = z1 - z0
    hw, hd = w * 0.5, d * 0.5
    span = d if face in ("+X", "-X") else w
    # Clamp window width so solid masonry piers always survive between bays
    bay = span / float(max(1, n_cols))
    ww = min(win_w, bay * 0.62)
    wh = min(win_h, h * 0.64)
    wz0 = z0 + (h - wh) * 0.44
    wz1 = wz0 + wh
    wall_t = recess + 0.08

    # Coordinate mapper: local (u along wall, v outward from wall plane, z up) -> UE (x, y, z)
    def to_ue(u, v, z):
        if face == "+X":
            return (hw + v, u, z)
        if face == "-X":
            return (-hw - v, -u, z)
        if face == "+Y":
            return (-u, hd + v, z)
        return (u, -hd - v, z)

    def quad(mat, u0, u1, v0, v1, za, zb, mode="face"):
        # mode='face': constant v=v0, outward normal +v; 'side_l': u=u0; 'side_r': u=u1; 'bot': z=za; 'top': z=zb
        if mode == "face":
            p0, p1, p2, p3 = to_ue(u0, v0, za), to_ue(u1, v0, za), to_ue(u1, v0, zb), to_ue(u0, v0, zb)
            b.add_quad(mat, p0, p1, p2, p3)
        elif mode == "jamb_l":
            p0, p1, p2, p3 = to_ue(u0, v0, za), to_ue(u0, v1, za), to_ue(u0, v1, zb), to_ue(u0, v0, zb)
            b.add_quad(mat, p0, p1, p2, p3)
        elif mode == "jamb_r":
            p0, p1, p2, p3 = to_ue(u1, v1, za), to_ue(u1, v0, za), to_ue(u1, v0, zb), to_ue(u1, v1, zb)
            b.add_quad(mat, p0, p1, p2, p3)
        elif mode == "sill_in":
            p0, p1, p2, p3 = to_ue(u0, v1, za), to_ue(u1, v1, za), to_ue(u1, v0, za), to_ue(u0, v0, za)
            b.add_quad(mat, p0, p1, p2, p3)
        elif mode == "head_in":
            p0, p1, p2, p3 = to_ue(u0, v0, zb), to_ue(u1, v0, zb), to_ue(u1, v1, zb), to_ue(u0, v1, zb)
            b.add_quad(mat, p0, p1, p2, p3)

    def local_box(mat, uc, vc, zc, su, sv, sz):
        # Maps a box aligned to wall local (u, v, z) into UE world coords
        if face in ("+X", "-X"):
            cx, cy, cz = to_ue(uc, vc, zc)
            _box_uv(b, mat, cx, cy, cz, sv, su, sz)
        else:
            cx, cy, cz = to_ue(uc, vc, zc)
            _box_uv(b, mat, cx, cy, cz, su, sv, sz)

    u_start = -span * 0.5
    # Bottom spandrel strip below windows & top spandrel strip above windows (full wall width)
    quad(wall_mat, u_start, -u_start, 0.0, 0.0, z0, wz0, "face")
    quad(wall_mat, u_start, -u_start, 0.0, 0.0, wz1, z1, "face")

    for i in range(n_cols):
        bu0 = u_start + i * bay
        bu1 = bu0 + bay
        wu0 = 0.5 * (bu0 + bu1) - ww * 0.5
        wu1 = 0.5 * (bu0 + bu1) + ww * 0.5
        # Solid wall piers left and right of window opening
        quad(wall_mat, bu0, wu0, 0.0, 0.0, wz0, wz1, "face")
        quad(wall_mat, wu1, bu1, 0.0, 0.0, wz0, wz1, "face")
        # 3D recessed window reveal jambs, head & sill
        quad(wall_mat, wu0, wu1, 0.0, -recess, wz0, wz1, "jamb_l")
        quad(wall_mat, wu0, wu1, 0.0, -recess, wz0, wz1, "jamb_r")
        quad(trim_mat, wu0, wu1, 0.0, -recess, wz0, wz1, "sill_in")
        quad(wall_mat, wu0, wu1, 0.0, -recess, wz0, wz1, "head_in")
        # Recessed reflective + night-lit glass pane
        quad(glass_mat, wu0 + 0.05, wu1 - 0.05, -recess + 0.02, -recess + 0.02, wz0 + 0.05, wz1 - 0.05, "face")
        # Dark-metal 3D window center mullion + upper transom bar (clean silhouette, low tri overhead)
        uc = 0.5 * (wu0 + wu1)
        zc = 0.5 * (wz0 + wz1)
        fv = -recess + 0.05
        local_box(metal_mat, uc, fv, zc, 0.055, 0.06, wh - 0.04)
        local_box(metal_mat, uc, fv, wz0 + wh * 0.68, ww - 0.04, 0.06, 0.055)
        # Protruding stone window sill & subtle header lintel (casts real sunlight shadows!)
        local_box(trim_mat, uc, 0.06, wz0 - 0.05, ww + 0.24, 0.22, 0.10)
        local_box(trim_mat, uc, 0.03, wz1 + 0.06, ww + 0.18, 0.14, 0.11)
        # Optional Mediterranean side shutters (Yamaç villas)
        if add_shutters:
            sw = min(0.38, (wu0 - bu0) * 0.72)
            if sw > 0.16:
                local_box(metal_mat, wu0 - sw * 0.5 - 0.03, 0.04, zc, sw, 0.06, wh)
                local_box(metal_mat, wu1 + sw * 0.5 + 0.03, 0.04, zc, sw, 0.06, wh)


def _add_balcony(b, slab_mat, rail_mat, face, w, d, u_center, z_floor, width, depth=0.95, rail_h=0.98):
    """Protruding 3D concrete balcony slab with slim dark-iron balustrade posts & top/mid rails."""
    hw, hd = w * 0.5, d * 0.5

    def local_box(mat, uc, vc, zc, su, sv, sz):
        if face == "+X":
            _box_uv(b, mat, hw + vc, uc, zc, sv, su, sz)
        elif face == "-X":
            _box_uv(b, mat, -hw - vc, -uc, zc, sv, su, sz)
        elif face == "+Y":
            _box_uv(b, mat, -uc, hd + vc, zc, su, sv, sz)
        else:
            _box_uv(b, mat, uc, -hd - vc, zc, su, sv, sz)

    # Balcony floor slab
    local_box(slab_mat, u_center, depth * 0.5, z_floor - 0.09, width, depth + 0.08, 0.18)
    # Front top & bottom rails
    local_box(rail_mat, u_center, depth - 0.04, z_floor + rail_h, width, 0.05, 0.05)
    local_box(rail_mat, u_center, depth - 0.04, z_floor + 0.14, width, 0.04, 0.04)
    # Left & right side rails
    for s in (-1.0, 1.0):
        local_box(rail_mat, u_center + s * (width * 0.5 - 0.03), depth * 0.5, z_floor + rail_h, 0.05, depth, 0.05)
        local_box(rail_mat, u_center + s * (width * 0.5 - 0.03), depth * 0.5, z_floor + 0.14, 0.04, depth, 0.04)
    # Vertical iron balusters along front edge
    n_posts = max(4, int(width / 0.42))
    for k in range(n_posts + 1):
        pu = u_center - width * 0.5 + 0.03 + (width - 0.06) * (k / float(n_posts))
        local_box(rail_mat, pu, depth - 0.04, z_floor + rail_h * 0.5, 0.032, 0.032, rail_h)


def _add_shopfront_ground(b, trim_mat, metal_mat, glass_mat, awning_mat, w, d, h_ground=4.0, faces=("+X", "-X", "+Y", "-Y")):
    """Ground-floor urban commercial arcade: heavy structural stone piers, recessed warm-lit display glass,
    and protruding sloped commercial awnings."""
    hw, hd = w * 0.5, d * 0.5
    # Core inner dark backing box (recessed 0.35 m inside footprint so piers have true 3D depth)
    _box_uv(b, trim_mat, 0.0, 0.0, h_ground * 0.5, w - 0.70, d - 0.70, h_ground)
    # Corner thick stone pilasters
    for sx in (-1.0, 1.0):
        for sy in (-1.0, 1.0):
            _box_uv(b, trim_mat, sx * (hw - 0.35), sy * (hd - 0.35), h_ground * 0.5, 0.78, 0.78, h_ground)
    # Horizontal entablature fascia band above ground floor
    _box_uv(b, trim_mat, 0.0, 0.0, h_ground - 0.22, w + 0.22, d + 0.22, 0.44)

    for face in faces:
        span = d if face in ("+X", "-X") else w
        n_bays = max(2, int(round(span / 4.2)))
        bay = (span - 1.2) / float(n_bays)
        u0_base = -(span - 1.2) * 0.5

        def local_box(mat, uc, vc, zc, su, sv, sz):
            if face == "+X":
                _box_uv(b, mat, hw + vc, uc, zc, sv, su, sz)
            elif face == "-X":
                _box_uv(b, mat, -hw - vc, -uc, zc, sv, su, sz)
            elif face == "+Y":
                _box_uv(b, mat, -uc, hd + vc, zc, su, sv, sz)
            else:
                _box_uv(b, mat, uc, -hd - vc, zc, su, sv, sz)

        # Intermediate stone columns between shop bays
        for k in range(1, n_bays):
            pu = u0_base + k * bay
            local_box(trim_mat, pu, -0.10, h_ground * 0.5, 0.56, 0.44, h_ground)

        # Recessed display windows + transom frames + sloped protruding commercial awning
        for k in range(n_bays):
            uc = u0_base + (k + 0.5) * bay
            bw = bay - 0.62
            # Recessed shop display glass
            local_box(glass_mat, uc, -0.32, 1.80, bw, 0.06, 2.75)
            # Dark-metal framing mullions
            local_box(metal_mat, uc, -0.28, 0.42, bw + 0.06, 0.10, 0.10)
            local_box(metal_mat, uc, -0.28, 3.16, bw + 0.06, 0.10, 0.10)
            local_box(metal_mat, uc, -0.28, 1.80, 0.06, 0.08, 2.75)
            # Protruding angled commercial awning (sloped wedge box built from quads)
            aw = bw + 0.18
            ad = 1.05
            z_top, z_bot = 3.38, 2.86
            au0, au1 = uc - aw * 0.5, uc + aw * 0.5

            def pt(u, v, z):
                if face == "+X":
                    return (hw + v, u, z)
                if face == "-X":
                    return (-hw - v, -u, z)
                if face == "+Y":
                    return (-u, hd + v, z)
                return (u, -hd - v, z)

            p0, p1, p2, p3 = pt(au0, 0.0, z_top), pt(au1, 0.0, z_top), pt(au1, ad, z_bot), pt(au0, ad, z_bot)
            b.add_quad(awning_mat, p0, p3, p2, p1)
            # Vertical front valance skirt of awning
            q0, q1, q2, q3 = pt(au0, ad, z_bot), pt(au1, ad, z_bot), pt(au1, ad, z_bot - 0.22), pt(au0, ad, z_bot - 0.22)
            b.add_quad(awning_mat, q0, q3, q2, q1)
            # Side triangular gussets
            b.add_tri(awning_mat, pt(au0, 0.0, z_top), pt(au0, 0.0, z_bot), pt(au0, ad, z_bot))
            b.add_tri(awning_mat, pt(au1, 0.0, z_top), pt(au1, ad, z_bot), pt(au1, 0.0, z_bot))


def _add_hollow_parapet_roof(b, roof_mat, trim_mat, metal_mat, w, d, z_roof, parapet_h=0.95, add_bulkhead=True):
    """Recessed flat roof slab surrounded by a true 3D parapet wall + coping cap + rooftop plant/bulkhead."""
    hw, hd = w * 0.5, d * 0.5
    pt = 0.42
    # Main recessed roof deck
    _box_uv(b, roof_mat, 0.0, 0.0, z_roof - 0.12, w - 0.1, d - 0.1, 0.24)
    # 4 perimeter parapet walls (hollow interior — real skyline silhouette!)
    _box_uv(b, trim_mat, 0.0, hd - pt * 0.5, z_roof + parapet_h * 0.5, w, pt, parapet_h)
    _box_uv(b, trim_mat, 0.0, -hd + pt * 0.5, z_roof + parapet_h * 0.5, w, pt, parapet_h)
    _box_uv(b, trim_mat, hw - pt * 0.5, 0.0, z_roof + parapet_h * 0.5, pt, d - pt * 2.0, parapet_h)
    _box_uv(b, trim_mat, -hw + pt * 0.5, 0.0, z_roof + parapet_h * 0.5, pt, d - pt * 2.0, parapet_h)
    # Overhanging cornice molding right under parapet top
    _box_uv(b, trim_mat, 0.0, hd + 0.08, z_roof + parapet_h + 0.07, w + 0.36, pt + 0.24, 0.14)
    _box_uv(b, trim_mat, 0.0, -hd - 0.08, z_roof + parapet_h + 0.07, w + 0.36, pt + 0.24, 0.14)
    _box_uv(b, trim_mat, hw + 0.08, 0.0, z_roof + parapet_h + 0.07, pt + 0.24, d + 0.12, 0.14)
    _box_uv(b, trim_mat, -hw - 0.08, 0.0, z_roof + parapet_h + 0.07, pt + 0.24, d + 0.12, 0.14)

    if add_bulkhead:
        # Rooftop stairwell bulkhead & HVAC mechanical unit
        bw, bd, bh = min(4.2, w * 0.30), min(3.4, d * 0.28), 2.5
        _box_uv(b, trim_mat, -w * 0.20, d * 0.18, z_roof + bh * 0.5, bw, bd, bh)
        _box_uv(b, metal_mat, -w * 0.20, d * 0.18, z_roof + bh + 0.08, bw + 0.2, bd + 0.2, 0.16)
        _box_uv(b, metal_mat, w * 0.18, -d * 0.16, z_roof + 0.65, min(2.4, w * 0.18), min(1.8, d * 0.16), 1.30)


def _add_perimeter_crown(b, crown_mat, w, d, z_top, thick=0.22, height=0.26):
    """Hollow 4-edge skyline neon trim band around the parapet perimeter (never covers the roof deck!)."""
    hw, hd = w * 0.5, d * 0.5
    _box_uv(b, crown_mat, 0.0, hd + thick * 0.35, z_top, w + thick * 1.4, thick, height)
    _box_uv(b, crown_mat, 0.0, -hd - thick * 0.35, z_top, w + thick * 1.4, thick, height)
    _box_uv(b, crown_mat, hw + thick * 0.35, 0.0, z_top, thick, d + thick * 0.6, height)
    _box_uv(b, crown_mat, -hw - thick * 0.35, 0.0, z_top, thick, d + thick * 0.6, height)


def _add_hip_roof(b, tile_mat, trim_mat, w, d, z_eave, pitch_h=2.8, overhang=0.65):
    """Overhanging 4-slope Anatolian clay-tile hip roof with fascia board & brick chimney."""
    hx = w * 0.5 + overhang
    hy = d * 0.5 + overhang
    long_x = w >= d
    ridge_half = max(0.4, abs(w - d) * 0.46)
    r0 = (-ridge_half, 0.0, z_eave + pitch_h) if long_x else (0.0, -ridge_half, z_eave + pitch_h)
    r1 = (ridge_half, 0.0, z_eave + pitch_h) if long_x else (0.0, ridge_half, z_eave + pitch_h)
    c00 = (-hx, -hy, z_eave)
    c10 = (hx, -hy, z_eave)
    c11 = (hx, hy, z_eave)
    c01 = (-hx, hy, z_eave)
    # Outward-facing roof slopes (CCW viewed from outside)
    if long_x:
        b.add_quad(tile_mat, c00, c10, r1, r0)  # -Y slope
        b.add_quad(tile_mat, c11, c01, r0, r1)  # +Y slope
        b.add_tri(tile_mat, c10, c11, r1)       # +X hip
        b.add_tri(tile_mat, c01, c00, r0)       # -X hip
    else:
        b.add_quad(tile_mat, c10, c11, r1, r0)  # +X slope
        b.add_quad(tile_mat, c01, c00, r0, r1)  # -X slope
        b.add_tri(tile_mat, c11, c01, r1)       # +Y hip
        b.add_tri(tile_mat, c00, c10, r0)       # -Y hip
    # Eave fascia & soffit box
    _box_uv(b, trim_mat, 0.0, 0.0, z_eave - 0.14, hx * 2.0, hy * 2.0, 0.28)
    # Brick chimney poking cleanly through roof slope
    _box_uv(b, trim_mat, hx * 0.32, -hy * 0.28, z_eave + pitch_h * 0.72, 0.72, 0.72, pitch_h * 0.95)
    _box_uv(b, trim_mat, hx * 0.32, -hy * 0.28, z_eave + pitch_h * 1.22, 0.88, 0.88, 0.14)


# ---------------------------------------------------------------------------
# 9 True-3D Architectural District Building Builders
# ---------------------------------------------------------------------------
def build_bldg_glass_tower(b):
    """Altın Çekirdek Financial Glass Skyscraper (20 x 18 x 40 m).
    Slots: [MI_Wall_Cladding, MI_Concrete, MI_Metal_Dark, MI_Arch_GlassCool, MI_CrownGlow_Cyan]"""
    M_WALL, M_CONC, M_MET, M_GLASS, M_CROWN = 0, 1, 2, 3, 4
    w, d = 20.0, 18.0
    # Double-height recessed lobby colonnade (0..5.2 m)
    _box_uv(b, M_GLASS, 0.0, 0.0, 2.5, w - 1.4, d - 1.4, 5.0)
    for sx in (-1.0, -0.35, 0.35, 1.0):
        for sy in (-1.0, 1.0):
            _box_uv(b, M_CONC, sx * (w * 0.5 - 0.45), sy * (d * 0.5 - 0.45), 2.6, 0.95, 0.95, 5.2)
    for sy in (-0.35, 0.35):
        for sx in (-1.0, 1.0):
            _box_uv(b, M_CONC, sx * (w * 0.5 - 0.45), sy * (d * 0.5 - 0.45), 2.6, 0.95, 0.95, 5.2)
    _box_uv(b, M_WALL, 0.0, 0.0, 5.35, w + 0.3, d + 0.3, 0.7)

    # Main curtain-wall shaft (5.7..32.7 m = 9 office floors @ 3.0 m)
    shaft_z0, shaft_z1 = 5.7, 32.7
    _box_uv(b, M_GLASS, 0.0, 0.0, 0.5 * (shaft_z0 + shaft_z1), w - 0.36, d - 0.36, shaft_z1 - shaft_z0)
    # 3D protruding horizontal spandrel floor slabs every 3.0 m
    for fl in range(10):
        zf = shaft_z0 + fl * 3.0
        _box_uv(b, M_WALL, 0.0, 0.0, zf, w + 0.16, d + 0.16, 0.58)
    # 3D protruding vertical aluminum curtain-wall mullions around all 4 facades
    for i in range(9):
        ux = -w * 0.5 + (i / 8.0) * w
        for sy in (-1.0, 1.0):
            _box_uv(b, M_MET, ux, sy * (d * 0.5 + 0.04), 0.5 * (shaft_z0 + shaft_z1), 0.22, 0.26, shaft_z1 - shaft_z0)
    for i in range(8):
        uy = -d * 0.5 + (i / 7.0) * d
        for sx in (-1.0, 1.0):
            _box_uv(b, M_MET, sx * (w * 0.5 + 0.04), uy, 0.5 * (shaft_z0 + shaft_z1), 0.26, 0.22, shaft_z1 - shaft_z0)
    # Heavy corner cladding fins
    for sx in (-1.0, 1.0):
        for sy in (-1.0, 1.0):
            _box_uv(b, M_WALL, sx * (w * 0.5 - 0.25), sy * (d * 0.5 - 0.25), 0.5 * (shaft_z0 + shaft_z1), 1.05, 1.05, shaft_z1 - shaft_z0 + 0.8)

    # Stepped upper penthouse setback (32.7..38.7 m) + glowing skyline crown + twin spire
    pw, pd = w * 0.72, d * 0.72
    _box_uv(b, M_GLASS, 0.0, 0.0, 35.7, pw - 0.25, pd - 0.25, 6.0)
    for fl in (35.7, 38.7):
        _box_uv(b, M_WALL, 0.0, 0.0, fl, pw + 0.2, pd + 0.2, 0.55)
    for sx in (-1.0, 0.0, 1.0):
        for sy in (-1.0, 1.0):
            _box_uv(b, M_MET, sx * pw * 0.48, sy * (pd * 0.5 + 0.03), 35.7, 0.28, 0.24, 6.0)
    _add_hollow_parapet_roof(b, M_CONC, M_WALL, M_MET, pw, pd, 39.0, parapet_h=1.1, add_bulkhead=False)
    _add_perimeter_crown(b, M_CROWN, pw + 0.16, pd + 0.16, 40.05, thick=0.24, height=0.26)
    _box_uv(b, M_MET, 0.0, 0.0, 44.5, 0.32, 0.32, 9.0)
    _box_uv(b, M_MET, 1.4, -1.2, 42.8, 0.20, 0.20, 5.6)


def build_bldg_concrete_tower(b):
    """Altın Çekirdek / Merkez Brutalist-Modern Office Tower (18 x 15 x 30 m).
    Slots: [MI_Wall_ConcretePanel, MI_Concrete, MI_Metal_Dark, MI_Arch_GlassWarm, MI_CrownGlow_Amber]"""
    M_WALL, M_CONC, M_MET, M_GLASS, M_CROWN = 0, 1, 2, 3, 4
    w, d = 18.0, 15.0
    _add_shopfront_ground(b, M_CONC, M_MET, M_GLASS, M_MET, w, d, h_ground=4.4)
    # 7 upper office floors @ 3.2 m (4.4..26.8 m) with recessed 3D windows + protruding horizontal sunshades
    for fl in range(7):
        z0 = 4.4 + fl * 3.2
        z1 = z0 + 3.2
        for face in ("+X", "-X", "+Y", "-Y"):
            cols = 4 if face in ("+X", "-X") else 5
            _add_wall_with_windows(b, M_WALL, M_CONC, M_MET, M_GLASS, face, w, d, z0, z1, cols, 2.25, 1.78, recess=0.22)
        # Protruding brise-soleil concrete floor band every 2 floors
        if fl % 2 == 1:
            _box_uv(b, M_CONC, 0.0, 0.0, z1, w + 0.44, d + 0.44, 0.24)
    # Vertical structural corner pilasters
    for sx in (-1.0, 1.0):
        for sy in (-1.0, 1.0):
            _box_uv(b, M_CONC, sx * (w * 0.5 + 0.04), sy * (d * 0.5 + 0.04), 15.6, 0.85, 0.85, 22.8)
    _add_hollow_parapet_roof(b, M_CONC, M_CONC, M_MET, w, d, 26.8, parapet_h=1.1, add_bulkhead=True)
    _add_perimeter_crown(b, M_CROWN, w + 0.18, d + 0.18, 27.85, thick=0.22, height=0.24)


def build_bldg_apt_brick(b):
    """Merkez 5-Storey Brick Urban Apartment Block with shopfront arcade, awnings, 3D recessed windows & balconies (16 x 13 x 17.6 m).
    Slots: [MI_Wall_Brick, MI_Concrete, MI_Metal_Dark, MI_Arch_GlassWarm, MI_Arch_Awning]"""
    M_WALL, M_CONC, M_MET, M_GLASS, M_AWN = 0, 1, 2, 3, 4
    w, d = 16.0, 13.0
    _add_shopfront_ground(b, M_CONC, M_MET, M_GLASS, M_AWN, w, d, h_ground=4.0, faces=("+X", "-X", "+Y", "-Y"))
    for fl in range(4):
        z0 = 4.0 + fl * 3.2
        z1 = z0 + 3.2
        for face in ("+X", "-X", "+Y", "-Y"):
            cols = 4 if face in ("+Y", "-Y") else 3
            _add_wall_with_windows(b, M_WALL, M_CONC, M_MET, M_GLASS, face, w, d, z0, z1, cols, 1.72, 1.75, recess=0.20)
        # Protruding concrete balconies with iron balustrades on both long facades (+Y / -Y)
        for face in ("+Y", "-Y"):
            for bu in (-4.0, 4.0):
                _add_balcony(b, M_CONC, M_MET, face, w, d, bu, z0 + 0.05, width=3.1, depth=0.95)
    # Corner stone quoins / pilasters
    for sx in (-1.0, 1.0):
        for sy in (-1.0, 1.0):
            _box_uv(b, M_CONC, sx * (w * 0.5), sy * (d * 0.5), 10.4, 0.60, 0.60, 12.8)
    _add_hollow_parapet_roof(b, M_CONC, M_CONC, M_MET, w, d, 16.8, parapet_h=0.90, add_bulkhead=True)


def build_bldg_apt_plaster(b):
    """Merkez / Carsi 4-Storey Anatolian Stucco Apartment Block with cantilevered bay projections ('cumba'), awnings & balconies (15 x 12.5 x 14.8 m).
    Slots: [MI_Wall_Plaster, MI_Concrete, MI_Metal_Dark, MI_Arch_GlassWarm, MI_Arch_Awning]"""
    M_WALL, M_CONC, M_MET, M_GLASS, M_AWN = 0, 1, 2, 3, 4
    w, d = 15.0, 12.5
    _add_shopfront_ground(b, M_CONC, M_MET, M_GLASS, M_AWN, w, d, h_ground=3.8, faces=("+X", "-X", "+Y", "-Y"))
    for fl in range(3):
        z0 = 3.8 + fl * 3.3
        z1 = z0 + 3.3
        for face in ("+X", "-X", "+Y", "-Y"):
            cols = 4 if face in ("+Y", "-Y") else 3
            _add_wall_with_windows(b, M_WALL, M_CONC, M_MET, M_GLASS, face, w, d, z0, z1, cols, 1.65, 1.80, recess=0.18)
        # Cantilevered central bay window projection on +Y / -Y + flanking balconies
        for sy in (-1.0, 1.0):
            _box_uv(b, M_WALL, 0.0, sy * (d * 0.5 + 0.28), 0.5 * (z0 + z1), 4.6, 0.56, 3.28)
            _box_uv(b, M_GLASS, 0.0, sy * (d * 0.5 + 0.54), z0 + 1.68, 3.4, 0.08, 1.72)
            _box_uv(b, M_CONC, 0.0, sy * (d * 0.5 + 0.58), z0 + 0.78, 3.7, 0.22, 0.12)
        for face in ("+Y", "-Y"):
            for bu in (-4.8, 4.8):
                _add_balcony(b, M_CONC, M_MET, face, w, d, bu, z0 + 0.04, width=2.8, depth=0.90)
    _add_hollow_parapet_roof(b, M_CONC, M_CONC, M_MET, w, d, 13.7, parapet_h=0.95, add_bulkhead=True)


def build_bldg_corner_block(b):
    """Merkez 4-Storey Corner Commercial Block with deep colonnade, awnings & dual-street balconies (14 x 14 x 15.0 m).
    Slots: [MI_Wall_ConcretePanel, MI_Concrete, MI_Metal_Dark, MI_Arch_GlassWarm, MI_Arch_Awning]"""
    M_WALL, M_CONC, M_MET, M_GLASS, M_AWN = 0, 1, 2, 3, 4
    w, d = 14.0, 14.0
    _add_shopfront_ground(b, M_CONC, M_MET, M_GLASS, M_AWN, w, d, h_ground=4.1)
    for fl in range(3):
        z0 = 4.1 + fl * 3.3
        z1 = z0 + 3.3
        for face in ("+X", "-X", "+Y", "-Y"):
            _add_wall_with_windows(b, M_WALL, M_CONC, M_MET, M_GLASS, face, w, d, z0, z1, 3, 2.05, 1.82, recess=0.20)
            _add_balcony(b, M_CONC, M_MET, face, w, d, 0.0, z0 + 0.05, width=3.4, depth=0.92)
    for sx in (-1.0, 1.0):
        for sy in (-1.0, 1.0):
            _box_uv(b, M_CONC, sx * (w * 0.5), sy * (d * 0.5), 9.0, 0.72, 0.72, 10.0)
    _add_hollow_parapet_roof(b, M_CONC, M_CONC, M_MET, w, d, 14.0, parapet_h=0.92, add_bulkhead=True)


def build_bldg_yamac_villa_a(b):
    """Yamac Mahallesi (Eski Karamara) 2-Storey Mediterranean Plaster Hillside Villa with deep stone skirt, shutters, balcony & clay-tile hip roof (12 x 10 x 9.6 m).
    Slots: [MI_Wall_Plaster, MI_Concrete, MI_Metal_Dark, MI_Arch_GlassWarm, MI_Building_Terracotta]"""
    M_WALL, M_CONC, M_MET, M_GLASS, M_TILE = 0, 1, 2, 3, 4
    w, d = 12.0, 10.0
    # Deep grounded stone-plinth basement skirt (-8.0 m .. +0.7 m) so hillside slopes NEVER show air!
    _box_uv(b, M_CONC, 0.0, 0.0, -3.65, w + 0.35, d + 0.35, 8.7)
    for fl in range(2):
        z0 = 0.7 + fl * 3.1
        z1 = z0 + 3.1
        for face in ("+X", "-X", "+Y", "-Y"):
            cols = 3 if face in ("+Y", "-Y") else 2
            _add_wall_with_windows(b, M_WALL, M_CONC, M_MET, M_GLASS, face, w, d, z0, z1, cols, 1.45, 1.55, recess=0.18, add_shutters=True)
    # 2nd-floor protruding terrace balcony facing the view (+Y and +X)
    _add_balcony(b, M_CONC, M_MET, "+Y", w, d, 0.0, 3.82, width=4.6, depth=1.15)
    _add_balcony(b, M_CONC, M_MET, "-Y", w, d, 0.0, 3.82, width=3.2, depth=0.95)
    # Stringcourse belt between floors
    _box_uv(b, M_CONC, 0.0, 0.0, 3.80, w + 0.24, d + 0.24, 0.20)
    _add_hip_roof(b, M_TILE, M_CONC, w, d, z_eave=6.95, pitch_h=2.75, overhang=0.72)


def build_bldg_yamac_villa_b(b):
    """Yamac Mahallesi 3-Storey Brick & Plaster Hillside Townhouse with stone skirt, upper bay & clay-tile roof (11 x 10.5 x 12.6 m).
    Slots: [MI_Wall_Brick, MI_Concrete, MI_Metal_Dark, MI_Arch_GlassWarm, MI_Building_Terracotta]"""
    M_WALL, M_CONC, M_MET, M_GLASS, M_TILE = 0, 1, 2, 3, 4
    w, d = 11.0, 10.5
    _box_uv(b, M_CONC, 0.0, 0.0, -3.65, w + 0.35, d + 0.35, 8.7)
    for fl in range(3):
        z0 = 0.7 + fl * 3.0
        z1 = z0 + 3.0
        for face in ("+X", "-X", "+Y", "-Y"):
            cols = 3 if face in ("+Y", "-Y") else 2
            _add_wall_with_windows(b, M_WALL, M_CONC, M_MET, M_GLASS, face, w, d, z0, z1, cols, 1.42, 1.52, recess=0.18, add_shutters=True)
    _add_balcony(b, M_CONC, M_MET, "+Y", w, d, 0.0, 3.72, width=4.2, depth=1.05)
    _add_balcony(b, M_CONC, M_MET, "+Y", w, d, 0.0, 6.72, width=4.2, depth=1.05)
    _box_uv(b, M_CONC, 0.0, 0.0, 3.70, w + 0.22, d + 0.22, 0.18)
    _box_uv(b, M_CONC, 0.0, 0.0, 6.70, w + 0.22, d + 0.22, 0.18)
    _add_hip_roof(b, M_TILE, M_CONC, w, d, z_eave=9.75, pitch_h=2.85, overhang=0.70)


def build_bldg_factory_hall(b):
    """Dogu Sanayi (Pasli Demir) Industrial Monitor-Roof Factory with exterior steel I-beams, recessed loading bays, hoods & glowing clerestory skylight (22 x 16 x 11.8 m).
    Slots: [MI_Building_Steel, MI_Concrete, MI_Metal_Dark, MI_Arch_GlassWarm, MI_LampGlow]"""
    M_STEEL, M_CONC, M_MET, M_GLASS, M_GLOW = 0, 1, 2, 3, 4
    w, d, h = 22.0, 16.0, 9.2
    # Deep grounded concrete foundation plinth (-3.5..+1.4 m)
    _box_uv(b, M_CONC, 0.0, 0.0, -1.05, w + 0.35, d + 0.35, 4.9)
    # Main corrugated steel hall shell
    _box_uv(b, M_STEEL, 0.0, 0.0, 1.4 + (h - 1.4) * 0.5, w, d, h - 1.4)
    # Exterior vertical structural steel I-beam pilasters along all 4 sides
    for i in range(6):
        ux = -w * 0.5 + (i / 5.0) * w
        for sy in (-1.0, 1.0):
            _box_uv(b, M_MET, ux, sy * (d * 0.5 + 0.14), h * 0.5, 0.42, 0.36, h)
    for i in range(4):
        uy = -d * 0.5 + (i / 3.0) * d
        for sx in (-1.0, 1.0):
            _box_uv(b, M_MET, sx * (w * 0.5 + 0.14), uy, h * 0.5, 0.36, 0.42, h)
    # High-wall recessed industrial ribbon windows along both long sides (+Y / -Y)
    for sy in (-1.0, 1.0):
        _box_uv(b, M_GLASS, 0.0, sy * (d * 0.5 + 0.02), h - 1.85, w - 2.2, 0.08, 1.45)
        _box_uv(b, M_MET, 0.0, sy * (d * 0.5 + 0.08), h - 2.60, w - 1.9, 0.18, 0.12)
        _box_uv(b, M_MET, 0.0, sy * (d * 0.5 + 0.08), h - 1.10, w - 1.9, 0.18, 0.12)
    # 3 loading bay doors on BOTH -Y and +Y with protruding steel rain canopies & warm dock lamps
    for sy in (-1.0, 1.0):
        for bx in (-6.5, 0.0, 6.5):
            _box_uv(b, M_MET, bx, sy * (d * 0.5 + 0.05), 2.1, 3.6, 0.16, 4.2)
            # Protruding corrugated canopy hood above loading bay
            _box_uv(b, M_MET, bx, sy * (d * 0.5 + 0.62), 4.42, 4.2, 1.25, 0.18)
            _box_uv(b, M_GLOW, bx, sy * (d * 0.5 + 0.40), 4.28, 1.3, 0.22, 0.12)
    # Overhanging industrial roof eaves + raised monitor clerestory skylight
    _box_uv(b, M_MET, 0.0, 0.0, h + 0.18, w + 1.1, d + 1.1, 0.36)
    _box_uv(b, M_MET, 0.0, 0.0, h + 1.25, w * 0.80, d * 0.36, 1.9)
    _box_uv(b, M_GLASS, 0.0, d * 0.18 + 0.03, h + 1.25, w * 0.76, 0.08, 1.15)
    _box_uv(b, M_GLASS, 0.0, -d * 0.18 - 0.03, h + 1.25, w * 0.76, 0.08, 1.15)
    _box_uv(b, M_MET, 0.0, 0.0, h + 2.30, w * 0.84, d * 0.42, 0.24)
    # Twin rooftop exhaust stacks
    for ex in (-7.2, 7.2):
        b.add_frustum_cyl(M_MET, (ex, d * 0.32, h + 0.30), 0.42, 0.38, 2.2, segments=10)


def build_bldg_harbor_depot(b):
    """Eski Liman (Mara Rihtimi) Maritime Steel-Truss Transit Warehouse with pitched gable roof & rust-red structural trim (18 x 14 x 11.2 m).
    Slots: [MI_Building_Steel, MI_Concrete, MI_BridgeSteel, MI_Metal_Dark, MI_LampGlow]"""
    M_STEEL, M_CONC, M_RUST, M_MET, M_GLOW = 0, 1, 2, 3, 4
    w, d, h = 18.0, 14.0, 8.6
    _box_uv(b, M_CONC, 0.0, 0.0, -1.1, w + 0.35, d + 0.35, 4.6)
    _box_uv(b, M_STEEL, 0.0, 0.0, 1.2 + (h - 1.2) * 0.5, w, d, h - 1.2)
    # Red-rust bridge-steel perimeter portal frames
    for i in range(5):
        ux = -w * 0.5 + (i / 4.0) * w
        for sy in (-1.0, 1.0):
            _box_uv(b, M_RUST, ux, sy * (d * 0.5 + 0.14), h * 0.5, 0.40, 0.32, h)
    # Cargo doors & dock awnings
    for sy in (-1.0, 1.0):
        for bx in (-4.4, 4.4):
            _box_uv(b, M_MET, bx, sy * (d * 0.5 + 0.06), 2.3, 4.0, 0.16, 4.6)
            _box_uv(b, M_RUST, bx, sy * (d * 0.5 + 0.65), 4.78, 4.6, 1.30, 0.20)
            _box_uv(b, M_GLOW, bx, sy * (d * 0.5 + 0.40), 4.62, 1.5, 0.22, 0.12)
    # Pitched overhanging maritime gable roof (ridge along X)
    hx, hy = w * 0.5 + 0.75, d * 0.5 + 0.80
    rh = 2.65
    r0, r1 = (-hx, 0.0, h + rh), (hx, 0.0, h + rh)
    c00, c10, c11, c01 = (-hx, -hy, h), (hx, -hy, h), (hx, hy, h), (-hx, hy, h)
    b.add_quad(M_MET, c00, c10, r1, r0)
    b.add_quad(M_MET, c11, c01, r0, r1)
    # Solid gable end triangles (+X and -X)
    b.add_tri(M_STEEL, (w * 0.5, -d * 0.5, h), (w * 0.5, d * 0.5, h), (w * 0.5, 0.0, h + rh - 0.05))
    b.add_tri(M_STEEL, (-w * 0.5, d * 0.5, h), (-w * 0.5, -d * 0.5, h), (-w * 0.5, 0.0, h + rh - 0.05))
    # Fascia trim along eaves
    _box_uv(b, M_RUST, 0.0, hy, h - 0.05, hx * 2.0, 0.22, 0.36)
    _box_uv(b, M_RUST, 0.0, -hy, h - 0.05, hx * 2.0, 0.22, 0.36)


BUILDING_MESH_MANIFEST = [
    ("SM_BN_Bldg_GlassTower",    build_bldg_glass_tower,    ["MI_Wall_Cladding", "MI_Concrete", "MI_Metal_Dark", "MI_Arch_GlassCool", "MI_CrownGlow_Cyan"], "simple"),
    ("SM_BN_Bldg_ConcreteTower", build_bldg_concrete_tower, ["MI_Wall_ConcretePanel", "MI_Concrete", "MI_Metal_Dark", "MI_Arch_GlassWarm", "MI_CrownGlow_Amber"], "simple"),
    ("SM_BN_Bldg_AptBrick",      build_bldg_apt_brick,      ["MI_Wall_Brick", "MI_Concrete", "MI_Metal_Dark", "MI_Arch_GlassWarm", "MI_Arch_Awning"], "simple"),
    ("SM_BN_Bldg_AptPlaster",    build_bldg_apt_plaster,    ["MI_Wall_Plaster", "MI_Concrete", "MI_Metal_Dark", "MI_Arch_GlassWarm", "MI_Arch_Awning"], "simple"),
    ("SM_BN_Bldg_CornerBlock",   build_bldg_corner_block,   ["MI_Wall_ConcretePanel", "MI_Concrete", "MI_Metal_Dark", "MI_Arch_GlassWarm", "MI_Arch_Awning"], "simple"),
    ("SM_BN_Bldg_YamacVillaA",   build_bldg_yamac_villa_a,  ["MI_Wall_Plaster", "MI_Concrete", "MI_Metal_Dark", "MI_Arch_GlassWarm", "MI_Building_Terracotta"], "simple"),
    ("SM_BN_Bldg_YamacVillaB",   build_bldg_yamac_villa_b,  ["MI_Wall_Brick", "MI_Concrete", "MI_Metal_Dark", "MI_Arch_GlassWarm", "MI_Building_Terracotta"], "simple"),
    ("SM_BN_Bldg_FactoryHall",   build_bldg_factory_hall,   ["MI_Building_Steel", "MI_Concrete", "MI_Metal_Dark", "MI_Arch_GlassWarm", "MI_LampGlow"], "simple"),
    ("SM_BN_Bldg_HarborDepot",   build_bldg_harbor_depot,   ["MI_Building_Steel", "MI_Concrete", "MI_BridgeSteel", "MI_Metal_Dark", "MI_LampGlow"], "simple"),
]

# Base native footprints (width_cm, depth_cm, height_cm) so scenery scales cleanly around 1.0x without warping windows
BUILDING_NATIVE_DIMS = {
    "SM_BN_Bldg_GlassTower":    (2000.0, 1800.0, 3900.0),
    "SM_BN_Bldg_ConcreteTower": (1800.0, 1500.0, 2780.0),
    "SM_BN_Bldg_AptBrick":      (1600.0, 1300.0, 1770.0),
    "SM_BN_Bldg_AptPlaster":    (1500.0, 1250.0, 1465.0),
    "SM_BN_Bldg_CornerBlock":   (1400.0, 1400.0, 1492.0),
    "SM_BN_Bldg_YamacVillaA":   (1200.0, 1000.0,  970.0),
    "SM_BN_Bldg_YamacVillaB":   (1100.0, 1050.0, 1260.0),
    "SM_BN_Bldg_FactoryHall":   (2200.0, 1600.0, 1150.0),
    "SM_BN_Bldg_HarborDepot":   (1800.0, 1400.0, 1125.0),
}


if __name__ == "__main__":
    import sys
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import bn_meshes
    os.makedirs(bn_meshes.OUT_DIR, exist_ok=True)
    for name, fn, mats, _ in BUILDING_MESH_MANIFEST:
        b = bn_meshes.GltfBuilder(mats)
        fn(b)
        out = os.path.join(bn_meshes.OUT_DIR, name + ".glb")
        b.save_glb(out)
        tris = sum(len(s["idx"]) // 3 for s in b.slots.values())
        print("BN: built %s (%d tris, %d slots, %.1f KB)" % (name, tris, len(mats), os.path.getsize(out) / 1024.0))
