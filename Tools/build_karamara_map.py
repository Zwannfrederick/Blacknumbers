"""Rebuilds /Game/Maps/KaramaraSlice — BLACK NUMBERS' full ~2.0 km x 2.0 km open-world city
spanning all 6 canon Karamara districts, backed by single-source-of-truth /Game/World/DA_KaramaraRoadGraph
and 9 unique race routes under /Game/Races/.

Run headless via Tools/build_map.sh or directly:
  UnrealEditor-Cmd BlackNumbers.uproject -run=pythonscript -script=Tools/build_karamara_map.py -unattended -nullrhi
"""
import math
import os
import random
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bn_materials  # noqa: E402
import bn_meshes  # noqa: E402
import karamara_network as knet  # noqa: E402
import karamara_scenery as kscene  # noqa: E402

MAP_PATH = "/Game/Maps/KaramaraSlice"
RACE_DIR = "/Game/Races"
GRAPH_DIR = "/Game/World"
GRAPH_NAME = "DA_KaramaraRoadGraph"

CUBE = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
CYLINDER = unreal.load_asset("/Engine/BasicShapes/Cylinder.Cylinder")

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
MI, PM = bn_materials.build()
MESHES = bn_meshes.ensure_ue_meshes()
counters = {}


def spawn(cls, label, location=(0.0, 0.0, 0.0), yaw=0.0, pitch=0.0, roll=0.0, folder=None):
    actor = actors.spawn_actor_from_class(
        cls, unreal.Vector(*location), unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw)
    )
    counters[label] = counters.get(label, 0) + 1
    actor.set_actor_label("%s_%d" % (label, counters[label]) if counters[label] > 1 else label)
    if folder:
        actor.set_folder_path(folder)
    return actor


def block(label, centre, size_m, material, physical=None, collision=True, yaw=0.0, pitch=0.0,
          mesh=CUBE, folder=None):
    actor = spawn(unreal.StaticMeshActor, label, centre, yaw=yaw, pitch=pitch, folder=folder)
    comp = actor.static_mesh_component
    comp.set_static_mesh(mesh)
    comp.set_material(0, MI[material])
    if physical:
        comp.set_phys_material_override(PM[physical])
    if not collision:
        comp.set_collision_profile_name("NoCollision")
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        comp.set_editor_property("cast_shadow", False)
    actor.set_actor_scale3d(unreal.Vector(*size_m))
    return actor


def place_mesh(label, mesh_name, location, yaw=0.0, pitch=0.0, scale=(1.0, 1.0, 1.0),
               collision=True, folder=None):
    actor = spawn(unreal.StaticMeshActor, label, location, yaw=yaw, pitch=pitch, folder=folder)
    comp = actor.static_mesh_component
    comp.set_static_mesh(MESHES[mesh_name])
    if not collision:
        comp.set_collision_profile_name("NoCollision")
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    return actor


def reset_level():
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        levels.load_level(MAP_PATH)
    else:
        levels.new_level(MAP_PATH)
    for actor in list(actors.get_all_level_actors()):
        if not isinstance(actor, (unreal.WorldSettings, unreal.Brush)):
            actors.destroy_actor(actor)
    counters.clear()


def build_road_geometry(nodes, edges):
    """Constructs continuous smooth curved & sloped drivable asphalt/cobblestone/dirt roads,
    sidewalks, viaduct support piers, embankment retaining slopes, and intersection pads."""
    road_f = "RoadNetwork/Surface"
    mark_f = "RoadNetwork/Markings"
    struct_f = "RoadNetwork/Structures"

    # Track widest incident edge per node for clean circular junction pads
    node_width = {nid: 1400.0 for nid in nodes}
    node_mat = {nid: ("MI_Asphalt", "PM_Asphalt") for nid in nodes}

    for e in edges:
        pts = e["points"]
        w = e["width"]
        is_dirt = e["dirt"]
        is_carsi = (e["node_a"] in (8, 9, 10, 11) and e["node_b"] in (8, 9, 10, 11) and abs(pts[0][1] - 16000.0) < 200.0)
        mat = "MI_Dirt" if is_dirt else ("MI_Cobblestone" if is_carsi else "MI_Asphalt")
        phys = "PM_Dirt" if is_dirt else "PM_Asphalt"

        node_width[e["node_a"]] = max(node_width[e["node_a"]], w)
        node_width[e["node_b"]] = max(node_width[e["node_b"]], w)
        if is_dirt:
            node_mat[e["node_a"]] = ("MI_Dirt", "PM_Dirt")
            node_mat[e["node_b"]] = ("MI_Dirt", "PM_Dirt")

        pier_accum = 2200.0
        for i in range(len(pts) - 1):
            a, b = pts[i], pts[i + 1]
            dx, dy, dz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
            dxy = math.hypot(dx, dy)
            seg3d = math.sqrt(dxy * dxy + dz * dz)
            if seg3d < 10.0:
                continue
            yaw = math.degrees(math.atan2(dy, dx))
            pitch = math.degrees(math.atan2(dz, dxy))
            mx, my, mz = 0.5 * (a[0] + b[0]), 0.5 * (a[1] + b[1]), 0.5 * (a[2] + b[2])

            # 45 cm thick drivable road slab overlapped 18% lengthwise so curves & grade crests stay butter-smooth
            slab_len_m = (seg3d * 1.18) / 100.0
            slab_w_m = w / 100.0
            thick_cm = 46.0
            block("RoadSeg", (mx, my, mz - thick_cm * 0.5), (slab_len_m, slab_w_m, thick_cm / 100.0),
                  mat, phys, yaw=yaw, pitch=pitch, folder=road_f)

            # Lane markings (center yellow dashes + white edges on paved roads)
            if not is_dirt:
                block("CenterLine", (mx, my, mz + 1.8), (slab_len_m * 0.52, 0.16, 0.02),
                      "MI_LineYellow", collision=False, yaw=yaw, pitch=pitch, folder=mark_f)
                nx, ny = -dy / dxy, dx / dxy
                for side in (-1.0, 1.0):
                    eo = (w * 0.5 - 55.0) * side
                    block("EdgeLine", (mx + nx * eo, my + ny * eo, mz + 1.5), (slab_len_m, 0.14, 0.02),
                          "MI_LineWhite", collision=False, yaw=yaw, pitch=pitch, folder=mark_f)

                # Raised urban sidewalks along flat Merkez / Sanayi / Liman / Yamac streets
                if not e["highway"] and e["district"] in (knet.DIST_MERKEZ, knet.DIST_SANAYI, knet.DIST_YAMAC):
                    sw_w = 300.0
                    for side in (-1.0, 1.0):
                        so = (w * 0.5 + sw_w * 0.5) * side
                        block("Sidewalk", (mx + nx * so, my + ny * so, mz - 12.0),
                              (slab_len_m, sw_w / 100.0, 0.52), "MI_Sidewalk", "PM_Concrete",
                              yaw=yaw, pitch=pitch, folder=road_f)

            # Structural support under elevated segments (Z > 120 cm)
            if mz > 120.0:
                is_bridge_span = (e["node_a"], e["node_b"]) in ((20, 21), (21, 20))
                if e["highway"] and not is_bridge_span:
                    # Viaduct box-girder underside + piers down to Z = -200 cm every ~45 m
                    block("ViaductGirder", (mx, my, mz - 95.0), (slab_len_m, slab_w_m * 0.86, 1.3),
                          "MI_Concrete", "PM_Concrete", yaw=yaw, pitch=pitch, folder=struct_f)
                    pier_accum += seg3d
                    if pier_accum >= 4500.0:
                        pier_accum = 0.0
                        pier_h = mz + 120.0
                        block("ViaductPier", (mx, my, (mz - 100.0) * 0.5 - 60.0),
                              (2.6, 2.6, pier_h / 100.0), "MI_Concrete", "PM_Concrete",
                              mesh=CYLINDER, folder=struct_f)
                elif not is_bridge_span:
                    # Mountain / hillside solid rock-concrete retaining embankment down to terrain!
                    emb_h = mz + 240.0
                    block("Embankment", (mx, my, mz - 42.0 - emb_h * 0.5),
                          (slab_len_m * 1.04, slab_w_m * 1.24, emb_h / 100.0),
                          "MI_MountainRock" if e["district"] == knet.DIST_GECIT else "MI_Concrete",
                          "PM_Concrete", yaw=yaw, pitch=0.0, folder=struct_f)

    # Circular intersection blend pads at every road node (eliminates T/4-way seam gaps)
    for nid, n in nodes.items():
        x, y, z = n["pos"]
        dia_m = (node_width[nid] * 1.18) / 100.0
        nmat, nphys = node_mat[nid]
        block("JunctionPad", (x, y, z - 22.5), (dia_m, dia_m, 0.45),
              nmat, nphys, mesh=CYLINDER, folder=road_f)


def export_road_graph_asset(nodes, edges):
    """Populates and saves /Game/World/DA_KaramaraRoadGraph (UBNRoadGraphAsset)."""
    path = "%s/%s" % (GRAPH_DIR, GRAPH_NAME)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        asset = unreal.load_asset(path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.BNRoadGraphAsset)
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            GRAPH_NAME, GRAPH_DIR, unreal.BNRoadGraphAsset, factory
        )
        if asset is None:
            raise RuntimeError("Could not create " + path)

    dist_enums = [
        unreal.BNDistrict.MERKEZ_BULVAR,
        unreal.BNDistrict.DOGU_SANAYI,
        unreal.BNDistrict.ESKI_LIMAN,
        unreal.BNDistrict.KUZEY_VIYADUGU,
        unreal.BNDistrict.YAMAC_MAHALLESI,
        unreal.BNDistrict.KUZEY_GECIDI,
    ]

    ue_nodes = []
    for nid in sorted(nodes.keys()):
        n = nodes[nid]
        rn = unreal.BNRoadNode()
        rn.set_editor_property("id", int(nid))
        rn.set_editor_property("position", unreal.Vector(*n["pos"]))
        rn.set_editor_property("district", dist_enums[n["district"]])
        rn.set_editor_property("signalized", bool(n["signalized"]))
        ue_nodes.append(rn)

    ue_edges = []
    for e in edges:
        re = unreal.BNRoadEdge()
        re.set_editor_property("node_a", int(e["node_a"]))
        re.set_editor_property("node_b", int(e["node_b"]))
        re.set_editor_property("sampled_points", [unreal.Vector(*p) for p in e["points"]])
        re.set_editor_property("width_cm", float(e["width"]))
        re.set_editor_property("speed_limit_kmh", float(e["speed"]))
        re.set_editor_property("district", dist_enums[e["district"]])
        re.set_editor_property("highway", bool(e["highway"]))
        re.set_editor_property("one_way", bool(e["one_way"]))
        re.set_editor_property("dirt", bool(e["dirt"]))
        ue_edges.append(re)

    ue_spots = []
    for s in knet.COOLDOWN_SPOTS:
        cs = unreal.BNCooldownSpot()
        cs.set_editor_property("location", unreal.Vector(*s["loc"]))
        cs.set_editor_property("radius_cm", float(s["radius"]))
        cs.set_editor_property("display_name", s["name"])
        cs.set_editor_property("district", dist_enums[s["district"]])
        ue_spots.append(cs)

    ue_dists = []
    for d in knet.DISTRICT_BOUNDS:
        db = unreal.BNDistrictBounds()
        db.set_editor_property("district", dist_enums[d["district"]])
        db.set_editor_property("center_cm", unreal.Vector2D(*d["center"]))
        db.set_editor_property("half_extent_cm", unreal.Vector2D(*d["half"]))
        db.set_editor_property("display_name", d["name"])
        db.set_editor_property("subtitle", d["subtitle"])
        ue_dists.append(db)

    asset.set_editor_property("nodes", ue_nodes)
    asset.set_editor_property("edges", ue_edges)
    asset.set_editor_property("cooldown_spots", ue_spots)
    asset.set_editor_property("districts", ue_dists)
    asset.set_editor_property("escape_route_a_location", unreal.Vector(*knet.ESCAPE_ROUTE_A))
    asset.set_editor_property("escape_route_b_location", unreal.Vector(*knet.ESCAPE_ROUTE_B))

    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Could not save " + path)
    unreal.log("BN: built %s (%d nodes, %d edges, %d cooldown spots)" % (path, len(ue_nodes), len(ue_edges), len(ue_spots)))
    return asset


def build_gameplay_actors(edges):
    """Spawns Selin's Garage, 10 Pursuit Breakers across all 6 districts, and 72 breakable street props."""
    gx, gy, gz, gyaw = knet.SELIN_GARAGE_POS
    garage = spawn(unreal.BNGarageZone, "Garage_Selin", (gx, gy, gz), yaw=gyaw, folder="Gameplay/Garage")
    garage.get_editor_property("pillar_l").set_material(0, MI["MI_Metal_Dark"])
    garage.get_editor_property("pillar_r").set_material(0, MI["MI_Metal_Dark"])
    garage.get_editor_property("beacon_beam").set_material(0, MI["MI_RaceHolo_Garage"])

    # 10 Pursuit Breakers spanning all 6 districts
    for label, x, y, z, yaw, title, cost, style in knet.PURSUIT_BREAKERS:
        b = spawn(unreal.BNPursuitBreaker, label, (x, y, z), yaw=yaw, folder="Gameplay/Breakers")
        b.set_editor_property("structure_name", title)
        b.set_editor_property("collateral_cost_dollars", cost)
        b.set_editor_property("visual_style", int(style))
        b.get_editor_property("pillar_l").set_material(0, MI["MI_Metal_Dark"])
        b.get_editor_property("pillar_r").set_material(0, MI["MI_Metal_Dark"])
        beam_mat = "MI_HazardYellow" if style == 0 else ("MI_Container_Amber" if style == 1 else "MI_Building_Steel")
        b.get_editor_property("overhead_beam").set_material(0, MI[beam_mat])

    # 72 Breakable street props (barriers, metal bins, wooden crates) distributed across city shoulders & drift yard
    rng = random.Random(303)
    kinds = [
        (unreal.BNBreakableKind.BARRIER, "MI_Barrier_Red", 45.0),
        (unreal.BNBreakableKind.BIN,     "MI_Metal_Dark",  48.0),
        (unreal.BNBreakableKind.CRATE,   "MI_CrateWood",   52.0),
    ]
    prop_count = 0
    for e in edges:
        if e["highway"] or len(e["points"]) < 3:
            continue
        mid = e["points"][len(e["points"]) // 2]
        nxt = e["points"][min(len(e["points"]) - 1, len(e["points"]) // 2 + 1)]
        dx, dy = nxt[0] - mid[0], nxt[1] - mid[1]
        l = max(math.hypot(dx, dy), 1.0)
        nx, ny = -dy / l, dx / l
        off = e["width"] * 0.5 + 150.0
        for side in (-1.0, 1.0):
            if prop_count >= 72:
                break
            kind, mat, zoff = kinds[prop_count % len(kinds)]
            prop_count += 1
            p = spawn(
                unreal.BNBreakableProp, "Prop_%d" % prop_count,
                (mid[0] + nx * off * side, mid[1] + ny * off * side, mid[2] + zoff),
                yaw=rng.uniform(0.0, 360.0), folder="Gameplay/Breakables",
            )
            p.set_editor_property("prop_kind", kind)
            p.get_editor_property("prop_mesh").set_material(0, MI[mat])


def build_races(nodes, edges, lookup):
    """Builds all 9 unique Karamara race DataAssets & places holographic start beacons in the world."""
    disc_map = {
        "CIRCUIT": unreal.BNRaceDiscipline.CIRCUIT,
        "SPRINT":  unreal.BNRaceDiscipline.SPRINT,
        "DRIFT":   unreal.BNRaceDiscipline.DRIFT,
    }
    specs = knet.build_race_specs(nodes, edges, lookup)

    for r in specs:
        path = "%s/%s" % (RACE_DIR, r["name"])
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            race = unreal.load_asset(path)
        else:
            factory = unreal.DataAssetFactory()
            factory.set_editor_property("data_asset_class", unreal.BNRaceDefinition)
            race = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                r["name"], RACE_DIR, unreal.BNRaceDefinition, factory
            )
            if race is None:
                raise RuntimeError("Could not create " + path)

        gx, gy, gz, gyaw = r["grid"]
        grid_xf = unreal.Transform(
            location=unreal.Vector(gx, gy, gz),
            rotation=unreal.Rotator(roll=0.0, pitch=0.0, yaw=gyaw),
        )
        gates = [
            unreal.Transform(location=unreal.Vector(x, y, z), rotation=unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
            for x, y, z, yaw in r["checkpoints"]
        ]
        race.set_editor_property("display_name", r["title"])
        race.set_editor_property("discipline", disc_map[r["discipline"]])
        race.set_editor_property("laps", int(r["laps"]))
        race.set_editor_property("time_limit_seconds", float(r["time_limit"]))
        race.set_editor_property("grid", grid_xf)
        race.set_editor_property("checkpoints", gates)
        race.set_editor_property("gate_width", float(r["gate_width"]))
        race.set_editor_property("racing_line", [unreal.Vector(*p) for p in r["racing_line"]])
        race.set_editor_property("rival_skills", list(r["rivals"]))
        if not unreal.EditorAssetLibrary.save_loaded_asset(race, only_if_is_dirty=False):
            raise RuntimeError("Could not save " + path)

        ev = spawn(unreal.BNRaceEvent, "Race_" + r["name"].replace("RACE_", ""), r["beacon"], folder="Gameplay/Races")
        ev.set_editor_property("definition", race)
        ev.set_editor_property("start_radius", 520.0)
        ev.get_editor_property("beacon").set_material(0, MI["MI_RaceHolo_Start"])
        for post in ev.get_editor_property("gate_posts"):
            post.set_material(0, MI["MI_RaceHolo_Gate"])


def build_lighting():
    def movable_light(label, cast_shadows):
        light = spawn(unreal.DirectionalLight, label, (0.0, 0.0, 2000.0), folder="Lighting")
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
    cycle.set_editor_property("time_of_day", 17.4)  # Iconic golden-amber late afternoon over Karamara
    for prop, actor in (("sun", sun), ("moon", moon), ("sky_light", sky_light), ("fog", fog), ("post_process", post)):
        cycle.set_editor_property(prop, actor)

    # Reflection captures spread across all 6 districts & Eski Liman Koprusu
    captures = [
        ("Refl_MerkezWest",  (-18000.0,  -4000.0,  550.0), 26000.0, 1.10),
        ("Refl_MerkezEast",  ( 16000.0,  -4000.0,  550.0), 26000.0, 1.10),
        ("Refl_SanayiDrift", ( 44000.0,  52000.0,  600.0), 28000.0, 1.05),
        ("Refl_EskiLiman",   ( 68000.0, -10000.0, 1800.0), 34000.0, 1.20),
        ("Refl_Viyaduk",     (  2000.0, -54000.0, 1700.0), 32000.0, 1.12),
        ("Refl_Yamac",       (-54000.0,  36000.0, 2100.0), 28000.0, 1.10),
        ("Refl_KuzeyGecidi", (-44000.0, -82000.0, 5600.0), 34000.0, 1.15),
    ]
    for label, loc, radius, brightness in captures:
        rc = spawn(unreal.SphereReflectionCapture, label, loc, folder="Lighting/Reflections")
        comp = rc.get_editor_property("capture_component")
        comp.set_editor_property("influence_radius", radius)
        comp.set_editor_property("brightness", brightness)


# ---------------------------------------------------------------------------
# Main Build Execution
# ---------------------------------------------------------------------------
reset_level()
nodes, edges, lookup = knet.build_network()
clearance = kscene.build_road_clearance_checker(edges)
ctx = dict(spawn=spawn, block=block, place_mesh=place_mesh, MI=MI, PM=PM, meshes=MESHES)

kscene.build_terrain_and_sea(ctx)
build_road_geometry(nodes, edges)
kscene.build_landmarks(ctx)
kscene.build_buildings_and_storefronts(ctx, clearance)
kscene.build_street_furniture_and_foliage(ctx, nodes, edges, clearance)
export_road_graph_asset(nodes, edges)
build_gameplay_actors(edges)
build_races(nodes, edges, lookup)
build_lighting()

px, py, pz, pyaw = knet.PLAYER_START
spawn(unreal.PlayerStart, "PlayerStart", (px, py, pz), yaw=pyaw, folder="Gameplay")

if not levels.save_current_level():
    raise RuntimeError("Could not save " + MAP_PATH)
unreal.log("BN: built %s (2 km x 2 km, 6 districts, 9 races) with %d actors" % (
    MAP_PATH, len(actors.get_all_level_actors())
))
