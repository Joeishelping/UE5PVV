# BORDER RUN — Game Design (UE 5.6.1, Blueprint only)

> Working title. Team vs. team, arcade-y, sneaky, comedic. Built to be played with friends and streamed.
> Tone note: keep both teams **fictional** (e.g. your Pirates vs. Vikings style, or made-up factions)
> and aim the satire at the **system** (bureaucracy, overkill gear, paperwork), not at real nationalities
> or ethnic groups. That keeps it edgy and funny, and it keeps streamers able to play it.

---

## 1. The match in one paragraph

**Runners** (attackers) start in a dusty border town with almost nothing. During the **Prep phase**
they work jobs, cook and sell food, buy (or illegally buy) gear, fuel up a beat-up car or buy a boat,
and plan a route. When the timer hits zero, the **Invasion phase** starts. Runners try to get across:
spike walls, the big wall, a dangerous river, cameras, floodlights and the **Wardens**. Wardens are fewer
but have free cars, drones and guns. Wardens must **catch, not kill**. Caught Runners go to the
**holding pen** for a few minutes and are then **launched back over the wall by catapult** (no damage, maximum comedy).

---

## 2. Teams and sizes

| Players | Split | Notes |
|---|---|---|
| 2 | 1 v 1 | Testing with your brother |
| 3–4 | 2 v 1, 3 v 1 | |
| 6 | **4 v 2** | Sweet spot |
| 8 | **6 v 2**, 5 v 3 | Best for streams |
| 12 | 8 v 4 | |

Rule of thumb: **Wardens ≈ 1/3 of players.** They're outnumbered but have the tools.
Make it a GameMode variable (`WardenRatio = 0.34`) so you can tweak it.

---

## 3. Phases and timers (all variables on the GameMode)

| Phase | Default | What happens |
|---|---|---|
| **Prep** | 30:00 (try 12–15 for streams) | Runners earn, buy, plan. Wardens set up: drones, patrol routes, extra cameras. Nobody can cross the **Line** yet. Crossing early = invisible wall or electric zap. |
| **Invasion** | 20:00 | Runners try to reach the **Safe Zone** (a city, a bus station, a taco stand). |
| **Overtime** | +2:00 | If a Runner is mid-climb or mid-river when time runs out. |

**Win conditions** (pick one per mode):
- **Classic:** Runners win if **half** of them (rounded up) reach the Safe Zone before time is up. Otherwise Wardens win.
- **Points:** each Runner who makes it = 3 pts; each catch = 1 pt for Wardens; highest total wins.
- **Smuggle:** Runners must deliver X crates of contraband (tacos? rubber chickens?) to the Safe Zone.

---

## 4. Runner systems

### 4.1 Money and jobs ("work a little bit")
Every job is a short, silly minigame. The money is replicated on the PlayerState.

| Job | How it works | Pays |
|---|---|---|
| **Taco cart** | Grab ingredients → press keys in order to cook → serve to NPC customers before they leave | $$ |
| **Tool shop clerk** | Restock shelves; sell ladders and cutters to NPCs (or teammates, with a markup) | $ |
| **Car wash** | Scrub dirty cars (hold-mouse "scrub" minigame) | $ |
| **Scrap collecting** | Find junk around town and sell it at the scrapyard | $ (random) |
| **Street performing** | Emote near NPCs; tips scale with how many players join in | $ (funny) |

Players can **give money to each other**, so a team can pool for a boat.

### 4.2 Shops
| Shop | Sells |
|---|---|
| **Tool shop + gas station** | Ladder, wire cutters, shovel, rope, duct tape, flashlight, **gas can**, binoculars |
| **Grocery / taco stand** | Food, water |
| **Marina** | Inflatable raft (cheap, can pop), motorboat (needs gas), life vest |
| **Back alley dealer** (opens at night) | **Illegal guns** (stun or real), smoke bombs, fake Warden uniform, cardboard box, jammer (kills drones nearby) |

### 4.3 Inventory
- Grid or slot inventory (start with **6 slots** plus 1 **big item** slot for ladders and boats-on-a-trailer).
- Items are a **Data Table** row: Name, Icon, Weight, Stackable, MaxStack, Price, UseAction, IsLoud, IsIllegal.
- Carrying weight slows you down. A ladder = slow and obvious.
- **Illegal items** show up on Warden scanners and checkpoints.

### 4.4 Hunger and thirst
- Bars drain over time, faster when sprinting, in the heat (day), or swimming.
- Low hunger → no sprint. Empty → you slowly lose stamina and **pass out** (ragdoll, easy catch).
- Food can also be **thrown to distract dogs**.

### 4.5 Stealth
A **Visibility** value (0–1) on each Runner, replicated so Warden UI and AI can read it:
- Higher when sprinting, in floodlights, during the **day**, near cameras, or carrying a ladder.
- Lower when crouching, in bushes, at **night**, in a **sandstorm**, or in rain.
- A **Noise** pulse from wire cutters, car engines, gunshots or falls shows a short ping on the Warden minimap.
- **Footprints in sand** stay visible for 60 s. Wardens can track them, and rain erases them.
- **Cardboard box** item: stand still and you're "a box" (classic joke; Wardens can kick it).

### 4.6 Vehicles
- **Team car:** auto-assigned to one random Runner, who holds the **key** (an inventory item, so it can be passed or stolen!).
  It starts with an **empty tank**. Buy gas at the tool shop.
- **Fuel** is a float on the vehicle. Gas can = +40%. Empty = it rolls to a stop.
- **Boats** for the river: raft (cheap, 2 seats, pops if shot) or motorboat (fast, loud, needs gas).

### 4.7 Crossing tools
| Obstacle | Counter |
|---|---|
| **Big wall** | Ladder (climb takes 4 s, loud, ladder stays there as evidence) · rope + grappling hook · dig a **tunnel** (takes a long time, team effort) |
| **Spike walls / razor wire** | Wire cutters (loud) · carpet thrown over the top · go around |
| **River** | Swim (strong current pushes you downstream, eats hunger) · raft · boat |
| **Cameras** | Smash (alert!) · spray paint (silent, a Warden can wipe it) · jammer |
| **Motion sensors** | Crawl slowly · throw food or a rock to trigger them elsewhere |
| **Floodlights** | Cut the generator's power (a Runner objective) |

---

## 5. Warden systems

- **Free stuff:** trucks and ATVs (always fuelled), **drones**, guns, tasers, handcuffs, spotlights and radios. Unlimited, but there are fewer of them.
- **Drones:** switch into a drone camera (Possess a drone pawn, while your body stands still and is **vulnerable**).
  Battery about 90 s, then recharge. Night-vision toggle. It can **tag** a Runner, who then glows for 10 s.
- **Watchtower** with a big spotlight one player can control.
- **Dogs** (simple AI): follow footprints and food smell.
- **Catch flow:** taser, tackle or net → **handcuff** (hold E) → carry, or drive to the **holding pen** → wait 5–10 min → **catapult**.

### 5.1 The rules of force (the core satirical twist)
Each Warden team has an **Approval / Funding** meter (shown on screen like a news ticker).

| Action | Effect |
|---|---|
| Catching a Runner who has **crossed the Line** | +Approval |
| Shooting or killing a Runner who **hasn't crossed** and **hasn't attacked** | **Huge −Approval**, "SCANDAL!" headline on everyone's screen |
| Killing a Runner who attacked first or is carrying a gun | No penalty |
| Approval drops below thresholds | Wardens **lose drones, then trucks, then guns** for the rest of the match |

Each Runner needs a **`HasCrossedLine`** bool and a **`IsAggressor`** bool (set when they hurt a Warden or draw a gun). Both are server-side and replicated.

### 5.2 Holding pen and catapult
- Pen = trigger box. Caught Runners are teleported in and get a **sentence timer** (5–10 min, longer for repeat offenders or illegal items).
- Teammates can **break them out** (wire cutters on the fence, or bribe the Warden on duty).
- Timer done → they're loaded into the **catapult** → camera follows them → they ragdoll-fly back over the wall → land in town with **no fall damage**.
  (Set a `NoFallDamage` flag for 10 s, plus a funny landing sound.) They keep their money but **lose illegal items**.

---

## 6. Day/night and weather
- **Day cycle:** a Directional Light (sun) rotated by a GameState float `TimeOfDay` (replicated, updated every second). One full day = about 20 real minutes.
  Use **Ultra Dynamic Sky** (free/paid on Fab) or the built-in **Sky Atmosphere + Exponential Height Fog + Sky Light (real-time capture)**.
- **Night:** better stealth; floodlights and night vision matter; the back-alley dealer is open.
- **Weather** (GameState enum, changes every 5–10 min): Sunny / Rain (erases footprints, river rises and speeds up) / **Sandstorm** (drones grounded, vision short).
  Use a Niagara rain system attached to each local player's camera.

---

## 7. Extra ideas to make it streamable and friend-fight-y

1. **Disguises:** a stolen Warden uniform lets a Runner walk past… until they do something un-Warden-like.
2. **Bribes:** Runners can pay a Warden **player** to look away. Money really transfers, and the Warden's team can see a "bribe taken" log at the end. Betrayal fuel.
3. **The Coyote:** an NPC guide you can pay to reveal a secret route… 30% chance he's lying.
4. **News helicopter:** replays the funniest catapult launch and the worst "SCANDAL" for everyone at the end of the match.
5. **Twitch-style chaos events** (random or voted): Taco Truck Spawn, Sandstorm, Budget Cuts (Wardens lose drones for 2 min), Free Ladders Day.
6. **Proximity voice and radios:** Runners only hear nearby players. Wardens get a **radio** (team channel), but Runners can steal a radio and **listen in**.
7. **Ragdoll everything:** tackles, catapult, slipping in the river. Physics jank is content.
8. **Paperwork minigame:** after a catch, the Warden has to fill in a silly form (3 clicks) before the timer starts. Slows them down, and it's funny.
9. **Ladder wars:** ladders are physical and stay on the wall. Wardens can kick them over, even with someone halfway up.
10. **End-of-match awards:** "Most Catapulted", "Taco Tycoon", "Scandal of the Year", "Best Disguise".

---

## 8. Blueprint architecture

| Blueprint | Job |
|---|---|
| `BP_BR_GameMode` | Assign teams, phases and timers, win check, Approval rules, catapult queue |
| `BP_BR_GameState` | Replicated: Phase, PhaseTimeLeft, TimeOfDay, Weather, Approval, scores, `MC_Announce` |
| `BP_BR_PlayerState` | Team, Money, Inventory (array of `S_ItemStack`), HasCrossedLine, IsAggressor, SentenceTimeLeft |
| `BP_BR_PlayerController` | HUD, menus, Run on Server requests (buy, sell, give money) |
| `BP_BR_Character` | Movement, hunger, visibility/noise, interaction, carry/cuff/ragdoll (reuse BLUEPRINT_GUIDE Steps 3–5) |
| `BPC_Inventory` (Actor Component) | Add/Remove/Use items, server-authoritative, on the PlayerState |
| `BPC_Hunger` (Actor Component) | Drains bars, applies effects |
| `BPI_Interactable` | Same interface as BLUEPRINT_GUIDE: shops, jobs, doors, vehicles, pen, catapult, items |
| `BP_Shop`, `BP_Job_TacoCart`, … | Interactables opening a widget; buying = `SR_Buy(ItemRow)` on the server |
| `BP_Vehicle_Car` (Chaos Vehicle) | Fuel, key check, seats |
| `BP_Boat` | From BLUEPRINT_GUIDE Step 6 |
| `BP_Drone` | Flying pawn with camera, battery, tag ability |
| `BP_Ladder`, `BP_Fence` (cuttable), `BP_Camera`, `BP_Floodlight` | Obstacles |
| `BP_HoldingPen`, `BP_Catapult` | Catch → timer → launch (Launch Character with a big impulse, NoFallDamage) |
| `BP_LineTrigger` | Box along the border; sets HasCrossedLine on overlap (server) |
| `BP_DayNight`, `BP_Weather` | Read the GameState, drive the sun, fog and rain locally |
| `DT_Items`, `DT_Jobs`, `S_Item`, `S_ItemStack`, `E_Team`, `E_Phase`, `E_Weather` | Data |

---

## 9. Build order (test with your brother after every step)

1. **Multiplayer connection** (main menu, host/join via Radmin VPN; see the chat instructions). ✅ Do this first.
2. Teams + spawn + **phase timer** (Prep → Invasion → End) + Line trigger.
3. Interaction system + **money** + **one shop** + **inventory** (buy a ladder, see it in your slots).
4. **Ladder climbing** the wall. Now the core crossing works.
5. Warden **tackle → cuff → carry → holding pen → catapult**.
6. **Hunger** + food + taco cart job.
7. Car with key + **fuel** + gas can.
8. **Stealth**: visibility, noise pings, footprints, night.
9. **Drones** + guns + Approval / scandal rules.
10. River + boats, spike walls + wire cutters.
11. Day/night + weather.
12. Proximity voice (Steam), polish, awards, chaos events.

---

## 10. Later: Steam (invites + proximity voice)

When you're ready to drop Radmin:
1. Plugins: enable **Online Subsystem Steam**. Restart.
2. Add this to `Config/DefaultEngine.ini` (open it in Notepad):
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
MaxPlayers=16
```
4. Host button → **Create Session** (Public Connections 12, Use LAN ✗) → On Success → Open Level (`listen`).
   Join button → **Find Sessions** → **Join Session**. Get the free **Advanced Sessions** plugin for friend invites and session names
   (AppID 480 is Valve's shared test app, so you'll see other people's test sessions without filtering).
5. **Proximity voice:** on each character, when its PlayerState is valid, add a **VOIP Talker** component →
   **Register With Player State** → **Set Settings** (Attenuation = a Sound Attenuation asset with about 2,000 cm falloff, Component To Attach To = Mesh).
6. Steam doesn't work in Play-In-Editor. Test with **Standalone** or a packaged build, with Steam open on both PCs.
