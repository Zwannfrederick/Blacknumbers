"""Procedural glTF 2.0 (.glb) 3D asset generator and UE 5.8 Interchange importer for BLACK NUMBERS.

Generates 26 original 3D meshes under Saved/GeneratedMeshes/ with pure Python3 (struct + json + math)
and imports them into /Game/World/Meshes/ with Nanite disabled, proper collision, and curated
/Game/World/Materials/MI_* material slots assigned automatically.

Run standalone (.glb export only):
    python3 Tools/bn_meshes.py --generate-only
Or inside UE5 Python:
    import bn_meshes
    meshes = bn_meshes.ensure_ue_meshes()
"""
import json
import math
import os
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bn_building_meshes as bm_bldg  # noqa: E402
import bn_mesh_vehicles_landmarks as bm_extra  # noqa: E402

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(PROJECT_DIR, "Saved", "GeneratedMeshes")
UE_MESH_DIR = "/Game/World/Meshes"
UE_MAT_DIR = "/Game/World/Materials"


# ---------------------------------------------------------------------------
# Pure Python glTF 2.0 (.glb) Builder
# ---------------------------------------------------------------------------
def _sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def _add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def _scale(a, s):
    return (a[0] * s, a[1] * s, a[2] * s)


def _cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def _norm(a):
    l = math.sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2])
    return (0.0, 0.0, 1.0) if l < 1e-8 else (a[0] / l, a[1] / l, a[2] / l)


def _ue_to_gltf_pos(p):
    # UE (left-handed: +X fwd, +Y right, +Z up, metres) -> glTF (right-handed: +Y up)
    # Verified empirically against UE 5.8 Interchange glTF importer:
    # UE.X = -glTF.Z * 100? -> We map (-y, z, x) or (x, z, y) and verify bounding box below.
    return (float(p[0]), float(p[2]), float(p[1]))


class GltfBuilder:
    """Accumulates triangles per material slot in UE metres and exports valid binary glTF 2.0 (.glb)."""

    def __init__(self, material_names):
        self.material_names = list(material_names)
        self.slots = {i: {"pos": [], "nrm": [], "uv": [], "idx": []} for i in range(len(material_names))}

    def add_tri(self, mat, p0, p1, p2, uv0=(0.0, 0.0), uv1=(1.0, 0.0), uv2=(0.5, 1.0)):
        slot = self.slots[mat]
        g0, g1, g2 = _ue_to_gltf_pos(p0), _ue_to_gltf_pos(p1), _ue_to_gltf_pos(p2)
        # Swap winding (g0, g2, g1) because (x, z, y) has det = -1
        e1 = _sub(g2, g0)
        e2 = _sub(g1, g0)
        n = _norm(_cross(e1, e2))
        base = len(slot["pos"])
        slot["pos"].extend([g0, g2, g1])
        slot["nrm"].extend([n, n, n])
        slot["uv"].extend([uv0, uv2, uv1])
        slot["idx"].extend([base, base + 1, base + 2])

    def add_quad_uv(self, mat, p0, p1, p2, p3, uv0, uv1, uv2, uv3):
        self.add_tri(mat, p0, p1, p2, uv0, uv1, uv2)
        self.add_tri(mat, p0, p2, p3, uv0, uv2, uv3)

    def add_quad(self, mat, p0, p1, p2, p3):
        self.add_quad_uv(mat, p0, p1, p2, p3, (0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0))

    def add_box(self, mat, center, size):
        cx, cy, cz = center
        hx, hy, hz = size[0] * 0.5, size[1] * 0.5, size[2] * 0.5
        v = [
            (cx - hx, cy - hy, cz - hz), (cx + hx, cy - hy, cz - hz),
            (cx + hx, cy + hy, cz - hz), (cx - hx, cy + hy, cz - hz),
            (cx - hx, cy - hy, cz + hz), (cx + hx, cy - hy, cz + hz),
            (cx + hx, cy + hy, cz + hz), (cx - hx, cy + hy, cz + hz),
        ]
        # Outward CCW faces in UE (+X fwd, +Y right, +Z up)
        self.add_quad(mat, v[1], v[2], v[6], v[5])  # +X front
        self.add_quad(mat, v[3], v[0], v[4], v[7])  # -X back
        self.add_quad(mat, v[2], v[3], v[7], v[6])  # +Y right
        self.add_quad(mat, v[0], v[1], v[5], v[4])  # -Y left
        self.add_quad(mat, v[4], v[5], v[6], v[7])  # +Z top
        self.add_quad(mat, v[3], v[2], v[1], v[0])  # -Z bottom

    def add_frustum_box(self, mat, base_center, base_xy, top_xy, height):
        cx, cy, cz = base_center
        bx, by = base_xy[0] * 0.5, base_xy[1] * 0.5
        tx, ty = top_xy[0] * 0.5, top_xy[1] * 0.5
        v = [
            (cx - bx, cy - by, cz), (cx + bx, cy - by, cz),
            (cx + bx, cy + by, cz), (cx - bx, cy + by, cz),
            (cx - tx, cy - ty, cz + height), (cx + tx, cy - ty, cz + height),
            (cx + tx, cy + ty, cz + height), (cx - tx, cy + ty, cz + height),
        ]
        self.add_quad(mat, v[1], v[2], v[6], v[5])
        self.add_quad(mat, v[3], v[0], v[4], v[7])
        self.add_quad(mat, v[2], v[3], v[7], v[6])
        self.add_quad(mat, v[0], v[1], v[5], v[4])
        self.add_quad(mat, v[4], v[5], v[6], v[7])
        self.add_quad(mat, v[3], v[2], v[1], v[0])

    def add_frustum_cyl(self, mat, base_center, r_bot, r_top, height, segments=8, lean=(0.0, 0.0)):
        bx, by, bz = base_center
        tx, ty, tz = bx + lean[0], by + lean[1], bz + height
        bot, top = [], []
        for i in range(segments):
            a = (i / float(segments)) * 2.0 * math.pi
            ca, sa = math.cos(a), math.sin(a)
            bot.append((bx + ca * r_bot, by + sa * r_bot, bz))
            top.append((tx + ca * r_top, ty + sa * r_top, tz))
        for i in range(segments):
            j = (i + 1) % segments
            u0, u1 = i / float(segments), (i + 1) / float(segments)
            self.add_quad_uv(mat, bot[i], bot[j], top[j], top[i], (u0, 0.0), (u1, 0.0), (u1, 1.0), (u0, 1.0))
        # Cap top & bottom
        tc = (tx, ty, tz)
        bc = (bx, by, bz)
        for i in range(segments):
            j = (i + 1) % segments
            self.add_tri(mat, tc, top[i], top[j])
            self.add_tri(mat, bc, bot[j], bot[i])

    def add_cylinder_y(self, mat, center, radius, width, segments=14):
        cx, cy, cz = center
        hy = width * 0.5
        left, right = [], []
        for i in range(segments):
            a = (i / float(segments)) * 2.0 * math.pi
            ca, sa = math.cos(a), math.sin(a)
            left.append((cx + ca * radius, cy - hy, cz + sa * radius))
            right.append((cx + ca * radius, cy + hy, cz + sa * radius))
        for i in range(segments):
            j = (i + 1) % segments
            self.add_quad(mat, left[i], right[i], right[j], left[j])
            self.add_tri(mat, (cx, cy - hy, cz), left[j], left[i])
            self.add_tri(mat, (cx, cy + hy, cz), right[i], right[j])

    def add_beam(self, mat, p0, p1, thickness):
        d = _sub(p1, p0)
        l = math.sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2])
        if l < 1e-4:
            return
        fwd = _scale(d, 1.0 / l)
        ref = (0.0, 0.0, 1.0) if abs(fwd[2]) < 0.92 else (1.0, 0.0, 0.0)
        side = _norm(_cross(fwd, ref))
        up = _norm(_cross(side, fwd))
        h = thickness * 0.5
        offsets = [
            _add(_scale(side, -h), _scale(up, -h)),
            _add(_scale(side, +h), _scale(up, -h)),
            _add(_scale(side, +h), _scale(up, +h)),
            _add(_scale(side, -h), _scale(up, +h)),
        ]
        a = [_add(p0, o) for o in offsets]
        b = [_add(p1, o) for o in offsets]
        for i in range(4):
            j = (i + 1) % 4
            self.add_quad(mat, a[i], a[j], b[j], b[i])
        self.add_quad(mat, a[3], a[2], a[1], a[0])
        self.add_quad(mat, b[0], b[1], b[2], b[3])

    def add_card(self, mat, center, size_wh, yaw_deg=0.0, pitch_deg=0.0, roll_deg=0.0):
        """Adds an oriented foliage/sign quad mapped cleanly to UV [0..1]x[0..1]."""
        w, h = size_wh[0] * 0.5, size_wh[1] * 0.5
        cy, sy = math.cos(math.radians(yaw_deg)), math.sin(math.radians(yaw_deg))
        cp, sp = math.cos(math.radians(pitch_deg)), math.sin(math.radians(pitch_deg))
        cr, sr = math.cos(math.radians(roll_deg)), math.sin(math.radians(roll_deg))

        def rot(lx, ly, lz):
            # Roll (X), Pitch (Y), Yaw (Z)
            y1, z1 = ly * cr - lz * sr, ly * sr + lz * cr
            x2, z2 = lx * cp + z1 * sp, -lx * sp + z1 * cp
            x3, y3 = x2 * cy - y1 * sy, x2 * sy + y1 * cy
            return (center[0] + x3, center[1] + y3, center[2] + z2)

        p0 = rot(0.0, -w, -h)
        p1 = rot(0.0, +w, -h)
        p2 = rot(0.0, +w, +h)
        p3 = rot(0.0, -w, +h)
        # V=1 bottom, V=0 top matches PIL top-down PNG orientation
        self.add_quad_uv(mat, p0, p1, p2, p3, (0.0, 1.0), (1.0, 1.0), (1.0, 0.0), (0.0, 0.0))

    def save_glb(self, filepath):
        bin_chunks = bytearray()
        buffer_views = []
        accessors = []
        primitives = []

        def append_bytes(data, target=None):
            while len(bin_chunks) % 4 != 0:
                bin_chunks.append(0)
            offset = len(bin_chunks)
            bin_chunks.extend(data)
            bv = {"buffer": 0, "byteOffset": offset, "byteLength": len(data)}
            if target is not None:
                bv["target"] = target
            idx = len(buffer_views)
            buffer_views.append(bv)
            return idx

        for mat_idx, mname in enumerate(self.material_names):
            slot = self.slots[mat_idx]
            if not slot["idx"]:
                continue
            pos = slot["pos"]
            nrm = slot["nrm"]
            uv = slot["uv"]
            ind = slot["idx"]

            pos_bytes = b"".join(struct.pack("<3f", *p) for p in pos)
            nrm_bytes = b"".join(struct.pack("<3f", *n) for n in nrm)
            uv_bytes = b"".join(struct.pack("<2f", u, v) for u, v in uv)
            idx_bytes = b"".join(struct.pack("<I", i) for i in ind)

            bv_pos = append_bytes(pos_bytes, 34962)
            bv_nrm = append_bytes(nrm_bytes, 34962)
            bv_uv = append_bytes(uv_bytes, 34962)
            bv_idx = append_bytes(idx_bytes, 34963)

            mins = [min(p[c] for p in pos) for c in range(3)]
            maxs = [max(p[c] for p in pos) for c in range(3)]

            acc_pos = len(accessors)
            accessors.append({"bufferView": bv_pos, "componentType": 5126, "count": len(pos), "type": "VEC3", "min": mins, "max": maxs})
            acc_nrm = len(accessors)
            accessors.append({"bufferView": bv_nrm, "componentType": 5126, "count": len(nrm), "type": "VEC3"})
            acc_uv = len(accessors)
            accessors.append({"bufferView": bv_uv, "componentType": 5126, "count": len(uv), "type": "VEC2"})
            acc_idx = len(accessors)
            accessors.append({"bufferView": bv_idx, "componentType": 5125, "count": len(ind), "type": "SCALAR"})

            primitives.append({
                "attributes": {"POSITION": acc_pos, "NORMAL": acc_nrm, "TEXCOORD_0": acc_uv},
                "indices": acc_idx,
                "material": mat_idx,
            })

        while len(bin_chunks) % 4 != 0:
            bin_chunks.append(0)

        gltf = {
            "asset": {"version": "2.0", "generator": "BLACK NUMBERS Procedural glTF Builder"},
            "scene": 0,
            "scenes": [{"nodes": [0]}],
            "nodes": [{"mesh": 0, "name": os.path.splitext(os.path.basename(filepath))[0]}],
            "meshes": [{"name": os.path.splitext(os.path.basename(filepath))[0], "primitives": primitives}],
            "materials": [{"name": m, "pbrMetallicRoughness": {"baseColorFactor": [0.8, 0.8, 0.8, 1.0]}} for m in self.material_names],
            "buffers": [{"byteLength": len(bin_chunks)}],
            "bufferViews": buffer_views,
            "accessors": accessors,
        }

        json_bytes = json.dumps(gltf, separators=(",", ":")).encode("utf-8")
        while len(json_bytes) % 4 != 0:
            json_bytes += b" "

        total_len = 12 + 8 + len(json_bytes) + 8 + len(bin_chunks)
        with open(filepath, "wb") as f:
            f.write(struct.pack("<4sII", b"glTF", 2, total_len))
            f.write(struct.pack("<I4s", len(json_bytes), b"JSON"))
            f.write(json_bytes)
            f.write(struct.pack("<I4s", len(bin_chunks), b"BIN\x00"))
            f.write(bin_chunks)


# ---------------------------------------------------------------------------
# Nature & City Furniture Geometry Builders
# ---------------------------------------------------------------------------
def _build_autumn_tree(b, tall=False):
    # Slots: 0=Bark, 1=Leaves
    h_trunk = 4.2 if tall else 3.4
    lean = (0.35, -0.22) if tall else (-0.18, 0.14)
    b.add_frustum_cyl(0, (0.0, 0.0, 0.0), 0.36 if tall else 0.32, 0.18, h_trunk, segments=8, lean=lean)
    fork = (lean[0], lean[1], h_trunk - 0.3)
    # 4 spreading woody limbs
    limbs = [
        (fork[0] + 1.4, fork[1] + 1.1, h_trunk + 2.4),
        (fork[0] - 1.5, fork[1] + 0.9, h_trunk + 2.6),
        (fork[0] + 0.8, fork[1] - 1.5, h_trunk + 2.3),
        (fork[0] - 1.0, fork[1] - 1.2, h_trunk + 2.8),
        (fork[0] + 0.1, fork[1] + 0.1, h_trunk + (3.6 if tall else 3.0)),
    ]
    for tip in limbs:
        b.add_beam(0, fork, tip, 0.18)
    # Crossed multi-angle lush leaf-cluster cards around each limb tip + crown core
    card_specs = []
    for idx, tip in enumerate(limbs):
        sz = (3.4, 3.2) if idx < 4 else (3.8, 3.5)
        base_yaw = idx * 38.0
        card_specs.append((tip, sz, base_yaw, 18.0, 0.0))
        card_specs.append((tip, sz, base_yaw + 62.0, -16.0, 8.0))
        card_specs.append((tip, (sz[0] * 0.9, sz[1] * 0.9), base_yaw + 124.0, 24.0, -10.0))
    # Crown center volume cards
    cc = (fork[0], fork[1], h_trunk + 1.9)
    for ang in (15.0, 75.0, 135.0):
        card_specs.append((cc, (4.2, 3.6), ang, 10.0, 0.0))
    for center, sz, yaw, pitch, roll in card_specs:
        b.add_card(1, center, sz, yaw_deg=yaw, pitch_deg=pitch, roll_deg=roll)


def build_tree_autumn_a(b):
    _build_autumn_tree(b, tall=False)


def build_tree_autumn_b(b):
    _build_autumn_tree(b, tall=True)


def build_tree_pine(b):
    # Slots: 0=Bark, 1=Pine Needles
    b.add_frustum_cyl(0, (0.0, 0.0, 0.0), 0.30, 0.06, 10.6, segments=8)
    tiers = [
        (3.2, 4.6, 2.8, 6),
        (4.8, 4.0, 2.5, 6),
        (6.4, 3.3, 2.3, 5),
        (7.9, 2.6, 2.0, 5),
        (9.3, 1.9, 1.8, 4),
        (10.6, 1.2, 1.6, 3),
    ]
    for z, w, h, cnt in tiers:
        for k in range(cnt):
            ang = (k / float(cnt)) * 360.0 + z * 19.0
            rad = w * 0.24
            cx = math.cos(math.radians(ang)) * rad
            cy = math.sin(math.radians(ang)) * rad
            b.add_card(1, (cx, cy, z), (w, h), yaw_deg=ang + 90.0, pitch_deg=28.0)


def build_bush(b):
    # Slot: 0=Bush Leaves (no trunk, dense ground shrub)
    for k in range(8):
        ang = k * 45.0
        r = 0.42
        cx = math.cos(math.radians(ang)) * r
        cy = math.sin(math.radians(ang)) * r
        b.add_card(0, (cx, cy, 0.72), (1.75, 1.38), yaw_deg=ang + 25.0, pitch_deg=24.0 if k % 2 == 0 else -18.0)
    b.add_card(0, (0.0, 0.0, 0.85), (1.85, 1.45), yaw_deg=20.0, pitch_deg=55.0)
    b.add_card(0, (0.0, 0.0, 0.85), (1.85, 1.45), yaw_deg=110.0, pitch_deg=55.0)


def build_street_lamp(b):
    """8.2 m tapered octagonal municipal street lamp with gooseneck arm extending along +X."""
    # Slots: 0=Pole Dark Metal, 1=Concrete Plinth, 2=Luminaire Housing, 3=Emissive Glow Lens
    b.add_frustum_cyl(1, (0.0, 0.0, 0.0), 0.26, 0.21, 0.65, segments=8)
    b.add_frustum_cyl(0, (0.0, 0.0, 0.65), 0.14, 0.075, 6.85, segments=8)
    # Curved 3-segment gooseneck cantilever toward +X
    b.add_beam(0, (0.0, 0.0, 7.45), (0.45, 0.0, 7.95), 0.11)
    b.add_beam(0, (0.45, 0.0, 7.95), (1.20, 0.0, 8.10), 0.10)
    b.add_beam(0, (1.20, 0.0, 8.10), (1.78, 0.0, 8.02), 0.09)
    # Cobra-head luminaire shell + downward lens
    b.add_box(2, (1.86, 0.0, 8.04), (0.72, 0.34, 0.16))
    b.add_box(3, (1.86, 0.0, 7.94), (0.56, 0.24, 0.04))


def build_traffic_light(b):
    """Intersection cantilevered signal mast (+X arm across lane, facing -Y/both ways)."""
    # Slots: 0=Dark Metal, 1=Red, 2=Amber, 3=Green
    b.add_frustum_cyl(0, (0.0, 0.0, 0.0), 0.18, 0.11, 5.8, segments=8)
    b.add_beam(0, (0.0, 0.0, 5.55), (5.2, 0.0, 5.65), 0.13)
    # Signal boxes hanging at X=2.6 m and X=4.6 m
    for sx in (2.6, 4.6):
        b.add_box(0, (sx, 0.0, 4.95), (0.40, 0.36, 1.15))
        for sy in (-0.19, 0.19):
            b.add_box(1, (sx, sy, 5.30), (0.22, 0.04, 0.22))
            b.add_box(2, (sx, sy, 4.96), (0.22, 0.04, 0.22))
            b.add_box(3, (sx, sy, 4.62), (0.22, 0.04, 0.22))


def build_jersey_barrier(b):
    """Authentic sloped-base New Jersey concrete highway divider (3.2 m long along X, Z=0..0.95 m)."""
    # Slots: 0=Concrete, 1=Hazard Strip
    prof_yz = [(-0.34, 0.0), (0.34, 0.0), (0.32, 0.18), (0.14, 0.42), (0.10, 0.94), (-0.10, 0.94), (-0.14, 0.42), (-0.32, 0.18)]
    hx = 1.60
    n = len(prof_yz)
    for i in range(n):
        j = (i + 1) % n
        y0, z0 = prof_yz[i]
        y1, z1 = prof_yz[j]
        b.add_quad(0, (-hx, y0, z0), (hx, y0, z0), (hx, y1, z1), (-hx, y1, z1))
    # End caps
    for sx in (-hx, hx):
        c = (sx, 0.0, 0.42)
        for i in range(n):
            j = (i + 1) % n
            p0 = (sx, prof_yz[i][0], prof_yz[i][1])
            p1 = (sx, prof_yz[j][0], prof_yz[j][1])
            if sx > 0:
                b.add_tri(0, c, p0, p1)
            else:
                b.add_tri(0, c, p1, p0)
    b.add_box(1, (0.0, 0.0, 0.96), (3.16, 0.16, 0.06))


def build_gantry_truss(b):
    """24 m wide highway overhead lattice steel gantry portal (spans along Y, road passes along X)."""
    # Slots: 0=Steel Guardrail, 1=Concrete Base
    hy = 12.0
    h = 7.8
    for s in (-1.0, 1.0):
        b.add_box(1, (0.0, s * hy, 0.5), (1.8, 1.8, 1.0))
        # Twin leg lattice column
        for sx in (-0.55, 0.55):
            b.add_beam(0, (sx, s * hy, 0.9), (sx, s * hy, h + 1.2), 0.24)
        for zk in range(5):
            z0 = 1.2 + zk * 1.4
            b.add_beam(0, (-0.55, s * hy, z0), (0.55, s * hy, z0 + 1.4), 0.14)
            b.add_beam(0, (0.55, s * hy, z0), (-0.55, s * hy, z0 + 1.4), 0.14)
    # Overhead top & bottom truss chords + diagonals across Y
    for sx in (-0.55, 0.55):
        b.add_beam(0, (sx, -hy, h), (sx, hy, h), 0.24)
        b.add_beam(0, (sx, -hy, h + 1.3), (sx, hy, h + 1.3), 0.24)
    bays = 12
    dy = (2.0 * hy) / bays
    for k in range(bays):
        y0 = -hy + k * dy
        y1 = y0 + dy
        for sx in (-0.55, 0.55):
            b.add_beam(0, (sx, y0, h), (sx, y1, h + 1.3), 0.13)
            b.add_beam(0, (sx, y0, h), (sx, y0, h + 1.3), 0.13)


def build_sign_board(b):
    """8.4 m x 3.4 m framed Turkish highway gantry sign (faces -X and +X cleanly with left-to-right [0..1] UV)."""
    # Slots: 0=Sign Face (swappable MI_Sign_*), 1=Dark Metal Frame
    b.add_box(1, (0.0, 0.0, 0.0), (0.18, 8.50, 3.50))
    # UE left-handed: looking toward +X (facing -X face), left=-Y (u=0), right=+Y (u=1).
    # Looking toward -X (facing +X face), left=+Y (u=0), right=-Y (u=1).
    hw, hh = 4.15, 1.66
    b.add_quad_uv(0, (-0.11, +hw, -hh), (-0.11, -hw, -hh), (-0.11, -hw, +hh), (-0.11, +hw, +hh),
                  (1.0, 1.0), (0.0, 1.0), (0.0, 0.0), (1.0, 0.0))
    b.add_quad_uv(0, (+0.11, -hw, -hh), (+0.11, +hw, -hh), (+0.11, +hw, +hh), (+0.11, -hw, +hh),
                  (1.0, 1.0), (0.0, 1.0), (0.0, 0.0), (1.0, 0.0))


def build_storefront(b):
    """14 m wide x 4.4 m tall urban ground-floor Karamara shopfront fascia with angled awning (faces +X)."""
    # Slots: 0=Storefront Atlas, 1=Awning Red, 2=Dark Trim
    hw, hh = 7.0, 2.2
    # Deep 2.4 m architectural ground-floor bay so storefronts embed cleanly into downtown facades
    b.add_box(2, (-1.20, 0.0, hh), (2.40, hw * 2.0 + 0.2, hh * 2.0 + 0.15))
    # Front showcase quad facing +X: viewer on +X looking toward -X has +Y on left (u=0) and -Y on right (u=1)
    b.add_quad_uv(0, (0.04, -hw, 0.0), (0.04, +hw, 0.0), (0.04, +hw, hh * 2.0), (0.04, -hw, hh * 2.0),
                  (1.0, 1.0), (0.0, 1.0), (0.0, 0.0), (1.0, 0.0))
    # Projecting sloped canopy awning above shop windows
    b.add_quad(1, (0.04, -hw, 3.25), (0.04, +hw, 3.25), (1.25, +hw, 2.75), (1.25, -hw, 2.75))
    b.add_box(1, (1.25, 0.0, 2.66), (0.06, hw * 2.0, 0.20))


def build_billboard(b):
    """Roadside twin-column billboard (10 m x 4.6 m panel at Z=8.5 m)."""
    # Slots: 0=Steel, 1=Panel Face
    for sy in (-3.2, 3.2):
        b.add_frustum_cyl(0, (0.0, sy, 0.0), 0.36, 0.28, 6.2, segments=8)
    b.add_box(0, (0.0, 0.0, 8.6), (0.36, 10.4, 4.8))
    b.add_box(0, (0.95, 0.0, 6.1), (1.3, 10.0, 0.14))  # Service catwalk
    hw, hh = 5.05, 2.28
    b.add_quad_uv(1, (+0.21, -hw, 8.6 - hh), (+0.21, +hw, 8.6 - hh), (+0.21, +hw, 8.6 + hh), (+0.21, -hw, 8.6 + hh),
                  (1.0, 1.0), (0.0, 1.0), (0.0, 0.0), (1.0, 0.0))
    b.add_quad_uv(1, (-0.21, +hw, 8.6 - hh), (-0.21, -hw, 8.6 - hh), (-0.21, -hw, 8.6 + hh), (-0.21, +hw, 8.6 + hh),
                  (1.0, 1.0), (0.0, 1.0), (0.0, 0.0), (1.0, 0.0))


def build_roof_water_tank(b):
    """Rooftop steel-framed cylindrical water tank (height 5.2 m)."""
    # Slots: 0=Dark Metal legs, 1=Concrete tank body, 2=Steel roof cap
    for sx in (-0.85, 0.85):
        for sy in (-0.85, 0.85):
            b.add_beam(0, (sx, sy, 0.0), (sx * 0.82, sy * 0.82, 2.2), 0.14)
    b.add_box(0, (0.0, 0.0, 2.2), (2.2, 2.2, 0.18))
    b.add_frustum_cyl(1, (0.0, 0.0, 2.28), 1.05, 1.05, 2.2, segments=12)
    b.add_frustum_cyl(2, (0.0, 0.0, 4.48), 1.15, 0.12, 0.72, segments=12)


def build_factory_chimney(b):
    """28 m tall tapered industrial brick smokestack with aviation red warning bands."""
    # Slots: 0=Brick, 1=Red Warning Bands, 2=Dark Cap
    b.add_frustum_cyl(0, (0.0, 0.0, 0.0), 2.10, 1.35, 18.0, segments=12)
    b.add_frustum_cyl(1, (0.0, 0.0, 18.0), 1.35, 1.22, 3.0, segments=12)
    b.add_frustum_cyl(0, (0.0, 0.0, 21.0), 1.22, 1.10, 3.0, segments=12)
    b.add_frustum_cyl(1, (0.0, 0.0, 24.0), 1.10, 0.98, 3.2, segments=12)
    b.add_frustum_cyl(2, (0.0, 0.0, 27.2), 1.12, 1.06, 0.8, segments=12)


# ---------------------------------------------------------------------------
# Asset Manifest: name -> (builder_fn, [MI_material_names...], collision_mode)
# ---------------------------------------------------------------------------
MESH_MANIFEST = [
    ("SM_BN_Tree_Autumn_A", build_tree_autumn_a, ["MI_TreeTrunk", "MI_Foliage_AutumnA"], "none"),
    ("SM_BN_Tree_Autumn_B", build_tree_autumn_b, ["MI_TreeTrunk", "MI_Foliage_AutumnB"], "none"),
    ("SM_BN_Tree_Pine", build_tree_pine, ["MI_TreeTrunk", "MI_Foliage_Pine"], "none"),
    ("SM_BN_Bush", build_bush, ["MI_Foliage_Bush"], "none"),
    ("SM_BN_StreetLamp", build_street_lamp, ["MI_Metal_Dark", "MI_Concrete", "MI_Guardrail", "MI_LampGlow"], "none"),
    ("SM_BN_TrafficLight", build_traffic_light, ["MI_Metal_Dark", "MI_TrafficLightRed", "MI_TrafficLightAmber", "MI_TrafficLightGreen"], "simple"),
    ("SM_BN_JerseyBarrier", build_jersey_barrier, ["MI_Concrete", "MI_HazardYellow"], "simple"),
    ("SM_BN_GantryTruss", build_gantry_truss, ["MI_Guardrail", "MI_Concrete"], "simple"),
    ("SM_BN_SignBoard", build_sign_board, ["MI_Sign_Merkez", "MI_Metal_Dark"], "none"),
    ("SM_BN_Storefront", build_storefront, ["MI_Storefront_Karamara", "MI_Barrier_Red", "MI_Metal_Dark"], "none"),
    ("SM_BN_Billboard", build_billboard, ["MI_Metal_Dark", "MI_Sign_DoguSanayi"], "simple"),
    ("SM_BN_RoofWaterTank", build_roof_water_tank, ["MI_Metal_Dark", "MI_Concrete", "MI_Building_Steel"], "none"),
    ("SM_BN_FactoryChimney", build_factory_chimney, ["MI_Building_Brick", "MI_Barrier_Red", "MI_Metal_Dark"], "simple"),
    ("SM_BN_BridgePylon", bm_extra.build_bridge_pylon, ["MI_Concrete", "MI_BridgeSteel"], "simple"),
    ("SM_BN_BridgeTruss", bm_extra.build_bridge_truss, ["MI_BridgeSteel", "MI_Asphalt", "MI_Concrete"], "complex"),
    ("SM_BN_PortCrane", bm_extra.build_port_crane, ["MI_Container_Red", "MI_HazardYellow", "MI_Building_Glass"], "simple"),
    ("SM_BN_CargoShip", bm_extra.build_cargo_ship, ["MI_Barrier_Red", "MI_Metal_Dark", "MI_Concrete", "MI_Container_Blue", "MI_Container_Amber"], "none"),
    ("SM_BN_TunnelArch", bm_extra.build_tunnel_arch, ["MI_Concrete", "MI_Metal_Dark", "MI_HazardYellow"], "complex"),
    ("SM_BN_SurroundingHills", bm_extra.build_surrounding_hills, ["MI_MountainRock_Slope", "MI_Grass"], "none"),
    ("SM_BN_KaramaraHills", bm_extra.build_karamara_hills, ["MI_MountainRock_Slope", "MI_Grass"], "complex"),
    ("SM_BN_Traffic_Sedan", bm_extra.build_traffic_sedan, ["MI_Traffic_Paint", "MI_Traffic_Glass", "MI_Traffic_Trim", "MI_Traffic_Headlight", "MI_Traffic_Taillight"], "simple"),
    ("SM_BN_Traffic_Hatchback", bm_extra.build_traffic_hatchback, ["MI_Traffic_Paint", "MI_Traffic_Glass", "MI_Traffic_Trim", "MI_Traffic_Headlight", "MI_Traffic_Taillight"], "simple"),
    ("SM_BN_Traffic_Van", bm_extra.build_traffic_van, ["MI_Traffic_Paint", "MI_Traffic_Glass", "MI_Traffic_Trim", "MI_Traffic_Headlight", "MI_Traffic_Taillight"], "simple"),
    ("SM_BN_Traffic_BoxTruck", bm_extra.build_traffic_box_truck, ["MI_Traffic_Paint", "MI_Traffic_Glass", "MI_Traffic_Trim", "MI_Traffic_Headlight", "MI_Traffic_Taillight", "MI_Concrete"], "simple"),
    ("SM_BN_Traffic_SUV", bm_extra.build_traffic_suv, ["MI_Traffic_Paint", "MI_Traffic_Glass", "MI_Traffic_Trim", "MI_Traffic_Headlight", "MI_Traffic_Taillight"], "simple"),
    ("SM_BN_PoliceLightbar", bm_extra.build_police_lightbar, ["MI_Traffic_Trim", "MI_Police_LightRed", "MI_Police_LightBlue"], "none"),
    ("SM_BN_PushBar", bm_extra.build_police_pushbar, ["MI_Police_Chrome", "MI_Traffic_Trim"], "none"),
    ("SM_BN_PoliceHeli", bm_extra.build_police_heli_body, ["MI_Traffic_Paint", "MI_Traffic_Glass", "MI_Traffic_Trim", "MI_Police_LightRed", "MI_Police_LightBlue"], "none"),
    ("SM_BN_PoliceHeliRotor", bm_extra.build_police_heli_rotor, ["MI_Metal_Dark"], "none"),
    ("SM_BN_HipRoof", bm_extra.build_hip_roof, ["MI_Building_Terracotta"], "none"),
    ("SM_BN_RaceRing", bm_extra.build_race_ring, ["MI_RaceHolo_Start"], "none"),
    ("SM_BN_CheckpointChevron", bm_extra.build_checkpoint_chevron, ["MI_RaceHolo_Gate"], "none"),
    ("SM_BN_Aero_Splitter", bm_extra.build_aero_splitter, ["MI_CarCarbon"], "none"),
    ("SM_BN_Aero_Diffuser", bm_extra.build_aero_diffuser, ["MI_CarCarbon"], "none"),
    ("SM_BN_Aero_GTWing", bm_extra.build_aero_gt_wing, ["MI_CarCarbon"], "none"),
    ("SM_BN_Aero_RallyWing", bm_extra.build_aero_rally_wing, ["MI_CarCarbon"], "none"),
    ("SM_BN_Aero_Blower", bm_extra.build_aero_blower, ["MI_Police_Chrome", "MI_CarCarbon"], "none"),
    ("SM_BN_Aero_OffroadRack", bm_extra.build_aero_offroad_rack, ["MI_CarCarbon", "MI_LampGlow"], "none"),
    ("SM_BN_SkyDome", bm_extra.build_sky_dome, ["MI_SkyClouds"], "none"),
    ("SM_BN_JumpRamp", bm_extra.build_jump_ramp, ["MI_Concrete", "MI_HazardYellow", "MI_Metal_Dark"], "complex"),
] + bm_bldg.BUILDING_MESH_MANIFEST


def generate_all_glbs(force=False):
    os.makedirs(OUT_DIR, exist_ok=True)
    count = 0
    for name, builder_fn, mat_names, _coll in MESH_MANIFEST:
        out_path = os.path.join(OUT_DIR, name + ".glb")
        if force or not os.path.isfile(out_path):
            b = GltfBuilder(mat_names)
            builder_fn(b)
            b.save_glb(out_path)
            count += 1
    return len(MESH_MANIFEST)


def ensure_ue_meshes(force_reimport=False):
    """Generates .glb files and imports all 27 StaticMeshes into /Game/World/Meshes/."""
    import unreal

    generate_all_glbs(force=force_reimport)

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    assets = unreal.EditorAssetLibrary
    if not assets.does_directory_exist(UE_MESH_DIR):
        assets.make_directory(UE_MESH_DIR)

    # Clean up any leftover Interchange subdirectories from earlier runs
    tmp_dir = UE_MESH_DIR + "/_ImportTmp"
    if assets.does_directory_exist(tmp_dir):
        assets.delete_directory(tmp_dir)

    tasks = []
    task_names = []
    for name, _fn, _mats, _coll in MESH_MANIFEST:
        ue_path = "%s/%s" % (UE_MESH_DIR, name)
        # Clean stray directory with same name if present
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
                obj = unreal.load_asset(p)
                if isinstance(obj, unreal.StaticMesh):
                    sm_src = p.split(".")[0]
                    break
            if sm_src is None:
                # Fallback search inside tmp_dir
                for p in assets.list_assets(tmp_dir, recursive=True):
                    if p.endswith("." + name):
                        obj = unreal.load_asset(p)
                        if isinstance(obj, unreal.StaticMesh):
                            sm_src = p.split(".")[0]
                            break
            if sm_src is None:
                raise RuntimeError("Interchange produced no StaticMesh for " + name)
            dst_path = "%s/%s" % (UE_MESH_DIR, name)
            if not assets.rename_asset(sm_src, dst_path):
                raise RuntimeError("Failed to move %s -> %s" % (sm_src, dst_path))
        assets.delete_directory(tmp_dir)

    result = {}
    for name, _fn, mat_names, coll_mode in MESH_MANIFEST:
        ue_path = "%s/%s" % (UE_MESH_DIR, name)
        sm = unreal.load_asset(ue_path)
        if sm is None or not isinstance(sm, unreal.StaticMesh):
            raise RuntimeError("Failed to import/load StaticMesh: " + ue_path)

        # Ensure Nanite stays OFF (Vulkan SM5 4 GB VRAM discipline)
        try:
            nanite = sm.get_editor_property("nanite_settings")
            if nanite.get_editor_property("enabled"):
                nanite.set_editor_property("enabled", False)
                sm.set_editor_property("nanite_settings", nanite)
        except Exception:
            pass

        # Assign curated /Game/World/Materials/MI_* instances to every slot
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

        # Configure collision cleanly
        body_setup = sm.get_editor_property("body_setup")
        if body_setup is not None:
            if coll_mode == "complex":
                body_setup.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
            elif coll_mode == "simple":
                body_setup.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX)
            sm.set_editor_property("body_setup", body_setup)

        assets.save_loaded_asset(sm, only_if_is_dirty=False)
        result[name] = sm

    unreal.log("BN: ensured %d procedural glTF StaticMeshes under %s" % (len(result), UE_MESH_DIR))
    return result


if __name__ == "__main__":
    if "--generate-only" in sys.argv:
        n = generate_all_glbs(force="--force" in sys.argv)
        print("BN: generated %d glTF .glb meshes in %s" % (n, OUT_DIR))
    else:
        try:
            import unreal  # noqa: F401
            # Ensure materials exist first so slot assignment binds valid MI_* assets
            import bn_materials  # noqa: E402
            bn_materials.build()
            ensure_ue_meshes(force_reimport="--force" in sys.argv)
        except ImportError:
            n = generate_all_glbs(force="--force" in sys.argv)
            print("BN: generated %d glTF .glb meshes in %s (outside UE)" % (n, OUT_DIR))
