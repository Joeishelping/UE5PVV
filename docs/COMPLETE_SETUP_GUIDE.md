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
**Already imported the animations before?** Just import **`SK_PVV_Animations_NewMoves.glb`** (same folder): it only has the new ones, **Emote_Smoke, Kick, Headbutt, Block**. Same import settings as below. Then set them in BP_Runner / BP_Warden (Part 3a).

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
| Village (west) | jobs, tacos, coyote counter, cockfight, piñata, **chickens**, villagers |
| **FERRETERIA** (big tool shop, south side of town, by the gas pumps) | ladders, cutters, gas, shovel, lasso, cardboard box, poncho, trumpet, cannonball suit, scrap buyer, register job, and **boats & rafts** (they wait for you at the river **BOAT DOCK**) |
| **ARMAS DON CHUY** (big gun shop, far west end of town) | knife, pistol, fake passport, fake uniform, TNT, smoke bombs, grappling hook, lockpick, fuse kit, and the **black market back room** (the cheap gun only at night) |
| Open desert by the scrapyard (south-west) | the **Lucha Libre ring** with its red challenge counters and green/blue betting counters |
| Near bank | **Human Cannon** (climb up on the barrel, a friend lights the fuse: BOOM), tunnels, the boat dock |
| Border Patrol side | 2 **watchtowers**, the Holding Pen and catapult, blue **GOV. SUPPLY** counters (now with a **Lasso**) |
| North of the plaza | the **mariachi band stage** |
| Around both sides | **hiding spots**: dumpsters, porta-potties, hay bales |

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
| **hold Left Alt** | **BLOCK**: fists up in front of your face. Hits on your guard only drain the guard bar, not you. Guard bar empty = GUARD BREAK (staggered, can't block 2.5 s). Grabs and hits from behind still get you. |
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
- **Car lot "Autos El Gato"** (north of the plaza): walk up to a car and **hold E** to buy it. Pickup $500, Sedan $500, **Lowrider $800** (fast, 2 seats), **Van $1200** (6 seats, big windows). You and your team can drive it.
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

## Part 15d: Fights last longer (Gang Beasts pacing)

- Every hit does about half the damage it used to and pushes you less, so brawls go on longer and you stay in the scrap.
- **DAZED:** the hit that *would* have knocked you out dazes you instead. Birds circle your head, you stagger slowly and can't punch for about 2 seconds. One more good hit while dazed = knocked out. Get away or block!
- **Bar fight:** 2.5 minutes on the clock. Getting knocked down once doesn't end it for you: get back up. The **second** time you're out (-$50).
- **Lucha ring:** 2.5-minute fights.
- Tweak it yourself: BP_Runner / BP_Warden → **BR | Brawl → Fight Toughness** (higher = longer fights, 1.9 now), **Hit Push Scale** (0.7), **Daze Seconds** (2.2).

---

## Part 15e: Slots fill themselves

If you leave a slot **empty** in BP_Runner / BP_Warden Class Defaults, the game fills it with the matching asset you imported, by name:
- **Hats** (SK_Hat_CowboyHat, SK_Hat_Sombrero, SK_Hat_PatrolCap, SK_Hat_HardHat, SK_Hat_CannonHelmet, SK_Hat_LuchaMask), **Poncho Mesh** (SK_Outfit_Poncho)
- **Hair Styles** (SK_Hair_...), **Facial Hair Styles** (SK_Face_...), **Shirt Styles** (SK_Shirt_... + SK_Outfit_WorkShirt), **Pants Styles** (SK_Pants_... + SK_Outfit_Jeans), **Glasses Styles** (SK_Glasses_... + SK_Outfit_Sunglasses)
- **Every animation slot** (Idle, Walk, ..., Emote_Smoke, Kick, Headbutt), matched on your body's skeleton.

So all you have to do is **import the files** (keep their names). Anything you set yourself is kept. Untick **BR | Look → Auto Fill Empty Slots** to turn this off.
(When you package the game later, fill the slots in by hand, because packaging only includes assets something points to.)

---

## Part 15f: Prison breaks, jobs, weapons, map, smarter AI

- **Prison breaks:** locked in the holding pen and no Border Patrol within ~18 m? **Mash SPACE** to pick your cuffs (bar on screen). A guard nearby = no progress. Escape = alarm + noise ping. NPC prisoners work their cuffs loose on their own if nobody watches them.
- **Smuggling jobs:** the **SE BUSCA** job board in town (near the cockfight pit). Take a package, get it to the Safe Zone before the timer runs out = $300-650 bonus. The package shows up as CONTRABAND on the scanner. Too slow = the client takes it back.
- **Weapons to grab (Z) and swing:** chairs and bottles in the cantina (bottles SMASH when they hit someone), planks lying around town.
- **Mariachi serenade:** the trumpet (G) now makes anyone nearby dance (locals, the other team), not just Border Patrol. Not your teammates.
- **Map:** hold **Tab** (with the scoreboard): top-down map with landmarks, you (white), your team (green) and tagged enemies (red).
- **Day counter** at the top, and the round-end screen lists the **5 best moments** of the round.
- **Ambient sounds:** Project Settings → Border Run → Sounds → **Night Crickets Loop, Town Chatter Loop, Desert Wind Loop, Distant Dog Barks** (tick Looping on the loops).
- **NPC trucks** keep a much bigger gap in the queue at the border.
- **Smarter AI:**
  - *Runner bots* spread out along the border, duck into hiding spots when Border Patrol is near, fight back when cornered (and finish off dazed cops), block.
  - *Border Patrol bots* lead their target, **tase** runners who run, investigate noises, and work the checkpoint (walk up to waiting trucks, scan everyone, order the dirty ones out, chase the ones that bolt).
  - *NPC fighters* back off and block when hurt, circle instead of walking into punches, and pounce on anyone who's dazed. Fleeing NPCs dive away from grabs.

---

## Part 15g: Gang Beasts hands, climbing and moves

**Hands (like Gang Beasts):** LEFT mouse = left hand, RIGHT mouse = right hand. Tap = punch. **Hold** = reach out and grab whatever that hand touches: a person, or a **wall / ledge**. Each hand is separate, so you can hang on a wall with one hand and hold a person with the other.

**Climbing:**
1. Walk up to a wall and **hold** a mouse button. You grab it and hang there (as long as you keep holding).
2. **Hold SPACE** and click **LEFT, RIGHT, LEFT, RIGHT**... to climb. Same hand twice = you slip.
3. At the top, press **SPACE** to pull yourself up.
4. **Let go of both mouse buttons** to drop. (Z at a wall still starts a climb too.)
- Someone below punching you while you hang knocks your grip loose. Grab someone on a wall to yank them off.

**Moves:**
- **Dangle and drop:** hold someone out over a ledge, the bridge or the Wall (or while you hang off a wall). Let go = they fall, and a big fall knocks them out.
- **Ankle grab:** dive (Ctrl) with a mouse button held = grab their ankles and faceplant them.
- **Chokehold:** grab someone **from behind with BOTH hands**: their balance drains until they pass out. They escape by mashing Space or headbutting.
- **Headlock drag:** hold someone and walk: they come with you. Border Patrol can drag a Runner straight to the **catapult** this way.
- **Directional throws (Z while holding someone):** hold **S** = **SUPLEX** over your head, hold **W** = big long toss, nothing = normal throw.
- **Body splash:** dive onto someone who's down: they stay down 4 s longer.
- **Belly-flop slide:** dives slide further, and much further through water.
- **Floppy chains:** when someone goes down, whoever's holding them (or held by them) often goes down too.
- **Sumo shove:** run into someone with **both** mouse buttons held = SHOVE (off ledges, into the river...).
- Throw or shove someone into the deep river and the kill feed says you **sent them for a swim**.

---

## Part 15h: Checkpoint fixes, Border Patrol door, hats, physics feel

- **Scan someone illegal** and within 2-3 s they react: get out and fight, pull a knife, run, give up, or (rarely) **pull a GUN** and shoot at you (shooting back = self-defense). Guns are rare and show up on the scan as CONTRABAND: ILLEGAL GUN.
- **Order out (O / hold E)** now works on NPC trucks.
- Trucks only smash the gate in an emergency (the driver himself is busted), and the barrier is fixed after 12 s. A smashed barrier is really open now (no invisible wall).
- **Border Patrol door:** a locked steel door in the Wall just north of the gate. Border Patrol hold the key: E opens it for 4 s (careful: Runners can slip through while it's open).
- **Border Patrol** always wear the default uniform (no custom shirts/pants/colours). Glasses are still allowed.
- **Hats squash the hair** (afros etc. no longer poke through). Re-import the body and every SK_Hair_* file (they got a new Hair_UnderHat shape).
- **Mariachi** wear black long-sleeve shirts + black jeans.
- **Physics feel:** legs are floppy too now, muscles are looser, your upper body lags behind when you start/stop/turn, and solid hits rock you (short stagger). Tune in BR | Physics (Spine/Head/Arm Strength, Momentum Sway, Wobbly Legs).
- Fixed: NPCs appearing in cardboard boxes during sandstorms (and around detained people).

---

## Part 15i: Physics puppet, dragging, lifting, run-overs

**Nothing to set up for most of this.** Just update the code (copy Source, delete Binaries + Intermediate, Generate VS files, open the .uproject).

**Balance (Gang Beasts style)**
- Your whole body now leans like a real body on top of your feet. Hits, shoves and your own sudden stops/turns tip you; your legs pull you back up. Lean too far and you fall over.
- Hurt, dazed or drunk = wobblier. Blocking = feet planted (steadier).
- Hands that grab a person or a wall are physically tied to it, so arms stretch and bodies tug.
- Tune it: open your character Blueprint -> **Class Defaults** -> search **Ragdoll**:
  - **Active Ragdoll** (untick to switch all of this off if it ever misbehaves)
  - **Balance Stiffness** (higher = harder to tip), **Fall Over Angle**, **Hit Tip**, **Move Tip**
  - **Physical Grabs** (untick = hands not tied with physics)

**Dragging people**
1. Knock someone down.
2. Hold a mouse button on them. You grab the part nearest your hands (arm, leg, collar, head).
3. Walk. The floppy body gets **dragged** behind you (you walk a bit slower).
4. Z = fling them. Let go of the mouse = drop them.

**Lifting people over your head**
1. Grab someone with **BOTH** hands (hold both mouse buttons).
2. Press **SPACE** to heave them over your head.
   - Someone still fresh is too strong: wear them down first (punch them, chokehold them, or get them dizzy).
3. While they're up there:
   - **Z** = HURL them (big throw)
   - **SPACE** again = BODY SLAM
   - Let go of a mouse button = drop them in front of you
4. If they wake up up there, they wriggle down.
- Animation: import **Art/Characters/SK_PVV_Animations_Lift.glb** (Skeleton = your body skeleton). It makes **Lift_Overhead**, which fills itself into the **Lift Anim** slot (or set it by hand: Class Defaults -> search **Lift Anim**).
- Tune: Class Defaults -> **Drag Strength**, **Drag Speed Scale**, **Lift Seconds**, **Overhead Throw Scale**.

**Running people over**
- Hit someone with a car faster than walking pace and they go flying and get knocked down. Nobody dies. People already lying down tumble under the wheels.
- Tune it on the car/truck Blueprint: search **Run Over** (**Run Over Speed**, **Run Over Launch**).

**Cannon**
- Sitting on top of the barrel? Press **E** to light the fuse yourself (3 s, hold on tight). It only fires once the Line is open.

**Cantina fixes**
- **Drinking contest:** tap SPACE in a steady rhythm. Spamming it makes you **GAG** and lose progress (watch the GAG bar).
  - El Borracho is faster now, and gets faster every time someone beats him.
  - You can't enter while you're already drunk.
  - You can only enter once every 3 minutes. Prize is $110 ($50 entry).
  - Tune it on the BR Cantina: **Contest Prize**, **Contest Cooldown**, **Rival Rate Per Win**.
- **Bar fight is less chaotic:**
  - 6 locals.
  - At most 2 locals on any one player at a time; the rest pair off and fight each other.
  - Everyone goes for the nearest free opponent (no random cross-room charges).
  - All locals fight equally hard.
  - Tune it: **Max On One Player**, **Local Aggression**, **Patron Count**.
- Hits shove you a bit less (**Hit Push Scale** 0.55) so fights are less pinball-y.

**Note:** If your character Blueprint already changed a value (e.g. Hit Push Scale), it keeps your value. Click the little yellow reset arrow next to it to use the new default.

---

## Part 15j: Hair that looks like hair

All hairstyles (**SK_Hair_***) and beards (**SK_Face_***) were rebuilt:
- Locks and strand grooves in the shape.
- A ragged hairline instead of a clean "bandana" edge.
- A strand texture + normal map built into each file. Afros, curly tops and twists get a curly texture.

The textures are light grey, so your **Hair Color** slider still tints them like before.

**Re-import them:**
1. In the Content Browser, open the folder with your hair meshes.
2. Select all the **SK_Hair_...** and **SK_Face_...** meshes, right-click, **Reimport**.
   - If it asks for a file, pick the matching one from **Art/Characters/**.
3. Unreal makes new textures next to them (**T_Hair_..._D** and **T_Hair_..._N**). Keep those.
4. Open one hair mesh and check the material shows the strands.
   - **If it still looks smooth:** your hair is using an old material you made. Delete the hair meshes and drag the .glb files in again (Skeleton = your body skeleton, Import Morph Targets ticked).
5. Check that **Hair_UnderHat** and the face shape keys are still listed under **Morph Targets**. They are in the new files.

Before/after picture: **Art/Characters/Previews/hair_before_after.png**

---

## Part 15k: Sound fixes (distance, cantina, trumpets, footsteps)

Nothing to set up. Just update the code.

- **Distance:** every sound in the world now fades with distance, so you only hear what's near you. Roughly:
  - Footsteps, reloads and swings: about 12 m.
  - Punches and grunts: about 20 m.
  - Trumpets, dogs and game sounds: about 35 m.
  - Horns, guns and crashes: about 60 m.
  - Cannon boom: about 150 m.
- **Cantina music** only plays while **you** are inside the cantina. You hear a little of it in the doorway, and it fades out as you walk away.
  - Tune it on the BR Cantina: **Music Spill Distance** (0 = inside only).
- **Trumpets:**
  - A trumpet never starts its tune again while it's still playing.
  - If the Mariachi Band has its own **Music** song, the musicians just mime to it.
  - If it has no song, only one musician plays.
  - Hearing distance of the band: **Music Range** on the band.
- **Footsteps:**
  - A step plays the moment each foot touches the ground (not on a timer).
  - Steps only play near you. NPC steps are a bit quieter than players'.
  - Tune it in your character Blueprint: Class Defaults -> **Footstep Range**, **Footstep Volume**.
- **Tip:** for looping sounds (cantina music, the band song), open the sound and tick **Looping**.

---

## Part 15l: Gang Beasts movement + knockouts

Nothing to set up. Just update the code. Everything is tunable in your character Blueprint -> **Class Defaults** -> search **GangBeasts**.

**Moving**
- **Heavy and clumsy:** slower to get going, you slide a bit when you stop, and turns are wide and lazy.
  - Tune: **Heavy Acceleration**, **Heavy Braking**, **Heavy Ground Friction**, **Heavy Turn Rate**.
  - Untick **Heavy Movement** to go back to normal.
- **The body leans into where you walk** (more when you run). Getting knocked off that lean is what makes you fall.
  - Tune: **Lean Into Move**.
- **Floppy head and upper body:** they jiggle with every step and every hit.
  - Tune: **Step Bob**, plus **Head Strength** / **Spine Strength** under Physics (lower = floppier).
- **Zombie arms:** hold a mouse button and that arm goes straight out at shoulder height. Look up and the arms go up (grab ledges, people above you). Look down and they go down (grab people on the floor).

**Fighting**
- **Physics punches:** no canned punch animation any more.
  - The arm pulls back for a split second, then gets flung at the other person's head.
  - Running into a punch makes it harder. A punch can miss or glance off.
  - Tune: **Punch Windup**. Tick **Punch Anim With Physics** to also play the old animation.
- **The head is what matters:**
  - Clean punches to the **head** fill a hidden **KO meter**.
  - About 3–4 head shots knock you out. A headbutt counts extra.
  - Halfway there, you see birds and wobble.
  - Body shots mostly shove you around and do less damage.
  - The meter drains if you avoid getting hit for a few seconds.
  - Blocking stops it filling.
  - Tune: **Head Hit KO**, **Body Hit KO**, **Body Hit Damage Scale**, **KO Recover Rate / Delay**.
- **Real knockouts:**
  - A KO'd person goes completely limp with **z Z Z** over their head.
  - They stay down a few seconds longer (**KO Extra Seconds**), then get up **groggy**: slow, wobbly, can't punch for a moment (**Groggy Seconds**).
  - That's your chance to grab, drag, lift or throw them.

**Note:** if your Blueprint already changed **Head Strength** or **Spine Strength**, it keeps your old value. Click the yellow reset arrow to use the new floppier ones (110 / 340).

---

## Part 15m: Gang Beasts round two (waddle, get-ups, floppy throws, tug of war)

**Keep your animations ON.** They're the "brain": the physics muscles pull the floppy body toward the animation pose. Without them the body has nothing to aim for.

**1. New animations (import these!)**
- Drag **Art/Characters/SK_PVV_Animations_RoundTwo.glb** into the Content Browser (Skeleton = your body skeleton). It has 5 animations:
  - **Walk / Run:** a waddle. Short stompy steps, wide stance, hips swinging over the planted foot, leaning forward, arms out front flopping.
  - **CrouchWalk:** the same waddle, crouched.
  - **Idle:** wide stance, swaying like a drunk teddy bear.
  - **GetUp:** new. Pushing up off the floor.
- If your Blueprint's **Walk Anim / Run Anim / Idle Anim** slots point at the OLD ones, set them to the new ones. If the slots are empty they fill themselves by name.
- (The full pack **SK_PVV_Animations.glb** has all 33, including these.)

**2. Getting up:** when you come round after being knocked down, you push yourself up off the floor. You can't walk for about a second (**Get Up Seconds**), so that's a window to grab them again.

**3. Floppy throws:**
- Throwing someone (Z while holding them) sends them flying as a ragdoll.
- They get back up after about 2 seconds (**Throw Knockdown Seconds**), unless they were already knocked out.
- Throws are a bit stronger (**Throw Speed** 1500), and your running speed adds to them.

**4. Hits make you go limp:** a hard hit makes the whole body go floppy for a split second, then the muscles catch you (**Hit Flop Threshold**, **Hit Flop Seconds**).

**5. Tug of war:** when two people grab each other, whoever pulls harder drags the other. What helps:
- Using two hands.
- Having more balance (less beaten up).
- Walking backwards, away from them.
- Mashing **Space**.
- Wearing the Lucha mask.

If it's even, you're locked together, shoving back and forth. Turn it off with **Tug Of War**.

Everything is in Class Defaults -> search **GangBeasts**.

---

## Part 15n: Grab anything, bowling throws, lift camera, Gang Beasts climbing

**Grab whatever you touch:**
- Hold a mouse button (it starts grabbing after about 0.15 s) and your hand grabs whatever it touches:
  - **People**, even ones that aren't straight in front of you.
  - **Crates, chairs, bottles, planks.**
  - **Walls and ledges.**
- You keep holding it until you let go of the button. A prop you grabbed this way drops when you let go of both buttons.

**Throw people into people:** a thrown person knocks down anyone they slam into ("STRIKE!"), and *those* people can knock over the next ones too.

**Lifting camera:** while you hold someone over your head, the camera pulls back and up, and they're held a little further in front, so they're not in your face.

**Climbing (Gang Beasts style):**
- Grab a wall with your hands, then hold **both mouse buttons** to hang on.
- Hold **SPACE** to climb up, hand over hand. Look **down** while holding SPACE to climb down.
- **A / D** to shimmy sideways along the wall.
- Only **one** button held: you slowly slide down.
- Let go of both buttons (or press **Z**) to drop.
- At the top, **SPACE** pulls you up.
- Your arms get tired: about 20 s of hanging, less if you keep climbing. You get a warning when they start shaking.
- Your legs dangle and kick while you hang.
- You don't have to click left, right, left any more (it still gives a small boost if you do).
- Tune it in Class Defaults -> search **Climb**: **Climb Speed**, **Shimmy Speed**, **Climb Slide Speed**, **Climb Stamina Seconds**.

---

## Part 15o: Big batch 1 (combat feel, fixes, world)

Nothing to set up. Just update the code. What's new:

**Fighting**
- **Punch from your guard:** while blocking, click to punch. The guard drops for the punch and comes back up if you're still holding block.
- **Punches are straight jabs:** the arm pulls back, then the fist is driven out at their head and stays out for a moment (no instant snap back). Keep holding the button and the arm stays out, grabbing whatever it touches.
- **Swing attacks:** with your arms out, move the mouse and your arms swing round with it. Swing them into people to hit them (fast swings hurt).
- **Kicks (R)** are physical now: the leg goes loose and is driven out through the kick, then eases back.
- **Drop kick:** press R while in the air (jump first), or press the **dive key (Left Ctrl / mouse thumb button) without clicking**. You fly feet-first, knock down whoever you hit, and land on your back (up again after about 1.3 s).
- **Dive punch:** press the **dive key (Left Ctrl) while holding a mouse button**. Superman dive, fists first. The body goes flat out like a real dive.
- **Hands grab walls at more heights** (reach up for ledges).
- **Climb animation** is now used while climbing (it's "Climb" in the animation pack; it fills itself in).
- **Tune:** Class Defaults -> search **GangBeasts** (Punch Extend Seconds, Kick Extend Seconds, Swing Attacks, Drop Kick Speed / Down Seconds, Max Fling Speed).

**Fixes**
- **Gas:** holding a gas can next to a car that needs fuel = **Pour in gas (hold)** again (it beats getting in).
- **Flying too far:** knocked-down bodies fly at most ~5.5 m/s (**Max Knockdown Speed**) and throws at most ~7.5 m/s (**Max Fling Speed**). No more flying across the map.
- **Bumping people:** NPCs only fight back once you've really hurt them (a proper punch, or a few hard knocks), not from a bump. Tune with **Fight Back Damage** under BR | NPC.
- **Your smuggling clients** never fight you, and now they **defend you**: anyone who hits, grabs or carries off their coyote gets jumped.

**Luchador:** always faces you, charges faster, launches flying dive punches and drop kicks from a few metres away, and half the time picks you up off the canvas and throws you. His own drop kicks don't count as knockdowns.

**World**
- **Sprint bar:** sprinting drains stamina (about 7 s). Run out and you're winded (no sprint) until it refills a bit. The bar shows at the bottom of the screen. Tune: **Sprint Seconds**, **Sprint Refill Seconds**.
- **The road is legal:** walking or driving on the road to the checkpoint doesn't count as crossing the Line. Leaving the road (toward the river / Wall) or getting past the Wall does.
- **Spike fences along both sides of the road** from the border fence to the checkpoint (on the bridge too). To get at the river or the Wall from the road, cut through with **wire cutters** (hold E).
- **Floodlights** along both river banks and the whole Wall, so you can see people crossing at night.
- **NPC drivers** each have their own speed and drift around a bit in their lane, so players driving blend in.
- **Weapon store "ARMAS DON CHUY"** in town by the other shops:
  - **Knife $600:** doubles your punch damage. Select it to hold it.
  - **Pistol $1800:** contraband, and it shows up on a scan!

---

## Part 15p: New hairstyles and beards

**25 hairstyles:**
- **Fixed** (hair no longer comes down to the eyebrows like a helmet; real hairline, slightly higher at the temples): Afro, Buzz (now a soft short buzz), Caesar, Cornrows, LongWavy, Mullet, Pompadour, SidePart, SlickBack, Twists, Waves.
- **Real skin fades** (the hair gets sparser down the sides until it's skin): Fade, HighAndTight, CrewCut, FlatTop (now a proper flat block), CurlyTop (now real curls on top).
- **New:** Undercut, SoccerMohawk (the Mexican "fohawk": short faded sides, a soft ridge down the middle), ManBun, Ponytail, Spiky, Curtains (middle part), Dreads.
- **Balding** (horseshoe ring round the sides and back, bare shiny top) and **CombOver** (the same, with a few long strands combed across, fooling nobody).
- (The old spiky Mohawk was removed. Delete **SK_Hair_Mohawk** in Unreal if you imported it.)

**19 beards and mustaches (all rebuilt to hug the face and fit around the new lips):**
- Stubble (speckled, not a solid block), BoxedBeard, FullBeard, LongBeard, Goatee, VanDyke, SoulPatch, ChinStrap, MuttonChops.
- Mustache, Pencil, Walrus, Handlebar (the tips curl UP properly now), Horseshoe, FuManchu.
- **Connected** (mustache joined down to the chin): CircleBeard, ExtendedGoatee (runs out along the jaw), ChinStrapConnected, ThinGoatee (a neat thin outline).

**Import:**
1. Drag **all** `Art/Characters/SK_Hair_*.glb` and `SK_Face_*.glb` into your hair folder in Unreal:
   - Skeleton = your body skeleton.
   - Tick **Import Morph Targets**.
   - When it asks about existing ones, **Reimport / Replace all**.
2. Your **Hair Styles** / **Facial Hair Styles** lists fill themselves with every SK_Hair_ / SK_Face_ mesh (if you filled them by hand, add the new ones).
3. NPCs and bots pick from all of them.

Previews: **Art/Characters/Previews/Hairstyles.png** and **FacialHair.png**.

---

## Part 15q: Checkpoint & police tools

**The scan checks PAPERS only now.**
- No papers = ILLEGAL (same as before).
- Papers OK but carrying contraband or a weapon = "papers in order... but they look **NERVOUS**". You have to **search** them to find it.
- **Fake passport ($1000, "PAPELES CHUCHO" in town):** the papers scan says you're legal. It **fails** if you're carrying contraband (drugs / gun / package). A search takes it off you.

**Search (frisk) people:**
1. As Border Patrol, walk up to anyone standing.
2. **Hold E: "SEARCH them"** (2 s). It takes contraband, guns, knives and fake passports.
   - Found something: they're spotted, you get the seizure points, and NPCs fight / run / give up.
   - Found nothing on a clean citizen: small approval hit (harassment).
   - The same person can be searched again after 20 s.

**Search the vehicle:**
- Some NPC trucks have a **hidden stash**.
- Once everyone's out (order them out first), **hold E on the empty vehicle: "SEARCH the vehicle"** (3 s).
- Found = seized + points. Nothing = small approval hit.

**Security camera ($350 at the Border Patrol supply counter):**
1. Select it and use it (F) to put it up where you stand. It watches the way you were facing and slowly pans.
2. Any Runner who has crossed the Line walking through its view gets **spotted**, and it sets off a **BORDER ALARM**.
3. Runners can sneak up and **smash it** (hold E).

**Border alarms:** a siren **everyone** hears, a banner, and the spot pings on Border Patrol's map. They go off when:
- A camera sees someone.
- Someone climbs the **Wall**.
- Someone **cuts a fence**.
(At most one every 30 s.)

**Reinforcements ($1200, supply counter, 60 s cooldown):**
- Two NPC officers in uniform and patrol caps arrive.
- For about 2.5 minutes they chase the nearest Runner who crossed the Line, fight them Gang Beasts style, and **zip-tie** them once they're down.
- Then they leave.

---

## Part 15r: Prisoners

- **Zip-tied = searched:** the moment anyone (player or NPC) is zip-tied, their contraband, guns, knives and fake passports are taken. The nearest Border Patrol agent gets the seizure points.
- **NPCs sometimes wriggle free:** a zip-tied NPC left alone may try to break out, now and then (every few minutes at most, about 1 in 3 attempts is a real one). If it works they run for it.
- **Free them and they follow you:** untie a zip-tied NPC as a Runner (or break them out of the pen) and they follow you like a smuggling client. Get them to the Safe Zone for **$120**. They also defend you.

---

## Part 15s: Gadgets

All of these are sold in the Runner town by the other shops; the smoke bomb is also at the Border Patrol supply counter. Select the item and use it (**F**).

- **Smoke bomb ($150, both teams):**
  - Throws a thick smoke cloud about 5 m ahead that lasts about 14 s.
  - Anyone **inside** it can't be scanned, spotted or seen by cameras or drones.
- **Grappling hook ($350, reusable):**
  1. Look at a wall or fence (up to 25 m away) and use it.
  2. You zip up the rope and hang off the top. **SPACE** pulls you over.
  - It's noisy: you get spotted, and on the Wall it sets off a border alarm.
- **Lockpick ($250):**
  - **Hold E** on the Border Patrol door in the Wall to pick it (6 s). It opens for a few seconds, with a 40% chance of a border alarm.
  - At the holding pen door, the jailbreak is about **3x faster**.
- **Fake BP uniform ($1500):** use it to put it on (use again to take it off). While wearing it:
  - You look like an agent (uniform and patrol cap).
  - Scanners say "fellow agent", Border Patrol can't frisk you, cameras and NPC officers ignore you, and the BP door opens with no lockpick.
  - **Fighting** (punch, kick, dive, headbutt...) or **sprinting near Border Patrol** blows your cover: the uniform is gone and you're spotted.

(The wheelbarrow comes with the vehicles in the next batch.)

---

## Part 15t: Breaching & defenses

**The Wall is made of 4 m sections now** (about 37 of them).

**TNT ($900, Runner shop):**
1. Use it (F) to plant it at your feet. It beeps for 5 s.
2. BOOM:
   - Everyone within about 5 m goes flying and gets rocked.
   - Wall sections it was against are **blown out**: a hole you can walk through.
   - Fences nearby are wrecked.
   - A border alarm goes off.

**Border Patrol:**
- **Rebuild:** walk up to a hole in the Wall and **hold E** (5 s, $400 from the budget).
- **Wall reinforcement ($500, supply counter):** walk up to a section and **hold E** (4 s). It turns darker and survives **one** TNT blast (then it's normal again).

**Electric fences:**
- Border Patrol: **"Electrify the fences" ($800, supply counter, 90 s, then a 90 s cooldown)**. While live, touching a fence = **ZZZAP**: you're stunned flat for about 3 s. Trying to cut a live fence zaps you too.
- Runners: **Fuse kit ($300)** kills the power for **45 s** (the razor wire still hurts, so cut it or climb carefully).

**Spike strips + trap picker (Border Patrol):**
- Press **N** to choose which trap **B** places: Bear trap, Glue, Alarm wire, or **Spike strip**.
- Put a spike strip across a road: any vehicle that drives over it gets **flat tyres** for 25 s and crawls along at a quarter speed.

---

## Part 15u: Skin range (limit the skin slider)

Skin tones go from **0 = very light** to **1 = very dark** (see **Art/Characters/Previews/SkinTones.png** for every value).

**Set the allowed range:** Project Settings -> Game -> **Border Run** -> **Skin Range**:
- **Runner skin: lightest allowed / darkest allowed** (default **0.20 - 0.75**: light tan to dark brown).
- **Border Patrol skin: lightest allowed / darkest allowed** (default 0 - 1: anything).

The **Skin slider** in the Customize menu now covers *only* that range: all the way left = the lightest allowed, all the way right = the darkest allowed. NPCs (locals) and bots also stay inside the Runner range. (The game mode's NPC Skin Tone Min/Max can narrow it further.)

---

## Part 15v: Air & water (balloon, raft, anti-air, wheelbarrow)

**Hot air balloon ("GLOBOS PANCHO", launch pad north-west of town):**
1. A Runner pays **$1500** (E) and climbs in. A second Runner can hop in for free.
2. It lifts off after 8 s (or as soon as it's full).
3. It drifts over the border at about 22 m up. The first rider steers a little with the mouse (up to 40 degrees).
4. It lands on its own about 40 m past the Wall.
- Everyone can see it: the riders keep getting spotted.
- **Border Patrol can pop it:** a pistol shot hits the balloon, or use the **anti-air guns**. The riders **fall** (a big drop = knocked out).
- The balloon comes back to the pad a minute later.

**Inflatable raft ($80, next to the boat shop):**
- Like a boat but cheaper, 2 seats, orange.
- A bullet or anti-air round **pops it** and everyone ends up in the river.

**Anti-air guns (two, by the watchtowers):** Border Patrol **hold E** to fire at the nearest flying balloon or raft within 90 m. 70% hit chance, 8 s reload. If nothing is in range it tells you so.

**Wheelbarrows (two, by the town shops):**
1. **E** to grab the handles.
2. A teammate **E** to hop in.
3. You push them around a little slower than walking.
- **E** again to let go. If you get knocked down, it tips over.

## Part 15w: Dig tunnels (walk under the border)

Tunnels are **real**: there's no teleporting. You walk down into a pit, along an underground corridor, and up a ramp on the other side. The corridor is made of 5 m sections, 16 in all. Everything is built by code; you don't need to place anything.

There are two tunnel spots on the Mexican side, a little past the line, each with a wooden **TUNNEL** sign.

**Runners: digging**
1. Buy a **Shovel** ($80 at the town shop). **Shovels wear out:** a **SHOVEL** bar at the bottom of the screen shows how many digs are left. After 6 digs it breaks and you buy another one.
2. Stand by the TUNNEL sign and **hold E (4 s)** to break ground. A pit opens with a dirt ramp going down.
3. **Walk down the ramp.** At the end of the corridor there's a wall of dirt. **Hold E (3 s)** to dig the next section. Keep walking forward and digging.
4. If you dig all the way, the last dig breaks into a ramp on the USA side past the Wall. Walk up and out.
- Teammates can walk through without a shovel.
- While you're down there, Border Patrol can't spot you.
- If Border Patrol fill a section with dirt, you can **dig it back out** from the edge of the plug (face the dirt, hold E). That uses up shovel too.

**Border Patrol: digging to meet them**
1. When the Runners' digging passes under the river, the seismic sensors go off: an **alarm**, a **map ping**, and a pile of **fresh dirt** above the end of their tunnel.
2. An orange **SEISMIC ALERT** sign appears at their tunnel's exit spot on the USA side, past the Wall. **Hold E there** to break ground (no shovel needed).
3. Walk down and **hold E at the dirt** to dig towards Mexico, section by section, to meet them.
4. When the two diggings meet (or the Runners dig all the way through), the tunnel is **complete**. Everyone gets an alarm, and you'll probably have a fight underground.

**Border Patrol: filling it in** (only a complete tunnel)
- Stand at **either opening** (the pit or ramp on the Mexican or USA side), or anywhere inside, and **hold E: "FILL WITH DIRT"** (3 s).
- The first load of dirt goes in the **middle** of the tunnel. After that, every fill grows the dirt plug **one section toward both ends at once**. Keep filling from the opening and you never have to go down there.
- Because the plug grows outward from the middle, the open part always leads to an exit. Nobody gets trapped between two walls of dirt.
- When the plug reaches an end it **seals that pit**. When every section is full, the tunnel is gone.
- Anyone standing in a section you fill gets shoved out of it and knocked down.
- When **every** section is filled, the tunnel is gone and you get the seizure points. That spot can then be dug again from scratch.

## Part 15x: Checkpoint searches, NPC wall climbing, dives, sticky hands

**Checkpoint vans:** scan everyone in the van. Then **everybody gets out** and waits beside it.
- Hold **E** on each person to **search them for weapons**. Clean people get back into their seat. The van drives on once everyone is back in.
- Anyone with **no papers** doesn't wait long: after a few seconds they fight, run or put their hands up.
- If a search finds a weapon or contraband, it's taken and they react the same way.
- If nobody searches them for 2 minutes, they get back in by themselves.

**NPCs climb the Wall:** smuggling clients follow their coyote up and over the Wall, the same way players climb (no more teleporting after you). Suspects who **run** from the checkpoint head for the Wall and climb it. The **fighting kind** stay and fight you instead.

**Dives:** a missed dive no longer knocks you down. You just belly-flop and scramble back up. You only go down if you dive **head-first into something** (wall, car, fence): **BONK**.

**Sticky hands:** hold a mouse button to reach. If that hand touches anything (wall, car, fence, crate), it **sticks there** like in Gang Beasts, until you let go or walk too far away.

## Part 15y: Gang Beasts fighting and climbing (round 3)

**Hits only count when they land.** A fist or foot has to physically touch someone to do anything.
- **How hard** depends on how fast your hand or foot was moving **into** them, plus your own running speed. A slow tap does almost nothing; a fast swing or a full punch hurts.
- **Where** matters:
  - **Head:** big damage, and it fills the KO meter fast. A **hard, clean head shot knocks them down on the spot ("CRACK!")**.
  - **Body:** mostly shoves them back. A few weak body shots won't drop anyone.
  - **Arms:** mostly blocked.
- **Every hit shows:** the part that got hit snaps back (a hard head shot whips the head round), and your own arm bounces off them.
- **Punches** wind up, the shoulders turn in, the arm goes out to full length, and you take a small step into it.
- **Reaching arms** sit out in front, low and wide (gorilla style). Swing the mouse and they sweep through whoever's in front of you.
- **Kicks** aim at the nearest person's **leg**. If they're moving, or it's a hard kick, they're **TRIPPED**. Otherwise they limp for a second. A kick to the body is a big shove.
- Holding someone with one hand: your free hand still has to actually land a punch.

**Tuning** (BP_Runner / BP_Warden, **BR | Brawl**): Hard Hit Speed, Head Knockdown Power, Punch Reach, Punch Step, Arm Swing Max Speed.

**NPC drivers** slow down, stop and honk for people in the road instead of running them over. Only gate runners still floor it.

**Round 3b: more physics**
- **Grab body parts:** you hold whatever part your hand closed on (head, arm, leg, collar), and the physics tether is on that part.
- **Grab a leg and walk away** from them and you pull their feet out from under them: **LEG PULL!**
- **Shove:** hold your arms out and walk into someone (too slow to be a hit) and you **push them back**. It's how you get people off the bridge, the Wall or a truck. It pushes harder the faster you're going, and even harder if they're blocking.
- **Rocked:** when your KO meter is high you **stumble about**. That's the moment to grab them.
- **NPC haymakers:** brawling NPCs wind up to one side and **swing both arms through you**. Like player swings, they only hurt if they land.

- **Grab what you touch:** hold a button to reach (players). If your hand touches **any part** of someone (their arm, head, collar, leg), you grab that part. You don't magnetically grab people who are just near you.
- **Punch direction (Gang Beasts style):** a punch goes in the **general direction you're facing/aiming** and **locks onto whoever's roughly in front of you** (inside about 45°), going for the head. Nobody there: it goes straight where you aim. Holding the button just puts your arm out that way (reach / grab / climb). Swinging the mouse is the sweeping weapon.

**Hands (round 4):**
- **Tap** a mouse button = **punch** with that hand (thrown when you let go).
- **Hold** a mouse button = that **arm stays out** for as long as you hold it, and the hand **glues** to whatever it touches: a person (the part you touched), a crate, a wall. Let go of the button to let go. Losing a grip doesn't drop your arm; it glues to the next thing it touches.
- Someone who wriggles free (mashing Space) can't be re-grabbed for about a second.
- **Climbing:** hold both buttons on the wall and hold **W** (or look up) to pull yourself up. While hanging, let go of one button and hold it again: that hand reaches **up the wall** for the next hold, hand over hand. **Space** = jump up from your hands.
- **Tuning:** BP_Runner / BP_Warden → **BR | Brawl** → **Hold To Reach Seconds** (0.18): how long a press has to be to count as a hold instead of a punch.

**Round 5: Gang Beasts pacing and NPC brains**
- **Pacing:** each hand can throw a punch about every 0.4 s (no machine-gun clicking). NPC fights breathe a little more too. Tune it with **BR | Brawl → Punch Cooldown**.
- **Body checks:** run into someone at full speed and you both stagger (they get the worst of it). If they're already rocked, they get **bowled over**.
- **NPC fighting styles** (each NPC gets one at random):
  - **Brawler:** rushes in swinging.
  - **Grappler:** grabs you, **drags you towards the river**, and throws you in.
  - **Sneak:** circles round behind you and hits you when you're not looking (or when you're busy with someone else).
  - **Coward:** keeps their distance with the guard up and only darts in when you're busy or groggy.
- NPCs **back off and block when they're rattled** (high KO meter), not just when they're nearly out.
- NPCs who knock you down and pick you up **drag you towards the water** before throwing.

**Round 6: real guard, smarter fights**
- **Blocking is physical:** your **forearms come up in front of your face**.
  - A punch that lands **on those arms (or the face behind them) from the front is BLOCKED**: no damage, it only wears down your guard meter. Hard punches wear it down more. Your guard sparks and rocks back, and their fist bounces off.
  - **Body shots slip under the guard**, and hits from the **side or behind** get through. Keep turning to face them!
  - Too many blocked punches and it's a **GUARD BREAK** (staggered, can't block for a moment).
- **Smart targeting:** punches lock onto the **head**, but if the target's guard is up they go for the **body** instead.
- **NPCs fight like a real fight (but arcadey):**
  - They **step in and throw a few**, then **back out of range** with the guard up, circle, and go again.
  - When you wind up a punch they may **block** or **slip to the side and counter**. Cowards block the most, brawlers the least.

**Climbing (players):** there's no climb mode any more.
1. Jump at a wall and **hold a mouse button**: your hand sticks.
2. **Your hands hold you up like ropes.** With two hands you hang. With one hand you slowly slide down.
3. **Look up** to pull yourself towards your hands. When your shoulders reach your hands, let go of one button and grab again higher: hand over hand.
4. **Your grip wears out** the longer you hang, faster on one hand.
5. **Space while hanging** is a real jump that pushes you up and in towards the wall. You still have to make it over the top yourself, or you fall.
- The map's **outer walls can't be grabbed**.
- NPCs and bots still use the simple climb.

## Part 15z: Blood, wounds and death (knives and guns)

Punches, kicks and throws **never kill**: they knock you down and out. **Knives and bullets do real damage:**
- **Blood:** red spray where it went in, and a dark puddle on the ground. Badly hurt people drip a blood trail for a while.
- **Health:** a red **HEALTH** bar appears once you're wounded. It heals slowly once you've stopped bleeding (about 15 s after the last wound).
- **How much damage:**
  - **Knife:** only hurts when the stab actually lands (same contact rule as punches). 2 to 5 stabs kill, depending on how hard they are.
  - **Bullet:** about 2 shots kill.
  - **Headshots** (stab or shot) do double.
- **Who it hits:** works on **players and NPCs**, with any real gun: a Runner's pistol, an NPC's gun, or Border Patrol in **LETHAL** mode. The Border Patrol taser stays non-lethal.
- **Dead players** lie there as a floppy body that can be dragged around, then **respawn after 12 seconds**. **Dead NPCs** stay for 45 seconds, then they're gone.
- The kill feed says who **SHOT** or **STABBED** who. Border Patrol's scandal / approval rules for shooting people still apply.

**Optional:** Project Settings → Border Run → Effects → **Blood Effect**: drop in a Niagara blood system to replace the built-in red droplets.

**Tuning** (BP_Runner / BP_Warden, **BR | Wounds**): Knife Wound, Gun Wound, Head Wound Multiplier, Death Respawn Seconds, NPC Body Seconds.

## Part 15aa: Fight pacing, real grips, climbing, doors, village life

**Fights last longer and are fairer:**
- One clean, hard punch to the head fills about a third of the knockout meter, so it takes about 3 or 4 good head shots to put someone out.
- The KO meter drains back faster once the hits stop.
- Only a near-perfect head shot drops someone in one punch.
- A **headbutt** only rattles you badly if the **heads actually meet**. Into the chest, it's just a shove.
- NPCs fight back after the **first** real hit (they used to need several).
- NPCs fight at **arm's length** instead of nose to nose. When they've got hold of you, they mostly punch and haul you around, and headbutt rarely.

**Real grips:**
- Your hands go to the **part you grabbed** and are physically tied to it, standing or limp.
- Dragging a knocked-out body pulls it by the part in your hand, so your hand is really on it.
- A hand **inside** someone's body (pressed against their back, chest, anything) counts as touching them.
- Holding a wall or a pole: your arm **holds you there**, so you can't just walk away while you grip it, and the grip doesn't slip when you move.

**Climbing:**
1. Put both hands on the wall above you and hold **W** (or look up): your arms **haul you up off the floor**.
2. Let go of one button and hold it again: that hand reaches **up the wall** for a new hold.
3. Keep pulling. **Space** jumps up from your hands.

**Dives:** **Ctrl is always a dive** now. The drop kick is **kick while you're in the air**. Landing from a dive can't make you "lose your balance" and fall over any more.

**Village:**
- Villagers go on **errands** (market, taco stand, church, well, shops) and stop to chat and wave.
- New grabbable, throwable clutter: **barrels**, **clay pots** (smash like bottles) and **sacks of grain**.

**Buildings:**
- The taco stand has a roof and an awning.
- The shops have **lights inside** and **doors that swing open when you walk into them** and shut behind you.
- The cantina has real **saloon doors** that flap open as you push through.

**Day and night:**
- Each round starts at a **random time of day** (usually daytime) with **random weather**.
- A full day is now 14 minutes, and **nights pass 2.5× faster**.
- Tune it in BP_BRGameMode under **BR | World**: Day Length Minutes, Night Speed Up, Random Start.

---

## Part 15ab: Controls cheat sheet, Border Patrol training yard, gyms

Nothing to plug in. Everything here is built from code and appears in the test map.

### Controls (also shown at the bottom of the screen in game)

| Key | Everyone |
|---|---|
| Left / right click | That hand: **tap** = punch, **hold** = reach out and stick to whatever it touches |
| Z | Grab / throw |
| Left Alt | Block |
| Left Ctrl | Dive |
| Middle mouse | Headbutt |
| Space | Jump / climb |
| Mouse wheel | Switch item (the yellow line under your items says what **F** does with it) |
| F | Use the selected item (eat a taco, throw a **smoke bomb**, place a **security camera**...) |
| G | Drop the selected item |
| E | Interact (hold it for jobs, digging, cutting wire, the gym) |
| T (hold) | Emote wheel |
| M | Menu |

| Key | Border Patrol only |
|---|---|
| V | Scan someone (papers and contraband) |
| B | Place a trap. **N** changes which trap (Bear Trap, Glue, Alarm Wire, Spike Strip). The bottom line shows the one picked. |
| L | **Look through your security cameras.** Press L again for the next one. Press L past the last one, or walk, to go back to yourself. |
| Q | Drone |
| X | Fists / taser / gun |
| O | Order everyone out of a truck |

Runners: **Q** whistles for your smuggling clients.

**Hard hat:** you take 40% less damage from hits. You unlock it by knocking out 25 people, or Border Patrol can requisition one at the supply counter.

### Border Patrol training yard (at the base, east of the supply counters)

Everything pays the **government budget**. Full pay during prep, half once the Line opens.

- **Shooting range:** stand behind the yellow line and shoot the pop-up targets. Taser mode counts too. A hit pays +$20 and a bullseye pays +$45.
- **Sparring mat:** hold E to call out a recruit for a fist fight. Knock him out for +$150. Every win in a row makes the next recruit tougher and pays $50 more, up to $400. If you lose, walk away, or run out the 90 seconds, the streak resets.
- **Duty desks** (hold E):
  - File Reports: +$50
  - Wash Patrol Trucks: +$80
  - Radio Dispatch: +$60
- **Press Conference** (west of HQ): hold E to win **+6 approval**. It has a 40-second cooldown.

### Approval comes back now

- If nobody on Border Patrol causes a scandal for 30 seconds, approval climbs about 6 points a minute.
- Good catches now give +5, up from +3.
- When approval climbs back over 60, the drone comes back. Over 30, you get your guns back. The announcer tells everyone.

### Gyms (stronger and faster, up to 5 levels each per match)

- **Weight bench = STRENGTH:** +10% knockout power per level, and you take knockout hits about 6% better per level.
- **Treadmill = SPEED:** +4% run speed per level, so +20% at level 5.
- **Border Patrol gym:** free, next to HQ.
- **Mexico gym** ("Gimnasio El Toro", west side of town): you need a **$600 membership** from the front desk first.

### Smarter NPCs, dogs and traffic

- Illegals and smugglers who make it over the Wall now **run for the Safe Zone**, dodging Border Patrol, instead of hanging around.
- **Dogs:**
  - They run to help any Border Patrol agent who's being attacked.
  - They chase anyone scanned as illegal or carrying contraband who runs or fights, and drag them down so you can zip-tie them.
  - They stand guard and bark at busted people who are just standing there.
  - They still sniff out Runner players who crossed the Line.
- **Border line:**
  - A new truck shows up every 28 to 50 seconds, and never while 3 are already waiting in line.
  - NPC drivers brake well back from the barrier and only smash it when they're deliberately running the gate.
  - The barrier stays up until the truck has rolled through.

If your **BP_BRGameMode** has its own values for *NPC Truck Every Seconds* or *Max NPC Trucks*, those win. Reset them with the yellow arrow to get the new defaults.

## Part 15ac: Bribes, box trucks, duels, effect sizes

Nothing to plug in.

### Effects too big? (the giant sand swirl)
**Edit → Project Settings → Border Run → Effects** now has a size slider for every effect, for example **Dust Effect Size** (default 0.3). Lots of free effects are made huge, so turn them down there. Untick **Dust Effects** to turn dust off completely. A normal dive landing doesn't make dust any more, only big falls and landings do.

### Treadmill
Each SPEED level also gives you 15% more sprint.

### Bribes at the checkpoint (Border Patrol)
- Sometimes a truck with someone dirty aboard offers a bribe ("psst... $350?"). It doesn't happen every time.
- Hold **E** at the truck to take it. The money goes to the budget, and the truck drives through with whoever's aboard, so the Runners can still score off them.
- Two **security cameras by the gate** pan side to side. If one is pointed at you when you take the money, it's a **SCANDAL**: you lose 20 approval and the bribe money as a fine.
- The middle of the screen tells you **ON CAMERA** or **looking away: hold E NOW**. Time it.
- Runners can smash those cameras.

### Box trucks
- **Mexicans:** the **SMUGGLING TRUCK JOB** board is on the south edge of town. Hold E and you get:
  - a box truck with its back doors open,
  - 4 smuggling packages,
  - 5 crates,
  - 2 clients.
- **Loading it:**
  1. Grab the packages (**Z**) and throw them in the back.
  2. Pile the crates in front of them.
  3. Your clients walk up the ramp and hide in the back by themselves.
  4. Close the doors (**E** at the back) and drive to the Safe Zone.
- **Pay:** $400 per package that arrives in the back, plus what the clients pay. Each package Border Patrol seize costs you $300.
- **Doors:** the two back doors swing on real hinges. They flap open further when you speed up, swing toward shut when you brake, and swing apart when you turn. People walking into them push them. Once you close them they stay latched until somebody opens them.
- **Cargo:** while the truck is moving, everything in the back is locked down. When it stops it tumbles loose, so you can dig through it.
- **Border Patrol:** about 1 in 4 NPC trucks is a box truck. The driver will scan clean, so open the back (**E**), climb up the ramp, shove the crates around and grab the packages. Grabbing one **seizes** it for +$200 budget. Anyone hiding back there jumps out to fight, run or give up. The NPC driver won't leave while the back is open.

### High-noon duel (west end of town, past the welcome sign)
1. Hold E at the duel post. You need at least $200.
2. You both stand on your marks for 3... 2... 1... then a random pause... **DRAW!**
3. Your revolver comes out: **right mouse** to aim, **left click** to fire. One hit kills.

- **Win:** your money is doubled, plus $1,500. Every pistolero after that draws faster.
- **Lose:** you're dead, and he takes ALL your money and everything in your pockets.
- **Fire before DRAW:** you're shot.
- **Walk off your mark:** you're a coward and he takes your money.

## Part 15ad: Border Rush (horde event)

Nothing to plug in. This only happens on the built-in test map.

- **How often:** every 2½ to 4½ minutes once the Line is open.
- **Warning:** the announcer shouts **BORDER RUSH!** and pings the spot on the map. Ten seconds later, **10 to 14 people** splash out of the river and charge the Wall together.
- **What they do:** they weave around Border Patrol, climb the Wall, then run for the Safe Zone. About 1 in 4 fights back if you grab them.
- **Counter:** the top of the screen shows how many are still loose.
- **Border Patrol:**
  - Run them over with a truck. It knocks them flat but never kills them.
  - Tase them and zip-tie them.
  - The **dogs** go straight for them and drag them down.
  - Traps, glue and the spotlight all help too.
  - Every one you catch counts like any other catch.
- **Runners:** run in with the crowd. You blend right in, and Border Patrol have their hands full.
- **Scoring:** every rusher who reaches the Safe Zone scores for the Runners.
- **Tuning:** **BP_BRGameMode → BR | Border Rush** has *Border Rush Every Seconds*, *Border Rush Size* (set it to 0 to switch rushes off) and *Border Rush Warning*.

## Part 15ae: Shooting from vehicles

- **Driving:** if you have a gun (Border Patrol always do; Runners need the Illegal Gun):
  - **Hold right mouse** to aim out of the window. The camera pulls in and a crosshair appears.
  - **Left click** to shoot at whatever's under the crosshair.
  - Shooting while you drive is less accurate.
  - When you're empty, the next click reloads.
- **Passengers:** same as on foot.
  1. Press **X** to get your gun out.
  2. **Hold right mouse** to aim.
  3. **Left click** to shoot.
- The bullets never hit your own vehicle.

## Part 15af: The checkpoint (you decide every truck)

Nothing drives through on its own. At the truck:

1. **Scan everyone (V).**
2. **They get out by themselves** as soon as everyone's been scanned, and wait beside the van. (If the driver offers a bribe, you have about 12 seconds to take it first.)
3. **Search them.** Either hold **E** on each person, or hold **E** at the truck to **pat everyone down** at once.
   - **Contraband:** they hand it over (seized) or fight or run.
   - **Clean, with papers:** they get back in the van.
   - **Clean, no papers:** they stay outside, waiting.
4. **Hold E: DENY ENTRY.** This only affects the people with no papers.
   - Most walk back to Mexico.
   - Some run for it.
   - Some swing at you.
5. **Hold E: WAVE THEM THROUGH.** Anyone left outside gets back in, the barrier lifts, and you get the cleared-truck budget.

**Box trucks:**
- **What's hidden:** contraband sits at the very front of the hold behind a wall of ordinary crates: **white bricks** (cocaine) and **olive green crates** (guns). Open the back, climb up the ramp, shove the crates aside and grab it.
- **When you grab it:** it's seized, and **everyone in that truck is busted**. They jump out to fight, run or give up, and arresting them counts as a good catch.
- **Cleanup:** when a box truck leaves (or a Runner's job truck is delivered), every crate and package it brought disappears with it, even ones you dragged out.

## Part 15ag: Climbing, grips, kicks and throws (Gang Beasts tuning)

- **Climbing:**
  - Hold a click so a hand sticks to something above you, then **HOLD SPACE** to heave your body up.
  - Two hands pull fast; one hand pulls slowly.
  - When your shoulders reach your hands and there's a top (a wall, a roof, a ledge), keep holding Space and you haul yourself **over** it.
  - If there's no top yet, reach up with one hand to re-grab higher, then the other.
- **Grip:**
  - Hanging on two hands lasts about 45 seconds (it used to be about 16); one hand lasts about 15.
  - It refills quickly once your feet are on the ground.
  - Holding a person doesn't break until you're 4.8 m apart.
  - Wriggling free takes about 22 mashes against two hands, or 13 against one.
- **Thrown bodies:**
  - A body flying into you shoves you and makes you stagger ("OOF!").
  - Only a really fast one, or one hitting someone who's already rocked, bowls you over ("STRIKE!"), and you get back up quickly.
  - Chain reactions only happen with really fast throws.
  - Falls have to be higher (4.5 m) to knock you flat.
- **Kicks:**
  - The foot reaches further and the hit window is longer.
  - You step into the kick, and even a scrappy kick lands with real weight.
  - A boot to the chest sends people stumbling back.
  - A leg kick trips anyone moving or already wobbly; otherwise it gives them a dead leg.
  - A hard head kick drops them.
  - You can **boot someone who's on the floor** and send them sliding.
- **Drop kick** (kick while in the air):
  - You fly feet-first and land back on your feet.
  - It no longer knocks *you* down.
  - The people you hit still go flying.
- **Duel:** you get a loaner revolver for the duel only. It comes out at DRAW and goes away when the duel ends.
- **Mexico gym:** moved west so it isn't inside a house any more.

## Part 15ah: More Gang Beasts moves

- **Charge punch:** punch while sprinting. You lunge in with your whole weight behind it, and it sends them flying back ("CHARGE!"). If they're already rocked, it drops them.
- **Uppercut:** crouch (C) and then punch. You spring up into it and their head snaps back ("UPPERCUT!"). It's a big hit to the KO meter.
- **Punch them off a ledge:** hit someone who's hanging by their hands.
  - Every punch loosens their grip.
  - A fist that lands on a **hand** knocks that hand off, and it takes them a moment to grab on again.
  - Keep going and they fall.
- **Wall slam:** a body thrown hard into a wall, truck or post gets rattled ("WALL SLAM!"). It's a big hit to the KO meter, and they stay down a bit longer.
- **Trip:** running flat out into something low (a crate, a sack, a low wall, someone lying on the floor) sends you over it ("TRIP!"). You're back up in about a second.
- **Recruits** at the sparring mat now wear the Border Patrol uniform and cap.

## Part 15ai: Fighting out of holds, swinging, vehicle cameras

- **Grabbed by someone?** Fight your way out:
  - Punching them, kicking, or (best) **headbutting** them loosens their grip.
  - Mashing Space still works too.
  - Break free and they stagger back ("BROKE FREE!").
- **Hanging by your hands:**
  - Press **A / D** (or **W / S**) to swing.
  - Build up a swing and let go to leap, or kick someone while you swing.
  - Kicking while you're hanging is a normal kick; it doesn't drop-kick you off the ledge.
- **Vehicle cameras** (raft, boats, trucks, and passengers too): the people riding with you no longer pull the camera into the back of their heads.

## Part 15aj: Fights: holds, fatigue, ground-and-pound, face plants

- **Holds matter:**
  - Grab someone with **both hands** and they're **pinned**: their punches and kicks don't work. They can only wriggle (mash Space) or **headbutt** you.
  - With **one hand** on them, they can still swing, but their punches are about half as strong.
  - The screen tells you when you're pinned.
- **Arm fatigue:** throwing flurries tires your arms. Punches slow down and get weaker, then recover after a couple of seconds of not punching. Mashing loses to timing, grabbing and throwing.
- **Keep them down:** punch someone who's on the floor to keep them there ("STAY DOWN!").
  - Each hit adds a little less time, so you can't keep anyone down forever.
  - NPCs only do it now and then.
  - Kicking someone on the floor still punts them across the ground.
- **Face plant:** hold someone in both hands and walk them into a wall, truck or post. Their face meets it, which is a big hit to the knockout meter. NPC grapplers do this to you too.
- **Aiming for the head:**
  - Punches lock onto the **face**, and a fist near the face counts as a head hit more reliably.
  - Look **down** at someone's belly to go for the body instead.
  - Players can now punch straight into a raised guard if they want to.

## Part 15ak: Sticky hands (glue)

- **What sticks:** hold a click and that hand sticks to **anything** it touches: walls, fences, the truck, a door, a barrel, a crate. That includes things your hand just brushes against, not only what's straight ahead.
- **Your hand stays put:** the hand is pinned hard to that spot and your body swings around it.
- **Moving things:** glue onto a truck or a swinging door and your hand goes with it (yes, you can hang off a moving truck).
- **Light things:** glue onto a crate, barrel or sack (under about 70 kg) and it comes with your hand as you move, so you can drag stuff around.
- **Letting go:** release the button. The hand also slips off if you get yanked more than about 3 m away from your hold (thrown or knocked flying).

## Part 15al: Mexican police (Policia) and the carcel

This only exists on the built-in test map.

- **Who they are:** 3 Policia officers in blue uniforms patrol town. The **CARCEL** (jail) is a stone cell with iron bars in the open north-west corner of town.
- **What they care about:** only **crimes against Mexico**, meaning hurting townspeople or the Policia **in town**:

  | Crime | Heat |
  |---|---|
  | Stabbing | +40 |
  | Shooting | +50 |
  | Killing | +70 |
  | Knocking out someone who wasn't fighting you | +20 |
  | Each punch on a citizen who wasn't fighting you (assault) | +10 |
  | Knocking a citizen down so they drop their stuff (robbery) | +15 |
  | Hitting a cop | 1.6 times as much |

  - Crimes against Border Patrol, and the Border Patrol wanted level, are a different system. **The Policia ignore them.**
  - Fist fights don't count. Neither does defending yourself, or a local jumping a Border Patrol agent.
- **A cop has to SEE it:** a Policia officer within 35 m with a clear line of sight. That goes for every crime, murder included. If no cop saw it, nothing happens.
- **Stars:** 40 heat = 1 star, 100 = 2 stars, 160 = 3 stars. The top of your screen shows them.
- **Losing the heat:** lay low for 30 seconds and it starts dropping, faster if you leave town.
- **Arrest:**
  1. They chase you and fight you.
  2. Once you're down, a cop **grabs you and drags you to the carcel** (mash Space when you come round to try to get loose).
  3. You're locked in the cell for **30 seconds (1 star), 1 minute (2 stars) or 1.5 minutes (3 stars)**, with a fine of up to $300.
  4. The cell is sealed and you can't climb out. When your time is up you're let out the front.
- **Troublemakers:** about 15% of the locals pick fights and mug people. If the Policia see it, they arrest **them** too. People defending themselves are never arrested.
- **Respawns:** dead townspeople are replaced about **3 minutes** later, and dead Policia about **2 minutes** later.
- **Tuning:** **BP_BRGameMode → BR | Town** has *Policia Count*, *Villager Respawn Seconds*, *Policia Respawn Seconds* and *Troublemaker Chance*.

## Part 15am: Uniforms (Policia, mariachi) and house burglary

**New clothes (import them like the other outfits):**
- **Files:** in `Art/Characters/Outfits/`, import **SK_Uniform_PoliciaShirt**, **SK_Uniform_PoliciaPants**, **SK_Uniform_MariachiJacket** and **SK_Uniform_MariachiPants** into the same folder as your other clothes, with the same skeleton.
- **They fill in automatically by name** (Class Defaults → BR | Look → *Policia Shirt Mesh* and the others).
- **Who wears them:**
  - The **Policia** wear the navy POLICIA uniform: badge, "POLICIA" on the back, duty belt.
  - The **mariachi band**, and **anyone holding a trumpet** (players included), wear the black charro suit: silver buttons and studs, a red bow tie and a white shirt. Put the trumpet away and your own clothes come back.
- **They aren't in the clothing menu.** Nobody else can pick them.
- **If you haven't imported them yet:** those characters' normal clothes are just tinted in the uniform colours.

**Burglary:**
- **Where:** every village house has a brass lock on its front door. With a **Lockpick**, hold E for 4 seconds to break in and rob the place.
- **What you find:** cash (up to $260), or a random item. Now and then you find a revolver.
- **Your lockpick:** it snaps 30% of the time.
- **Restocking:** each house takes 4 minutes before there's something to steal again.
- **It's a crime against Mexico,** but only if a Policia officer sees you do it (+45 heat).

## Part 15an: Self-defence and the evidence table

- **Not a crime:**
  - Beating up the mugger who robbed you to get your stuff back.
  - Fighting someone who's mugging you.
  - Roughing up someone the Policia already want.
- **Evidence table:** when the Policia arrest someone (player or NPC), everything in their pockets goes onto the **EVIDENCE** table at the station, beside the cell. That includes anything they'd stolen.
  - The sign shows how many things are on it.
  - Hold E at the table to **steal it all back** (as much as your pockets hold).
  - That's a crime (+35) if a cop sees you.

## Part 15ao: Disguises

- **Fake BP Uniform:** wearing it puts you in the **real Border Patrol uniform**: the green shirt, the black vest with "BORDER PATROL" on the back, the green pants and the patrol cap. It fills itself in from SK_Outfit_PatrolUniform and SK_Outfit_PatrolPants. Take it off (or get caught) and your own clothes come back.
- **Fake Passport:** now **$300** and **single-use**. It gets you through **one** papers scan, then it's gone. After that you count as having papers for about 5 minutes, so you won't be turned away at that checkpoint. It still doesn't hide contraband.

## Part 15ap: Busted box trucks, simpler checkpoint, Border Rush and dog tuning, menu

- **Box trucks:** if you open the back and find **people hiding** or **contraband**, the whole truck is **busted**.
  - **Everyone** from it is denied entry and can be arrested: the driver, the passengers, people already standing outside for the search, and the stowaways.
  - They fight, run or give up, and arresting them counts as a good catch.
- **Checkpoint:** the "pat down everyone" option at the truck is gone.
  - Search people one at a time by holding E on each.
  - The truck itself only offers **Deny entry** (for the ones with no papers) and **Wave through**.
- **Border Rush:** smaller crowds (5 to 8 people), less often (every 3½ to 6 minutes). The **Wall is hard for them**:
  - Only about 1 in 4 attempts gets someone climbing.
  - The rest jump at it, slide off and stand there catching their breath (easy to grab).
  - The ones who do get on climb at about half speed.
  - Tune *Horde Climb Chance* on the NPC (BR | NPC).
- **Dogs:**
  - A little slower.
  - They steer around walls, trucks and crates.
  - They **give up** after chasing someone for 15 seconds without catching them, and leave that person alone for a bit.
  - Under half their bites knock you down, and you get back up faster.
  - They ignore the Policia.
- **Menu:** the hosting and joining buttons are removed (you're adding your own).

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
- [ ] **Sounds**: every slot, including Gate Smash, Cantina Music, Bar Fight, Drink, Rain Loop, Wind Loop, Thunder, **Night Crickets Loop, Town Chatter Loop, Desert Wind Loop, Distant Dog Barks**
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
