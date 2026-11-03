# Complete setup guide (everything, from zero)

This one guide replaces all the older ones. It covers everything you have to set up yourself, in order. If you skip a part, that feature just falls back to something simple (a grey block, no sound, or the default hat). Nothing breaks.

**Words used here**
- **Content Browser**: the file panel at the bottom of Unreal.
- **Class Defaults**: open a Blueprint and click **Class Defaults** in the top toolbar. Its settings show on the right, in **Details**. To find a setting fast, type its name in the Details search box.
- **Compile / Save**: the two buttons at the top left of an open Blueprint. Click both after every change.

---

## Part 1: Update the code (every time I send an update)
1. Download the newest code from GitHub (branch `claude/serene-hypatia-ues3ex`) and unzip it.
2. Copy its **Source** folder into your project folder and click **Replace**. Only copy **Config** if I tell you to.
3. In your project folder, delete **Binaries** and **Intermediate**.
4. Right-click **PiratesVsVikings.uproject** → **Generate Visual Studio project files**.
5. Double-click **PiratesVsVikings.uproject** → **Yes** to rebuild.
6. **If the build fails:** send me `%localappdata%\UnrealBuildTool\Log.txt`.
   **If the game crashes while playing:** send me the text in the crash window. If no window appears, send `Saved\Logs\PiratesVsVikings.log` from your project folder.

---

## Part 2: The art files (all in `Art/Characters` in the download)

### 2a. Body with face shapes
1. In the Content Browser, find your character body mesh (the one BP_Runner uses).
2. Right-click it → **Reimport With New File** → pick `Art/Characters/SK_PVV_Body_Custom.glb`.
3. In the import window, tick **Import Morph Targets**. Click **Import**.
   - The body now has **eyebrows** (slot **M_Brow**, they follow the hair colour) and 27 face shapes: head, nose size, lips, ears, **face width, jaw, nose width, nose bridge (flat ↔ hooked/Aztec), nose tip (pointy ↔ round), eye size, brow thickness, brow height, brow mood (worried ↔ angry)** and Blink. NPCs and bots get random faces from all of them.
   - **Re-import the hair, beard and hat files too** (`SK_Hair_*`, `SK_Face_*`, `SK_Hat_*`, same way: Reimport With New File, tick Import Morph Targets). They got the face-width and jaw shapes so beards and masks follow the face.
   - The body now also has a **Blink** shape (everyone blinks at random and their eyes shut when knocked out). Re-import it if you imported the body before.
4. Keep the same skeleton.

### 2b. Hair, beards, hats, poncho
1. In the Content Browser, make a folder **Characters → Wearables**.
2. Drag in all of these files:
   - every `SK_Hair_*.glb` (15 hairstyles)
   - every `SK_Face_*.glb` (8 beards and mustaches)
   - `SK_Hat_Sombrero.glb`, `SK_Hat_HardHat.glb`, `SK_Hat_CannonHelmet.glb`, `SK_Hat_LuchaMask.glb`
   - `SK_Outfit_Poncho.glb`
   - `SK_Hat_CowboyHat.glb` and `SK_Hat_PatrolCap.glb`
3. In the import window:
   - **Skeleton** = your body's skeleton. This is the important part; without it the hats float.
   - **Import Morph Targets** ✓
   - Click **Import All**.

### 2c. Trumpet
Drag `SM_Trumpet.glb` into the same folder. It's a normal static mesh, so the skeleton doesn't matter.

### 2d. Animations
**Already imported the animations before?** Just import **`SK_PVV_Animations_NewMoves.glb`** (same folder): it only has the 3 new ones, **Emote_Smoke, Kick, Headbutt**. Same import settings as below. Then set them in BP_Runner / BP_Warden (Part 3a).

1. Make a folder **Characters → Animations**.
2. Drag in `SK_PVV_Animations.glb`.
   - **Skeleton** = your body's skeleton
   - Tick **Import Animations**
   - Click **Import**.
3. You get one animation per move: Idle, Walk, Run, CrouchIdle, CrouchWalk, Fall, Swim, Seated, Drive, Restrained, Struggle, Carried, Work, Aim, AimWalk, Punch, Fire, Reload, Eat, Dance, Trumpet, Emote_Wave, Emote_Flex, Emote_Taunt, Emote_Beg, Emote_Cheer, **Emote_Smoke, Kick, Headbutt** (new: re-import this file if you imported it before).
4. If it also made a copy of the body mesh, delete that copy and keep the animations.

---

## Part 3: BP_Runner and BP_Warden
Open **BP_Runner**. Do everything below, **Compile** and **Save**, then do the same in **BP_Warden**.

### 3a. Animations
1. Click the **Mesh** component (top left, in the Components list). In Details → **Animation**:
   - **Animation Mode** = *Use Animation Blueprint*
   - **Anim Class** = **None**
2. Back in **Class Defaults**, under **BR | Animations**, drag each animation into the slot with the same name (Idle Anim = Idle, Walk Anim = Walk, and so on). For the emotes:
   - Emote Wave Anim = Emote_Wave
   - Emote Flex Anim = Emote_Flex
   - Emote Taunt Anim = Emote_Taunt
   - Emote Beg Anim = Emote_Beg
   - Emote Cheer Anim = Emote_Cheer
   - **Emote Smoke Anim = Emote_Smoke**
   - **Kick Anim = Kick**, **Headbutt Anim = Headbutt**
3. Leave a slot empty if you don't have that animation. Your character just keeps the closest one it has.

### 3b. Hairstyles and beards (BR | Look)
- **Hair Styles**: click **+** once per hairstyle and pick the `SK_Hair_...` meshes. The order here is the order you scroll through them in the menu.
- **Facial Hair Styles**: the same, with the `SK_Face_...` meshes.
- If your old hair or hat is set on the **OutfitHead** or **OutfitFace** component, set that component's Skeletal Mesh to **None**. Otherwise you'll wear two.

### 3c. Hats (BR | Hats)
**Hat Meshes**: click **+** six times and fill in these rows:

| Left box | Right box |
|---|---|
| Cowboy Hat | SK_Hat_CowboyHat |
| Sombrero | SK_Hat_Sombrero |
| Patrol Cap | SK_Hat_PatrolCap |
| Hard Hat | SK_Hat_HardHat |
| Cannonball Helmet | SK_Hat_CannonHelmet |
| Lucha Mask | SK_Hat_LuchaMask |

Then set:
- **Default Hat**: Cowboy Hat in BP_Runner, Patrol Cap in BP_Warden.
- **Poncho Mesh** = SK_Outfit_Poncho.
- **Held Item Meshes**: click **+**, set **Mariachi Trumpet** → **SM_Trumpet**.
- **Held Item Offset**: only change this if the trumpet sits wrong in the hand. Change its Location and Rotation a little at a time.

### 3d. Sounds on the character (optional)
Drag in any sound you like:
- **BR | Brawl**: Punch Swing, Punch Hit, Knockout, Grab, Slap, Tackle, Throw
- **BR | Mariachi**: Trumpet Sound
- **BR | Weapons**: Gun, Taser, Reload

### 3e. Make the colour sliders work (optional, about 3 min each)
- **Skin:** open the body mesh, then double-click its material. Right-click → **Vector Parameter**, name it `SkinColor`, and set its default to white. Connect it:
  - With no texture: plug it straight into **Base Color**.
  - With a texture: add a **Multiply** node, plug the texture into A and SkinColor into B, then plug Multiply into **Base Color**.
  - Save.
- **Shirt / pants:** the same on the shirt material (`ShirtColor`) and the pants material (`PantsColor`).
- **Hair:** the same on the hair material (`HairColor`).

---

## Part 4: BP_BRGameMode (Class Defaults)
| Setting | Set it to |
|---|---|
| **PVV \| Factions → Pirate Character Class** | BP_Runner |
| **PVV \| Factions → Viking Character Class** | BP_Warden |
| **BR \| Your Blueprints → Runner / Warden / NPC Truck Class, Dog Class** | your truck and dog Blueprints, or leave them empty for the built-in ones |
| **BR \| Traffic → NPC Passenger Class** | leave it on **BRCharacter** (it uses your BP_Runner automatically) |
| **BR \| World → Use Built In Test Map** | ✓ = the desert test map gets built for you |

Also: **World Settings** (Window → World Settings) → **GameMode Override = BP_BRGameMode**, in every level you play.

Everything else (timers, points, budget, bounty, bots) can be changed from the in-game **M** menu or in the BR | ... sections here.

---

## Part 5: Game-wide sounds (optional)
**Edit → Project Settings → (left list, Game) Border Run → Sounds.** Drag a sound into any slot:
- Buy, Sell, Cash Pickup, Payday
- Zip Tie, Jail Door, Splash
- Scan OK, Scan Alarm
- Trap Snap, Tripwire Alarm, Hat Pop
- Spotted, Bounty
- Round Start, Round End, Countdown
- Safe Zone, Whistle, Landing, Requisition

Each slot has a description of what to look for.

**Body Sounds** (same page, further down): you can add several of each, and one is picked at random every time:
| Slot | What to find |
|---|---|
| **Footstep Sounds** | 3-6 footsteps on dirt or gravel |
| **Body Thump Sounds** | 2-4 heavy body thuds (ragdolls hitting the ground) |
| **Hit Grunt Sounds** | 4-8 "oof", "ugh" or pain grunts |
| **Flying Yell Sounds** | 2-4 screams (thrown, launched, catapulted) |
| **Whoosh Sounds** | 2-3 swing or whoosh sounds (punch, kick, dive) |

**Effects** (same page): drag in Niagara particle effects (free ones on **Fab**: search "dust", "smoke", "splash", "impact", "sparks"):
| Slot | Used for |
|---|---|
| Dust Effect | hard landings, belly-flops, falling off walls |
| Splash Effect | someone hitting deep water |
| Smoke Effect | cannon smoke |
| Punch Hit Effect | where a punch, kick or headbutt lands |
| Cigarette Smoke Effect | the smoking emote's puffs |
| Sparks Effect | the cannon's fuse |

Empty slots use simple built-in puffs.

---

## Part 6: What's on the test map (built automatically)
| Where | What |
|---|---|
| Village (west) | jobs, shops (ladder, cutters, gas, tacos, **poncho**, **trumpet**, **shovel**, **cannonball suit**), coyote counter, cockfight, piñata, **chickens**, villagers |
| Open desert by the scrapyard (south-west) | the **Lucha Libre ring** with its red challenge counters and green/blue betting counters |
| Near bank | **Human Cannon** (climb up on the barrel, a friend lights the fuse: BOOM), tunnels, marina |
| Border Patrol side | 2 **watchtowers**, the Holding Pen and catapult, blue **GOV. SUPPLY** counters (now with a **Lasso**) |
| North of the plaza | the **mariachi band stage** |
| Around both sides | **hiding spots**: dumpsters, porta-potties, hay bales |
| Tool shop | new counters: **LASSO $150**, **CARDBOARD BOX $30** |

**The mariachi band:**
1. Buy a trumpet, walk onto the stage, and press **E** to join the band.
2. Pick your trumpet (mouse wheel) and press **F ON THE BEAT**. Watch the pulsing square at the bottom: green = now.
3. Every 4 good notes in a row, the crowd tips you, and more for longer streaks (up to $120 a minute).
4. To use your own song, select the stage in your level (Map Kit: **BR Mariachi Band**). Set **Music** to a looping song, set **Beats Per Minute** to match it, and add short **Note Sounds** and a **Miss Sound**.

**The Lucha Libre ring:**
1. Walk up to a red counter and press **E** to fight for $50, $100 or $250. Bigger fights mean a tougher luchador.
2. You get 15 seconds while others bet at the green (on you) or blue (on the luchador) counters.
3. At the bell, the first fighter knocked down twice loses. Throwing him out of the ring counts as a knockdown, but if you leave the ring yourself, you lose.
4. If you win, you get double your money **plus his mask**. Everyone who bet on the winner gets double their bet.

---

## Part 7: Controls
| Key | What it does |
|---|---|
| WASD / Shift / Space / C | move / sprint / jump / crouch |
| **Left click / Right click** | your **left / right hand**. **Tap = punch** with that hand. **Hold = reach out and grab** whoever it touches; let go to drop them. Hold one hand on someone and tap the other to punch them. |
| **Space while grabbed** | mash it to wriggle free |
| **Left Ctrl or mouse thumb button** | **dive / tackle** |
| **O** (Border Patrol) | **order the suspects OUT of the vehicle you're next to** (whistle!) |
| **hold Left Alt** | **BLOCK** (hits from the front do ~15% damage; too many hits = guard break; grabs and hits from behind still get you) |
| Z | grab with both hands (people) or pick up things; Z again to throw; on a teammate: climb on their shoulders |
| E | interact (hold for jobs, zip-ties, ladders) |
| Mouse wheel / F / G | pick an item / use it / drop it |
| Hold T | emote wheel (Wave, Flex, Taunt, Beg, Cheer, Dance, **Smoke**). Emotes keep going until you move, fight, or pick the same one again. |
| **R** (fists out) | **kick** |
| **Middle mouse** | **headbutt** (breaks the grip of whoever's holding you) |
| **Z at a tall wall** | start **climbing**: then **left, right, left, right** clicks. The same hand twice = slip (3 slips = fall). At the top, **Space** pulls you up. Space while climbing = let go. Falls over ~3 m knock you out. Climbing the Border Wall sets off its **sensors**. |
| **Z on a knocked-out person** | pick them up and carry them (Z again throws) |
| **F with the Lasso** | throw it where you're looking: yanks people off their feet (and off walls) |
| **F with the Cardboard Box** | get in or out of the box. You're invisible while standing still. |
| **E at a dumpster / porta-potty / hay** | hide inside (Border Patrol: search it) |
| **Z near a dog** (Border Patrol) | put it on a leash. It pulls toward Runners. Z again lets it loose to chase. |
| **Tab** | scoreboard |
| H | drop $50 |
| **X** | weapon: fists → taser → pistol → fists (Runners: fists ↔ revolver, if you bought one). With a weapon out, right mouse aims and left click shoots. |
| V / B / Q | (Border Patrol) scan / trap / drone |
| Y | ready up in the lobby |
| N | watch other players while in the pen |
| K | mute mic |
| M | menu (team, customize, host settings, **settings**: mouse, invert, field of view, volume, music) |

### Grabbing and arresting (Gang Beasts style)
- **Hold right click** next to someone to grab them with your right hand. **Tap left click** to punch them in the face while you hold them.
- **Border Patrol:** while holding someone, **hold E** to zip-tie them. It takes a bit longer than cuffing a knocked-out person, and they can **mash Space** to wriggle free. One hand is easier to escape than two (Z grabs with both).
- NPCs with no papers fight back when grabbed.

### How fighting works now (Gang Beasts style)
- **Any hand moving fast enough hits whoever it touches.** That covers punches, flailing arms and swinging around, and it hits players, bots, villagers and the luchador alike.
- Hits to the **head** do 60% more.
- Getting hit makes you **woozy**: your body goes floppier the closer you are to being knocked out. At zero balance you go down.
- To tune it: **BP_Runner / BP_Warden → BR | Brawl**:
  - **Slap Speed**: how fast a hand must move to hit (lower = more hits)
  - **Hand Hit Min/Max Damage**
  - **Head Hit Multiplier**
  - **Hit Reaction**: how hard body parts snap back
  - **Punch Impulse**: how far punches fly out
  - **Dive Speed**

---

### Play of the Round
When a round ends, the biggest moment gets replayed in slow motion with a circling camera. That might be a cannon launch, a catapult, a throw, a lasso, a flying tackle, a huge fall, the bounty catch or a luchador win. It happens automatically.

---

## Part 8: Playing together
- **Standalone / packaged game needs a SAVED level.** If your editor shows "Untitled", use **File → Save Current Level As**. Then go to **Project Settings → Maps & Modes** and set **Editor Startup Map** and **Game Default Map** to it.
- **Host:** M → **HOST GAME (Steam)** or **HOST GAME (Radmin / LAN)**. Your friend uses **FIND A FRIEND'S GAME** or types your Radmin IP into **JOIN IP**.
- **Lobby:** everyone presses **Y** when ready. The host can press **START NOW**.
- **Host settings** (right side of the M menu, host only): Prep time, Invasion time, Ready-up lobby, Best of 3, Fill with bots, team sizes.
- **Bots** also work from the console (press **~**): `AddBots 3`, `AddBot Wardens`, `RemoveBots`.

## Unlockable hats
- **Sombrero:** eat 30 tacos.
- **Hard Hat:** knock out 25 people.
- **Cannonball Helmet:** get shot from the cannon 10 times.
- **Lucha Mask:** beat the luchador.

Pick your hat in **M → Customize → Hat**.

---

## Part 9: Texture the test map (water, sand, buildings...)
Everything on the test map can get a real texture from one settings page. Nothing to place by hand.

### Step 1: Get some textures (5 min)
**Starter Content (free, built in):** Content Browser → **+ Add** → **Add Feature or Content Pack** → **Content** tab → **Starter Content** → **Add to Project**. You get:
- `StarterContent/Textures`: e.g. `T_Rock_Sandstone_D` (sand), `T_Concrete_Poured_D` (road), `T_Brick_Clay_Old_D` (buildings), `T_Wood_Pine_D` (wood), `T_Metal_Rust_D` (metal), `T_Cobble_Stone_Pebble_D` (plaza)
- `StarterContent/Materials/M_Water_Lake` (water)

**Fab (free, nicer):** **Window → Fab**. Search "sand", "asphalt", "stucco", "wood planks", "rusty metal" and filter **Free**, then **Add to project**. Use the texture whose name ends in `_D`, `_BaseColor` or `_Albedo`.

### Step 2: Make ONE "world texture" material (5 min, only once)
This makes a texture repeat every few metres instead of stretching one picture across a 300 m desert.
1. Content Browser → right-click → **Material** → name it `M_WorldTexture` → double-click it.
2. In the graph: right-click empty space → type **WorldAlignedTexture** → click it.
3. Right-click → type **TextureObjectParameter** → click it. In Details on the left, set **Parameter Name** = `Texture` (exactly) and pick any texture as its default. Drag its output into the **TextureObject** pin of WorldAlignedTexture.
4. Right-click → **ScalarParameter**. Set **Parameter Name** = `TileSize` and **Default Value** = `400`. Drag it into the **TextureSize** pin.
5. Drag WorldAlignedTexture's **XYZ Texture** output into **Base Color** on the big node.
6. Click **Save**.

### Step 3: Fill in the slots (2 min)
**Edit → Project Settings → (Game) Border Run → Test Map Look**:
1. **World Texture Material** = `M_WorldTexture`.
2. For each surface, open it and set either a **Texture** (it repeats; change **Tile Size** to make it bigger or smaller) or a finished **Material** (a Material always wins over a Texture):

| Slot | What it covers | Try |
|---|---|---|
| **Ground** | desert sand and the map edges | T_Rock_Sandstone_D, Tile Size 600 |
| **Road** | road and bridge deck | T_Concrete_Poured_D |
| **Water** | the river and the fountain | **Material** = M_Water_Lake |
| **Buildings** | houses, shops, church, HQ | T_Brick_Clay_Old_D or a stucco texture |
| **Roofs** | village roofs | a tile or wood texture |
| **Border Wall** | THE WALL | T_Concrete_Poured_D |
| **Fences** | razor-wire panels | T_Metal_Rust_D |
| **Wood** | counters, benches, stalls, doors, watchtowers | T_Wood_Pine_D |
| **Metal** | rails, gas pumps, cells, junk | T_Metal_Rust_D |
| **Plaza** | plaza floor and fountain stone | T_Cobble_Stone_Pebble_D |
| **Ring Canvas** | the lucha ring floor | any cloth texture |

3. Press **Play**. The textures go on when the round starts. Empty slots keep their old colours.

> **Water looks stretched or too smooth?** Use the Texture route with a water picture instead, or get a free water material from Fab.

---

## Part 10: Border Patrol: hauling NPCs
1. At the checkpoint, scan the passengers (**V**). Then hold **E** on the truck: **Order everyone OUT**.
2. People with **no papers make a run for it**. Chase them, then **tase** them (X to switch to taser, right mouse to aim, left click to shoot) or punch them down.
3. Knocked-down **NPCs stay down 14 seconds**, so there's time to zip-tie them: hold **E** on them, then tap **E** to pick them up.
4. Walk to your Border Patrol truck and press **E** to load them in the back. Drive to the Holding Pen.
5. Get out and press **E** on the truck: **Take them out**. Carry them to the **pen**, or to the **catapult** to fling them back.

To tune: BP_Runner → **BR | NPC** → **NPC Knockdown Seconds** and **Flee Speed**.

Only **one NPC truck plays music** at a time now.

---

## Part 11: Test multiplayer by yourself

1. Next to the green **Play** button, click the **three dots** (⋮).
2. Set **Number of Players** to **2** (or 3, 4...).
3. Set **Net Mode** to **Play As Listen Server**.
4. Press **Play**. You get one window per player: the first one is the host, the others are friends joining.
5. Click inside a window to control that player. **Shift+F1** frees your mouse so you can click the other window.
6. For a more realistic test: ⋮ → **Advanced Settings** → untick **Run Under One Process** (slower to start, but each window is like a separate PC).

Steam invites only work in a real packaged/standalone game, not in these test windows.

## Part 12: Lasso, busts and trucks

- **Lasso:** buy it (shop stand near spawn $, or the Border Patrol supply counter $250). Scroll the **mouse wheel** until the lasso is your selected item, look at someone and press **F**. It yanks them off their feet (works on players, NPCs and climbers), then **E** (hold) to zip-tie them. It recharges every 4 seconds.
- **Ordering people out of a truck (E, hold):** only the people with no papers or with contraband get out; the clean ones stay seated. Each one picks at random, depending on how bad they are: **fight you Gang Beasts style**, **pull a KNIFE** (double damage; knock them down and it drops), **run**, or **put their hands up**. Sometimes a dirty driver just **floors it through the gate**.
- **Driver pulled out?** After a second someone else aboard slides over and takes the wheel. Clean people drive on through the checkpoint; dirty ones floor it or jump out and run.
- An NPC truck is only towed away once **nobody is left in it**, and anyone who already got out (running, cuffed, being carried) stays in the world.

## Part 13: Kick + headbutt animations

They're already inside **Art/Characters/SK_PVV_Animations.glb** (it has Kick and Headbutt).
1. Copy the new **SK_PVV_Animations.glb** into the same Content folder you imported it to before, and drag it into the Content Browser over the old one (or right-click the old animations -> **Reimport**). Pick your character skeleton if it asks.
2. You'll get new animation assets named **Kick** and **Headbutt** (and **Emote_Smoke**).
3. Open **BP_Runner** (and **BP_Warden**, and your NPC Blueprint if you have one) -> Class Defaults -> **BR | Animations**:
   - **Kick Anim** = Kick
   - **Headbutt Anim** = Headbutt
4. Compile + Save.

## Part 14: The bigger road + smashing the gate

- The test map's road is now **two lanes wide** (yellow line in the middle) from the edge of town all the way to the safe zone, and the far side is **much longer** (the safe zone moved way further east).
- NPC trucks now start at the edge of town (the narrow town street was where they jammed) and won't spawn on top of each other.
- **Smash the gate:** drive at the closed checkpoint barrier fast in any vehicle and it SNAPS - pieces fly, everyone hears about it. Border Patrol fixes it after 30 seconds (Gate Smash sound slot: Project Settings -> Border Run -> Sounds -> **Gate Smash Sound**).

## Part 15: Town life (cantina, car lot, muggings...)

Everything here is built on the test map automatically.

- **La Cantina del Diablo** (south of the town street, door faces the road). 8 locals hang out inside. Nothing happens until YOU start it.
  - **TEQUILA ($15)** at the bar: you're *tipsy* for 25 s (max 45 s if you keep drinking; the timer shows at the bottom of the screen), wobblier but you take 30% less damage.
  - **DRINKING CONTEST ($50, win $150)** at the table: **mash SPACE** to chug faster than El Borracho. Lose and you pass out.
  - **THE BIG GUY** at the bar: hold E to shove him and start a **BAR FIGHT** (punching anybody in there starts one too). Everybody fights everybody, and they all gang up on you. You must be the **ONE last person standing** (if two players are left, you fight each other) within **75 seconds**. The winner gets a big **YOU WON THE BAR FIGHT!** banner and **$40 per knocked-out local** on the floor (only the winner can pick it up). **Get knocked out = YOU LOST, -$50**, nothing for you. Time runs out = the cops break it up, nobody wins. The top of the screen shows how many locals are still standing and the clock.
  - **El Bouncer** stays outside at the door. Start 2 bar fights and he shoves you away every time you try to come in.
  - Locals come back about 2.5 min after a fight.
- **Car lot "Autos El Gato"** (north of the plaza): walk up to a car and **hold E** to buy it. Pickup $500, Sedan $500, **Lowrider $800** (fast, 2 seats), **Van $1200** (6 seats; the 4 in the back are hidden inside the box, great for smuggling). You and your team can drive it.
- **NPC trucks** now come in all 4 car types and lots of colours, so you can't tell a player's car from an NPC's.
- **Punch a local and they fight back.** Knock them out and they drop something: scrap, a taco, sometimes $50. **1 in 20 locals carries a fat wad ($200)**.
- **Muggings:** now and then a local walks up, shoves you over and **steals an item**. Beat them up and it drops; only you can pick it back up.
- **Troublemakers:** starting lots of fights puts a **BOUNTY** on you (Border Patrol gets bonus points for catching you). If a Border Patrol player keeps beating up locals, approval drops.
- **Border Patrol in town:** the locals HATE it. Hang around in town and they start jumping you, sometimes with knives.
- **Wanted posters** around town show whoever has the bounty (or is winning).
- **The scanner** now says **"NO PAPERS - ILLEGAL"** for Runner players exactly like it does for illegal NPCs (it never says "player"), and lists any contraband: **Drugs, Illegal Gun**.
- **Chickens** walk around, peck and run away from you. Grab them while they move (Z).
- **Mariachi:** the band members can't knock each other over any more, and only ONE trumpet plays at a time (it goes quiet while you solo).

**New sound slots** (Project Settings → Border Run → Sounds): **Cantina Music**, **Bar Fight Sound**, **Drink Sound** (plus **Gate Smash Sound** from last time).

---

## Part 15b: Lucha ring, blocking, day/night + weather

- **Lucha ring:** the ropes are solid during a fight, so you bounce off them and stay in the ring. Hits throw you far less in there. The luchador now actually moves: he charges, circles, blocks and goes for grabs and kicks when you block.
- **Block:** hold **Left Alt**. You move slowly and can't punch while blocking. Every NPC fighter blocks too.
- **Mariachi** wear all black with sombreros. The black only shows if your shirt and pants materials have the `ShirtColor` / `PantsColor` parameters (Part 3e).
- **Day and night really happen now.** The game uses your level's own sun, sky light and fog if it has them (before, it switched day/night off when your level had a sun). Night gets dark and blue, and street lamps along the big road switch on. A few old lamps flicker and cut out now and then.
- **Rain:** grey and darker, rain streaks around you, lightning flashes and thunder.
- **Sandstorm:** thick orange fog and blowing sand.
- Optional, to make weather look and sound better (Project Settings → Border Run):
  - **Effects → Rain Effect / Sandstorm Effect**: any Niagara rain or dust system (free ones on Fab). They replace the simple built-in streaks.
  - **Sounds → Rain Loop Sound, Wind Loop Sound** (tick *Looping* on the sound asset), and **Thunder Sound**.
- If night is still bright: in your level, delete any extra **Directional Light**s (keep one), and remove any **Post Process Volume** with *Infinite Extent* that forces exposure.

---

## Part 15c: Clothes, glasses and the little extras

**New clothes + glasses** (in `Art/Characters/Outfits`):
- Shirts: `SK_Shirt_TShirt`, `SK_Shirt_TankTop`, `SK_Shirt_Flannel`, `SK_Shirt_Hoodie`, `SK_Shirt_Guayabera` (+ the old `SK_Outfit_WorkShirt`)
- Pants: `SK_Pants_Shorts`, `SK_Pants_Cargo`, `SK_Pants_Sweatpants` (+ the old `SK_Outfit_Jeans`)
- Glasses: `SK_Glasses_Aviator`, `SK_Glasses_Round`, `SK_Glasses_Shades` (+ the old `SK_Outfit_Sunglasses`)

1. Import them like the hair: drag into the Content Browser, **Skeleton = your body's skeleton**, tick **Import Morph Targets**.
2. Open **BP_Runner** (and BP_Warden if you want them there too) → Class Defaults → **BR | Look**:
   - **Shirt Styles**: + one per shirt
   - **Pants Styles**: + one per pants
   - **Glasses Styles**: + one per glasses
3. Compile + Save. In the **M** menu you now have **Shirt style / Pants style / Glasses** pickers (the colour sliders still tint them if their material has `ShirtColor` / `PantsColor`). NPCs and bots get random clothes and sometimes glasses too.

**Extras:**
- **Name tags:** only your teammates see your name (it shrinks and disappears with distance). Border Patrol never sees "Runner" tags, names or emote bubbles over players. Only a **tagged** Runner shows anything to them.
- **Wanted stars:** start fights → 1, 2, 3 stars (shown at the top of your screen; your team sees them over your head). 3 stars = Border Patrol hears where you are every 10 s. They cool off after 45 s of behaving.
- **Bribes:** drop cash (your cash-drop key) at a Border Patrol player's feet. If they pick it up, your stars and bounty are wiped. ("Totally normal.")
- **Coyote whistle:** Runners press **Q** → your smuggling clients sprint to you. Loud: Border Patrol gets a noise ping.
- **Victory poses** at the end of the round (winners flex/cheer/dance, losers beg).
- **Dizzy birds** circle your head when you're nearly knocked out.
- **Knockout taunts** in the kill feed. **Big knockouts** near you punch the camera in (and a moment of slow-mo when playing alone).
- **Pinata candy:** smashing the pinata scatters candy. Eat it (E) for full balance.
- **Chicken stampede:** throw a chicken (Z) at the other chickens and they all panic.
- **Traffic honks:** NPC trucks stuck in line lean on the horn (uses the truck's Horn Sound).
- **Footprints** in the sand that fade after ~20 s, handy for tracking people.

---

## Part 16: Checklist: everything you can plug in

Tick these off. Anything you skip just stays silent or uses a simple placeholder; nothing breaks.

**Art (Content Browser imports, Part 2):**
- [ ] Body with face shapes (SK_PVV_Body...)
- [ ] Hairs, beards, outfits, **poncho** (SK_Outfit_Poncho)
- [ ] All 6 hats: CowboyHat, Sombrero, PatrolCap, HardHat, CannonHelmet, **LuchaMask**
- [ ] Trumpet (SM_Trumpet)
- [ ] **SK_PVV_Animations.glb: re-import it.** It has 30 animations including the new **Emote_Smoke, Kick, Headbutt**

**BP_Runner AND BP_Warden (Part 3):**
- [ ] Every **BR | Animations** slot, especially the new **Emote Smoke Anim, Kick Anim, Headbutt Anim**
- [ ] **BR | Look**: Hair Styles, Facial Hair Styles, **Shirt Styles, Pants Styles, Glasses Styles**
- [ ] **BR | Hats**: Hat Meshes (6 rows), Default Hat, **Poncho Mesh**, Held Item Meshes (Trumpet)
- [ ] **BR | Brawl / Mariachi / Weapons** sounds (optional)
- [ ] Colour parameters in the materials (optional, 3e)

**BP_BRGameMode (Part 4):** character classes + GameMode Override in World Settings.

**Project Settings → Border Run (all optional):**
- [ ] **Sounds**: every slot, including the new Gate Smash, Cantina Music, Bar Fight, Drink, **Rain Loop, Wind Loop, Thunder**
- [ ] **Body Sounds**: footsteps, thumps, grunts, yells, whooshes
- [ ] **Effects**: Niagara effects (dust, splash, smoke, punch hit, cigarette smoke, sparks, **rain, sandstorm**)
- [ ] **Test Map Look**: the World Texture material + textures (Part 9)

**Level:** an **Empty Level** saved as L_TestMap, GameMode Override = BP_BRGameMode, and set as the default map (Project Settings → Maps & Modes).

## If something's wrong
| Problem | Fix |
|---|---|
| Characters T-pose or slide | Part 3a: Anim Class = None and the animation slots are filled. |
| Hats float or sit inside the head | Re-import the hats with **Skeleton** = your body's skeleton (Part 2b). |
| Everyone wears the same hat | **Hat Meshes** is empty in that Blueprint (Part 3c). |
| No face sliders effect | Part 2a: reimport the body with Import Morph Targets ticked. |
| Skin colour also changes eyes and lips | Your body uses one material for everything. Give the skin its own material slot, or follow Part 3e. |
| Punches don't land | Make sure **BR \| Physics → Wobbly Body** is ✓ and your body has a **Physics Asset**. Try **Slap Speed** 300. |
| The old desert shows up on your own map | Your level needs a **BR Border Line** or **BR Safe Zone** (see MAP_KIT_GUIDE). |
