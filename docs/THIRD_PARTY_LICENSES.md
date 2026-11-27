# Third-Party Asset Licenses & Attributions

BLACK NUMBERS uses strictly royalty-free **CC0 1.0 Public Domain**, **CC-BY 4.0**, and **Unreal Engine EULA** assets.

## 1. Poly Haven PBR Textures, PureSky & 3D Models — CC0 1.0 Universal (Public Domain)
- **Source:** [Poly Haven](https://polyhaven.com/)
- **License:** [CC0 1.0 Universal](https://polyhaven.com/license) (no attribution required, documented for provenance)
- **PBR Surface Textures (`Tools/fetch_cc0_textures.py`):**
  `asphalt_02`, `asphalt_pit_lane`, `concrete_floor_02`, `precast_stone_paving`, `cobblestone_02`, `dirt_aerial_02`, `sparse_grass`, `aerial_rocks_04`, `aerial_grass_rock`, `cliff_side`, `corrugated_iron_02`, `wood_planks`, `bark_brown_02`, `red_brick_03`, `clay_plaster`, `exterior_wall_cladding_02`, `concrete_tile_facade`, `autumn_field_puresky`.
- **Eski Liman bridge steel:** [Rusty Metal 02 by Rob Tuytel](https://polyhaven.com/a/rusty_metal_02), imported as 1K albedo, normal and roughness maps. The albedo is neutralized for the game's red steel tint.
- **3D Street / Industrial / Garage / Nature Props (`Tools/fetch_cc0_models.py`, 36 models):**
  `concrete_road_barrier`, `concrete_road_barrier_02`, `fire_hydrant`, `metal_trash_can`, `utility_box_01`, `utility_box_02`, `water_manhole_cover`, `modular_street_seating`, `security_camera_01`, `exterior_aircon_unit`, `modular_fire_escape`, `modular_chainlink_fence`, `rollershutter_door`, `flood_lights`, `covered_car`, `old_tyre`, `Barrel_01`, `metal_tool_chest`, `boulder_01`, `modular_urban_apartments_facade`, `modular_factory_facade`, `rollershutter_window_01`, `modular_metal_gutter`, `modular_airduct_circular_01`, `modular_electricity_poles`, `large_iron_gate`, `street_lamp_01`, `street_lamp_02`, `security_light`, `trashbag`, `wooden_crate_01`, `lifebuoy`, `planter_box_01`, `tool_cart`, `rusted_wheel_rim_01`, `shrub_01`.

## 1b. ambientCG Building Facades — CC0 1.0 Universal (Public Domain)

- Source: https://ambientcg.com/ (license: https://docs.ambientcg.com/license/) — no attribution required.
- Sets: `Facade006`, `Facade018A`, `Facade018B`, `Facade019A`, `Facade020A` (1K-JPG), fetched and cached by
  `Tools/fetch_cc0_textures.py` (`AMBIENTCG_FACADES`) into `T_{Brick,Plaster,Cladding,ConcreteTower,GlassSteel}_*`.
  Night window emission is synthesized from each facade's glass mask when the set has no usable emission map.

## 2. External 3D Vehicle Models — CC0 1.0 & CC-BY 4.0
- **Khronos `CarConcept.glb` — CC-BY 4.0** (`/Game/Vehicles/GrandTourer/SM_BN_GrandTourer`, powering `Draco RS-T`)
  - **Author / Copyright:** Eric Chadwick / Darmstadt Graphics Group GmbH (via [KhronosGroup/glTF-Sample-Assets](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/CarConcept))
  - **License:** [Creative Commons Attribution 4.0 International (CC-BY 4.0)](https://creativecommons.org/licenses/by/4.0/)
  - **Modifications:** Flattened to single StaticMesh, static wheels stripped for live Chaos wheel rotation, Nanite disabled, LOD reduced, neutral metallic trim over badge slot, and dynamic clearcoat paint tinting.
- **`BMW M6 2006` — CC0 1.0 Universal (Public Domain)** (`/Game/Vehicles/External/SM_Ext_ValkenRS4`, powering `Valken RS-4`)
  - **Original Author:** tyrant monkey (Blend Swap #3557; glTF conversion via [ErfanMo77/gltf-research-scenes](https://github.com/ErfanMo77/gltf-research-scenes/tree/main/scenes/bmw-m6))
  - **License:** [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/)
  - **Modifications:** Studio floor & static wheel/brake meshes stripped, brand logos neutralized to dark trim, normalized to UE +X forward & game scale, Nanite disabled, LOD reduced to ~30k tris, dynamic paint tinting enabled.
- **`1967 Pontiac GTO` — CC0 1.0 Universal (Public Domain)** (`/Game/Vehicles/External/SM_Ext_Vandal440R`, powering `Vandal 440R`)
  - **Original Author:** thecali (Blend Swap #75850; Benedikt Bitterli Rendering Resources; glTF conversion via [ErfanMo77/gltf-research-scenes](https://github.com/ErfanMo77/gltf-research-scenes/tree/main/scenes/pontiac-gto))
  - **License:** [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/)
  - **Modifications:** Studio ground plane & wheel-well tires stripped, normalized to UE +X forward & game scale, Nanite disabled, LOD reduced to ~30k tris, dynamic paint tinting enabled.
- **`BMW X5 2018 (G05)` — CC-BY 4.0** (`/Game/Vehicles/External/SM_Ext_TauronV6Rally` powering `Tauron V6 Rally`, and `/Game/Vehicles/External/SM_Ext_HeavyRhino` powering Heat 5 `Heavy Rhino` Armored Police SUV)
  - **Author / Copyright:** BMW Car IT GmbH ([bmwcarit/digital-car-3d](https://github.com/bmwcarit/digital-car-3d/tree/master/G05))
  - **License:** [Creative Commons Attribution 4.0 International (CC-BY 4.0)](https://creativecommons.org/licenses/by/4.0/)
  - **Modifications:** Exterior, doors (mirrored left/right), hood, tailgate, headlights, taillights, and interior seats merged into a single open-wheel-arch StaticMesh, normalized to UE +X forward & SUV scale, Nanite disabled, LOD reduced to ~28k–32k tris, dynamic paint tinting & tactical obsidian matte variant added.

## 3. Unreal Engine 5.8 Template Vehicles — Unreal Engine EULA
- **Assets:** `SKM_SportsCar`, `SM_SportsCar`, `SM_SportsCar_Wheel`, `SM_Offroad_Body`, `SM_AutomotiveTP_Car`
- **Source:** Epic Games Unreal Engine 5.8 built-in Vehicle / Automotive templates (staged by `Tools/import_ue_vehicles.py`).

## 4. Typography — SIL Open Font License 1.1 (OFL-1.1)
- **Source:** [Google Fonts](https://fonts.google.com/) (`Content/UI/Fonts/`)
- **License:** [SIL Open Font License 1.1](https://openfontlicense.org/)
- **Bundled Fonts:** `NotoSans-CondensedBold.ttf`, `NotoSans-CondensedMedium.ttf`, `NotoSans-ExtraCondensedBlackItalic.ttf`. All verified for full Turkish glyph coverage (`İıŞşĞğÇçÖöÜü`).

## 5. Original Procedural Assets — Project Copyright
- Procedural glTF landmarks, bridge, cranes, ships, sculpted carbon aero kits, traffic vehicles, jump ramps, sky dome, procedural audio WAVs, and UI textures generated by `Tools/bn_*.py`.
