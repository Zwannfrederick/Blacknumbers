"""Downloads & caches 19 lean, verified 100% CC0 1.0 Public Domain 3D glTF models from Poly Haven
(https://polyhaven.com/license) under Saved/CC0ModelsCache/, and imports them into UE5 under
/Game/World/Meshes/CC0/ with Nanite OFF, automatic LOD reduction for 4 GB VRAM safety, and
used_with_instanced_static_meshes=True on every material for HISM batching.

Standalone download (system python3):
    python3 Tools/fetch_cc0_models.py --download-only
UE5 import (UnrealEditor-Cmd -run=pythonscript):
    UnrealEditor-Cmd BlackNumbers.uproject -run=pythonscript -script=Tools/fetch_cc0_models.py
"""
import hashlib
import json
import os
import sys
import urllib.request

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CACHE_DIR = os.path.join(PROJECT_DIR, "Saved", "CC0ModelsCache")
STAMP_PATH = os.path.join(CACHE_DIR, ".cc0_models_manifest_hash")
UE_CC0_DIR = "/Game/World/Meshes/CC0"

# All 36 assets are CC0 1.0 Public Domain from Poly Haven (verified 1.6-16.8 MB 1K glTF each).
# Tuple: (ue_mesh_name, polyhaven_id, lod_percent_triangles, default_scale)
CC0_MODELS = [
    ("SM_CC0_ConcreteBarrier",   "concrete_road_barrier",    0.50, 1.0),
    ("SM_CC0_ConcreteBarrierB",  "concrete_road_barrier_02", 0.50, 1.0),
    ("SM_CC0_FireHydrant",       "fire_hydrant",             0.45, 1.0),
    ("SM_CC0_TrashCan",          "metal_trash_can",          0.50, 1.0),
    ("SM_CC0_UtilityBoxA",       "utility_box_01",           0.55, 1.0),
    ("SM_CC0_UtilityBoxB",       "utility_box_02",           0.55, 1.0),
    ("SM_CC0_ManholeCover",      "water_manhole_cover",      0.55, 1.0),
    ("SM_CC0_StreetSeating",     "modular_street_seating",   0.45, 1.0),
    ("SM_CC0_SecurityCamera",    "security_camera_01",       0.55, 1.0),
    ("SM_CC0_AirconUnit",        "exterior_aircon_unit",     0.45, 1.0),
    ("SM_CC0_FireEscape",        "modular_fire_escape",      0.45, 1.0),
    ("SM_CC0_ChainlinkFence",    "modular_chainlink_fence",  0.50, 1.0),
    ("SM_CC0_RollershutterDoor", "rollershutter_door",       0.55, 1.0),
    ("SM_CC0_FloodLights",       "flood_lights",             0.45, 1.0),
    ("SM_CC0_CoveredCar",        "covered_car",              0.45, 1.0),
    ("SM_CC0_OldTyre",           "old_tyre",                 0.50, 1.0),
    ("SM_CC0_Barrel",            "Barrel_01",                0.55, 1.0),
    ("SM_CC0_ToolChest",         "metal_tool_chest",         0.50, 1.0),
    ("SM_CC0_Boulder",           "boulder_01",               0.42, 1.0),
    # --- Expanded Real 3D Architectural Facades, Street Lamps, Urban Props & Shrubs ---
    ("SM_CC0_UrbanApartmentFacade", "modular_urban_apartments_facade", 0.40, 1.0),
    ("SM_CC0_FactoryFacade",        "modular_factory_facade",          0.40, 1.0),
    ("SM_CC0_ShutterWindow",        "rollershutter_window_01",         0.50, 1.0),
    ("SM_CC0_MetalGutter",          "modular_metal_gutter",            0.50, 1.0),
    ("SM_CC0_AirDuct",              "modular_airduct_circular_01",     0.50, 1.0),
    ("SM_CC0_ElectricityPole",      "modular_electricity_poles",       0.45, 1.0),
    ("SM_CC0_IronGate",             "large_iron_gate",                 0.50, 1.0),
    ("SM_CC0_StreetLampA",          "street_lamp_01",                  0.45, 1.0),
    ("SM_CC0_StreetLampB",          "street_lamp_02",                  0.45, 1.0),
    ("SM_CC0_SecurityLight",        "security_light",                  0.50, 1.0),
    ("SM_CC0_TrashBag",             "trashbag",                        0.45, 1.0),
    ("SM_CC0_WoodenCrate",          "wooden_crate_01",                 0.50, 1.0),
    ("SM_CC0_Lifebuoy",             "lifebuoy",                        0.50, 1.0),
    ("SM_CC0_PlanterBox",           "planter_box_01",                  0.50, 1.0),
    ("SM_CC0_ToolCart",             "tool_cart",                       0.45, 1.0),
    ("SM_CC0_WheelRim",             "rusted_wheel_rim_01",             0.45, 1.0),
    ("SM_CC0_Shrub",                "shrub_01",                        0.40, 1.0),
]

UA = {"User-Agent": "BlackNumbers-UE5-CC0ModelFetcher/1.0"}


def _manifest_sig():
    return hashlib.sha256((json.dumps(CC0_MODELS) + "|v2_urban_facades").encode("utf-8")).hexdigest()[:16]


def _fetch_bytes(url, timeout=30):
    req = urllib.request.Request(url, headers=UA)
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return resp.read()


def _download_model(asset_id):
    asset_dir = os.path.join(CACHE_DIR, asset_id)
    gltf_path = os.path.join(asset_dir, asset_id + "_1k.gltf")
    done_marker = os.path.join(asset_dir, ".complete")
    if os.path.isfile(gltf_path) and os.path.isfile(done_marker):
        return gltf_path

    os.makedirs(asset_dir, exist_ok=True)
    meta = json.loads(_fetch_bytes("https://api.polyhaven.com/files/%s" % asset_id, timeout=20).decode("utf-8"))
    gltf_entry = meta.get("gltf", {}).get("1k", {}).get("gltf")
    if not gltf_entry or "url" not in gltf_entry:
        raise RuntimeError("Poly Haven asset %s missing 1k gltf entry" % asset_id)

    main_url = gltf_entry["url"]
    main_name = os.path.basename(main_url.split("?")[0])
    gltf_path = os.path.join(asset_dir, main_name)
    if not os.path.isfile(gltf_path) or os.path.getsize(gltf_path) < 128:
        with open(gltf_path, "wb") as f:
            f.write(_fetch_bytes(main_url, timeout=30))

    includes = gltf_entry.get("include", {})
    for rel_path, info in includes.items():
        url = info.get("url")
        if not url:
            continue
        # Guard against path traversal
        norm_rel = os.path.normpath(rel_path).lstrip(os.sep)
        if norm_rel.startswith(".."):
            continue
        dst = os.path.join(asset_dir, norm_rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        if not os.path.isfile(dst) or os.path.getsize(dst) < 64:
            with open(dst, "wb") as f:
                f.write(_fetch_bytes(url, timeout=35))

    # Create canonical pointer filename if upstream filename differs
    canon = os.path.join(asset_dir, asset_id + "_1k.gltf")
    if canon != gltf_path and not os.path.isfile(canon):
        with open(gltf_path, "rb") as src, open(canon, "wb") as dst_f:
            dst_f.write(src.read())
        gltf_path = canon

    with open(done_marker, "w", encoding="utf-8") as f:
        f.write("ok\n")
    return gltf_path


def download_all(force=False):
    os.makedirs(CACHE_DIR, exist_ok=True)
    sig = _manifest_sig()
    if not force and os.path.isfile(STAMP_PATH):
        with open(STAMP_PATH, "r", encoding="utf-8") as f:
            if f.read().strip() == sig:
                all_ok = all(os.path.isfile(os.path.join(CACHE_DIR, aid, ".complete")) for _, aid, _, _ in CC0_MODELS)
                if all_ok:
                    print("BN: CC0 3D models cache up to date (%d models)" % len(CC0_MODELS))
                    return len(CC0_MODELS)

    ok = 0
    failed = 0
    for ue_name, aid, _, _ in CC0_MODELS:
        try:
            _download_model(aid)
            ok += 1
        except Exception as exc:
            failed += 1
            print("BN: CC0 3D model fallback/skip for %s (%s): %s" % (ue_name, aid, exc), file=sys.stderr)

    if failed == 0:
        with open(STAMP_PATH, "w", encoding="utf-8") as f:
            f.write(sig + "\n")
    print("BN: downloaded/cached %d/%d Poly Haven CC0 1.0 3D glTF models" % (ok, len(CC0_MODELS)))
    return ok


def _get_or_clone_instanced_parent(mat_iface, mel, lib):
    """Returns a writable /Game/World/Materials/ copy of any read-only Engine/Interchange material
    parent chain with used_with_instanced_static_meshes=True so HISM renders real CC0 PBR textures."""
    import unreal

    if mat_iface is None:
        return None
    pkg_path = mat_iface.get_path_name().split(".")[0]
    if pkg_path.startswith("/Game/"):
        if isinstance(mat_iface, unreal.Material):
            if not mat_iface.get_editor_property("used_with_instanced_static_meshes"):
                mat_iface.set_editor_property("used_with_instanced_static_meshes", True)
                mel.recompile_material(mat_iface)
                lib.save_loaded_asset(mat_iface, only_if_is_dirty=False)
        elif isinstance(mat_iface, unreal.MaterialInstanceConstant):
            parent = mat_iface.get_editor_property("parent")
            new_parent = _get_or_clone_instanced_parent(parent, mel, lib)
            if new_parent is not None and new_parent != parent:
                mel.set_material_instance_parent(mat_iface, new_parent)
                mel.update_material_instance(mat_iface)
                lib.save_loaded_asset(mat_iface, only_if_is_dirty=False)
        return mat_iface

    # Read-only Engine / Plugin (/InterchangeAssets/...) material -> clone into /Game/World/Materials/
    dst_path = "/Game/World/Materials/BN_Gltf_" + mat_iface.get_name()
    if lib.does_asset_exist(dst_path):
        cloned = unreal.load_asset(dst_path)
    else:
        cloned = lib.duplicate_asset(pkg_path, dst_path)
    if cloned is None:
        return mat_iface
    if isinstance(cloned, unreal.Material):
        if not cloned.get_editor_property("used_with_instanced_static_meshes"):
            cloned.set_editor_property("used_with_instanced_static_meshes", True)
            mel.recompile_material(cloned)
            lib.save_loaded_asset(cloned, only_if_is_dirty=False)
    elif isinstance(cloned, unreal.MaterialInstanceConstant):
        parent = cloned.get_editor_property("parent")
        new_parent = _get_or_clone_instanced_parent(parent, mel, lib)
        if new_parent is not None and new_parent != parent:
            mel.set_material_instance_parent(cloned, new_parent)
            mel.update_material_instance(cloned)
            lib.save_loaded_asset(cloned, only_if_is_dirty=False)
    return cloned


def _enable_instancing_on_material(mat_iface, mel, lib):
    if mat_iface is None:
        return
    try:
        _get_or_clone_instanced_parent(mat_iface, mel, lib)
    except Exception as exc:
        import unreal
        unreal.log_warning("BN: could not enable InstancedStaticMeshes on %s: %s" % (mat_iface.get_name(), exc))


def _find_gltf_file(asset_id):
    asset_dir = os.path.join(CACHE_DIR, asset_id)
    if not os.path.isdir(asset_dir):
        return None
    canon = os.path.join(asset_dir, asset_id + "_1k.gltf")
    if os.path.isfile(canon):
        return canon
    for fn in sorted(os.listdir(asset_dir)):
        if fn.endswith(".gltf") or fn.endswith(".glb"):
            return os.path.join(asset_dir, fn)
    return None


def import_to_ue(force_reimport=False):
    import unreal

    lib = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mel = unreal.MaterialEditingLibrary
    sm_sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)

    if not lib.does_directory_exist(UE_CC0_DIR):
        lib.make_directory(UE_CC0_DIR)

    imported_count = 0
    existing_count = 0
    for ue_name, aid, lod_frac, _scale in CC0_MODELS:
        final_path = "%s/%s" % (UE_CC0_DIR, ue_name)
        if not force_reimport and lib.does_asset_exist(final_path):
            existing_sm = unreal.load_asset(final_path)
            if isinstance(existing_sm, unreal.StaticMesh):
                for slot in existing_sm.get_editor_property("static_materials"):
                    _enable_instancing_on_material(slot.material_interface, mel, lib)
            existing_count += 1
            continue

        gltf_path = _find_gltf_file(aid)
        if not gltf_path:
            continue

        sub_dir = "%s/%s_Src" % (UE_CC0_DIR, ue_name)
        if lib.does_directory_exist(sub_dir):
            lib.delete_directory(sub_dir)
        lib.make_directory(sub_dir)

        task = unreal.AssetImportTask()
        task.filename = gltf_path
        task.destination_path = sub_dir
        task.destination_name = ue_name
        task.replace_existing = True
        task.automated = True
        task.save = True
        tools.import_asset_tasks([task])

        # Find hero StaticMesh inside sub_dir (largest mesh by bounds * triangle count, skipping tiny brackets/cables)
        best_mesh = None
        best_score = -1.0
        best_tris = 0
        for asset_path in lib.list_assets(sub_dir, recursive=True, include_folder=False):
            obj = unreal.load_asset(asset_path)
            if isinstance(obj, unreal.StaticMesh):
                name_l = obj.get_name().lower()
                tris = 0
                for fn in (lambda o: o.get_num_triangles(0), lambda o: sm_sub.get_number_verts(o, 0)):
                    try:
                        tris = int(fn(obj))
                        if tris > 0:
                            break
                    except Exception:
                        pass
                diag = 1.0
                try:
                    bb = obj.get_bounding_box()
                    ext = bb.max - bb.min
                    diag = max(1.0, (ext.x * ext.x + ext.y * ext.y + ext.z * ext.z) ** 0.5)
                except Exception:
                    pass
                penalty = 0.05 if any(k in name_l for k in ("bracket", "cable", "chain", "cap_", "latch", "wheel", "aged", "bolt", "plug", "coupler", "graffiti", "bracing")) else 1.0
                score = (max(tris, 10) * diag) * penalty
                if score > best_score:
                    best_score = score
                    best_tris = tris
                    best_mesh = obj

        if best_mesh is None:
            unreal.log_warning("BN: no StaticMesh found after importing CC0 %s" % aid)
            continue

        # Disable Nanite, enforce HISM instancing on all materials, and reduce dense meshes for 4 GB VRAM
        try:
            ns = best_mesh.get_editor_property("nanite_settings")
            ns.set_editor_property("enabled", False)
            best_mesh.set_editor_property("nanite_settings", ns)
        except Exception:
            pass

        try:
            if best_tris > 1800 and lod_frac < 0.95:
                red = unreal.StaticMeshReductionSettings(percent_triangles=float(lod_frac), screen_size=1.0)
                opts = unreal.StaticMeshReductionOptions(reduction_settings=[red], auto_compute_lod_screen_size=True)
                sm_sub.set_lods(best_mesh, opts)
        except Exception:
            pass

        try:
            for slot in best_mesh.get_editor_property("static_materials"):
                _enable_instancing_on_material(slot.material_interface, mel, lib)
        except Exception:
            pass

        if lib.does_asset_exist(final_path):
            lib.delete_asset(final_path)
        src_path = best_mesh.get_path_name().split(".")[0]
        if src_path != final_path:
            lib.rename_asset(src_path, final_path)
        saved_mesh = unreal.load_asset(final_path)
        if saved_mesh:
            lib.save_loaded_asset(saved_mesh, only_if_is_dirty=False)
            imported_count += 1

    lib.save_directory(UE_CC0_DIR, only_if_is_dirty=False, recursive=True)
    unreal.log("BN: ensured %d Poly Haven CC0 3D meshes under %s (%d newly imported)" % (
        imported_count + existing_count, UE_CC0_DIR, imported_count
    ))
    return imported_count + existing_count


def load_available_cc0_meshes():
    """Returns dict {ue_name: UStaticMesh} for all imported CC0 meshes (empty if offline/skipped)."""
    import unreal

    lib = unreal.EditorAssetLibrary
    out = {}
    for ue_name, _, _, _ in CC0_MODELS:
        path = "%s/%s" % (UE_CC0_DIR, ue_name)
        if lib.does_asset_exist(path):
            mesh = unreal.load_asset(path)
            if isinstance(mesh, unreal.StaticMesh):
                out[ue_name] = mesh
    return out


if __name__ == "__main__":
    if "--download-only" in sys.argv:
        download_all(force="--force" in sys.argv)
    else:
        import_to_ue(force_reimport="--force" in sys.argv)
