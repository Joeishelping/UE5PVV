# Border Run — Build It Yourself (Blueprint only, UE 5.6)

Cartoony, silly, arcade-y. Think 3–4 friends in a beat-up truck trying to sneak past the checkpoint, one of them holding a gun.
This guide rebuilds the C++ prototype in **Blueprints only** and adds the playtest feedback:
tasks, betting, nerfed cutters, traps, upgradable walls, the boombox gimmick, climbing on friends,
voice chat, digging, dogs, and **NPC trucks + Warden scanners**.

**Notation:** `A → B → C` = wire the exec pins in that order. **(Server)** = Custom Event *Run on Server*.
**(Multicast)** = *Multicast*. **(Client)** = *Run on owning Client*. **RepNotify** = variable replication *RepNotify*.

**Golden rule:** clients only ask (Server events). The server changes variables. RepNotify functions update visuals everywhere.
Test **every step** with Play ▸ ⋮ ▸ 2 players ▸ *Play As Listen Server*.

---

## 0. Project setup (day 1)

1. New project: **Games → Third Person → Blueprint**. Plugins: enable **Chaos Vehicles** (cars), **Water** (river; optional), **Online Subsystem Steam** (later).
2. **Look:** cartoony = flat colours + chunky shapes. Grab free or cheap stylised packs from **Fab** (search: *low poly desert*, *stylized western town*, *cartoon truck*).
   A cel-shade post-process material on an unbound Post Process Volume sells the look.
3. **Data:**
   - Enums: `E_Team` (None, Runner, Warden) · `E_Phase` (Prep, Invasion, Ended) · `E_Weather` (Clear, Rain, Sandstorm) · `E_Item` (Ladder, WireCutters, GasCan, Taco, Scrap, Gun, CarKey, Shovel, Boombox, TrapKit, LotteryTicket)
   - Struct `S_ItemInfo`: Name (Text), Icon (Texture2D), Price (int), Illegal (bool), MaxUses (int), Loudness (float)
   - Data Table `DT_Items` (row = S_ItemInfo, row name = item name)
   - Interface `BPI_Interactable`: `CanInteract(Interactor)→bool`, `GetHoldTime(Interactor)→float`, `GetPrompt(Interactor)→Text`, `Interact(Interactor)`
4. **Multiplayer menu (Radmin VPN):**
   - Map `MainMenu` + widget `WBP_MainMenu`: **Host** → `Open Level (by Name)` YourMap, Options `listen`.
     **Join** → `Execute Console Command` "open " + IP text box.
   - Project Settings → Maps & Modes → Game Default Map = MainMenu.

---

## 1. Core classes

| Blueprint | Parent | Holds |
|---|---|---|
| `BP_BR_GameMode` | GameModeBase | Teams, phases, catches, approval rules, NPC truck spawner |
| `BP_BR_GameState` | GameStateBase | **Replicated:** Phase, PhaseSecondsLeft, TimeOfDay, Weather, Approval, RunnersSafe, RunnersNeeded · Multicast `MC_Announce(Text)` + dispatcher |
| `BP_BR_PlayerState` | PlayerState | **Replicated:** Team, Money, Inventory (Array of `E_Item`), ItemUses (Map E_Item→int), HasCrossedLine, IsAggressor, SentenceSeconds, TasksDone |
| `BP_BR_PlayerController` | PlayerController | HUD, menus, Server requests (buy, bet, team switch) |
| `BP_BR_Character` | BP_ThirdPersonCharacter | Movement, hunger, stealth, interaction, carry/cuff, climbing friends |
| `BPC_Inventory` | Actor Component (on PlayerState) | `SR_Add`, `SR_Remove`, `HasItem`, `UseItem` (counts uses, breaks items) |

Start from **BLUEPRINT_GUIDE.md Steps 1–5**: teams, spawning, punching, knockdown, zip-ties, carrying, the interact system. They carry over 1:1.
Rename Pirates → Runner and Vikings → Warden.

---

## 2. Phases, the Line and win check (GameMode)

- BeginPlay → GS Phase = Prep, PhaseSecondsLeft = **PrepSeconds** (600) → **Set Timer by Event** 1 s looping → `TickClock`.
- `TickClock`: TimeOfDay += 24 / (DayLengthMinutes × 60) · PhaseSecondsLeft −1 · at 0 → `AdvancePhase`.
- `AdvancePhase`: Prep → Invasion (RunnersNeeded = ceil(Runners / 2), Announce "THE LINE IS OPEN") · Invasion → Ended (Wardens win).
- **Line:** `BP_LineBarrier` with a Box. **OnRep_Phase** in GameState → Get Actor of Class LineBarrier → Set Collision Enabled (Prep: Query and Physics / Invasion: No Collision).
- **Crossing / safety:** in TickClock, For Each Runner character: if X > Line X → PS.HasCrossedLine = true. If X > Safe Zone X and not counted → RunnersSafe +1 → if ≥ RunnersNeeded → Runners win.

---

## 3. Money, jobs and shops

`BP_Station` (Actor, Replicates, **Always Relevant**), implements BPI_Interactable. Instance-editable: Kind (Buy / Sell / Job / Bet), Item, Price, HoldTime, Cooldown, NightOnly.
- **Buy:** Server: Money ≥ Price AND a free slot → Money −= Price → Inventory SR_Add.
- **Job:** HoldTime > 0. On Interact → Money += Price → Set `NextUse` = GameTime + Cooldown.
- Place stations in the town: taco grill, shop register, scrap piles, gas pump, marina, black market (NightOnly).

### 3a. Among Us–style tasks (brother's idea)
Each Runner gets **3 random tasks** at Prep start. Completing tasks pays money and unlocks a team bonus.
- `DT_Tasks` rows: TaskName, StationTag, Minigame widget class, Reward $.
- GameMode on Prep start → for each Runner → pick 3 random rows → PS `Tasks` (Array, replicated to owner).
- Task stations (`BP_TaskStation`, tagged) → Interact opens a **minigame widget** on the client (Client event). Widget finished → `SR_CompleteTask(TaskName)` → server checks the player is near the station → pay.
- **Minigame ideas (all UMG):** wire-matching (fix the car's wiring), timing bar (flip tacos), keypad code (shop safe), drag-and-drop crates, "keep the needle in the green" (fuel pump).
- **Team bonus:** when **all** Runners finish their tasks → Announce "Runners found a smuggler's map!" → reveal one weak fence section or a tunnel start on the minimap.
- **Wardens' sabotage (optional):** Wardens get one *Inspection* per Prep. Pick a town station and it's locked for 60 s.

### 3b. Betting: Chicken Races (brother's "win it all or lose it all")
`BP_ChickenRace` arena in town: a short oval track with 4 cartoon chickens (simple skeletal or static meshes moving along a **Spline**).
- Betting window (30 s): `WBP_Bet` shows chickens 1–4, odds, and an **ALL IN** button. `SR_PlaceBet(Chicken, Amount)` → server stores (Player, Chicken, Amount) and takes the money.
- Race: each chicken's speed = random 0.8–1.2, re-rolled every second (Timeline or Tick moving along the spline). The first to reach the end wins.
- Payout: winners get Amount × 4 (×3 on the favourite). **All in** pays ×5 but is all-or-nothing. Multicast a funny "WINNER: Sir Clucksalot" banner.
- Run a race every 3 minutes during Prep. It's a money sink and a great stream moment.

---

## 4. Inventory with item durability (nerfing wire cutters)

- PS `ItemUses` (Map). When an item is used: uses −1 → at 0, remove the item and tell the player "Your wire cutters snapped!".
- **Wire cutter nerf** (pick 2–3):
  1. Cutting takes **8 s** hold (was 4), and you **can't sprint** for 10 s after.
  2. **2 uses** per pair, then they break.
  3. Each cut sends a **NOISE ping** to Wardens *and* sets off any **alarm sensor** within 15 m.
  4. A cut fence leaves a **narrow gap** (crouch-only), and a Warden can **patch** it by holding E for 5 s.
  5. Reinforced fence sections (see 6b) need **2 cuts**.

---

## 5. Hunger, stealth and day/night

- **Hunger** (on character, replicated): Tick (Authority) → Hunger −= Rate × Delta (×1.8 sprinting, ×2 swimming). <25 → no sprint. 0 → faint (Knockdown) every 45 s. **F** = eat taco (Server).
- **Visibility** (replicated, updated by a 0.25 s timer): base 1 (day) / 0.3 (night) × 0.5 crouch × 1.5 sprint × 0.5 sandstorm × 1.3 holding a ladder.
- **Hiding on Warden screens:** on each Runner, Tick → if the *local* player (Get Player Controller 0 → PlayerState Team) is a Warden AND distance > 800 + Visibility × 7000 AND not Tagged → **Mesh → Set Visibility(false, propagate)**. Component visibility is local, so it only affects that screen.
- **Day/night:** a `BP_Sky` actor reads GameState TimeOfDay → rotates the Directional Light (pitch = −(Time − 6) / 24 × 360) and fades the moon light.
  Or buy **Ultra Dynamic Sky** on Fab: it has a Time of Day variable you can set from the GameState.

---

## 6. The border: traps, walls, fences

### 6a. Traps as Blueprints (`BP_Trap_*`, parent `BP_TrapBase`)
Base: Replicates, a Box trigger, `Armed` (RepNotify), `Owner Team`. **OnComponentBeginOverlap** (Authority) → if a Runner → `Trigger(Victim)` → Armed = false → re-arm timer.
| Trap | Effect |
|---|---|
| **Bear trap** (cartoon) | Victim can't move for 6 s (Disable Movement → timer → Set Walking), alarm ping |
| **Tripwire flare** | Lights up the area for 15 s (spawn a point light) + NOISE ping |
| **Sticky glue pad** | Max Walk Speed = 80 for 8 s |
| **Spring board** | Launch Character backwards (0, 0, 1200) — funny, sends them back over the fence |
| **Motion sensor** | No effect on the victim; pings Wardens with a red marker for 5 s |

Wardens get **TrapKits** for free at HQ: E on the ground → spawn the trap (Server). Max 6 traps per Warden team.
Runners can **spot** traps with a cheap *Metal Detector* item (shows trap outlines within 10 m on their screen: Set Render Custom Depth + outline material).

### 6b. Upgradable walls and fences (brother's "reinforce more walls x fence")
Each `BP_FenceSegment` and `BP_WallSegment` has `Level` (RepNotify, 1–3) and `Health`.
- Wardens spend **Budget** (a GameState team value that comes from Approval and catches) at segments: hold E → Level +1.
  Level 2 = needs 2 cuts / ladder slips once; Level 3 = electrified (cutting it knocks you down unless you're wearing *Rubber Gloves*).
- **OnRep_Level** swaps the mesh (wood → chain-link → razor + sparks VFX).
- Runners' counters: ladders (wall), cutters (fence), **digging under** (Section 8), **climbing on friends** (Section 7), boats around the end.

---

## 7. Climbing on your friends (mount / human ladder)

Piggy-back: one Runner stands on another, then jumps off their head for extra height.
1. Interact on a **teammate** who is Normal and **crouched** → prompt "Climb on" (hold 0.5 s).
2. Server: Climber → **Attach Actor to Component** (Base's mesh, socket `head`, or Capsule + offset 0, 0, 170) → Climber movement mode **None** → Climber RepNotify `StandingOn` = Base.
3. While mounted: the Base can walk **slowly** (speed × 0.5), carrying the Climber. Up to **3 high** is the fun limit.
4. Climber presses **Jump** → Server: Detach → Launch Character (forward × 300, Z = 900). Clears a 4 m wall or a fence.
5. If the Base is tased or punched → everyone on top falls off (Detach + Launch random sideways). Comedy guaranteed.
Use this as the "no ladder" way over **low walls and fences**, but **not** the big Wall (too high — keep ladders and digging valuable).

---

## 8. Digging tunnels

- Item **Shovel** ($40). In the Invasion phase, a Runner at a `BP_DigSpot` (soft sand, marked) holds E → `TunnelProgress` (replicated, 0–100) on the dig spot rises **+1 per second per digger** — so more friends = faster.
- Digging makes a **NOISE ping every 15 s**. Wardens can **fill in** the hole (hold E 5 s → progress −40).
- At 100 → spawn `BP_TunnelExit` on the far side of the Wall (a fixed linked actor). Interact on the hole → hold 3 s → Teleport to the exit. The tunnel stays until a Warden collapses it (hold E 10 s at either end).
- Optional: have Runners carry **dirt bags** away (item) — leaving dirt piles near the hole gives it away to drones.

---

## 9. Dogs (Warden AI)

`BP_K9` (Character, AI Controller `AIC_K9`) with a **Behavior Tree**:
1. **Follow handler** (default): MoveTo the Warden who owns the dog, 3 m away.
2. **Sniff:** every 1 s, Sphere Overlap Actors 15 m for Runner characters. Weight each by `1 − Visibility` (dogs *smell*, so they ignore darkness) and taco smell (holding food = ×2 range).
3. **Chase** the best target → on reach → **Bark** (Multicast sound + NOISE ping) and **grab the leg**: target speed × 0.3 for 4 s.
4. **Distract:** if a Taco is thrown (Runner item: Throw → spawn `BP_ThrownTaco`), the dog switches target to the taco for 8 s and eats it.
The handler whistles (key **G**, Server) → the dog returns. One dog per 2 Wardens.

---

## 10. NPC trucks and Warden scanners (the checkpoint)

The road crosses the border at a **checkpoint gate**. NPC traffic is the perfect cover.
- **`BP_NPCTruck`** (Chaos Vehicle or simple spline mover): spawned by the GameMode every 40–90 s in town. It follows `BP_RoadSpline` → stops at the **checkpoint** → waits until a Warden waves it through (Interact "Wave through") or 60 s pass → continues to the far side.
- **NPC passengers:** 1–4 `BP_NPC` (same mesh and customization options as players, random outfits) **seated** in the truck. They look exactly like players.
- **Runners can hitch a ride:** while a truck is parked in town (10 s), Interact → *Hide in truck* → take a free seat (attach to a seat socket, same as the boat seats). Hide in the back crouched → not visible from outside (hide the mesh unless scanned).
- **Warden Scanner** (free Warden tool, key **R**): hold on a vehicle for 3 s → Server counts seated actors that are `BP_BR_Character` (players) vs `BP_NPC` → shows "**SCAN: 3 people, 1 HEAT SIGNATURE ANOMALY**" (players read as anomalies).
  Upgrades: Level 1 says *how many* players, Level 2 highlights *which seat*.
- **Searching:** "Open the doors" (hold E) → kicks everyone out of the truck. Kicking out only NPCs = **Approval −5** ("Harassing citizens!").
  The Runners hiding inside can **bail and run** — chaos.
- **Fake-outs:** a Runner can **pay an NPC driver** ($50) to drive *around* the checkpoint on a dirt road, at risk of a drone spotting it.

---

## 11. The boombox gimmick (pacify Wardens)

Item **Boombox** (rare, $120, black market or a task reward). Use it (key **T**) → Server: spawn `BP_Boombox` at your feet → Multicast music (Audio component with attenuation 15 m).
- Every Warden within 12 m for **8 s**: Disable input (Set Ignore Move Input + Ignore Look Input on their controller, via a Client event) and play a **dance montage**. A cartoon disco light spins.
- Wardens can **smash it** (punch) after the first 3 s. Cooldown 90 s, one charge per buy.
- Variations for the shop: *Air horn* (Wardens stumble), *Piñata decoy* (drones lock onto it), *Fake mustache disguise*.

---

## 11b. Truck radio (switch stations while driving)

Everyone in or near the truck hears the same song, and the driver changes stations.

**Assets:**
- 3–6 music tracks (Sound Waves) set to **Looping ✓**, plus a short "static/tune" crackle sound.
- A Sound Attenuation asset `SA_Radio` (Inner 300, Falloff 1500) so people outside hear it faintly as the truck passes.
- **Use music you have the rights to** (royalty-free libraries, Fab audio packs, or your own). Copyrighted songs get streams muted or DMCA'd.

**Data:** Struct `S_RadioStation` (Name: Text, Track: Sound Base). Make an array variable `Stations` on the truck,
e.g. "KDST 101.5 Desert Hits", "Polka Loco", "News: Wardens Budget Talks", "Static".

**On the truck Blueprint:**
1. Add an **Audio** component `RadioAudio`, attached to the truck body, Auto Activate ✗, Attenuation = SA_Radio.
2. Variables (replicated):
   - `StationIndex` (int, **RepNotify**, −1 = off)
   - `StationStartTime` (float, Replicated): the server time when the song started, so late joiners hear the same part of the song.
3. **Input** (driver only, in the vehicle's input): **R** = next station, **Shift+R** = previous, **Backspace** = radio off.
   → `SR_ChangeStation(Delta)` (Server): `StationIndex = (StationIndex + Delta) mod Stations Length` →
   `StationStartTime = Get Game State → Get Server World Time Seconds`.
4. **OnRep_StationIndex** (runs on every machine, and on the server too):
   - Play Sound 2D of the static crackle (local only, short), or play it at the truck location.
   - If StationIndex == −1 → RadioAudio **Stop**.
   - Else → RadioAudio **Set Sound** (Stations[StationIndex].Track) →
     **Play** (Start Time = (ServerWorldTime − StationStartTime) **mod** track Duration). That keeps everyone in sync.
   - Show a little toast for the driver and passengers: "📻 KDST 101.5 — Desert Hits".
5. **Truck starts with a random station** on BeginPlay (Authority). NPC trucks too, so you can **hear** traffic coming.

**Gameplay hooks (the fun part):**
- **Loud = visible:** while the radio is on, the truck sends a soft NOISE ping to Wardens every 20 s. Sneaking means **radio off**.
- **"Breaking news" station:** the radio reads the kill feed or Approval changes as fake news headlines (Text-to-speech, or a few recorded lines).
- **NPC trucks always play music.** A Runner truck with the radio off at the checkpoint looks suspicious, so leave it on and blend in.
- **Boombox synergy:** the truck radio can play the Boombox track (Section 11) for a moving dance party. Wardens near the truck dance; 60 s cooldown.

---

## 12. Voice chat (proximity VOIP)

Easiest working route: **Steam** (free test App ID 480).
1. Plugins: **Online Subsystem Steam** ✓.
2. `Config/DefaultEngine.ini` (Notepad):
```ini
[/Script/Engine.GameEngine]
+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="OnlineSubsystemSteam.SteamNetDriver",DriverClassNameFallback="OnlineSubsystemUtils.IpNetDriver")
[OnlineSubsystem]
DefaultPlatformService=Steam
bHasVoiceEnabled=true
[OnlineSubsystemSteam]
bEnabled=true
SteamDevAppId=480
bInitServerOnClient=true
[/Script/OnlineSubsystemSteam.SteamNetDriver]
NetConnectionClassName="OnlineSubsystemSteam.SteamNetConnection"
[Voice]
bEnabled=true
```
3. `Config/DefaultGame.ini`:
```ini
[/Script/Engine.GameSession]
bRequiresPushToTalk=false
```
4. Character **Event BeginPlay** (and OnRep_PlayerState): if PlayerState is valid → **Add VOIP Talker Component** → **Register with Player State** →
   **Set Settings**: Attenuation = `SA_Voice` (a Sound Attenuation asset: Inner 500, Falloff 2500), Component To Attach To = Mesh.
5. **Warden radios:** Wardens hear each other everywhere. Use a second talker without attenuation, or simply let them use a Discord channel. Runners can **steal a radio** item to listen in.
6. Host and Join with the **Create Session / Find Sessions / Join Session** nodes, or get the free **Advanced Sessions** plugin for friend invites.
   Steam doesn't work in PIE. Test with **Standalone** or a packaged build, with Steam running.

---

## 13. Holding pen and catapult

- `BP_HoldingPen`: drop-off box + cell. Drop-off → teleport the prisoner into the cell → PS SentenceSeconds = 300 (+60 per repeat).
- GameMode TickClock → SentenceSeconds −1 → 0 → `BP_Catapult.Launch(Prisoner)`:
  Teleport to the basket → **Suggest Projectile Velocity (Custom Arc)** to the town landing point (Arc 0.5) → **Launch Character** (override XY and Z).
  Multicast: arm swing (Timeline rotating the arm 0 → 110°) + "YEEEET" sound. No fall damage exists unless you add it, so they land laughing.
- Jailbreak: Runners hold E 5 s at the cell door → everyone inside is teleported to the release point.

---

## 14. Approval and the "don't shoot first" rule

GameState `Approval` (0–100). Events:
| Event | Change |
|---|---|
| Shooting a Runner who hasn't crossed and isn't an aggressor | **−35**, SCANDAL headline |
| Arresting someone who never crossed | −10 |
| Searching an NPC-only truck | −5 |
| Catching a Runner who crossed | +3 |
Below 60 → drones disabled · below 30 → guns disabled (taser only) · below 10 → Runners get a free "Public Sympathy" Line extension (+2 min).

---

## 15. Suggested build order (≈ 2–3 months part-time)

1. Menu + host/join · teams · phases + Line (Sections 0–2)
2. Interact, punching, zip-ties, carrying, holding pen, catapult (old guide 3–5, Section 13)
3. Money, stations, inventory, durability (3–4)
4. Fence, wall, ladders, **climbing friends** (6–7)
5. Hunger, stealth, day/night (5)
6. **NPC trucks + scanner** (10). This is the signature mechanic, so playtest it early.
7. Tasks + chicken races (3a–3b)
8. Traps, upgrades, digging, dogs (6, 8, 9)
9. Boombox + gadgets (11), voice (12), polish

**Playtest rule:** after every step, play one match with your brother and write down the funniest moment and the most annoying moment.
Build more of the first and fix the second.
