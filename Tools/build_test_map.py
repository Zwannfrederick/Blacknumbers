"""Rebuilds /Game/Maps/TestTrack: a flat driving pad with a clear start straight, ramps and a slalom, lit by ABNDayNightCycle.

Run headless (see README):
  UnrealEditor-Cmd BlackNumbers.uproject -run=pythonscript -script=Tools/build_test_map.py -unattended -nullrhi
"""
import unreal

MAP_PATH = "/Game/Maps/TestTrack"
CUBE = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")  # 1 m cube, has collision
GRID = unreal.load_asset("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial")

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def spawn(cls, label, location=(0, 0, 0), pitch=0.0, yaw=0.0):
    actor = actors.spawn_actor_from_class(
        cls, unreal.Vector(*location), unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))
    actor.set_actor_label(label)
    return actor


def block(label, location, scale_m, pitch=0.0, yaw=0.0):
    actor = spawn(unreal.StaticMeshActor, label, location, pitch, yaw)
    mesh = actor.static_mesh_component
    mesh.set_static_mesh(CUBE)
    mesh.set_material(0, GRID)
    actor.set_actor_scale3d(unreal.Vector(*scale_m))
    return actor


def movable_light(cls, label, cast_shadows=True):
    light = spawn(cls, label, (0, 0, 1000))
    component = light.root_component
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_editor_property("cast_shadows", cast_shadows)
    return light


# The map is the editor startup map, so it cannot be deleted while loaded: empty it instead.
if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
    if not levels.load_level(MAP_PATH):
        raise RuntimeError("Could not load " + MAP_PATH)
    old = [a for a in actors.get_all_level_actors() if not isinstance(a, unreal.WorldSettings)]
    if old and not actors.destroy_actors(old):
        raise RuntimeError("Could not clear " + MAP_PATH)
elif not levels.new_level(MAP_PATH, False):
    raise RuntimeError("Could not create " + MAP_PATH)

# 400 m x 400 m pad, top surface at z = 0.
block("Ground", (0, 0, -50), (400, 400, 1))

# Start straight along +X (y = 0) is kept empty: the drive test measures acceleration and body hop on it.
# Jump ramps run in a lane on the right, the slalom in a lane on the left.
block("Ramp_Small", (-6000, 6000, 0), (12, 8, 1), pitch=10.0)
block("Ramp_Big", (4000, 6000, 50), (16, 10, 1), pitch=16.0)
for i in range(8):
    block("Slalom_%d" % i, (-12000 + i * 2500, -6000 + (i % 2) * 700, 150), (1, 1, 3))

# Lighting rig driven by the day/night cycle.
sun = movable_light(unreal.DirectionalLight, "Sun")
moon = movable_light(unreal.DirectionalLight, "Moon", cast_shadows=False)
spawn(unreal.SkyAtmosphere, "SkyAtmosphere")
sky_light = spawn(unreal.SkyLight, "SkyLight")
sky_light.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
fog = spawn(unreal.ExponentialHeightFog, "HeightFog")
post = spawn(unreal.PostProcessVolume, "GlobalGrade")
post.set_editor_property("unbound", True)

cycle = spawn(unreal.BNDayNightCycle, "DayNightCycle")
cycle.set_editor_property("sun", sun)
cycle.set_editor_property("moon", moon)
cycle.set_editor_property("sky_light", sky_light)
cycle.set_editor_property("fog", fog)
cycle.set_editor_property("post_process", post)

spawn(unreal.PlayerStart, "PlayerStart", (-18000, 0, 100))  # 380 m of runway ahead

if not levels.save_current_level():
    raise RuntimeError("Could not save " + MAP_PATH)
unreal.log("BN: built " + MAP_PATH)
