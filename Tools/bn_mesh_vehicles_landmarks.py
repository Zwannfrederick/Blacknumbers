"""Procedural glTF 2.0 geometry builders for BLACK NUMBERS:
- Monumental landmarks (Eski Liman Koprusu pylons & truss, port cranes, cargo ship, tunnel arch, 360-deg hills)
- 4 distinct civilian traffic vehicles (Sedan, Hatchback, Van, BoxTruck)
- Police equipment (aerodynamic LED lightbar, interceptor push-bar)
- Holographic race start ring & checkpoint chevron markers

All coordinates are specified in UE meters (+X forward, +Y right, +Z up) and
passed through GltfBuilder (defined in bn_meshes.py).
"""
import math


def _add_car_wheels(b, mat_trim, x_front, x_rear, half_y, radius=0.34, width=0.22):
    for wx in (x_front, x_rear):
        for side in (-1.0, 1.0):
            # Cylinder axis along Y (axle)
            b.add_cylinder_y(
                mat_trim,
                (wx, side * (half_y - width * 0.42), radius),
                radius,
                width,
                segments=14,
            )


def _add_extruded_profile(b, mat, profile_xz, half_y_profile, cap_sides=True):
    """Builds a symmetric vehicle hull from a closed 2D side silhouette [(x, z), ...]
    with optional tapered width half_y_profile[i] at each profile vertex."""
    n = len(profile_xz)
    # Build left (-Y) and right (+Y) loops; top/front/back strip connects them
    for i in range(n):
        j = (i + 1) % n
        x0, z0 = profile_xz[i]
        x1, z1 = profile_xz[j]
        w0 = half_y_profile[i]
        w1 = half_y_profile[j]
        b.add_quad(
            mat,
            (x0, -w0, z0),
            (x0, +w0, z0),
            (x1, +w1, z1),
            (x1, -w1, z1),
        )
    if cap_sides:
        cx = sum(p[0] for p in profile_xz) / n
        cz = sum(p[1] for p in profile_xz) / n
        cw = sum(half_y_profile) / n
        for side in (-1.0, 1.0):
            center = (cx, side * cw, cz)
            for i in range(n):
                j = (i + 1) % n
                p0 = (profile_xz[i][0], side * half_y_profile[i], profile_xz[i][1])
                p1 = (profile_xz[j][0], side * half_y_profile[j], profile_xz[j][1])
                if side > 0:
                    b.add_tri(mat, center, p0, p1)
                else:
                    b.add_tri(mat, center, p1, p0)


def build_traffic_sedan(b):
    """4.56 m sleek European/Turkish street sedan (+X forward, Z=0 ground)."""
    # Slots: 0=Paint, 1=Glass, 2=Trim, 3=Headlight, 4=Taillight
    lower = [
        (2.26, 0.30), (2.28, 0.64), (2.10, 0.80), (1.15, 0.88),
        (-1.45, 0.90), (-2.18, 0.86), (-2.28, 0.62), (-2.22, 0.30),
    ]
    lower_w = [0.84, 0.89, 0.90, 0.92, 0.92, 0.89, 0.88, 0.84]
    _add_extruded_profile(b, 0, lower, lower_w)
    # Cabin greenhouse (slightly narrower at roof for sleek tumblehome)
    cabin = [
        (1.06, 0.88), (0.48, 1.41), (-0.72, 1.42), (-1.42, 0.90),
    ]
    cabin_w = [0.84, 0.72, 0.72, 0.84]
    _add_extruded_profile(b, 1, cabin, cabin_w)
    # Roof paint cap
    b.add_box(0, (-0.12, 0.0, 1.425), (1.18, 1.42, 0.04))
    # Lower bumpers, grille & rockers
    b.add_box(2, (2.27, 0.0, 0.44), (0.12, 1.66, 0.24))
    b.add_box(2, (-2.26, 0.0, 0.42), (0.12, 1.64, 0.22))
    b.add_box(2, (0.0, 0.0, 0.27), (3.80, 1.72, 0.08))
    # Side mirrors
    for s in (-1.0, 1.0):
        b.add_box(0, (0.76, s * 0.98, 0.96), (0.16, 0.16, 0.10))
        # Headlights & taillights
        b.add_box(3, (2.25, s * 0.64, 0.67), (0.10, 0.34, 0.13))
        b.add_box(4, (-2.26, s * 0.64, 0.72), (0.08, 0.40, 0.14))
    _add_car_wheels(b, 2, 1.40, -1.36, 0.92, radius=0.33, width=0.22)


def build_traffic_hatchback(b):
    """4.04 m compact urban hatchback (+X forward, Z=0 ground)."""
    lower = [
        (2.00, 0.30), (2.02, 0.64), (1.84, 0.82), (1.02, 0.90),
        (-1.60, 0.92), (-1.98, 0.84), (-2.02, 0.56), (-1.94, 0.30),
    ]
    lower_w = [0.82, 0.87, 0.88, 0.90, 0.90, 0.86, 0.85, 0.82]
    _add_extruded_profile(b, 0, lower, lower_w)
    cabin = [
        (0.95, 0.90), (0.36, 1.47), (-1.38, 1.46), (-1.92, 0.88),
    ]
    cabin_w = [0.84, 0.74, 0.74, 0.84]
    _add_extruded_profile(b, 1, cabin, cabin_w)
    b.add_box(0, (-0.52, 0.0, 1.475), (1.72, 1.46, 0.05))
    b.add_box(2, (2.01, 0.0, 0.44), (0.10, 1.58, 0.22))
    b.add_box(2, (-2.00, 0.0, 0.42), (0.10, 1.56, 0.22))
    for s in (-1.0, 1.0):
        b.add_box(0, (0.64, s * 0.95, 0.98), (0.15, 0.15, 0.10))
        b.add_box(3, (1.99, s * 0.60, 0.68), (0.10, 0.32, 0.14))
        b.add_box(4, (-1.99, s * 0.64, 0.78), (0.08, 0.30, 0.22))
    _add_car_wheels(b, 2, 1.24, -1.24, 0.90, radius=0.32, width=0.21)


def build_traffic_van(b):
    """5.12 m commercial delivery van (+X forward, Z=0 ground)."""
    body = [
        (2.54, 0.36), (2.56, 0.88), (2.28, 1.18), (1.62, 1.28),
        (1.02, 2.26), (-2.48, 2.26), (-2.54, 1.98), (-2.54, 0.38),
    ]
    body_w = [0.94, 0.98, 0.99, 1.00, 0.92, 0.92, 0.98, 0.96]
    _add_extruded_profile(b, 0, body, body_w)
    # Windshield & front door windows
    b.add_quad(1, (1.64, -0.88, 1.29), (1.64, 0.88, 1.29), (1.06, 0.84, 2.18), (1.06, -0.84, 2.18))
    for s in (-1.0, 1.0):
        b.add_box(1, (1.16, s * 0.985, 1.66), (0.95, 0.04, 0.66))
        b.add_box(2, (1.42, s * 1.08, 1.38), (0.16, 0.16, 0.26))
        b.add_box(3, (2.54, s * 0.72, 0.84), (0.10, 0.34, 0.18))
        b.add_box(4, (-2.54, s * 0.82, 0.98), (0.08, 0.20, 0.44))
    b.add_box(2, (2.56, 0.0, 0.56), (0.14, 1.92, 0.28))
    b.add_box(2, (-2.54, 0.0, 0.46), (0.14, 1.94, 0.22))
    _add_car_wheels(b, 2, 1.66, -1.58, 1.00, radius=0.38, width=0.25)


def build_traffic_box_truck(b):
    """6.55 m urban freight box truck (+X forward, Z=0 ground; slot 5 = white cargo box)."""
    # Cab-over front
    cab = [
        (3.22, 0.44), (3.25, 1.44), (2.96, 2.38), (1.58, 2.38),
        (1.58, 0.44),
    ]
    cab_w = [1.04, 1.08, 1.04, 1.04, 1.06]
    _add_extruded_profile(b, 0, cab, cab_w)
    # Wide windshield & side glass
    b.add_quad(1, (3.24, -0.96, 1.46), (3.24, 0.96, 1.46), (3.00, 0.94, 2.28), (3.00, -0.94, 2.28))
    for s in (-1.0, 1.0):
        b.add_box(1, (2.38, s * 1.06, 1.84), (1.10, 0.05, 0.66))
        b.add_box(3, (3.23, s * 0.76, 0.82), (0.10, 0.36, 0.20))
        b.add_box(4, (-3.26, s * 0.86, 0.82), (0.08, 0.36, 0.16))
    # Rear cargo box (slot 5) + steel frame underbody
    b.add_box(5, (-0.88, 0.0, 1.96), (4.72, 2.34, 2.18))
    b.add_box(2, (-0.50, 0.0, 0.68), (5.50, 1.72, 0.32))
    b.add_box(2, (3.26, 0.0, 0.56), (0.16, 2.12, 0.28))
    b.add_box(2, (-3.24, 0.0, 0.56), (0.14, 2.18, 0.20))
    _add_car_wheels(b, 2, 2.18, -1.72, 1.08, radius=0.44, width=0.28)


def build_police_lightbar(b):
    """Aerodynamic low-profile LED police lightbar (1.28 m wide, centered on roof)."""
    # Slots: 0=Trim, 1=Red, 2=Blue
    b.add_box(0, (0.0, 0.0, 0.03), (0.28, 1.24, 0.06))
    b.add_box(0, (0.0, 0.0, 0.085), (0.22, 0.24, 0.07))
    # Angled V-wings red (left, -Y) & blue (right, +Y)
    b.add_box(1, (-0.01, -0.38, 0.085), (0.24, 0.48, 0.075))
    b.add_box(2, (-0.01, +0.38, 0.085), (0.24, 0.48, 0.075))
    # Roof mounting feet
    for s in (-1.0, 1.0):
        b.add_box(0, (0.0, s * 0.54, -0.02), (0.16, 0.08, 0.06))


def build_police_pushbar(b):
    """Heavy-duty interceptor ramming push-bar (front bumper mounted)."""
    # Slots: 0=Chrome/Steel, 1=Rubber Pad
    for s in (-1.0, 1.0):
        # Vertical ram uprights
        b.add_box(0, (0.0, s * 0.44, 0.0), (0.12, 0.07, 0.58))
        b.add_box(1, (0.07, s * 0.44, 0.02), (0.04, 0.08, 0.50))
        # Side wrap wings
        b.add_box(0, (-0.10, s * 0.62, 0.06), (0.22, 0.05, 0.05))
        b.add_box(0, (-0.10, s * 0.62, -0.14), (0.22, 0.05, 0.05))
    # Horizontal crossbars
    b.add_box(0, (0.02, 0.0, 0.14), (0.06, 0.96, 0.06))
    b.add_box(0, (0.02, 0.0, -0.10), (0.06, 0.96, 0.06))


def build_bridge_pylon(b):
    """44 m tall Eski Liman Koprusu H-frame twin steel-concrete suspension pylon."""
    # Slots: 0=Concrete piers, 1=BridgeSteel tower & cross-braces
    # Twin pier footings extending down to seabed (-8 m..+18 m)
    for s in (-1.0, 1.0):
        b.add_box(0, (0.0, s * 11.5, 4.0), (5.2, 4.4, 24.0))
        # Tapered upper steel tower legs (+16 m..+44 m)
        b.add_frustum_box(1, (0.0, s * 11.5, 16.0), (3.4, 2.8), (2.2, 1.9), 28.0)
    # Cross portals (under-deck + mid + top crown)
    b.add_box(0, (0.0, 0.0, 14.5), (3.8, 22.0, 2.4))
    b.add_box(1, (0.0, 0.0, 28.0), (2.2, 22.0, 1.8))
    b.add_box(1, (0.0, 0.0, 41.5), (2.4, 23.0, 2.2))
    # X-brace struts between 28 m and 41 m
    b.add_beam(1, (0.0, -10.5, 28.5), (0.0, 10.5, 41.0), 0.75)
    b.add_beam(1, (0.0, 10.5, 28.5), (0.0, -10.5, 41.0), 0.75)


def build_bridge_truss(b):
    """40 m span Eski Liman Koprusu arched Warren steel truss section (road along +X)."""
    # Slots: 0=BridgeSteel, 1=Deck(Asphalt), 2=Walkway(Concrete)
    span = 40.0
    half_w = 10.2
    # Solid roadway deck & elevated pedestrian curbs
    b.add_box(1, (0.0, 0.0, -0.45), (span, 18.6, 0.90))
    for s in (-1.0, 1.0):
        b.add_box(2, (0.0, s * 9.8, 0.05), (span, 1.4, 1.1))
        # Lower steel chord
        b.add_box(0, (0.0, s * half_w, 0.6), (span, 0.7, 0.7))
        # Arched upper chord & Warren web diagonals (6 bays)
        bays = 6
        dx = span / bays
        for k in range(bays):
            x0 = -span * 0.5 + k * dx
            x1 = x0 + dx
            t0 = (k / float(bays)) * math.pi
            t1 = ((k + 1) / float(bays)) * math.pi
            z0 = 5.2 + 5.0 * math.sin(t0)
            z1 = 5.2 + 5.0 * math.sin(t1)
            b.add_beam(0, (x0, s * half_w, z0), (x1, s * half_w, z1), 0.62)
            # Vertical post & diagonal strut
            b.add_beam(0, (x0, s * half_w, 0.6), (x0, s * half_w, z0), 0.52)
            if k < bays // 2:
                b.add_beam(0, (x0, s * half_w, 0.6), (x1, s * half_w, z1), 0.42)
            else:
                b.add_beam(0, (x0, s * half_w, z0), (x1, s * half_w, 0.6), 0.42)
        b.add_beam(0, (span * 0.5, s * half_w, 0.6), (span * 0.5, s * half_w, 5.2), 0.52)
    # Overhead lateral wind-bracing crossbeams
    for k in (1, 2, 3, 4, 5):
        xk = -span * 0.5 + k * (span / 6.0)
        zk = 5.2 + 5.0 * math.sin((k / 6.0) * math.pi)
        b.add_beam(0, (xk, -half_w, zk), (xk, half_w, zk), 0.48)


def build_port_crane(b):
    """36 m tall maritime container gantry crane with lattice legs & 48 m cantilever boom."""
    # Slots: 0=Crane Red Steel, 1=Hazard Yellow Boom/Trolley, 2=Glass Cabin
    for sx in (-1.0, 1.0):
        for sy in (-1.0, 1.0):
            # Slanted A-frame corner legs
            b.add_beam(0, (sx * 7.5, sy * 8.5, 0.0), (sx * 5.2, sy * 5.6, 26.0), 0.95)
            # Crane bogie rail trucks at base
            b.add_box(1, (sx * 7.5, sy * 8.5, 0.9), (3.2, 1.8, 1.8))
        # Transverse portal & X-bracing
        b.add_beam(0, (sx * 6.4, -7.2, 12.0), (sx * 6.4, 7.2, 12.0), 0.75)
        b.add_beam(0, (sx * 5.2, -5.6, 25.5), (sx * 5.2, 5.6, 25.5), 0.85)
        b.add_beam(0, (sx * 6.4, -7.0, 12.2), (sx * 5.3, 5.5, 25.2), 0.45)
        b.add_beam(0, (sx * 6.4, 7.0, 12.2), (sx * 5.3, -5.5, 25.2), 0.45)
    # Upper apex tower (26 m..37 m)
    for sy in (-1.0, 1.0):
        b.add_beam(0, (-5.2, sy * 5.6, 25.8), (0.0, sy * 3.8, 36.5), 0.72)
        b.add_beam(0, (5.2, sy * 5.6, 25.8), (0.0, sy * 3.8, 36.5), 0.72)
    b.add_beam(0, (0.0, -3.8, 36.5), (0.0, 3.8, 36.5), 0.72)
    # 48 m cantilever boom projecting along +X (out over harbor water)
    for sy in (-1.0, 1.0):
        b.add_beam(1, (-16.0, sy * 4.2, 25.2), (32.0, sy * 3.6, 25.2), 0.85)
        # Tension forestay & backstay cables
        b.add_beam(0, (0.0, sy * 3.8, 36.5), (28.0, sy * 3.6, 25.4), 0.28)
        b.add_beam(0, (0.0, sy * 3.8, 36.5), (-15.0, sy * 4.2, 25.4), 0.28)
    # Operator cab & hanging spreader frame
    b.add_box(2, (11.0, 0.0, 22.8), (4.2, 3.4, 2.8))
    b.add_box(1, (18.0, 0.0, 13.5), (3.4, 9.6, 1.0))
    for sy in (-1.0, 1.0):
        b.add_beam(0, (18.0, sy * 3.6, 25.0), (18.0, sy * 4.0, 14.0), 0.16)


def build_cargo_ship(b):
    """112 m container freighter berthed in Eski Liman (bow along +X, waterline Z=0)."""
    # Slots: 0=Hull Red, 1=Upper Hull Dark, 2=Superstructure White/Concrete, 3=Cont Blue, 4=Cont Amber
    # Flared ship hull profile along X
    b.add_box(0, (-2.0, 0.0, 1.2), (90.0, 22.0, 5.2))
    b.add_box(1, (-2.0, 0.0, 6.2), (92.0, 22.4, 5.0))
    # Real tapered bow wedge via quads
    bow_rear_x = 44.0
    bow_tip_x = 59.0
    for z0, z1, m in [(-1.4, 3.8, 0), (3.8, 8.8, 1)]:
        b.add_quad(m, (bow_rear_x, -11.2, z0), (bow_tip_x, 0.0, z0 + 0.6), (bow_tip_x, 0.0, z1 + 0.8), (bow_rear_x, -11.2, z1))
        b.add_quad(m, (bow_tip_x, 0.0, z0 + 0.6), (bow_rear_x, 11.2, z0), (bow_rear_x, 11.2, z1), (bow_tip_x, 0.0, z1 + 0.8))
    b.add_tri(1, (bow_rear_x, -11.2, 8.8), (bow_rear_x, 11.2, 8.8), (bow_tip_x, 0.0, 9.6))
    # Stern bridge tower & funnel
    b.add_box(2, (-34.0, 0.0, 17.5), (14.0, 19.6, 18.0))
    b.add_box(2, (-33.0, 0.0, 27.6), (7.0, 23.0, 3.2))  # Bridge wings
    b.add_box(0, (-40.0, 0.0, 29.0), (4.5, 4.0, 8.5))   # Exhaust funnel
    # Stacked container cargo bays on deck
    for ix, cx in enumerate((-16.0, -2.0, 12.0, 26.0)):
        for iy, cy in enumerate((-6.8, 0.0, 6.8)):
            slot = 3 if (ix + iy) % 2 == 0 else 4
            h = 7.8 if (ix % 2 == 0) else 10.4
            b.add_box(slot, (cx, cy, 8.8 + h * 0.5), (11.6, 5.8, h))


def build_tunnel_arch(b):
    """Arched concrete highway tunnel portal (18 m clear width, 8.5 m peak height, 22 m long along X)."""
    # Slots: 0=Concrete exterior, 1=Dark interior liner, 2=Hazard portal rim
    length = 22.0
    half_x = length * 0.5
    inner_rx, inner_rz = 9.2, 7.6
    outer_rx, outer_rz = 11.4, 9.8
    segs = 12
    for i in range(segs):
        a0 = (i / float(segs)) * math.pi
        a1 = ((i + 1) / float(segs)) * math.pi
        iy0, iz0 = -math.cos(a0) * inner_rx, 1.0 + math.sin(a0) * inner_rz
        iy1, iz1 = -math.cos(a1) * inner_rx, 1.0 + math.sin(a1) * inner_rz
        oy0, oz0 = -math.cos(a0) * outer_rx, 0.0 + math.sin(a0) * outer_rz
        oy1, oz1 = -math.cos(a1) * outer_rx, 0.0 + math.sin(a1) * outer_rz
        # Inner vault liner (normals facing inward)
        b.add_quad(1, (-half_x, iy0, iz0), (half_x, iy0, iz0), (half_x, iy1, iz1), (-half_x, iy1, iz1))
        # Outer concrete arch shell
        b.add_quad(0, (-half_x, oy1, oz1), (half_x, oy1, oz1), (half_x, oy0, oz0), (-half_x, oy0, oz0))
        # Front & back concrete ring faces
        b.add_quad(2 if i in (0, segs - 1) else 0, (half_x, oy0, oz0), (half_x, oy1, oz1), (half_x, iy1, iz1), (half_x, iy0, iz0))
        b.add_quad(2 if i in (0, segs - 1) else 0, (-half_x, iy0, iz0), (-half_x, iy1, iz1), (-half_x, oy1, oz1), (-half_x, oy0, oz0))
    # Side portal headwalls
    for s in (-1.0, 1.0):
        b.add_box(0, (0.0, s * 10.6, 5.2), (length, 2.4, 10.4))


def build_surrounding_hills(b):
    """360-deg distant Karamara coastal mountain skyline ring (radius 1180..2200 m).
    Stays low/submerged on the eastern Mara Sea opening (-35..+35 deg from +X) so
    the ocean horizon stays open while wrapping N/W/S in dramatic coastal mountains."""
    # Slots: 0=MountainRock (upper ridges), 1=Grass (lower foothills)
    rings = 72
    r_inner = 1120.0
    r_mid = 1520.0
    r_outer = 2180.0

    def ridge_height(theta):
        # East (+X, theta=0) faces the open Mara Sea -> drop to -18 m underwater
        east_gap = math.exp(-((math.atan2(math.sin(theta), math.cos(theta))) / 0.58) ** 2)
        base = (
            125.0
            + 55.0 * math.sin(theta * 3.0 + 0.7)
            + 38.0 * math.cos(theta * 5.0 - 1.1)
            + 26.0 * math.sin(theta * 9.0 + 0.4)
            + 18.0 * math.cos(theta * 14.0)
        )
        # Extra prominence over Kuzey Gecidi (NW, theta around -2.35 rad)
        nw_peak = 85.0 * math.exp(-((math.atan2(math.sin(theta + 2.35), math.cos(theta + 2.35))) / 0.75) ** 2)
        h = (base + nw_peak) * (1.0 - 0.98 * east_gap) - 18.0 * east_gap
        return h

    for i in range(rings):
        t0 = (i / float(rings)) * 2.0 * math.pi
        t1 = ((i + 1) / float(rings)) * 2.0 * math.pi
        h0 = ridge_height(t0)
        h1 = ridge_height(t1)
        c0, s0 = math.cos(t0), math.sin(t0)
        c1, s1 = math.cos(t1), math.sin(t1)

        p_in0 = (c0 * r_inner, s0 * r_inner, -14.0)
        p_in1 = (c1 * r_inner, s1 * r_inner, -14.0)
        p_mid0 = (c0 * r_mid, s0 * r_mid, h0)
        p_mid1 = (c1 * r_mid, s1 * r_mid, h1)
        p_out0 = (c0 * r_outer, s0 * r_outer, -20.0)
        p_out1 = (c1 * r_outer, s1 * r_outer, -20.0)

        # Inward-facing foothills (grass lower half -> rock upper peak)
        p_half0 = (c0 * (r_inner + (r_mid - r_inner) * 0.45), s0 * (r_inner + (r_mid - r_inner) * 0.45), max(-12.0, h0 * 0.42))
        p_half1 = (c1 * (r_inner + (r_mid - r_inner) * 0.45), s1 * (r_inner + (r_mid - r_inner) * 0.45), max(-12.0, h1 * 0.42))
        b.add_quad(1, p_in0, p_in1, p_half1, p_half0)
        b.add_quad(0, p_half0, p_half1, p_mid1, p_mid0)
        b.add_quad(0, p_mid0, p_mid1, p_out1, p_out0)


def build_karamara_hills(b):
    """Sculpted organic 3D coastal mountain & hillside terrain for Kuzey Gecidi (up to +88 m ridges)
    and Yamac Mahallesi (+26 m slopes), smoothly carved below every road corridor so zero terrain
    triangles ever poke above the asphalt."""
    import karamara_network as knet

    # Gather road segments in metres: (ax, ay, az, bx, by, bz, hw_m)
    _nodes, edges, _lookup = knet.build_network()
    segs = []
    for e in edges:
        pts = e["points"]
        hw_m = (e["width"] * 0.5) / 100.0
        for i in range(len(pts) - 1):
            a, c = pts[i], pts[i + 1]
            segs.append((
                a[0] * 0.01, a[1] * 0.01, a[2] * 0.01,
                c[0] * 0.01, c[1] * 0.01, c[2] * 0.01,
                hw_m,
            ))

    def lowered_height(raw_z, deck_z, distance):
        # Leave enough room for interpolation between vertices at the road shoulder.
        blend = max(0.0, min(1.0, (distance - 20.0) / 30.0))
        return raw_z + min(0.0, deck_z - 0.70 - raw_z) * (1.0 - blend * blend * (3.0 - 2.0 * blend))

    def road_clamp(xm, ym, raw_z):
        z = raw_z
        for ax, ay, az, bx, by, bz, hw in segs:
            margin = hw + 50.0
            if (xm < min(ax, bx) - margin or xm > max(ax, bx) + margin
                    or ym < min(ay, by) - margin or ym > max(ay, by) + margin):
                continue
            vx, vy = bx - ax, by - ay
            l2 = vx * vx + vy * vy
            if l2 < 1e-4:
                t = 0.0
            else:
                t = max(0.0, min(1.0, ((xm - ax) * vx + (ym - ay) * vy) / l2))
            px, py, pz = ax + vx * t, ay + vy * t, az + (bz - az) * t
            # At crossings, the lower road must stay clear even when the higher road is closer in XY.
            z = min(z, lowered_height(raw_z, pz, math.hypot(xm - px, ym - py) - hw))
        for cx, cy, cz, rad in ((-600.0, -860.0, 61.5, 32.0), (-549.0, 372.0, 15.6, 28.0)):
            z = min(z, lowered_height(raw_z, cz, math.hypot(xm - cx, ym - cy) - rad))
        return z

    def emit_patch(x0, x1, y0, y1, nx, ny, height_fn, rock_z_thresh):
        grid = []
        for iy in range(ny + 1):
            row = []
            fy = iy / float(ny)
            ym = y0 + (y1 - y0) * fy
            # Fade outer boundary smoothly below main ground plane (-5.5 m) so no cliff skirt shows
            ey = min(fy, 1.0 - fy) * 5.5
            ey_mask = 1.0 if ey >= 1.0 else ey * ey * (3.0 - 2.0 * ey)
            for ix in range(nx + 1):
                fx = ix / float(nx)
                xm = x0 + (x1 - x0) * fx
                ex = min(fx, 1.0 - fx) * 5.5
                ex_mask = 1.0 if ex >= 1.0 else ex * ex * (3.0 - 2.0 * ex)
                edge_m = ex_mask * ey_mask
                raw_z = -5.5 + (height_fn(xm, ym) + 5.5) * edge_m
                zm = road_clamp(xm, ym, raw_z)
                row.append((xm, ym, zm))
            grid.append(row)

        for iy in range(ny):
            for ix in range(nx):
                p00 = grid[iy][ix]
                p10 = grid[iy][ix + 1]
                p11 = grid[iy + 1][ix + 1]
                p01 = grid[iy + 1][ix]
                # Skip triangles completely submerged below flat city ground
                if max(p00[2], p10[2], p11[2], p01[2]) < -3.5:
                    continue
                avg_z = 0.25 * (p00[2] + p10[2] + p11[2] + p01[2])
                dz = max(p00[2], p10[2], p11[2], p01[2]) - min(p00[2], p10[2], p11[2], p01[2])
                cell_span = max(abs(p10[0] - p00[0]), abs(p01[1] - p00[1]), 1.0)
                # Rock on steep faces or alpine altitude, Mediterranean grass on lower rolling foothills
                slot = 0 if (avg_z > rock_z_thresh or (dz / cell_span) > 0.36) else 1
                # Outward upward normal (+Z) matching add_box top face: (x0,y0) -> (x1,y0) -> (x1,y1) -> (x0,y1)
                b.add_quad(slot, p00, p10, p11, p01)

    def kuzey_height(x, y):
        # Main summit massif under Seyir Tepesi (-600, -860) + alpine ridge behind it + east switchback shoulder
        summit = 66.0 * math.exp(-(((x + 580.0) / 310.0) ** 2 + ((y + 870.0) / 165.0) ** 2))
        back_peak = 36.0 * math.exp(-(((x + 480.0) / 360.0) ** 2 + ((y + 985.0) / 105.0) ** 2))
        east_spur = 34.0 * math.exp(-(((x + 170.0) / 230.0) ** 2 + ((y + 775.0) / 140.0) ** 2))
        west_ridge = 38.0 * math.exp(-(((x + 830.0) / 190.0) ** 2 + ((y + 720.0) / 200.0) ** 2))
        crags = (
            4.2 * math.sin(x * 0.023 + y * 0.019)
            + 2.8 * math.cos(x * 0.041 - y * 0.037)
            + 1.6 * math.sin(x * 0.073 + y * 0.061)
        )
        base = summit + back_peak + east_spur + west_ridge
        return base + crags * min(1.0, base / 25.0)

    def yamac_height(x, y):
        upper = 27.5 * math.exp(-(((x + 730.0) / 220.0) ** 2 + ((y - 420.0) / 220.0) ** 2))
        mid = 14.5 * math.exp(-(((x + 510.0) / 210.0) ** 2 + ((y - 360.0) / 230.0) ** 2))
        south_knoll = 12.0 * math.exp(-(((x + 540.0) / 190.0) ** 2 + ((y - 590.0) / 150.0) ** 2))
        undulate = 2.0 * math.sin(x * 0.025 - y * 0.021) + 1.2 * math.cos(x * 0.048 + y * 0.034)
        base = upper + mid + south_knoll
        return base + undulate * min(1.0, base / 14.0)

    emit_patch(-1040.0, 120.0, -1080.0, -560.0, 80, 48, kuzey_height, rock_z_thresh=21.0)
    emit_patch(-940.0, -240.0, 80.0, 740.0, 52, 48, yamac_height, rock_z_thresh=19.5)


def build_race_ring(b):
    """Sleek octagonal holographic start-zone ground ring (radius 4.6 m, height 2.9 m, UV V=0 bottom..1 top)."""
    segs = 24
    r = 4.6
    h = 2.9
    for i in range(segs):
        a0 = (i / float(segs)) * 2.0 * math.pi
        a1 = ((i + 1) / float(segs)) * 2.0 * math.pi
        u0 = i / float(segs)
        u1 = (i + 1) / float(segs)
        p00 = (math.cos(a0) * r, math.sin(a0) * r, 0.08)
        p10 = (math.cos(a1) * r, math.sin(a1) * r, 0.08)
        p11 = (math.cos(a1) * (r * 0.94), math.sin(a1) * (r * 0.94), h)
        p01 = (math.cos(a0) * (r * 0.94), math.sin(a0) * (r * 0.94), h)
        b.add_quad_uv(0, p00, p10, p11, p01, (u0, 0.0), (u1, 0.0), (u1, 1.0), (u0, 1.0))
    # Floating inner ground curb ring
    r_in = 3.9
    for i in range(segs):
        a0 = (i / float(segs)) * 2.0 * math.pi
        a1 = ((i + 1) / float(segs)) * 2.0 * math.pi
        p0 = (math.cos(a0) * r_in, math.sin(a0) * r_in, 0.14)
        p1 = (math.cos(a1) * r_in, math.sin(a1) * r_in, 0.14)
        p2 = (math.cos(a1) * r, math.sin(a1) * r, 0.14)
        p3 = (math.cos(a0) * r, math.sin(a0) * r, 0.14)
        b.add_quad_uv(0, p0, p1, p2, p3, (0.0, 0.0), (1.0, 0.0), (1.0, 0.2), (0.0, 0.2))


def build_checkpoint_chevron(b):
    """Sleek tapered holographic gate pillar + angled inner chevron wing (height 5.5 m, UV V=0 bottom..1 top)."""
    # Tapered vertical light fin along Z (0..5.5 m)
    w0, w1 = 0.45, 0.18
    for s in (-1.0, 1.0):
        b.add_quad_uv(
            0,
            (-w0, s * 0.12, 0.0), (w0, s * 0.12, 0.0),
            (w1, s * 0.08, 5.5), (-w1, s * 0.08, 5.5),
            (0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0),
        )
    # Two glowing floating arrow chevrons pointing along +X
    for zc in (2.2, 3.8):
        b.add_quad_uv(
            0,
            (-0.55, 0.0, zc - 0.35), (0.75, 0.0, zc),
            (0.75, 0.0, zc + 0.28), (-0.55, 0.0, zc + 0.65),
            (0.0, 0.1), (1.0, 0.1), (1.0, 0.4), (0.0, 0.4),
        )


def build_traffic_suv(b):
    """4.68 m sleek modern urban crossover SUV (+X forward, Z=0 ground; replaces buggy in civilian traffic)."""
    # Slots: 0=Paint, 1=Glass, 2=Trim, 3=Headlight, 4=Taillight
    lower = [
        (2.32, 0.36), (2.34, 0.80), (2.14, 1.04), (1.20, 1.12),
        (-1.82, 1.14), (-2.30, 1.06), (-2.34, 0.68), (-2.24, 0.36),
    ]
    lower_w = [0.88, 0.94, 0.96, 0.98, 0.98, 0.94, 0.92, 0.88]
    _add_extruded_profile(b, 0, lower, lower_w)
    cabin = [
        (1.12, 1.12), (0.52, 1.72), (-1.56, 1.70), (-2.18, 1.10),
    ]
    cabin_w = [0.92, 0.80, 0.79, 0.90]
    _add_extruded_profile(b, 1, cabin, cabin_w)
    # Painted roof + low-profile roof rails
    b.add_box(0, (-0.52, 0.0, 1.73), (2.06, 1.58, 0.05))
    for s in (-1.0, 1.0):
        b.add_box(2, (-0.54, s * 0.68, 1.78), (1.76, 0.06, 0.05))
        b.add_box(0, (0.74, s * 1.04, 1.18), (0.16, 0.16, 0.11))
        b.add_box(3, (2.32, s * 0.68, 0.84), (0.10, 0.36, 0.14))
        b.add_box(4, (-2.32, s * 0.68, 0.96), (0.08, 0.38, 0.18))
    # Lower protective wheel-arch cladding & bumpers
    b.add_box(2, (2.34, 0.0, 0.50), (0.14, 1.78, 0.28))
    b.add_box(2, (-2.32, 0.0, 0.48), (0.14, 1.76, 0.26))
    b.add_box(2, (0.0, 0.0, 0.34), (4.00, 1.92, 0.10))
    _add_car_wheels(b, 2, 1.44, -1.42, 0.98, radius=0.38, width=0.25)


def _add_cambered_blade(b, mat, x_center, z_base, half_span, chord, pitch_deg, thick=0.032, segs=6):
    """Sculpts a curved, cambered carbon airfoil blade spanning -half_span..+half_span along Y."""
    pr = math.radians(pitch_deg)
    cp, sp = math.cos(pr), math.sin(pr)
    for i in range(segs):
        t0 = -1.0 + 2.0 * (i / float(segs))
        t1 = -1.0 + 2.0 * ((i + 1) / float(segs))
        y0, y1 = t0 * half_span, t1 * half_span
        # Subtle swept-back curve toward wingtips
        sw0, sw1 = -0.045 * (t0 * t0), -0.045 * (t1 * t1)
        di0, di1 = 0.025 * (t0 * t0), 0.025 * (t1 * t1)
        hc = chord * 0.5
        ht = thick * 0.5
        # Leading (+X) and trailing (-X) edges rotated by pitch_deg
        lx0, lz0 = x_center + sw0 + hc * cp, z_base + di0 - hc * sp
        tx0, tz0 = x_center + sw0 - hc * cp, z_base + di0 + hc * sp
        lx1, lz1 = x_center + sw1 + hc * cp, z_base + di1 - hc * sp
        tx1, tz1 = x_center + sw1 - hc * cp, z_base + di1 + hc * sp
        u0, u1 = (t0 + 1.0) * 3.0, (t1 + 1.0) * 3.0
        # Top & bottom airfoil skins
        b.add_quad_uv(mat, (tx0, y0, tz0 + ht), (lx0, y0, lz0 + ht * 0.8),
                      (lx1, y1, lz1 + ht * 0.8), (tx1, y1, tz1 + ht),
                      (u0, 0.0), (u0, 1.5), (u1, 1.5), (u1, 0.0))
        b.add_quad_uv(mat, (lx0, y0, lz0 - ht * 0.6), (tx0, y0, tz0 - ht * 0.6),
                      (tx1, y1, tz1 - ht * 0.6), (lx1, y1, lz1 - ht * 0.6),
                      (u0, 0.0), (u0, 1.5), (u1, 1.5), (u1, 0.0))
        # Leading & trailing edge caps
        b.add_quad(mat, (lx0, y0, lz0 - ht * 0.6), (lx1, y1, lz1 - ht * 0.6),
                   (lx1, y1, lz1 + ht * 0.8), (lx0, y0, lz0 + ht * 0.8))
        b.add_quad(mat, (tx1, y1, tz1 - ht * 0.6), (tx0, y0, tz0 - ht * 0.6),
                   (tx0, y0, tz0 + ht), (tx1, y1, tz1 + ht))


def build_aero_splitter(b):
    """Curved carbon front aero splitter with side winglets & support tie-rods."""
    segs = 8
    half_w = 0.82
    for i in range(segs):
        t0 = -1.0 + 2.0 * (i / float(segs))
        t1 = -1.0 + 2.0 * ((i + 1) / float(segs))
        y0, y1 = t0 * half_w, t1 * half_w
        # Tapered curved nose protruding furthest at center
        fx0 = 0.16 - 0.09 * (t0 * t0)
        fx1 = 0.16 - 0.09 * (t1 * t1)
        bx = -0.18
        u0, u1 = (t0 + 1.0) * 3.0, (t1 + 1.0) * 3.0
        b.add_quad_uv(0, (bx, y0, 0.015), (fx0, y0, 0.015), (fx1, y1, 0.015), (bx, y1, 0.015),
                      (u0, 0.0), (u0, 1.2), (u1, 1.2), (u1, 0.0))
        b.add_quad_uv(0, (fx0, y0, -0.015), (bx, y0, -0.015), (bx, y1, -0.015), (fx1, y1, -0.015),
                      (u0, 0.0), (u0, 1.2), (u1, 1.2), (u1, 0.0))
        b.add_quad(0, (fx0, y0, -0.015), (fx1, y1, -0.015), (fx1, y1, 0.015), (fx0, y0, 0.015))
    # Corner dive-plane winglets + twin front tie-rods
    for s in (-1.0, 1.0):
        b.add_box(0, (-0.04, s * (half_w + 0.01), 0.055), (0.26, 0.025, 0.10))
        b.add_beam(0, (0.08, s * 0.34, 0.015), (-0.04, s * 0.32, 0.15), 0.018)


def build_aero_diffuser(b):
    """Sculpted rear carbon venturi diffuser tray with 4 tapered shark-fin aero strakes."""
    b.add_quad_uv(0, (-0.14, -0.62, 0.05), (0.16, -0.62, -0.02),
                  (0.16, 0.62, -0.02), (-0.14, 0.62, 0.05),
                  (0.0, 0.0), (0.0, 1.2), (4.0, 1.2), (4.0, 0.0))
    b.add_box(0, (0.02, 0.0, 0.03), (0.28, 1.24, 0.035))
    for fy in (-0.44, -0.16, 0.16, 0.44):
        b.add_box(0, (-0.02, fy, -0.035), (0.30, 0.026, 0.12))


def build_aero_gt_wing(b):
    """Sculpted swan-neck GT carbon rear wing with cambered airfoil blade & angled endplates (Z=0 trunk mount)."""
    # Twin curved swan-neck pylons
    for s in (-1.0, 1.0):
        py = s * 0.44
        b.add_beam(0, (0.05, py, 0.0), (-0.04, py, 0.24), 0.045)
        b.add_beam(0, (-0.04, py, 0.24), (0.02, py, 0.40), 0.042)
        b.add_beam(0, (0.02, py, 0.40), (-0.06, py, 0.39), 0.036)
        b.add_box(0, (0.05, py, 0.01), (0.18, 0.09, 0.02))
        # Sculpted angled GT endplates
        b.add_box(0, (-0.05, s * 0.95, 0.38), (0.38, 0.026, 0.24))
    _add_cambered_blade(b, 0, -0.04, 0.37, half_span=0.94, chord=0.29, pitch_deg=-7.5, thick=0.036, segs=6)


def build_aero_rally_wing(b):
    """Box-arch rally double-element carbon rear wing with side uprights & center vortex fins."""
    for s in (-1.0, 1.0):
        b.add_beam(0, (0.04, s * 0.82, 0.0), (-0.06, s * 0.84, 0.32), 0.065)
        b.add_box(0, (-0.04, s * 0.85, 0.20), (0.32, 0.032, 0.28))
    # Lower deck blade + upper cambered main blade + twin center stabilizers
    _add_cambered_blade(b, 0, -0.02, 0.14, half_span=0.82, chord=0.18, pitch_deg=-3.0, thick=0.026, segs=4)
    _add_cambered_blade(b, 0, -0.06, 0.32, half_span=0.86, chord=0.25, pitch_deg=-6.0, thick=0.034, segs=6)
    for fy in (-0.26, 0.26):
        b.add_box(0, (-0.05, fy, 0.23), (0.20, 0.022, 0.16))


def build_aero_blower(b):
    """Chrome muscle supercharger block + pulley snout + triple-butterfly bug-catcher intake scoop."""
    # Slots: 0=Chrome, 1=Dark Carbon intake interior
    b.add_box(0, (0.0, 0.0, 0.07), (0.56, 0.38, 0.14))
    # Ribbed blower cooling fins along sides
    for rz in (0.04, 0.08, 0.12):
        b.add_box(0, (0.0, 0.0, rz), (0.52, 0.42, 0.016))
    # Front drive snout & belt pulley
    b.add_box(0, (0.34, 0.0, 0.07), (0.14, 0.14, 0.10))
    b.add_cylinder_y(1, (0.41, 0.0, 0.07), 0.065, 0.05, segments=12)
    # Raised triple-throat bug-catcher scoop
    b.add_frustum_box(0, (0.04, 0.0, 0.14), (0.44, 0.34), (0.48, 0.30), 0.13)
    for by in (-0.09, 0.0, 0.09):
        b.add_box(1, (0.27, by, 0.205), (0.05, 0.065, 0.085))


def build_aero_offroad_rack(b):
    """Expedition tubular roof rack basket with 4 front-facing LED rally light pods."""
    # Slots: 0=Dark Carbon/Metal frame, 1=LampGlow lenses
    hx, hy = 0.66, 0.54
    # Perimeter basket rails (lower & upper)
    for zr in (0.03, 0.11):
        for s in (-1.0, 1.0):
            b.add_beam(0, (-hx, s * hy, zr), (hx, s * hy, zr), 0.032)
            b.add_beam(0, (s * hx, -hy, zr), (s * hx, hy, zr), 0.032)
    # Corner stanchions & crossbars
    for sx in (-hx, hx):
        for sy in (-hy, hy):
            b.add_beam(0, (sx, sy, -0.02), (sx, sy, 0.11), 0.034)
    for cx in (-0.42, 0.0, 0.42):
        b.add_beam(0, (cx, -hy, 0.035), (cx, hy, 0.035), 0.028)
    # 4 front rally LED spotlight pods
    for ly in (-0.36, -0.12, 0.12, 0.36):
        b.add_box(0, (hx + 0.04, ly, 0.10), (0.11, 0.14, 0.12))
        b.add_box(1, (hx + 0.10, ly, 0.10), (0.02, 0.11, 0.09))


def build_sky_dome(b):
    """Inward-facing 2320 m upper sky hemisphere with horizon-to-zenith V=0..1 UVs for seamless cloud panning."""
    radius = 2320.0
    az_segs = 36
    el_segs = 10
    el_min = math.radians(-3.0)
    el_max = math.radians(86.0)
    for ie in range(el_segs):
        v0 = ie / float(el_segs)
        v1 = (ie + 1) / float(el_segs)
        e0 = el_min + (el_max - el_min) * v0
        e1 = el_min + (el_max - el_min) * v1
        r0, z0 = radius * math.cos(e0), radius * math.sin(e0)
        r1, z1 = radius * math.cos(e1), radius * math.sin(e1)
        for ia in range(az_segs):
            u0 = ia / float(az_segs)
            u1 = (ia + 1) / float(az_segs)
            a0 = u0 * 2.0 * math.pi
            a1 = u1 * 2.0 * math.pi
            p00 = (math.cos(a0) * r0, math.sin(a0) * r0, z0)
            p10 = (math.cos(a1) * r0, math.sin(a1) * r0, z0)
            p11 = (math.cos(a1) * r1, math.sin(a1) * r1, z1)
            p01 = (math.cos(a0) * r1, math.sin(a0) * r1, z1)
            # Inward-facing winding so camera inside the dome sees the clouds cleanly
            b.add_quad_uv(0, p00, p01, p11, p10, (u0 * 2.0, v0), (u0 * 2.0, v1), (u1 * 2.0, v1), (u1 * 2.0, v0))


def build_jump_ramp(b):
    """Smooth 10.4 m long x 5.4 m wide stunt jump launch ramp along +X (Z=0..2.35 m, curved takeoff)."""
    # Slots: 0=Concrete deck, 1=HazardYellow side stripes, 2=MetalDark side struts
    length = 10.4
    half_w = 2.7
    segs = 8
    x_start = -length * 0.5
    max_h = 2.35

    def z_of(t):
        # Smooth quadratic takeoff ramp (zero entry lip at t=0, ~22 deg launch lip at t=1)
        return max_h * (0.35 * t + 0.65 * t * t)

    for i in range(segs):
        t0, t1 = i / float(segs), (i + 1) / float(segs)
        x0, x1 = x_start + t0 * length, x_start + t1 * length
        z0, z1 = z_of(t0), z_of(t1)
        # Top drivable ramp surface (+Z normal)
        b.add_quad(0, (x0, -half_w, z0), (x1, -half_w, z1), (x1, +half_w, z1), (x0, +half_w, z0))
        # Solid side walls down to Z=-0.2 m so ramp sits flush on slightly sloped roads
        b.add_quad(2, (x0, -half_w, -0.2), (x1, -half_w, -0.2), (x1, -half_w, z1), (x0, -half_w, z0))
        b.add_quad(2, (x1, +half_w, -0.2), (x0, +half_w, -0.2), (x0, +half_w, z0), (x1, +half_w, z1))
        # High-visibility hazard curb strips on both edges
        for s in (-1.0, 1.0):
            cy = s * (half_w - 0.14)
            b.add_quad(1, (x0, cy - 0.14, z0 + 0.03), (x1, cy - 0.14, z1 + 0.03),
                       (x1, cy + 0.14, z1 + 0.03), (x0, cy + 0.14, z0 + 0.03))
    # Rear vertical wall at launch end (+X) & bottom cap
    x_end = x_start + length
    b.add_quad(2, (x_end, -half_w, -0.2), (x_end, +half_w, -0.2), (x_end, +half_w, max_h), (x_end, -half_w, max_h))
    b.add_quad(2, (x_start, +half_w, -0.2), (x_end, +half_w, -0.2), (x_end, -half_w, -0.2), (x_start, -half_w, -0.2))


def build_police_heli_body(b):
    """Karamara police helicopter fuselage (~11 m long, origin at rotor hub base, X forward)."""
    # Slots: 0=Body, 1=Canopy glass, 2=Skids/trim, 3=Red beacon, 4=Blue beacon
    # Cabin: tapered nose + main cabin + engine hump
    b.add_frustum_box(0, (0.6, 0.0, -2.1), (3.4, 1.9), (3.0, 1.6), 1.55)
    b.add_frustum_box(1, (2.55, 0.0, -1.95), (1.1, 1.6), (0.5, 1.2), 1.15)
    b.add_frustum_box(0, (-0.4, 0.0, -0.55), (2.2, 1.2), (1.6, 0.9), 0.45)
    # Tail boom tapering aft + fin + horizontal stabilizer
    b.add_frustum_cyl(0, (-1.0, 0.0, -1.2), 0.42, 0.18, 0.1, segments=8)
    b.add_beam(0, (-1.0, 0.0, -1.15), (-6.4, 0.0, -0.85), 0.36)
    b.add_box(0, (-6.3, 0.0, -0.25), (0.9, 0.12, 1.3))
    b.add_box(0, (-5.6, 0.0, -0.9), (0.6, 1.9, 0.08))
    # Skids
    for s in (-1.0, 1.0):
        b.add_beam(2, (1.6, s * 1.05, -2.55), (-1.6, s * 1.05, -2.55), 0.1)
        b.add_beam(2, (1.6, s * 1.05, -2.55), (2.0, s * 1.05, -2.35), 0.1)
        b.add_beam(2, (0.9, s * 0.8, -2.1), (0.9, s * 1.05, -2.55), 0.08)
        b.add_beam(2, (-0.9, s * 0.8, -2.1), (-0.9, s * 1.05, -2.55), 0.08)
    # Rotor mast + belly beacons
    b.add_frustum_cyl(2, (0.0, 0.0, -0.15), 0.14, 0.1, 0.15, segments=8)
    b.add_box(3, (0.2, -0.45, -2.15), (0.3, 0.22, 0.1))
    b.add_box(4, (0.2, 0.45, -2.15), (0.3, 0.22, 0.1))


def build_police_heli_rotor(b):
    """Main rotor: 4 blades, 11 m disc, hub at origin (spun in code around Z)."""
    b.add_frustum_cyl(0, (0.0, 0.0, -0.06), 0.28, 0.2, 0.14, segments=10)
    for i in range(4):
        a = i * math.pi * 0.5
        tip = (math.cos(a) * 5.5, math.sin(a) * 5.5, 0.02)
        b.add_beam(0, (0.0, 0.0, 0.02), tip, 0.16)
