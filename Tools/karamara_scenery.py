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

    # Keep the quay below the south exit as it descends from node 22 toward Sanayi.
    block("HarborPeninsula_East", (94000.0, -8000.0, -400.0), (260.0, 620.0, 14.0),
          "MI_Concrete", "PM_Concrete", folder=folder)

    # Wide open Sanayi Drift Asphalt Courtyard (flat smooth drift pad, X: +330..+550 m, Y: +450..+640 m)
    block("SanayiDriftYard", (44000.0, 54500.0, -1.0), (220.0, 190.0, 0.25),
          "MI_Asphalt", "PM_Asphalt", folder="Districts/DoguSanayi")

    # Sculpted organic 3D mountain & hillside terrain under Kuzey Gecidi (+62..+88 m) and Yamac Mahallesi (+26 m)
    hills = place_mesh("Karamara_SculptedHills", "SM_BN_KaramaraHills", (0.0, 0.0, 0.0), folder=folder)
    hills.static_mesh_component.set_phys_material_override(ctx["PM"]["PM_Grass"])
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

    # 1. Eski Liman Koprusu: 2 monumental H-frame towers + 5 exact 40 m arched Warren truss spans (X = 64000..84000, zero overhang!)
    bf = "Landmarks/EskiLimanKoprusu"
    for px in (68000.0, 80000.0):
        place_mesh("EskiLimanBridge_Pylon", "SM_BN_BridgePylon", (px, -14000.0, 0.0),
                   yaw=0.0, collision=False, folder=bf)
    for k in range(5):
        sx = 66000.0 + k * 4000.0
        place_mesh("EskiLimanBridge_Truss", "SM_BN_BridgeTruss", (sx, -14000.0, 1582.0),
                   yaw=0.0, folder=bf)

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

    # 5. Roadside Turkish Billboards (all verified >10 m clear of all roads)
    billboards = [
        ("Billboard_Merkez", (-6000.0, -13500.0, 0.0), 45.0, "MI_Sign_Merkez"),
        ("Billboard_Sanayi", (32000.0, 32000.0, 0.0), -30.0, "MI_Sign_DoguSanayi"),
        ("Billboard_Liman",  (44000.0, -16000.0, 0.0), 15.0, "MI_Sign_EskiLiman"),
        ("Billboard_Gecit",  (-14200.0, -59500.0, 1650.0), 120.0, "MI_Sign_KuzeyGecidi"),
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

    # Stacked colorful shipping containers framing Sanayi Drift Courtyard & Eski Liman wharf (HISM batched)
    hism = ctx["hism"]
    c_mats = ["MI_Container_Blue", "MI_Container_Red", "MI_Container_Amber"]
    for idx, (cx, cy, yaw, stack) in enumerate([
        (34500.0, 49500.0, 0.0, 2), (34500.0, 56500.0, 0.0, 2), (34500.0, 62500.0, 0.0, 1),
        (53500.0, 49500.0, 0.0, 2), (53500.0, 57500.0, 0.0, 2),
        (42000.0, 63200.0, 90.0, 2), (48000.0, 63200.0, 90.0, 1),
        # Eski Liman wharf container stacks (well clear of X=50000 road & West Bridge Ramp)
        (55500.0, -28000.0, 0.0, 2), (55500.0, -8000.0, 0.0, 2),
        (55500.0,  2000.0, 0.0, 2), (55500.0,  10000.0, 0.0, 2), (55500.0, 24000.0, 0.0, 2),
    ]):
        for lv in range(stack):
            hism.add("Container", (cx, cy, 130.0 + lv * 265.0), (12.0, 2.5, 2.6),
                     c_mats[(idx + lv) % len(c_mats)], "PM_Concrete", collision=True,
                     yaw=yaw, cast_shadow=True, cull_cm=55000)

    # 7. Selin's Enclosed Garage Courtyard in Yamac Mahallesi (Z = +1600 cm, clear of roads 33->34 & 33->35)
    yf = "Districts/YamacMahallesi"
    gx, gy, gz, gyaw = knet.SELIN_GARAGE_POS
    block("SelinCourtyard_Pad", (gx - 2200.0, gy - 2000.0, gz - 15.0), (26.0, 22.0, 0.4),
          "MI_Concrete", "PM_Concrete", folder=yf)
    block("SelinWorkshop_Main", (gx - 2800.0, gy - 2600.0, gz + 450.0), (14.0, 16.0, 9.0),
          "MI_Building_Brick", "PM_Concrete", folder=yf)
    block("SelinWorkshop_Roof", (gx - 2800.0, gy - 2600.0, gz + 930.0), (14.8, 16.8, 0.7),
          "MI_Building_Terracotta", None, collision=False, folder=yf)
    block("SelinWorkshop_Side", (gx + 2600.0, gy - 1800.0, gz + 350.0), (12.0, 10.0, 7.0),
          "MI_Building_Plaster", "PM_Concrete", folder=yf)
    block("SelinWorkshop_SideRoof", (gx + 2600.0, gy - 1800.0, gz + 730.0), (12.8, 10.8, 0.65),
          "MI_Building_Terracotta", None, collision=False, folder=yf)
    place_mesh("SelinWorkshop_Front", "SM_BN_Storefront", (gx - 2080.0, gy - 2600.0, gz),
               yaw=0.0, collision=False, folder=yf)

    # Rich CC0 workshop & dockyard dressing (Poly Haven CC0 1.0 HISM-batched)
    cc0 = ctx.get("cc0", {})
    if "SM_CC0_RollershutterDoor" in cc0:
        hism.add("CC0_Shutter", (gx - 2090.0, gy - 2100.0, gz), (1.1, 1.1, 1.1),
                 None, None, collision=False, yaw=90.0, mesh=cc0["SM_CC0_RollershutterDoor"], cast_shadow=False, cull_cm=28000)
        hism.add("CC0_Shutter", (27000.0, 41100.0, 0.0), (1.3, 1.3, 1.3),
                 None, None, collision=False, yaw=0.0, mesh=cc0["SM_CC0_RollershutterDoor"], cast_shadow=False, cull_cm=32000)
    if "SM_CC0_CoveredCar" in cc0:
        hism.add("CC0_CoveredCar", (gx - 1550.0, gy - 1350.0, gz + 4.0), (1.0, 1.0, 1.0),
                 None, None, collision=False, yaw=18.0, mesh=cc0["SM_CC0_CoveredCar"], cast_shadow=True, cull_cm=26000)
    if "SM_CC0_ToolChest" in cc0:
        hism.add("CC0_ToolChest", (gx - 1950.0, gy - 3150.0, gz + 4.0), (1.15, 1.15, 1.15),
                 None, None, collision=False, yaw=-15.0, mesh=cc0["SM_CC0_ToolChest"], cast_shadow=False, cull_cm=20000)
    if "SM_CC0_FloodLights" in cc0:
        for fl_loc, fl_yaw in (
            ((gx - 1200.0, gy - 1100.0, gz + 4.0), -135.0),
            ((gx - 3100.0, gy - 1200.0, gz + 4.0), -45.0),
            ((36000.0, 48000.0, 0.0), 45.0),
            ((52000.0, 48000.0, 0.0), 135.0),
            ((52000.0, 62000.0, 0.0), -135.0),
            ((56500.0, -10000.0, 0.0), 180.0),
            ((56500.0, 14000.0, 0.0), 180.0),
        ):
            hism.add("CC0_FloodLight", fl_loc, (1.15, 1.15, 1.15), None, None,
                     collision=False, yaw=fl_yaw, mesh=cc0["SM_CC0_FloodLights"], cast_shadow=False, cull_cm=30000)
    if "SM_CC0_OldTyre" in cc0:
        for ty_loc in (
            (gx - 1800.0, gy - 3350.0, gz + 4.0), (gx - 1720.0, gy - 3350.0, gz + 38.0),
            (36200.0, 52000.0, 0.0), (36200.0, 52800.0, 0.0), (36200.0, 58000.0, 0.0),
            (51800.0, 52000.0, 0.0), (51800.0, 58500.0, 0.0),
        ):
            hism.add("CC0_OldTyre", ty_loc, (1.2, 1.2, 1.2), None, None,
                     collision=False, yaw=25.0, mesh=cc0["SM_CC0_OldTyre"], cast_shadow=False, cull_cm=22000)
    if "SM_CC0_Barrel" in cc0:
        for br_loc in (
            (gx - 1650.0, gy - 3250.0, gz + 4.0), (gx - 1570.0, gy - 3180.0, gz + 4.0),
            (35800.0, 50500.0, 0.0), (35920.0, 50640.0, 0.0), (52200.0, 50800.0, 0.0),
            (57200.0, -24000.0, 0.0), (57340.0, -23880.0, 0.0), (57200.0, 4000.0, 0.0),
            (57340.0, 4120.0, 0.0), (57200.0, 20000.0, 0.0),
        ):
            hism.add("CC0_Barrel", br_loc, (1.1, 1.1, 1.1), None, None,
                     collision=False, yaw=0.0, mesh=cc0["SM_CC0_Barrel"], cast_shadow=False, cull_cm=24000)
    if "SM_CC0_ChainlinkFence" in cc0:
        for fx in range(36500, 51500, 320):
            hism.add("CC0_Fence", (float(fx), 64200.0, 0.0), (1.0, 1.0, 1.0), None, None,
                     collision=False, yaw=0.0, mesh=cc0["SM_CC0_ChainlinkFence"], cast_shadow=False, cull_cm=28000)


def build_buildings_and_storefronts(ctx, clearance):
    """Populates varied urban skyline blocks across Merkez, Sanayi, Liman, and Yamac via HISM batching
    while strictly respecting road clearance so zero buildings ever clip a road."""
    hism = ctx["hism"]
    meshes = ctx["meshes"]
    rng = random.Random(2026)

    # 1. Merkez Bulvar & Carsi dense urban city blocks (with ground-floor Turkish storefronts & roof tanks!)
    block_centers = [
        # North row (between Y=-220 m and Y=-40 m)
        (-22000.0, -13000.0), (-1000.0, -13000.0), (20000.0, -13000.0),
        # South row (between Y=-40 m and Y=+160 m)
        (-22000.0,   6000.0), (-1000.0,   6000.0), (20000.0,   6000.0),
        # Outer Merkez apron (north of Y=-220 m, south of Y=+160 m)
        (-22000.0, -34000.0), (  2000.0, -34000.0), (19000.0, -34000.0),
        ( -4000.0,  29000.0), ( 22000.0,  28000.0), (-26000.0,  28000.0),
    ]
    cc0 = ctx.get("cc0", {})
    core_mats = ["MI_Building_Glass", "MI_Building_Modern", "MI_Building_ConcreteTower", "MI_Building_Brick", "MI_Building_Plaster"]
    edge_mats = ["MI_Building_Brick", "MI_Building_Plaster", "MI_Building_ConcreteTower"]
    crown_mats = ["MI_CrownGlow_Amber", "MI_CrownGlow_Cyan", "MI_CrownGlow_Crimson"]
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
                # Verify ALL 4 building footprint corners clear every adjacent road by >= 280 cm!
                if any(clearance(px + sx * w, py + sy * d)[0] < 280.0 for sx in (-0.5, 0.5) for sy in (-0.5, 0.5)):
                    continue
                # Tall modern neon financial towers near downtown core; mid-rise brick/plaster around edges
                is_core = (abs(px) < 22000.0 and abs(py) < 18000.0)
                h = rng.uniform(2400.0, 5200.0) if is_core else rng.uniform(1200.0, 2700.0)
                mat = rng.choice(core_mats) if is_core else rng.choice(edge_mats)
                hism.add("Bldg_Merkez", (px, py, h * 0.5), (w / 100.0, d / 100.0, h / 100.0),
                         mat, "PM_Concrete", collision=True, cast_shadow=True)
                # Rooftop parapet, stepped architectural setback penthouse on tall core towers, & water tanks
                hism.add("RoofCap", (px, py, h + 40.0), (w / 100.0 + 0.4, d / 100.0 + 0.4, 0.8),
                         "MI_Concrete", None, collision=False, cast_shadow=False, cull_cm=55000)
                if h >= 2800.0:
                    ph = rng.uniform(580.0, 1150.0)
                    pw, pd = w * 0.62, d * 0.62
                    hism.add("Bldg_Penthouse", (px, py, h + 80.0 + ph * 0.5),
                             (pw / 100.0, pd / 100.0, ph / 100.0),
                             mat, None, collision=False, cast_shadow=False, cull_cm=95000)
                    hism.add("RoofCap", (px, py, h + 80.0 + ph + 35.0),
                             (pw / 100.0 + 0.3, pd / 100.0 + 0.3, 0.7),
                             "MI_Concrete", None, collision=False, cast_shadow=False, cull_cm=65000)
                    # Glowing financial-district penthouse neon crown band (Altin Cekirdek signature!)
                    if mat in ("MI_Building_Glass", "MI_Building_Modern", "MI_Building_ConcreteTower"):
                        crown = crown_mats[(bi + int(px > 0) + int(py > 0)) % len(crown_mats)]
                        hism.add("CrownGlow", (px, py, h + 80.0 + ph + 6.0),
                                 (pw / 100.0 + 0.45, pd / 100.0 + 0.45, 0.28),
                                 crown, None, collision=False, cast_shadow=False, cull_cm=110000)
                    if mat in ("MI_Building_Glass", "MI_Building_Modern"):
                        hism.add("RoofSpire", (px, py, h + ph + 450.0), (0.35, 0.35, 7.8),
                                 "MI_Metal_Dark", None, collision=False, cast_shadow=False, cull_cm=85000)
                elif rng.random() < 0.42:
                    hism.add("RoofTank", (px + rng.uniform(-1200, 1200), py + rng.uniform(-1000, 1000), h + 80.0),
                             (1.0, 1.0, 1.0), None, None, collision=False,
                             mesh=meshes["SM_BN_RoofWaterTank"], cast_shadow=False, cull_cm=45000)
                # CC0 architectural wall detail (aircon units & fire escapes on brick/plaster mid-rises)
                if mat in ("MI_Building_Brick", "MI_Building_Plaster"):
                    if "SM_CC0_AirconUnit" in cc0 and rng.random() < 0.55:
                        hism.add("CC0_Aircon", (px, py + d * 0.5 + 18.0, rng.uniform(500.0, min(1400.0, h - 250.0))),
                                 (1.15, 1.15, 1.15), None, None, collision=False, yaw=90.0,
                                 mesh=cc0["SM_CC0_AirconUnit"], cast_shadow=False, cull_cm=22000)
                    if "SM_CC0_FireEscape" in cc0 and h >= 1400.0 and rng.random() < 0.35:
                        hism.add("CC0_FireEscape", (px + w * 0.5 + 15.0, py, 360.0),
                                 (1.0, 1.0, 1.0), None, None, collision=False, yaw=0.0,
                                 mesh=cc0["SM_CC0_FireEscape"], cast_shadow=False, cull_cm=28000)

    # Ground-floor Karamara storefronts flush against Merkez Bulvar (Y = -4000) & Carsi Caddesi (Y = +16000) facades
    for sx in (-26500.0, -21000.0, -15500.0, -5500.0, 500.0, 6000.0, 16500.0, 22500.0):
        # North side of Merkez Bulvar facing south (+Y -> yaw=+90)
        hism.add("Storefront", (sx, -5950.0, 0.0), (1.0, 1.0, 1.0), None, None, collision=False,
                 yaw=90.0, mesh=meshes["SM_BN_Storefront"], cast_shadow=False, cull_cm=42000)
        # Warm glowing canopy accent strip above storefront
        hism.add("StoreAccent", (sx, -5830.0, 445.0), (8.2, 0.16, 0.14), "MI_LampGlow", None,
                 collision=False, cast_shadow=False, cull_cm=32000)
        # South side of Merkez Bulvar facing north (-Y -> yaw=-90)
        hism.add("Storefront", (sx, -2050.0, 0.0), (1.0, 1.0, 1.0), None, None, collision=False,
                 yaw=-90.0, mesh=meshes["SM_BN_Storefront"], cast_shadow=False, cull_cm=42000)
        hism.add("StoreAccent", (sx, -2170.0, 445.0), (8.2, 0.16, 0.14), "MI_LampGlow", None,
                 collision=False, cast_shadow=False, cull_cm=32000)
    carsi_shops = (-25000.0, -17000.0, -5000.0, 3000.0, 16500.0, 23500.0)
    awning_mats = ("MI_Container_Red", "MI_Container_Blue", "MI_Container_Amber")
    for index, sx in enumerate(carsi_shops):
        hism.add("Storefront", (sx, 14450.0, 0.0), (1.0, 1.0, 1.0), None, None, collision=False,
                 yaw=90.0, mesh=meshes["SM_BN_Storefront"], cast_shadow=False, cull_cm=42000)
        hism.add("StoreAccent", (sx, 14560.0, 445.0), (8.0, 0.16, 0.14), "MI_CrownGlow_Amber", None,
                 collision=False, cast_shadow=False, cull_cm=30000)
        if clearance(sx, 14715.0)[0] > 460.0:
            hism.add("CarsiAwning", (sx, 14715.0, 375.0), (8.1, 1.9, 0.24),
                     awning_mats[index % len(awning_mats)], None, collision=False, cast_shadow=False, cull_cm=32000)

    # Eski Liman waterfront mooring bollards along concrete quay edge (X = +615 m)
    for by in range(-36000, 42000, 2800):
        hism.add("HarborBollard", (61550.0, float(by), 45.0), (0.55, 0.55, 0.90),
                 "MI_Metal_Dark", None, collision=False, cast_shadow=False, cull_cm=28000)

    # 2. Dogu Sanayi (Pasli Demir) Warehouses & Industrial Monitor Roofs
    sanayi_lots = [
        ( 6000.0, 52000.0, 24.0, 18.0, 12.0), (14000.0, 57000.0, 22.0, 16.0, 11.0),
        (26000.0, 54000.0, 20.0, 16.0, 10.0), (46000.0, 32000.0, 22.0, 14.0, 12.0),
        (65000.0, 30000.0, 18.0, 16.0, 11.0), (36000.0, 74000.0, 26.0, 14.0, 10.0),
    ]
    for wx, wy, ww, wd, wh in sanayi_lots:
        clr, _ = clearance(wx, wy)
        if clr > max(ww, wd) * 45.0:
            hism.add("Warehouse_Sanayi", (wx, wy, wh * 50.0), (ww, wd, wh),
                     rng.choice(["MI_Building_Steel", "MI_Building_Brick"]), "PM_Concrete",
                     collision=True, cast_shadow=True)
            # Industrial sawtooth/monitor skylight clerestory + hazard trim
            hism.add("Warehouse_Monitor", (wx, wy, wh * 100.0 + 110.0), (ww * 0.78, wd * 0.34, 2.2),
                     "MI_Metal_Dark", None, collision=False, cast_shadow=False, cull_cm=55000)
            hism.add("Warehouse_Beacon", (wx, wy, wh * 100.0 + 235.0), (ww * 0.76, 0.25, 0.25),
                     "MI_CrownGlow_Amber", None, collision=False, cast_shadow=False, cull_cm=45000)
            for bay in (-0.28, 0.0, 0.28):
                dx = wx + ww * 100.0 * bay
                dy = wy - wd * 50.0 - 12.0
                hism.add("SanayiBayDoor", (dx, dy, 190.0), (3.4, 0.24, 3.8),
                         "MI_Metal_Dark", None, collision=False, cast_shadow=False, cull_cm=28000)
                hism.add("SanayiBayLight", (dx, dy - 16.0, 422.0), (1.1, 0.23, 0.16),
                         "MI_CrownGlow_Amber", None, collision=False, cast_shadow=False, cull_cm=23000)

    # 3. Eski Liman (Mara Rihtimi) Maritime Depots with steel harbor trim
    for lx, ly in ((41000.0, -14000.0), (41000.0, 6000.0), (41000.0, 26000.0)):
        clr, _ = clearance(lx, ly)
        if clr > 650.0:
            hism.add("Depot_Liman", (lx, ly, 600.0), (14.0, 18.0, 12.0),
                     "MI_Building_Steel", "PM_Concrete", collision=True, cast_shadow=True)
            hism.add("Depot_Trim", (lx, ly, 1220.0), (14.4, 18.4, 0.5),
                     "MI_BridgeSteel", None, collision=False, cast_shadow=False, cull_cm=55000)

    # 4. Yamac Mahallesi (Eski Karamara) Terraced Hillside Villas with Mediterranean Terracotta Roofs
    yamac_lots = [
        (-42000.0,  26000.0, 1050.0), (-42000.0,  38000.0, 1300.0), (-62000.0,  24000.0, 1600.0),
        (-62000.0,  52000.0, 1800.0), (-34000.0,  42000.0,  800.0), (-50000.0,  64000.0, 1050.0),
        (-30000.0,  60000.0,  450.0), (-66000.0,  16000.0, 1250.0), (-46000.0,  28000.0, 1200.0),
        (-58000.0,  48000.0, 1650.0), (-68000.0,  32000.0, 2100.0), (-38000.0,  64000.0,  950.0),
        (-48000.0,  11000.0,  900.0), (-64000.0,  62000.0, 1350.0),
    ]
    for yx, yy, yz in yamac_lots:
        clr, _ = clearance(yx, yy)
        if clr > 680.0:
            h = rng.uniform(950.0, 1650.0)
            vw = rng.uniform(10.5, 15.0)
            vd = rng.uniform(9.5, 13.5)
            # Extend basement skirt downward so hillside villas sit firmly in the slope
            hism.add("House_Yamac", (yx, yy, yz + h * 0.5 - 300.0),
                     (vw, vd, (h + 600.0) / 100.0),
                     rng.choice(["MI_Building_Plaster", "MI_Building_Brick"]), "PM_Concrete",
                     collision=True, cast_shadow=True)
            # Iconic overhanging Anatolian/Mediterranean terracotta tile roof cap!
            hism.add("Roof_Terracotta", (yx, yy, yz + h + 45.0),
                     (vw + 0.9, vd + 0.9, 0.9),
                     "MI_Building_Terracotta", None, collision=False, cast_shadow=False, cull_cm=75000)
            hism.add("Roof_TerracottaRidge", (yx, yy, yz + h + 135.0),
                     (vw * 0.65, vd * 0.65, 0.9),
                     "MI_Building_Terracotta", None, collision=False, cast_shadow=False, cull_cm=65000)


def build_street_furniture_and_foliage(ctx, nodes, edges, clearance):
    """Places sodium street lamps across ALL 6 districts, intersection traffic lights,
    crosswalks, jersey barriers on highways/mountains, and HISM real-foliage trees/bushes."""
    spawn = ctx["spawn"]
    block = ctx["block"]
    place_mesh = ctx["place_mesh"]
    MI = ctx["MI"]
    meshes = ctx["meshes"]

    # Pre-index road segments & junction degrees so lamps and jersey barriers NEVER block an intersecting road!
    node_degree = {nid: 0 for nid in nodes}
    edge_segs = []
    for e in edges:
        node_degree[e["node_a"]] += 1
        node_degree[e["node_b"]] += 1
        ep = e["points"]
        hw = e["width"] * 0.5
        for k in range(len(ep) - 1):
            u, v = ep[k], ep[k + 1]
            edge_segs.append((
                e["index"], hw,
                min(u[0], v[0]) - hw - 600.0, max(u[0], v[0]) + hw + 600.0,
                min(u[1], v[1]) - hw - 600.0, max(u[1], v[1]) + hw + 600.0,
                u[0], u[1], u[2], v[0], v[1], v[2],
            ))

    def encroaches_road(x, y, z, pad_cm, z_tol_cm=320.0, ignore_edge_idx=-1):
        for eidx, hw, minx, maxx, miny, maxy, ax, ay, az, bx, by, bz in edge_segs:
            if eidx == ignore_edge_idx or x < minx or x > maxx or y < miny or y > maxy:
                continue
            vx, vy = bx - ax, by - ay
            l2 = vx * vx + vy * vy
            t = 0.0 if l2 < 1.0 else max(0.0, min(1.0, ((x - ax) * vx + (y - ay) * vy) / l2))
            qx, qy, qz = ax + vx * t, ay + vy * t, az + (bz - az) * t
            if abs(qz - z) <= z_tol_cm and math.hypot(x - qx, y - qy) < hw + pad_cm:
                return True
        return False

    junction_nodes = [(n["pos"][0], n["pos"][1], n["pos"][2]) for nid, n in nodes.items() if node_degree[nid] >= 3]

    # 1. Sodium street lamps on ALL 6 districts (strictly clear of ALL roads and intersections)
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
                off = hw + 220.0
                lx = 0.5 * (a[0] + b[0]) + nx * off * side
                ly = 0.5 * (a[1] + b[1]) + ny * off * side
                lz = 0.5 * (a[2] + b[2])
                # Skip lamps that fall too close to another lamp, any road asphalt, or any junction
                if any(math.hypot(lx - ox, ly - oy) < 2400.0 for ox, oy in lamp_positions):
                    continue
                if encroaches_road(lx, ly, lz, pad_cm=160.0, z_tol_cm=420.0, ignore_edge_idx=-1):
                    continue
                if any(math.hypot(lx - jx, ly - jy) < 1600.0 and abs(lz - jz) < 400.0 for jx, jy, jz in junction_nodes):
                    continue
                lamp_positions.append((lx, ly))
                yaw = math.degrees(math.atan2(-ny * side, -nx * side))
                lamp = spawn(unreal.BNStreetLamp, "Lamp", (lx, ly, lz), yaw=yaw, folder="Street/Lamps")
                lamp.get_editor_property("glow").set_material(0, MI["MI_LampGlow"])

    # 2. Traffic lights & Crosswalks at signalized intersections in Merkez / Sanayi / Liman / Yamac (HISM batched)
    hism = ctx["hism"]
    for nid, n in nodes.items():
        if not n["signalized"]:
            continue
        nx, ny, nz = n["pos"]
        hism.add("TrafficLight", (nx + 1050.0, ny + 1050.0, nz), (1.0, 1.0, 1.0), None, None,
                 collision=False, yaw=180.0, mesh=meshes["SM_BN_TrafficLight"], cast_shadow=False, cull_cm=34000)
        hism.add("TrafficLight", (nx - 1050.0, ny - 1050.0, nz), (1.0, 1.0, 1.0), None, None,
                 collision=False, yaw=0.0, mesh=meshes["SM_BN_TrafficLight"], cast_shadow=False, cull_cm=34000)
        # Crisp zebra crosswalk decals, stop bars & directional road arrows on approaches
        if nz < 50.0:
            hism.add("Crosswalk", (nx + 1250.0, ny, nz + 3.0), (3.8, 13.0, 0.01),
                     "MI_Crosswalk", None, collision=False, cast_shadow=False, cull_cm=26000)
            hism.add("Crosswalk", (nx, ny + 1250.0, nz + 3.0), (13.0, 3.8, 0.01),
                     "MI_Crosswalk", None, collision=False, cast_shadow=False, cull_cm=26000)
            hism.add("StopBar", (nx + 1580.0, ny + 320.0, nz + 2.6), (0.55, 6.2, 0.01),
                     "MI_LineWhite", None, collision=False, cast_shadow=False, cull_cm=24000)
            hism.add("StopBar", (nx - 320.0, ny + 1580.0, nz + 2.6), (6.2, 0.55, 0.01),
                     "MI_LineWhite", None, collision=False, cast_shadow=False, cull_cm=24000)
            hism.add("RoadArrow", (nx + 2250.0, ny + 360.0, nz + 3.0), (4.6, 2.2, 0.01),
                     "MI_RoadArrow", None, collision=False, yaw=180.0, cast_shadow=False, cull_cm=26000)
            hism.add("RoadArrow", (nx - 360.0, ny + 2250.0, nz + 3.0), (4.6, 2.2, 0.01),
                     "MI_RoadArrow", None, collision=False, yaw=-90.0, cast_shadow=False, cull_cm=26000)

    # 2b. Poly Haven CC0 1.0 micro street furniture along Merkez / Sanayi sidewalks & Kuzey Gecidi rocky outcrops
    cc0 = ctx.get("cc0", {})
    rng_cc0 = random.Random(2028)
    sidewalk_props = [k for k in (
        "SM_CC0_FireHydrant", "SM_CC0_TrashCan", "SM_CC0_UtilityBoxA",
        "SM_CC0_UtilityBoxB", "SM_CC0_StreetSeating"
    ) if k in cc0]
    if sidewalk_props:
        for e in edges:
            if e["highway"] or e["dirt"] or e["district"] not in (knet.DIST_MERKEZ, knet.DIST_SANAYI):
                continue
            pts = e["points"]
            hw = e["width"] * 0.5
            for idx_p in range(1, len(pts) - 1, 2):
                a, b = pts[idx_p], pts[min(len(pts) - 1, idx_p + 1)]
                seg = math.hypot(b[0] - a[0], b[1] - a[1])
                if seg < 50.0:
                    continue
                dx, dy = (b[0] - a[0]) / seg, (b[1] - a[1]) / seg
                nx_e, ny_e = -dy, dx
                yaw_e = math.degrees(math.atan2(dy, dx))
                side = 1.0 if ((idx_p + e["index"]) % 2 == 0) else -1.0
                px = a[0] + nx_e * (hw + 185.0) * side
                py = a[1] + ny_e * (hw + 185.0) * side
                pz = a[2] + 14.0  # Flush on raised sidewalk top
                if encroaches_road(px, py, a[2], pad_cm=120.0, z_tol_cm=260.0, ignore_edge_idx=-1):
                    continue
                if any(math.hypot(px - jx, py - jy) < 1750.0 and abs(a[2] - jz) < 350.0 for jx, jy, jz in junction_nodes):
                    continue
                pk = sidewalk_props[(e["index"] * 5 + idx_p) % len(sidewalk_props)]
                sc = 1.12 if pk == "SM_CC0_FireHydrant" else 1.0
                hism.add(pk, (px, py, pz), (sc, sc, sc), None, None, collision=False,
                         yaw=yaw_e + (90.0 if side > 0 else -90.0), mesh=cc0[pk], cast_shadow=False, cull_cm=22000)
                if "SM_CC0_ManholeCover" in cc0 and (idx_p % 4 == 1):
                    hism.add("SM_CC0_ManholeCover", (a[0] + nx_e * (hw * 0.42) * side, a[1] + ny_e * (hw * 0.42) * side, a[2] + 1.4),
                             (1.15, 1.15, 1.0), None, None, collision=False, yaw=yaw_e,
                             mesh=cc0["SM_CC0_ManholeCover"], cast_shadow=False, cull_cm=16000)
    if "SM_CC0_Boulder" in cc0:
        for e in edges:
            if e["district"] != knet.DIST_GECIT:
                continue
            for pt in e["points"][::2]:
                for side in (-1.0, 1.0):
                    bx = pt[0] + side * rng_cc0.uniform(1350.0, 2800.0)
                    by = pt[1] + rng_cc0.uniform(-1100.0, 1100.0)
                    clr, _ = clearance(bx, by)
                    if clr < 460.0:
                        continue
                    sc = rng_cc0.uniform(1.6, 3.1)
                    hism.add("SM_CC0_Boulder", (bx, by, max(0.0, pt[2] - 120.0)), (sc, sc, sc * 0.85),
                             None, None, collision=False, yaw=rng_cc0.uniform(0.0, 360.0),
                             mesh=cc0["SM_CC0_Boulder"], cast_shadow=False, cull_cm=42000)
    if "SM_CC0_ConcreteBarrier" in cc0:
        for b_loc, b_yaw in (
            ((-48000.0, -52000.0, 1200.0), 0.0), ((48000.0, -52000.0, 950.0), 0.0),
            ((54000.0, -18000.0, 0.0), 90.0), ((54000.0, 14000.0, 0.0), 90.0),
        ):
            bk = "SM_CC0_ConcreteBarrierB" if ("SM_CC0_ConcreteBarrierB" in cc0 and b_loc[0] > 0) else "SM_CC0_ConcreteBarrier"
            hism.add(bk, b_loc, (1.0, 1.0, 1.0), None, None, collision=False,
                     yaw=b_yaw, mesh=cc0[bk], cast_shadow=False, cull_cm=30000)

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

    h_autumn_a.set_cull_distances(0, 75000)
    h_autumn_b.set_cull_distances(0, 75000)
    h_pine.set_cull_distances(0, 85000)
    h_bush.set_cull_distances(0, 24000)
    h_barrier.set_cull_distances(0, 38000)

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
                    # Never block intersecting on-ramps, off-ramps, or multi-way junctions!
                    if encroaches_road(bx, by, pz, pad_cm=300.0, z_tol_cm=280.0, ignore_edge_idx=e["index"]):
                        continue
                    if any(math.hypot(bx - jx, by - jy) < 1750.0 and abs(pz - jz) < 350.0 for jx, jy, jz in junction_nodes):
                        continue
                    # Keep barriers clean of the bridge truss portals (X=64000 & X=84000) so entry/exit stay unobstructed
                    if abs(by - (-14000.0)) < 1300.0 and (63200.0 <= bx <= 64800.0 or 83200.0 <= bx <= 84800.0):
                        continue
                    h_barrier.add_instance(unreal.Transform(
                        location=unreal.Vector(bx, by, pz),
                        rotation=unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw),
                        scale=unreal.Vector(1.16, 1.0, 1.0),
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


def build_parked_cars(ctx, edges, clearance):
    """Parked cars in kerbside lots just off city streets: fills empty pavements without touching any lane."""
    meshes = ctx["meshes"]
    hism = ctx["hism"]
    car_keys = [k for k in ("SM_BN_Traffic_Sedan", "SM_BN_Traffic_Hatchback", "SM_BN_Traffic_SUV", "SM_BN_Traffic_Van") if k in meshes]
    if not car_keys:
        return 0
    solid = getattr(hism, "solid_boxes", [])
    rng = random.Random(2031)
    city = (knet.DIST_MERKEZ, knet.DIST_SANAYI, knet.DIST_LIMAN, knet.DIST_YAMAC)
    car_half = (260.0, 120.0)
    placed = []
    for e in edges:
        if e["highway"] or e["dirt"] or e["district"] not in city:
            continue
        pts = e["points"]
        hw = e["width"] * 0.5
        for i in range(0, len(pts) - 1):
            if rng.random() > 0.45:
                continue
            a, b = pts[i], pts[i + 1]
            seg = math.hypot(b[0] - a[0], b[1] - a[1])
            if seg < 900.0:
                continue
            dx, dy = (b[0] - a[0]) / seg, (b[1] - a[1]) / seg
            side = rng.choice((-1.0, 1.0))
            t = rng.uniform(0.25, 0.75)
            off = hw + rng.uniform(520.0, 700.0)  # beyond the kerb and sidewalk furniture line (hw + 185)
            px = a[0] + dx * seg * t - dy * off * side
            py = a[1] + dy * seg * t + dx * off * side
            clr, _ = clearance(px, py)
            if clr < 420.0:
                continue  # too close to some other road (junctions, parallel streets)
            r = max(car_half) + 80.0
            if any(x0 - r < px < x1 + r and y0 - r < py < y1 + r for x0, x1, y0, y1 in solid):
                continue
            if any(math.hypot(px - qx, py - qy) < 700.0 for qx, qy in placed):
                continue
            placed.append((px, py))
            key = car_keys[rng.randrange(len(car_keys))]
            yaw = math.degrees(math.atan2(dy, dx)) + (180.0 if rng.random() < 0.5 else 0.0) + rng.uniform(-6.0, 6.0)
            hism.add("Parked_" + key, (px, py, a[2] + 2.0), (1.0, 1.0, 1.0), None, None, collision=True,
                     yaw=yaw, mesh=meshes[key], cast_shadow=True, cull_cm=24000)
    unreal.log("BN: placed %d parked cars" % len(placed))
    return len(placed)
