"""Six-district scenery, landmarks, architecture, foliage HISM, signage, and street lighting
for BLACK NUMBERS' 2 km x 2 km Karamara open-world map.

Called by Tools/build_karamara_map.py inside UnrealEditor-Cmd Python.
"""
import math
import random

import unreal
import bn_building_meshes as bm_bldg
import karamara_network as knet


def _place_3d_bldg(hism, meshes, mesh_key, cx, cy, base_z, target_w, target_d, target_h, yaw=0.0, cull_cm=115000):
    """Places a true-3D multi-material architectural StaticMesh scaled proportionally around its
    native footprint so windows, balconies, awnings, and hip roofs stay crisp and un-warped."""
    sm = meshes.get(mesh_key)
    if sm is None:
        return False
    nw, nd, nh = bm_bldg.BUILDING_NATIVE_DIMS[mesh_key]
    sx = max(0.68, min(1.48, target_w / nw))
    sy = max(0.68, min(1.48, target_d / nd))
    # Prevent extreme horizontal aspect warping (> 1.45:1) so window proportions remain realistic
    if sx > sy * 1.42:
        sx = sy * 1.42
    elif sy > sx * 1.42:
        sy = sx * 1.42
    sz = max(0.72, min(1.45, target_h / nh))
    hism.add(mesh_key, (cx, cy, base_z), (sx, sy, sz), None, "PM_Concrete",
             collision=True, yaw=yaw, mesh=sm, cast_shadow=True, cull_cm=cull_cm)
    fw, fd = nw * sx, nd * sy
    hism.solid_boxes.append(_rect_aabb(cx, cy, fw, fd, yaw))
    return True


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

    # Wide open Sanayi Drift Asphalt Courtyard (flat smooth drift pad, 2 cm below road to prevent z-fighting)
    block("SanayiDriftYard", (44000.0, 54500.0, -14.5), (220.0, 190.0, 0.25),
          "MI_Asphalt", "PM_Asphalt", folder="Districts/DoguSanayi")

    # Sculpted organic 3D mountain & hillside terrain under Kuzey Gecidi (+62..+88 m) and Yamac Mahallesi (+26 m)
    hills = place_mesh("Karamara_SculptedHills", "SM_BN_KaramaraHills", (0.0, 0.0, 0.0), folder=folder)
    hills.static_mesh_component.set_phys_material_override(ctx["PM"]["PM_Grass"])
    # Seyir Tepesi flat paved overlook terrace at Z = +6197 cm (3 cm below road deck, zero z-fighting)
    block("SeyirTepesi_Overlook", (-60000.0, -86000.0, 6172.0), (46.0, 36.0, 0.5),
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
    block("Tunnel_RouteA_Foundation", (88000.0, -24000.0, 445.0), (24.0, 26.0, 3.1), "MI_Concrete", "PM_Concrete", folder=tf)
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

    # Tarik Oto Tamir & Arda Galeri flagship hall along north side of Sanayi Bulvari (storefronts flush on hall)
    block("TarikArda_Hall", (27000.0, 33000.0, 550.0), (36.0, 16.0, 11.0), "MI_Building_Brick", "PM_Concrete", folder=sf)
    place_mesh("TarikArda_Storefront_A", "SM_BN_Storefront", (25900.0, 33820.0, 0.0), yaw=-90.0, collision=False, folder=sf)
    place_mesh("TarikArda_Storefront_B", "SM_BN_Storefront", (28100.0, 33820.0, 0.0), yaw=-90.0, collision=False, folder=sf)

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
    block("SelinCourtyard_Pad", (gx - 2200.0, gy - 2000.0, gz - 22.0), (26.0, 22.0, 0.4),
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

    cc0 = ctx.get("cc0", {})
    if "SM_CC0_RollershutterDoor" in cc0:
        hism.add("CC0_Shutter", (gx - 2090.0, gy - 2100.0, gz), (1.1, 1.1, 1.1), None, None, collision=False, yaw=90.0, mesh=cc0["SM_CC0_RollershutterDoor"], cast_shadow=False, cull_cm=28000)
        hism.add("CC0_Shutter", (27000.0, 33815.0, 0.0), (1.3, 1.3, 1.3), None, None, collision=False, yaw=0.0, mesh=cc0["SM_CC0_RollershutterDoor"], cast_shadow=False, cull_cm=32000)
    if "SM_CC0_CoveredCar" in cc0:
        hism.add("CC0_CoveredCar", (gx - 1550.0, gy - 1350.0, gz + 4.0), (1.0, 1.0, 1.0), None, None, collision=False, yaw=18.0, mesh=cc0["SM_CC0_CoveredCar"], cast_shadow=True, cull_cm=26000)
    if "SM_CC0_ToolChest" in cc0:
        hism.add("CC0_ToolChest", (gx - 1950.0, gy - 3150.0, gz + 4.0), (1.15, 1.15, 1.15), None, None, collision=False, yaw=-15.0, mesh=cc0["SM_CC0_ToolChest"], cast_shadow=False, cull_cm=20000)
    if "SM_CC0_FloodLights" in cc0:
        for fl_loc, fl_yaw in (((gx - 1200.0, gy - 1100.0, gz + 4.0), -135.0), ((gx - 3100.0, gy - 1200.0, gz + 4.0), -45.0), ((36000.0, 48000.0, 0.0), 45.0), ((52000.0, 48000.0, 0.0), 135.0), ((52000.0, 62000.0, 0.0), -135.0), ((56500.0, -10000.0, 0.0), 180.0), ((56500.0, 14000.0, 0.0), 180.0)):
            hism.add("CC0_FloodLight", fl_loc, (1.15, 1.15, 1.15), None, None, collision=False, yaw=fl_yaw, mesh=cc0["SM_CC0_FloodLights"], cast_shadow=False, cull_cm=30000)
    if "SM_CC0_OldTyre" in cc0:
        for ty_loc in ((gx - 1800.0, gy - 3350.0, gz + 4.0), (gx - 1720.0, gy - 3350.0, gz + 38.0), (36200.0, 52000.0, 0.0), (36200.0, 52800.0, 0.0), (36200.0, 58000.0, 0.0), (51800.0, 52000.0, 0.0), (51800.0, 58500.0, 0.0)):
            hism.add("CC0_OldTyre", ty_loc, (1.2, 1.2, 1.2), None, None, collision=False, yaw=25.0, mesh=cc0["SM_CC0_OldTyre"], cast_shadow=False, cull_cm=22000)
    if "SM_CC0_Barrel" in cc0:
        for br_loc in ((gx - 1650.0, gy - 3250.0, gz + 4.0), (gx - 1570.0, gy - 3180.0, gz + 4.0), (35800.0, 50500.0, 0.0), (35920.0, 50640.0, 0.0), (52200.0, 50800.0, 0.0), (57200.0, -24000.0, 0.0), (57340.0, -23880.0, 0.0), (57200.0, 4000.0, 0.0), (57340.0, 4120.0, 0.0), (57200.0, 20000.0, 0.0)):
            hism.add("CC0_Barrel", br_loc, (1.1, 1.1, 1.1), None, None, collision=False, yaw=0.0, mesh=cc0["SM_CC0_Barrel"], cast_shadow=False, cull_cm=24000)
    if "SM_CC0_ChainlinkFence" in cc0:
        for fx in range(36500, 51500, 320):
            hism.add("CC0_Fence", (float(fx), 64200.0, 0.0), (1.0, 1.0, 1.0), None, None, collision=False, yaw=0.0, mesh=cc0["SM_CC0_ChainlinkFence"], cast_shadow=False, cull_cm=28000)
    # Expanded Poly Haven CC0 workshop, courtyard & maritime quay details
    if "SM_CC0_IronGate" in cc0:
        hism.add("CC0_IronGate", (gx - 950.0, gy - 1150.0, gz + 2.0), (1.25, 1.25, 1.25), None, None, collision=False, yaw=45.0, mesh=cc0["SM_CC0_IronGate"], cast_shadow=True, cull_cm=28000)
    if "SM_CC0_ToolCart" in cc0:
        hism.add("CC0_ToolCart", (gx - 1820.0, gy - 2950.0, gz + 4.0), (1.15, 1.15, 1.15), None, None, collision=False, yaw=20.0, mesh=cc0["SM_CC0_ToolCart"], cast_shadow=False, cull_cm=20000)
        hism.add("CC0_ToolCart", (26400.0, 34150.0, 4.0), (1.15, 1.15, 1.15), None, None, collision=False, yaw=-15.0, mesh=cc0["SM_CC0_ToolCart"], cast_shadow=False, cull_cm=22000)
    if "SM_CC0_WheelRim" in cc0:
        for rl in ((gx - 1600.0, gy - 3350.0, gz + 18.0), (gx - 1540.0, gy - 3320.0, gz + 18.0), (27500.0, 34200.0, 18.0)):
            hism.add("CC0_WheelRim", rl, (1.1, 1.1, 1.1), None, None, collision=False, yaw=35.0, mesh=cc0["SM_CC0_WheelRim"], cast_shadow=False, cull_cm=18000)
    if "SM_CC0_WoodenCrate" in cc0:
        for wc in ((56200.0, -22000.0, 2.0), (56290.0, -21920.0, 2.0), (56200.0, 6000.0, 2.0), (56280.0, 6100.0, 2.0), (35600.0, 52200.0, 2.0)):
            hism.add("CC0_WoodenCrate", wc, (1.4, 1.4, 1.4), None, None, collision=False, yaw=12.0, mesh=cc0["SM_CC0_WoodenCrate"], cast_shadow=False, cull_cm=24000)
    if "SM_CC0_Lifebuoy" in cc0:
        for ly in range(-32000, 36000, 11200):
            hism.add("CC0_Lifebuoy", (61480.0, float(ly), 95.0), (1.2, 1.2, 1.2), None, None, collision=False, yaw=90.0, mesh=cc0["SM_CC0_Lifebuoy"], cast_shadow=False, cull_cm=20000)
    if "SM_CC0_SecurityLight" in cc0:
        for sl_loc, sl_yaw in (((gx - 2080.0, gy - 2600.0, gz + 480.0), 0.0), ((27000.0, 33830.0, 480.0), 90.0)):
            hism.add("CC0_SecurityLight", sl_loc, (1.4, 1.4, 1.4), None, None, collision=False, yaw=sl_yaw, mesh=cc0["SM_CC0_SecurityLight"], cast_shadow=False, cull_cm=24000)


def _merkez_mesh_for(mat, h):
    if mat in ("MI_Building_Glass", "MI_Building_Modern"):
        return "SM_BN_Bldg_GlassTower"
    if mat == "MI_Building_ConcreteTower":
        return "SM_BN_Bldg_ConcreteTower" if h >= 2100.0 else "SM_BN_Bldg_CornerBlock"
    if mat == "MI_Building_Plaster":
        return "SM_BN_Bldg_AptPlaster"
    return "SM_BN_Bldg_AptBrick"


def _add_merkez_building(hism, meshes, px, py, w, d, h, mat, yaw=0.0):
    """One true-3D downtown building (recessed windows, balconies, shopfront arcade, awnings, hollow parapet)."""
    key = _merkez_mesh_for(mat, h)
    # Align longer building axis with longer lot axis
    if d > w * 1.15:
        yaw = yaw + 90.0
        w, d = d, w
    _place_3d_bldg(hism, meshes, key, px, py, 0.0, w, d, h, yaw=yaw)


def build_roofline(hism, cc0, brng, px, py, w, d, h, mat):
    """Adds rooftop CC0 aircon units and tall-tower telecom antennas on top of 3D downtown buildings."""
    if "SM_CC0_AirconUnit" in cc0 and h < 2800.0:
        for _ in range(brng.randint(1, 2)):
            hism.add("Roof_Aircon", (px + brng.uniform(-0.22, 0.22) * w, py + brng.uniform(-0.22, 0.22) * d, h + 15.0),
                     (2.0, 2.0, 2.0), None, None, collision=False, yaw=brng.choice((0.0, 90.0)),
                     mesh=cc0["SM_CC0_AirconUnit"], cast_shadow=False, cull_cm=28000)
    if h >= 3200.0 and brng.random() < 0.45:
        hism.add("Roof_Antenna", (px + w * 0.22, py - d * 0.22, h + 850.0), (0.16, 0.16, 16.0),
                 "MI_Metal_Dark", None, collision=False, cast_shadow=False, cull_cm=90000)


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
                # A city block is a row of 3 attached buildings (own widths, heights, facades) instead of one
                # giant box. Local rng: the seeded global layout stays identical. The middle building keeps the
                # block height so the penthouse/crown below sits on it.
                brng = random.Random(bi * 4099 + int(dx > 0) * 31 + int(dy > 0) * 7)
                along_x = w >= d
                span = w if along_x else d
                cuts = sorted((brng.uniform(0.25, 0.4), brng.uniform(0.6, 0.75)))
                bounds = (0.0, cuts[0], cuts[1], 1.0)
                pool = core_mats if is_core else edge_mats
                mid_w = mid_d = 0.0
                for k in range(3):
                    f0, f1 = bounds[k], bounds[k + 1]
                    off = (0.5 * (f0 + f1) - 0.5) * span
                    pw_k, pd_k = ((f1 - f0) * span, d) if along_x else (w, (f1 - f0) * span)
                    cx_k, cy_k = (px + off, py) if along_x else (px, py + off)
                    h_k = h if k == 1 else h * brng.uniform(0.6, 1.08)
                    mat_k = mat if k == 1 else pool[(pool.index(mat) + k + brng.randint(0, 2)) % len(pool)] if mat in pool else mat
                    if k == 1:
                        mid_w, mid_d = pw_k, pd_k
                    _add_merkez_building(hism, meshes, cx_k, cy_k, pw_k, pd_k, h_k, mat_k)
                build_roofline(hism, cc0, brng, px, py, mid_w, mid_d, h, mat)
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

    # Eski Liman waterfront mooring bollards along concrete quay edge (X = +615 m)
    for by in range(-36000, 42000, 2800):
        hism.add("HarborBollard", (61550.0, float(by), 45.0), (0.55, 0.55, 0.90),
                 "MI_Metal_Dark", None, collision=False, cast_shadow=False, cull_cm=28000)

    # 2. Dogu Sanayi (Pasli Demir) True-3D Industrial Monitor-Roof Factory Halls
    sanayi_lots = [
        ( 6000.0, 52000.0, 24.0, 18.0, 12.0), (14000.0, 57000.0, 22.0, 16.0, 11.0),
        (26000.0, 54000.0, 20.0, 16.0, 10.0), (46000.0, 32000.0, 22.0, 14.0, 12.0),
        (65000.0, 30000.0, 18.0, 16.0, 11.0), (36000.0, 74000.0, 26.0, 14.0, 10.0),
    ]
    for wx, wy, ww, wd, wh in sanayi_lots:
        clr, _ = clearance(wx, wy)
        if clr > max(ww, wd) * 45.0:
            _ = rng.choice(["MI_Building_Steel", "MI_Building_Brick"])  # keep rng cadence
            _place_3d_bldg(hism, meshes, "SM_BN_Bldg_FactoryHall", wx, wy, 0.0, ww * 100.0, wd * 100.0, wh * 100.0, yaw=0.0)

    # 3. Eski Liman (Mara Rihtimi) True-3D Maritime Transit Depots
    for lx, ly in ((41000.0, -14000.0), (41000.0, 6000.0), (41000.0, 26000.0)):
        clr, _ = clearance(lx, ly)
        if clr > 650.0:
            _place_3d_bldg(hism, meshes, "SM_BN_Bldg_HarborDepot", lx, ly, 0.0, 1800.0, 1400.0, 1150.0, yaw=90.0)

    # 4. Yamac Mahallesi (Eski Karamara) True-3D Hillside Mediterranean Villas with Shutters, Balconies & Hip Roofs
    yamac_lots = [
        (-42000.0,  26000.0, 1050.0), (-42000.0,  38000.0, 1300.0), (-62000.0,  24000.0, 1600.0),
        (-62000.0,  52000.0, 1800.0), (-34000.0,  42000.0,  800.0), (-50000.0,  64000.0, 1050.0),
        (-30000.0,  60000.0,  450.0), (-66000.0,  16000.0, 1250.0), (-46000.0,  28000.0, 1200.0),
        (-58000.0,  48000.0, 1650.0), (-68000.0,  32000.0, 2100.0), (-38000.0,  64000.0,  950.0),
        (-48000.0,  11000.0,  900.0), (-64000.0,  62000.0, 1350.0),
    ]
    for yi, (yx, yy, yz) in enumerate(yamac_lots):
        clr, _ = clearance(yx, yy)
        if clr > 680.0:
            h = rng.uniform(950.0, 1450.0)
            vw = rng.uniform(10.5, 14.5) * 100.0
            vd = rng.uniform(9.5, 13.0) * 100.0
            vmat = rng.choice(["MI_Building_Plaster", "MI_Building_Brick"])
            vkey = "SM_BN_Bldg_YamacVillaA" if vmat == "MI_Building_Plaster" else "SM_BN_Bldg_YamacVillaB"
            _place_3d_bldg(hism, meshes, vkey, yx, yy, yz, vw, vd, h, yaw=(yi % 4) * 90.0)


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

    # 2. Traffic lights at signalized intersections (aligned crosswalks & stop bars live in SM_BN_RoadNet_Markings)
    hism = ctx["hism"]
    for nid, n in nodes.items():
        if not n["signalized"]:
            continue
        nx, ny, nz = n["pos"]
        for sx, sy, tyaw in ((nx + 1150.0, ny + 1150.0, 180.0), (nx - 1150.0, ny - 1150.0, 0.0)):
            if not encroaches_road(sx, sy, nz, pad_cm=90.0, z_tol_cm=260.0):
                hism.add("TrafficLight", (sx, sy, nz), (1.0, 1.0, 1.0), None, None,
                         collision=False, yaw=tyaw, mesh=meshes["SM_BN_TrafficLight"], cast_shadow=False, cull_cm=34000)

    # 2b. Poly Haven CC0 1.0 micro street furniture along Merkez / Sanayi sidewalks & Kuzey Gecidi rocky outcrops
    cc0 = ctx.get("cc0", {})
    rng_cc0 = random.Random(2028)
    sidewalk_props = [k for k in (
        "SM_CC0_FireHydrant", "SM_CC0_TrashCan", "SM_CC0_UtilityBoxA",
        "SM_CC0_UtilityBoxB", "SM_CC0_StreetSeating", "SM_CC0_PlanterBox", "SM_CC0_TrashBag",
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
    if "SM_CC0_ElectricityPole" in cc0:
        for e in edges:
            if e["highway"] or e["dirt"] or e["district"] not in (knet.DIST_SANAYI, knet.DIST_YAMAC):
                continue
            pts = e["points"]
            hw = e["width"] * 0.5
            for idx_p in range(1, len(pts) - 1, 3):
                a, b = pts[idx_p], pts[min(len(pts) - 1, idx_p + 1)]
                seg = math.hypot(b[0] - a[0], b[1] - a[1])
                if seg < 80.0:
                    continue
                dx, dy = (b[0] - a[0]) / seg, (b[1] - a[1]) / seg
                px, py, pz = a[0] - dy * (hw + 270.0), a[1] + dx * (hw + 270.0), a[2]
                if encroaches_road(px, py, pz, pad_cm=160.0, z_tol_cm=360.0) or any(math.hypot(px - jx, py - jy) < 1800.0 for jx, jy, _ in junction_nodes):
                    continue
                hism.add("SM_CC0_ElectricityPole", (px, py, pz), (1.0, 1.0, 1.0), None, None, collision=False,
                         yaw=math.degrees(math.atan2(dy, dx)), mesh=cc0["SM_CC0_ElectricityPole"], cast_shadow=False, cull_cm=34000)
    if "SM_CC0_Shrub" in cc0:
        for e in edges:
            if e["district"] not in (knet.DIST_YAMAC, knet.DIST_GECIT):
                continue
            for pt in e["points"][::3]:
                sx, sy = pt[0] + rng_cc0.uniform(-1600.0, 1600.0), pt[1] + rng_cc0.uniform(-1600.0, 1600.0)
                if clearance(sx, sy)[0] < 480.0:
                    continue
                hism.add("SM_CC0_Shrub", (sx, sy, max(0.0, pt[2] - 20.0)), (1.5, 1.5, 1.5), None, None,
                         collision=False, yaw=rng_cc0.uniform(0.0, 360.0), mesh=cc0["SM_CC0_Shrub"], cast_shadow=False, cull_cm=26000)
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

    # 3. Real-foliage HISM (SubobjectDataSubsystem persists components into .umap cleanly)
    hism_actor = spawn(unreal.Actor, "Karamara_FoliageAndBarriers_HISM", (0.0, 0.0, 0.0), folder="Environment/HISM")
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    actor_h = root_h = None
    for h in sds.k2_gather_subobject_data_for_instance(hism_actor):
        d = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h)
        if unreal.SubobjectDataBlueprintFunctionLibrary.is_root_component(d): root_h = h
        elif unreal.SubobjectDataBlueprintFunctionLibrary.is_actor(d): actor_h = h
    if not root_h:
        root_h, err = sds.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=actor_h, new_class=unreal.SceneComponent.static_class(), blueprint_context=None))
        if not err.is_empty(): raise RuntimeError(str(err))
        sds.rename_subobject(handle=root_h, new_name=unreal.Text("HismRoot"))

    def make_hism(comp_name, mesh_name, cast_shadow=True, cull_cm=75000):
        ch, err = sds.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=root_h, new_class=unreal.HierarchicalInstancedStaticMeshComponent.static_class(), blueprint_context=None))
        if not err.is_empty(): raise RuntimeError(str(err))
        sds.rename_subobject(handle=ch, new_name=unreal.Text(comp_name))
        comp = unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(ch))
        comp.set_static_mesh(meshes[mesh_name])
        comp.set_editor_property("cast_shadow", cast_shadow)
        comp.set_collision_profile_name("NoCollision")
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        comp.set_cull_distances(0, cull_cm)
        return comp

    h_autumn_a = make_hism("AutumnTreesA_HISM", "SM_BN_Tree_Autumn_A", True, 75000)
    h_autumn_b = make_hism("AutumnTreesB_HISM", "SM_BN_Tree_Autumn_B", True, 75000)
    h_pine = make_hism("PineTrees_HISM", "SM_BN_Tree_Pine", True, 85000)
    h_bush = make_hism("Bushes_HISM", "SM_BN_Bush", False, 24000)

    rng = random.Random(777)
    beacon_keep = _gameplay_keep_out()

    # 3a. Boulevard autumn trees framing Merkez Bulvar & Carsi sidewalks (strictly clear of beacons & walls!)
    for tx in range(-30000, 30000, 4200):
        for ty in (-5550.0, -2450.0, 14550.0, 17450.0):
            if any(abs(tx - ix) < 2400.0 for ix in (-32000, -12000, 10000, 30000)):
                continue
            if any(math.hypot(tx - kx, ty - ky) < kr for kx, ky, kr in beacon_keep):
                continue
            sc = rng.uniform(0.85, 1.15)
            xf = unreal.Transform(unreal.Vector(tx + rng.uniform(-160, 160), ty, 0.0), unreal.Rotator(0.0, 0.0, rng.uniform(0.0, 360.0)), unreal.Vector(sc, sc, sc))
            (h_autumn_a if rng.random() < 0.55 else h_autumn_b).add_instance(xf, True)

    # 3b. Scattered autumn groves & shrubs (grounded strictly on land, never floating by viaducts/beacons)
    for _ in range(360):
        tx, ty = rng.uniform(-90000.0, 46000.0), rng.uniform(-92000.0, 86000.0)
        if not _on_land(tx, ty) or tx > 48000.0 or (31000.0 < tx < 57000.0 and 43000.0 < ty < 66000.0):
            continue
        clr, near_z = clearance(tx, ty)
        if clr < 460.0 or clr > 24000.0:
            continue
        if any(math.hypot(tx - kx, ty - ky) < kr for kx, ky, kr in beacon_keep):
            continue
        is_hill = (ty < -46000.0 or tx < -30000.0)
        if not is_hill and near_z > 260.0:
            continue
        if -30000.0 < tx < 28000.0 and -24000.0 < ty < 18000.0 and clr > 950.0:
            continue
        sc = rng.uniform(0.85, 1.38)
        tz = max(0.0, near_z - 35.0) if is_hill else _ground_z_for(tx, ty, 0.0, False)
        xf = unreal.Transform(unreal.Vector(tx, ty, tz), unreal.Rotator(0.0, 0.0, rng.uniform(0.0, 360.0)), unreal.Vector(sc, sc, sc))
        (h_pine if (ty < -46000.0 or near_z > 1800.0) else h_bush if rng.random() < 0.35 else h_autumn_a if rng.random() < 0.55 else h_autumn_b).add_instance(xf, True)

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


PARKED_PAINTS = ("MI_Traffic_Paint", "MI_Parked_Red", "MI_Parked_Blue", "MI_Parked_White",
                 "MI_Parked_Black", "MI_Parked_Green", "MI_Parked_Taxi")


def build_parked_cars(ctx, edges, clearance):
    """Parked cars in flat ground-level kerbside bays (strictly grounded, on land, never floating on slopes)."""
    meshes = ctx["meshes"]
    hism = ctx["hism"]
    car_keys = [k for k in ("SM_BN_Traffic_Sedan", "SM_BN_Traffic_Hatchback", "SM_BN_Traffic_SUV", "SM_BN_Traffic_Van") if k in meshes]
    if not car_keys:
        return 0
    solid = getattr(hism, "solid_boxes", [])
    keep_out = _gameplay_keep_out()
    rng = random.Random(2031)
    flat_city = (knet.DIST_MERKEZ, knet.DIST_SANAYI, knet.DIST_LIMAN)
    car_half = (260.0, 120.0)
    placed = []
    for e in edges:
        if e["highway"] or e["dirt"] or e["district"] not in flat_city:
            continue
        pts = e["points"]
        hw = e["width"] * 0.5
        for i in range(0, len(pts) - 1):
            if rng.random() > 0.45:
                continue
            a, b = pts[i], pts[i + 1]
            # Strictly flat ground-level edges only — never spawn floating cars beside ramps or bridges
            if abs(a[2]) > 30.0 or abs(b[2]) > 30.0:
                continue
            seg = math.hypot(b[0] - a[0], b[1] - a[1])
            if seg < 900.0:
                continue
            dx, dy = (b[0] - a[0]) / seg, (b[1] - a[1]) / seg
            side = rng.choice((-1.0, 1.0))
            t = rng.uniform(0.25, 0.75)
            off = hw + rng.uniform(480.0, 650.0)
            px = a[0] + dx * seg * t - dy * off * side
            py = a[1] + dy * seg * t + dx * off * side
            if not _on_land(px, py):
                continue
            clr, near_z = clearance(px, py)
            if clr < 400.0 or abs(near_z) > 80.0:
                continue
            if any(math.hypot(px - kx, py - ky) < kr for kx, ky, kr in keep_out):
                continue
            r = max(car_half) + 90.0
            if any(x0 - r < px < x1 + r and y0 - r < py < y1 + r for x0, x1, y0, y1 in solid):
                continue
            if any(math.hypot(px - qx, py - qy) < 700.0 for qx, qy in placed):
                continue
            placed.append((px, py))
            key = car_keys[rng.randrange(len(car_keys))]
            yaw = math.degrees(math.atan2(dy, dx)) + (180.0 if rng.random() < 0.5 else 0.0) + rng.uniform(-6.0, 6.0)
            paint = rng.choice(PARKED_PAINTS)
            hism.add("Parked_" + key, (px, py, _ground_z_for(px, py, 0.0, False) + 2.0), (1.0, 1.0, 1.0),
                     paint, None, collision=True, yaw=yaw, mesh=meshes[key], cast_shadow=True, cull_cm=24000)
    unreal.log("BN: placed %d parked cars" % len(placed))
    return len(placed)


# District frontage styles: (materials, height range cm, width range cm, depth range cm, setback cm, kind)
_INFILL_STYLES = {
    knet.DIST_MERKEZ: (("MI_Building_Brick", "MI_Building_Plaster", "MI_Building_ConcreteTower", "MI_Building_Modern"),
                       (1300.0, 2700.0), (1600.0, 2800.0), (1400.0, 2200.0), 420.0, "block"),
    knet.DIST_SANAYI: (("MI_Building_Steel", "MI_Container_Blue", "MI_Container_Red", "MI_Container_Amber"),
                       (650.0, 1100.0), (1800.0, 3200.0), (1400.0, 2400.0), 900.0, "shed"),
    knet.DIST_LIMAN:  (("MI_Building_Steel", "MI_Container_Blue", "MI_Building_Plaster"),
                       (800.0, 1400.0), (1800.0, 3000.0), (1400.0, 2200.0), 850.0, "shed"),
    knet.DIST_YAMAC:  (("MI_Building_Plaster", "MI_Building_Brick"),
                       (650.0, 1050.0), (900.0, 1400.0), (850.0, 1250.0), 700.0, "house"),
}


def _rect_aabb(px, py, w, d, yaw_deg):
    c, s = abs(math.cos(math.radians(yaw_deg))), abs(math.sin(math.radians(yaw_deg)))
    hx, hy = 0.5 * (w * c + d * s), 0.5 * (w * s + d * c)
    return (px - hx, px + hx, py - hy, py + hy)


def _ground_z_for(x, y, near_z, is_hillside):
    """Returns guaranteed grounded base Z so no building ever floats over water, quays, or elevated ramps."""
    if is_hillside:
        return max(0.0, near_z)
    # East Harbor peninsula deck top sits at Z = +300 cm; main landmass sits at Z = 0 cm
    return 300.0 if x > 81000.0 else 0.0


def _add_district_building(hism, meshes, kind, mat, cx, cy, z, w, d, h, yaw):
    """One true-3D district-styled architectural building (block / shed / house) grounded cleanly on terrain."""
    if kind == "house":
        key = "SM_BN_Bldg_YamacVillaA" if mat == "MI_Building_Plaster" else "SM_BN_Bldg_YamacVillaB"
    elif kind == "shed":
        key = "SM_BN_Bldg_HarborDepot" if mat == "MI_Building_Plaster" or mat == "MI_Container_Blue" else "SM_BN_Bldg_FactoryHall"
    else:
        key = _merkez_mesh_for(mat, h)
    _place_3d_bldg(hism, meshes, key, cx, cy, z, w, d, h, yaw=yaw)


def _gameplay_keep_out():
    """Gameplay spaces (incl. all 11 race start beacons) stay unobstructed by buildings, props, trees & storefronts."""
    keep_out = [(c["loc"][0], c["loc"][1], c["radius"] + 900.0) for c in knet.COOLDOWN_SPOTS]
    keep_out += [(knet.SELIN_GARAGE_POS[0], knet.SELIN_GARAGE_POS[1], 3500.0)]
    keep_out += [(b[1], b[2], 2200.0) for b in knet.PURSUIT_BREAKERS]
    keep_out += [(t[1], t[2], 1800.0) for t in knet.SPEED_TRAPS]
    keep_out += [(j[1], j[2], 6500.0) for j in knet.STUNT_JUMPS]
    keep_out += [(r[0], r[1], 2500.0) for r in (knet.ESCAPE_ROUTE_A, knet.ESCAPE_ROUTE_B)]
    nodes, edges, lookup = knet.build_network()
    for spec in knet.build_race_specs(nodes, edges, lookup):
        bx, by, _bz = spec["beacon"]
        keep_out.append((bx, by, 1050.0))
    return keep_out


def build_frontage_infill(ctx, edges, clearance):
    """Lines city streets with grounded district-styled buildings strictly on land and clear of elevated ramps."""
    hism = ctx["hism"]
    meshes = ctx["meshes"]
    solid = hism.solid_boxes
    placed = 0
    keep_out = _gameplay_keep_out()
    for e in edges:
        style = _INFILL_STYLES.get(e["district"])
        if style is None or e["dirt"] or e["highway"]:
            continue
        mats, h_rng, w_rng, d_rng, setback, kind = style
        is_hill = (kind == "house")
        rng = random.Random(7001 + e["index"] * 131)
        pts = e["points"]
        hw = e["width"] * 0.5
        for i in range(len(pts) - 1):
            a, b = pts[i], pts[i + 1]
            if not is_hill and (abs(a[2]) > 140.0 or abs(b[2]) > 140.0):
                continue
            seg = math.hypot(b[0] - a[0], b[1] - a[1])
            if seg < 1200.0:
                continue
            dx, dy = (b[0] - a[0]) / seg, (b[1] - a[1]) / seg
            road_yaw = math.degrees(math.atan2(dy, dx))
            for side in (-1.0, 1.0):
                t = rng.uniform(300.0, 900.0)
                while t < seg - 600.0:
                    w, d, h = rng.uniform(*w_rng), rng.uniform(*d_rng), rng.uniform(*h_rng)
                    off = hw + setback + d * 0.5
                    cx = a[0] + dx * (t + w * 0.5) - dy * off * side
                    cy = a[1] + dy * (t + w * 0.5) + dx * off * side
                    t += w + (rng.uniform(0.0, 120.0) if kind == "block" else rng.uniform(150.0, 450.0))
                    yaw = road_yaw if kind == "shed" else round(road_yaw / 90.0) * 90.0
                    box = _rect_aabb(cx, cy, w, d, yaw)
                    corners = ((box[0], box[2]), (box[0], box[3]), (box[1], box[2]), (box[1], box[3]), (cx, cy))
                    if not all(_on_land(x, y) for x, y in corners):
                        continue
                    if any(clearance(x, y)[0] < 360.0 for x, y in corners):
                        continue
                    near_z = clearance(cx, cy)[1]
                    if not is_hill and near_z > 220.0:
                        continue
                    reach = 0.5 * math.hypot(w, d)
                    if any(math.hypot(cx - kx, cy - ky) < kr + reach for kx, ky, kr in keep_out):
                        continue
                    if any(box[0] - 250.0 < x1 and box[1] + 250.0 > x0 and box[2] - 250.0 < y1 and box[3] + 250.0 > y0
                           for x0, x1, y0, y1 in solid):
                        continue
                    base_z = _ground_z_for(cx, cy, near_z, is_hill)
                    _add_district_building(hism, meshes, kind, rng.choice(mats), cx, cy, base_z, w, d, h, yaw)
                    solid.append(box)
                    placed += 1
    unreal.log("BN: placed %d frontage infill buildings" % placed)
    return placed


def _on_land(x, y):
    """Main landmass ends at the Mara seawall (x ~ 600 m); East Harbor peninsula sits east of the bridge span."""
    return x < 59500.0 or (83500.0 < x < 105500.0 and -36500.0 < y < 20500.0)


def build_block_interiors(ctx, edges, clearance):
    """Fills the land behind the frontage rows: grounded back-row buildings, pocket parks & courtyard car parks."""
    hism = ctx["hism"]
    meshes = ctx["meshes"]
    solid = hism.solid_boxes
    keep_out = _gameplay_keep_out()
    trees = [k for k in ("SM_BN_Tree_Autumn_A", "SM_BN_Tree_Autumn_B") if k in meshes]
    cars = [k for k in ("SM_BN_Traffic_Sedan", "SM_BN_Traffic_Hatchback", "SM_BN_Traffic_SUV") if k in meshes]
    counts = {"building": 0, "park": 0, "carpark": 0}
    step = 3200.0
    for zone in knet.DISTRICT_BOUNDS:
        style = _INFILL_STYLES.get(zone["district"])
        if style is None:
            continue
        mats, h_rng, w_rng, d_rng, _setback, kind = style
        is_hill = (kind == "house")
        rng = random.Random(9100 + zone["district"] * 37)
        (zx, zy), (hx, hy) = zone["center"], zone["half"]
        max_clear = 3400.0 if is_hill else 1.0e9
        y = zy - hy + step * 0.5
        while y < zy + hy:
            x = zx - hx + step * 0.5
            while x < zx + hx:
                cx, cy = x + rng.uniform(-500.0, 500.0), y + rng.uniform(-500.0, 500.0)
                x += step
                roll = rng.random()
                clr, near_z = clearance(cx, cy)
                half = 1450.0
                corners = ((cx - half, cy - half), (cx - half, cy + half),
                           (cx + half, cy - half), (cx + half, cy + half), (cx, cy))
                if not all(_on_land(px, py) for px, py in corners) or clr < 1300.0 or clr > max_clear:
                    continue
                # Never spawn interior blocks under elevated bridge/viaduct decks or in Sanayi Drift Courtyard
                if not is_hill and near_z > 320.0:
                    continue
                if 33000.0 <= cx <= 55500.0 and 45000.0 <= cy <= 64500.0:
                    continue
                if any(math.hypot(cx - kx, cy - ky) < kr + 1800.0 for kx, ky, kr in keep_out):
                    continue
                if any(cx - half < x1 and cx + half > x0 and cy - half < y1 and cy + half > y0 for x0, x1, y0, y1 in solid):
                    continue
                z = _ground_z_for(cx, cy, near_z, is_hill)
                if roll < 0.55:
                    w, d = min(rng.uniform(*w_rng), 2700.0), min(rng.uniform(*d_rng), 2700.0)
                    _add_district_building(hism, meshes, kind, rng.choice(mats), cx, cy, z, w, d, rng.uniform(*h_rng),
                                           rng.choice((0.0, 90.0)))
                    solid.append((cx - w * 0.5, cx + w * 0.5, cy - d * 0.5, cy + d * 0.5))
                    counts["building"] += 1
                elif roll < 0.75 and trees:
                    hism.add("Park_Lawn", (cx, cy, z + 3.0), (26.0, 26.0, 0.06), "MI_Grass", None,
                             collision=False, cast_shadow=False, cull_cm=45000)
                    for _ in range(rng.randint(3, 6)):
                        hism.add("Park_" + trees[0], (cx + rng.uniform(-1000.0, 1000.0), cy + rng.uniform(-1000.0, 1000.0), z),
                                 (1.0, 1.0, rng.uniform(0.85, 1.2)), None, None, collision=False,
                                 yaw=rng.uniform(0.0, 360.0), mesh=meshes[rng.choice(trees)], cast_shadow=True, cull_cm=40000)
                    counts["park"] += 1
                elif cars and not is_hill:
                    hism.add("Carpark_Pad", (cx, cy, z + 2.0), (24.0, 18.0, 0.04), "MI_Asphalt", None,
                             collision=False, cast_shadow=False, cull_cm=40000)
                    for k in range(rng.randint(2, 6)):
                        hism.add("Carpark_Car", (cx - 900.0 + k * 360.0, cy + rng.choice((-450.0, 450.0)), z + 4.0),
                                 (1.0, 1.0, 1.0), rng.choice(PARKED_PAINTS), None, collision=True, yaw=90.0,
                                 mesh=meshes[rng.choice(cars)], cast_shadow=True, cull_cm=24000)
                    counts["carpark"] += 1
                solid.append((cx - half, cx + half, cy - half, cy + half))
            y += step
    unreal.log("BN: block interiors: %d buildings, %d pocket parks, %d car parks" % (
        counts["building"], counts["park"], counts["carpark"]))
    return counts


def build_road_boundaries(ctx, edges):
    """Kept as clean entry point; continuous vertex-welded curbs, walls & barriers are now built
    directly inside Tools/bn_road_mesh.py (SM_BN_RoadNet_Sidewalk / SM_BN_RoadNet_Wall) with zero gap."""
    unreal.log("BN: road boundaries welded into SM_BN_RoadNet_Wall (0 legacy block pieces)")
    return 0
