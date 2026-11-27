"""Downloads, flattens, aligns & imports royalty-free external vehicle 3D models into BLACK NUMBERS:
1. Automatic zero-login CC-BY 4.0 Khronos `CarConcept.glb` -> /Game/Vehicles/GrandTourer/SM_BN_GrandTourer (Draco RS-T)
2. Automatic zero-login CC0 1.0 `BMW M6 2006` (tyrant monkey via ErfanMo77/gltf-research-scenes) ->
   /Game/Vehicles/External/SM_Ext_ValkenRS4 (Valken RS-4)
3. Automatic zero-login CC0 1.0 `1967 Pontiac GTO` (thecali via ErfanMo77/gltf-research-scenes) ->
   /Game/Vehicles/External/SM_Ext_Vandal440R (Vandal 440R)
4. Automatic zero-login CC-BY 4.0 `BMW X5 2018 G05 SUV` (BMW Car IT via bmwcarit/digital-car-3d) ->
   /Game/Vehicles/External/SM_Ext_TauronV6Rally (Tauron V6 Rally) &
   /Game/Vehicles/External/SM_Ext_HeavyRhino (Heat 5 Heavy Rhino Armored Police SUV)
5. Optional user drop-in folder `Saved/ExternalVehicles/` (ALWAYS takes priority over automatic defaults):
   Drop any royalty-free CC0 / CC-BY 4.0 / Epic Fab `.glb` or `.fbx` named:
     - street_kurogane.glb  -> /Game/Vehicles/External/SM_Ext_KuroganeMSpec
     - coupe_valken.glb     -> /Game/Vehicles/External/SM_Ext_ValkenRS4
     - muscle_vandal.glb    -> /Game/Vehicles/External/SM_Ext_Vandal440R
     - rally_tauron.glb     -> /Game/Vehicles/External/SM_Ext_TauronV6Rally
     - gt_draco.glb         -> /Game/Vehicles/External/SM_Ext_DracoRST
     - hyper_nocturne.glb   -> /Game/Vehicles/External/SM_Ext_NocturneBN01
     - police_rhino.glb     -> /Game/Vehicles/External/SM_Ext_HeavyRhino

Standalone download & GLB body merge (system python3):
    python3 Tools/import_external_vehicles.py --stage-only
UE5 import (UnrealEditor-Cmd -run=pythonscript):
    UnrealEditor-Cmd BlackNumbers.uproject -run=pythonscript -script=Tools/import_external_vehicles.py
"""
import base64
import json
import math
import os
import struct
import sys
import urllib.request

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CACHE_ROOT = os.path.join(PROJECT_DIR, "Saved", "CC0ModelsCache")
CACHE_DIR = os.path.join(CACHE_ROOT, "CarConcept")
BMW_M6_DIR = os.path.join(CACHE_ROOT, "BMW_M6")
GTO_DIR = os.path.join(CACHE_ROOT, "Pontiac_GTO")
G05_DIR = os.path.join(CACHE_ROOT, "BMW_X5_G05")
DROPIN_DIR = os.path.join(PROJECT_DIR, "Saved", "ExternalVehicles")

KHRONOS_CAR_URL = (
    "https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/"
    "main/Models/CarConcept/glTF-Binary/CarConcept.glb"
)
KHRONOS_CAR_LOCAL = os.path.join(CACHE_DIR, "CarConcept.glb")
KHRONOS_CAR_MERGED = os.path.join(CACHE_DIR, "CarConcept_BodyMerged.glb")

BMW_M6_GLTF_URL = "https://raw.githubusercontent.com/ErfanMo77/gltf-research-scenes/main/scenes/bmw-m6/gltf/bmw_m6_core.gltf"
BMW_M6_BIN_URL = "https://media.githubusercontent.com/media/ErfanMo77/gltf-research-scenes/main/scenes/bmw-m6/gltf/bmw_m6.bin"
BMW_M6_MERGED = os.path.join(BMW_M6_DIR, "BMW_M6_BodyMerged.glb")

GTO_GLTF_URL = "https://raw.githubusercontent.com/ErfanMo77/gltf-research-scenes/main/scenes/pontiac-gto/gltf/pontiac_gto_core.gltf"
GTO_BIN_URL = "https://media.githubusercontent.com/media/ErfanMo77/gltf-research-scenes/main/scenes/pontiac-gto/gltf/pontiac_gto.bin"
GTO_MERGED = os.path.join(GTO_DIR, "Pontiac_GTO_BodyMerged.glb")

G05_BASE_URL = "https://media.githubusercontent.com/media/bmwcarit/digital-car-3d/master/G05/meshes/"
# (relative_path, mirror_z_for_both_sides)
G05_PARTS = [
    ("Exterior/O_G05_Exterior_Paint.gltf", False),
    ("Exterior/O_G05_Exterior_non-Paint.gltf", False),
    ("Exterior/O_G05_Hood.gltf", False),
    ("Exterior/O_G05_Headlights.gltf", False),
    ("Exterior/O_G05_Taillights.gltf", False),
    ("Doors/O_G05_Doors_F.gltf", True),
    ("Doors/O_G05_Doors_B.gltf", True),
    ("Doors/O_G05_Tailgate.gltf", False),
    ("Interior/O_G05_Seats.gltf", False),
]
G05_RALLY_MERGED = os.path.join(G05_DIR, "BMW_X5_RallyMerged.glb")
G05_RHINO_MERGED = os.path.join(G05_DIR, "BMW_X5_HeavyRhinoMerged.glb")

UE_GT_DIR = "/Game/Vehicles/GrandTourer"
UE_EXT_DIR = "/Game/Vehicles/External"

DROPIN_MAP = [
    ("street_kurogane", "SM_Ext_KuroganeMSpec"),
    ("coupe_valken",    "SM_Ext_ValkenRS4"),
    ("muscle_vandal",   "SM_Ext_Vandal440R"),
    ("rally_tauron",    "SM_Ext_TauronV6Rally"),
    ("gt_draco",        "SM_Ext_DracoRST"),
    ("hyper_nocturne",  "SM_Ext_NocturneBN01"),
    ("police_rhino",    "SM_Ext_HeavyRhino"),
]

AUTO_EXTERNAL_DEFAULTS = [
    ("coupe_valken",  "SM_Ext_ValkenRS4",     BMW_M6_MERGED,    30000),
    ("muscle_vandal", "SM_Ext_Vandal440R",    GTO_MERGED,       30000),
    ("rally_tauron",  "SM_Ext_TauronV6Rally", G05_RALLY_MERGED, 32000),
    ("police_rhino",  "SM_Ext_HeavyRhino",    G05_RHINO_MERGED, 28000),
]

DROPIN_README = """BLACK NUMBERS — Harici Araç İçe Aktarım Klasörü (Saved/ExternalVehicles/)
=========================================================================
Bu klasöre telifsiz (CC0 / CC-BY 4.0 / Epic Fab ücretsiz) `.glb`, `.gltf` veya `.fbx`
araç dosyaları bırakıp `Tools/build_map.sh` çalıştırdığında oyun o araç gövdelerini
otomatik içe aktarır (varsayılan CC0/CC-BY araçların yerine geçer).

Desteklenen dosya adları:
  - street_kurogane.glb  -> Kurogane M-Spec (varsayılan: UE5 SM_SportsCar)
  - coupe_valken.glb     -> Valken RS-4     (varsayılan: CC0 BMW M6 2006)
  - muscle_vandal.glb    -> Vandal 440R     (varsayılan: CC0 1967 Pontiac GTO)
  - rally_tauron.glb     -> Tauron V6 Rally (varsayılan: CC-BY 4.0 BMW X5 G05 SUV)
  - gt_draco.glb         -> Draco RS-T      (varsayılan: CC-BY 4.0 Khronos CarConcept)
  - hyper_nocturne.glb   -> Nocturne BN-01  (varsayılan: UE5 SM_AutomotiveTP_Car)
  - police_rhino.glb     -> Heavy Rhino SUV (varsayılan: CC-BY 4.0 BMW X5 Zırhlı Polis SUV)
"""

UA = {"User-Agent": "BlackNumbers-UE5-VehicleFetcher/2.0"}


def _download_if_missing(url, dst_path, min_bytes=512, force=False, timeout=90):
    os.makedirs(os.path.dirname(dst_path), exist_ok=True)
    if not force and os.path.isfile(dst_path) and os.path.getsize(dst_path) >= min_bytes:
        return True
    req = urllib.request.Request(url, headers=UA)
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        data = resp.read()
    with open(dst_path, "wb") as f:
        f.write(data)
    return True


def _node_local_matrix(node):
    import numpy as np

    if "matrix" in node:
        return np.array(node["matrix"], dtype=np.float64).reshape(4, 4).T
    M = np.eye(4, dtype=np.float64)
    if "translation" in node:
        M[:3, 3] = node["translation"]
    if "rotation" in node:
        x, y, z, w = node["rotation"]
        R = np.array([
            [1 - 2 * (y * y + z * z), 2 * (x * y - z * w),     2 * (x * z + y * w),     0.0],
            [2 * (x * y + z * w),     1 - 2 * (x * x + z * z), 2 * (y * z - x * w),     0.0],
            [2 * (x * z - y * w),     2 * (y * z + x * w),     1 - 2 * (x * x + y * y), 0.0],
            [0.0,                     0.0,                     0.0,                     1.0],
        ], dtype=np.float64)
        M = M @ R
    if "scale" in node:
        sx, sy, sz = node["scale"]
        M = M @ np.diag([sx, sy, sz, 1.0])
    return M


def _load_gltf_and_buffers(path):
    """Loads either binary .glb or JSON .gltf (with data-URI or external .bin buffers)."""
    base_dir = os.path.dirname(os.path.abspath(path))
    with open(path, "rb") as f:
        head = f.read(4)
        f.seek(0)
        if head == b"glTF":
            _magic, _ver, _length = struct.unpack("<4sII", f.read(12))
            jlen, _jtype = struct.unpack("<I4s", f.read(8))
            gltf = json.loads(f.read(jlen).decode("utf-8"))
            blen, _btype = struct.unpack("<I4s", f.read(8))
            return gltf, [f.read(blen)]
        gltf = json.loads(f.read().decode("utf-8"))

    bufs = []
    for b in gltf.get("buffers", []):
        uri = b.get("uri", "")
        if uri.startswith("data:"):
            _, b64 = uri.split(",", 1)
            bufs.append(base64.b64decode(b64))
        elif uri:
            with open(os.path.join(base_dir, uri), "rb") as bf:
                bufs.append(bf.read())
        else:
            bufs.append(b"")
    return gltf, bufs


def _read_accessor(gltf, bufs, idx):
    import numpy as np

    comp_dtypes = {5120: np.int8, 5121: np.uint8, 5122: np.int16, 5123: np.uint16, 5125: np.uint32, 5126: np.float32}
    type_counts = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}
    a = gltf["accessors"][idx]
    bv = gltf["bufferViews"][a["bufferView"]]
    blob = bufs[bv.get("buffer", 0)]
    dt = comp_dtypes[a["componentType"]]
    cnt = type_counts[a["type"]]
    off = bv.get("byteOffset", 0) + a.get("byteOffset", 0)
    stride = bv.get("byteStride", 0)
    itemsize = np.dtype(dt).itemsize * cnt
    if stride and stride != itemsize:
        raw = np.frombuffer(blob, dtype=np.uint8, count=stride * a["count"], offset=off)
        raw = raw.reshape(a["count"], stride)[:, :itemsize].copy()
        arr = np.frombuffer(raw.tobytes(), dtype=dt)
    else:
        arr = np.frombuffer(blob, dtype=dt, count=a["count"] * cnt, offset=off).copy()
    return arr.reshape(a["count"], cnt) if cnt > 1 else arr


def _write_merged_glb(dst_glb, mesh_name, out_materials, buckets, src_gltf=None, src_bufs=None):
    """Packs merged (P, N, U, I) material buckets into a single compact binary glTF 2.0 (.glb)."""
    import numpy as np

    out_bin = bytearray()
    out_bviews = []
    out_accessors = []
    out_images = []

    def push_bytes(raw_bytes, target=None):
        while len(out_bin) % 4 != 0:
            out_bin.append(0)
        off = len(out_bin)
        out_bin.extend(raw_bytes)
        bv = {"buffer": 0, "byteOffset": off, "byteLength": len(raw_bytes)}
        if target is not None:
            bv["target"] = target
        idx = len(out_bviews)
        out_bviews.append(bv)
        return idx

    if src_gltf is not None and src_bufs is not None:
        src_bviews = src_gltf.get("bufferViews", [])
        for img in src_gltf.get("images", []):
            new_img = dict(img)
            if "bufferView" in img:
                sbv = src_bviews[img["bufferView"]]
                sblob = src_bufs[sbv.get("buffer", 0)]
                soff = sbv.get("byteOffset", 0)
                new_img["bufferView"] = push_bytes(sblob[soff:soff + sbv["byteLength"]])
            out_images.append(new_img)

    out_primitives = []
    total_verts = 0
    total_tris = 0
    for mi in sorted(buckets.keys()):
        chunks = buckets[mi]
        if not chunks:
            continue
        pos_list, nrm_list, uv_list, ind_list = [], [], [], []
        v_off = 0
        for p, n, u, ind in chunks:
            pos_list.append(p)
            nrm_list.append(n)
            uv_list.append(u)
            ind_list.append(ind + np.uint32(v_off))
            v_off += len(p)
        P = np.vstack(pos_list).astype(np.float32)
        N = np.vstack(nrm_list).astype(np.float32)
        U = np.vstack(uv_list).astype(np.float32)
        I = np.concatenate(ind_list).astype(np.uint32)
        total_verts += len(P)
        total_tris += len(I) // 3

        bv_p = push_bytes(P.tobytes(), 34962)
        bv_n = push_bytes(N.tobytes(), 34962)
        bv_u = push_bytes(U.tobytes(), 34962)
        bv_i = push_bytes(I.tobytes(), 34963)

        acc_p = len(out_accessors)
        out_accessors.append({
            "bufferView": bv_p, "componentType": 5126, "count": len(P), "type": "VEC3",
            "min": [float(x) for x in P.min(axis=0)], "max": [float(x) for x in P.max(axis=0)],
        })
        acc_n = len(out_accessors)
        out_accessors.append({"bufferView": bv_n, "componentType": 5126, "count": len(N), "type": "VEC3"})
        acc_u = len(out_accessors)
        out_accessors.append({"bufferView": bv_u, "componentType": 5126, "count": len(U), "type": "VEC2"})
        acc_i = len(out_accessors)
        out_accessors.append({"bufferView": bv_i, "componentType": 5125, "count": len(I), "type": "SCALAR"})
        out_primitives.append({
            "attributes": {"POSITION": acc_p, "NORMAL": acc_n, "TEXCOORD_0": acc_u},
            "indices": acc_i,
            "material": mi,
        })

    while len(out_bin) % 4 != 0:
        out_bin.append(0)

    out_gltf = {
        "asset": {"version": "2.0", "generator": "BLACK NUMBERS External Vehicle Merger"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "name": mesh_name}],
        "meshes": [{"name": mesh_name, "primitives": out_primitives}],
        "materials": out_materials,
        "textures": (src_gltf.get("textures", []) if src_gltf else []),
        "samplers": (src_gltf.get("samplers", []) if src_gltf else []),
        "images": out_images,
        "buffers": [{"byteLength": len(out_bin)}],
        "bufferViews": out_bviews,
        "accessors": out_accessors,
    }
    if src_gltf and "extensionsUsed" in src_gltf:
        out_gltf["extensionsUsed"] = [e for e in src_gltf["extensionsUsed"] if e != "KHR_materials_variants"]

    json_bytes = json.dumps(out_gltf, separators=(",", ":")).encode("utf-8")
    while len(json_bytes) % 4 != 0:
        json_bytes += b" "

    total_len = 12 + 8 + len(json_bytes) + 8 + len(out_bin)
    os.makedirs(os.path.dirname(os.path.abspath(dst_glb)), exist_ok=True)
    with open(dst_glb, "wb") as f:
        f.write(struct.pack("<4sII", b"glTF", 2, total_len))
        f.write(struct.pack("<I4s", len(json_bytes), b"JSON"))
        f.write(json_bytes)
        f.write(struct.pack("<I4s", len(out_bin), b"BIN\x00"))
        f.write(out_bin)
    return total_verts, total_tris, len(out_primitives)


def _rot_y_mat(deg):
    import numpy as np

    r = math.radians(deg)
    c, s = math.cos(r), math.sin(r)
    return np.array([
        [ c, 0.0,  s, 0.0],
        [0.0, 1.0, 0.0, 0.0],
        [-s, 0.0,  c, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ], dtype=np.float64)


def merge_glb_to_single_mesh(src_glb, dst_glb, strip_wheels=True, yaw_y_deg=90.0):
    """Flattens Khronos CarConcept multi-node GLB into a single clean StaticMesh."""
    import numpy as np

    gltf, bufs = _load_gltf_and_buffers(src_glb)
    materials = gltf.get("materials", [])
    root_rot = _rot_y_mat(yaw_y_deg)
    skip_node_tokens = ("wheelfront", "wheelrear", "license") if strip_wheels else ("license",)
    skip_mat_tokens = ("tireside", "tiretread", "rim1", "rim2") if strip_wheels else ()

    buckets = {}
    nodes = gltf.get("nodes", [])
    meshes = gltf.get("meshes", [])

    def walk(node_idx, parent_m):
        node = nodes[node_idx]
        nname = (node.get("name") or "").lower()
        if any(tok in nname for tok in skip_node_tokens):
            return
        world_m = parent_m @ _node_local_matrix(node)
        if "mesh" in node and node["mesh"] is not None:
            mesh = meshes[node["mesh"]]
            lin = world_m[:3, :3]
            nrm_m = np.linalg.inv(lin).T
            det_neg = np.linalg.det(lin) < 0.0
            for pr in mesh.get("primitives", []):
                mi = pr.get("material", 0) or 0
                mname = (materials[mi].get("name") or "").lower() if mi < len(materials) else ""
                if any(tok in mname for tok in skip_mat_tokens):
                    continue
                if "license" in mname or "logo" in mname or "emblem" in mname:
                    mi = 0
                attrs = pr.get("attributes", {})
                if "POSITION" not in attrs:
                    continue
                pos_w = (_read_accessor(gltf, bufs, attrs["POSITION"]).astype(np.float64) @ lin.T) + world_m[:3, 3]
                if "NORMAL" in attrs:
                    nrm = _read_accessor(gltf, bufs, attrs["NORMAL"]).astype(np.float64) @ nrm_m.T
                    nrm /= np.maximum(np.linalg.norm(nrm, axis=1, keepdims=True), 1e-8)
                else:
                    nrm = np.tile(np.array([[0.0, 1.0, 0.0]]), (len(pos_w), 1))
                uv0 = _read_accessor(gltf, bufs, attrs["TEXCOORD_0"]).astype(np.float32) if "TEXCOORD_0" in attrs else np.zeros((len(pos_w), 2), dtype=np.float32)
                ind = _read_accessor(gltf, bufs, pr["indices"]).astype(np.uint32) if "indices" in pr else np.arange(len(pos_w), dtype=np.uint32)
                if det_neg and len(ind) >= 3:
                    ind = ind.reshape(-1, 3)[:, [0, 2, 1]].reshape(-1)
                buckets.setdefault(mi, []).append((pos_w.astype(np.float32), nrm.astype(np.float32), uv0, ind))
        for ch in node.get("children", []):
            walk(ch, world_m)

    for root_idx in gltf["scenes"][gltf.get("scene", 0)].get("nodes", []):
        walk(root_idx, root_rot)
    if not buckets:
        raise RuntimeError("No geometry survived merge for " + src_glb)
    verts, _tris, prims = _write_merged_glb(dst_glb, "SM_BN_GrandTourer", materials, buckets, gltf, bufs)
    return verts, prims


def _merge_research_car_gltf(src_gltf_path, dst_glb, mesh_name, skip_mats, wheel_filter_mode="none",
                             yaw_y_deg=0.0, target_len_m=4.68):
    """Flattens CC0 ErfanMo77/gltf-research-scenes car (BMW M6 / Pontiac GTO):
    - Drops studio ground planes & static wheels so UE5 Chaos wheels spin cleanly in the arches.
    - Normalizes length to target_len_m, centers XZ, rests lowest body point at Y = 0.22 m.
    - Neutralizes CarPaint baseColorFactor to bright clearcoat metallic so Garage Paint Picker tints cleanly.
    """
    import numpy as np

    gltf, bufs = _load_gltf_and_buffers(src_gltf_path)
    src_mats = gltf.get("materials", [])
    nodes = gltf.get("nodes", [])
    meshes = gltf.get("meshes", [])

    # First pass: collect valid primitives and measure car-body bounds (excluding skipped floor/wheel materials)
    raw_items = []
    body_pts = []

    def walk(node_idx, parent_m):
        node = nodes[node_idx]
        world_m = parent_m @ _node_local_matrix(node)
        if "mesh" in node and node["mesh"] is not None:
            mesh = meshes[node["mesh"]]
            lin = world_m[:3, :3]
            nrm_m = np.linalg.inv(lin).T
            det_neg = np.linalg.det(lin) < 0.0
            for pr in mesh.get("primitives", []):
                mi = pr.get("material", 0) or 0
                mname = (src_mats[mi].get("name") or "") if mi < len(src_mats) else ""
                ml = mname.lower()
                if any(tok in ml for tok in skip_mats):
                    continue
                attrs = pr.get("attributes", {})
                if "POSITION" not in attrs:
                    continue
                pos = (_read_accessor(gltf, bufs, attrs["POSITION"]).astype(np.float64) @ lin.T) + world_m[:3, 3]
                if len(pos) < 3:
                    continue
                if "NORMAL" in attrs:
                    nrm = _read_accessor(gltf, bufs, attrs["NORMAL"]).astype(np.float64) @ nrm_m.T
                    nrm /= np.maximum(np.linalg.norm(nrm, axis=1, keepdims=True), 1e-8)
                else:
                    nrm = np.tile(np.array([[0.0, 1.0, 0.0]]), (len(pos), 1))
                uv0 = _read_accessor(gltf, bufs, attrs["TEXCOORD_0"]).astype(np.float32) if "TEXCOORD_0" in attrs else np.zeros((len(pos), 2), dtype=np.float32)
                ind = _read_accessor(gltf, bufs, pr["indices"]).astype(np.uint32) if "indices" in pr else np.arange(len(pos), dtype=np.uint32)
                if det_neg and len(ind) >= 3:
                    ind = ind.reshape(-1, 3)[:, [0, 2, 1]].reshape(-1)
                raw_items.append((mi, ml, pos, nrm, uv0, ind))
                if "carpaint" in ml:
                    body_pts.append(pos)
        for ch in node.get("children", []):
            walk(ch, world_m)

    for root_idx in gltf["scenes"][gltf.get("scene", 0)].get("nodes", []):
        walk(root_idx, np.eye(4, dtype=np.float64))

    if not body_pts:
        raise RuntimeError("No CarPaint geometry found in " + src_gltf_path)
    bp = np.vstack(body_pts)
    bmin, bmax = bp.min(axis=0), bp.max(axis=0)
    bsize = bmax - bmin
    bctr = 0.5 * (bmin + bmax)

    # Determine car's long axis in glTF XZ plane and orient nose toward +X (glTF +X -> UE +X)
    long_is_z = bsize[2] > bsize[0]
    base_yaw = 90.0 if long_is_z else 0.0
    rot_m = _rot_y_mat(base_yaw + yaw_y_deg)[:3, :3]
    car_len = max(bsize[0], bsize[2], 1e-3)
    scale = target_len_m / car_len

    # Optional spatial wheel-strip for models whose wheels share generic Rubber/Steel materials (e.g. Pontiac GTO)
    kept_items = []
    for mi, ml, pos, nrm, uv0, ind in raw_items:
        pmin, pmax = pos.min(axis=0), pos.max(axis=0)
        psize = pmax - pmin
        pctr = 0.5 * (pmin + pmax)
        # Drop any oversized studio backdrop/ground plane (> 1.8x car length)
        if max(psize[0], psize[2]) > car_len * 1.8:
            continue
        if wheel_filter_mode == "gto" and any(k in ml for k in ("rubber", "steel", "chrome", "black")):
            # GTO wheels sit in the lower 45% of body height, near front/rear axles, outboard in width, circular-ish
            rel_y = (pctr[1] - bmin[1]) / max(bsize[1], 1e-3)
            lat_axis = 0 if long_is_z else 2
            lon_axis = 2 if long_is_z else 0
            rel_lat = abs(pctr[lat_axis] - bctr[lat_axis]) / max(bsize[lat_axis] * 0.5, 1e-3)
            rel_lon = abs(pctr[lon_axis] - bctr[lon_axis]) / max(bsize[lon_axis] * 0.5, 1e-3)
            is_wheel_zone = (rel_y < 0.36 and rel_lat > 0.52 and 0.32 < rel_lon < 0.88 and psize[1] < bsize[1] * 0.72)
            if is_wheel_zone and max(psize[0], psize[2]) < car_len * 0.25:
                continue
        kept_items.append((mi, ml, pos, nrm, uv0, ind))

    # Build clean consolidated PBR materials (slot 0 = tintable CarPaint)
    out_mats = []
    mat_remap = {}
    for mi, ml, *_ in kept_items:
        if mi in mat_remap:
            continue
        sm = src_mats[mi] if mi < len(src_mats) else {}
        pbr = dict(sm.get("pbrMetallicRoughness", {}))
        m_out = {"name": sm.get("name", f"Mat_{mi}"), "doubleSided": True}
        if "carpaint" in ml:
            m_out["name"] = "CarPaint"
            m_out["pbrMetallicRoughness"] = {
                "baseColorFactor": [0.88, 0.88, 0.90, 1.0],
                "metallicFactor": 0.78,
                "roughnessFactor": 0.18,
            }
        elif "logo" in ml:
            m_out["name"] = "TrimDark"
            m_out["pbrMetallicRoughness"] = {"baseColorFactor": [0.07, 0.075, 0.085, 1.0], "metallicFactor": 0.6, "roughnessFactor": 0.35}
        elif "glass" in ml or "windscreen" in ml:
            m_out["pbrMetallicRoughness"] = {"baseColorFactor": [0.05, 0.07, 0.095, 1.0], "metallicFactor": 0.15, "roughnessFactor": 0.06}
        else:
            bc = list(pbr.get("baseColorFactor", [0.4, 0.4, 0.4, 1.0]))
            if len(bc) == 3:
                bc.append(1.0)
            bc[3] = 1.0
            m_out["pbrMetallicRoughness"] = {
                "baseColorFactor": bc,
                "metallicFactor": float(pbr.get("metallicFactor", 0.35)),
                "roughnessFactor": float(pbr.get("roughnessFactor", 0.42)),
            }
        mat_remap[mi] = len(out_mats)
        out_mats.append(m_out)

    # Transform all kept geometry into normalized glTF space (centered XZ, nose +X, bottom Y = 0.22 m)
    all_rot_y = []
    transformed = []
    for mi, _ml, pos, nrm, uv0, ind in kept_items:
        p = ((pos - bctr) * scale) @ rot_m.T
        n = nrm @ rot_m.T
        n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-8)
        all_rot_y.append(p[:, 1])
        transformed.append((mat_remap[mi], p, n, uv0, ind))

    min_y = float(np.min(np.concatenate(all_rot_y)))
    buckets = {}
    for new_mi, p, n, uv0, ind in transformed:
        p[:, 1] += (0.22 - min_y)
        buckets.setdefault(new_mi, []).append((p.astype(np.float32), n.astype(np.float32), uv0, ind))

    return _write_merged_glb(dst_glb, mesh_name, out_mats, buckets)


def _merge_bmw_g05_suv(dst_glb, mesh_name, tactical_black=False):
    """Merges the CC-BY 4.0 BMW X5 (G05) exterior, doors (+ mirrored left side), hood, tailgate,
    headlights, taillights, and seats into a single crisp SUV body with open wheel wells."""
    import numpy as np

    mat_index = {}
    out_mats = []
    buckets = {}

    def get_mat_idx(raw_name):
        nl = (raw_name or "").lower()
        if "paint" in nl and "black" not in nl:
            key = "CarPaint"
        elif "glass" in nl or "lens_clear" in nl:
            key = "SUV_Glass"
        elif "lens_red" in nl or "opaque_red" in nl or "lighteffect_b" in nl:
            key = "SUV_TailLight"
        elif "lighteffect_f" in nl or "emission" in nl:
            key = "SUV_HeadLight"
        elif "chrome" in nl or "aluminum_light" in nl or "vaporized" in nl:
            key = "SUV_Chrome" if not tactical_black else "SUV_TacticalTrim"
        else:
            key = "SUV_TrimDark"

        if key in mat_index:
            return mat_index[key]
        if key == "CarPaint":
            spec = (
                {"name": "CarPaint", "doubleSided": True, "pbrMetallicRoughness": {"baseColorFactor": [0.028, 0.032, 0.042, 1.0], "metallicFactor": 0.22, "roughnessFactor": 0.56}}
                if tactical_black else
                {"name": "CarPaint", "doubleSided": True, "pbrMetallicRoughness": {"baseColorFactor": [0.88, 0.88, 0.90, 1.0], "metallicFactor": 0.78, "roughnessFactor": 0.18}}
            )
        elif key == "SUV_Glass":
            spec = {"name": "SUV_Glass", "doubleSided": True, "pbrMetallicRoughness": {"baseColorFactor": [0.03, 0.045, 0.065, 1.0], "metallicFactor": 0.15, "roughnessFactor": 0.07}}
        elif key == "SUV_TailLight":
            spec = {"name": "SUV_TailLight", "doubleSided": True, "emissiveFactor": [0.55, 0.04, 0.02], "pbrMetallicRoughness": {"baseColorFactor": [0.50, 0.04, 0.03, 1.0], "metallicFactor": 0.1, "roughnessFactor": 0.18}}
        elif key == "SUV_HeadLight":
            spec = {"name": "SUV_HeadLight", "doubleSided": True, "emissiveFactor": [0.85, 0.82, 0.72], "pbrMetallicRoughness": {"baseColorFactor": [0.92, 0.90, 0.86, 1.0], "metallicFactor": 0.2, "roughnessFactor": 0.15}}
        elif key == "SUV_Chrome":
            spec = {"name": "SUV_Chrome", "doubleSided": True, "pbrMetallicRoughness": {"baseColorFactor": [0.82, 0.84, 0.86, 1.0], "metallicFactor": 0.92, "roughnessFactor": 0.16}}
        elif key == "SUV_TacticalTrim":
            spec = {"name": "SUV_TacticalTrim", "doubleSided": True, "pbrMetallicRoughness": {"baseColorFactor": [0.06, 0.065, 0.075, 1.0], "metallicFactor": 0.65, "roughnessFactor": 0.42}}
        else:
            spec = {"name": "SUV_TrimDark", "doubleSided": True, "pbrMetallicRoughness": {"baseColorFactor": [0.045, 0.048, 0.055, 1.0], "metallicFactor": 0.20, "roughnessFactor": 0.62}}
        idx = len(out_mats)
        out_mats.append(spec)
        mat_index[key] = idx
        return idx

    # Ensure CarPaint is always slot 0 so FindPaintMaterialSlot binds immediately
    get_mat_idx("M_E_common_Paint")

    # G05 is in cm, nose at -X, height +Y, right side +Z -> scale 0.01 & rotate 180° around Y so nose faces +X
    rot180 = _rot_y_mat(180.0)[:3, :3]
    all_items = []

    for rel, mirror_z in G05_PARTS:
        local_p = os.path.join(G05_DIR, os.path.basename(rel))
        if not os.path.isfile(local_p):
            continue
        gltf, bufs = _load_gltf_and_buffers(local_p)
        src_mats = gltf.get("materials", [])
        nodes = gltf.get("nodes", [])
        meshes = gltf.get("meshes", [])

        def walk(node_idx, parent_m):
            node = nodes[node_idx]
            world_m = parent_m @ _node_local_matrix(node)
            nname = (node.get("name") or "").lower()
            if "mesh" in node and node["mesh"] is not None:
                mesh = meshes[node["mesh"]]
                lin = world_m[:3, :3]
                nrm_m = np.linalg.inv(lin).T
                det_neg = np.linalg.det(lin) < 0.0
                for pr in mesh.get("primitives", []):
                    mi = pr.get("material", 0) or 0
                    mname = (src_mats[mi].get("name") or "") if mi < len(src_mats) else ""
                    attrs = pr.get("attributes", {})
                    if "POSITION" not in attrs:
                        continue
                    pos = (_read_accessor(gltf, bufs, attrs["POSITION"]).astype(np.float64) @ lin.T) + world_m[:3, 3]
                    if "NORMAL" in attrs:
                        nrm = _read_accessor(gltf, bufs, attrs["NORMAL"]).astype(np.float64) @ nrm_m.T
                        nrm /= np.maximum(np.linalg.norm(nrm, axis=1, keepdims=True), 1e-8)
                    else:
                        nrm = np.tile(np.array([[0.0, 1.0, 0.0]]), (len(pos), 1))
                    uv0 = np.zeros((len(pos), 2), dtype=np.float32)
                    ind = _read_accessor(gltf, bufs, pr["indices"]).astype(np.uint32) if "indices" in pr else np.arange(len(pos), dtype=np.uint32)
                    if det_neg and len(ind) >= 3:
                        ind = ind.reshape(-1, 3)[:, [0, 2, 1]].reshape(-1)
                    out_mi = get_mat_idx(mname)
                    all_items.append((out_mi, pos, nrm, uv0, ind))
                    # Mirror right-side-only parts (Doors_F, Doors_B, Seats_F) across Z=0 for left side
                    if mirror_z or "seats_f" in nname:
                        pos_m = pos.copy()
                        pos_m[:, 2] = -pos_m[:, 2]
                        nrm_m2 = nrm.copy()
                        nrm_m2[:, 2] = -nrm_m2[:, 2]
                        ind_m = ind.reshape(-1, 3)[:, [0, 2, 1]].reshape(-1).copy()
                        all_items.append((out_mi, pos_m, nrm_m2, uv0, ind_m))
            for ch in node.get("children", []):
                walk(ch, world_m)

        for root_idx in gltf["scenes"][gltf.get("scene", 0)].get("nodes", []):
            walk(root_idx, np.eye(4, dtype=np.float64))

    if not all_items:
        raise RuntimeError("No BMW X5 G05 geometry loaded")

    all_p = np.vstack([it[1] for it in all_items])
    pmin, pmax = all_p.min(axis=0), all_p.max(axis=0)
    cx, cz = 0.5 * (pmin[0] + pmax[0]), 0.5 * (pmin[2] + pmax[2])
    min_y = float(pmin[1])
    # Scale to ~4.82 m SUV length (in metres)
    scale = 4.82 / max(float(pmax[0] - pmin[0]), 1.0)

    for out_mi, pos, nrm, uv0, ind in all_items:
        p = np.column_stack([pos[:, 0] - cx, pos[:, 1] - min_y, pos[:, 2] - cz]) * scale
        p = p @ rot180.T
        p[:, 1] += 0.24  # SUV ground clearance for spinning Chaos wheels
        n = nrm @ rot180.T
        n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-8)
        buckets.setdefault(out_mi, []).append((p.astype(np.float32), n.astype(np.float32), uv0, ind))

    return _write_merged_glb(dst_glb, mesh_name, out_mats, buckets)


def stage_files(force=False):
    os.makedirs(CACHE_DIR, exist_ok=True)
    os.makedirs(DROPIN_DIR, exist_ok=True)
    readme_path = os.path.join(DROPIN_DIR, "README.txt")
    with open(readme_path, "w", encoding="utf-8") as f:
        f.write(DROPIN_README)

    # 1. Khronos CarConcept (Draco RS-T)
    try:
        _download_if_missing(KHRONOS_CAR_URL, KHRONOS_CAR_LOCAL, min_bytes=1000000, force=force)
        if force or not (os.path.isfile(KHRONOS_CAR_MERGED) and os.path.getsize(KHRONOS_CAR_MERGED) > 500000):
            verts, prims = merge_glb_to_single_mesh(KHRONOS_CAR_LOCAL, KHRONOS_CAR_MERGED, strip_wheels=True, yaw_y_deg=90.0)
            print("BN: merged Khronos CarConcept body -> %d verts, %d PBR slots" % (verts, prims))
    except Exception as exc:
        print("BN: Khronos CarConcept offline/skipped: %s" % exc, file=sys.stderr)

    # 2. CC0 BMW M6 2006 (Valken RS-4)
    try:
        m6_gltf = os.path.join(BMW_M6_DIR, "bmw_m6_core.gltf")
        m6_bin = os.path.join(BMW_M6_DIR, "bmw_m6.bin")
        _download_if_missing(BMW_M6_GLTF_URL, m6_gltf, min_bytes=50000, force=force)
        _download_if_missing(BMW_M6_BIN_URL, m6_bin, min_bytes=5000000, force=force)
        if force or not (os.path.isfile(BMW_M6_MERGED) and os.path.getsize(BMW_M6_MERGED) > 1000000):
            v, t, p = _merge_research_car_gltf(
                m6_gltf, BMW_M6_MERGED, "SM_Ext_ValkenRS4",
                skip_mats=("floor", "mattefloor", "tire", "wheelrim", "brakedisc", "brake_clipper"),
                wheel_filter_mode="none", yaw_y_deg=180.0, target_len_m=4.68,
            )
            print("BN: staged CC0 BMW M6 (Valken RS-4) -> %d verts, %d tris, %d slots" % (v, t, p))
    except Exception as exc:
        print("BN: CC0 BMW M6 offline/skipped: %s" % exc, file=sys.stderr)

    # 3. CC0 1967 Pontiac GTO (Vandal 440R)
    try:
        gto_gltf = os.path.join(GTO_DIR, "pontiac_gto_core.gltf")
        gto_bin = os.path.join(GTO_DIR, "pontiac_gto.bin")
        _download_if_missing(GTO_GLTF_URL, gto_gltf, min_bytes=50000, force=force)
        _download_if_missing(GTO_BIN_URL, gto_bin, min_bytes=10000000, force=force)
        if force or not (os.path.isfile(GTO_MERGED) and os.path.getsize(GTO_MERGED) > 1000000):
            v, t, p = _merge_research_car_gltf(
                gto_gltf, GTO_MERGED, "SM_Ext_Vandal440R",
                skip_mats=("ground", "whiterubber"),
                wheel_filter_mode="gto", yaw_y_deg=180.0, target_len_m=4.74,
            )
            print("BN: staged CC0 Pontiac GTO (Vandal 440R) -> %d verts, %d tris, %d slots" % (v, t, p))
    except Exception as exc:
        print("BN: CC0 Pontiac GTO offline/skipped: %s" % exc, file=sys.stderr)

    # 4. CC-BY 4.0 BMW X5 2018 G05 SUV (Tauron V6 Rally + Heavy Rhino Police SUV)
    try:
        for rel, _mir in G05_PARTS:
            _download_if_missing(G05_BASE_URL + rel, os.path.join(G05_DIR, os.path.basename(rel)), min_bytes=100000, force=force)
        if force or not (os.path.isfile(G05_RALLY_MERGED) and os.path.getsize(G05_RALLY_MERGED) > 1000000):
            v, t, p = _merge_bmw_g05_suv(G05_RALLY_MERGED, "SM_Ext_TauronV6Rally", tactical_black=False)
            print("BN: staged CC-BY BMW X5 (Tauron V6 Rally) -> %d verts, %d tris, %d slots" % (v, t, p))
        if force or not (os.path.isfile(G05_RHINO_MERGED) and os.path.getsize(G05_RHINO_MERGED) > 1000000):
            v, t, p = _merge_bmw_g05_suv(G05_RHINO_MERGED, "SM_Ext_HeavyRhino", tactical_black=True)
            print("BN: staged CC-BY BMW X5 (Heavy Rhino SUV) -> %d verts, %d tris, %d slots" % (v, t, p))
    except Exception as exc:
        print("BN: CC-BY BMW X5 G05 offline/skipped: %s" % exc, file=sys.stderr)


def _ensure_lean_lods(mesh, sm_sub, target_tris, final_name):
    import unreal

    try:
        ns = mesh.get_editor_property("nanite_settings")
        if ns.get_editor_property("enabled"):
            ns.set_editor_property("enabled", False)
            mesh.set_editor_property("nanite_settings", ns)
    except Exception:
        pass

    cur_tris = 0
    try:
        cur_tris = int(mesh.get_num_triangles(0))
    except Exception:
        cur_tris = 0
    if cur_tris <= int(target_tris * 1.25):
        return cur_tris, False
    try:
        pct = max(0.025, min(0.85, float(target_tris) / float(cur_tris)))
        red0 = unreal.StaticMeshReductionSettings(percent_triangles=pct, screen_size=1.0)
        red1 = unreal.StaticMeshReductionSettings(percent_triangles=max(0.010, pct * 0.35), screen_size=0.35)
        opts = unreal.StaticMeshReductionOptions(reduction_settings=[red0, red1], auto_compute_lod_screen_size=True)
        sm_sub.set_lods(mesh, opts)
        after_tris = int(mesh.get_num_triangles(0))
        unreal.log("BN: decimated %s (%d -> %d tris, pct=%.3f)" % (final_name, cur_tris, after_tris, pct))
        return after_tris, True
    except Exception as exc:
        unreal.log_warning("BN: LOD reduction skipped on %s: %s" % (final_name, exc))
        return cur_tris, False


def _import_single_vehicle(src_file, dest_dir, final_name, force_reimport=False, strip_logo=False, target_tris=32000):
    import unreal

    lib = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    sm_sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    final_path = "%s/%s" % (dest_dir, final_name)

    if not force_reimport and lib.does_asset_exist(final_path):
        existing = unreal.load_asset(final_path)
        if isinstance(existing, unreal.StaticMesh):
            slots = len(existing.get_editor_property("static_materials"))
            if slots >= 3:
                after_tris, changed = _ensure_lean_lods(existing, sm_sub, target_tris, final_name)
                if changed:
                    lib.save_loaded_asset(existing, only_if_is_dirty=False)
                return True

    if not lib.does_directory_exist(dest_dir):
        lib.make_directory(dest_dir)

    sub_dir = "%s/%s_Src" % (dest_dir, final_name)
    if lib.does_directory_exist(sub_dir):
        lib.delete_directory(sub_dir)
    lib.make_directory(sub_dir)

    task = unreal.AssetImportTask()
    task.filename = src_file
    task.destination_path = sub_dir
    task.destination_name = final_name
    task.replace_existing = True
    task.automated = True
    task.save = True
    tools.import_asset_tasks([task])

    meshes = []
    for asset_path in lib.list_assets(sub_dir, recursive=True, include_folder=False):
        obj = unreal.load_asset(asset_path)
        if isinstance(obj, unreal.StaticMesh):
            score = 1
            try:
                score = obj.get_num_triangles(0)
            except Exception:
                score = len(obj.get_editor_property("static_materials")) * 1000
            meshes.append((score, obj))

    if not meshes:
        unreal.log_warning("BN: no StaticMesh imported from %s" % src_file)
        return False

    meshes.sort(key=lambda pair: pair[0], reverse=True)
    raw_tris, hero_mesh = meshes[0]

    # Disable Nanite for 4 GB VRAM safety and decimate dense CAD/PBRT meshes cleanly to ~target_tris
    try:
        ns = hero_mesh.get_editor_property("nanite_settings")
        ns.set_editor_property("enabled", False)
        hero_mesh.set_editor_property("nanite_settings", ns)
    except Exception:
        pass

    try:
        if raw_tris > target_tris * 1.15:
            pct = max(0.035, min(0.85, float(target_tris) / float(raw_tris)))
            red0 = unreal.MeshReductionSettings()
            red0.set_editor_property("percent_triangles", pct)
            red1 = unreal.MeshReductionSettings()
            red1.set_editor_property("percent_triangles", max(0.012, pct * 0.35))
            opts = unreal.StaticMeshReductionOptions()
            opts.set_editor_property("reduction_settings", [red0, red1])
            opts.set_editor_property("auto_compute_lod_screen_size", True)
            sm_sub.set_lods(hero_mesh, opts)
    except Exception as exc:
        unreal.log_warning("BN: LOD reduction skipped on %s: %s" % (final_name, exc))

    if strip_logo:
        dark_trim = unreal.load_asset("/Game/World/Materials/MI_Metal_Dark")
        if dark_trim:
            mats = list(hero_mesh.get_editor_property("static_materials"))
            changed = False
            for slot in mats:
                sname = str(slot.material_slot_name).lower()
                mname = slot.material_interface.get_name().lower() if slot.material_interface else ""
                if any(k in sname or k in mname for k in ("logo", "badge", "emblem", "license")):
                    slot.material_interface = dark_trim
                    changed = True
            if changed:
                hero_mesh.set_editor_property("static_materials", mats)

    if lib.does_asset_exist(final_path):
        lib.delete_asset(final_path)
    src_path = hero_mesh.get_path_name().split(".")[0]
    if src_path != final_path:
        lib.rename_asset(src_path, final_path)

    saved = unreal.load_asset(final_path)
    if saved:
        final_tris, _ = _ensure_lean_lods(saved, sm_sub, target_tris, final_name)
        lib.save_loaded_asset(saved, only_if_is_dirty=False)
        lib.save_directory(dest_dir, only_if_is_dirty=False, recursive=True)
        nslots = len(saved.get_editor_property("static_materials"))
        unreal.log("BN: verified external vehicle mesh %s (%d -> %d tris, %d slots)" % (
            final_path, raw_tris, final_tris, nslots
        ))
        return True
    return False


def import_to_ue(force_reimport=False):
    import unreal

    count = 0
    src_gt = KHRONOS_CAR_MERGED if os.path.isfile(KHRONOS_CAR_MERGED) else KHRONOS_CAR_LOCAL
    if os.path.isfile(src_gt):
        if _import_single_vehicle(src_gt, UE_GT_DIR, "SM_BN_GrandTourer",
                                  force_reimport=force_reimport, strip_logo=True, target_tris=28000):
            count += 1

    # User drop-in folder ALWAYS wins over automatic CC0/CC-BY defaults
    imported_names = set()
    if os.path.isdir(DROPIN_DIR):
        for stem, ue_name in DROPIN_MAP:
            for ext in (".glb", ".gltf", ".fbx"):
                cand = os.path.join(DROPIN_DIR, stem + ext)
                if os.path.isfile(cand):
                    if _import_single_vehicle(cand, UE_EXT_DIR, ue_name, force_reimport=force_reimport):
                        count += 1
                        imported_names.add(ue_name)
                    break

    for _stem, ue_name, merged_glb, target_tris in AUTO_EXTERNAL_DEFAULTS:
        if ue_name in imported_names or not os.path.isfile(merged_glb):
            continue
        if _import_single_vehicle(merged_glb, UE_EXT_DIR, ue_name,
                                  force_reimport=force_reimport, strip_logo=True, target_tris=target_tris):
            count += 1

    unreal.log("BN: ensured %d external vehicle mesh(es)" % count)
    return count


if __name__ == "__main__":
    if "--stage-only" in sys.argv:
        stage_files(force="--force" in sys.argv)
    else:
        import_to_ue(force_reimport="--force" in sys.argv)
