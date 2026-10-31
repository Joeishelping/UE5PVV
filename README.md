# Pirates vs. Vikings — UE 5.6 Multiplayer Prototype

An asymmetric multiplayer satire about immigration enforcement on Lake Superior.
**Pirates** run boats, stay mobile and break each other out. **Vikings** (an ICE parody in horned
helmets) patrol, capture, transport and process detainees.

This is **Phase 1**: the core systems in C++, with everything tunable and hookable from Blueprint.
The aim is a playable listen-server brawl you can test in the editor with 2–8 players.

---

## What's in Phase 1

| System | Where | What it does |
|---|---|---|
| Faction selection | `APVVGameMode`, `APVVPlayerController`, `APVVPlayerState` | Auto-balances players for testing, or waits for a widget to call `RequestFaction`. Console commands: `JoinPirates` / `JoinVikings`. Spawns at faction `APVVPlayerStart`s. |
| Movement | `APVVCharacter` | Third-person walk, sprint, jump and swim. Built-in default controls, so no input assets are needed. |
| Boats | `APVVBoat` | Physics hull with pontoon buoyancy, throttle and rudder. Seat 0 is the driver. The other seats take passengers, who can still swing, or prisoners. Ramming knocks swimmers down and throws people out of other boats. A capsized boat dumps everyone and rights itself. |
| Melee | `APVVCharacter` | Server-checked swing with knockback. Hits drain **Balance**, and 0 Balance knocks you down (ragdoll, auto-recover after 8 s). Friendly hits knock back but do no damage by default. |
| Capture | `APVVCharacter` | A Viking **holds** Interact on a downed Pirate to zip-tie them, then **taps** to carry them over the shoulder. |
| Transport | `APVVBoat` | Interact with a boat while carrying to load the prisoner into a passenger seat. A Viking can lift them back out later. |
| Detention | `APVVDetentionZone` | Carry a prisoner into the orange box and Interact to process them into the cell. |
| Escape | `APVVCharacter`, `APVVDetentionZone` | Restrained players mash **Jump** to struggle free (slower while carried, and the carrier stumbles). Pirates hold Interact on a restrained teammate to cut them loose, or in the cyan door box for a **jailbreak** that frees the whole cell. |
| Match | `APVVGameState` | 10-minute clock, team scores, an announcement feed ("Bjorn hauled Abdi off to processing!"). Vikings win at once if every Pirate is detained. |
| Water | `APVVLake` | A flat placeholder lake. Boats read their water height from it. |

### Controls (defaults)

| | On foot | Driving a boat |
|---|---|---|
| Move / throttle | WASD / left stick | W/S, triggers |
| Steer | — | A/D, left stick |
| Look | Mouse / right stick | Mouse / right stick |
| Jump · **struggle** (when tied) · leave seat | Space / A | — |
| Sprint | Left Shift / L3 | — |
| Attack | Left mouse / X | — |
| Interact (tap or hold) | E / B | E / B = exit boat |

To use your own Input Actions, assign them on the character or boat Blueprint. The built-in
defaults are only created when no mapping context is assigned.

---

## Quick start (no setup)

1. Open the project. Any level works, even an empty one.
2. Next to the green **Play** button, click the **⋮** menu and set **Number of Players = 2** (or more)
   and **Net Mode = Play As Listen Server**.
3. Press **Play**. With no `PVV Lake` in the level, the game builds a greybox **test arena** high above
   the level: a lake, two shores, the Learning Center, the Viking Processing Facility with its cell,
   docks, boats and lighting. On-screen text tells each player their team, goal and controls.

When you build a real map, place a `PVV Lake`. The arena then stops spawning (you can also turn it
off with `bBuildTestArenaIfNoLake` on the GameMode).

## Getting it running (full setup)

### 1. Build
1. Install **Visual Studio 2022** with the *Game development with C++* workload. On Mac, use Xcode.
2. Right-click `PiratesVsVikings.uproject` and choose **Generate Visual Studio project files**.
3. Open `PiratesVsVikings.sln`, pick the **Development Editor** configuration, and build.
   You can also double-click the `.uproject` and accept the rebuild prompt.

> This code was written without access to an Unreal build, so it hasn't been compiled yet.
> If the first build reports errors, paste them back to me and I'll fix them.

### 2. Get a mannequin (optional but recommended)
Go to **Content Drawer → Add → Add Feature or Content Pack → Third Person**. This adds Manny/Quinn
and their Animation Blueprint. Without a mesh the game still runs: each player shows as a
labelled cylinder that tips over when knocked down.

### 3. Make the Blueprints (5 minutes)
Create a `Content/PVV` folder and add:

| Blueprint | Parent class | Set up |
|---|---|---|
| `BP_Pirate` | `PVVCharacter` | Mesh = SKM_Manny, Anim Class = ABP_Manny. Suggested tuning: Sprint 850, Swim 480, Max Balance 90. |
| `BP_Viking` | `PVVCharacter` | Mesh = SKM_Quinn (or a scaled Manny), same Anim BP. Suggested tuning: Sprint 750, Max Balance 120, Attack Damage 30. |
| `BP_PVVGameMode` | `PVVGameMode` | Pirate Character Class = BP_Pirate, Viking Character Class = BP_Viking. |
| `BP_Skiff` | `PVVBoat` | Optional: change the hull mesh. If you do, **reset the Hull scale to 1** and drag the seat and pontoon diamonds onto the new mesh. |

Then set **Project Settings → Maps & Modes → Default GameMode** to `BP_PVVGameMode`.

### 4. Build a test map
1. **File → New Level → Basic**.
2. Drag in **PVV Lake** and place it at Z = 0.
3. Add a **Physics Volume** covering the lake from Z = 0 down to the lakebed, and tick
   **Water Volume** so characters swim. Add a floor under it, such as a scaled cube lakebed at Z = -800.
4. Add land on two shores using cubes or Landscape:
   - **Pirate shore**, the Learning Center: 3–4 `PVV Player Start` actors with Faction = Pirates.
   - **Viking shore**, the processing facility: 3–4 `PVV Player Start` actors with Faction = Vikings,
     plus a `PVV Detention Zone`. Build cell walls around its `CellPoint`, and leave the cyan
     `DoorArea` reachable from outside.
5. Drop 3–4 `BP_Skiff` boats on the water near each shore.

### 5. Play multiplayer in the editor
In the dropdown next to Play, set **Number of Players = 4** and **Net Mode = Play As Listen Server**,
then press Play. Players alternate factions automatically. Type `JoinVikings` or `JoinPirates` in the
console (`~`) to switch sides.

---

## Blueprint hooks

Use these for animation, VFX, sound and HUD. No C++ needed.

- **PVVCharacter**
  - `OnAttackStarted`: play the swing montage.
  - `OnHitReceived`: hit reaction and sound.
  - `OnStateChanged`: fires when you're knocked down, tied, carried, seated or detained. Swap anim states here.
  - `OnFocusChanged`, plus `FocusPrompt`, `HoldProgress`, `StruggleProgress` and `GetBalanceFraction`: drive the HUD.
- **PVVBoat**: `OnOccupantsChanged`, `GetForwardSpeed` (engine sound pitch), `WetFraction` (spray VFX).
- **PVVDetentionZone**: `OnPrisonerProcessed` (stamp sound, paperwork confetti), `OnJailbreak` (alarm).
- **PVVGameState**: `OnAnnouncement(Message)` for the kill feed, `OnMatchEnded(Winner)`, plus `PirateScore`, `VikingScore` and `RemainingSeconds`.
- **PVVPlayerController**: `ShowFactionSelect`. Turn off `bAutoAssignFactions` on the GameMode, show
  a widget here, and have its buttons call `RequestFaction`.
- **PVVInteractable** (interface): add it to any Blueprint actor to make a new Interact target, such as
  a supply crate, a sabotage point or a hideout flag.

---

## Architecture notes

- **Server-authoritative.** Clients send intent through RPCs: attack, interact press and release,
  struggle, sprint and drive input. All outcomes are decided on the server and replicated.
- **One state machine per character:** `Condition` (Normal, KnockedDown, Restrained, Detained), plus
  separate `CarriedBy` / `CarriedCharacter` / `CurrentBoat` links. `RefreshStateEffects()` turns those
  into movement, collision, ragdoll and label changes on every machine.
- **Boats:** the driver's controller possesses the boat pawn, and the character body rides along
  attached to the hull. Exiting hands possession back. Physics runs on all machines with replicated
  inputs and is corrected by server movement replication. Expect slight input delay for client
  drivers. Phase 2 can move to Chaos physics prediction if it feels bad.
- **Interaction:** one Interact button. Each target (`IPVVInteractable`) decides whether it is valid for
  this player right now, and whether it fires on a tap or a hold. People rank above props, so a nearby
  boat never steals a rescue.

---

## Roadmap

Build these one at a time, and playtest after each.

1. **Phase 1 – Core:** faction select, movement, boats, melee, capture, carry, transport,
   detention, struggle and jailbreak. *(this commit)*
2. **Phase 2 – Feel:** animations and montages, hit-stop, sounds, HUD (prompts, balance bar, hold ring,
   kill feed, scoreboard), boat wakes and spray, faction-select widget, proximity voice or quick pings.
3. **Phase 3 – Game modes**, as `APVVGameMode` subclasses:
   - **Waterway Pursuit:** Pirates carry supply crates (a carryable interactable) between docks. Vikings intercept.
   - **Learning Center Defense:** Vikings must hold capture zones inside the HQ while Pirates defend.
   - **Prisoner Escape:** the match starts with Pirates detained. They coordinate a breakout, and Vikings contain it.
   - **Minnesota Takeover:** king-of-the-hill control points around the lake.
4. **Phase 4 – Progression:** supply resources, temporary hideouts (Pirate respawn beacons),
   surveillance tools for Vikings (drones, spotlights, boat sirens), sabotage targets.
5. **Phase 5 – World:** a stylised Lake Superior map, ambient civilian NPCs, Steam sessions for online play.

### Tone note
The comedy works best aimed at the enforcement machine: bureaucracy, paperwork, overkill gear and
Norse pomposity. The Pirate crew should be the scrappy, likeable underdogs, not ethnic caricatures.
That keeps the satire sharp and the game streamable.
