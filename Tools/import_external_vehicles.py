"""Downloads & imports royalty-free external vehicle 3D models into BLACK NUMBERS:
1. Automatic zero-login CC-BY 4.0 Khronos `CarConcept.glb` -> merged single-mesh body
   `/Game/Vehicles/GrandTourer/SM_BN_GrandTourer` (powers Draco RS-T / EBNBodyStyle::GrandTourer
   with realistic clearcoat coupe sculpting, glass, interior, lights, and open wheel wells for
   live physics-rotating Chaos wheels).
2. Optional user drop-in folder `Saved/ExternalVehicles/`:
   Drop any royalty-free CC0 / CC-BY 4.0 / Epic Fab `.glb` or `.fbx` named:
     - street_kurogane.glb  -> /Game/Vehicles/External/SM_Ext_KuroganeMSpec
     - coupe_valken.glb     -> /Game/Vehicles/External/SM_Ext_ValkenRS4
     - muscle_vandal.glb    -> /Game/Vehicles/External/SM_Ext_Vandal440R
     - rally_tauron.glb     -> /Game/Vehicles/External/SM_Ext_TauronV6Rally
     - gt_draco.glb         -> /Game/Vehicles/External/SM_Ext_DracoRST
     - hyper_nocturne.glb   -> /Game/Vehicles/External/SM_Ext_NocturneBN01

Standalone download & GLB body merge (system python3):
    python3 Tools/import_external_vehicles.py --stage-only
UE5 import (UnrealEditor-Cmd -run=pythonscript):
    UnrealEditor-Cmd BlackNumbers.uproject -run=pythonscript -script=Tools/import_external_vehicles.py
"""
import json
import math
import os
import struct
import sys
import urllib.request

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CACHE_DIR = os.path.join(PROJECT_DIR, "Saved", "CC0ModelsCache", "CarConcept")
DROPIN_DIR = os.path.join(PROJECT_DIR, "Saved", "ExternalVehicles")

KHRONOS_CAR_URL = (
    "https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/"
    "main/Models/CarConcept/glTF-Binary/CarConcept.glb"
)
KHRONOS_CAR_LOCAL = os.path.join(CACHE_DIR, "CarConcept.glb")
KHRONOS_CAR_MERGED = os.path.join(CACHE_DIR, "CarConcept_BodyMerged.glb")

UE_GT_DIR = "/Game/Vehicles/GrandTourer"
UE_EXT_DIR = "/Game/Vehicles/External"

DROPIN_MAP = [
    ("street_kurogane", "SM_Ext_KuroganeMSpec"),
    ("coupe_valken",    "SM_Ext_ValkenRS4"),
    ("muscle_vandal",   "SM_Ext_Vandal440R"),
    ("rally_tauron",    "SM_Ext_TauronV6Rally"),
    ("gt_draco",        "SM_Ext_DracoRST"),
    ("hyper_nocturne",  "SM_Ext_NocturneBN01"),
]

DROPIN_README = """BLACK NUMBERS — Opsiyonel Harici Araç İçe Aktarım Klasörü (Saved/ExternalVehicles/)
===================================================================================
Bu klasöre telifsiz (CC0 / CC-BY 4.0 / Epic Fab ücretsiz) `.glb` veya `.fbx` araç dosyaları
bırakıp `Tools/build_map.sh` çalıştırdığında oyun o araç gövdelerini otomatik içe aktarır.

Desteklenen dosya adları:
  - street_kurogane.glb  -> Kurogane M-Spec
  - coupe_valken.glb     -> Valken RS-4 (ör. Sketchfab CC-BY: Generic Coupe "Azura")
  - muscle_vandal.glb    -> Vandal 440R (ör. Sketchfab CC-BY: Phoenix 455 '71)
  - rally_tauron.glb     -> Tauron V6 Rally
  - gt_draco.glb         -> Draco RS-T (varsayılan: otomatik Khronos CarConcept.glb CC-BY 4.0)
  - hyper_nocturne.glb   -> Nocturne BN-01

Doğrulanmış ücretsiz CC-BY 4.0 kurgusal oyun-hazır araç kaynakları:
  - Phoenix 455 '71 (25.9k tri, DanielZhabotinsky):
    https://sketchfab.com/3d-models/none-955edc733c6d44fabc0ad7c246a15896
  - Generic Coupe "Azura" (36.2k tri, mk2design):
    https://sketchfab.com/3d-models/none-2e1a3699de7e4f688d328dc005ef1aac
  - Generic passenger car pack (69.3k tri, comrade1280):
    https://sketchfab.com/3d-models/none-20f9af9b8a404d5cb022ac6fe87f21f5
"""

UA = {"User-Agent": "BlackNumbers-UE5-VehicleFetcher/1.0"}


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


def merge_glb_to_single_mesh(src_glb, dst_glb, strip_wheels=True, yaw_y_deg=90.0):
    """Flattens a multi-node GLB (e.g. Khronos CarConcept's 97 sub-meshes) into a single
    clean glTF 2.0 StaticMesh while preserving all embedded PBR materials & textures.
    Optionally strips static wheel subtrees so UE Chaos physics wheels spin cleanly inside arches."""
    import numpy as np

    with open(src_glb, "rb") as f:
        magic, _ver, _length = struct.unpack("<4sII", f.read(12))
        if magic != b"glTF":
            raise RuntimeError("Invalid GLB header in " + src_glb)
        jlen, _jtype = struct.unpack("<I4s", f.read(8))
        gltf = json.loads(f.read(jlen).decode("utf-8"))
        blen, _btype = struct.unpack("<I4s", f.read(8))
        blob = f.read(blen)

    accessors = gltf.get("accessors", [])
    bviews = gltf.get("bufferViews", [])
    materials = gltf.get("materials", [])

    comp_dtypes = {5120: np.int8, 5121: np.uint8, 5122: np.int16, 5123: np.uint16, 5125: np.uint32, 5126: np.float32}
    type_counts = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}

    def read_acc(idx):
        a = accessors[idx]
        bv = bviews[a["bufferView"]]
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

    # Root rotation around glTF +Y axis (det=+1, zero winding flip) to align car nose with UE +X
    rad = math.radians(yaw_y_deg)
    c, s = math.cos(rad), math.sin(rad)
    root_rot = np.array([
        [ c, 0.0,  s, 0.0],
        [0.0, 1.0, 0.0, 0.0],
        [-s, 0.0,  c, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ], dtype=np.float64)

    skip_node_tokens = ("wheelfront", "wheelrear", "license") if strip_wheels else ("license",)
    skip_mat_tokens = ("tireside", "tiretread", "rim1", "rim2") if strip_wheels else ()

    # Group merged geometry by material index
    buckets = {}  # mat_idx -> list of (pos_f32, nrm_f32, uv0_f32, idx_u32)
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
                mi = pr.get("material", 0)
                if mi is None:
                    mi = 0
                mname = (materials[mi].get("name") or "").lower() if mi < len(materials) else ""
                if any(tok in mname for tok in skip_mat_tokens):
                    continue
                if "license" in mname or "logo" in mname or "emblem" in mname:
                    mi = 0
                attrs = pr.get("attributes", {})
                if "POSITION" not in attrs:
                    continue
                pos = read_acc(attrs["POSITION"]).astype(np.float64)
                pos_w = (pos @ lin.T) + world_m[:3, 3]
                if "NORMAL" in attrs:
                    nrm = read_acc(attrs["NORMAL"]).astype(np.float64) @ nrm_m.T
                    lens = np.linalg.norm(nrm, axis=1, keepdims=True)
                    nrm = nrm / np.maximum(lens, 1e-8)
                else:
                    nrm = np.tile(np.array([[0.0, 1.0, 0.0]]), (len(pos), 1))
                if "TEXCOORD_0" in attrs:
                    uv0 = read_acc(attrs["TEXCOORD_0"]).astype(np.float32)
                else:
                    uv0 = np.zeros((len(pos), 2), dtype=np.float32)
                if "indices" in pr:
                    ind = read_acc(pr["indices"]).astype(np.uint32)
                else:
                    ind = np.arange(len(pos), dtype=np.uint32)
                if det_neg and len(ind) >= 3:
                    ind = ind.reshape(-1, 3)[:, [0, 2, 1]].reshape(-1)
                buckets.setdefault(mi, []).append((pos_w.astype(np.float32), nrm.astype(np.float32), uv0, ind))
        for ch in node.get("children", []):
            walk(ch, world_m)

    scene_idx = gltf.get("scene", 0)
    for root_idx in gltf["scenes"][scene_idx].get("nodes", []):
        walk(root_idx, root_rot)

    if not buckets:
        raise RuntimeError("No geometry survived merge for " + src_glb)

    # Rebuild compact GLB preserving all original images, samplers, textures & materials
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

    for img in gltf.get("images", []):
        new_img = dict(img)
        if "bufferView" in img:
            src_bv = bviews[img["bufferView"]]
            s_off = src_bv.get("byteOffset", 0)
            s_len = src_bv["byteLength"]
            new_img["bufferView"] = push_bytes(blob[s_off:s_off + s_len])
        out_images.append(new_img)

    out_primitives = []
    total_verts = 0
    for mi in sorted(buckets.keys()):
        chunks = buckets[mi]
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
        "nodes": [{"mesh": 0, "name": "SM_BN_GrandTourer"}],
        "meshes": [{"name": "SM_BN_GrandTourer", "primitives": out_primitives}],
        "materials": materials,
        "textures": gltf.get("textures", []),
        "samplers": gltf.get("samplers", []),
        "images": out_images,
        "buffers": [{"byteLength": len(out_bin)}],
        "bufferViews": out_bviews,
        "accessors": out_accessors,
    }
    if "extensionsUsed" in gltf:
        out_gltf["extensionsUsed"] = [e for e in gltf["extensionsUsed"] if e != "KHR_materials_variants"]

    json_bytes = json.dumps(out_gltf, separators=(",", ":")).encode("utf-8")
    while len(json_bytes) % 4 != 0:
        json_bytes += b" "

    total_len = 12 + 8 + len(json_bytes) + 8 + len(out_bin)
    with open(dst_glb, "wb") as f:
        f.write(struct.pack("<4sII", b"glTF", 2, total_len))
        f.write(struct.pack("<I4s", len(json_bytes), b"JSON"))
        f.write(json_bytes)
        f.write(struct.pack("<I4s", len(out_bin), b"BIN\x00"))
        f.write(out_bin)

    return total_verts, len(out_primitives)


def stage_files(force=False):
    os.makedirs(CACHE_DIR, exist_ok=True)
    os.makedirs(DROPIN_DIR, exist_ok=True)
    readme_path = os.path.join(DROPIN_DIR, "README.txt")
    if not os.path.isfile(readme_path):
        with open(readme_path, "w", encoding="utf-8") as f:
            f.write(DROPIN_README)

    if force or not (os.path.isfile(KHRONOS_CAR_LOCAL) and os.path.getsize(KHRONOS_CAR_LOCAL) > 1000000):
        try:
            req = urllib.request.Request(KHRONOS_CAR_URL, headers=UA)
            with urllib.request.urlopen(req, timeout=45) as resp:
                data = resp.read()
            with open(KHRONOS_CAR_LOCAL, "wb") as f:
                f.write(data)
            print("BN: downloaded Khronos CarConcept.glb (%.2f MB, CC-BY 4.0)" % (len(data) / 1048576.0))
        except Exception as exc:
            print("BN: Khronos CarConcept.glb offline/skipped: %s" % exc, file=sys.stderr)
            return
    else:
        print("BN: Khronos CarConcept.glb cached (%.2f MB)" % (os.path.getsize(KHRONOS_CAR_LOCAL) / 1048576.0))

    if force or not (os.path.isfile(KHRONOS_CAR_MERGED) and os.path.getsize(KHRONOS_CAR_MERGED) > 500000):
        verts, prims = merge_glb_to_single_mesh(KHRONOS_CAR_LOCAL, KHRONOS_CAR_MERGED, strip_wheels=True, yaw_y_deg=90.0)
        print("BN: merged Khronos CarConcept body -> %d verts, %d PBR slots (%.2f MB)" % (
            verts, prims, os.path.getsize(KHRONOS_CAR_MERGED) / 1048576.0
        ))


def _import_single_vehicle(src_file, dest_dir, final_name, force_reimport=False, strip_logo=False):
    import unreal

    lib = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    sm_sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    final_path = "%s/%s" % (dest_dir, final_name)

    if not force_reimport and lib.does_asset_exist(final_path):
        existing = unreal.load_asset(final_path)
        if isinstance(existing, unreal.StaticMesh):
            # Reimport if existing asset was an unmerged tiny submesh (< 4 material slots for CarConcept)
            slots = len(existing.get_editor_property("static_materials"))
            if slots >= 4 or final_name != "SM_BN_GrandTourer":
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
                try:
                    score = len(obj.get_editor_property("static_materials")) * 1000
                except Exception:
                    score = 1
            meshes.append((score, obj))

    if not meshes:
        unreal.log_warning("BN: no StaticMesh imported from %s" % src_file)
        return False

    meshes.sort(key=lambda pair: pair[0], reverse=True)
    hero_mesh = meshes[0][1]

    # Disable Nanite for 4 GB VRAM safety and keep polygon count lean (~45% LOD if dense)
    try:
        ns = hero_mesh.get_editor_property("nanite_settings")
        ns.set_editor_property("enabled", False)
        hero_mesh.set_editor_property("nanite_settings", ns)
    except Exception:
        pass

    try:
        if meshes[0][0] > 28000:
            red = unreal.MeshReductionSettings()
            red.set_editor_property("percent_triangles", 0.45)
            opts = unreal.StaticMeshReductionOptions()
            opts.set_editor_property("reduction_settings", [red])
            opts.set_editor_property("auto_compute_lod_screen_size", True)
            sm_sub.set_lods(hero_mesh, opts)
    except Exception:
        pass

    if strip_logo:
        dark_trim = unreal.load_asset("/Game/World/Materials/MI_Metal_Dark")
        if dark_trim:
            mats = list(hero_mesh.get_editor_property("static_materials"))
            changed = False
            for slot in mats:
                sname = str(slot.material_slot_name).lower()
                mname = slot.material_interface.get_name().lower() if slot.material_interface else ""
                if "logo" in sname or "logo" in mname or "badge" in sname or "emblem" in sname or "license" in sname:
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
        lib.save_loaded_asset(saved, only_if_is_dirty=False)
        lib.save_directory(dest_dir, only_if_is_dirty=False, recursive=True)
        nslots = len(saved.get_editor_property("static_materials"))
        unreal.log("BN: verified external vehicle mesh %s (%d tris, %d material slots)" % (
            final_path, meshes[0][0], nslots
        ))
        return True
    return False


def import_to_ue(force_reimport=False):
    import unreal

    count = 0
    src_gt = KHRONOS_CAR_MERGED if os.path.isfile(KHRONOS_CAR_MERGED) else KHRONOS_CAR_LOCAL
    if os.path.isfile(src_gt):
        if _import_single_vehicle(src_gt, UE_GT_DIR, "SM_BN_GrandTourer",
                                  force_reimport=force_reimport, strip_logo=True):
            count += 1

    if os.path.isdir(DROPIN_DIR):
        for stem, ue_name in DROPIN_MAP:
            for ext in (".glb", ".gltf", ".fbx"):
                cand = os.path.join(DROPIN_DIR, stem + ext)
                if os.path.isfile(cand):
                    if _import_single_vehicle(cand, UE_EXT_DIR, ue_name, force_reimport=force_reimport):
                        count += 1
                    break

    unreal.log("BN: ensured %d external vehicle mesh(es)" % count)
    return count


if __name__ == "__main__":
    if "--stage-only" in sys.argv:
        stage_files(force="--force" in sys.argv)
    else:
        import_to_ue(force_reimport="--force" in sys.argv)
