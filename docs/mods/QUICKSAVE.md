# Quick save on a key

Status: **works** (`src/game/vcs_quicksave.cpp`; olaftnt's upstream PR #184, checked and extended here).

(Translated from the Polish original and shortened on 2026-11-23; the addresses and test results are unchanged.)

## Settings

- F2 > Game > Map and saving > **Quick save**: Off (default) / Enabled / Enabled + prompt after mission;
  `VCS_QUICKSAVE=0/1/2`, `VCS_QUICKSAVE_HOOKS=0` installs no hook.
- The key: F2 > Keyboard > Shortcuts > **Quick save**, **F12** by default (the game does not use it; F12 is the
  screenshot key in Steam and Bandicam, hence rebindable).

## What it does

- The key opens the **game's own Save menu** (a slot or Cancel) anywhere outside a mission, on foot or in a vehicle.
  It is a normal save: the same slots and file format; loaded from Load Game or at start, and a later safehouse save
  is a normal one again.
- After loading you are **where you saved**: the same heading, inside a safehouse or an Empire building if you were,
  with the same vehicle (you sit in it, or it is parked where you left it) and your wanted level. The clock does not
  move (a safehouse save adds 6 h).
- **Enabled + prompt after mission**: the same menu also opens by itself after a mission is passed. A hook on the
  `REGISTER_MISSION_PASSED` handler (opcode 0x01EB, `FUN_00184D18`) marks the pass; the menu opens when "Mission
  passed" has been up 5 s, the mission script ended 1.5 s ago, the camera is on the player and the game is not
  saving. Not when the next mission starts first or a game is loaded. Test: the `quicksave prompt` command.

**When the key does nothing** (a message on screen): the option is off ("Quick save is off (Settings > Game > Map and
saving)"); a mission runs (u8 `0x4CD3E7`; side jobs - Paramedic, Vigilante, fire truck, taxi, races - run as missions
too); a cutscene or script camera (`TheCamera+0x888` != 1), the player dead, the game already saving; **an interior
the game cannot put you back into** (area != 0 and neither a safehouse, G171, nor an Empire building, G172 - a shop);
the settings or trainer menu open, or the game paused.

## Saving

1. **The safehouse save subroutine**: the main script at IP **0xC646** (every save point CALLs it, 0x1222B). The mod
   starts it as a new script in the game's script VM (`vcs::rig::startScriptAt`, returning on `TERMINATE_THIS_SCRIPT`
   at 0x16E90) with **L0 = 1**, so it fades the screen back in itself (0xC71B). It takes the controls, sets **G4 =
   G684 = 1** (the flags by which `INITSAV` knows a safehouse save after loading), remembers the start point (G678..680
   -> G174..176), the weapons (0xC4C3) and money, fades out, calls `ACTIVATE_SAVE_MENU`, waits for
   `HAS_SAVE_GAME_FINISHED`, restores the camera, fades in and clears the flags. `ACTIVATE_SAVE_MENU` alone left G4 =
   0, so such a save loaded as a new game.
2. **In a vehicle**: the game's `ACTIVATE_SAVE_MENU` handler (`FUN_002D38C8`) does nothing there (`FUN_00219D50`). For
   the mod's script it is replaced and does what the game does on foot: `0x48A65C = 1` (the menu request) and the
   counter `0x4CD480 + 1`. The car is stopped first (`SET_CAR_FORWARD_SPEED 0`): the subroutine takes the controls.
3. **The clock**: right before writing the slot the card manager (0x275EC4) calls `FUN_003ADF68` -> `FUN_0024D5B8(0x168)`
   = +360 minutes ("sleep"). During a quick save a hook on `FUN_0024D5B8` skips the call with 360; 720 minutes after
   death or arrest (0x24C778, 0x24CB7C) stay.
4. The quick save's data is written just before the save and the game's own values are put back right after it.

## Where the quick save's data lives

**A save keeps only global script variables, and not all of them.** The loader of the script block (`FUN_0025A348`,
from `FUN_003ADAE0`) copies the globals word by word but restores a word only if its offset is on MAIN.SCM segment 2's
list (1092 u16 offsets, searched by `FUN_00255188` - game-internals.md 11a / 11b once called it a model table); the
rest are **zeroed**. Running scripts are not saved: each starts anew, hence `INITSAV` and G4. (Checked: data in a
script's locals and in LAN_B1's variables, G6346.., was in the file and gone after loading; G1106 and G678 survived.)

- **The player's place: G678..680 (the start point) and G177 (the heading)** - where the game keeps the safehouse's
  start point. After loading, `INITSAV` (main script 0x2ADF7) puts the player there itself: G171 != -1 into the
  safehouse interior (0x20C69); G172 != -1 into the Empire building (0x14750, 0x126D0); otherwise outdoors
  (`LOAD_SCENE`, `SET_CHAR_COORDINATES`, heading G177). During the save G678..680 / G177 = the current place and, when
  outdoors, G171 = G172 = -1; the game's values go back after it. Only the save and `INITSAV` read them.
- **The rest: G1106, one word** - of the 1092 restored variables the only one no script uses (MAIN.SCM disassembly:
  references and sized arrays) and 0 in every save looked at:

  | bits | meaning |
  |---|---|
  | 31..28 | 0xB: a quick save (0xA: the first quick saves, without data in G177's fraction) |
  | 27..26 | vehicle: 0 none, 1 the player sits in it, 2 parked nearby (the last one driven, still there, <= 100 m) |
  | 25..17 | the vehicle's model |
  | 16..9 / 8..1 | the parked vehicle's place from the player, signed, 0.8 m units (+-100 m) |

- **G177's fraction**: the heading is saved to the whole degree and its fraction (15 bits, value / 32768, exact below
  512) carries: bits 14..12 the wanted stars (ped +0x920, 0..6; restored with `ALTER_WANTED_LEVEL_NO_DROP`), 11..7 the
  parked vehicle's height from the player (0.8 m units, 16 = the same, 0 = outside +-12 m), 6..0 its heading (360° /
  128).

After loading, once `INITSAV` is done (G4 = G684 = 0), the mod reads G1106, clears it, outdoors places the player
again (`LOAD_SCENE`, `SET_CHAR_COORDINATES`: at a cold start `INITSAV` placed him before the ground had loaded and he
fell through), gives the stars back and recreates the vehicle (`REQUEST_MODEL`, `CREATE_CAR`, `WARP_CHAR_INTO_CAR`,
`MARK_CAR_AS_NO_LONGER_NEEDED`).

**The vehicle's height**: `CREATE_CAR` raises z by the model's height above its centre (a car ~1 m, a bike 0.6 m), so z
must be the ground. G680 after loading is no longer the saved height (in a car 1.8 m lower: a car made there landed
under the road), so heights are taken from where the player stands after loading (his centre 1 m above the ground): in
the car, `CREATE_CAR` at the player's z - 1; parked, at z = -100 (the highest ground there), and when that differs by
0.6 m or more from the saved height (a bridge over the car, a roof, ground not loaded yet) the car gets the saved
height. **Not kept**: the vehicle's colours and damage (no free room in the save) - it comes back as the same model in
a colour the game picks. The last vehicle: the ped's fields (+0x450 / +0x480 / +0x484) clear on getting out, so the mod
remembers the player's last car and at saving checks it is still in the pool (entry in use, same model, <= 100 m).

## Safehouses and Empire buildings

Empire buildings have interiors with a save disk, as safehouses do: the save point's script enters the building
(0x12676..: `CALL 0x11131(1)`, G2301, **G172 = the building 0..29**) and on leaving sets `CALL 0x11131(0)`, G172 = -1
(0x127A3..); their data is in GA1583 / 1613 / 1643 (position), GA263, GA323, GA353, GA413 (0x14750); after loading
with G172 != -1 `INITSAV` calls 0x14750 (G172), places the player in GA1583..[G172] and calls 0x126D0 (G172, 0).
Safehouses do the same with G171 (0..3: the barracks, Viceport, (-1073, 353), (255, -177); interiors 22 + i). A quick
save inside keeps G171 / G172, so loading takes the game's own path, with the exact position from G678..680.

Saves from the earlier "autosave after a mission" (made where the mission ended): when loading with G171 = G172 = -1,
G1106 not a quick save and G678..679 one of the 4 safehouses' start points (GA3305 / 3309), the mod enters it (0x20C69
with L0 = i). There is no such fix for Empire buildings (their start point, G2293.. from the disk object 0x119B0,
matches no table): such an old save loads on the street; enter and save again.

## Checked

Hidden games, `tools/quicksave_check.py` (port 47701, the card copy `build/rig/saves/user`), 13 / 13: setting and
hooks; in the barracks (G171 = 0, area 2) -> slot 2; on foot in Vice Point (470, 900) -> slot 3 (clock 16:09 ->
16:15, no +6 h); in a car at 15 m/s -> slot 4 (car stopped, menu); on foot 12 m from the car just left -> slot 5
("parked nearby"); loading slot 3 exact (470.0, 900.0, 13.3), heading, area 0, no clock jump; slot 4 in the car
(model 211); slot 5 on foot with the car within 3 m; slot 2 in the barracks; then a normal safehouse save (0x12213,
`quicksave savepoint`) -> slot 6 ok; the key during mission 44 refused; a start with the quick save newest loads it in
place, with a normal save newest the barracks as always. Note: after loading from the menu ScriptSpace moves, so read
variables through the pointer `0x48741C`.

Not checked: an Empire building (no card with one bought - the same `INITSAV` path as safehouses); safehouses 2 and 3,
a bike, boat, helicopter or plane (a vehicle saved in the air is recreated in the air and falls); the gamepad (the key
is keyboard only).
