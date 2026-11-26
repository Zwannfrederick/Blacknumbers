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
import fetch_cc0_models  # noqa: E402
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


class HismBatcher:
    """Batches thousands of repeated road/city StaticMeshes into minimal GPU-instanced HISM components."""

    def __init__(self, actor_label, folder):
        self.actor = spawn(unreal.Actor, actor_label, (0.0, 0.0, 0.0), folder=folder)
        self.solid_boxes = []
        self.sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
        self.root_handle = None
        actor_handle = None
        for h in self.sds.k2_gather_subobject_data_for_instance(self.actor):
            d = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h)
            if unreal.SubobjectDataBlueprintFunctionLibrary.is_root_component(d):
                self.root_handle = h
            elif unreal.SubobjectDataBlueprintFunctionLibrary.is_actor(d):
                actor_handle = h
        if not self.root_handle:
            self.root_handle, err = self.sds.add_new_subobject(unreal.AddNewSubobjectParams(
                parent_handle=actor_handle,
                new_class=unreal.SceneComponent.static_class(),
                blueprint_context=None,
            ))
            if not err.is_empty():
                raise RuntimeError(str(err))
            self.sds.rename_subobject(handle=self.root_handle, new_name=unreal.Text("BatchRoot"))
        self.buckets = {}
        self.counter = 0

    def _get_comp(self, name_hint, mesh, mat_name, phys_name, collision, cast_shadow, cull_cm):
        key = (mesh.get_name(), mat_name, phys_name, bool(collision), bool(cast_shadow), int(cull_cm))
        comp = self.buckets.get(key)
        if comp is not None:
            return comp
        self.counter += 1
        comp_handle, err = self.sds.add_new_subobject(unreal.AddNewSubobjectParams(
            parent_handle=self.root_handle,
            new_class=unreal.HierarchicalInstancedStaticMeshComponent.static_class(),
            blueprint_context=None,
        ))
        if not err.is_empty():
            raise RuntimeError(str(err))
        self.sds.rename_subobject(handle=comp_handle, new_name=unreal.Text("%s_%d" % (name_hint, self.counter)))
        comp = unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(
            unreal.SubobjectDataBlueprintFunctionLibrary.get_data(comp_handle)
        )
        comp.set_static_mesh(mesh)
        if mat_name is not None:
            comp.set_material(0, MI[mat_name])
        if phys_name is not None:
            comp.set_phys_material_override(PM[phys_name])
        comp.set_editor_property("cast_shadow", bool(cast_shadow))
        if collision:
            comp.set_collision_profile_name("BlockAll")
            comp.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
        else:
            comp.set_collision_profile_name("NoCollision")
            comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        if cull_cm > 0:
            comp.set_cull_distances(0, int(cull_cm))
        self.buckets[key] = comp
        return comp

    def add(self, name_hint, loc, scale, mat_name=None, phys_name=None, collision=True,
            yaw=0.0, pitch=0.0, mesh=CUBE, cast_shadow=True, cull_cm=0):
        comp = self._get_comp(name_hint, mesh, mat_name, phys_name, collision, cast_shadow, cull_cm)
        if collision and mesh == CUBE:
            # ponytail: axis-aligned footprint (yaw ignored); enough to keep later props out of buildings.
            hx, hy = abs(scale[0]) * 50.0, abs(scale[1]) * 50.0
            self.solid_boxes.append((loc[0] - hx, loc[0] + hx, loc[1] - hy, loc[1] + hy))
        xf = unreal.Transform(
            location=unreal.Vector(*loc),
            rotation=unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw),
            scale=unreal.Vector(*scale),
        )
        comp.add_instance(xf, True)


def reset_level():
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        levels.load_level(MAP_PATH)
    else:
        levels.new_level(MAP_PATH)
    for actor in list(actors.get_all_level_actors()):
        if not isinstance(actor, (unreal.WorldSettings, unreal.Brush)):
            actors.destroy_actor(actor)
    counters.clear()


ROAD_SLAB_MAX_CM = 400.0
ROAD_SLAB_OVERLAP_CM = 20.0


def _pchip_slopes(d, z):
    """Fritsch-Carlson monotone slopes: the height profile never overshoots a crest or bridge deck."""
    n = len(z)
    h = [d[i + 1] - d[i] for i in range(n - 1)]
    m = [(z[i + 1] - z[i]) / h[i] if h[i] > 0 else 0.0 for i in range(n - 1)]
    t = [0.0] * n
    t[0], t[-1] = m[0], m[-1]
    for i in range(1, n - 1):
        if m[i - 1] * m[i] <= 0.0:
            t[i] = 0.0
        else:
            w1, w2 = 2.0 * h[i] + h[i - 1], h[i] + 2.0 * h[i - 1]
            t[i] = (w1 + w2) / (w1 / m[i - 1] + w2 / m[i])
    return t


def _densify_road(points, max_seg=ROAD_SLAB_MAX_CM):
    """Splits an edge polyline into short slabs: XY stays on the original polyline (AI lines unchanged),
    Z follows a monotone cubic over distance, so consecutive slabs differ by a fraction of a degree and
    their overlaps never poke up as visible steps on ramps and bridge approaches."""
    if len(points) < 2:
        return list(points)
    d = [0.0]
    for a, b in zip(points, points[1:]):
        d.append(d[-1] + math.hypot(b[0] - a[0], b[1] - a[1]))
    z = [p[2] for p in points]
    t = _pchip_slopes(d, z) if len(points) > 2 else [(z[1] - z[0]) / max(d[1], 1.0)] * 2
    out = [tuple(points[0])]
    for i in range(len(points) - 1):
        a, b = points[i], points[i + 1]
        h = d[i + 1] - d[i]
        n = max(1, int(math.ceil(h / max_seg)))
        for k in range(1, n + 1):
            u = k / float(n)
            h00, h10 = 2 * u ** 3 - 3 * u ** 2 + 1, u ** 3 - 2 * u ** 2 + u
            h01, h11 = -2 * u ** 3 + 3 * u ** 2, u ** 3 - u ** 2
            zz = h00 * z[i] + h10 * h * t[i] + h01 * z[i + 1] + h11 * h * t[i + 1]
            # XY: Catmull-Rom through the network samples (same curve the network was sampled from), so slabs
            # turn gradually instead of kinking every 15 m; deviation from walls/AI lines is a few cm.
            p0, p3 = points[max(0, i - 1)], points[min(len(points) - 1, i + 2)]
            cx = 0.5 * (2 * a[0] + (-p0[0] + b[0]) * u + (2 * p0[0] - 5 * a[0] + 4 * b[0] - p3[0]) * u * u
                        + (-p0[0] + 3 * a[0] - 3 * b[0] + p3[0]) * u ** 3)
            cy = 0.5 * (2 * a[1] + (-p0[1] + b[1]) * u + (2 * p0[1] - 5 * a[1] + 4 * b[1] - p3[1]) * u * u
                        + (-p0[1] + 3 * a[1] - 3 * b[1] + p3[1]) * u ** 3)
            out.append((cx, cy, zz))
    return out


def build_road_geometry(nodes, edges):
    """Constructs continuous smooth curved & sloped drivable asphalt/cobblestone/dirt roads,
    sidewalks, viaduct support piers, embankment retaining slopes, and intersection pads via HISM batching."""
    roads = HismBatcher("Karamara_RoadNetwork_HISM", "RoadNetwork/HISM")

    # Pre-index road segments so sidewalks, embankments, and piers NEVER intrude into intersecting or lower roads!
    all_segs = []
    for e in edges:
        ep = e["points"]
        hw = e["width"] * 0.5
        for k in range(len(ep) - 1):
            u, v = ep[k], ep[k + 1]
            all_segs.append((
                e["index"], hw,
                min(u[0], v[0]) - hw - 1800.0, max(u[0], v[0]) + hw + 1800.0,
                min(u[1], v[1]) - hw - 1800.0, max(u[1], v[1]) + hw + 1800.0,
                u[0], u[1], u[2], v[0], v[1], v[2],
            ))

    def hits_other_road(own_idx, x, y, z, pad_cm, z_tol_cm=220.0, only_lower=False):
        for eidx, hw, minx, maxx, miny, maxy, ax, ay, az, bx, by, bz in all_segs:
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

    # Track widest incident edge & degree/slope per node for clean circular junction pads
    node_width = {nid: 1400.0 for nid in nodes}
    node_mat = {nid: ("MI_Asphalt", "PM_Asphalt") for nid in nodes}
    node_degree = {nid: 0 for nid in nodes}
    node_sloped = {nid: False for nid in nodes}
    for e in edges:
        a_id, b_id = e["node_a"], e["node_b"]
        node_width[a_id] = max(node_width[a_id], e["width"])
        node_width[b_id] = max(node_width[b_id], e["width"])
        node_degree[a_id] += 1
        node_degree[b_id] += 1
        pts_e = e["points"]
        if len(pts_e) >= 2:
            if abs(pts_e[1][2] - pts_e[0][2]) > 45.0:
                node_sloped[a_id] = True
            if abs(pts_e[-1][2] - pts_e[-2][2]) > 45.0:
                node_sloped[b_id] = True

    for e in edges:
        w = e["width"]
        is_dirt = e["dirt"]
        is_carsi = (e["node_a"] in (8, 9, 10, 11) and e["node_b"] in (8, 9, 10, 11) and abs(e["points"][0][1] - 16000.0) < 200.0)
        zs = [q[2] for q in e["points"]]
        # Ramps get 2 m slabs: on a slope any yaw change between slabs shows as a crease.
        pts = _densify_road(e["points"], 200.0 if max(zs) - min(zs) > 50.0 else ROAD_SLAB_MAX_CM)
        is_old_yamac = (e["district"] == knet.DIST_YAMAC and e["node_a"] in (32, 33, 34) and e["node_b"] in (32, 33, 34))
        if is_dirt:
            mat = "MI_Dirt"
        elif is_carsi or is_old_yamac:
            mat = "MI_Cobblestone"
        elif e["highway"]:
            mat = "MI_HighwayAsphalt"
        else:
            mat = "MI_Asphalt"
        phys = "PM_Dirt" if is_dirt else "PM_Asphalt"

        if is_dirt:
            node_mat[e["node_a"]] = ("MI_Dirt", "PM_Dirt")
            node_mat[e["node_b"]] = ("MI_Dirt", "PM_Dirt")

        pier_accum = 2200.0
        na_pos, nb_pos = nodes[e["node_a"]]["pos"], nodes[e["node_b"]]["pos"]
        na_clear = node_width[e["node_a"]] * 0.65 + 450.0
        nb_clear = node_width[e["node_b"]] * 0.65 + 450.0

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

            # 46 cm thick drivable slab; short slabs + a small fixed overlap keep joints seamless (see _densify_road)
            slab_len_m = (seg3d + ROAD_SLAB_OVERLAP_CM) / 100.0
            slab_w_m = w / 100.0
            thick_cm = 46.0
            roads.add("RoadSeg", (mx, my, mz - thick_cm * 0.5), (slab_len_m, slab_w_m, thick_cm / 100.0),
                      mat, phys, collision=True, yaw=yaw, pitch=pitch, cast_shadow=False)

            # Lane markings (center yellow dashes, white edges, + dashed multi-lane dividers on >= 18m boulevards/highways)
            if not is_dirt:
                roads.add("CenterLine", (mx, my, mz + 1.8), (slab_len_m * 0.52, 0.16, 0.02),
                          "MI_LineYellow", None, collision=False, yaw=yaw, pitch=pitch, cast_shadow=False, cull_cm=28000)
                nx, ny = -dy / dxy, dx / dxy
                for side in (-1.0, 1.0):
                    eo = (w * 0.5 - 55.0) * side
                    roads.add("EdgeLine", (mx + nx * eo, my + ny * eo, mz + 1.5), (slab_len_m, 0.14, 0.02),
                              "MI_LineWhite", None, collision=False, yaw=yaw, pitch=pitch, cast_shadow=False, cull_cm=28000)
                    if w >= 1800.0:
                        lo = (w * 0.25) * side
                        roads.add("LaneDash", (mx + nx * lo, my + ny * lo, mz + 1.6), (slab_len_m * 0.42, 0.12, 0.02),
                                  "MI_LineWhite", None, collision=False, yaw=yaw, pitch=pitch, cast_shadow=False, cull_cm=26000)

                # Raised urban sidewalks along flat Merkez / Sanayi / Yamac streets — strictly clear of junctions!
                if not e["highway"] and e["district"] in (knet.DIST_MERKEZ, knet.DIST_SANAYI, knet.DIST_YAMAC):
                    near_junction = (
                        math.hypot(mx - na_pos[0], my - na_pos[1]) < na_clear
                        or math.hypot(mx - nb_pos[0], my - nb_pos[1]) < nb_clear
                    )
                    if not near_junction:
                        sw_w = 300.0
                        for side in (-1.0, 1.0):
                            so = (w * 0.5 + sw_w * 0.5) * side
                            sx, sy = mx + nx * so, my + ny * so
                            if not hits_other_road(e["index"], sx, sy, mz, pad_cm=260.0, z_tol_cm=200.0):
                                roads.add("Sidewalk", (sx, sy, mz - 12.0),
                                          (slab_len_m, sw_w / 100.0, 0.52), "MI_Sidewalk", "PM_Concrete",
                                          collision=True, yaw=yaw, pitch=pitch, cast_shadow=False)

            # Structural support under elevated segments (Z > 120 cm)
            if mz > 120.0:
                is_bridge_span = (e["node_a"], e["node_b"]) in ((20, 21), (21, 20))
                over_lower_road = hits_other_road(e["index"], mx, my, mz, pad_cm=w * 0.55 + 250.0, only_lower=True)
                if (e["highway"] or over_lower_road) and not is_bridge_span:
                    roads.add("ViaductGirder", (mx, my, mz - 95.0), (slab_len_m, slab_w_m * 0.86, 1.3),
                              "MI_Concrete", "PM_Concrete", collision=True, yaw=yaw, pitch=pitch, cast_shadow=True)
                    pier_accum += seg3d
                    if pier_accum >= 4500.0 and not over_lower_road:
                        pier_accum = 0.0
                        pier_h = mz + 120.0
                        roads.add("ViaductPier", (mx, my, (mz - 100.0) * 0.5 - 60.0),
                                  (2.6, 2.6, pier_h / 100.0), "MI_Concrete", "PM_Concrete",
                                  collision=True, mesh=CYLINDER, cast_shadow=True)
                elif not is_bridge_span:
                    emb_h = mz + 240.0
                    roads.add("Embankment", (mx, my, mz - 42.0 - emb_h * 0.5),
                              (slab_len_m * 1.04, slab_w_m * 1.24, emb_h / 100.0),
                              "MI_MountainRock" if e["district"] == knet.DIST_GECIT else "MI_Concrete",
                              "PM_Concrete", collision=True, yaw=yaw, pitch=0.0, cast_shadow=False)

    # Circular intersection blend pads at multi-way junctions (skip degree-2 pass-through sloped nodes so a flat pad
    # never juts out as a horizontal shelf above a climbing ramp like Eski Liman Koprusu!)
    for nid, n in nodes.items():
        if node_degree[nid] <= 2 and (node_sloped[nid] or nid in (20, 21)):
            continue
        x, y, z = n["pos"]
        scale_f = 0.85 if node_sloped[nid] else 1.18
        dia_m = (node_width[nid] * scale_f) / 100.0
        nmat, nphys = node_mat[nid]
        roads.add("JunctionPad", (x, y, z - 24.5), (dia_m, dia_m, 0.45),
                  nmat, nphys, collision=True, mesh=CYLINDER, cast_shadow=False)


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

    dist_enums = [
        unreal.BNDistrict.MERKEZ_BULVAR,
        unreal.BNDistrict.DOGU_SANAYI,
        unreal.BNDistrict.ESKI_LIMAN,
        unreal.BNDistrict.KUZEY_VIYADUGU,
        unreal.BNDistrict.YAMAC_MAHALLESI,
        unreal.BNDistrict.KUZEY_GECIDI,
    ]

    # 6 Open-World Roadside Speed Traps (1 per borough)
    for trap_id, tx, ty, tz, tyaw, tname, tdist, min_kmh, trad in knet.SPEED_TRAPS:
        st = spawn(unreal.BNSpeedTrap, trap_id, (tx, ty, tz), yaw=tyaw, folder="Gameplay/Activities")
        st.set_editor_property("trap_id", trap_id)
        st.set_editor_property("display_name", tname)
        st.set_editor_property("district", dist_enums[tdist])
        st.set_editor_property("min_trigger_speed_kmh", float(min_kmh))
        st.set_editor_property("trigger_radius_cm", float(trad))
        st.get_editor_property("post_mesh").set_material(0, MI["MI_Metal_Dark"])
        st.get_editor_property("camera_head_mesh").set_material(0, MI["MI_HazardYellow"])

    # 4 Open-World Stunt Jump Ramps
    for jump_id, jx, jy, jz, jyaw, jname, jdist in knet.STUNT_JUMPS:
        jp = spawn(unreal.BNStuntJump, jump_id, (jx, jy, jz), yaw=jyaw, folder="Gameplay/Activities")
        jp.set_editor_property("jump_id", jump_id)
        jp.set_editor_property("display_name", jname)
        jp.set_editor_property("district", dist_enums[jdist])
        if "SM_BN_JumpRamp" in MESHES:
            rmesh = jp.get_editor_property("ramp_mesh")
            rmesh.set_static_mesh(MESHES["SM_BN_JumpRamp"])
            rmesh.set_editor_property("relative_scale3d", unreal.Vector(1.0, 1.0, 1.0))
            rmesh.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, 0.0))
            rmesh.set_editor_property("relative_rotation", unreal.Rotator(0.0, 0.0, 0.0))


def build_races(nodes, edges, lookup):
    """Builds all 11 unique Karamara race DataAssets (Circuit, Sprint, Drift, Drag) & places holographic beacons."""
    disc_map = {
        "CIRCUIT": unreal.BNRaceDiscipline.CIRCUIT,
        "SPRINT":  unreal.BNRaceDiscipline.SPRINT,
        "DRIFT":   unreal.BNRaceDiscipline.DRIFT,
        "DRAG":    unreal.BNRaceDiscipline.DRAG,
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
    if "SM_BN_SkyDome" in MESHES:
        sky_dome = place_mesh("Karamara_CloudSkyDome", "SM_BN_SkyDome", (0.0, 0.0, -2400.0),
                              collision=False, folder="Lighting")
        sky_dome.static_mesh_component.set_editor_property("cast_shadow", False)

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
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property("kill_z", -500.0)  # Just below Mara Sea at -320 cm.
nodes, edges, lookup = knet.build_network()
clearance = kscene.build_road_clearance_checker(edges)
scenery_batch = HismBatcher("Karamara_CityScenery_HISM", "Environment/HISM")
cc0_meshes = fetch_cc0_models.load_available_cc0_meshes()
ctx = dict(spawn=spawn, block=block, place_mesh=place_mesh, hism=scenery_batch,
           MI=MI, PM=PM, meshes=MESHES, cc0=cc0_meshes, cube=CUBE)

kscene.build_terrain_and_sea(ctx)
build_road_geometry(nodes, edges)
kscene.build_landmarks(ctx)
kscene.build_buildings_and_storefronts(ctx, clearance)
kscene.build_street_furniture_and_foliage(ctx, nodes, edges, clearance)
kscene.build_frontage_infill(ctx, edges, clearance)
kscene.build_block_interiors(ctx, edges, clearance)
kscene.build_road_boundaries(ctx, edges)
kscene.build_parked_cars(ctx, edges, clearance)
export_road_graph_asset(nodes, edges)
build_gameplay_actors(edges)
build_races(nodes, edges, lookup)
build_lighting()

px, py, pz, pyaw = knet.PLAYER_START
spawn(unreal.PlayerStart, "PlayerStart", (px, py, pz), yaw=pyaw, folder="Gameplay")

if not levels.save_current_level():
    raise RuntimeError("Could not save " + MAP_PATH)
unreal.log("BN: built %s (2 km x 2 km, 6 districts, 11 races, %d traps, %d jumps, %d CC0 meshes) with %d actors" % (
    MAP_PATH, len(knet.SPEED_TRAPS), len(knet.STUNT_JUMPS), len(cc0_meshes), len(actors.get_all_level_actors())
))
