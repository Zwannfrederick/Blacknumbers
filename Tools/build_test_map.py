"""Rebuilds /Game/Maps/TestTrack: concrete lot with an asphalt start straight (vehicle tests), ramps, slalom,
dirt and grass test strips, and a street loop with hairpins, sidewalks, lamps, buildings and props.
Everything is lit by ABNDayNightCycle. Materials come from bn_materials.py.

Run headless (see README):
  UnrealEditor-Cmd BlackNumbers.uproject -run=pythonscript -script=Tools/build_test_map.py -unattended -nullrhi

Layout (cm, +X forward from the player start, +Y to the right):
  y = 0         asphalt start straight, x -19000..19000 (drive test runway, keep empty)
  y = +6000     ramp lane            y = -6000   slalom lane
  y = +11000    dirt strip           y = +17000  grass strip   (surface test starts at x = -24000)
  x = +25000    asphalt skidpad x 20500..29500, y -9000..9000  (drift test starts at y = -8000 heading +Y)
  y = -17000    street loop centre: straights x -9000..9000, hairpins of radius 4500
"""
import math
import os
import random
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bn_materials  # noqa: E402

MAP_PATH = "/Game/Maps/TestTrack"
CUBE = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")  # 1 m, centred pivot, simple collision
CYLINDER = unreal.load_asset("/Engine/BasicShapes/Cylinder.Cylinder")
CONE = unreal.load_asset("/Engine/BasicShapes/Cone.Cone")

LOOP_CY = -17000.0
LOOP_HALF = 9000.0  # half length of each straight
LOOP_R = 4500.0
ROAD_W = 1400.0
CURVE_SEGMENTS = 24
ROAD_TOP = 2.0
SIDEWALK_TOP = 15.0

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
MI, PM = bn_materials.build()
counters = {}


def spawn(cls, label, location=(0, 0, 0), yaw=0.0, pitch=0.0, folder=None):
    actor = actors.spawn_actor_from_class(
        cls, unreal.Vector(*location), unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))
    counters[label] = counters.get(label, 0) + 1
    actor.set_actor_label("%s_%d" % (label, counters[label]) if counters[label] > 1 else label)
    if folder:
        actor.set_folder_path(folder)
    return actor


def block(label, centre, size_m, material, physical=None, collision=True, yaw=0.0, pitch=0.0,
          mesh=CUBE, folder=None):
    actor = spawn(unreal.StaticMeshActor, label, centre, yaw, pitch, folder)
    comp = actor.static_mesh_component
    comp.set_static_mesh(mesh)
    comp.set_material(0, MI[material])
    if physical:
        comp.set_phys_material_override(PM[physical])
    if not collision:
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        comp.set_editor_property("cast_shadow", False)
    actor.set_actor_scale3d(unreal.Vector(*size_m))
    return actor


def slab(label, x0, x1, y0, y1, top, material, physical, folder):
    """Flat surface whose top sits at `top` cm; 2 cm thick so it never z-fights the ground."""
    block(label, ((x0 + x1) / 2, (y0 + y1) / 2, top - 1), ((x1 - x0) / 100, (y1 - y0) / 100, 0.02),
          material, physical, folder=folder)


# ---------------------------------------------------------------- loop geometry
def loop_pieces():
    """Clockwise loop as (kind, data). Outward normal always points away from the loop centre."""
    north, south = LOOP_CY + LOOP_R, LOOP_CY - LOOP_R
    return [
        ("straight", (-LOOP_HALF, north, LOOP_HALF, north, 0.0, 1.0)),
        ("arc", (LOOP_HALF, math.radians(90), math.radians(-90))),
        ("straight", (LOOP_HALF, south, -LOOP_HALF, south, 0.0, -1.0)),
        ("arc", (-LOOP_HALF, math.radians(-90), math.radians(-270))),
    ]


def loop_samples(step):
    """Points every `step` cm along the centreline: (x, y, heading_deg, normal_x, normal_y)."""
    for kind, data in loop_pieces():
        if kind == "straight":
            x0, y0, x1, y1, nx, ny = data
            length = math.hypot(x1 - x0, y1 - y0)
            heading = math.degrees(math.atan2(y1 - y0, x1 - x0))
            for i in range(int(length // step)):
                t = (i + 0.5) * step / length
                yield x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, heading, nx, ny
        else:
            cx, a0, a1 = data
            count = int(abs(a1 - a0) * LOOP_R // step)
            for i in range(count):
                a = a0 + (a1 - a0) * (i + 0.5) / count
                yield (cx + LOOP_R * math.cos(a), LOOP_CY + LOOP_R * math.sin(a),
                       math.degrees(a) - 90, math.cos(a), math.sin(a))


def strip(label, offset, width, top, thickness, material, physical=None, collision=True, folder=None):
    """Continuous strip parallel to the loop centreline at `offset` cm outward."""
    z = top - thickness / 2
    for kind, data in loop_pieces():
        if kind == "straight":
            x0, y0, x1, y1, nx, ny = data
            heading = math.degrees(math.atan2(y1 - y0, x1 - x0))
            length = math.hypot(x1 - x0, y1 - y0)
            centre = ((x0 + x1) / 2 + nx * offset, (y0 + y1) / 2 + ny * offset, z)
            block(label, centre, (length / 100, width / 100, thickness / 100), material, physical,
                  collision, yaw=heading, folder=folder)
            continue
        cx, a0, a1 = data
        radius = LOOP_R + offset
        step = (a1 - a0) / CURVE_SEGMENTS
        # Chord at the strip's outer edge plus overlap, so the outside of the bend has no gaps.
        chord = 2 * (radius + width / 2) * math.sin(abs(step) / 2) * 1.08
        for i in range(CURVE_SEGMENTS):
            a = a0 + step * (i + 0.5)
            centre = (cx + radius * math.cos(a), LOOP_CY + radius * math.sin(a), z)
            block(label, centre, (chord / 100, width / 100, thickness / 100), material, physical,
                  collision, yaw=math.degrees(a) - 90, folder=folder)


def build_street():
    road = "Street/Road"
    strip("Road", 0, ROAD_W, ROAD_TOP, 4, "MI_Asphalt", "PM_Asphalt", folder=road)
    strip("Sidewalk", ROAD_W / 2 + 200, 400, SIDEWALK_TOP, 17, "MI_Concrete", "PM_Concrete", folder=road)
    for side in (-1, 1):
        strip("EdgeLine", side * (ROAD_W / 2 - 40), 15, ROAD_TOP + 0.5, 1, "MI_LineWhite",
              collision=False, folder="Street/Markings")
    for x, y, heading, _, _ in loop_samples(900):
        block("CentreDash", (x, y, ROAD_TOP), (3.0, 0.12, 0.01), "MI_LineYellow",
              collision=False, yaw=heading, folder="Street/Markings")

    # Grass infield: rectangle between the straights plus a disc inside each hairpin.
    inner = LOOP_R - ROAD_W / 2
    slab("Infield", -LOOP_HALF, LOOP_HALF, LOOP_CY - inner, LOOP_CY + inner, 1, "MI_Grass", "PM_Grass", road)
    for cx in (-LOOP_HALF, LOOP_HALF):
        block("InfieldBend", (cx, LOOP_CY, 0), (inner * 2 / 100, inner * 2 / 100, 0.02), "MI_Grass", "PM_Grass",
              mesh=CYLINDER, folder=road)

    # Lamps on the outer kerb, arm over the road; one every 30 m.
    for x, y, _, nx, ny in loop_samples(3000):
        kerb = ROAD_W / 2 + 80
        spawn(unreal.BNStreetLamp, "Lamp", (x + nx * kerb, y + ny * kerb, SIDEWALK_TOP),
              yaw=math.degrees(math.atan2(-ny, -nx)), folder="Street/Lamps")

    build_props()
    build_buildings()


def build_props():
    props = "Street/Props"
    rng = random.Random(11)
    # Bins and benches on the straights' sidewalks, between lamps.
    for x, y, heading, nx, ny in loop_samples(1500):
        if abs(nx) > 0.01 or rng.random() < 0.4:
            continue
        spot = ROAD_W / 2 + 330
        if rng.random() < 0.5:
            block("Bin", (x + nx * spot, y + ny * spot, SIDEWALK_TOP + 45), (0.5, 0.5, 0.9), "MI_Metal_Dark",
                  mesh=CYLINDER, folder=props)
        else:
            block("Bench", (x + nx * spot, y + ny * spot, SIDEWALK_TOP + 22), (1.8, 0.5, 0.45), "MI_Concrete",
                  yaw=heading, folder=props)
    # Red barriers outside the hairpins, cones on the apex.
    for kind, data in loop_pieces():
        if kind != "arc":
            continue
        cx, a0, a1 = data
        for i in range(32):
            a = a0 + (a1 - a0) * (i + 0.5) / 32
            r = LOOP_R + ROAD_W / 2 + 500
            block("Barrier", (cx + r * math.cos(a), LOOP_CY + r * math.sin(a), 40), (2.0, 0.5, 0.8),
                  "MI_Barrier_Red", yaw=math.degrees(a) - 90, folder=props)
        for i in range(5):
            a = (a0 + a1) / 2 + (i - 2) * 0.15
            r = LOOP_R - ROAD_W / 2 - 60
            block("Cone", (cx + r * math.cos(a), LOOP_CY + r * math.sin(a), 35), (0.4, 0.4, 0.7),
                  "MI_Barrier_Red", mesh=CONE, folder=props)


def build_buildings():
    rng = random.Random(7)
    finishes = ["MI_Building_Brick", "MI_Building_Plaster", "MI_Building_Steel"]
    # North row stays clear of the slalom lane (y > -7000 is off limits); south row runs to the lot edge.
    rows = [(-11000.0, -7200.0), (-23000.0, -29000.0)]
    for near, far in rows:
        x = -12000.0
        while x < 12000.0:
            width = rng.uniform(1800, 3500)
            depth = rng.uniform(1500, abs(far - near))
            height = rng.uniform(800, 4000)
            y = near + math.copysign(depth / 2, far - near)
            block("Building", (x + width / 2, y, height / 2), (width / 100, depth / 100, height / 100),
                  rng.choice(finishes), "PM_Concrete", folder="Street/Buildings")
            x += width + rng.uniform(600, 1000)
    # Warehouses beyond the hairpins.
    for sign in (-1, 1):
        block("Warehouse", (sign * 20000, LOOP_CY, 600), (60, 50, 12), "MI_Building_Steel", "PM_Concrete",
              folder="Street/Buildings")


# ---------------------------------------------------------------- assets
def prepare_car():
    """The template car ships with Nanite on. Vulkan SM5 and low-end GPUs cannot draw Nanite skeletal
    meshes (no fallback), so the car rendered invisible: build it as a classic skeletal mesh instead."""
    car = unreal.load_asset("/Game/Vehicles/SportsCar/SKM_SportsCar")
    settings = car.get_editor_property("nanite_settings")
    if settings.get_editor_property("enabled"):
        settings.set_editor_property("enabled", False)
        car.set_editor_property("nanite_settings", settings)
        if not unreal.EditorAssetLibrary.save_loaded_asset(car, only_if_is_dirty=False):
            raise RuntimeError("Could not save SKM_SportsCar")
        unreal.log("BN: disabled Nanite on SKM_SportsCar")


# ---------------------------------------------------------------- level
def reset_level():
    # The map is the editor startup map, so it cannot be deleted while loaded: empty it instead.
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        if not levels.load_level(MAP_PATH):
            raise RuntimeError("Could not load " + MAP_PATH)
        old = [a for a in actors.get_all_level_actors() if not isinstance(a, unreal.WorldSettings)]
        if old and not actors.destroy_actors(old):
            raise RuntimeError("Could not clear " + MAP_PATH)
    elif not levels.new_level(MAP_PATH, False):
        raise RuntimeError("Could not create " + MAP_PATH)


def build_test_areas():
    areas = "TestAreas"
    block("Ground", (0, 0, -50), (600, 600, 1), "MI_Concrete", "PM_Concrete", folder=areas)
    slab("Runway", -19500, 19500, -700, 700, ROAD_TOP, "MI_Asphalt", "PM_Asphalt", areas)
    slab("DirtStrip", -25000, 25000, 9000, 13000, 1, "MI_Dirt", "PM_Dirt", areas)
    slab("GrassStrip", -25000, 25000, 15000, 19000, 1, "MI_Grass", "PM_Grass", areas)
    slab("Skidpad", 20500, 29500, -9000, 9000, ROAD_TOP, "MI_Asphalt", "PM_Asphalt", areas)
    block("Ramp_Small", (-6000, 6000, 0), (12, 8, 1), "MI_Concrete", "PM_Concrete", pitch=10.0, folder=areas)
    block("Ramp_Big", (4000, 6000, 50), (16, 10, 1), "MI_Concrete", "PM_Concrete", pitch=16.0, folder=areas)
    for i in range(8):
        block("Slalom", (-12000 + i * 2500, -6000 + (i % 2) * 700, 150), (1, 1, 3), "MI_Barrier_Red",
              "PM_Concrete", folder=areas)


def build_lighting():
    def movable_light(label, cast_shadows):
        light = spawn(unreal.DirectionalLight, label, (0, 0, 1000), folder="Lighting")
        light.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        light.root_component.set_editor_property("cast_shadows", cast_shadows)
        return light

    sun = movable_light("Sun", True)
    moon = movable_light("Moon", False)
    spawn(unreal.SkyAtmosphere, "SkyAtmosphere", folder="Lighting")
    sky_light = spawn(unreal.SkyLight, "SkyLight", folder="Lighting")
    sky_light.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    fog = spawn(unreal.ExponentialHeightFog, "HeightFog", folder="Lighting")
    post = spawn(unreal.PostProcessVolume, "GlobalGrade", folder="Lighting")
    post.set_editor_property("unbound", True)

    cycle = spawn(unreal.BNDayNightCycle, "DayNightCycle", folder="Lighting")
    for prop, actor in (("sun", sun), ("moon", moon), ("sky_light", sky_light), ("fog", fog), ("post_process", post)):
        cycle.set_editor_property(prop, actor)


prepare_car()
reset_level()
build_test_areas()
build_street()
build_lighting()
spawn(unreal.PlayerStart, "PlayerStart", (-18000, 0, 100))  # the only start: drive tests use the runway

for lamp in [a for a in actors.get_all_level_actors() if isinstance(a, unreal.BNStreetLamp)]:
    lamp.get_editor_property("glow").set_material(0, MI["MI_LampGlow"])
    for part in ("pole", "arm"):
        lamp.get_editor_property(part).set_material(0, MI["MI_Metal_Dark"])

if not levels.save_current_level():
    raise RuntimeError("Could not save " + MAP_PATH)
unreal.log("BN: built %s with %d actors" % (MAP_PATH, len(actors.get_all_level_actors())))
