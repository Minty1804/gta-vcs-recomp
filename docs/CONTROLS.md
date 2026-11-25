# Controls

Keyboard, mouse and a gamepad (anything SDL3 knows: Xbox, DualShock / DualSense, Switch Pro, ...) all drive the
same PS2 controller (DualShock 2, analog mode), so they can be mixed. Every binding can be changed in the settings
menu (F2): the **Keyboard** and **Gamepad** pages, by action and situation - on foot, fighting, vehicle, aircraft -, as
San Andreas' Controls menu (src/game/vcs_input.cpp).

## Controls by action

The keyboard, the mouse and the gamepad press the PS2 controller's buttons through actions, as in the PC games (their
CControllerConfigManager) and San Andreas' Controls menu: each key or gamepad button does one thing on foot and another
in a vehicle or an aircraft, and the game's menus have their own keys. Each action presses the PS2 button VCS's default
controller setup gives it (the game's own help-text key names, game-internals.md 12a).

The **PS2 layout** - a key, and a gamepad button, for each PS2 button, the keyboard's default before 2026-11-05 and the
gamepad's before 2026-11-19 - was removed on 2026-11-19 at the user's request (DECISIONS.md). A player's gamepad
buttons from it carry over (below); keys they had set in it don't.

## The actions and their default keys

Default keys are Vice City's on PC, with San Andreas' Q / E for the weapons (Vice City's keypad Enter / Delete:
Keypad Enter is Enter here). Every action has two slots on the Keyboard page.

**On foot**

| Action | Keys | PS2 |
|---|---|---|
| Forward / backward / left / right (also in the scopes) | W S A D, arrow keys | Left stick |
| Walk (hold) | Alt (as in San Andreas; Alt + Enter stays full screen, so walk to a car and press F) | small stick tilt |
| Sprint / swim faster | Shift | Cross |
| Jump / climb | Space | Square |
| Manual aim (while targeting: Vic aims where the stick points; VCS has no crouch) | C | L3 |
| Fire / attack | left mouse button, Ctrl | Circle |
| Target / lock on | right mouse button, Delete | R1 |
| Next / previous weapon | E / Q, mouse wheel | R2 / L2 |
| Next / previous target | ] / [ | R2 / L2 |
| Pick up weapon (and the camera behind Vic) | Tab | L1 |
| Look behind | middle mouse button, Keypad 1 | R3 |
| Enter vehicle | F, Enter | Triangle |
| Change camera view (also in the scopes) | V, Home | Select |
| Activity (recruit, buy sites, start a site attack) | Caps Lock, Keypad + | R3 |
| Reload weapon (on foot; in a vehicle R is the radio, and the game refills the clip itself) | R | (our Reload key) |
| Sniper zoom in / out (scopes, camera, binoculars) | Page Up, Z / Page Down, X; mouse wheel | Square / Cross |
| Steady aim (sniper scope; the game's C0SNSLO) | Shift | L1 |

The wheel bound to Sniper zoom in / out zooms in steps that the scope eases (as the unbound wheel does), not as short
Square / Cross taps. Whenever the wheel's meaning changes (into or out of a scope, a vehicle, the menus), notches still
queued are dropped, so zoom notches scrolled just before leaving the scope don't come out as weapon changes.

**Fighting** (VCS's hand-to-hand combat: fists or a melee weapon - bat, machete, knife... - in hand and Target / lock on
held - locked on to a ped, or ready to). These
keys win over the on-foot ones meanwhile (Space kicks instead of jumping, Shift blocks instead of sprinting); let go of
Target and they are the on-foot keys again.

| Action | Keys | PS2 |
|---|---|---|
| Light attack | Fire's keys (left mouse button, Ctrl) | Circle |
| Heavy attack | Space, mouse button 4 | Cross |
| Block (hold) | Shift, mouse button 5 | Square |
| Grab (the grabbed ped can't defend) | F | Triangle |

Next / previous target (] / [, and Q / E: L2 / R2 change the target while locked on) work as on foot.

While fighting, Cross, Square and Triangle come only from these rows: an on-foot key whose PS2 button is one of them
(Sprint is Cross, Jump Square, Enter vehicle Triangle) does nothing until Target is let go, whatever the fighting keys
are bound to - before 2026-11-24, with the heavy attack moved off Space, Shift (Sprint) still heavy-attacked.
`VCS_FIGHT_BUTTONS=0` = before.

A melee weapon fights with the same buttons: locked on with a machete, Cross is a blow (a ped lost 39 health to three,
to a bat 32) and Triangle / Square none, as with the fists. Until 2026-11-24 only the fists counted, so with a machete
in hand Shift (Sprint, Cross) swung it and the heavy attack's own key did what it does on foot (the player's report:
"the fighting keys don't match the configuration"). `VCS_FIGHT_MELEE=0` = fists only, as before.

**Vehicle** (cars, bikes, boats - the Skimmer is a boat to the game - and the hovercraft, which drives like one)

| Action | Keys | PS2 |
|---|---|---|
| Accelerate | W, Up | Cross |
| Brake / reverse | S, Down | Square |
| Steer left / right | A / D, Left / Right | Left stick |
| Lean forward / back (bikes) | Keypad 8 / Keypad 5 | Left stick up / down |
| Handbrake | Space | R1 |
| Fire (drive-by, the fire engine's water, other vehicle weapons) | left mouse button, Ctrl | Circle |
| Secondary fire / aim (R1, so in a car also the handbrake; with Free aim on it aims from the driver's seat) | right mouse button | R1 |
| Horn / siren | Shift, H | L3 |
| Next radio station | R, Insert | L1 |
| Look left / right | Q / E | L2 / R2 |
| Look behind | middle mouse button, Keypad 1 | L2 + R2 |
| Exit vehicle | F, Enter | Triangle |
| Change camera view | V, Home | Select |
| Sub-missions (taxi, police, fire truck, ambulance) | Caps Lock, Keypad + | R3 |
| Reload weapon (drive-by: fills the clip at once) | - (none) | - |
| Forklift: forks up / down | Page Up / Page Down | D-pad up / down |

**Aircraft** (helicopters - the autogyro too - and planes; since 2026-11-19, before which they used the vehicle keys).
The PS2 buttons were measured in a Maverick and the Biplane with the gamepad injected (game-internals.md 12a); a
helicopter and a plane take the same ones.

| Action | Keys | PS2 |
|---|---|---|
| Climb (helicopters) / throttle (planes) | W, Up | Cross |
| Descend / brake | S, Down | Square |
| Turn left / right (rudder) | Q / E | L2 / R2 |
| Bank left / right | A / D, Left / Right | Left stick left / right |
| Nose down / up | Keypad 8 / Keypad 5 | Left stick up / down |
| Fire (the Hunter's rockets) | left mouse button, Ctrl | Circle |
| Secondary fire (the Hunter's cannon) | right mouse button, Space | R1 |
| Next radio station | R, Insert | L1 |
| Exit vehicle | F, Enter | Triangle |
| Change camera view | V, Home | Select |
| Sub-missions | Caps Lock, Keypad + | R3 |

The actions split from older ones on 2026-11-19 (the vehicle's and aircraft's Exit vehicle, Change camera view and
Sub-missions from the shared rows; the aircraft's from the vehicle's) take the older ones' keys from vcs.ini until they
are set (settings::Spec::alias), so a player's own keys carry over.

**Tap to exit a vehicle** (Keyboard, under Vehicle; on by default, the user's wish of 2026-11-20): one press of the exit
key gets Vic out - the vehicle brakes to a stop first, with gas and brake let go meanwhile (press one again to keep
driving). Off: the PS2's way, where a press while rolling slowly only brakes, the button must then be pressed again
once the car stands, and with the gas held he never gets out (game-internals.md 12a). The gamepad keeps the PS2's way.

**All situations** (everywhere but the menus): camera I J K L (right stick); D-pad up / down / left / right (unbound
keys).

**Menus and pause**: Pause menu Escape (fixed) and P (Start); Menu select / skip Enter, Space (Cross); Menu back
Backspace, F (Triangle); the arrow keys are the D-pad in the menus (while playing they move, as in Vice City; the
D-pad while playing has rows of its own under "All situations", unbound); W / A / S / D the left stick in the menus
(2026-11-23: the shops' text says "left analog stick, or directional buttons", and Ammu-Nation's weapons only changed
with the arrows - the on-foot W / A / S / D move only while playing). The menu keys apply while the game is paused, before
the player exists (title screen, intro movies) and while the game holds the player's controls (cutscenes, shops); then
the play keys do nothing. A key held from a menu into play (Enter choosing Resume) does nothing on the other side until
let go.

A key can do several actions as long as they never work at the same moment, as in the PC games' Controls menu: W is
Forward on foot and Accelerate in a vehicle, Z is a sniper zoom in a scope and could be Jump on foot. Binding a key
takes it from the actions that work at the same time as this one (the menu says which, in blue under the list) and
leaves it on the others. The situations: on foot, in a scope (first-person weapon views: the sniper rifles, rocket
launcher, M16, camera, binoculars - there the on-foot-only actions do nothing, as on PC), in a vehicle, in an
aircraft, in the menus.
Forward / back / left / right, Fire and Target work on foot and in scopes; Change camera view and the camera keys
everywhere but the menus. The fighting keys may share with the on-foot keys (they win while fighting). The menu keys
(F2, F5, F6, F8) never share.

## The gamepad

Since 2026-11-19 (the user's ask: the controls set by action and situation, as in San Andreas, not by the PS2 button
they press) the Gamepad page lists the same actions as the Keyboard page, in the
same sections (On foot, Fighting, Vehicle, Aircraft, All situations), each with a gamepad button (any button, LT / RT,
a stick click, the D-pad, or none):

- An action's button works in its situation only, as a key does: RT given to Accelerate accelerates in a car and is
  still the next weapon on foot.
- At the defaults every button presses its own PS2 button in every situation (A Cross, B Circle, X Square, Y Triangle,
  LB / RB L1 / R1, LT / RT L2 / R2, the stick clicks L3 / R3, Back Select, the D-pad), so the game plays as on the PS2.
  Look behind in a vehicle is both triggers together, as on the PS2.
- A button may do several actions; in one situation it does all it is set to (the dropdowns don't take a button from
  another action). The fighting actions win over the on-foot ones they share, as the keys.
- In the game's menus (paused, the title screen, cutscenes, shops) the buttons are the PS2 controller's whatever the
  actions say; Start pauses everywhere.
- The sticks are the gamepad's own (Sticks and triggers: deadzones, swap, invert). Reload weapon has no button by
  default.
- Until an action's button is set it takes the button the old PS2 layout gave its PS2 button (`pad.cross` and so on,
  still read from vcs.ini), so a player's own gamepad map carries over.

`tools/pc_scheme_check.py` checks the defaults on foot, in a car and in a helicopter, an action rebound in its
situation only, and the menus; `tools/controls_check.py` the keys in play, the mouse and the settings.

**Right mouse button = R1** (Target): on foot it locks on to the ped nearest in front while held (with fists or a weapon), as R1
does on the PS2; left mouse button (Circle) attacks / fires. With **Free aim** on (below) it aims a gun instead.

## Free aim

Keyboard > **Free aim** (off by default) and Gamepad > **Free aim** (off), each for its own device (src/game/vcs_aim.cpp,
game-internals.md 12c):

- With a gun in hand (pistols, shotguns, SMGs, rifles, the M249, minigun and flame-thrower), holding the target button
  (Target / lock on: the right mouse button; on the gamepad Target's button, RB unless changed) puts up the game's own
  white aiming circle in the middle of the screen instead of locking on to the nearest person. The mouse (or the right
  stick, 2 rad/s at full tilt, finer near the centre) turns the aim and the camera with it, 1:1 like the camera on foot
  (Mouse sensitivity, Invert mouse up / down apply; zoomed views turn slower).
- The left button (Circle) fires **through the circle**: a shot hits what is under it, whatever the gun's own line.
  Firing a gun from the hip with the left button aims too (the aim is held 0.6 s after the last shot). Letting go of
  the target button ends the aim at once (the camera leaves it 2 frames later), shots or not.
- **Moving while aiming** is what the game allows locked on with that gun - a pistol walks and strafes; no new movement
  is added. Letting go leaves the camera looking where the aim did. The camera moves into the aim in 0.1 s and out of
  it in 0.2 s, with every gun (the game blends 0.35-0.4 s, and cuts out of the aim with the guns that stand Vic still;
  its other camera changes keep their blends and cuts).
- Sprinting the game doesn't aim: with the button held the mouse turns the camera as without it, and the aim starts
  the frame Vic stops sprinting. In a jump it starts the frame his landing ends.
- When the gun runs dry while aiming the game puts the fists in Vic's hand: with the button still held nothing
  locks on until it is let go, and choosing another gun (the wheel) aims again at once.
- Fists, melee weapons, thrown weapons and the scoped guns (sniper rifles, rocket launcher, camera, binoculars) keep
  R1 as on the PS2. There is no aim assist on the gamepad.
- It is the game's own free aim (R1 then L3 on the PS2: the left stick moves an aiming point while Vic stands still),
  driven so that Vic can move and the mouse aims; `VCS_FREEAIM=0` leaves it out entirely.
- **Shooting from a car** (Jive Drive: Vic in the passenger seat while Lance drives - the game's own free aim, which the
  mission starts itself): with free aim on for either device, the mouse and the right stick turn the circle instead of
  the left stick (the game's help text still says the left stick), no button to hold; the left button fires through
  the circle. On the gamepad **R1 fires** there (as Circle, which still fires too): the right thumb is on the stick.
  Only in this car aim and Phil's shooting range (below) - R1 is the target button everywhere else. The circle keeps its direction while the car turns, up to 20 degrees above level and 5 below, as the game
  allows there. With free aim off it is the PS2's left stick.
- **Phil's shooting range** (the game's free aim forced on foot, `SET_PLAYER_FORCE_FREE_AIMING`): the same - the mouse
  and the right stick turn the circle, no button to hold, the left button fires; the right button does nothing there
  (as R1 on the PS2), and on the gamepad **R1 fires** (as Circle), as in Jive Drive's car. Up to 110 degrees either
  side of Vic's heading, 40 above level and 20 below, as the game allows.
- **A gun on a vehicle** (Farewell To Arms: Vic in the helicopter's door with an M249, the first-person view with the
  circle in the middle): the same again - the mouse and the right stick turn the view, the left button fires, on the
  gamepad R1 fires. Up to 70 degrees either side of the door, as the game allows. With free aim off the mouse still
  turns it (as in the scopes) and the gamepad's left stick does, as on the PS2.
- **Driving a car** (after ChrisM-Pek's fork): hold the target button - the **right mouse button** - with a pistol or an SMG and move the mouse to aim from the driver's seat; a
  small circle above the car shows where the shots go, and the **left mouse button** fires through it. The chase camera
  stays where the mouse puts it while you aim. W / A / S / D still drive and **Space is still the handbrake** (and the
  gamepad's R1); the aim key isn't. Letting go, the pause menu or the free camera end it. Cars only: on a motorcycle
  the game's own drive-by stays as it is. Other guns (shotguns, rifles) can't be fired from the driver's seat, as on
  the PS2. The game's lean-out, fire rate, effects and ammo are its own; only the line of the shot follows the mouse.

**In the scopes** (sniper rifles, rocket launcher, M16 view, camera, binoculars) the mouse turns the view itself, 1:1,
with Mouse camera on (zoomed in it turns slower). **Mouse wheel** (Keyboard page, on by default): on foot down is the
next weapon and up the previous one (R2 / L2, one per notch); in the sniper scope, the camera and binoculars it zooms;
in a vehicle it follows **Mouse wheel in a vehicle** (Keyboard page): **Weapons** (the default) switches between the
pistols and SMGs you have ammo for, the guns a driver fires (down: next, up: previous; on a motorcycle too, aiming or
not); **Radio** changes the station (down: next, up: previous - the game's own L1 steps forwards only: a step back is
the station written two back and L1, game-internals.md 12a; nothing happens while the radio plays an announcement, as
with L1). Aiming from the driver's seat the wheel always switches weapons. It does nothing of this while a control is
bound to the wheel, or on foot while aiming.

## Keyboard page

- Every action, both sticks' directions and **Walk** have two slots. Click a slot, then press a key, a mouse
  button (left, right, middle, 4, 5) or turn the wheel (each notch is a short press); Esc cancels, a right-click on a
  slot clears it. Shift / Ctrl / Alt bind both sides, Enter is also Keypad Enter.
- A key does one thing: binding it to a control takes it from the control that had it. The menu keys (Settings menu
  F2, Trainer F5, Free camera F6, Screenshot F8, same page) never take a control's key, and a control can't take theirs - the menu says which setting
  has it.
- **Walk (hold)**: the left stick at the **Walking speed** (45% by default) while held - GTA walks below about half
  a tilt, which keys can't do otherwise. The speed is set in the game's own stick units, so a diagonal (W + D) walks
  as fast as a straight line (before 2026-11-16 it stood still: both axes fell into the game's dead third;
  `VCS_WALK_FIX=0`: as before).
- **Reload weapon** (R; a gamepad button on the Gamepad page, none by default): reloads the gun in hand with the game's
  own reload (state, time, and its reload animation, played as the game plays it after an attack) while the clip is not
  full and there is ammo to fill it; no ammo is lost. In a vehicle the game refills the clip itself whenever you stop
  shooting, so there the key fills it at once during a drive-by. The PS2 reloads only an empty clip (game-internals.md
  12c, "Reloading"; `VCS_RELOAD_ANIM=0`: no animation).
- **On-foot camera hold** (Mouse section; 2 s by default; Off, 1 / 2 / 3 / 5 s, Always): for that long after the mouse
  turns the on-foot camera it stays where the mouse put it while Vic moves - the game's string and its swing behind
  him are off (game-internals.md 12b), so holding D no longer turns the camera against the mouse. Then the game's camera
  follows him again. Keyboard and mouse only; the right stick or the camera keys hand it back at once. `VCS_FOOTCAM=0`:
  not installed.
- **A driving key** may share its key with an on-foot control (W is Move forward on foot and Accelerate in a car); within
  the Driving section a key does one thing, and the menu keys stay out of both.
- **Mouse camera** (on by default): the mouse turns the camera, and the mouse is held in the window while playing (free
  in the menus and when the window is in the background). **On foot** it turns the camera itself, as in a PC game:
  every movement turns it by the same angle at any speed and frame rate (100%: 400 counts - half an inch at 800 DPI -
  a radian), moving the mouse forward looks up. Looking up / down goes through the game's own stick pitch, which has a
  top speed (about 1.5 rad/s at 100%); a faster movement still arrives, a few frames later. Turned toward a wall, the
  camera is pulled in toward the player, as the gamepad's is (`VCS_FOOT_WALL=0`: it stops at the wall's edge, as
  before 2026-11-11). **In the scopes and free
  aim** it turns the view itself too (above). **Behind a car, bike or boat** it turns the camera itself as well, and
  while the mouse moves the camera stays where it is put - the car neither drags it along nor pulls it back; **Car
  camera return** (1.5 s; 0.5-5 s or Never) after the mouse stops it eases back behind the vehicle (about 2 s) and the
  game's camera carries on. Never while a fire truck sprays water: its hose aims where the camera looks, so the camera
  stays where the mouse left it until the water has stopped. The gamepad's right stick (or the camera keys) takes it back at once, held or not.
  **Car camera sensitivity** (100%, 10-400%) scales its turn on top of Mouse sensitivity.
  `VCS_CARCAM=0` puts the old way back (the mouse as the right stick). **In the other vehicle
  cameras and locked on** the mouse's speed becomes the right stick's tilt (1,500 counts a second is full tilt), so very
  fast flicks there turn no faster. **Mouse sensitivity**, **Invert mouse up / down**, **Free aim** and **Mouse wheel**
  (above). (game-internals.md 12b, 12c)

## Gamepad page

- **The actions**, by situation (On foot, Fighting, Vehicle, Aircraft, All situations): a gamepad button each ("The
  gamepad" above).
- **Sticks and triggers**: deadzone per stick (12%; the range past it is stretched, so a small tilt beyond it still
  counts), **Camera speed** (the right stick's tilt scaled), **Trigger press point** (LT / RT are L2 / R2 buttons),
  **Swap sticks**, **Left stick without the square dead zone** (off: the game ignores each axis of the stick on its own
  for its first third, so a stick held a little off straight goes straight and a slight diagonal does nothing - as on
  the PS2; on: the stick goes where it points; willzmkv's, their issue 93), **Invert camera** left / right and up /
  down, **Free aim** (above; to aim with the left trigger, give Target the LT and move Previous weapon / target to
  another button).
- **Vibration**: the DualShock 2's motors on the gamepad's rumble, 0-200% (100% as on the PS2, 0% off). The game
  drives them itself (crashes, explosions, gunfire; game-internals.md 12a).

## The game's pause menu

Escape (Start) opens it - also the Save menu a safehouse opens - and it works as a PC game's menu (src/game/vcs_frontend.cpp,
game-internals.md 6m; before 2026-11-05 Start was Enter and the menu took the pad's buttons only):
- **Keyboard**: the arrow keys move, **Enter** selects (Cross), **Escape** goes back (Triangle) - on the tab bar
  (map / brief / game / stats / controls / audio / display) that closes the menu. Escape is the fixed pause key, and the
  menu's own Enter / Escape win over the menu keys while it is up (Space and Backspace / F still select / go back).
- **Mouse**: the cursor shows while the menu is up. Pointing at a tab or an item moves the game's highlight there (a tab
  shows its page, as the D-pad does); a click selects it - a tab opens its page, an item of the page under the tab bar
  opens that page and selects it (Game > Load Game in one click). The **right button** goes back (closes the menu on
  the tab bar).
  - **Sliders** (Audio > Music / SFX volume, Display > Brightness): a click on a bar sets the level, holding the button
    and moving drags it, the wheel turns it a step a notch; a click on the label only moves the highlight there.
  - **Choices** (Speaker setup, Radio station, Subtitles, Radar, HUD, Widescreen, Trails, the Controls page): a click
    turns to the next one, the wheel turns either way (up: next).
  - **Map**: drag it with the left button (it stays in the range the D-pad scrolls it); the wheel zooms (up: in, as R1).
    Cross (Enter) shows the legend, Square (Space) places the target as on the PS2.
  - **Brief, Stats**: the wheel scrolls.
- **Gamepad**: unchanged (Start opens and closes, the D-pad / left stick move, Cross selects, Triangle goes back).
- What the menu takes stays out of the game: the mouse buttons and the wheel aren't Circle / R1 / weapon changes while
  it is up, nor while still held after it closed (a click on Load Game doesn't punch); Escape held from opening it
  isn't a back. Any key or gamepad button pressed while the mouse is on its way to an item takes over.
- It all goes through the game's own pad: the menu sees D-pad steps, Cross and Triangle, so it sounds, highlights and
  switches pages as on the PS2. `VCS_MENU_MOUSE=0` leaves the mouse out (Enter / Escape stay).
  `python tools/menu_check.py` checks all of it.

## Button prompts

F2 > Game > **Button prompts** (default Original): the controls the game's help text and pause menu show.
Automatic follows the device used last - the keys and mouse buttons bound on the Keyboard page, or the gamepad's own
buttons (PlayStation or Xbox glyphs, from the pad's maker) as mapped on the Gamepad page. PlayStation, Xbox and
Keyboard & mouse fix a set; Original is the PS2's text. game-internals.md 6n, DECISIONS.md 2026-11-07.

A glyph shows the keys of the action that works where the player is (2026-11-23; before, every text that was not a
vehicle's showed the on-foot keys - Ammu-Nation's "Shift to buy", where Shift does nothing and Enter / Space buy):

| The text's action | Keys shown |
|---|---|
| The shops (AMBUY, AMEXI, AMMOV, AMLEF / AMRIG) | the menu keys: select, back, W / A / S / D, the arrows - the game holds the controls there |
| A plain `~X~` ("Press X to continue") | by the situation: the pause menu's Enter / Escape, the menu keys while the game holds the controls, a vehicle's, a scope's, on foot |
| The sniper scope (SNZI, SNZO, SNSLO) | Sniper zoom in / out, Steady aim |
| Unarmed fighting (PUNA, PUHA, PUBL, PUGR) | the Fighting section's keys |
| Sub-missions / activities (TGSUB) | Activity on foot, the vehicle's or the aircraft's Sub-missions in one (each its own key) |
| Targets while locked on (PDCTL) | Next / Previous target (not the weapons) |
| The Hunter's cannon (VEWE2) | Secondary fire (not the handbrake) |
| Change camera view (CVEIW) | the situation's Change camera view |
| Vehicle texts (VE*, FL*, HE*, TAXJU, FIREH) | the vehicle's, the aircraft's in one |
| The rest | on foot |

`prompts actions` (control channel) lists the 61 actions with their context, `prompts keys <ACTION>` the glyphs they
show now; tools/prompts_check.py checks them.

## Menus

F2 (or Back + Start on a gamepad) opens the settings, F5 (or Back + RB) the trainer (docs/TRAINER.md), F6 (or Back + LB)
turns the trainer's free camera on and off. Flying: W / A / S / D, E or Space up, Q down, mouse look, Shift faster,
Ctrl slower, Z / X zoom out / in; gamepad left stick, right stick, RT / LT, A / B, D-pad up / down (fixed keys; the
game gets no input meanwhile). In the
trainer: Up / Down or numpad 8 / 2 or the D-pad select, Left / Right or numpad 4 / 6 change a value, Enter / numpad
5 / A applies, Backspace / numpad 0 / Escape / B goes back (and closes it on the first page). The game gets no input
while a menu is open.

## Screenshots

**F8** (Keyboard > Shortcuts > Screenshot, rebindable) saves the game's picture as a PNG in a `screenshots` folder next to
vcs.exe, named `vcs_<date>_<time>.png` (`_2`, `_3`... for several in one second); a notice names the file once it is
written. The shot is the window pixel for pixel - its resolution (the monitor's in full screen), its letterboxing, the
UI where it was - and holds what is on screen: the settings or trainer menu, the F3 overlay, the frame-rate counter and
the trainer's notices, except the notices of earlier shots, so shots in a row stay clean. The PNG is written on a thread
of its own (a 2560x1440 one takes ~0.6 s to compress; the game keeps its frame rate meanwhile). It does nothing while
the settings menu waits for a key to bind. `VCS_SCREENSHOT_DIR=<dir>` saves them elsewhere (the test tools do).
(src/main.cpp `screenshotKey`; the control channel's `screenshot` stays at its fixed test size, `PS2X_CAPTURE_SIZE`)

## Window

Alt+Enter switches between full screen and the window (Display > Window mode; saved in vcs.ini). Enter with Alt
held is not Cross in the pause menu (nor a binding). Game > **In the background** decides what happens when another window is in front: keep running
(as before), mute, or pause.

## Debug keys
| Key | Effect |
|---|---|
| F3 | performance overlay: game fps and frame-time percentiles, EE thread time per phase (game / render / IOP / wait), GS and DMA counters, host frame and GPU submit times, audio buffer, and a frame-time graph; under it (beside it on a short screen) the game's state: the player's position, heading, state, health, armour, money and weapons, the vehicle (model, position, heading, health, speed), the camera (mode, position, heading, pitch, FOV), the clock, area, world level, game time, time step, pauses, pool use and the running scripts (the mission's with its IP). The same lines answer `python tools/gamectl.py gameinfo` |
| F4 | start / stop recording one CSV row per game frame to `perf_<date>_<time>.csv` (working directory); summarise with `python tools/perf_report.py <file>` |
| F12 | (hardware renderer, only with `PS2X_HW_DUMP_KEY=1`: F12 is Steam's and Bandicam's key) list the next frame's draws on stderr and dump its render targets |

## Test runs without a window
`python tools/run.py 600 test --hidden` starts the game with no window on the desktop, no sound, and the physical
keyboard / mouse / gamepad ignored (`PS2X_HIDDEN=1`). It listens on 127.0.0.1:47615 (`PS2X_CONTROL_PORT`) for
`tools/gamectl.py`: `screenshot out.png` (the picture plus the perf overlay, rendered off-screen), `press Return 150`,
`key W down` / `up` (also `mouseleft`, `mouseright`, `wheelup`, `rshift`, `kpenter`, ...), `perf`, `quit`, and from
the game layer `setting <key> [value]` (a setting read or changed; an action setting is pressed), `menu <page>` /
`menu scroll <0..1>` / `menu close` (the settings menu for screenshots), `padstate` (the last pad report the
bindings made, and the pad reads so far), `mouse <dx> <dy>` (mouse motion in counts, y down, as the mouse sends it),
`gamepad on|off`, `gamepad <a b x y lb rb back start ls rs up down left right> <0|1>`, `gamepad <lx ly rx ry lt rt>
<value>` (a gamepad set from the channel in place of the real one while on) and `aimstate` (free aim on or not, and
the shots sent down the camera line so far). `--control` (`PS2X_CONTROL=1`) opens the same channel with the window shown; `PS2X_INPUT=injected`
ignores physical input with the window shown. Injected keys go through the bindings like physical ones:
`tools/controls_check.py` checks them (also a `regress.py` step). `VCS_INPUT_MAP=0` falls back to the runtime's fixed
built-in mapping (`ps2_pad.cpp`, the old defaults).
