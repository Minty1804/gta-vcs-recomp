# GPS route on the radar and the pause map

Status: **works** (`src/game/vcs_gps.cpp`; olaftnt's upstream PR #183, checked and extended here). Setting: F2 > Game >
Map and saving > **GPS route** - Off (default) / In a vehicle / Always; `VCS_GPS=0/1/2`, `VCS_GPS_HOOKS=0` installs no
hook. The control channel's `gps` command reports the target, the node count and the quads drawn.

(Translated from the Polish original and shortened on 2026-11-23; the addresses and measurements are unchanged.)

## What it does

- A line along the roads to the **mission's target** (the active blip with no icon, `+0x29 == 0`), in the blip's
  colour, under the blips and the player's arrow, clipped to the radar's circle.
- A second, **red** line to the **player's own target**: the square placed on the pause map (Space on the keyboard,
  field +0x26). `FUN_0026BE00` then calls `FUN_00115030` = SetTargetBlip (type 4, colour 0, **icon +0x29 = 0x10**,
  flags |0x10, display 2); the mod looks for the blip with icon 0x10. Right click in the menu stays "back".
- Both routes are drawn on the **pause map** too: `0x113418` also calls `FUN_0026BA60` (map drawing) with
  `radar+0x1AD9 = 1`; `FUN_00111130` then sets the centre (0, 0), range 1000 and rotation 0, and `FUN_00114758` places
  points from `+0x1ADC / +0x1AE0` (the map's offset) and `+0x1AE4` (zoom) x 0.56 x (4/3, or 1 for 16:9 TV) around
  (-0.4256, -0.1166).
- Routes are hidden indoors. The off-road ends (player -> first node, last node -> target) are drawn only up to
  `kOffRoad` = 12 m, so a target behind a wall does not get a line through it.
- **Water nodes skipped**: the car node table also holds the boats' routes, flagged **+8 bit 0x100** (z = 6, sea
  level) - separate components along the coast. Without the filter the first mission of a new game routed through the
  sea; with it, 37 nodes over land.
- **Roads over each other** (2026-11-15): the nearest node to the player and to the target is searched in 3D (node
  height: s8 at +4, whole metres; differences up to 2 m ignored, above that x10; a map target with z = 0 uses x, y
  only). The first mission's target (-1823.9, -715.9, z 14.3) is on Fort Baxter's upper deck: by x, y alone node 2406
  on the road below won; now 1035 does and the route ends up the ramp 9 -> 11 -> 13. The car network has 20 such node
  pairs (<= 12 m apart, >= 5 m above each other), mostly Fort Baxter and around (-800, 0).
- **Hand-fixed route end** (`kDirtyFixRouteEnds`, `dirtyFixMissionSoldierRouteEnd`): mission 1 "Soldier", "Go to your
  barracks" - the blip at about (-1648.87, -261.00) gets the route end (-1733.14, -314.18, z 14.71); checked with a
  blip placed by ADD_BLIP_FOR_COORD (`mission=-1733.1,-314.2 nodes=4`).

Ideas (no code; that repository has no licence) from GTA-GPS-Redux for San Andreas.

## Checked in the running game (2026-11-14, hidden rig, a card copy after "Soldier")

- Hidden test (mission 44 JER_A2, teleports along the route): the line sits under the blips and arrow, clipped to the
  radar, in the blip's colour (purple for colour 5); the route shrinks 110 -> 75 -> 46 -> 22 nodes. Both routes on the
  radar and the map; the target lands under the cursor.
- **Radar**: `*0x48F548` = 0x74E800; range `+0x1AC8` = 30.0 m on foot; centre `0x4D3E68` = the player's x, y;
  `0x4D3E70` / `0x4D3E74` = sin / cos.
- **Blips**: ADD_BLIP_FOR_COORD took free slot 4, handle `0x20004` = index | (counter << 16); z = -100 lands on the
  ground. Icon blips (contacts, safehouses): `+0x28 = 01`, **`+0x29` = icon id** (8, 0x0A, 0x2B...), flags 0x7E /
  0x76. The **mission target** (JER_A2): colour 5, type 4 (coord), `+0x29 = 0`, `+0x28 = 03`, flags 0x66. Type 5: an
  icon blip for a coordinate / contact point (colour 2). Car and ped blips keep (0, 0) in the entry; the radar reads
  the entity live.
- **ThePaths** `*0x487490` = 0x1A44D70: `+0x0C` = 8387 nodes in all, **`+0x10` = 3079 car nodes (0..3078)**, `+0x14`
  = 5308 ped nodes (3079..8386), **`+0x7BA0` = the links**. The whole network is in memory at once (the pointer and
  header do not change across the islands): no zone streaming.
- The graph is symmetric, car links never lead to ped nodes. Cars span x -2080..1501, y -1838..1817. 13 connected
  components (2391 nodes in the main one, then 266, 185, 125, 28...): a target in another component is routed to the
  nearest reachable node. Links' top 2 bits: 0 (17320x), 2 (56x), 3 (276x) - meaning unknown.
- **A\***: the nearest node to the player matches the game's GET_CLOSEST_CAR_NODE; Fort Baxter to the mission target
  (-711.5, -1541.1) is 110 nodes, 2000 m (1545 m straight), 362 nodes expanded - a fraction of a millisecond.
- The pause map shifts point blips while `radar+0x1AF0` (vec3, an interior's entrance) is non-zero
  (`FUN_001112C0`); seen in rig tests after teleporting out of the barracks without the door; zero outdoors in normal
  play.

Not checked: driving in the "In a vehicle" mode at length, targets on a car or ped, interiors, the meaning of the
links' top bits.

## The game's structures (static analysis; command numbers from the Sanny Builder Library, handlers from the opcode table at 0x4BEAF0)

**Blips (CRadar)**: the radar object is `*(u32*)0x48F548` (created lazily by `func_471A48`).

| Command | Opcode | Handler | Calls |
|---|---|---|---|
| REMOVE_BLIP | 0x00BB | 0x35B038 | `func_1153B0(radar, handle)` = ClearBlip |
| ADD_BLIP_FOR_CAR / CHAR | 0x00C0 / 0x00C1 | 0x35B380 / 0x35B498 | |
| ADD_BLIP_FOR_COORD | 0x00C3 | 0x35B6C8 | `func_114F00(radar, type 4, &pos, colour 5, display 3)`, then `func_115590(radar, handle, 3)` |
| CHANGE_BLIP_DISPLAY | 0x00C4 | 0x35B840 | |
| ADD_SPRITE_BLIP_FOR_COORD | 0x00C6 | 0x35BA30 | |

`func_114F00` = SetCoordBlip: a free slot in **`radar + 0x280`, 75 entries of 0x30 bytes** (in use: flag bit 2);
handle from `func_110E30(radar, index)`, -1 when full. Entry: +0x00 u32 colour, +0x04 u32 type (4 coord), +0x08 u32
entity handle, +0x10 vec4 position, +0x20 u32 flags (bit 1 set at creation, **bit 2 in use**, bits 5..7 display),
+0x24 f32 scale, +0x28 u8, +0x29 u8 icon.

**Road network (ThePaths)**: `func_268880(paths, pos*, type, ...)` = FindNodeClosestToCoors (type 0 cars);
`func_26B200(out, paths, node)` = a node's position (lane offset applied). **Node, 10 bytes**: +0 s16 x x 8, +2 s16
y x 8, +4 s8 z in metres (negative: value - 100), +5 u8 width / lane offset (x 0.0625 + 2.5), +6 s16 first link,
+8 u16 flags (`& 0xF` link count; `>> 12 & 3` road class, game-internals.md 12d; 0x100 water). **Links**: u16 each,
`& 0x3FFF` the neighbour node.

**World -> radar -> screen**: `func_114C98(radar, out, in, scale)` = TransformRealWorldPointToRadarSpace (`v = in x
scale - centre`, `/= range`, rotated by the cos / sin above: the radar is the unit circle);
`func_114758(radar, out, in)` = TransformRadarPointToScreenSpace (HUD radar: the rectangle from the HUD object
`*0x48F374`, `func_31F2D0` x, `func_31F270` width, `func_31F2E0` y, `func_31F2C0` height; the pause map: the branch
above); `func_114978` the inverse for the pause map.

**Draw order**: `0x321500` (the radar in CHud) - `func_110EB0` draws the map tiles, `func_113418` the arrow, north and
the blips. The mod hooks `func_113418`'s entry (before the original, AGENTS.md rule 2): the route first, then the
original draws the blips over it. Drawing goes through RwIm2D (`FUN_002fbe08`, game-internals.md 6f).
