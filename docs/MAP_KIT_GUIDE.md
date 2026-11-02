# Map Kit: build your own Border Run map

Build any map you want (Landscape, Fab buildings, your own village), then drop **BR pieces** on top so the game knows where everything is.
- **Your level has a BR Border Line or BR Safe Zone in it:** the game uses YOUR map.
- **Your level has none:** the old built-in test map is used.

**Does everything still happen at the right time? Yes.** These work exactly like before, but on your pieces:
- Prep → Line opens → Invasion → End
- NPC trucks during the Invasion
- villagers all game
- dogs, traps, drowning, the catapult, the Safe Zone win

Day/night, weather and ambient music are added to your map automatically.

---

## Step 0: Update
1. Copy the new **Source** folder into your project.
2. Delete **Binaries** and **Intermediate**.
3. Right-click the .uproject → **Generate Visual Studio project files**.
4. Double-click the .uproject → **Yes**.

## Step 1: Make your level
1. **File → New Level → Basic** (or Open World / Empty, your choice). **File → Save Current Level As** → name it, for example, `L_BorderTown`.
2. Build the ground: **Landscape mode** (Shift+2) → **Create**, or just drag in big cubes or Fab terrain.
3. **World Settings** (Window → World Settings) → **GameMode Override = BP_BRGameMode**.
4. To make it the map the game starts on: **Edit → Project Settings → Maps & Modes → Game Default Map** and **Editor Startup Map** = `L_BorderTown`.

## Step 2: How to find and place BR pieces
**Window → Place Actors** → type `BR ` (B, R, space) in the search box. Drag a piece into the level.
- Move: **W**, rotate: **E**, resize: **R**.
- Click a piece to see its settings in **Details** (right side).
- Zones and markers show as colored boxes and poles in the editor and are **invisible in game**. If you want to see them while testing, tick **Show In Game** on that piece.

---

## Step 3: The pieces

### Rules (you need these)
| Piece | What to do |
|---|---|
| **BR Border Line** | Place it where the Line is. **Rotate it so the yellow arrow points toward the Safe Zone side.** Set **Length** to how long the painted line and the Prep barrier should be. Everyone is blocked from crossing until the Line opens. |
| **BR Safe Zone** | Put it at the far end. Resize it with **R** to cover the safe area (green box). |
| **BR River** | Put it **at the water surface height** in your river bed. Resize with **R** so the blue box covers the river. **Rotate so the blue arrow points the way the water flows.** **Shallow Edge** = how wide the wade-able strip along each bank is. You get drowning, the current, flooded trucks and floating boats. Bridges above the water are safe automatically. Untick **Has Current** for a calm lake. |

### Border stuff
| Piece | What to do |
|---|---|
| **BR Wall Segment** | A piece of the Wall. Set **Length** and **Height**. Line several up; leave a gap for the gate. Runners holding a ladder can lean it anywhere on a segment. **Material** = your own concrete or steel look. |
| **BR Checkpoint Gate** | Put it in the gap between two wall segments, where the road goes through. |
| **BR Spike Fence** | A 4 m razor-wire panel. Line up as many as you want; leave gaps for risky routes. |

### Town (Runner side)
| Piece | What to do |
|---|---|
| **BR Station** | A counter. In **Details → BR Station** pick **Kind** (Buy Item / Sell Item / Job / Buy Boat / Bet) and **Item** (Ladder, Wire Cutters, Gas Can, Taco, Scrap, Illegal Gun), then set **Price**. Leave **Title** empty and the sign is written for you. Jobs: set **Work Seconds** (hold time) and **Cooldown Seconds**. **Night Only** = black market. Boats: drag the little diamond (**Boat Spawn Offset**) onto the water. |
| **BR Cockfight Pit** | The 4 betting counters appear around it by themselves. |
| **BR Villager Spot** | Put a few around town (plaza, market, houses). NPC villagers spawn and wander around them. How many: **BP_BRGameMode → Villager Count**. |

### NPC traffic
| Piece | What to do |
|---|---|
| **BR Road Point** | Click-place them along your road. Set **Order**: **0** = where trucks appear (start of the road), then **1, 2, 3...** to the end, where trucks disappear. On the point just **in front of the Checkpoint Gate**, tick **Wait For Checkpoint**: trucks stop there until a Warden opens the gate (or after a while). Their labels show "ROAD 0", "ROAD 1 (WAIT)" and so on. |

### Warden side
| Piece | What to do |
|---|---|
| **PVV Detention Zone** | The holding pen. Build walls around it with cubes or Fab assets. |
| **BR Catapult** | Place it near the pen. |
| **BR Catapult Landing** | Where released detainees land (back in Mexico). |

### Spawns, trucks and dogs
| Piece | What to do |
|---|---|
| **PVV Player Start** | Put 4 or more for each team. In Details set **Faction** to **Runners** or **Wardens** (Wardens = Border Patrol). |
| **Trucks** | Drag in your truck Blueprint (or **BRTruck**). Runner truck: tick **Requires Key**. Warden trucks: untick **Requires Key** and tick **Infinite Fuel**. Set **Paint Color**. |
| **Dogs** | Drag in your dog Blueprint (or **BRDog**). They patrol around wherever you put them (**Patrol Radius**). |

**Everything else you place** (houses, cacti, cars, people, lights, Fab assets) is just scenery and stays as you put it.

---

### Fun stuff
| Piece | What to do |
|---|---|
| **BR Human Cannon** | Point its arrow over the border. Runners need a **Cannonball Suit** (a BR Station selling Item = Cannonball Suit, Price 500). Climb in (hold E), aim with the mouse, press E to fire. Border Patrol can plug it. |
| **BR Tunnel** | Put it on the Mexico side. Drag the **diamond** (Exit Offset) to where people should come out, past the Wall. Runners need a **Shovel** (a Station selling Item = Shovel) to dig it open (25 s), then anyone can crawl through once the Line is open. Border Patrol can cave it in from either end. |
| **BR Pinata** | Put it in the plaza. Runners whack it (E) and the 10th whack wins cash or a random item. It comes back after 2.5 minutes. |
| **Mariachi Trumpet** | Sell it at a Station (Item = Mariachi Trumpet). A Runner who owns one presses **G** near Border Patrol and they dance for 4 seconds. Put your music in **BP_Runner → BR Mariachi → Trumpet Sound** and a dance animation in **BR Animations → Dance Anim**. |

| **BR Watchtower** | Border Patrol side. Rotate the arrow to where the spotlight should sweep. Agents climb it with E and aim the light with their view. |
| **BR Lucha Ring** | Wrestling ring. Runners pay to fight the luchador (win = double + his mask); others bet. Its counters appear by themselves. |
| **BR Chicken** | Put a few in a yard. They wander, flee, and can be grabbed and thrown. |
| **BR Station, Kind = Warden Supply** | A Border Patrol shop paid from the government budget. Pick **Warden Gear** (Extra Traps, Hard Hat, Drone Recharge, Patrol Dog), or set Gear = None and pick an **Item**. |
| **BR Station, Item = Poncho** | Sells the poncho disguise (try Price 200). |

## Step 4: Test checklist
1. Play → you spawn at your team's Player Start.
2. During Prep, try to walk across the Border Line. You should be blocked.
3. Press **~** and type `SkipPhase` to open the Line. Walk across: "You crossed the Line".
4. Walk into the deep part of the river: the breath bar drains and the current pushes you.
5. Walk into the Safe Zone: "MADE IT to the Safe Zone!"
6. Watch NPC trucks come along your road and stop at the gate.
7. Villagers wander around your Villager Spots.

**If something doesn't happen:**

| Problem | Fix |
|---|---|
| Crossing doesn't count | The Border Line arrow is pointing the wrong way. Rotate it 180°. |
| No NPC trucks | You need at least 2 Road Points, and it has to be the Invasion phase. |
| No drowning | The BR River box must reach down into the water and its top must be at the water surface. |
| Still getting the old desert | You're missing a **BR Border Line** or **BR Safe Zone**, or the level's GameMode isn't BP_BRGameMode. |
