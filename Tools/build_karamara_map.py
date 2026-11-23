"""Builds /Game/Maps/KaramaraSlice (vertical slice of Karamara coastal-industrial city):
- 2x2 city street grid (Merkez Bulvar, Dogu Sanayi Cd., Bati Bulvari, Liman Cd.) + central industrial shortcut
- Raised segmented sidewalks, highway guardrails, elevated Kuzey Viyadugu (3.99 deg ramps, Z=700 cm) + overhead gantries
- Multi-story architectural buildings (podium + setbacks + roof parapets/HVAC + lit night-window facades)
- Eski Liman harbor water basin + 3 steel port cranes + stacked colorful corrugated shipping containers
- Tunnel portals framing Perde 5 Route A (24500, 14000) and Route B (-25000, -24000) escape corridors
- Instanced autumn trees/bushes via HierarchicalInstancedStaticMeshComponent (3 draw calls total)
- 8 SphereReflectionCapture actors + sodium street lamps + day/night cycle + Selin's Garage + 6 Breakers + 36 Breakables + 3 Races.

Run headless via Tools/build_map.sh or:
  UnrealEditor-Cmd BlackNumbers.uproject -run=pythonscript -script=Tools/build_karamara_map.py -unattended -nullrhi
"""
import math
import os
import random
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bn_materials  # noqa: E402

MAP_PATH = "/Game/Maps/KaramaraSlice"
RACE_DIR = "/Game/Races"
CUBE = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
CYLINDER = unreal.load_asset("/Engine/BasicShapes/Cylinder.Cylinder")
SPHERE = unreal.load_asset("/Engine/BasicShapes/Sphere.Sphere")

ROAD_TOP = 2.0
SIDEWALK_TOP = 16.0
ROAD_W = 1600.0
ALLEY_W = 1200.0
VIADUCT_Y = -24000.0
VIADUCT_Z = 700.0

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
MI, PM = bn_materials.build()
counters = {}


def spawn(cls, label, location=(0, 0, 0), yaw=0.0, pitch=0.0, folder=None):
    actor = actors.spawn_actor_from_class(
        cls, unreal.Vector(*location), unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw)
    )
    counters[label] = counters.get(label, 0) + 1
    actor.set_actor_label("%s_%d" % (label, counters[label]) if counters[label] > 1 else label)
    if folder:
        actor.set_folder_path(folder)
    return actor


def block(label, centre, size_m, material, physical=None, collision=True, yaw=0.0, pitch=0.0,
          mesh=CUBE, folder=None, cast_shadow=True):
    actor = spawn(unreal.StaticMeshActor, label, centre, yaw, pitch, folder)
    comp = actor.static_mesh_component
    comp.set_static_mesh(mesh)
    comp.set_material(0, MI[material])
    if physical:
        comp.set_phys_material_override(PM[physical])
    if not collision:
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        comp.set_editor_property("cast_shadow", False)
    elif not cast_shadow:
        comp.set_editor_property("cast_shadow", False)
    actor.set_actor_scale3d(unreal.Vector(*size_m))
    return actor


def road_segment(label, x0, y0, x1, y1, width=ROAD_W, top=ROAD_TOP, folder="Karamara/Roads"):
    length = math.hypot(x1 - x0, y1 - y0)
    yaw = math.degrees(math.atan2(y1 - y0, x1 - x0))
    cx, cy = (x0 + x1) * 0.5, (y0 + y1) * 0.5
    block(label, (cx, cy, top - 2.0), (length / 100.0, width / 100.0, 0.04), "MI_Asphalt", "PM_Asphalt", yaw=yaw, folder=folder)

    nx, ny = -math.sin(math.radians(yaw)), math.cos(math.radians(yaw))
    for side in (-1.0, 1.0):
        off = side * (width * 0.5 - 45.0)
        block("EdgeLine", (cx + nx * off, cy + ny * off, top + 0.4), (length / 100.0, 0.14, 0.01),
              "MI_LineWhite", collision=False, yaw=yaw, folder="Karamara/Markings")

    steps = max(1, int(length // 900.0))
    for i in range(steps):
        t = (i + 0.5) / steps
        block("CentreDash", (x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, top + 0.7), (3.0, 0.12, 0.01),
              "MI_LineYellow", collision=False, yaw=yaw, folder="Karamara/Markings")


def sidewalk_strip(label, x0, y0, x1, y1, width=420.0):
    length = math.hypot(x1 - x0, y1 - y0)
    if length < 100.0:
        return
    yaw = math.degrees(math.atan2(y1 - y0, x1 - x0))
    cx, cy = (x0 + x1) * 0.5, (y0 + y1) * 0.5
    block(label, (cx, cy, SIDEWALK_TOP * 0.5), (length / 100.0, width / 100.0, SIDEWALK_TOP / 100.0),
          "MI_Sidewalk", "PM_Concrete", yaw=yaw, folder="Karamara/Sidewalks")


def reset_level():
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        if not levels.load_level(MAP_PATH):
            raise RuntimeError("Could not load " + MAP_PATH)
        old = [a for a in actors.get_all_level_actors() if not isinstance(a, unreal.WorldSettings)]
        if old and not actors.destroy_actors(old):
            raise RuntimeError("Could not clear " + MAP_PATH)
    elif not levels.new_level(MAP_PATH, False):
        raise RuntimeError("Could not create " + MAP_PATH)


def build_roads_and_viaduct():
    # City ground slabs cut cleanly around Eski Liman recessed harbor basin (X 25500..46000, Y -19000..12500)
    block("Ground_Main", (-8250, -4000, -50), (675.0, 720.0, 1.0), "MI_Concrete", "PM_Concrete", folder="Karamara/Ground")
    block("Ground_NE", (34750, -29500, -50), (185.0, 210.0, 1.0), "MI_Concrete", "PM_Concrete", folder="Karamara/Ground")
    block("Ground_SE", (34750, 22250, -50), (185.0, 195.0, 1.0), "MI_Concrete", "PM_Concrete", folder="Karamara/Ground")

    # 2x2 Karamara city street grid + central industrial shortcut (0.3 cm vertical offset avoids Z-fighting at crossings)
    road_segment("MerkezBulvar", -26000, -14000, 26000, -14000, ROAD_W, ROAD_TOP)
    road_segment("DoguSanayiCd", -26000, 14000, 26000, 14000, ROAD_W, ROAD_TOP)
    road_segment("BatiBulvari", -20000, -26000, -20000, 20000, ROAD_W, ROAD_TOP + 0.3)
    road_segment("LimanCaddesi", 20000, -26000, 20000, 20000, ROAD_W, ROAD_TOP + 0.3)
    road_segment("SanayiAraSokak", 0, -14000, 0, 14000, ALLEY_W, ROAD_TOP + 0.3)

    # Dogu Sanayi container drift courtyard
    block("DriftCourtyard", (10500, 6750, ROAD_TOP - 2.0), (140.0, 95.0, 0.04),
          "MI_Asphalt", "PM_Asphalt", folder="Karamara/Roads")

    # Segmented raised sidewalks along avenues (broken cleanly at intersections, Garage & Drift yard driveways)
    sw_off = ROAD_W * 0.5 + 210.0
    ew_spans = [(-25800, -20900), (-19100, -700), (700, 19100), (20900, 24800)]
    for x0, x1 in ew_spans:
        sidewalk_strip("Sidewalk_MerkezN", x0, -14000 - sw_off, x1, -14000 - sw_off)
        sidewalk_strip("Sidewalk_MerkezS", x0, -14000 + sw_off, x1, -14000 + sw_off)
        sidewalk_strip("Sidewalk_SanayiS", x0, 14000 + sw_off, x1, 14000 + sw_off)

    sanayi_n_spans = [
        (-25800, -20900), (-19100, -16100), (-12900, -700),
        (700, 4200), (6600, 14400), (16800, 19100), (20900, 24600),
    ]
    for x0, x1 in sanayi_n_spans:
        sidewalk_strip("Sidewalk_SanayiN", x0, 14000 - sw_off, x1, 14000 - sw_off)

    ns_spans = [(-22800, -14900), (-13100, 13100), (14900, 19800)]
    for y0, y1 in ns_spans:
        sidewalk_strip("Sidewalk_BatiW", -20000 - sw_off, y0, -20000 - sw_off, y1)
        sidewalk_strip("Sidewalk_BatiE", -20000 + sw_off, y0, -20000 + sw_off, y1)
        sidewalk_strip("Sidewalk_LimanE", 20000 + sw_off, y0, 20000 + sw_off, y1)
    for y0, y1 in [(-22800, -14900), (-13100, 5200), (8200, 13100), (14900, 19800)]:
        sidewalk_strip("Sidewalk_LimanW", 20000 - sw_off, y0, 20000 - sw_off, y1)

    # Elevated Kuzey Viyadugu (Y = -24000): flat aprons, smooth 3.99 deg ramps, Z = 700 cm deck
    road_segment("ViaductApproachW", -27000, VIADUCT_Y, -22000, VIADUCT_Y, ROAD_W)
    road_segment("ViaductApproachE", 22000, VIADUCT_Y, 27000, VIADUCT_Y, ROAD_W)

    deck_thick = 40.0
    block("ViaductDeck", (0, VIADUCT_Y, VIADUCT_Z - deck_thick * 0.5),
          (240.0, ROAD_W / 100.0, deck_thick / 100.0), "MI_Asphalt", "PM_Asphalt", folder="Karamara/Viaduct")
    for side in (-1.0, 1.0):
        ry = VIADUCT_Y + side * (ROAD_W * 0.5 - 30.0)
        block("ViaductRail", (0, ry, VIADUCT_Z + 45.0), (240.0, 0.5, 0.9), "MI_Concrete", "PM_Concrete", folder="Karamara/Viaduct")
        block("ViaductGuardCap", (0, ry, VIADUCT_Z + 96.0), (240.0, 0.24, 0.16), "MI_Guardrail", folder="Karamara/Viaduct")

    for x in range(-10000, 10001, 5000):
        for side in (-500.0, 500.0):
            block("ViaductPier", (x, VIADUCT_Y + side, (VIADUCT_Z - deck_thick) * 0.5),
                  (1.6, 1.6, (VIADUCT_Z - deck_thick) / 100.0), "MI_Concrete", "PM_Concrete", folder="Karamara/Viaduct")
        block("ViaductCrossbeam", (x, VIADUCT_Y, VIADUCT_Z - deck_thick - 35.0),
              (1.9, 13.5, 0.7), "MI_Concrete", "PM_Concrete", folder="Karamara/Viaduct")

    span_x = 10000.0
    rise_z = VIADUCT_Z - ROAD_TOP
    slope_len = math.hypot(span_x, rise_z)
    theta = math.atan2(rise_z, span_x)
    pitch_deg = math.degrees(theta)
    half_h = deck_thick * 0.5

    # Shift center down along ramp normal so top plane meets Z=2 and Z=700 seamlessly.
    z_mid = (ROAD_TOP + VIADUCT_Z) * 0.5 - half_h * math.cos(theta)
    dx_norm = half_h * math.sin(theta)
    block("ViaductRampW", (-17000.0 + dx_norm, VIADUCT_Y, z_mid),
          (slope_len / 100.0, ROAD_W / 100.0, deck_thick / 100.0), "MI_Asphalt", "PM_Asphalt",
          pitch=pitch_deg, folder="Karamara/Viaduct")
    block("ViaductRampE", (17000.0 - dx_norm, VIADUCT_Y, z_mid),
          (slope_len / 100.0, ROAD_W / 100.0, deck_thick / 100.0), "MI_Asphalt", "PM_Asphalt",
          pitch=-pitch_deg, folder="Karamara/Viaduct")

    # Sloped steel guardrails along ramp edges (flush on ramp surface, outside drivable width)
    rail_z = (ROAD_TOP + VIADUCT_Z) * 0.5 + 39.0 * math.cos(theta)
    for side in (-1.0, 1.0):
        ry = VIADUCT_Y + side * (ROAD_W * 0.5 - 22.0)
        block("RampGuardW", (-17000.0 - 39.0 * math.sin(theta), ry, rail_z),
              (slope_len / 100.0, 0.30, 0.78), "MI_Guardrail", "PM_Concrete", pitch=pitch_deg, folder="Karamara/Viaduct")
        block("RampGuardE", (17000.0 + 39.0 * math.sin(theta), ry, rail_z),
              (slope_len / 100.0, 0.30, 0.78), "MI_Guardrail", "PM_Concrete", pitch=-pitch_deg, folder="Karamara/Viaduct")

    # Overhead green/amber highway gantries along Kuzey Viyadugu (full vertical clearance >= 620 cm above road)
    for gx, deck_z in ((-22200.0, ROAD_TOP), (-8000.0, VIADUCT_Z), (0.0, VIADUCT_Z), (8000.0, VIADUCT_Z)):
        clr_z = deck_z + 640.0
        for side in (-1.0, 1.0):
            block("GantryPillar", (gx, VIADUCT_Y + side * 1020.0, deck_z + 350.0),
                  (0.55, 0.55, 7.0), "MI_Metal_Dark", folder="Karamara/Gantries")
        block("GantryTruss", (gx, VIADUCT_Y, clr_z + 45.0), (0.55, 21.2, 0.75), "MI_Metal_Dark", folder="Karamara/Gantries")
        block("GantrySign", (gx, VIADUCT_Y - 220.0, clr_z + 45.0), (0.72, 6.2, 1.85), "MI_HighwaySign", collision=False, folder="Karamara/Gantries")
        block("GantrySignSub", (gx, VIADUCT_Y + 340.0, clr_z + 45.0), (0.72, 3.8, 1.55), "MI_HighwaySign", collision=False, folder="Karamara/Gantries")
        for s in (-560.0, 560.0):
            block("GantryBeacon", (gx, VIADUCT_Y + s, clr_z + 145.0), (0.5, 0.5, 0.25), "MI_HazardYellow", collision=False, folder="Karamara/Gantries")


def build_harbor_and_tunnels():
    # --- Eski Liman Harbor Water Basin + Concrete Seawall + 3 Steel Port Cranes + Stacked Containers ---
    block("HarborSeabed", (35750, -3250, -390), (205.0, 315.0, 0.5), "MI_Concrete", folder="Karamara/Harbor")
    block("HarborWater", (35750, -3250, -165), (205.0, 315.0, 0.12), "MI_WaterHarbor", collision=False, folder="Karamara/Harbor")

    # Concrete seawall quay walls & coping
    block("QuayWallW", (25420, -3250, -100), (1.8, 315.0, 2.25), "MI_Concrete", "PM_Concrete", folder="Karamara/Harbor")
    block("QuayWallN", (35750, -18950, -100), (205.0, 1.8, 2.25), "MI_Concrete", "PM_Concrete", folder="Karamara/Harbor")
    block("QuayWallS", (35750, 12450, -100), (205.0, 1.8, 2.25), "MI_Concrete", "PM_Concrete", folder="Karamara/Harbor")
    block("QuayGuardrail", (25260, -3250, 52), (0.35, 306.0, 0.95), "MI_Guardrail", "PM_Concrete", folder="Karamara/Harbor")

    # Harbor safety bollards along pier edge
    for by in range(-11000, 11001, 2200):
        block("QuayBollard", (25020, by, 45), (0.55, 0.55, 0.9), "MI_HazardYellow", mesh=CYLINDER, folder="Karamara/Harbor")

    # 3 towering Steel Port Gantry Cranes overhanging the harbor water
    for cy in (-9000.0, -1200.0, 6800.0):
        for lx in (22500.0, 24800.0):
            for ly_off in (-520.0, 520.0):
                block("CraneLeg", (lx, cy + ly_off, 1050.0), (0.95, 0.95, 21.0), "MI_Container_Amber", "PM_Concrete", folder="Karamara/Harbor")
            block("CranePortalBeam", (lx, cy, 2080.0), (1.1, 11.4, 1.1), "MI_Metal_Dark", folder="Karamara/Harbor")
        # Horizontal boom extending out over the harbor water (X 21600..29400)
        block("CraneBoom", (25500.0, cy, 2160.0), (78.0, 2.2, 1.4), "MI_Container_Amber", folder="Karamara/Harbor")
        block("CraneBackstay", (23650.0, cy, 2550.0), (24.0, 1.4, 0.9), "MI_Metal_Dark", folder="Karamara/Harbor")
        block("CraneCab", (24500.0, cy - 220.0, 1880.0), (2.8, 2.2, 2.4), "MI_Building_Glass", folder="Karamara/Harbor")
        block("CraneHazard", (25500.0, cy, 2040.0), (18.0, 2.4, 0.45), "MI_HazardYellow", collision=False, folder="Karamara/Harbor")
        block("CraneTipLight", (29200.0, cy, 2260.0), (0.6, 0.6, 0.4), "MI_LampGlow", collision=False, folder="Karamara/Harbor")

    # Stacked colorful corrugated shipping containers along Eski Liman quay
    cont_mats = ["MI_Container_Blue", "MI_Container_Red", "MI_Container_Amber"]
    quay_stacks = [
        (22300, -11200, 3), (23500, -11200, 2),
        (22300, -5400, 2), (23500, -5400, 3),
        (22300, 2800, 3), (23500, 2800, 2),
        (22300, 9800, 2), (23500, 9800, 2),
    ]
    for idx, (cx, cy, layers) in enumerate(quay_stacks):
        for layer in range(layers):
            mat = cont_mats[(idx + layer) % len(cont_mats)]
            block("HarborContainer", (cx, cy, 155.0 + layer * 315.0), (2.9, 12.2, 3.1), mat, "PM_Concrete", folder="Karamara/Containers")

    # --- Perde 5 Escape Corridor Tunnel Portals ---
    # Route A: Liman Tunelleri (escape target at (24500, 14000); portal mouth starts east at X = 25800)
    def tunnel_portal(name, mouth_x, center_y, direction):
        # direction = +1 extends east (+X), -1 extends west (-X)
        barrel_len = 2400.0
        bx = mouth_x + direction * (barrel_len * 0.5)
        for side in (-1.0, 1.0):
            # Massive concrete portal walls strictly outside drivable width (clear opening = 1900 cm)
            block(name + "_Wall", (bx, center_y + side * 1120.0, 440.0),
                  (barrel_len / 100.0, 3.4, 8.8), "MI_Concrete", "PM_Concrete", folder="Karamara/Tunnels")
            block(name + "_Wing", (mouth_x - direction * 120.0, center_y + side * 1220.0, 460.0),
                  (2.2, 4.4, 9.2), "MI_Concrete", "PM_Concrete", folder="Karamara/Tunnels")
            block(name + "_Hazard", (mouth_x - direction * 235.0, center_y + side * 1015.0, 220.0),
                  (0.25, 0.6, 3.6), "MI_HazardYellow", collision=False, folder="Karamara/Tunnels")
        # Concrete roof slab & portal arch lintel (clear overhead height = 660 cm)
        block(name + "_Roof", (bx, center_y, 820.0), (barrel_len / 100.0, 26.0, 3.0), "MI_Concrete", "PM_Concrete", folder="Karamara/Tunnels")
        block(name + "_Facade", (mouth_x - direction * 120.0, center_y, 840.0), (2.4, 28.0, 3.4), "MI_Concrete", "PM_Concrete", folder="Karamara/Tunnels")
        block(name + "_Sign", (mouth_x - direction * 245.0, center_y, 830.0), (0.35, 9.5, 1.8), "MI_HighwaySign", collision=False, folder="Karamara/Tunnels")
        # Dark recessed interior tunnel end & warm sodium ceiling light strips
        block(name + "_Back", (mouth_x + direction * (barrel_len - 40.0), center_y, 380.0),
              (0.8, 22.0, 7.6), "MI_Metal_Dark", folder="Karamara/Tunnels")
        for i in range(3):
            lx = mouth_x + direction * (350.0 + i * 750.0)
            block(name + "_Light", (lx, center_y, 662.0), (2.6, 0.9, 0.14), "MI_LampGlow", collision=False, folder="Karamara/Tunnels")

    tunnel_portal("Tunnel_RouteA_Liman", 25800.0, 14000.0, +1.0)
    tunnel_portal("Tunnel_RouteB_Kuzey", -26500.0, VIADUCT_Y, -1.0)


def architectural_building(rng, cx, cy, w, d, h, facade_mat):
    """Multi-story Karamara building: podium + main shaft + setback penthouse + parapet + rooftop HVAC."""
    podium_h = min(460.0, h * 0.28)
    podium_mat = "MI_Concrete" if facade_mat in ("MI_Building_Brick", "MI_Building_Plaster") else "MI_Metal_Dark"
    block("BldgPodium", (cx, cy, podium_h * 0.5), ((w + 160.0) / 100.0, (d + 160.0) / 100.0, podium_h / 100.0),
          podium_mat, "PM_Concrete", folder="Karamara/Buildings")

    shaft_h = h - podium_h
    block("BldgShaft", (cx, cy, podium_h + shaft_h * 0.5), (w / 100.0, d / 100.0, shaft_h / 100.0),
          facade_mat, "PM_Concrete", folder="Karamara/Buildings")

    top_z = h
    top_w, top_d = w, d
    # Taller towers get an architectural upper setback tier
    if h >= 2100.0:
        setback_h = rng.uniform(450.0, 950.0)
        top_w = w * 0.72
        top_d = d * 0.72
        setback_mat = "MI_Building_Glass" if facade_mat != "MI_Building_Glass" else "MI_Building_Steel"
        block("BldgSetback", (cx, cy, h + setback_h * 0.5), (top_w / 100.0, top_d / 100.0, setback_h / 100.0),
              setback_mat, "PM_Concrete", folder="Karamara/Buildings")
        top_z = h + setback_h

    # Rooftop parapet trim + mechanical HVAC housing
    block("BldgParapet", (cx, cy, top_z + 55.0), ((top_w + 80.0) / 100.0, (top_d + 80.0) / 100.0, 1.1),
          "MI_Metal_Dark", folder="Karamara/Buildings")
    hvac_w = min(top_w * 0.34, 620.0)
    hvac_d = min(top_d * 0.30, 520.0)
    block("BldgHVAC", (cx + rng.uniform(-0.18, 0.18) * top_w, cy + rng.uniform(-0.18, 0.18) * top_d, top_z + 195.0),
          (hvac_w / 100.0, hvac_d / 100.0, 2.2), "MI_Concrete", folder="Karamara/Buildings")


def build_buildings_and_lamps():
    rng = random.Random(2026)
    finishes = ["MI_Building_Brick", "MI_Building_Plaster", "MI_Building_Glass", "MI_Building_Steel"]
    waterfront_finishes = ["MI_Building_Glass", "MI_Building_Steel", "MI_Building_Plaster"]

    # Architectural city blocks (strictly clear of roads, sidewalks, Garage (-14500,9500),
    # Race_KaramaraCircuit (-16500,-11800), DriftCourtyard (3500..17500,2000..11500), and Harbor!)
    zones = [
        ((-17600, -2400, -10200, 5200), finishes, 1300.0, 3600.0),
        ((-17600, -2400, -21200, -16400), finishes, 1500.0, 3400.0),
        ((2400, 17600, -21200, -16400), waterfront_finishes, 1800.0, 4200.0),
        ((2400, 17600, -11200, -1600), waterfront_finishes, 1400.0, 3800.0),
        ((-11400, -2400, 7400, 11400), finishes, 1100.0, 2400.0),
        ((-29200, -22800, -11000, 11000), finishes, 1400.0, 3400.0),
        ((-17600, 17600, 16600, 21200), finishes, 1200.0, 2800.0),
    ]
    for (x0, x1, y0, y1), palette, hmin, hmax in zones:
        x = x0 + 500.0
        while x + 2200.0 < x1:
            w = rng.uniform(1900.0, 3100.0)
            d = rng.uniform(1800.0, min(3300.0, y1 - y0 - 700.0))
            h = rng.uniform(hmin, hmax)
            y = rng.uniform(y0 + d * 0.5 + 250.0, y1 - d * 0.5 - 250.0)
            architectural_building(rng, x + w * 0.5, y, w, d, h, rng.choice(palette))
            x += w + rng.uniform(900.0, 1550.0)

    # Selin's Garage architectural workshop hangar framing (-14500, 9500) from behind (Y = 6600, trigger untouched)
    block("GarageHangarBack", (-14500, 6200, 420), (34.0, 3.0, 8.4), "MI_Building_Brick", "PM_Concrete", folder="Karamara/Garage")
    block("GarageHangarSideW", (-16350, 7700, 420), (3.0, 33.0, 8.4), "MI_Building_Brick", "PM_Concrete", folder="Karamara/Garage")
    block("GarageHangarSideE", (-12650, 7700, 420), (3.0, 33.0, 8.4), "MI_Building_Brick", "PM_Concrete", folder="Karamara/Garage")
    block("GarageHangarRoof", (-14500, 7700, 875), (40.0, 34.0, 0.9), "MI_Metal_Dark", "PM_Concrete", folder="Karamara/Garage")
    block("GarageHangarSign", (-14500, 9380, 780), (14.0, 0.4, 1.4), "MI_GarageBeacon", collision=False, folder="Karamara/Garage")

    # Stacked colorful corrugated shipping containers framing Dogu Sanayi Drift courtyard
    cont_palette = ["MI_Container_Red", "MI_Container_Blue", "MI_Container_Amber"]
    for i in range(6):
        cx = 4500 + i * 2200
        mat_n = cont_palette[i % 3]
        mat_s = cont_palette[(i + 1) % 3]
        block("ContainerN", (cx, 12100, 160), (14.0, 3.0, 3.2), mat_n, "PM_Concrete", folder="Karamara/Containers")
        block("ContainerS", (cx, 1300, 160), (14.0, 3.0, 3.2), mat_s, "PM_Concrete", folder="Karamara/Containers")
        if i % 2 == 0:
            block("ContainerN_Top", (cx, 12100, 480), (14.0, 3.0, 3.2), cont_palette[(i + 2) % 3], "PM_Concrete", folder="Karamara/Containers")
        if i % 2 == 1:
            block("ContainerS_Top", (cx, 1300, 480), (14.0, 3.0, 3.2), cont_palette[(i + 2) % 3], "PM_Concrete", folder="Karamara/Containers")

    # Sodium street lamps along Merkez Bulvar, Dogu Sanayi Cd., and Elevated Viaduct
    for x in range(-22000, 22001, 4000):
        spawn(unreal.BNStreetLamp, "Lamp_Merkez", (x, -14000 + ROAD_W * 0.5 + 90.0, SIDEWALK_TOP), yaw=-90.0, folder="Karamara/Lamps")
        spawn(unreal.BNStreetLamp, "Lamp_Sanayi", (x, 14000 - ROAD_W * 0.5 - 90.0, SIDEWALK_TOP), yaw=90.0, folder="Karamara/Lamps")
    for x in range(-10000, 10001, 4000):
        spawn(unreal.BNStreetLamp, "Lamp_Viaduct", (x, VIADUCT_Y + ROAD_W * 0.5 + 60.0, VIADUCT_Z), yaw=-90.0, folder="Karamara/Lamps")


def _add_hism(actor, name, mesh, material, collision=False, cast_shadow=True):
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    actor_handle = None
    root_handle = None
    for h in sds.k2_gather_subobject_data_for_instance(actor):
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h)
        if unreal.SubobjectDataBlueprintFunctionLibrary.is_root_component(data):
            root_handle = h
        elif unreal.SubobjectDataBlueprintFunctionLibrary.is_actor(data):
            actor_handle = h
    if not root_handle:
        root_handle, err = sds.add_new_subobject(unreal.AddNewSubobjectParams(
            parent_handle=actor_handle,
            new_class=unreal.SceneComponent.static_class(),
            blueprint_context=None,
        ))
        if not err.is_empty():
            raise RuntimeError(str(err))
        sds.rename_subobject(handle=root_handle, new_name=unreal.Text("FoliageRoot"))

    comp_handle, err = sds.add_new_subobject(unreal.AddNewSubobjectParams(
        parent_handle=root_handle,
        new_class=unreal.HierarchicalInstancedStaticMeshComponent.static_class(),
        blueprint_context=None,
    ))
    if not err.is_empty():
        raise RuntimeError(str(err))
    sds.rename_subobject(handle=comp_handle, new_name=unreal.Text(name))
    comp_data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(comp_handle)
    comp = unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(comp_data)
    comp.set_static_mesh(mesh)
    comp.set_material(0, material)
    comp.set_editor_property("cast_shadow", cast_shadow)
    if not collision:
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    return comp


def build_foliage_hism():
    """Instanced autumn trees and boulevard bushes (3 HISM components = 3 draw calls total)."""
    rng = random.Random(2027)
    foliage_actor = spawn(unreal.Actor, "Karamara_AutumnFoliage", (0, 0, 0), folder="Karamara/Foliage")
    trunks = _add_hism(foliage_actor, "TrunksHISM", CYLINDER, MI["MI_TreeTrunk"], collision=False, cast_shadow=True)
    canopies = _add_hism(foliage_actor, "AutumnCanopyHISM", SPHERE, MI["MI_FoliageAutumn"], collision=False, cast_shadow=True)
    bushes = _add_hism(foliage_actor, "BushesHISM", SPHERE, MI["MI_FoliageAutumn"], collision=False, cast_shadow=False)

    tree_spots = []
    # Trees along Merkez Bulvar outer/inner verges (avoiding lamps at multiples of 4000 & intersections)
    for x in range(-24000, 24001, 2000):
        if abs(x + 20000) < 1600 or abs(x) < 1500 or abs(x - 20000) < 1600:
            continue
        if abs(x + 16500) < 1400:
            continue  # keep Race_KaramaraCircuit beacon clear
        tree_spots.append((x, -14000 - ROAD_W * 0.5 - 320.0))
        if x % 4000 != 0:
            tree_spots.append((x, -14000 + ROAD_W * 0.5 + 320.0))

    # Trees along Bati Bulvari and Liman Caddesi boulevards
    for y in range(-11500, 12000, 2600):
        tree_spots.append((-20000 - ROAD_W * 0.5 - 320.0, y))
        tree_spots.append((-20000 + ROAD_W * 0.5 + 320.0, y))
        if not (4800 <= y <= 8800):
            tree_spots.append((20000 - ROAD_W * 0.5 - 320.0, y))

    # Trees along Dogu Sanayi outer south sidewalk
    for x in range(-18000, 18001, 2600):
        if abs(x) < 1500:
            continue
        tree_spots.append((x, 14000 + ROAD_W * 0.5 + 320.0))

    trunk_xforms = []
    canopy_xforms = []
    bush_xforms = []

    for tx, ty in tree_spots:
        h_m = rng.uniform(3.4, 5.2)
        r_m = rng.uniform(0.24, 0.34)
        yaw = rng.uniform(0.0, 360.0)
        trunk_xforms.append(unreal.Transform(
            location=unreal.Vector(tx, ty, SIDEWALK_TOP + h_m * 50.0),
            rotation=unreal.Rotator(0.0, 0.0, yaw),
            scale=unreal.Vector(r_m, r_m, h_m),
        ))
        # Multi-lobe sculpted autumn crown (3 slightly offset flattened spheres per tree)
        crown_z = SIDEWALK_TOP + h_m * 100.0
        for dx, dy, dz, sx, sy, sz in (
            (0.0, 0.0, 40.0, 2.7, 2.6, 2.1),
            (65.0, 45.0, -25.0, 2.1, 2.0, 1.6),
            (-60.0, -40.0, -15.0, 2.0, 2.1, 1.7),
        ):
            sc = rng.uniform(0.86, 1.16)
            canopy_xforms.append(unreal.Transform(
                location=unreal.Vector(tx + dx, ty + dy, crown_z + dz),
                rotation=unreal.Rotator(rng.uniform(-6, 6), rng.uniform(-6, 6), rng.uniform(0, 360)),
                scale=unreal.Vector(sx * sc, sy * sc, sz * sc),
            ))
        # Companion low planter bush offset from trunk
        if rng.random() < 0.72:
            bx = tx + rng.choice((-420.0, 420.0)) * (1.0 if abs(ty) > 12500 else 0.0)
            by = ty + (0.0 if abs(ty) > 12500 else rng.choice((-420.0, 420.0)))
            bs = rng.uniform(0.95, 1.45)
            bush_xforms.append(unreal.Transform(
                location=unreal.Vector(bx, by, SIDEWALK_TOP + bs * 32.0),
                rotation=unreal.Rotator(0.0, 0.0, rng.uniform(0.0, 360.0)),
                scale=unreal.Vector(bs * 1.25, bs * 1.1, bs * 0.75),
            ))

    trunks.add_instances(trunk_xforms, False, True)
    canopies.add_instances(canopy_xforms, False, True)
    bushes.add_instances(bush_xforms, False, True)


def build_gameplay_props():
    garage = spawn(unreal.BNGarageZone, "Garage_Selin_Karamara", (-14500, 9500, 0), yaw=90.0, folder="Karamara/Garage")
    garage.set_editor_property("garage_name", "Selin'in Garaji (Yamac)")
    garage.get_editor_property("pillar_l").set_material(0, MI["MI_Metal_Dark"])
    garage.get_editor_property("pillar_r").set_material(0, MI["MI_Metal_Dark"])
    garage.get_editor_property("beacon_beam").set_material(0, MI["MI_GarageBeacon"])

    breakers = [
        ("KaramaraBreaker_MerkezW", (-10000, -14000, 0), 0.0, "Merkez Bulvar Kemeri", 30000),
        ("KaramaraBreaker_MerkezE", (10000, -14000, 0), 0.0, "Carsibasi Tabelasi", 32000),
        ("KaramaraBreaker_SanayiW", (-10000, 14000, 0), 0.0, "Sanayi Iskele Taki", 28000),
        ("KaramaraBreaker_SanayiE", (10000, 14000, 0), 0.0, "Dogu Depo Vinci", 35000),
        ("KaramaraBreaker_AraSokak", (0, 0, 0), 90.0, "Ara Sokak Viyaduk Taki", 26000),
        ("KaramaraBreaker_Liman", (20000, 0, 0), 90.0, "Liman Konteyner Vinci", 45000),
    ]
    for label, loc, yaw, title, cost in breakers:
        b = spawn(unreal.BNPursuitBreaker, label, loc, yaw=yaw, folder="Karamara/Breakers")
        b.set_editor_property("structure_name", title)
        b.set_editor_property("collateral_cost_dollars", cost)
        b.get_editor_property("pillar_l").set_material(0, MI["MI_Metal_Dark"])
        b.get_editor_property("pillar_r").set_material(0, MI["MI_Metal_Dark"])
        b.get_editor_property("overhead_beam").set_material(0, MI["MI_HazardYellow"])

    # Breakable street props placed atop raised kerb edges, shortcut alley, and container lot edges
    prop_idx = 0
    for x in range(-16000, 16001, 4000):
        for y_side, kind, mat, z in [
            (-14000 + ROAD_W * 0.5 + 140.0, unreal.BNBreakableKind.BARRIER, "MI_Barrier_Red", 42.5 + SIDEWALK_TOP),
            (14000 - ROAD_W * 0.5 - 140.0, unreal.BNBreakableKind.BIN, "MI_Metal_Dark", 47.5 + SIDEWALK_TOP),
        ]:
            prop_idx += 1
            p = spawn(unreal.BNBreakableProp, "Prop_%d" % prop_idx, (x, y_side, z), folder="Karamara/Breakables")
            p.set_editor_property("prop_kind", kind)
            p.get_editor_property("prop_mesh").set_material(0, MI[mat])

    for y in range(-10000, 10001, 2500):
        for side in (-1.0, 1.0):
            prop_idx += 1
            p = spawn(unreal.BNBreakableProp, "AlleyCrate_%d" % prop_idx, (side * 420.0, y, 50.0), folder="Karamara/Breakables")
            p.set_editor_property("prop_kind", unreal.BNBreakableKind.CRATE)
            p.get_editor_property("prop_mesh").set_material(0, MI["MI_CrateWood"])


def build_lighting():
    def movable_light(label, cast_shadows):
        light = spawn(unreal.DirectionalLight, label, (0, 0, 1200), folder="Karamara/Lighting")
        light.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        light.root_component.set_editor_property("cast_shadows", cast_shadows)
        return light

    sun = movable_light("Sun", True)
    moon = movable_light("Moon", False)
    spawn(unreal.SkyAtmosphere, "SkyAtmosphere", folder="Karamara/Lighting")
    sky_light = spawn(unreal.SkyLight, "SkyLight", folder="Karamara/Lighting")
    sky_light.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    fog = spawn(unreal.ExponentialHeightFog, "HeightFog", folder="Karamara/Lighting")
    post = spawn(unreal.PostProcessVolume, "GlobalGrade", folder="Karamara/Lighting")
    post.set_editor_property("unbound", True)

    cycle = spawn(unreal.BNDayNightCycle, "DayNightCycle", folder="Karamara/Lighting")
    cycle.set_editor_property("time_of_day", 17.5)
    for prop, actor in (("sun", sun), ("moon", moon), ("sky_light", sky_light), ("fog", fog), ("post_process", post)):
        cycle.set_editor_property(prop, actor)

    # 8 SphereReflectionCapture actors over intersections, drift yard, harbor water, and elevated viaduct
    captures = [
        ("Refl_MerkezWest", (-20000, -14000, 450), 9500.0, 1.1),
        ("Refl_MerkezCenter", (0, -14000, 450), 9500.0, 1.1),
        ("Refl_MerkezEast", (20000, -14000, 450), 9500.0, 1.1),
        ("Refl_SanayiWest", (-20000, 14000, 450), 9500.0, 1.1),
        ("Refl_SanayiEast", (20000, 14000, 450), 9500.0, 1.1),
        ("Refl_DriftCourtyard", (10500, 6750, 500), 9000.0, 1.05),
        ("Refl_HarborWater", (26500, -1500, 650), 15000.0, 1.2),
        ("Refl_ViaductDeck", (0, VIADUCT_Y, VIADUCT_Z + 420.0), 14000.0, 1.15),
    ]
    for label, loc, radius, brightness in captures:
        rc = spawn(unreal.SphereReflectionCapture, label, loc, folder="Karamara/Reflections")
        comp = rc.get_editor_property("capture_component")
        comp.set_editor_property("influence_radius", radius)
        comp.set_editor_property("brightness", brightness)


def gate(x, y, yaw, z=0.0):
    return unreal.Transform(location=unreal.Vector(x, y, z), rotation=unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))


def race_asset(name, title, discipline, grid, checkpoints=(), laps=1, time_limit=60.0, gate_width=2000.0,
               racing_line=(), rivals=()):
    path = "%s/%s" % (RACE_DIR, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        race = unreal.load_asset(path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.BNRaceDefinition)
        race = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, RACE_DIR, unreal.BNRaceDefinition, factory)
        if race is None:
            raise RuntimeError("Could not create " + path)
    race.set_editor_property("display_name", title)
    race.set_editor_property("discipline", discipline)
    race.set_editor_property("laps", laps)
    race.set_editor_property("time_limit_seconds", time_limit)
    race.set_editor_property("grid", grid)
    race.set_editor_property("checkpoints", list(checkpoints))
    race.set_editor_property("gate_width", gate_width)
    race.set_editor_property("racing_line", [unreal.Vector(x, y, z) for x, y, z in racing_line])
    race.set_editor_property("rival_skills", list(rivals))
    if not unreal.EditorAssetLibrary.save_loaded_asset(race, only_if_is_dirty=False):
        raise RuntimeError("Could not save " + path)
    return race


def race_event(label, race, location):
    event = spawn(unreal.BNRaceEvent, label, location, folder="Karamara/Races")
    event.set_editor_property("definition", race)
    event.set_editor_property("start_radius", 450.0)
    event.get_editor_property("beacon").set_material(0, MI["MI_RaceBeacon"])
    for post in event.get_editor_property("gate_posts"):
        post.set_material(0, MI["MI_RaceGate"])


def build_races():
    circuit_line = [
        (-15000, -14000, 0), (0, -14000, 0), (16500, -14000, 0),
        (19200, -12500, 0), (20000, -10000, 0), (20000, 0, 0),
        (20000, 10000, 0), (19200, 12500, 0), (16500, 14000, 0),
        (0, 14000, 0), (-16500, 14000, 0), (-19200, 12500, 0),
        (-20000, 10000, 0), (-20000, 0, 0), (-20000, -10000, 0),
        (-19200, -12500, 0), (-16500, -14000, 0),
    ]
    circuit = race_asset(
        "RACE_KaramaraCircuit", "Karamara Merkez Circuit", unreal.BNRaceDiscipline.CIRCUIT,
        unreal.Transform(location=unreal.Vector(-15000, -14000, 60)),
        [gate(0, -14000, 0), gate(20000, 0, 90), gate(0, 14000, 180), gate(-20000, 0, -90)],
        laps=2, gate_width=2200.0, racing_line=circuit_line, rivals=(0.72, 0.85, 0.96),
    )

    viaduct_sprint = race_asset(
        "RACE_KuzeyViyadukSprint", "Kuzey Viyaduk Sprint", unreal.BNRaceDiscipline.SPRINT,
        unreal.Transform(location=unreal.Vector(-25000, VIADUCT_Y, 60)),
        [gate(-15000, VIADUCT_Y, 0, 490.0), gate(0, VIADUCT_Y, 0, VIADUCT_Z), gate(24500, VIADUCT_Y, 0, 0.0)],
        gate_width=2000.0,
        racing_line=[(-25000, VIADUCT_Y, 0), (28500, VIADUCT_Y, 0)],
        rivals=(0.75, 0.88, 1.0),
    )

    sanayi_drift = race_asset(
        "RACE_SanayiDrift", "Dogu Sanayi Drift", unreal.BNRaceDiscipline.DRIFT,
        unreal.Transform(location=unreal.Vector(10500, 3500, 60), rotation=unreal.Rotator(0, 0, 90)),
        time_limit=60.0,
    )

    race_event("Race_KaramaraCircuit", circuit, (-16500, -11800, 0))
    race_event("Race_KuzeyViyadukSprint", viaduct_sprint, (-25000, VIADUCT_Y + 2800.0, 0))
    race_event("Race_SanayiDrift", sanayi_drift, (5000, 3500, 0))


reset_level()
build_roads_and_viaduct()
build_harbor_and_tunnels()
build_buildings_and_lamps()
build_foliage_hism()
build_gameplay_props()
build_lighting()
build_races()
spawn(unreal.PlayerStart, "PlayerStart", (-24500, -14000, 100))

for lamp in [a for a in actors.get_all_level_actors() if isinstance(a, unreal.BNStreetLamp)]:
    lamp.get_editor_property("glow").set_material(0, MI["MI_LampGlow"])
    for part in ("pole", "arm"):
        lamp.get_editor_property(part).set_material(0, MI["MI_Metal_Dark"])

if not levels.save_current_level():
    raise RuntimeError("Could not save " + MAP_PATH)
unreal.log("BN: built %s with %d actors" % (MAP_PATH, len(actors.get_all_level_actors())))
