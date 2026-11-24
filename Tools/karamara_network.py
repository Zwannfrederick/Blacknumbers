"""Single source of truth for BLACK NUMBERS' ~2.0 km x 2.0 km Karamara open-world city:
- 6 canon districts (Merkez Bulvar, Dogu Sanayi, Eski Liman + Kopru, Kuzey Viyadugu & Cevre Yolu,
  Yamac Mahallesi + Selin'in Garaji, Kuzey Gecidi & Seyir Tepesi up to Z = +62 m)
- Curved & sloped Catmull-Rom road graph (Nodes + Edges with exact 3D sampled centerlines)
- 9 unique race routes derived directly from the road graph
- 6 Cooldown hiding spots, 10 multi-type Pursuit Breakers, Perde 5 Escape Portals A & B

All units are UE centimetres (1 m = 100 cm). +X = East (toward Mara Sea), +Y = South, +Z = Up.
Run standalone to validate graph connectivity and road grades:
    python3 Tools/karamara_network.py --check
"""
import math
import sys

# District integer IDs matching C++ EBNDistrict enum order
DIST_MERKEZ = 0
DIST_SANAYI = 1
DIST_LIMAN = 2
DIST_VIYADUK = 3
DIST_YAMAC = 4
DIST_GECIT = 5

DISTRICT_BOUNDS = [
    dict(district=DIST_MERKEZ,  center=(-4000.0,   -2000.0), half=(34000.0, 26000.0),
         name="MERKEZ BULVAR & ÇARŞI",        subtitle="KARAMARA ŞEHİR MERKEZİ • TİCARET HATTI"),
    dict(district=DIST_SANAYI,  center=(38000.0,   46000.0), half=(30000.0, 28000.0),
         name="DOĞU SANAYİ",                  subtitle="TARIK OTO TAMİR • ARDA GALERİ • DRİFT AVLUSU"),
    dict(district=DIST_LIMAN,   center=(72000.0,   -4000.0), half=(30000.0, 44000.0),
         name="ESKİ LİMAN",                   subtitle="MARA RIHTIM • ESKİ LİMAN KÖPRÜSÜ"),
    dict(district=DIST_VIYADUK, center=(0.0,      -54000.0), half=(66000.0, 16000.0),
         name="KUZEY VİYADÜĞÜ & ÇEVRE YOLU",  subtitle="O-5 YÜKSEK HIZ OTOYOLU"),
    dict(district=DIST_YAMAC,   center=(-58000.0,  42000.0), half=(32000.0, 34000.0),
         name="YAMAÇ MAHALLESİ",              subtitle="SELİN'İN GARAJI • YOKUŞ SOKAKLAR"),
    dict(district=DIST_GECIT,   center=(-56000.0, -72000.0), half=(42000.0, 28000.0),
         name="KUZEY GEÇİDİ & SEYİR TEPESİ",  subtitle="DAĞ SAÇ TOKALARI • EMRE ANITI"),
]

# Scenic title-screen & free-roam PlayerStart inside Merkez Bulvar looking east down the avenue
# toward storefronts, autumn trees, highway gantry, downtown skyline and Eski Liman Koprusu!
PLAYER_START = (-18000.0, -4000.0, 115.0, 0.0)  # x, y, z, yaw_deg (Z=115 clears UE PlayerStart 92cm capsule)

# Selin's hidden workshop courtyard in Yamac Mahallesi (elevated hillside, Z = +16 m)
SELIN_GARAGE_POS = (-54000.0, 36000.0, 1600.0, -25.0)

# Act 5 Escape Portals
ESCAPE_ROUTE_A = (88000.0, -24000.0, 600.0)   # Eski Liman Tuneli (East Peninsula after Bridge)
ESCAPE_ROUTE_B = (-76000.0, -82000.0, 4600.0) # Kuzey Gecidi Toprak Servis Yolu

# 6 Cooldown Hiding Spots across all 6 districts
COOLDOWN_SPOTS = [
    dict(loc=(-24000.0,  14000.0,    0.0), radius=1400.0, district=DIST_MERKEZ,  name="Çarşı Arka Avlusu"),
    dict(loc=( 46000.0,  56000.0,    0.0), radius=1600.0, district=DIST_SANAYI,  name="Sanayi Konteyner Cebi"),
    dict(loc=( 56000.0,  18000.0,    0.0), radius=1500.0, district=DIST_LIMAN,   name="Eski Liman Antreposu"),
    dict(loc=(  4000.0, -40000.0,    0.0), radius=1500.0, district=DIST_VIYADUK, name="Viyadük Altı Sığınağı"),
    dict(loc=(-54000.0,  36000.0, 1600.0), radius=1500.0, district=DIST_YAMAC,   name="Selin'in Garaj Avlusu"),
    dict(loc=(-60000.0, -86000.0, 6200.0), radius=1600.0, district=DIST_GECIT,   name="Seyir Tepesi Gözlem Cebi"),
]

# ---------------------------------------------------------------------------
# Road Graph Nodes: id -> (x, y, z, district, signalized)
# Carefully laid out across ~1.9 km x 1.9 km with gentle <= 9.5% grades.
# ---------------------------------------------------------------------------
RAW_NODES = {
    # --- Merkez Bulvar & Carsi (Z = 0) ---
    0:  (-32000.0, -22000.0,    0.0, DIST_MERKEZ,  True),
    1:  (-12000.0, -22000.0,    0.0, DIST_MERKEZ,  True),
    2:  ( 10000.0, -22000.0,    0.0, DIST_MERKEZ,  True),
    3:  ( 30000.0, -22000.0,    0.0, DIST_MERKEZ,  True),

    4:  (-32000.0,  -4000.0,    0.0, DIST_MERKEZ,  True),
    5:  (-12000.0,  -4000.0,    0.0, DIST_MERKEZ,  True),
    6:  ( 10000.0,  -4000.0,    0.0, DIST_MERKEZ,  True),
    7:  ( 30000.0,  -4000.0,    0.0, DIST_MERKEZ,  True),

    8:  (-32000.0,  16000.0,    0.0, DIST_MERKEZ,  True),
    9:  (-12000.0,  16000.0,    0.0, DIST_MERKEZ,  True),
    10: ( 10000.0,  16000.0,    0.0, DIST_MERKEZ,  True),
    11: ( 30000.0,  16000.0,    0.0, DIST_MERKEZ,  True),

    # --- Dogu Sanayi (Z = 0) ---
    12: ( 16000.0,  42000.0,    0.0, DIST_SANAYI,  True),
    13: ( 38000.0,  42000.0,    0.0, DIST_SANAYI,  True),
    14: ( 60000.0,  42000.0,    0.0, DIST_SANAYI,  False),
    15: ( 22000.0,  68000.0,    0.0, DIST_SANAYI,  False),
    16: ( 56000.0,  68000.0,    0.0, DIST_SANAYI,  False),

    # --- Eski Liman & Eski Liman Koprusu (Z = 0..+16 m) ---
    17: ( 50000.0, -24000.0,    0.0, DIST_LIMAN,   True),   # Liman North Dock
    18: ( 50000.0,  -4000.0,    0.0, DIST_LIMAN,   True),   # Liman Central Quay
    19: ( 50000.0,  20000.0,    0.0, DIST_LIMAN,   False),  # Liman South Quay
    20: ( 62000.0, -14000.0, 1600.0, DIST_LIMAN,   False),  # Bridge West Tower Deck (+16 m)
    21: ( 84000.0, -14000.0, 1600.0, DIST_LIMAN,   False),  # Bridge East Tower Deck (+16 m)
    22: ( 88000.0,   8000.0,  600.0, DIST_LIMAN,   False),  # East Harbor Peninsula Loop
    23: ( 88000.0, -24000.0,  600.0, DIST_LIMAN,   False),  # Eski Liman Tunnel Portal (Route A)

    # --- Kuzey Viyadugu & O-5 Cevre Yolu Perimeter Highway ---
    24: (-58000.0, -52000.0, 1200.0, DIST_VIYADUK, False),  # NW Viaduct Interchange (+12 m)
    25: (-12000.0, -54000.0, 1200.0, DIST_VIYADUK, False),  # North Viaduct West (+12 m)
    26: ( 28000.0, -54000.0, 1200.0, DIST_VIYADUK, False),  # North Viaduct East (+12 m)
    27: ( 62000.0, -48000.0,  600.0, DIST_VIYADUK, False),  # NE Coastal Highway Sweep
    28: ( 68000.0,  66000.0,    0.0, DIST_VIYADUK, False),  # SE Beltway Corner
    29: ( -6000.0,  76000.0,    0.0, DIST_VIYADUK, False),  # South Beltway Straight
    30: (-66000.0,  72000.0,  800.0, DIST_VIYADUK, False),  # SW Beltway Climb
    31: (-76000.0,  10000.0, 1000.0, DIST_VIYADUK, False),  # West Beltway Bypass

    # --- Yamac Mahallesi (Hillside climbing Z = 0 -> +28 m) ---
    32: (-50000.0,  16000.0, 1100.0, DIST_YAMAC,   True),   # Lower Yamac Terrace (+11 m)
    33: (-54000.0,  36000.0, 1600.0, DIST_YAMAC,   False),  # Selin's Garage Terrace (+16 m)
    34: (-72000.0,  42000.0, 2600.0, DIST_YAMAC,   False),  # Upper Yamac Ridge (+26 m)
    35: (-42000.0,  56000.0, 1400.0, DIST_YAMAC,   False),  # South Yamac Descent (+14 m)
    36: (-20000.0,  46000.0,  400.0, DIST_YAMAC,   False),  # Yamac -> Merkez Connector (+4 m)

    # --- Kuzey Gecidi & Seyir Tepesi (Mountain Switchbacks Z = +12 m -> +62 m) ---
    37: (-26000.0, -68000.0, 2400.0, DIST_GECIT,   False),  # Hairpin 1 (+24 m)
    38: ( -8000.0, -78000.0, 3600.0, DIST_GECIT,   False),  # Hairpin 2 (+36 m)
    39: (-34000.0, -86000.0, 5000.0, DIST_GECIT,   False),  # Hairpin 3 (+50 m)
    40: (-60000.0, -86000.0, 6200.0, DIST_GECIT,   False),  # Seyir Tepesi Summit & Emre Memorial (+62 m)
    41: (-76000.0, -82000.0, 4600.0, DIST_GECIT,   False),  # Route B Dirt Service Portal (+46 m)
    42: (-84000.0, -54000.0, 2400.0, DIST_GECIT,   False),  # West Mountain Dirt Descent (+24 m)
}

# ---------------------------------------------------------------------------
# Road Graph Edges:
# (node_a, node_b, mid_controls, width_cm, speed_kmh, district, highway, dirt)
# ---------------------------------------------------------------------------
RAW_EDGES = [
    # --- Merkez Bulvar & Carsi Grid ---
    (0, 1, [], 1600.0, 75.0, DIST_MERKEZ, False, False),
    (1, 2, [], 1600.0, 75.0, DIST_MERKEZ, False, False),
    (2, 3, [], 1600.0, 75.0, DIST_MERKEZ, False, False),
    # Merkez Bulvar (wide grand boulevard, 20 m!)
    (4, 5, [], 2000.0, 85.0, DIST_MERKEZ, False, False),
    (5, 6, [], 2000.0, 85.0, DIST_MERKEZ, False, False),
    (6, 7, [], 2000.0, 85.0, DIST_MERKEZ, False, False),
    # Carsi Caddesi (south downtown commercial street)
    (8, 9, [], 1500.0, 65.0, DIST_MERKEZ, False, False),
    (9, 10, [], 1500.0, 65.0, DIST_MERKEZ, False, False),
    (10, 11, [], 1500.0, 65.0, DIST_MERKEZ, False, False),
    # North-South Avenues in Merkez
    (0, 4, [], 1500.0, 70.0, DIST_MERKEZ, False, False),
    (4, 8, [], 1500.0, 70.0, DIST_MERKEZ, False, False),
    (1, 5, [], 1500.0, 70.0, DIST_MERKEZ, False, False),
    (5, 9, [], 1500.0, 70.0, DIST_MERKEZ, False, False),
    (2, 6, [], 1500.0, 70.0, DIST_MERKEZ, False, False),
    (6, 10, [], 1500.0, 70.0, DIST_MERKEZ, False, False),
    (3, 7, [], 1600.0, 75.0, DIST_MERKEZ, False, False),
    (7, 11, [], 1600.0, 75.0, DIST_MERKEZ, False, False),

    # --- Dogu Sanayi ---
    (10, 12, [(12000.0, 29000.0, 0.0)], 1600.0, 80.0, DIST_SANAYI, False, False),
    (11, 13, [(34000.0, 29000.0, 0.0)], 1600.0, 80.0, DIST_SANAYI, False, False),
    (12, 13, [], 1800.0, 80.0, DIST_SANAYI, False, False),
    (13, 14, [], 1800.0, 80.0, DIST_SANAYI, False, False),
    (12, 15, [], 1600.0, 75.0, DIST_SANAYI, False, False),
    (15, 16, [], 1600.0, 80.0, DIST_SANAYI, False, False),
    (14, 16, [], 1600.0, 80.0, DIST_SANAYI, False, False),
    (13, 16, [(47000.0, 54000.0, 0.0)], 1500.0, 75.0, DIST_SANAYI, False, False),

    # --- Eski Liman & Eski Liman Koprusu ---
    (3, 17, [], 1700.0, 85.0, DIST_LIMAN, False, False),
    (7, 18, [], 1800.0, 85.0, DIST_LIMAN, False, False),
    (11, 19, [], 1600.0, 80.0, DIST_LIMAN, False, False),
    (17, 18, [], 1800.0, 85.0, DIST_LIMAN, False, False),
    (18, 19, [], 1800.0, 85.0, DIST_LIMAN, False, False),
    (19, 14, [(56000.0, 31000.0, 0.0)], 1600.0, 80.0, DIST_LIMAN, False, False),
    # West Bridge Approach Ramp (17 -> 20: 0 m -> +16 m over ~220 m curved climb)
    (17, 20, [(55000.0, -22500.0, 520.0), (59000.0, -18000.0, 1120.0)], 1800.0, 100.0, DIST_LIMAN, True, False),
    # Main Eski Liman Koprusu Truss Span (20 -> 21: straight elevated Z = +16 m over Mara Harbor!)
    (20, 21, [(73000.0, -14000.0, 1600.0)], 1860.0, 120.0, DIST_LIMAN, True, False),
    # East Bridge North Spur to Eski Liman Tunnel Portal (Route A)
    (21, 23, [(87500.0, -17500.0, 1150.0)], 1700.0, 100.0, DIST_LIMAN, True, False),
    # East Bridge South Sweep returning to East Harbor & Dogu Sanayi
    (21, 22, [(89500.0, -4000.0, 1150.0)], 1800.0, 105.0, DIST_LIMAN, True, False),
    (22, 14, [(78000.0, 26000.0, 300.0), (69000.0, 38000.0, 80.0)], 1700.0, 95.0, DIST_LIMAN, True, False),

    # --- Kuzey Viyadugu & O-5 Cevre Yolu Perimeter Highway ---
    (24, 25, [(-35000.0, -54500.0, 1200.0)], 2200.0, 140.0, DIST_VIYADUK, True, False),
    (25, 26, [(  8000.0, -54500.0, 1200.0)], 2200.0, 140.0, DIST_VIYADUK, True, False),
    (26, 27, [( 48000.0, -52500.0,  950.0)], 2200.0, 135.0, DIST_VIYADUK, True, False),
    (27, 17, [( 58000.0, -35000.0,  280.0)], 1800.0, 105.0, DIST_VIYADUK, True, False),
    # East -> South -> West Perimeter Beltway
    (16, 28, [], 2000.0, 120.0, DIST_VIYADUK, True, False),
    (14, 28, [(66000.0, 54000.0, 0.0)], 1800.0, 110.0, DIST_VIYADUK, True, False),
    (28, 29, [(34000.0, 77000.0, 0.0)], 2200.0, 140.0, DIST_VIYADUK, True, False),
    (29, 30, [(-38000.0, 77000.0, 350.0)], 2200.0, 140.0, DIST_VIYADUK, True, False),
    (30, 31, [(-76000.0, 44000.0, 950.0)], 2200.0, 135.0, DIST_VIYADUK, True, False),
    (31, 24, [(-74000.0, -24000.0, 1100.0)], 2200.0, 135.0, DIST_VIYADUK, True, False),
    # Smooth On/Off Ramps connecting North Viaduct & South Beltway to Merkez
    (25, 1, [(-12000.0, -38000.0, 600.0)], 1500.0, 90.0, DIST_VIYADUK, True, False),
    (26, 3, [( 29000.0, -38000.0, 600.0)], 1500.0, 90.0, DIST_VIYADUK, True, False),
    (24, 0, [(-46000.0, -37000.0, 600.0)], 1500.0, 90.0, DIST_VIYADUK, True, False),
    (15, 29, [(  8000.0, 73000.0,   0.0)], 1600.0, 95.0, DIST_VIYADUK, True, False),
    (31, 4, [(-54000.0,  2000.0,  500.0)], 1600.0, 90.0, DIST_VIYADUK, True, False),

    # --- Yamac Mahallesi (Hillside Residential + Selin'in Garaji) ---
    (8, 32, [(-41000.0, 16000.0, 550.0)], 1400.0, 65.0, DIST_YAMAC, False, False),
    (32, 33, [(-53000.0, 26000.0, 1350.0)], 1400.0, 65.0, DIST_YAMAC, False, False),
    (33, 34, [(-64000.0, 39000.0, 2150.0)], 1350.0, 60.0, DIST_YAMAC, False, False),
    (34, 31, [(-75500.0, 26000.0, 1800.0)], 1400.0, 65.0, DIST_YAMAC, False, False),
    (34, 30, [(-71000.0, 58000.0, 1700.0)], 1400.0, 65.0, DIST_YAMAC, False, False),
    (33, 35, [(-49000.0, 47000.0, 1500.0)], 1350.0, 65.0, DIST_YAMAC, False, False),
    (35, 36, [(-31000.0, 51500.0,  900.0)], 1400.0, 70.0, DIST_YAMAC, False, False),
    (36, 9,  [(-16000.0, 31000.0,  180.0)], 1450.0, 70.0, DIST_YAMAC, False, False),
    (36, 12, [( -2000.0, 44000.0,  180.0)], 1450.0, 75.0, DIST_YAMAC, False, False),
    (35, 29, [(-24000.0, 67000.0,  650.0)], 1450.0, 75.0, DIST_YAMAC, False, False),

    # --- Kuzey Gecidi & Seyir Tepesi (Mountain Drift Switchbacks Z = +12 m -> +62 m) ---
    (25, 37, [(-18000.0, -61000.0, 1800.0)], 1350.0, 70.0, DIST_GECIT, False, False),
    (37, 38, [(-15000.0, -72000.0, 3000.0)], 1350.0, 65.0, DIST_GECIT, False, False),
    (38, 39, [(-18000.0, -84500.0, 4300.0)], 1350.0, 65.0, DIST_GECIT, False, False),
    (39, 40, [(-47000.0, -89500.0, 5650.0)], 1400.0, 65.0, DIST_GECIT, False, False),
    # Route B Dirt Mountain Service Shortcut descending West Ridge (40 -> 41 -> 42 -> 24)
    (40, 41, [(-68500.0, -85500.0, 5400.0)], 1250.0, 65.0, DIST_GECIT, False, True),
    (41, 42, [(-82500.0, -69000.0, 3500.0)], 1250.0, 65.0, DIST_GECIT, False, True),
    (42, 24, [(-72000.0, -52000.0, 1750.0)], 1300.0, 70.0, DIST_GECIT, False, True),
    # Eastern mountain access ramp down from Hairpin 2 back toward North Viaduct East
    (38, 26, [( 12000.0, -72000.0, 2400.0), ( 22000.0, -63000.0, 1750.0)], 1350.0, 70.0, DIST_GECIT, False, False),
]


def _catmull_rom(p0, p1, p2, p3, t):
    t2 = t * t
    t3 = t2 * t
    out = []
    for c in range(3):
        v = 0.5 * (
            (2.0 * p1[c])
            + (-p0[c] + p2[c]) * t
            + (2.0 * p0[c] - 5.0 * p1[c] + 4.0 * p2[c] - p3[c]) * t2
            + (-p0[c] + 3.0 * p1[c] - 3.0 * p2[c] + p3[c]) * t3
        )
        out.append(v)
    return tuple(out)


def sample_polyline(ctrl_pts, step_cm=1600.0):
    """Smoothly samples a Catmull-Rom curve passing through every control point."""
    if len(ctrl_pts) < 2:
        return list(ctrl_pts)
    pts = [ctrl_pts[0]]
    n = len(ctrl_pts)
    for i in range(n - 1):
        p0 = ctrl_pts[max(0, i - 1)]
        p1 = ctrl_pts[i]
        p2 = ctrl_pts[i + 1]
        p3 = ctrl_pts[min(n - 1, i + 2)]
        dist = math.sqrt(sum((p2[c] - p1[c]) ** 2 for c in range(3)))
        steps = max(2, int(round(dist / step_cm)))
        for s in range(1, steps + 1):
            pts.append(_catmull_rom(p0, p1, p2, p3, s / float(steps)))
    return pts


def build_network():
    """Returns (nodes_dict, edges_list) with sampled 3D points on every edge."""
    nodes = {}
    for nid, (x, y, z, dist, sig) in RAW_NODES.items():
        nodes[nid] = dict(id=nid, pos=(x, y, z), district=dist, signalized=sig)

    edges = []
    edge_lookup = {}
    for idx, (a, b, mids, width, spd, dist, hwy, dirt) in enumerate(RAW_EDGES):
        ctrl = [nodes[a]["pos"]] + list(mids) + [nodes[b]["pos"]]
        step = 1500.0 if (mids or abs(nodes[a]["pos"][2] - nodes[b]["pos"][2]) > 50.0) else 2000.0
        sampled = sample_polyline(ctrl, step_cm=step)
        edge = dict(
            index=idx, node_a=a, node_b=b, points=sampled,
            width=width, speed=spd, district=dist, highway=hwy, one_way=False, dirt=dirt,
        )
        edges.append(edge)
        edge_lookup[(a, b)] = (idx, False)
        edge_lookup[(b, a)] = (idx, True)
    return nodes, edges, edge_lookup


def trace_node_chain(nodes, edges, edge_lookup, node_chain):
    """Converts a sequence of connected node IDs into a continuous sampled 3D path."""
    pts = []
    for i in range(len(node_chain) - 1):
        a, b = node_chain[i], node_chain[i + 1]
        if (a, b) not in edge_lookup:
            raise ValueError("Missing road edge between node %d and %d" % (a, b))
        eidx, rev = edge_lookup[(a, b)]
        ep = list(edges[eidx]["points"])
        if rev:
            ep.reverse()
        if pts:
            ep = ep[1:]
        pts.extend(ep)
    return pts


def _heading_deg(p0, p1):
    return math.degrees(math.atan2(p1[1] - p0[1], p1[0] - p0[0]))


def _smooth_race_path(pts, closed=False):
    """Rounds sharp >40-deg intersection vertices into gentle curved corner arcs so UE's USplineComponent
    never overshoots into corner buildings and AI rivals can track every city turn cleanly."""
    n = len(pts)
    if n < 3:
        return list(pts)
    out = []
    # For closed circuits, strip duplicate last point while rounding, then re-close at the end
    work = pts[:-1] if (closed and math.hypot(pts[0][0] - pts[-1][0], pts[0][1] - pts[-1][1]) < 50.0) else list(pts)
    m = len(work)
    for i in range(m):
        if not closed and (i == 0 or i == m - 1):
            out.append(work[i])
            continue
        p_prev = work[(i - 1) % m]
        p_cur = work[i]
        p_next = work[(i + 1) % m]
        v1 = (p_cur[0] - p_prev[0], p_cur[1] - p_prev[1], p_cur[2] - p_prev[2])
        v2 = (p_next[0] - p_cur[0], p_next[1] - p_cur[1], p_next[2] - p_cur[2])
        l1 = math.hypot(v1[0], v1[1])
        l2 = math.hypot(v2[0], v2[1])
        if l1 < 10.0 or l2 < 10.0:
            out.append(p_cur)
            continue
        dot = max(-1.0, min(1.0, (v1[0] * v2[0] + v1[1] * v2[1]) / (l1 * l2)))
        ang = math.degrees(math.acos(dot))
        if ang > 40.0:
            # Quadratic Bezier chamfer pulled 950 cm back along each leg inside the wide junction pad
            cut1 = min(950.0, l1 * 0.42)
            cut2 = min(950.0, l2 * 0.42)
            a = tuple(p_cur[c] - v1[c] * (cut1 / l1) for c in range(3))
            b = tuple(p_cur[c] + v2[c] * (cut2 / l2) for c in range(3))
            for t in (0.0, 0.35, 0.65, 1.0):
                u = 1.0 - t
                q = tuple(u * u * a[c] + 2.0 * u * t * p_cur[c] + t * t * b[c] for c in range(3))
                out.append(q)
        else:
            out.append(p_cur)
    if closed and out:
        out.append(out[0])
    return out


def _make_checkpoints_from_path(pts, num_gates, closed=False):
    """Picks evenly spaced (x, y, z, yaw_deg) checkpoint gates directly on straight/gentle portions of the
    road polyline, nudging away from sharp corner apexes so gates never sit twisted across a junction corner."""
    cum = [0.0]
    for i in range(1, len(pts)):
        cum.append(cum[-1] + math.sqrt(sum((pts[i][c] - pts[i - 1][c]) ** 2 for c in range(3))))
    total = cum[-1]

    def at_dist(d):
        d = max(0.0, min(total, d))
        for k in range(len(cum) - 1):
            if d <= cum[k + 1] or k == len(cum) - 2:
                seg = max(cum[k + 1] - cum[k], 1e-4)
                t = (d - cum[k]) / seg
                p = tuple(pts[k][c] + (pts[k + 1][c] - pts[k][c]) * t for c in range(3))
                yaw = _heading_deg(pts[k], pts[k + 1])
                return p[0], p[1], p[2], yaw
        return pts[-1][0], pts[-1][1], pts[-1][2], 0.0

    def curvature_at(d):
        _, _, _, y0 = at_dist(max(0.0, d - 900.0))
        _, _, _, y1 = at_dist(min(total, d + 900.0))
        diff = abs(((y1 - y0 + 180.0) % 360.0) - 180.0)
        return diff

    gates = []
    for i in range(num_gates):
        if closed:
            d = total * (i + 1) / float(num_gates) - (1500.0 if i == num_gates - 1 else 0.0)
        else:
            d = total * (i + 1) / float(num_gates) - (800.0 if i == num_gates - 1 else 0.0)
        # If the nominal distance lands right on a sharp corner (>32 deg turn), pick the cleanest nearby straight!
        if curvature_at(d) > 32.0:
            best_d = d
            best_c = curvature_at(d)
            for shift in (-2400.0, 2400.0, -3800.0, 3800.0):
                cand = max(1400.0, min(total - 1000.0, d + shift))
                c = curvature_at(cand)
                if c < best_c:
                    best_c = c
                    best_d = cand
            d = best_d
        gates.append(at_dist(d))
    return gates, at_dist(1400.0), total


def build_race_specs(nodes, edges, edge_lookup):
    """Returns all 9 unique Karamara race definitions derived directly from the road graph."""
    specs = []

    def add_route(asset_name, title, discipline, node_chain, num_gates, laps=1, time_limit=75.0,
                  gate_width=1750.0, rivals=(0.76, 0.86, 0.94), beacon_offset=(0.0, -1400.0)):
        closed = (discipline == "CIRCUIT")
        raw_pts = trace_node_chain(nodes, edges, edge_lookup, node_chain)
        pts = _smooth_race_path(raw_pts, closed=closed)
        gates, grid, total_cm = _make_checkpoints_from_path(pts, num_gates, closed=closed)
        # Fit gate width cleanly to the narrowest road on this route so posts stay on shoulders, never inside walls!
        min_road_w = min(edges[edge_lookup[(node_chain[k], node_chain[k + 1])][0]]["width"] for k in range(len(node_chain) - 1))
        clamped_gw = min(gate_width, min_road_w + 150.0)
        # Beacon sits right off the start shoulder so player spots it cleanly without accidental instant re-trigger
        gyaw_rad = math.radians(grid[3])
        rx, ry = -math.sin(gyaw_rad), math.cos(gyaw_rad)
        bx = grid[0] + rx * beacon_offset[1] + math.cos(gyaw_rad) * beacon_offset[0]
        by = grid[1] + ry * beacon_offset[1] + math.sin(gyaw_rad) * beacon_offset[0]
        specs.append(dict(
            name=asset_name, title=title, discipline=discipline,
            grid=(grid[0], grid[1], grid[2] + 65.0, grid[3]),
            checkpoints=gates, laps=laps, time_limit=time_limit, gate_width=clamped_gw,
            racing_line=pts, rivals=rivals, beacon=(bx, by, grid[2]), length_m=total_cm / 100.0,
        ))

    # 1. Merkez Bulvar & Carsi Circuit (2 laps, classic downtown loop)
    add_route("RACE_KaramaraCircuit", "Merkez Bulvar Pisti", "CIRCUIT",
              [4, 5, 6, 7, 11, 10, 9, 8, 4], num_gates=8, laps=2, rivals=(0.74, 0.84, 0.92),
              beacon_offset=(-2400.0, -1500.0))

    # 2. Kuzey Viyadugu High-Speed Sprint (West Viaduct -> North Viaduct -> Eski Liman)
    add_route("RACE_KuzeyViyadukSprint", "Kuzey Viyadüğü Sprint", "SPRINT",
              [4, 0, 24, 25, 26, 27, 17, 18], num_gates=9, laps=1, gate_width=2400.0,
              rivals=(0.76, 0.86, 0.95), beacon_offset=(0.0, -1500.0))

    # 3. Dogu Sanayi Container Courtyard Drift (75 s)
    specs.append(dict(
        name="RACE_SanayiDrift", title="Doğu Sanayi Drift Avlusu", discipline="DRIFT",
        grid=(44000.0, 52000.0, 65.0, 0.0), checkpoints=[], laps=1, time_limit=75.0,
        gate_width=2200.0, racing_line=[], rivals=(), beacon=(39000.0, 56000.0, 0.0), length_m=0.0,
    ))

    # 4. Eski Liman & Eski Liman Koprusu Sprint (Merkez -> Harbor -> Bridge -> Sanayi)
    add_route("RACE_LimanSprint", "Eski Liman Köprüsü Sprint", "SPRINT",
              [6, 7, 3, 17, 20, 21, 22, 14, 13], num_gates=9, laps=1, gate_width=2200.0,
              rivals=(0.80, 0.89, 0.96), beacon_offset=(0.0, 1500.0))

    # 5. Yamac Mahallesi Technical Hillside Circuit (2 laps, Z = 0..+26 m)
    add_route("RACE_YamacCircuit", "Yamaç Mahallesi Yokuşu", "CIRCUIT",
              [8, 32, 33, 34, 31, 4, 8], num_gates=8, laps=2, gate_width=1800.0,
              rivals=(0.78, 0.88, 0.95), beacon_offset=(0.0, 1300.0))

    # 6. Kuzey Gecidi Mountain Hairpins Touge Drift (90 s, Z = +24..+62 m)
    specs.append(dict(
        name="RACE_KuzeyGecidiDrift", title="Kuzey Geçidi Dağ Drifti", discipline="DRIFT",
        grid=(-24000.0, -69000.0, 2520.0, -35.0), checkpoints=[], laps=1, time_limit=90.0,
        gate_width=1900.0, racing_line=[], rivals=(), beacon=(-29000.0, -65000.0, 2320.0), length_m=0.0,
    ))

    # 7. O-5 Cevre Yolu Full Beltway High-Speed Sprint (~4.2 km)
    add_route("RACE_CevreYoluSprint", "O-5 Çevre Yolu Sprint", "SPRINT",
              [29, 30, 31, 24, 25, 26, 27, 17, 18, 19, 14, 28], num_gates=12, laps=1, gate_width=2400.0,
              rivals=(0.82, 0.91, 0.97), beacon_offset=(0.0, -1600.0))

    # 8. Eski Liman + Kopru + Sanayi Fast Circuit (2 laps)
    add_route("RACE_LimanCircuit", "Liman & Sanayi Pisti", "CIRCUIT",
              [18, 17, 20, 21, 22, 14, 19, 18], num_gates=8, laps=2, gate_width=2200.0,
              rivals=(0.84, 0.92, 0.98), beacon_offset=(0.0, -1500.0))

    # 9. #01 Viktor Serez Grand Finale: Karamara Buyuk Turu (all 6 districts -> Eski Liman Koprusu!)
    add_route("RACE_KaramaraBuyukTuru", "Karamara Büyük Turu (#01)", "CIRCUIT",
              [5, 4, 8, 32, 33, 34, 31, 24, 25, 37, 38, 26, 27, 17, 20, 21, 22, 14, 13, 11, 7, 6, 5],
              num_gates=16, laps=1, gate_width=2400.0, rivals=(0.94, 0.98, 1.00),
              beacon_offset=(2200.0, 1500.0))

    return specs


# 10 Multi-Style Pursuit Breakers across all 6 districts:
# (label, x, y, z, yaw_deg, title, cost_dollars, style)  style: 0=GantrySign, 1=CraneLoad, 2=WaterTower
PURSUIT_BREAKERS = [
    ("Breaker_MerkezTak",      -1000.0,  -4000.0,    0.0,   0.0, "Merkez Bulvar Tabelası",    30000, 0),
    ("Breaker_CarsiKemer",      1000.0,  16000.0,    0.0,   0.0, "Çarşı İskele Kemeri",       28000, 2),
    ("Breaker_SanayiVinc",     30000.0,  42000.0,    0.0,   0.0, "Doğu Sanayi Depo Vinci",    38000, 1),
    ("Breaker_SanayiKule",     46000.0,  68000.0,    0.0,   0.0, "Sanayi Su Kulesi",          35000, 2),
    ("Breaker_LimanVinc",      50000.0,   8000.0,    0.0,  90.0, "Eski Liman Konteyner Vinci",45000, 1),
    ("Breaker_LimanTabela",    50000.0, -14000.0,    0.0,  90.0, "Liman Giriş Gantry'si",     40000, 0),
    ("Breaker_ViyadukBati",   -35000.0, -54500.0, 1200.0,   0.0, "Kuzey Viyadüğü O-5 Takı",   50000, 0),
    ("Breaker_CevreGuney",     20000.0,  76500.0,    0.0,   0.0, "Güney Çevre Yolu Tabelası", 42000, 0),
    ("Breaker_YamacKule",     -51000.0,  22000.0, 1220.0,  80.0, "Yamaç Su Deposu İskelesi",  36000, 2),
    ("Breaker_GecitKayalik",  -20000.0, -78000.0, 3950.0, 175.0, "Kuzey Geçidi Kaya Bariyeri",48000, 1),
]


def validate_network():
    nodes, edges, lookup = build_network()
    # 1. Check graph connectivity from node 0 via BFS
    adj = {nid: [] for nid in nodes}
    max_grade = 0.0
    worst_edge = None
    for e in edges:
        adj[e["node_a"]].append(e["node_b"])
        adj[e["node_b"]].append(e["node_a"])
        pts = e["points"]
        for i in range(len(pts) - 1):
            dxy = math.hypot(pts[i + 1][0] - pts[i][0], pts[i + 1][1] - pts[i][1])
            dz = abs(pts[i + 1][2] - pts[i][2])
            g = dz / max(dxy, 1.0)
            if g > max_grade:
                max_grade = g
                worst_edge = (e["node_a"], e["node_b"], g)

    seen = {0}
    stack = [0]
    while stack:
        cur = stack.pop()
        for nb in adj[cur]:
            if nb not in seen:
                seen.add(nb)
                stack.append(nb)
    if len(seen) != len(nodes):
        missing = sorted(set(nodes.keys()) - seen)
        raise RuntimeError("Disconnected road graph nodes: %s" % missing)

    races = build_race_specs(nodes, edges, lookup)
    districts_used = {n["district"] for n in nodes.values()}
    total_road_km = sum(
        sum(math.sqrt(sum((e["points"][i + 1][c] - e["points"][i][c]) ** 2 for c in range(3))) for i in range(len(e["points"]) - 1))
        for e in edges
    ) / 100000.0
    print("BN network OK: %d nodes, %d edges (%.2f km total road), %d districts, %d races, max grade=%.1f%% (edge %s)" % (
        len(nodes), len(edges), total_road_km, len(districts_used), len(races), max_grade * 100.0, worst_edge
    ))
    for r in races:
        print("  - %-24s | %-7s | %2d gates | %4.0f m" % (r["name"], r["discipline"], len(r["checkpoints"]), r["length_m"]))


if __name__ == "__main__":
    if "--check" in sys.argv:
        validate_network()
