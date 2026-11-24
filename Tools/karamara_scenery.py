"""Six-district scenery, landmarks, architecture, foliage HISM, signage, and street lighting
for BLACK NUMBERS' 2 km x 2 km Karamara open-world map.

Called by Tools/build_karamara_map.py inside UnrealEditor-Cmd Python.
"""
import math
import random

import unreal
import karamara_network as knet


def _dist_point_to_seg_2d(px, py, ax, ay, bx, by):
    vx, vy = bx - ax, by - ay
    l2 = vx * vx + vy * vy
    if l2 < 1e-4:
        return math.hypot(px - ax, py - ay)
    t = max(0.0, min(1.0, ((px - ax) * vx + (py - ay) * vy) / l2))
    return math.hypot(px - (ax + vx * t), py - (ay + vy * t))


def build_road_clearance_checker(edges):
    """Returns a fast clearance function min_dist_from_road(x, y) -> (dist_cm, nearest_z)."""
    segs = []
    for e in edges:
        pts = e["points"]
        hw = e["width"] * 0.5
        for i in range(len(pts) - 1):
            a, b = pts[i], pts[i + 1]
            min_x = min(a[0], b[0]) - hw - 4500.0
            max_x = max(a[0], b[0]) + hw + 4500.0
            min_y = min(a[1], b[1]) - hw - 4500.0
            max_y = max(a[1], b[1]) + hw + 4500.0
            segs.append((min_x, max_x, min_y, max_y, a[0], a[1], a[2], b[0], b[1], b[2], hw))

    def clearance(x, y):
        best_clear = 1e9
        best_z = 0.0
        for min_x, max_x, min_y, max_y, ax, ay, az, bx, by, bz, hw in segs:
            if x < min_x or x > max_x or y < min_y or y > max_y:
                continue
            d = _dist_point_to_seg_2d(x, y, ax, ay, bx, by) - hw
            if d < best_clear:
                best_clear = d
                best_z = 0.5 * (az + bz)
        return best_clear, best_z

    return clearance


def build_terrain_and_sea(ctx):
    """Builds Karamara landmass, coastal quay walls, East Harbor peninsula, animated Mara Sea,
    and 360-degree surrounding coastal mountains."""
    block = ctx["block"]
    place_mesh = ctx["place_mesh"]
    folder = "Environment/Terrain"

    # Main city landmass (X: -1150 m .. +620 m, Y: -1100 m .. +1050 m, top at Z = -4 cm)
    block("Landmass_Main", (-26500.0, -2500.0, -204.0), (1770.0, 2150.0, 4.0),
          "MI_Grass", "PM_Grass", folder=folder)

    # Central urban concrete ground bed under Merkez & Dogu Sanayi (clean city apron at Z = -2 cm)
    block("UrbanPad_Merkez", (-2000.0, -2000.0, -52.0), (760.0, 560.0, 1.0),
          "MI_Concrete", "PM_Concrete", folder=folder)
    block("UrbanPad_Sanayi", (38000.0, 48000.0, -52.0), (620.0, 580.0, 1.0),
          "MI_Concrete", "PM_Concrete", folder=folder)

    # Eski Liman solid concrete maritime wharf (X: +420 m .. +620 m, Y: -420 m .. +460 m)
    block("HarborQuay_Main", (52000.0, 2000.0, -202.0), (200.0, 880.0, 4.0),
          "MI_Concrete", "PM_Concrete", folder=folder)
    # Quay seawall concrete coping lip along X = +620 m
    block("HarborSeawall_W", (62100.0, 2000.0, -140.0), (2.5, 880.0, 3.2),
          "MI_Concrete", "PM_Concrete", folder=folder)

    # East Harbor Peninsula (bridge landing & Eski Liman Tunnel portal, top Z = +596 cm)
    block("HarborPeninsula_East", (94000.0, -8000.0, -102.0), (260.0, 620.0, 14.0),
          "MI_Concrete", "PM_Concrete", folder=folder)

    # Wide open Sanayi Drift Asphalt Courtyard (flat smooth drift pad, X: +330..+550 m, Y: +450..+640 m)
    block("SanayiDriftYard", (44000.0, 54500.0, -1.0), (220.0, 190.0, 0.25),
          "MI_Asphalt", "PM_Asphalt", folder="Districts/DoguSanayi")

    # Sculpted hillside massing underneath Yamac Mahallesi & Kuzey Gecidi so slopes feel solid
    block("HillMass_Yamac_Mid", (-56000.0, 38000.0, 400.0), (380.0, 420.0, 16.0),
          "MI_Grass", "PM_Grass", pitch=2.5, folder=folder)
    block("HillMass_Yamac_Upper", (-74000.0, 44000.0, 950.0), (260.0, 380.0, 22.0),
          "MI_MountainRock", "PM_Dirt", pitch=-3.0, folder=folder)
    block("HillMass_KuzeyGecidi_East", (-22000.0, -76000.0, 1100.0), (460.0, 280.0, 28.0),
          "MI_MountainRock", "PM_Dirt", folder=folder)
    block("HillMass_KuzeyGecidi_Summit", (-58000.0, -88000.0, 2600.0), (480.0, 300.0, 58.0),
          "MI_MountainRock", "PM_Dirt", folder=folder)
    # Seyir Tepesi flat paved overlook terrace at Z = +6200 cm
    block("SeyirTepesi_Overlook", (-60000.0, -86000.0, 6175.0), (46.0, 36.0, 0.5),
          "MI_Concrete", "PM_Concrete", folder="Districts/KuzeyGecidi")
    # Emre's Memorial Stone Monument on Seyir Tepesi ridge looking out over Karamara
    block("EmreMemorial_Plinth", (-61600.0, -87400.0, 6245.0), (3.4, 3.4, 0.9),
          "MI_Concrete", "PM_Concrete", folder="Districts/KuzeyGecidi")
    block("EmreMemorial_Stele", (-61600.0, -87400.0, 6420.0), (1.2, 0.5, 2.8),
          "MI_Metal_Dark", "PM_Concrete", folder="Districts/KuzeyGecidi")

    # Animated Mara Harbor & Open Sea Water (Z = -320 cm)
    block("MaraWater_Harbor", (76000.0, 0.0, -320.0), (320.0, 1400.0, 0.2),
          "MI_WaterHarbor", collision=False, folder="Environment/Water")
    block("MaraWater_Ocean", (175000.0, 0.0, -325.0), (1700.0, 3600.0, 0.2),
          "MI_WaterOcean", collision=False, folder="Environment/Water")

    # 360-degree surrounding coastal mountain ring closing the horizon completely (open to Mara Sea east)
    place_mesh("Karamara_SurroundingHills", "SM_BN_SurroundingHills", (0.0, 0.0, 0.0),
               collision=False, folder="Environment/Horizon")


def build_landmarks(ctx):
    """Places Eski Liman Koprusu, port cranes, cargo ship, tunnels, highway gantries & Turkish signs,
    industrial chimneys, and Selin's enclosed workshop courtyard."""
    block = ctx["block"]
    place_mesh = ctx["place_mesh"]
    MI = ctx["MI"]

    # 1. Eski Liman Koprusu: 2 monumental H-frame towers + 6 arched Warren truss spans across Mara Channel
    bf = "Landmarks/EskiLimanKoprusu"
    for px in (66000.0, 80000.0):
        place_mesh("EskiLimanBridge_Pylon", "SM_BN_BridgePylon", (px, -14000.0, 0.0),
                   yaw=0.0, collision=False, folder=bf)
    for k in range(6):
        sx = 63000.0 + k * 4000.0
        place_mesh("EskiLimanBridge_Truss", "SM_BN_BridgeTruss", (sx, -14000.0, 1582.0),
                   yaw=0.0, collision=False, folder=bf)

    # 2. Eski Liman Port Cranes & Berthed Container Freighter Ship
    hf = "Districts/EskiLiman"
    for cy in (-32000.0, -4000.0, 12000.0, 28000.0):
        place_mesh("PortCrane", "SM_BN_PortCrane", (59500.0, cy, 0.0), yaw=0.0, folder=hf)
    place_mesh("MaraCargoShip_A", "SM_BN_CargoShip", (72500.0, 14000.0, -320.0),
               yaw=90.0, collision=False, folder=hf)
    place_mesh("MaraCargoShip_B", "SM_BN_CargoShip", (98000.0, 36000.0, -320.0),
               yaw=112.0, collision=False, folder=hf)

    # 3. Highway Tunnel Portals (Act 5 Route A Eski Liman Tuneli + Route B Kuzey Gecidi + NW Viaduct)
    tf = "Landmarks/Tunnels"
    place_mesh("Tunnel_RouteA_Liman", "SM_BN_TunnelArch", (88000.0, -24000.0, 600.0), yaw=-90.0, collision=False, folder=tf)
    place_mesh("Tunnel_RouteB_Gecit", "SM_BN_TunnelArch", (-76000.0, -82000.0, 4600.0), yaw=160.0, collision=False, folder=tf)
    place_mesh("Tunnel_KuzeyViyaduk", "SM_BN_TunnelArch", (-58000.0, -52000.0, 1200.0), yaw=0.0, collision=False, folder=tf)

    # 4. Readable Turkish Overhead Highway Gantries & Signboards
    gf = "Landmarks/HighwayGantries"
    gantries = [
        ("Gantry_MerkezBulvar",  (  1000.0,  -4000.0,    0.0),   0.0, "MI_Sign_Merkez"),
        ("Gantry_EskiLiman",     ( 41000.0,  -4000.0,    0.0),   0.0, "MI_Sign_EskiLiman"),
        ("Gantry_KuzeyViyadukW", (-24000.0, -54500.0, 1200.0),   0.0, "MI_Sign_KuzeyGecidi"),
        ("Gantry_KuzeyViyadukE", ( 16000.0, -54500.0, 1200.0),   0.0, "MI_Sign_EskiLiman"),
        ("Gantry_DoguSanayi",    ( 26000.0,  42000.0,    0.0),   0.0, "MI_Sign_DoguSanayi"),
        ("Gantry_CevreGuney",    ( 12000.0,  76500.0,    0.0),   0.0, "MI_Sign_Merkez"),
    ]
    for label, loc, yaw, sign_mi in gantries:
        place_mesh(label, "SM_BN_GantryTruss", loc, yaw=yaw, collision=False, folder=gf)
        sb = place_mesh(label + "_Sign", "SM_BN_SignBoard", (loc[0], loc[1], loc[2] + 820.0),
                        yaw=yaw, collision=False, folder=gf)
        sb.static_mesh_component.set_material(0, MI[sign_mi])

    # 5. Roadside Turkish Billboards
    billboards = [
        ("Billboard_Merkez", (-6000.0, -13500.0, 0.0), 45.0, "MI_Sign_Merkez"),
        ("Billboard_Sanayi", (32000.0, 32000.0, 0.0), -30.0, "MI_Sign_DoguSanayi"),
        ("Billboard_Liman",  (44000.0, -16000.0, 0.0), 15.0, "MI_Sign_EskiLiman"),
        ("Billboard_Gecit",  (-21000.0, -64000.0, 1850.0), 120.0, "MI_Sign_KuzeyGecidi"),
    ]
    for label, loc, yaw, sign_mi in billboards:
        bb = place_mesh(label, "SM_BN_Billboard", loc, yaw=yaw, folder=gf)
        bb.static_mesh_component.set_material(1, MI[sign_mi])

    # 6. Doğu Sanayi Smokestacks & Tarık/Arda Landmark Workshop + Drift Yard Container Perimeter
    sf = "Districts/DoguSanayi"
    for cx, cy in ((25000.0, 56000.0), (63000.0, 52000.0), (19000.0, 32000.0)):
        place_mesh("FactoryChimney", "SM_BN_FactoryChimney", (cx, cy, 0.0), folder=sf)

    # Tarik Oto Tamir & Arda Galeri flagship hall along north side of Sanayi Bulvari
    block("TarikArda_Hall", (27000.0, 33000.0, 550.0), (36.0, 16.0, 11.0), "MI_Building_Brick", "PM_Concrete", folder=sf)
    place_mesh("TarikArda_Storefront_A", "SM_BN_Storefront", (20000.0, 41200.0, 0.0), yaw=90.0, collision=False, folder=sf)
    place_mesh("TarikArda_Storefront_B", "SM_BN_Storefront", (34000.0, 41200.0, 0.0), yaw=90.0, collision=False, folder=sf)

    # Stacked colorful shipping containers framing Sanayi Drift Courtyard & Eski Liman wharf
    c_mats = ["MI_Container_Blue", "MI_Container_Red", "MI_Container_Amber"]
    rng = random.Random(42)
    for idx, (cx, cy, yaw, stack) in enumerate([
        (34500.0, 49500.0, 0.0, 2), (34500.0, 56500.0, 0.0, 2), (34500.0, 62500.0, 0.0, 1),
        (53500.0, 49500.0, 0.0, 2), (53500.0, 57500.0, 0.0, 2),
        (42000.0, 63200.0, 90.0, 2), (48000.0, 63200.0, 90.0, 1),
        # Eski Liman wharf container stacks (well clear of X=50000 road)
        (55500.0, -28000.0, 0.0, 2), (55500.0, -18000.0, 0.0, 2),
        (55500.0,  2000.0, 0.0, 2), (55500.0,  10000.0, 0.0, 2), (55500.0, 24000.0, 0.0, 2),
    ]):
        for lv in range(stack):
            block("Container", (cx, cy, 130.0 + lv * 265.0), (12.0, 2.5, 2.6),
                  c_mats[(idx + lv) % len(c_mats)], "PM_Concrete", yaw=yaw, folder=sf if cy > 35000 else hf)

    # 7. Selin's Enclosed Garage Courtyard in Yamac Mahallesi (Z = +1600 cm)
    yf = "Districts/YamacMahallesi"
    gx, gy, gz, gyaw = knet.SELIN_GARAGE_POS
    # Flat concrete courtyard slab & U-shaped brick workshop shell behind the garage entrance
    block("SelinCourtyard_Pad", (gx - 900.0, gy + 1200.0, gz - 15.0), (34.0, 30.0, 0.4),
          "MI_Concrete", "PM_Concrete", folder=yf)
    block("SelinWorkshop_Main", (gx - 2200.0, gy + 1800.0, gz + 450.0), (14.0, 24.0, 9.0),
          "MI_Building_Brick", "PM_Concrete", folder=yf)
    block("SelinWorkshop_Side", (gx - 600.0, gy + 2600.0, gz + 350.0), (18.0, 6.0, 7.0),
          "MI_Building_Plaster", "PM_Concrete", folder=yf)
    place_mesh("SelinWorkshop_Front", "SM_BN_Storefront", (gx - 1480.0, gy + 1800.0, gz),
               yaw=0.0, collision=False, folder=yf)


def build_buildings_and_storefronts(ctx, clearance):
    """Populates varied urban skyline blocks across Merkez, Sanayi, Liman, and Yamac
    while strictly respecting road clearance so zero buildings ever clip a road."""
    block = ctx["block"]
    place_mesh = ctx["place_mesh"]
    rng = random.Random(2026)

    # 1. Merkez Bulvar & Carsi dense urban city blocks (with ground-floor Turkish storefronts & roof tanks!)
    mf = "Districts/MerkezBulvar"
    block_centers = [
        # North row (between Y=-220 m and Y=-40 m)
        (-22000.0, -13000.0), (-1000.0, -13000.0), (20000.0, -13000.0),
        # South row (between Y=-40 m and Y=+160 m)
        (-22000.0,   6000.0), (-1000.0,   6000.0), (20000.0,   6000.0),
        # Outer Merkez apron (north of Y=-220 m, south of Y=+160 m)
        (-22000.0, -34000.0), (  2000.0, -34000.0), (19000.0, -34000.0),
        ( -4000.0,  29000.0), ( 22000.0,  28000.0), (-26000.0,  28000.0),
    ]
    mats = ["MI_Building_Brick", "MI_Building_Plaster", "MI_Building_Glass"]
    for bi, (bx, by) in enumerate(block_centers):
        # 2x2 sub-lots inside each downtown block for skyline height & facade rhythm
        for dx in (-4800.0, 4800.0):
            for dy in (-3600.0, 3600.0):
                px, py = bx + dx, by + dy
                clr, _ = clearance(px, py)
                if clr < 480.0:
                    continue
                w = rng.uniform(5400.0, 7400.0)
                d = rng.uniform(4600.0, 6000.0)
                if clr < max(w, d) * 0.55:
                    scale_down = max(0.55, clr / (max(w, d) * 0.62))
                    w *= scale_down
                    d *= scale_down
                # Tall modern glass towers near downtown core; mid-rise brick/plaster around edges
                is_core = (abs(px) < 22000.0 and abs(py) < 18000.0)
                h = rng.uniform(2200.0, 4600.0) if is_core else rng.uniform(1200.0, 2600.0)
                mat = rng.choice(mats) if is_core else rng.choice(mats[:2])
                block("Bldg_Merkez", (px, py, h * 0.5), (w / 100.0, d / 100.0, h / 100.0),
                      mat, "PM_Concrete", folder=mf)
                # Rooftop parapet & occasional water tank for crisp skyline silhouettes
                block("RoofCap", (px, py, h + 40.0), (w / 100.0 + 0.4, d / 100.0 + 0.4, 0.8),
                      "MI_Concrete", collision=False, folder=mf)
                if rng.random() < 0.35:
                    place_mesh("RoofTank", "SM_BN_RoofWaterTank",
                               (px + rng.uniform(-1200, 1200), py + rng.uniform(-1000, 1000), h + 80.0),
                               collision=False, folder=mf)

    # Ground-floor Karamara storefronts flush against Merkez Bulvar (Y = -4000) & Carsi Caddesi (Y = +16000) facades
    for sx in (-25000.0, -17000.0, -4000.0, 3000.0, 17000.0, 24000.0):
        # North side of Merkez Bulvar facing south (+Y -> yaw=+90)
        place_mesh("Storefront_BulvarN", "SM_BN_Storefront", (sx, -5950.0, 0.0), yaw=90.0, collision=False, folder=mf)
        # South side of Merkez Bulvar facing north (-Y -> yaw=-90)
        place_mesh("Storefront_BulvarS", "SM_BN_Storefront", (sx, -2050.0, 0.0), yaw=-90.0, collision=False, folder=mf)
    for sx in (-24000.0, -4000.0, 18000.0):
        place_mesh("Storefront_Carsi", "SM_BN_Storefront", (sx, 14450.0, 0.0), yaw=90.0, collision=False, folder=mf)

    # 2. Dogu Sanayi Warehouses & Industrial Halls
    sf = "Districts/DoguSanayi"
    sanayi_lots = [
        ( 6000.0, 52000.0, 24.0, 18.0, 12.0), (14000.0, 57000.0, 22.0, 16.0, 11.0),
        (26000.0, 54000.0, 20.0, 16.0, 10.0), (46000.0, 32000.0, 22.0, 14.0, 12.0),
        (65000.0, 30000.0, 18.0, 16.0, 11.0), (36000.0, 74000.0, 26.0, 14.0, 10.0),
    ]
    for wx, wy, ww, wd, wh in sanayi_lots:
        clr, _ = clearance(wx, wy)
        if clr > max(ww, wd) * 45.0:
            block("Warehouse_Sanayi", (wx, wy, wh * 50.0), (ww, wd, wh),
                  rng.choice(["MI_Building_Steel", "MI_Building_Brick"]), "PM_Concrete", folder=sf)

    # 3. Eski Liman Maritime Depots
    hf = "Districts/EskiLiman"
    for lx, ly in ((41000.0, -14000.0), (41000.0, 6000.0), (41000.0, 26000.0)):
        clr, _ = clearance(lx, ly)
        if clr > 650.0:
            block("Depot_Liman", (lx, ly, 600.0), (14.0, 18.0, 12.0), "MI_Building_Steel", "PM_Concrete", folder=hf)

    # 4. Yamac Mahallesi Terraced Hillside Residences (elevated onto the slope!)
    yf = "Districts/YamacMahallesi"
    yamac_lots = [
        (-42000.0,  26000.0, 1050.0), (-42000.0,  38000.0, 1300.0), (-62000.0,  24000.0, 1600.0),
        (-62000.0,  52000.0, 1800.0), (-34000.0,  42000.0,  800.0), (-50000.0,  64000.0, 1050.0),
        (-30000.0,  60000.0,  450.0), (-66000.0,  16000.0, 1250.0),
    ]
    for yx, yy, yz in yamac_lots:
        clr, _ = clearance(yx, yy)
        if clr > 700.0:
            h = rng.uniform(1000.0, 1800.0)
            # Extend basement skirt downward so hillside buildings sit firmly in the slope
            block("House_Yamac", (yx, yy, yz + h * 0.5 - 300.0),
                  (rng.uniform(11.0, 16.0), rng.uniform(10.0, 14.0), (h + 600.0) / 100.0),
                  rng.choice(["MI_Building_Plaster", "MI_Building_Brick"]), "PM_Concrete", folder=yf)


def build_street_furniture_and_foliage(ctx, nodes, edges, clearance):
    """Places sodium street lamps across ALL 6 districts, intersection traffic lights,
    crosswalks, jersey barriers on highways/mountains, and HISM real-foliage trees/bushes."""
    spawn = ctx["spawn"]
    block = ctx["block"]
    place_mesh = ctx["place_mesh"]
    MI = ctx["MI"]
    meshes = ctx["meshes"]

    # 1. Sodium street lamps on ALL 6 districts (~110 lamps total, spaced smoothly along roads)
    lamp_positions = []
    for e in edges:
        if e["dirt"]:
            continue
        pts = e["points"]
        hw = e["width"] * 0.5
        spacing = 5200.0 if e["highway"] else (4200.0 if e["district"] == knet.DIST_MERKEZ else 5800.0)
        accum = spacing * 0.45
        for i in range(len(pts) - 1):
            a, b = pts[i], pts[i + 1]
            seg_len = math.hypot(b[0] - a[0], b[1] - a[1])
            if seg_len < 10.0:
                continue
            dx, dy = (b[0] - a[0]) / seg_len, (b[1] - a[1]) / seg_len
            nx, ny = -dy, dx  # right-hand outward normal
            accum += seg_len
            if accum >= spacing:
                accum -= spacing
                side = 1.0 if (len(lamp_positions) % 2 == 0) else -1.0
                off = hw + 135.0
                lx = 0.5 * (a[0] + b[0]) + nx * off * side
                ly = 0.5 * (a[1] + b[1]) + ny * off * side
                lz = 0.5 * (a[2] + b[2])
                # Skip lamps that fall too close to another intersecting road
                if any(math.hypot(lx - ox, ly - oy) < 2400.0 for ox, oy in lamp_positions):
                    continue
                lamp_positions.append((lx, ly))
                yaw = math.degrees(math.atan2(-ny * side, -nx * side))
                lamp = spawn(unreal.BNStreetLamp, "Lamp", (lx, ly, lz), yaw=yaw, folder="Street/Lamps")
                lamp.get_editor_property("glow").set_material(0, MI["MI_LampGlow"])

    # 2. Traffic lights & Crosswalks at signalized intersections in Merkez / Sanayi / Liman / Yamac
    for nid, n in nodes.items():
        if not n["signalized"]:
            continue
        nx, ny, nz = n["pos"]
        place_mesh("TrafficLight", "SM_BN_TrafficLight", (nx + 1050.0, ny + 1050.0, nz),
                   yaw=180.0, collision=False, folder="Street/Signals")
        place_mesh("TrafficLight", "SM_BN_TrafficLight", (nx - 1050.0, ny - 1050.0, nz),
                   yaw=0.0, collision=False, folder="Street/Signals")
        # Crisp zebra crosswalk decals on north & east approaches
        if nz < 50.0:
            block("Crosswalk", (nx + 1250.0, ny, nz + 3.0), (3.8, 13.0, 0.01),
                  "MI_Crosswalk", collision=False, folder="Street/Markings")
            block("Crosswalk", (nx, ny + 1250.0, nz + 3.0), (13.0, 3.8, 0.01),
                  "MI_Crosswalk", collision=False, folder="Street/Markings")

    # 3. Jersey barriers & real-foliage HISM (SubobjectDataSubsystem persists components into .umap cleanly)
    hism_actor = spawn(unreal.Actor, "Karamara_FoliageAndBarriers_HISM", (0.0, 0.0, 0.0), folder="Environment/HISM")
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)

    def make_hism(comp_name, mesh_name, collision=False, cast_shadow=True):
        actor_handle = None
        root_handle = None
        for h in sds.k2_gather_subobject_data_for_instance(hism_actor):
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
            sds.rename_subobject(handle=root_handle, new_name=unreal.Text("HismRoot"))

        comp_handle, err = sds.add_new_subobject(unreal.AddNewSubobjectParams(
            parent_handle=root_handle,
            new_class=unreal.HierarchicalInstancedStaticMeshComponent.static_class(),
            blueprint_context=None,
        ))
        if not err.is_empty():
            raise RuntimeError(str(err))
        sds.rename_subobject(handle=comp_handle, new_name=unreal.Text(comp_name))
        comp_data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(comp_handle)
        comp = unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(comp_data)
        comp.set_static_mesh(meshes[mesh_name])
        comp.set_editor_property("cast_shadow", cast_shadow)
        if not collision:
            comp.set_collision_profile_name("NoCollision")
            comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        return comp

    h_autumn_a = make_hism("AutumnTreesA_HISM", "SM_BN_Tree_Autumn_A", collision=False, cast_shadow=True)
    h_autumn_b = make_hism("AutumnTreesB_HISM", "SM_BN_Tree_Autumn_B", collision=False, cast_shadow=True)
    h_pine = make_hism("PineTrees_HISM", "SM_BN_Tree_Pine", collision=False, cast_shadow=True)
    h_bush = make_hism("Bushes_HISM", "SM_BN_Bush", collision=False, cast_shadow=False)
    h_barrier = make_hism("JerseyBarriers_HISM", "SM_BN_JerseyBarrier", collision=True, cast_shadow=False)

    # Jersey barriers lining Kuzey Viyadugu & mountain hairpins
    for e in edges:
        if not (e["highway"] or e["district"] == knet.DIST_GECIT):
            continue
        # Bridge already has steel truss sides
        if (e["node_a"], e["node_b"]) in ((20, 21), (21, 20)):
            continue
        pts = e["points"]
        hw = e["width"] * 0.5 + 55.0
        for i in range(len(pts) - 1):
            a, b = pts[i], pts[i + 1]
            seg = math.hypot(b[0] - a[0], b[1] - a[1])
            if seg < 400.0:
                continue
            dx, dy = (b[0] - a[0]) / seg, (b[1] - a[1]) / seg
            nx, ny = -dy, dx
            yaw = math.degrees(math.atan2(dy, dx))
            pitch = math.degrees(math.atan2(b[2] - a[2], seg))
            steps = max(1, int(seg // 360.0))
            for k in range(steps):
                t = (k + 0.5) / float(steps)
                px = a[0] + (b[0] - a[0]) * t
                py = a[1] + (b[1] - a[1]) * t
                pz = a[2] + (b[2] - a[2]) * t
                for side in (-1.0, 1.0):
                    bx = px + nx * hw * side
                    by = py + ny * hw * side
                    h_barrier.add_instance(unreal.Transform(
                        location=unreal.Vector(bx, by, pz),
                        rotation=unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw),
                        scale=unreal.Vector(1.0, 1.0, 1.0),
                    ), True)

    # Real-foliage Autumn Trees along Merkez, Yamac, Parks & Pine Forests on Kuzey Gecidi!
    rng = random.Random(777)

    # 3a. Boulevard autumn trees framing Merkez Bulvar & Carsi sidewalks
    for tx in range(-30000, 30000, 3800):
        for ty in (-5300.0, -2700.0, 14800.0, 17200.0):
            if any(abs(tx - ix) < 2200.0 for ix in (-32000, -12000, 10000, 30000)):
                continue
            sc = rng.uniform(0.85, 1.15)
            xf = unreal.Transform(
                location=unreal.Vector(tx + rng.uniform(-180, 180), ty, 0.0),
                rotation=unreal.Rotator(0.0, 0.0, rng.uniform(0.0, 360.0)),
                scale=unreal.Vector(sc, sc, sc),
            )
            (h_autumn_a if rng.random() < 0.55 else h_autumn_b).add_instance(xf, True)

    # 3b. Scattered autumn groves & shrubs across Yamac, Beltway medians, and parks
    for _ in range(360):
        tx = rng.uniform(-90000.0, 46000.0)
        ty = rng.uniform(-92000.0, 86000.0)
        # Keep out of harbor water & Sanayi drift yard
        if tx > 48000.0 or (31000.0 < tx < 57000.0 and 43000.0 < ty < 66000.0):
            continue
        clr, near_z = clearance(tx, ty)
        if clr < 420.0 or clr > 24000.0:
            continue
        # Don't block downtown building footprints heavily
        if -30000.0 < tx < 28000.0 and -24000.0 < ty < 18000.0 and clr > 950.0:
            continue
        sc = rng.uniform(0.85, 1.38)
        xf = unreal.Transform(
            location=unreal.Vector(tx, ty, max(0.0, near_z - 30.0)),
            rotation=unreal.Rotator(0.0, 0.0, rng.uniform(0.0, 360.0)),
            scale=unreal.Vector(sc, sc, sc),
        )
        if ty < -46000.0 or near_z > 1800.0:
            h_pine.add_instance(xf, True)
        elif rng.random() < 0.35:
            h_bush.add_instance(xf, True)
        elif rng.random() < 0.55:
            h_autumn_a.add_instance(xf, True)
        else:
            h_autumn_b.add_instance(xf, True)

    # 3c. Extra pine & bush lining along Kuzey Gecidi mountain switchbacks
    for e in edges:
        if e["district"] not in (knet.DIST_GECIT, knet.DIST_YAMAC):
            continue
        for pt in e["points"][::2]:
            for side in (-1.0, 1.0):
                ox = pt[0] + side * rng.uniform(1100.0, 2400.0)
                oy = pt[1] + rng.uniform(-1200.0, 1200.0)
                clr, _ = clearance(ox, oy)
                if clr < 480.0:
                    continue
                sc = rng.uniform(0.95, 1.45)
                xf = unreal.Transform(
                    location=unreal.Vector(ox, oy, max(0.0, pt[2] - 80.0)),
                    rotation=unreal.Rotator(0.0, 0.0, rng.uniform(0.0, 360.0)),
                    scale=unreal.Vector(sc, sc, sc),
                )
                (h_pine if e["district"] == knet.DIST_GECIT else h_autumn_a).add_instance(xf, True)

    # 4. Subtle blue holographic markers at the 6 Cooldown Hiding Spots
    for spot in knet.COOLDOWN_SPOTS:
        ring = place_mesh("CooldownSpot_" + spot["name"], "SM_BN_RaceRing", spot["loc"],
                          collision=False, folder="Police/CooldownSpots")
        ring.static_mesh_component.set_material(0, MI["MI_RaceHolo_Cooldown"])
        ring.set_actor_scale3d(unreal.Vector(1.4, 1.4, 0.65))
