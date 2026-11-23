"""Imports UE5's built-in high-poly OffroadCar and ConceptCar vehicle meshes and
their materials/textures from ~/UnrealEngine/Templates/TemplateResources/Standard/
into BLACK NUMBERS project Content:
  - /Game/Vehicles/OffroadCar/SM_Offroad_Body (+ Materials & Textures)
  - /Game/Vehicles/ConceptCar/SM_AutomotiveTP_Car (+ ConceptCar materials/textures)

Run in two ways:
  1) Standalone staging (copies raw template uassets before UE launches):
       python3 Tools/import_ue_vehicles.py --stage-files
  2) Inside UnrealEditor-Cmd (scans, duplicates ConceptCar to /Game/Vehicles/ConceptCar,
     disables Nanite for Vulkan 4 GB VRAM safety, saves & verifies via EditorAssetLibrary):
       UnrealEditor-Cmd BlackNumbers.uproject -run=pythonscript -script=Tools/import_ue_vehicles.py -unattended -nullrhi
"""
import os
import shutil
import sys

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONTENT_DIR = os.path.join(PROJECT_DIR, "Content")
UE_ROOT = os.environ.get("UE", os.path.expanduser("~/UnrealEngine"))
TEMPLATE_STD = os.path.join(UE_ROOT, "Templates", "TemplateResources", "Standard")

OFFROAD_SRC = os.path.join(TEMPLATE_STD, "Vehicles", "Content", "OffroadCar")
CONCEPT_SRC = os.path.join(TEMPLATE_STD, "ConceptCar", "Content", "Car")

OFFROAD_DEST = os.path.join(CONTENT_DIR, "Vehicles", "OffroadCar")
CONCEPT_STAGE_DEST = os.path.join(CONTENT_DIR, "ConceptCar", "Car")

OFFROAD_UE_MESH = "/Game/Vehicles/OffroadCar/SM_Offroad_Body"
CONCEPT_STAGE_UE_MESH = "/Game/ConceptCar/Car/SM_AutomotiveTP_Car"
CONCEPT_UE_MESH = "/Game/Vehicles/ConceptCar/SM_AutomotiveTP_Car"


def _copy_if_needed(src, dst):
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    if not os.path.isfile(dst) or os.path.getmtime(src) > os.path.getmtime(dst):
        if os.path.isfile(dst):
            os.chmod(dst, 0o644)
        shutil.copy2(src, dst)
        os.chmod(dst, 0o644)
        return True
    if not os.access(dst, os.W_OK):
        os.chmod(dst, 0o644)
    return False


def stage_template_files():
    """Copies OffroadCar and ConceptCar static mesh & material uassets into their native Content paths."""
    copied = 0
    if not os.path.isdir(OFFROAD_SRC):
        raise FileNotFoundError("UE OffroadCar template folder missing: " + OFFROAD_SRC)
    if not os.path.isdir(CONCEPT_SRC):
        raise FileNotFoundError("UE ConceptCar template folder missing: " + CONCEPT_SRC)

    # OffroadCar: copy SM_Offroad_Body, SM_Offroad_Tire, and Materials/Textures (skip AnimBP/SKM template rig).
    for fname in ("SM_Offroad_Body.uasset", "SM_Offroad_Tire.uasset"):
        src = os.path.join(OFFROAD_SRC, fname)
        if os.path.isfile(src) and _copy_if_needed(src, os.path.join(OFFROAD_DEST, fname)):
            copied += 1

    offroad_mats = os.path.join(OFFROAD_SRC, "Materials")
    for root, _dirs, files in os.walk(offroad_mats):
        rel_root = os.path.relpath(root, OFFROAD_SRC)
        for fname in files:
            if not fname.endswith(".uasset"):
                continue
            src = os.path.join(root, fname)
            dst = os.path.join(OFFROAD_DEST, rel_root, fname)
            if _copy_if_needed(src, dst):
                copied += 1

    # ConceptCar: stage under Content/ConceptCar/Car so UE resolves internal /Game/ConceptCar/Car/Assets/* paths.
    for root, _dirs, files in os.walk(CONCEPT_SRC):
        rel_root = os.path.relpath(root, CONCEPT_SRC)
        for fname in files:
            if not fname.endswith(".uasset"):
                continue
            src = os.path.join(root, fname)
            dst = os.path.join(CONCEPT_STAGE_DEST, rel_root, fname)
            if _copy_if_needed(src, dst):
                copied += 1

    return copied


def _disable_nanite(mesh, label):
    """Disables Nanite on imported vehicle static meshes so Vulkan 4 GB VRAM renders cleanly with VSM OFF."""
    try:
        settings = mesh.get_editor_property("nanite_settings")
        if settings and settings.get_editor_property("enabled"):
            settings.set_editor_property("enabled", False)
            mesh.set_editor_property("nanite_settings", settings)
            return True
    except Exception as exc:
        import unreal
        unreal.log_warning("BN: could not inspect nanite_settings on %s: %s" % (label, exc))
    return False


def import_in_ue():
    import unreal

    staged = stage_template_files()
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous(
        ["/Game/Vehicles/OffroadCar", "/Game/ConceptCar", "/Game/Vehicles/ConceptCar"],
        force_rescan=True,
    )

    assets = unreal.EditorAssetLibrary
    if not assets.does_directory_exist("/Game/Vehicles/ConceptCar"):
        assets.make_directory("/Game/Vehicles/ConceptCar")

    # Verify OffroadCar mesh + materials
    if not assets.does_asset_exist(OFFROAD_UE_MESH):
        raise RuntimeError("OffroadCar static mesh not found after staging: " + OFFROAD_UE_MESH)
    offroad_mesh = unreal.load_asset(OFFROAD_UE_MESH)
    if offroad_mesh is None:
        raise RuntimeError("Could not load " + OFFROAD_UE_MESH)
    if _disable_nanite(offroad_mesh, OFFROAD_UE_MESH):
        assets.save_loaded_asset(offroad_mesh, only_if_is_dirty=False)

    # Duplicate ConceptCar from its native staged path -> /Game/Vehicles/ConceptCar/SM_AutomotiveTP_Car
    # so all /Game/ConceptCar/Car/Assets/* PBR materials & flake/smudge textures stay 100% bound.
    if not assets.does_asset_exist(CONCEPT_STAGE_UE_MESH):
        raise RuntimeError("ConceptCar staged mesh not found: " + CONCEPT_STAGE_UE_MESH)
    if not assets.does_asset_exist(CONCEPT_UE_MESH):
        dup = assets.duplicate_asset(CONCEPT_STAGE_UE_MESH, CONCEPT_UE_MESH)
        if dup is None:
            raise RuntimeError("Could not duplicate ConceptCar to " + CONCEPT_UE_MESH)

    concept_mesh = unreal.load_asset(CONCEPT_UE_MESH)
    if concept_mesh is None:
        raise RuntimeError("Could not load " + CONCEPT_UE_MESH)
    if _disable_nanite(concept_mesh, CONCEPT_UE_MESH):
        assets.save_loaded_asset(concept_mesh, only_if_is_dirty=False)
    else:
        assets.save_loaded_asset(concept_mesh, only_if_is_dirty=True)

    # Final verification using EditorAssetLibrary.does_asset_exist + material slot sanity check
    for path, mesh in ((OFFROAD_UE_MESH, offroad_mesh), (CONCEPT_UE_MESH, concept_mesh)):
        if not assets.does_asset_exist(path):
            raise RuntimeError("Verification failed: missing asset " + path)
        slots = mesh.get_editor_property("static_materials")
        valid_mats = sum(1 for s in slots if s.get_editor_property("material_interface") is not None)
        unreal.log("BN: verified vehicle %s (%d/%d material slots bound)" % (path, valid_mats, len(slots)))
        if valid_mats == 0:
            raise RuntimeError("Vehicle mesh has 0 bound materials: " + path)

    unreal.log("BN: built vehicle imports (staged %d files): %s, %s" % (staged, OFFROAD_UE_MESH, CONCEPT_UE_MESH))


if __name__ == "__main__":
    if "--stage-files" in sys.argv:
        n = stage_template_files()
        print("BN: staged %d UE vehicle template files" % n)
    else:
        import_in_ue()
