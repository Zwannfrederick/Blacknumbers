"""Single-piece continuous 3D road network & welded junction mesh generator for BLACK NUMBERS.

Replaces thousands of discrete overlapping road slabs, crooked junction cylinders, and gapped
boundary-wall blocks with vertex-welded glTF 2.0 (.glb) StaticMeshes derived directly from
Tools/karamara_network.py:
  - SM_BN_RoadNet_Asphalt    (MI_Asphalt,        PM_Asphalt,  complex collision)
  - SM_BN_RoadNet_Highway    (MI_HighwayAsphalt, PM_Asphalt,  complex collision)
  - SM_BN_RoadNet_Cobble     (MI_Cobblestone,    PM_Asphalt,  complex collision)
  - SM_BN_RoadNet_Dirt       (MI_Dirt,           PM_Dirt,     complex collision)
  - SM_BN_RoadNet_Sidewalk   (MI_Sidewalk,       PM_Concrete, complex collision)
  - SM_BN_RoadNet_Wall       (walls/curbs/jersey barriers/rails, PM_Concrete, complex collision)
  - SM_BN_RoadNet_Structure  (viaduct box girders, piers, retaining embankments, PM_Concrete, complex)
  - SM_BN_RoadNet_Markings   (lane dashes, edge lines, crosswalks, stop bars — NoCollision)

Run standalone:
    python3 Tools/bn_road_mesh.py
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bn_meshes  # noqa: E402
import karamara_network as knet  # noqa: E402

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(PROJECT_DIR, "Saved", "GeneratedMeshes")
UE_MESH_DIR = "/Game/World/Meshes"
UE_MAT_DIR = "/Game/World/Materials"

ROAD_MESH_MANIFEST = [
    ("SM_BN_RoadNet_Asphalt",   ["MI_Asphalt"], "PM_Asphalt", "complex", False),
    ("SM_BN_RoadNet_Highway",   ["MI_HighwayAsphalt"], "PM_Asphalt", "complex", False),
    ("SM_BN_RoadNet_Cobble",    ["MI_Cobblestone"], "PM_Asphalt", "complex", False),
    ("SM_BN_RoadNet_Dirt",      ["MI_Dirt"], "PM_Dirt", "complex", False),
    ("SM_BN_RoadNet_Sidewalk",  ["MI_Sidewalk", "MI_Concrete", "MI_Dirt"], "PM_Concrete", "complex", False),
    ("SM_BN_RoadNet_Wall",      ["MI_Concrete", "MI_Sidewalk", "MI_Cobblestone", "MI_Metal_Dark",
                                 "MI_HazardYellow", "MI_CrateWood"], "PM_Concrete", "complex", False),
    ("SM_BN_RoadNet_Structure", ["MI_Concrete", "MI_MountainRock"], "PM_Concrete", "complex", True),
    ("SM_BN_RoadNet_Markings",  ["MI_LineYellow", "MI_LineWhite"], None, "none", False),
]

SURFACE_TO_MESH = {
    "asphalt": "SM_BN_RoadNet_Asphalt",
    "highway": "SM_BN_RoadNet_Highway",
    "cobble":  "SM_BN_RoadNet_Cobble",
    "dirt":    "SM_BN_RoadNet_Dirt",
}
WALL_MAT_IDX = {
    "MI_Concrete": 0, "MI_Sidewalk": 1, "MI_Cobblestone": 2,
    "MI_Metal_Dark": 3, "MI_HazardYellow": 4, "MI_CrateWood": 5,
}
SURFACE_PRIORITY = {"dirt": 0, "cobble": 1, "asphalt": 2, "highway": 3}

# (sidewalk_width, sidewalk_height, wall_mat, cap_mat, wall_height, is_jersey) in cm
_EDGE_STYLES = {
    knet.DIST_MERKEZ:  (250.0, 14.0, "MI_Sidewalk",    "MI_Metal_Dark",   95.0, False),
    knet.DIST_SANAYI:  (230.0, 14.0, "MI_Concrete",    "MI_HazardYellow", 100.0, False),
    knet.DIST_LIMAN:   (180.0, 12.0, "MI_Concrete",    "MI_HazardYellow", 95.0, False),
    knet.DIST_YAMAC:   (190.0, 14.0, "MI_Cobblestone", "MI_Metal_Dark",   95.0, False),
    knet.DIST_VIYADUK: ( 90.0,  6.0, "MI_Concrete",    "MI_HazardYellow", 94.0, True),
    knet.DIST_GECIT:   ( 90.0,  6.0, "MI_Concrete",    "MI_HazardYellow", 94.0, True),
}
_DIRT_STYLE = (80.0, 0.0, "MI_CrateWood", "MI_CrateWood", 85.0, False)


def _cm_to_m(p):
    return (p[0] * 0.01, p[1] * 0.01, p[2] * 0.01)


def _pchip_slopes(d, z, flat_start=False, flat_end=False):
    n = len(z)
    h = [d[i + 1] - d[i] for i in range(n - 1)]
    m = [(z[i + 1] - z[i]) / h[i] if h[i] > 1e-4 else 0.0 for i in range(n - 1)]
    t = [0.0] * n
    t[0] = 0.0 if flat_start else m[0]
    t[-1] = 0.0 if flat_end else m[-1]
    for i in range(1, n - 1):
        if m[i - 1] * m[i] > 0.0:
            w1, w2 = 2.0 * h[i] + h[i - 1], h[i] + 2.0 * h[i - 1]
            t[i] = (w1 + w2) / (w1 / m[i - 1] + w2 / m[i])
    return t


def densify_edge_points(points, max_seg=360.0, flat_start=False, flat_end=False):
    """Smooth ~2.4-3.6 m vertices (Catmull-Rom XY + Fritsch-Carlson monotone cubic Z).
    Junction endpoints enforce C1 flat vertical aprons (dz/ds = 0) so sloped ramps never
    create vertical bumper lips at intersections."""
    if len(points) < 2:
        return list(points)
    d = [0.0]
    for a, b in zip(points, points[1:]):
        d.append(d[-1] + math.hypot(b[0] - a[0], b[1] - a[1]))
    z = [p[2] for p in points]
    if len(points) > 2:
        t = _pchip_slopes(d, z, flat_start=flat_start, flat_end=flat_end)
    else:
        g = (z[1] - z[0]) / max(d[1], 1.0)
        t = [0.0 if flat_start else g, 0.0 if flat_end else g]
    out = [tuple(points[0])]
    for i in range(len(points) - 1):
        a, b = points[i], points[i + 1]
        h = d[i + 1] - d[i]
        n = max(1, int(math.ceil(h / max_seg)))
        p0, p3 = points[max(0, i - 1)], points[min(len(points) - 1, i + 2)]
        for k in range(1, n + 1):
            u = k / float(n)
            u2, u3 = u * u, u * u * u
            zz = ((2.0 * u3 - 3.0 * u2 + 1.0) * z[i] + (u3 - 2.0 * u2 + u) * h * t[i]
                  + (-2.0 * u3 + 3.0 * u2) * z[i + 1] + (u3 - u2) * h * t[i + 1])
            cx = 0.5 * (2.0 * a[0] + (-p0[0] + b[0]) * u
                        + (2.0 * p0[0] - 5.0 * a[0] + 4.0 * b[0] - p3[0]) * u2
                        + (-p0[0] + 3.0 * a[0] - 3.0 * b[0] + p3[0]) * u3)
            cy = 0.5 * (2.0 * a[1] + (-p0[1] + b[1]) * u
                        + (2.0 * p0[1] - 5.0 * a[1] + 4.0 * b[1] - p3[1]) * u2
                        + (-p0[1] + 3.0 * a[1] - 3.0 * b[1] + p3[1]) * u3)
            out.append((cx, cy, zz))
    # Smoothly damp any micro vertical jitter on interior points while keeping endpoints exact
    if len(out) >= 5:
        zs = [p[2] for p in out]
        for _ in range(3):
            nzs = list(zs)
            for idx_z in range(1, len(zs) - 1):
                nzs[idx_z] = 0.25 * zs[idx_z - 1] + 0.50 * zs[idx_z] + 0.25 * zs[idx_z + 1]
            zs = nzs
        out = [(out[k][0], out[k][1], zs[k]) for k in range(len(out))]
    return out


def _edge_surface_key(e):
    if e["dirt"]:
        return "dirt"
    is_carsi = (e["node_a"] in (8, 9, 10, 11) and e["node_b"] in (8, 9, 10, 11)
                and abs(e["points"][0][1] - 16000.0) < 200.0)
    is_old_yamac = (e["district"] == knet.DIST_YAMAC
                    and e["node_a"] in (32, 33, 34) and e["node_b"] in (32, 33, 34))
    if is_carsi or is_old_yamac:
        return "cobble"
    return "highway" if e["highway"] else "asphalt"


def _edge_style(e):
    if e["dirt"]:
        return _DIRT_STYLE
    if e["highway"]:
        return _EDGE_STYLES[knet.DIST_VIYADUK]
    return _EDGE_STYLES.get(e["district"], _EDGE_STYLES[knet.DIST_MERKEZ])


def _arc_lengths(pts):
    cum = [0.0]
    for a, b in zip(pts, pts[1:]):
        cum.append(cum[-1] + math.hypot(b[0] - a[0], b[1] - a[1]))
    return cum


def _interp_only(pts, cum, s):
    s = max(0.0, min(cum[-1], s))
    lo, hi = 0, len(cum) - 2
    while lo < hi:
        mid = (lo + hi) // 2
        if cum[mid + 1] < s:
            lo = mid + 1
        else:
            hi = mid
    seg = max(cum[lo + 1] - cum[lo], 1e-4)
    u = max(0.0, min(1.0, (s - cum[lo]) / seg))
    a, b = pts[lo], pts[lo + 1]
    return (a[0] + (b[0] - a[0]) * u, a[1] + (b[1] - a[1]) * u, a[2] + (b[2] - a[2]) * u)


def _sample_at_arc(pts, cum, s):
    total = cum[-1]
    if total < 1e-4 or len(pts) < 2:
        return pts[0], (1.0, 0.0)
    p = _interp_only(pts, cum, s)
    s0, s1 = max(0.0, s - 180.0), min(total, s + 180.0)
    p0, p1 = _interp_only(pts, cum, s0), _interp_only(pts, cum, s1)
    dx, dy = p1[0] - p0[0], p1[1] - p0[1]
    l = math.hypot(dx, dy)
    return p, ((dx / l, dy / l) if l > 1e-5 else (1.0, 0.0))


def _right_2d(tan):
    return (-tan[1], tan[0])


def _cross_2d(ax, ay, bx, by):
    return ax * by - ay * bx


def _slice_polyline(pts, cum, s_start, s_end):
    if s_end <= s_start + 10.0:
        p, _ = _sample_at_arc(pts, cum, 0.5 * (s_start + s_end))
        return [p, p]
    out = [_interp_only(pts, cum, s_start)]
    for k in range(len(pts)):
        if s_start + 35.0 < cum[k] < s_end - 35.0:
            out.append(pts[k])
    out.append(_interp_only(pts, cum, s_end))
    return out


def _intersect_lines_2d(p, d_p, q, d_q):
    denom = _cross_2d(d_p[0], d_p[1], d_q[0], d_q[1])
    if abs(denom) < 1e-4:
        return None
    rx, ry = q[0] - p[0], q[1] - p[1]
    t = _cross_2d(rx, ry, d_q[0], d_q[1]) / denom
    s = _cross_2d(rx, ry, d_p[0], d_p[1]) / denom
    return (p[0] + t * d_p[0], p[1] + t * d_p[1], t, s)


def _quad_bezier(p0, q, p2, n_steps):
    pts = []
    for k in range(n_steps + 1):
        u = k / float(n_steps)
        inv = 1.0 - u
        pts.append(tuple(inv * inv * p0[c] + 2.0 * inv * u * q[c] + u * u * p2[c] for c in range(3)))
    return pts


def _hermite_arc(p0, t0_xy, p1, t1_xy, n_steps):
    scale = math.hypot(p1[0] - p0[0], p1[1] - p0[1]) * 0.45
    pts = []
    for k in range(n_steps + 1):
        u = k / float(n_steps)
        u2, u3 = u * u, u * u * u
        h00, h10 = 2.0 * u3 - 3.0 * u2 + 1.0, u3 - 2.0 * u2 + u
        h01, h11 = -2.0 * u3 + 3.0 * u2, u3 - u2
        pts.append((
            h00 * p0[0] + h10 * t0_xy[0] * scale + h01 * p1[0] + h11 * t1_xy[0] * scale,
            h00 * p0[1] + h10 * t0_xy[1] * scale + h01 * p1[1] + h11 * t1_xy[1] * scale,
            p0[2] + (p1[2] - p0[2]) * u,
        ))
    return pts


def _gameplay_wall_openings():
    openings = [(c["loc"][0], c["loc"][1], c["radius"] + 750.0) for c in knet.COOLDOWN_SPOTS]
    openings.append((knet.SELIN_GARAGE_POS[0], knet.SELIN_GARAGE_POS[1], 3200.0))
    openings += [(j[1], j[2], 3200.0) for j in knet.STUNT_JUMPS]
    openings += [(r[0], r[1], 2200.0) for r in (knet.ESCAPE_ROUTE_A, knet.ESCAPE_ROUTE_B)]
    return openings


def _in_wall_opening(x, y, openings):
    if 34000.0 <= x <= 54000.0 and 46000.0 <= y <= 63500.0:
        return True
    return any(math.hypot(x - kx, y - ky) < kr for kx, ky, kr in openings)


def _build_other_road_index(dense_edges):
    segs = []
    for ed in dense_edges:
        pts, hw, idx = ed["pts"], ed["hw"], ed["index"]
        for a, b in zip(pts, pts[1:]):
            segs.append((
                idx, hw,
                min(a[0], b[0]) - hw - 1800.0, max(a[0], b[0]) + hw + 1800.0,
                min(a[1], b[1]) - hw - 1800.0, max(a[1], b[1]) + hw + 1800.0,
                a[0], a[1], a[2], b[0], b[1], b[2],
            ))
    return segs


def _hits_other_road(segs, own_idx, x, y, z, pad_cm, z_tol_cm=240.0, only_lower=False):
    for eidx, hw, minx, maxx, miny, maxy, ax, ay, az, bx, by, bz in segs:
        if eidx == own_idx or x < minx or x > maxx or y < miny or y > maxy:
            continue
        vx, vy = bx - ax, by - ay
        l2 = vx * vx + vy * vy
        t = 0.0 if l2 < 1.0 else max(0.0, min(1.0, ((x - ax) * vx + (y - ay) * vy) / l2))
        qx, qy, qz = ax + vx * t, ay + vy * t, az + (bz - az) * t
        if only_lower:
            if qz >= z - 60.0:
                continue
        elif abs(qz - z) > z_tol_cm:
            continue
        if math.hypot(x - qx, y - qy) < hw + pad_cm:
            return True
    return False


def _emit_quad_cm(builder, mat_idx, p0, p1, p2, p3):
    builder.add_quad(mat_idx, _cm_to_m(p0), _cm_to_m(p1), _cm_to_m(p2), _cm_to_m(p3))


def _emit_tri_cm(builder, mat_idx, p0, p1, p2):
    builder.add_tri(mat_idx, _cm_to_m(p0), _cm_to_m(p1), _cm_to_m(p2))


def _emit_side_profile_segment(builders, a_edge, b_edge, out_a, out_b, style,
                               emit_wall, is_dirt, is_bridge, drop_skirt, sw_w_a=None, sw_w_b=None):
    """Welded continuous flush curb + sidewalk/shoulder + boundary wall/NJ barrier + retaining skirt.
    Guarantees ZERO horizontal or vertical trench between road edge and boundary wall."""
    sw_w_def, sw_h, wall_mat_name, cap_mat_name, wall_h, is_jersey = style
    wa = sw_w_def if sw_w_a is None else sw_w_a
    wb = sw_w_def if sw_w_b is None else sw_w_b
    sw_bldr = builders["SM_BN_RoadNet_Sidewalk"]
    wall_bldr = builders["SM_BN_RoadNet_Wall"]
    sw_slot = 2 if is_dirt else (1 if is_jersey else 0)
    w_slot = WALL_MAT_IDX[wall_mat_name]
    c_slot = WALL_MAT_IDX[cap_mat_name]

    def pt(base, nrm, dist, dz):
        return (base[0] + nrm[0] * dist, base[1] + nrm[1] * dist, base[2] + dz)

    dx, dy = b_edge[0] - a_edge[0], b_edge[1] - a_edge[1]
    is_right_side = (dx * out_a[1] - dy * out_a[0]) >= 0.0

    def emit_strip(bldr, slot, pa0, pa1, pb0, pb1):
        if is_right_side:
            _emit_quad_cm(bldr, slot, pa0, pb0, pb1, pa1)
        else:
            _emit_quad_cm(bldr, slot, pa1, pb1, pb0, pa0)

    kerb_w = 24.0 if sw_h > 2.0 else 10.0
    pa_e0, pb_e0 = pt(a_edge, out_a, 0.0, 0.0), pt(b_edge, out_b, 0.0, 0.0)
    pa_k1, pb_k1 = pt(a_edge, out_a, kerb_w, sw_h), pt(b_edge, out_b, kerb_w, sw_h)
    pa_sw, pb_sw = pt(a_edge, out_a, wa, sw_h), pt(b_edge, out_b, wb, sw_h)
    emit_strip(sw_bldr, sw_slot, pa_e0, pa_k1, pb_e0, pb_k1)
    emit_strip(sw_bldr, sw_slot, pa_k1, pa_sw, pb_k1, pb_sw)

    skirt_z = -750.0 if drop_skirt else -90.0
    emit_strip(sw_bldr, 1, pa_sw, pt(a_edge, out_a, wa + 40.0, skirt_z),
               pb_sw, pt(b_edge, out_b, wb + 40.0, skirt_z))

    if not emit_wall or is_bridge:
        return

    if is_jersey:
        prof = [
            (-18.0, sw_h - 4.0), (-12.0, sw_h + 24.0), (4.0, sw_h + wall_h),
            (24.0, sw_h + wall_h), (42.0, sw_h + 20.0), (46.0, sw_h - 12.0),
        ]
        for k in range(len(prof) - 1):
            d0, z0 = prof[k]
            d1, z1 = prof[k + 1]
            emit_strip(wall_bldr, w_slot,
                       pt(a_edge, out_a, wa + d0, z0), pt(a_edge, out_a, wa + d1, z1),
                       pt(b_edge, out_b, wb + d0, z0), pt(b_edge, out_b, wb + d1, z1))
        emit_strip(wall_bldr, c_slot,
                   pt(a_edge, out_a, wa + 5.0, sw_h + wall_h + 2.5),
                   pt(a_edge, out_a, wa + 23.0, sw_h + wall_h + 2.5),
                   pt(b_edge, out_b, wb + 5.0, sw_h + wall_h + 2.5),
                   pt(b_edge, out_b, wb + 23.0, sw_h + wall_h + 2.5))
    else:
        z_base, z_top = sw_h - 10.0, sw_h + wall_h
        prof = [(-18.0, z_base), (-18.0, z_top), (22.0, z_top), (22.0, z_base - 30.0)]
        for k in range(len(prof) - 1):
            d0, z0 = prof[k]
            d1, z1 = prof[k + 1]
            emit_strip(wall_bldr, w_slot,
                       pt(a_edge, out_a, wa + d0, z0), pt(a_edge, out_a, wa + d1, z1),
                       pt(b_edge, out_b, wb + d0, z0), pt(b_edge, out_b, wb + d1, z1))
        emit_strip(wall_bldr, c_slot,
                   pt(a_edge, out_a, wa - 20.0, z_top + 2.5),
                   pt(a_edge, out_a, wa + 24.0, z_top + 2.5),
                   pt(b_edge, out_b, wb - 20.0, z_top + 2.5),
                   pt(b_edge, out_b, wb + 24.0, z_top + 2.5))


def _compute_junction_trims(nodes, dense_edges):
    inc_by_node = {nid: [] for nid in nodes}
    for ed in dense_edges:
        pts, cum = ed["pts"], ed["cum"]
        _, tan_a = _sample_at_arc(pts, cum, min(400.0, cum[-1] * 0.25))
        inc_by_node[ed["node_a"]].append(dict(
            edge=ed, at_end=False, theta=math.atan2(tan_a[1], tan_a[0]), tan=tan_a,
        ))
        _, tan_b_fwd = _sample_at_arc(pts, cum, max(0.0, cum[-1] - min(400.0, cum[-1] * 0.25)))
        tan_b = (-tan_b_fwd[0], -tan_b_fwd[1])
        inc_by_node[ed["node_b"]].append(dict(
            edge=ed, at_end=True, theta=math.atan2(tan_b[1], tan_b[0]), tan=tan_b,
        ))

    trims = {}
    smooth_deg2 = {}
    for nid, inc in inc_by_node.items():
        inc.sort(key=lambda item: item["theta"])
        k = len(inc)
        if k == 0:
            continue
        if k == 1:
            trims[(inc[0]["edge"]["index"], inc[0]["at_end"])] = 0.0
            continue
        if k == 2:
            e0, e1 = inc[0], inc[1]
            dot = e0["tan"][0] * e1["tan"][0] + e0["tan"][1] * e1["tan"][1]
            # Only weld directly as pass-through when nearly collinear (<22 deg bend) so sharp hairpins never pinch!
            if dot < -0.92 and e0["edge"]["surface"] == e1["edge"]["surface"]:
                trims[(e0["edge"]["index"], e0["at_end"])] = 0.0
                trims[(e1["edge"]["index"], e1["at_end"])] = 0.0
                tx, ty = e1["tan"][0] - e0["tan"][0], e1["tan"][1] - e0["tan"][1]
                tl = max(math.hypot(tx, ty), 1e-5)
                smooth_deg2[nid] = ((tx / tl, ty / tl), 0.5 * (e0["edge"]["hw"] + e1["edge"]["hw"]))
                continue

        req_trim = [0.0] * k
        nx, ny = nodes[nid]["pos"][0], nodes[nid]["pos"][1]

        def edge_side_pt(item, dist_from_node, side_sign):
            ed = item["edge"]
            s = (ed["total_len"] - dist_from_node) if item["at_end"] else dist_from_node
            p, t_fwd = _sample_at_arc(ed["pts"], ed["cum"], s)
            t_out = (-t_fwd[0], -t_fwd[1]) if item["at_end"] else t_fwd
            r = _right_2d(t_out)
            return (p[0] + r[0] * ed["hw"] * side_sign, p[1] + r[1] * ed["hw"] * side_sign)

        for i in range(k):
            j = (i + 1) % k
            ei, ej = inc[i], inc[j]
            wi, wj = ei["edge"]["hw"], ej["edge"]["hw"]
            alpha = (ej["theta"] - ei["theta"]) % (2.0 * math.pi)
            if 0.25 < alpha < math.pi - 0.15:
                sa, ca = math.sin(alpha), math.cos(alpha)
                ti_est = max(wj * 0.7, (wi * ca + wj) / sa)
                sj_est = max(wi * 0.7, (wj * ca + wi) / sa)
            elif alpha <= 0.25:
                ti_est = sj_est = (wi + wj) * 1.4
            else:
                ti_est = sj_est = max(wi, wj) * 0.75 + 180.0

            fillet_pad = 240.0 if alpha < math.pi - 0.2 else 140.0
            max_i, max_j = ei["edge"]["total_len"] * 0.36, ej["edge"]["total_len"] * 0.36
            ti = min(max(ti_est + fillet_pad, wi * 0.65 + 180.0), max_i)
            sj = min(max(sj_est + fillet_pad, wj * 0.65 + 180.0), max_j)

            if alpha < math.pi * 0.85:
                for _ in range(12):
                    ri, lj = edge_side_pt(ei, ti, +1.0), edge_side_pt(ej, sj, -1.0)
                    cp = _cross_2d(ri[0] - nx, ri[1] - ny, lj[0] - nx, lj[1] - ny)
                    if cp > 0.0 and math.hypot(ri[0] - lj[0], ri[1] - lj[1]) >= 140.0:
                        break
                    ti = min(ti + 260.0, max_i)
                    sj = min(sj + 260.0, max_j)

            req_trim[i] = max(req_trim[i], ti)
            req_trim[j] = max(req_trim[j], sj)

        for i, item in enumerate(inc):
            trims[(item["edge"]["index"], item["at_end"])] = req_trim[i]

    return inc_by_node, trims, smooth_deg2


def _emit_crosswalk(b_mark, arm):
    """Crisp zebra crosswalk & approach stop bar aligned to the actual trimmed road arm."""
    ed = arm["edge"]
    ci = arm["C"]
    to_x, to_y = arm["tan_out"]
    ro_x, ro_y = arm["right_out"]
    hw_i = ed["hw"]
    # Sample exact road Z at crosswalk offsets so even sloped approaches stay flush
    s_base = arm["s_trim"]
    sign = -1.0 if arm["at_end"] else 1.0
    z0 = _interp_only(ed["pts"], ed["cum"], s_base + sign * 70.0)[2] + 2.2
    z1 = _interp_only(ed["pts"], ed["cum"], s_base + sign * 340.0)[2] + 2.2

    n_bars = max(4, int((hw_i * 2.0 - 240.0) // 175.0))
    span_w = hw_i * 2.0 - 220.0
    bar_step = span_w / float(n_bars)
    for bk in range(n_bars):
        lat0 = -span_w * 0.5 + bk * bar_step + 18.0
        lat1 = lat0 + bar_step * 0.52
        p0 = (ci[0] + to_x * 70.0 + ro_x * lat0, ci[1] + to_y * 70.0 + ro_y * lat0, z0)
        p1 = (ci[0] + to_x * 340.0 + ro_x * lat0, ci[1] + to_y * 340.0 + ro_y * lat0, z1)
        p2 = (ci[0] + to_x * 340.0 + ro_x * lat1, ci[1] + to_y * 340.0 + ro_y * lat1, z1)
        p3 = (ci[0] + to_x * 70.0 + ro_x * lat1, ci[1] + to_y * 70.0 + ro_y * lat1, z0)
        _emit_quad_cm(b_mark, 1, p0, p1, p2, p3)

    zs0 = _interp_only(ed["pts"], ed["cum"], s_base + sign * 410.0)[2] + 2.2
    zs1 = _interp_only(ed["pts"], ed["cum"], s_base + sign * 465.0)[2] + 2.2
    sl0, sl1 = 40.0, hw_i - 80.0
    q0 = (ci[0] + to_x * 410.0 + ro_x * sl0, ci[1] + to_y * 410.0 + ro_y * sl0, zs0)
    q1 = (ci[0] + to_x * 465.0 + ro_x * sl0, ci[1] + to_y * 465.0 + ro_y * sl0, zs1)
    q2 = (ci[0] + to_x * 465.0 + ro_x * sl1, ci[1] + to_y * 465.0 + ro_y * sl1, zs1)
    q3 = (ci[0] + to_x * 410.0 + ro_x * sl1, ci[1] + to_y * 410.0 + ro_y * sl1, zs0)
    _emit_quad_cm(b_mark, 1, q0, q1, q2, q3)


def build_road_meshes(nodes, edges):
    """Generates all 8 seamless single-piece road network .glb builders and returns (builders, stats)."""
    builders = {name: bn_meshes.GltfBuilder(mats) for name, mats, _, _, _ in ROAD_MESH_MANIFEST}

    node_deg = {nid: 0 for nid in nodes}
    for e in edges:
        node_deg[e["node_a"]] += 1
        node_deg[e["node_b"]] += 1

    dense_edges = []
    for e in edges:
        zs = [p[2] for p in e["points"]]
        is_curved = len(e["points"]) > 3
        is_sloped = (max(zs) - min(zs) > 40.0)
        seg_cm = 300.0 if (is_sloped or is_curved) else 500.0
        pts = densify_edge_points(
            e["points"], max_seg=seg_cm,
            flat_start=(node_deg[e["node_a"]] >= 3),
            flat_end=(node_deg[e["node_b"]] >= 3),
        )
        cum = _arc_lengths(pts)
        dense_edges.append(dict(
            edge=e, index=e["index"], node_a=e["node_a"], node_b=e["node_b"],
            pts=pts, cum=cum, total_len=cum[-1], hw=e["width"] * 0.5, width=e["width"],
            district=e["district"], highway=e["highway"], dirt=e["dirt"],
            surface=_edge_surface_key(e), style=_edge_style(e),
            is_bridge=(e["node_a"], e["node_b"]) in ((20, 21), (21, 20)),
        ))

    inc_by_node, trims, smooth_deg2 = _compute_junction_trims(nodes, dense_edges)

    # Flatten every trimmed junction apron to its exact node Z and re-fit C1 monotone Z along the
    # untrimmed road span [s_a, s_b]. Eliminates 100% of vertical junction-corner steps & wheel snags!
    for ed in dense_edges:
        eidx = ed["index"]
        s_a = trims.get((eidx, False), 0.0)
        s_b = ed["total_len"] - trims.get((eidx, True), 0.0)
        if s_b <= s_a + 100.0:
            continue
        has_junc_a = (s_a > 1.0) and (ed["node_a"] not in smooth_deg2)
        has_junc_b = (trims.get((eidx, True), 0.0) > 1.0) and (ed["node_b"] not in smooth_deg2)
        if not has_junc_a and not has_junc_b:
            continue
        za = nodes[ed["node_a"]]["pos"][2]
        zb = nodes[ed["node_b"]]["pos"][2]
        pts = ed["pts"]
        cum = ed["cum"]
        # Build monotone PCHIP control knots on [s_a, s_b] using original spline heights well inside the span
        ctrl_s = [s_a]
        ctrl_z = [za]
        # Keep interior height samples from original curve spaced away from junction aprons
        pad_a = min(1400.0, (s_b - s_a) * 0.22)
        pad_b = min(1400.0, (s_b - s_a) * 0.22)
        raw_pts = ed["edge"]["points"]
        raw_cum = _arc_lengths(raw_pts)
        raw_scale = ed["total_len"] / max(raw_cum[-1], 1.0)
        for rp, rc in zip(raw_pts[1:-1], raw_cum[1:-1]):
            rs = rc * raw_scale
            if s_a + pad_a < rs < s_b - pad_b and rs > ctrl_s[-1] + 500.0:
                ctrl_s.append(rs)
                ctrl_z.append(rp[2])
        ctrl_s.append(s_b)
        ctrl_z.append(zb)
        if len(ctrl_s) > 2:
            slopes = _pchip_slopes(ctrl_s, ctrl_z, flat_start=has_junc_a, flat_end=has_junc_b)
        else:
            g = (zb - za) / max(s_b - s_a, 1.0)
            slopes = [0.0 if has_junc_a else g, 0.0 if has_junc_b else g]
        # Cap endpoint tangent scaling so short ramps stay below ~13.5 deg max slope
        for ki in range(len(ctrl_s) - 1):
            seg_h = max(ctrl_s[ki + 1] - ctrl_s[ki], 1.0)
            avg_g = (ctrl_z[ki + 1] - ctrl_z[ki]) / seg_h
            # If average slope exceeds ~8 deg, ease junction apron tangent to avoid mid-span >14 deg overshoot
            if abs(avg_g) > 0.140:
                if ki == 0 and has_junc_a:
                    slopes[0] = avg_g * 0.65
                if ki == len(ctrl_s) - 2 and has_junc_b:
                    slopes[-1] = avg_g * 0.65
            elif abs(avg_g) > 0.095:
                if ki == 0 and has_junc_a:
                    slopes[0] = avg_g * 0.45
                if ki == len(ctrl_s) - 2 and has_junc_b:
                    slopes[-1] = avg_g * 0.45

        new_pts = []
        for p, s in zip(pts, cum):
            if s <= s_a:
                zz = za
            elif s >= s_b:
                zz = zb
            else:
                # Locate control interval
                ci = 0
                while ci + 1 < len(ctrl_s) - 1 and ctrl_s[ci + 1] < s:
                    ci += 1
                h = max(ctrl_s[ci + 1] - ctrl_s[ci], 1e-4)
                u = max(0.0, min(1.0, (s - ctrl_s[ci]) / h))
                u2, u3 = u * u, u * u * u
                zz = ((2.0 * u3 - 3.0 * u2 + 1.0) * ctrl_z[ci] + (u3 - 2.0 * u2 + u) * h * slopes[ci]
                      + (-2.0 * u3 + 3.0 * u2) * ctrl_z[ci + 1] + (u3 - u2) * h * slopes[ci + 1])
            new_pts.append((p[0], p[1], zz))
        ed["pts"] = new_pts

    other_segs = _build_other_road_index(dense_edges)
    openings = _gameplay_wall_openings()

    junction_arms = {nid: {} for nid in nodes}
    b_mark = builders["SM_BN_RoadNet_Markings"]
    b_struct = builders["SM_BN_RoadNet_Structure"]

    # 1. Trimmed road ribbons, lane markings, continuous curbs/walls & viaduct undersides
    for ed in dense_edges:
        eidx = ed["index"]
        s_a = trims.get((eidx, False), 0.0)
        s_b = ed["total_len"] - trims.get((eidx, True), 0.0)
        if s_b <= s_a + 40.0:
            mid = 0.5 * ed["total_len"]
            s_a, s_b = max(0.0, mid - 20.0), min(ed["total_len"], mid + 20.0)

        sub_pts = _slice_polyline(ed["pts"], ed["cum"], s_a, s_b)
        sub_cum = _arc_lengths(sub_pts)
        n_pts, hw = len(sub_pts), ed["hw"]
        surf_builder = builders[SURFACE_TO_MESH[ed["surface"]]]

        rings = []
        for i, p in enumerate(sub_pts):
            if i == 0:
                dx, dy = sub_pts[1][0] - p[0], sub_pts[1][1] - p[1]
            elif i == n_pts - 1:
                dx, dy = p[0] - sub_pts[i - 1][0], p[1] - sub_pts[i - 1][1]
            else:
                dx, dy = sub_pts[i + 1][0] - sub_pts[i - 1][0], sub_pts[i + 1][1] - sub_pts[i - 1][1]
            l = math.hypot(dx, dy)
            tan = (dx / l, dy / l) if l > 1e-5 else (1.0, 0.0)
            local_hw = hw

            if i == 0 and ed["node_a"] in smooth_deg2:
                sh_tan, local_hw = smooth_deg2[ed["node_a"]]
                tan = (-sh_tan[0], -sh_tan[1]) if (sh_tan[0] * tan[0] + sh_tan[1] * tan[1] < 0.0) else sh_tan
            elif i == n_pts - 1 and ed["node_b"] in smooth_deg2:
                sh_tan, local_hw = smooth_deg2[ed["node_b"]]
                tan = (-sh_tan[0], -sh_tan[1]) if (sh_tan[0] * tan[0] + sh_tan[1] * tan[1] < 0.0) else sh_tan

            r = _right_2d(tan)
            p_l = (p[0] - r[0] * local_hw, p[1] - r[1] * local_hw, p[2])
            p_r = (p[0] + r[0] * local_hw, p[1] + r[1] * local_hw, p[2])
            # Prevent inner-curve vertex fold-over AND vertical cliff steps on tight sloped ramps!
            if rings:
                prev_l, prev_c, prev_r, prev_t, _prev_right, _prev_hw = rings[-1]
                adv_l = (p_l[0] - prev_l[0]) * prev_t[0] + (p_l[1] - prev_l[1]) * prev_t[1]
                adv_r = (p_r[0] - prev_r[0]) * prev_t[0] + (p_r[1] - prev_r[1]) * prev_t[1]
                c_step = max(math.hypot(p[0] - prev_c[0], p[1] - prev_c[1]), 1.0)
                max_dz_l = max(12.0, adv_l) * 0.16
                max_dz_r = max(12.0, adv_r) * 0.16
                if adv_l < 45.0:
                    step_l = max(45.0, c_step * 0.35)
                    z_l = prev_l[2] + max(-max_dz_l, min(max_dz_l, p[2] - prev_l[2]))
                    p_l = (prev_l[0] + prev_t[0] * step_l, prev_l[1] + prev_t[1] * step_l, z_l)
                if adv_r < 45.0:
                    step_r = max(45.0, c_step * 0.35)
                    z_r = prev_r[2] + max(-max_dz_r, min(max_dz_r, p[2] - prev_r[2]))
                    p_r = (prev_r[0] + prev_t[0] * step_r, prev_r[1] + prev_t[1] * step_r, z_r)
            rings.append((p_l, p, p_r, tan, r, local_hw))

        r0, rn = rings[0], rings[-1]
        junction_arms[ed["node_a"]][(eidx, False)] = dict(
            edge=ed, at_end=False, s_trim=s_a, L=r0[0], C=r0[1], R=r0[2], tan_out=r0[3], right_out=r0[4],
        )
        junction_arms[ed["node_b"]][(eidx, True)] = dict(
            edge=ed, at_end=True, s_trim=s_b, L=rn[2], C=rn[1], R=rn[0],
            tan_out=(-rn[3][0], -rn[3][1]), right_out=(-rn[4][0], -rn[4][1]),
        )

        pier_accum = 2200.0
        for i in range(n_pts - 1):
            l0, c0, r0_p, _t0, right0, hw0 = rings[i]
            l1, c1, r1_p, _t1, right1, hw1 = rings[i + 1]
            _emit_quad_cm(surf_builder, 0, l0, l1, c1, c0)
            _emit_quad_cm(surf_builder, 0, c0, c1, r1_p, r0_p)

            if not ed["dirt"]:
                s0, s1 = sub_cum[i], sub_cum[i + 1]
                for side in (-1.0, 1.0):
                    eo0, eo1, wl = (hw0 - 52.0) * side, (hw1 - 52.0) * side, 7.5
                    _emit_quad_cm(
                        b_mark, 1,
                        (c0[0] + right0[0] * (eo0 - wl), c0[1] + right0[1] * (eo0 - wl), c0[2] + 1.8),
                        (c1[0] + right1[0] * (eo1 - wl), c1[1] + right1[1] * (eo1 - wl), c1[2] + 1.8),
                        (c1[0] + right1[0] * (eo1 + wl), c1[1] + right1[1] * (eo1 + wl), c1[2] + 1.8),
                        (c0[0] + right0[0] * (eo0 + wl), c0[1] + right0[1] * (eo0 + wl), c0[2] + 1.8),
                    )
                mid_s = 0.5 * (s0 + s1)
                if (mid_s % 720.0) < 340.0 and 200.0 < mid_s < sub_cum[-1] - 200.0:
                    wc = 8.5
                    _emit_quad_cm(
                        b_mark, 0,
                        (c0[0] - right0[0] * wc, c0[1] - right0[1] * wc, c0[2] + 2.0),
                        (c1[0] - right1[0] * wc, c1[1] - right1[1] * wc, c1[2] + 2.0),
                        (c1[0] + right1[0] * wc, c1[1] + right1[1] * wc, c1[2] + 2.0),
                        (c0[0] + right0[0] * wc, c0[1] + right0[1] * wc, c0[2] + 2.0),
                    )
                    if ed["width"] >= 1800.0:
                        for side in (-1.0, 1.0):
                            lo0, lo1, wl = hw0 * 0.5 * side, hw1 * 0.5 * side, 6.5
                            _emit_quad_cm(
                                b_mark, 1,
                                (c0[0] + right0[0] * (lo0 - wl), c0[1] + right0[1] * (lo0 - wl), c0[2] + 1.9),
                                (c1[0] + right1[0] * (lo1 - wl), c1[1] + right1[1] * (lo1 - wl), c1[2] + 1.9),
                                (c1[0] + right1[0] * (lo1 + wl), c1[1] + right1[1] * (lo1 + wl), c1[2] + 1.9),
                                (c0[0] + right0[0] * (lo0 + wl), c0[1] + right0[1] * (lo0 + wl), c0[2] + 1.9),
                            )

            mx, my, mz = 0.5 * (c0[0] + c1[0]), 0.5 * (c0[1] + c1[1]), 0.5 * (c0[2] + c1[2])
            sw_w = ed["style"][0]
            over_lower = mz > 120.0 and _hits_other_road(other_segs, eidx, mx, my, mz, pad_cm=hw + 260.0, only_lower=True)
            drop_skirt = (mz > 25.0 and not over_lower and not ed["is_bridge"])

            for ea, eb, na, nb in (
                (l0, l1, (-right0[0], -right0[1]), (-right1[0], -right1[1])),
                (r0_p, r1_p, right0, right1),
            ):
                tx = 0.5 * (ea[0] + eb[0]) + 0.5 * (na[0] + nb[0]) * (sw_w * 0.65)
                ty = 0.5 * (ea[1] + eb[1]) + 0.5 * (na[1] + nb[1]) * (sw_w * 0.65)
                if _hits_other_road(other_segs, eidx, tx, ty, mz, pad_cm=45.0, z_tol_cm=220.0):
                    continue
                _emit_side_profile_segment(
                    builders, ea, eb, na, nb, ed["style"],
                    emit_wall=not _in_wall_opening(tx, ty, openings),
                    is_dirt=ed["dirt"], is_bridge=ed["is_bridge"], drop_skirt=drop_skirt,
                )

            if mz > 120.0 and not ed["is_bridge"]:
                seg_len = sub_cum[i + 1] - sub_cum[i]
                if ed["highway"] or over_lower:
                    gw0, gw1, gd = hw0 * 0.86, hw1 * 0.86, 145.0
                    g_a = [
                        (c0[0] - right0[0] * hw0, c0[1] - right0[1] * hw0, c0[2] - 8.0),
                        (c0[0] - right0[0] * gw0, c0[1] - right0[1] * gw0, c0[2] - gd),
                        (c0[0] + right0[0] * gw0, c0[1] + right0[1] * gw0, c0[2] - gd),
                        (c0[0] + right0[0] * hw0, c0[1] + right0[1] * hw0, c0[2] - 8.0),
                    ]
                    g_b = [
                        (c1[0] - right1[0] * hw1, c1[1] - right1[1] * hw1, c1[2] - 8.0),
                        (c1[0] - right1[0] * gw1, c1[1] - right1[1] * gw1, c1[2] - gd),
                        (c1[0] + right1[0] * gw1, c1[1] + right1[1] * gw1, c1[2] - gd),
                        (c1[0] + right1[0] * hw1, c1[1] + right1[1] * hw1, c1[2] - 8.0),
                    ]
                    for k in range(3):
                        _emit_quad_cm(b_struct, 0, g_a[k + 1], g_b[k + 1], g_b[k], g_a[k])
                    pier_accum += seg_len
                    if pier_accum >= 4500.0 and not over_lower:
                        pier_accum = 0.0
                        b_struct.add_frustum_cyl(
                            0, (mx * 0.01, my * 0.01, -2.5), 1.35, 1.15, (mz - 130.0) * 0.01 + 2.5, segments=10,
                        )
                else:
                    s_slot = 1 if ed["district"] == knet.DIST_GECIT else 0
                    ew0, ew1 = hw0 + sw_w + 35.0, hw1 + sw_w + 35.0
                    bz0, bz1 = min(c0[2] - 800.0, -250.0), min(c1[2] - 800.0, -250.0)
                    _emit_quad_cm(
                        b_struct, s_slot,
                        (c0[0] - right0[0] * (ew0 + 260.0), c0[1] - right0[1] * (ew0 + 260.0), bz0),
                        (c1[0] - right1[0] * (ew1 + 260.0), c1[1] - right1[1] * (ew1 + 260.0), bz1),
                        (c1[0] - right1[0] * ew1, c1[1] - right1[1] * ew1, c1[2] - 20.0),
                        (c0[0] - right0[0] * ew0, c0[1] - right0[1] * ew0, c0[2] - 20.0),
                    )
                    _emit_quad_cm(
                        b_struct, s_slot,
                        (c0[0] + right0[0] * ew0, c0[1] + right0[1] * ew0, c0[2] - 20.0),
                        (c1[0] + right1[0] * ew1, c1[1] + right1[1] * ew1, c1[2] - 20.0),
                        (c1[0] + right1[0] * (ew1 + 260.0), c1[1] + right1[1] * (ew1 + 260.0), bz1),
                        (c0[0] + right0[0] * (ew0 + 260.0), c0[1] + right0[1] * (ew0 + 260.0), bz0),
                    )

    # 2. Welded Junction Polygons + Curved Corner Curb Fillets & Signalized Crosswalks
    junction_count = 0
    for nid, inc in inc_by_node.items():
        if len(inc) < 2 or nid in smooth_deg2:
            continue
        arms = [junction_arms[nid][(item["edge"]["index"], item["at_end"])] for item in inc]
        k = len(arms)
        dom_surf = max((a["edge"]["surface"] for a in arms), key=lambda s: SURFACE_PRIORITY[s])
        j_builder = builders[SURFACE_TO_MESH[dom_surf]]

        nx, ny, nz = nodes[nid]["pos"]
        avg_arm_z = sum(a["C"][2] for a in arms) / float(k)
        center = (nx, ny, 0.55 * nz + 0.45 * avg_arm_z)

        perim = []
        corner_arcs = []
        for i in range(k):
            arm_i, arm_j = arms[i], arms[(i + 1) % k]
            perim.extend([arm_i["L"], arm_i["C"], arm_i["R"]])

            ri, lj = arm_i["R"], arm_j["L"]
            ti_in = (-arm_i["tan_out"][0], -arm_i["tan_out"][1])
            tj_out = arm_j["tan_out"]
            sep = math.hypot(lj[0] - ri[0], lj[1] - ri[1])
            n_sub = 5 if sep > 250.0 else 3

            hit = _intersect_lines_2d(ri, ti_in, lj, (-tj_out[0], -tj_out[1]))
            mid_xy = (0.5 * (ri[0] + lj[0]), 0.5 * (ri[1] + lj[1]))
            if (hit is not None and hit[2] > 20.0 and hit[3] > 20.0
                    and math.hypot(hit[0] - mid_xy[0], hit[1] - mid_xy[1]) < max(sep * 1.15, 650.0)):
                arc = _quad_bezier(ri, (hit[0], hit[1], 0.5 * (ri[2] + lj[2])), lj, n_sub)
            else:
                arc = _hermite_arc(ri, ti_in, lj, tj_out, n_sub)

            perim.extend(arc[1:-1])
            n_start = arm_i["right_out"]
            n_end = (-arm_j["right_out"][0], -arm_j["right_out"][1])
            corner_arcs.append((
                arc, n_start, n_end, arm_i["edge"]["style"], arm_j["edge"]["style"][0],
                arm_i["edge"]["dirt"] and arm_j["edge"]["dirt"],
                arm_i["edge"]["is_bridge"] or arm_j["edge"]["is_bridge"],
                center[2] > 25.0 and not (arm_i["edge"]["is_bridge"] or arm_j["edge"]["is_bridge"]),
            ))

            if nodes[nid]["signalized"] and not arm_i["edge"]["dirt"] and not arm_i["edge"]["highway"]:
                _emit_crosswalk(b_mark, arm_i)

        m = len(perim)
        for idx_p in range(m):
            p0, p1 = perim[idx_p], perim[(idx_p + 1) % m]
            if math.hypot(p1[0] - p0[0], p1[1] - p0[1]) < 1.0:
                continue
            area2 = _cross_2d(p0[0] - center[0], p0[1] - center[1], p1[0] - center[0], p1[1] - center[1])
            if area2 > 1.0:
                _emit_tri_cm(j_builder, 0, center, p0, p1)
            elif area2 < -1.0:
                _emit_tri_cm(j_builder, 0, center, p1, p0)

        for arc, n_start, n_end, c_style, sw_end, c_dirt, c_bridge, c_drop in corner_arcs:
            if len(arc) < 2:
                continue
            arc_nrms = []
            for ai in range(len(arc)):
                if ai == 0:
                    arc_nrms.append(n_start)
                elif ai == len(arc) - 1:
                    arc_nrms.append(n_end)
                else:
                    tx, ty = arc[ai + 1][0] - arc[ai - 1][0], arc[ai + 1][1] - arc[ai - 1][1]
                    tl = math.hypot(tx, ty)
                    if tl < 1e-4:
                        ox, oy = arc[ai][0] - center[0], arc[ai][1] - center[1]
                        ol = max(math.hypot(ox, oy), 1e-4)
                        arc_nrms.append((ox / ol, oy / ol))
                    else:
                        nx_o, ny_o = ty / tl, -tx / tl
                        if nx_o * (arc[ai][0] - center[0]) + ny_o * (arc[ai][1] - center[1]) < 0.0:
                            nx_o, ny_o = -nx_o, -ny_o
                        arc_nrms.append((nx_o, ny_o))

            sw_start = c_style[0]
            steps = float(len(arc) - 1)
            for ai in range(len(arc) - 1):
                pa, pb = arc[ai], arc[ai + 1]
                if math.hypot(pb[0] - pa[0], pb[1] - pa[1]) < 5.0:
                    continue
                wa = sw_start + (sw_end - sw_start) * (ai / steps)
                wb = sw_start + (sw_end - sw_start) * ((ai + 1) / steps)
                mx, my = 0.5 * (pa[0] + pb[0]), 0.5 * (pa[1] + pb[1])
                _emit_side_profile_segment(
                    builders, pa, pb, arc_nrms[ai], arc_nrms[ai + 1], c_style,
                    emit_wall=not _in_wall_opening(mx, my, openings),
                    is_dirt=c_dirt, is_bridge=c_bridge, drop_skirt=c_drop,
                    sw_w_a=wa, sw_w_b=wb,
                )
        junction_count += 1

    tri_counts = {}
    inverted_road_tris = 0
    drivable_names = {"SM_BN_RoadNet_Asphalt", "SM_BN_RoadNet_Highway", "SM_BN_RoadNet_Cobble", "SM_BN_RoadNet_Dirt"}
    for name, bldr in builders.items():
        tri_counts[name] = sum(len(s["idx"]) // 3 for s in bldr.slots.values())
        if name in drivable_names:
            for s in bldr.slots.values():
                for nrm in s["nrm"]:
                    if nrm[1] < 0.65:
                        inverted_road_tris += 1

    return builders, dict(
        junctions=junction_count,
        smooth_deg2=len(smooth_deg2),
        tri_counts=tri_counts,
        total_tris=sum(tri_counts.values()),
        inverted_road_tris=inverted_road_tris,
    )


def generate_road_glbs(force=True):
    nodes, edges, _ = knet.build_network()
    builders, stats = build_road_meshes(nodes, edges)
    if stats["inverted_road_tris"] > 0:
        raise RuntimeError("Road mesh check failed: %d inverted/steep road vertices" % stats["inverted_road_tris"])
    os.makedirs(OUT_DIR, exist_ok=True)
    for name, _, _, _, _ in ROAD_MESH_MANIFEST:
        out_path = os.path.join(OUT_DIR, name + ".glb")
        if force or not os.path.isfile(out_path):
            builders[name].save_glb(out_path)
    return stats


def ensure_ue_road_meshes(force_reimport=True):
    """Generates road network .glb files and imports them into /Game/World/Meshes/."""
    import unreal

    stats = generate_road_glbs(force=force_reimport)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    assets = unreal.EditorAssetLibrary
    if not assets.does_directory_exist(UE_MESH_DIR):
        assets.make_directory(UE_MESH_DIR)

    tmp_dir = UE_MESH_DIR + "/_ImportRoadTmp"
    if assets.does_directory_exist(tmp_dir):
        assets.delete_directory(tmp_dir)

    tasks, task_names = [], []
    for name, _, _, _, _ in ROAD_MESH_MANIFEST:
        ue_path = "%s/%s" % (UE_MESH_DIR, name)
        if assets.does_directory_exist(ue_path):
            assets.delete_directory(ue_path)
        glb_path = os.path.join(OUT_DIR, name + ".glb")
        if force_reimport or not assets.does_asset_exist(ue_path):
            if assets.does_asset_exist(ue_path):
                assets.delete_asset(ue_path)
            task = unreal.AssetImportTask()
            task.filename = glb_path
            task.destination_path = tmp_dir
            task.destination_name = name
            task.replace_existing = True
            task.automated = True
            task.save = False
            tasks.append(task)
            task_names.append(name)

    if tasks:
        assets.make_directory(tmp_dir)
        tools.import_asset_tasks(tasks)
        for name, task in zip(task_names, tasks):
            sm_src = None
            for p in task.imported_object_paths:
                if isinstance(unreal.load_asset(p), unreal.StaticMesh):
                    sm_src = p.split(".")[0]
                    break
            if sm_src is None:
                for p in assets.list_assets(tmp_dir, recursive=True):
                    if p.endswith("." + name) and isinstance(unreal.load_asset(p), unreal.StaticMesh):
                        sm_src = p.split(".")[0]
                        break
            if sm_src is None:
                raise RuntimeError("Interchange produced no StaticMesh for " + name)
            dst_path = "%s/%s" % (UE_MESH_DIR, name)
            if not assets.rename_asset(sm_src, dst_path):
                raise RuntimeError("Failed to move %s -> %s" % (sm_src, dst_path))
        assets.delete_directory(tmp_dir)

    result = {}
    for name, mat_names, _phys, coll_mode, _cast_shadow in ROAD_MESH_MANIFEST:
        ue_path = "%s/%s" % (UE_MESH_DIR, name)
        sm = unreal.load_asset(ue_path)
        if sm is None or not isinstance(sm, unreal.StaticMesh):
            raise RuntimeError("Failed to load road StaticMesh: " + ue_path)
        try:
            nanite = sm.get_editor_property("nanite_settings")
            if nanite.get_editor_property("enabled"):
                nanite.set_editor_property("enabled", False)
                sm.set_editor_property("nanite_settings", nanite)
        except Exception:
            pass

        static_mats = list(sm.get_editor_property("static_materials"))
        for slot_idx, smat in enumerate(static_mats):
            target_mi_name = mat_names[slot_idx] if slot_idx < len(mat_names) else mat_names[-1]
            slot_str = str(smat.get_editor_property("material_slot_name"))
            if slot_str in mat_names:
                target_mi_name = slot_str
            mi = unreal.load_asset("%s/%s" % (UE_MAT_DIR, target_mi_name))
            if mi is not None:
                smat.set_editor_property("material_interface", mi)
        sm.set_editor_property("static_materials", static_mats)

        body_setup = sm.get_editor_property("body_setup")
        if body_setup is not None and coll_mode == "complex":
            body_setup.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
            sm.set_editor_property("body_setup", body_setup)

        assets.save_loaded_asset(sm, only_if_is_dirty=False)
        result[name] = sm

    unreal.log("BN: ensured %d single-piece road network meshes (%d tris, %d welded junctions)" % (
        len(result), stats["total_tris"], stats["junctions"]
    ))
    return result


if __name__ == "__main__":
    stats = generate_road_glbs(force=True)
    print("BN road mesh OK: %d total tris, %d welded junctions, %d smooth pass-throughs, inverted=%d" % (
        stats["total_tris"], stats["junctions"], stats["smooth_deg2"], stats["inverted_road_tris"]
    ))
    for k, v in stats["tri_counts"].items():
        print("  - %-26s : %6d tris" % (k, v))
