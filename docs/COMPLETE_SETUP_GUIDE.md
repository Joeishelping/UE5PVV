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
4. Keep the same skeleton.

### 2b. Hair, beards, hats, poncho
1. In the Content Browser, make a folder **Characters → Wearables**.
2. Drag in all of these files:
   - every `SK_Hair_*.glb` (15 hairstyles)
   - every `SK_Face_*.glb` (8 beards and mustaches)
   - `SK_Hat_Sombrero.glb`, `SK_Hat_HardHat.glb`, `SK_Hat_CannonHelmet.glb`, `SK_Hat_LuchaMask.glb`
   - `SK_Outfit_Poncho.glb`
   - from `Art/Characters/Outfits`: `SK_Outfit_CowboyHat.glb` and `SK_Outfit_PatrolCap.glb`, if you haven't imported them already
3. In the import window:
   - **Skeleton** = your body's skeleton. This is the important part; without it the hats float.
   - **Import Morph Targets** ✓
   - Click **Import All**.

### 2c. Trumpet
Drag `SM_Trumpet.glb` into the same folder. It's a normal static mesh, so the skeleton doesn't matter.

### 2d. Animations
1. Make a folder **Characters → Animations**.
2. Drag in `SK_PVV_Animations.glb`.
   - **Skeleton** = your body's skeleton
   - Tick **Import Animations**
   - Click **Import**.
3. You get one animation per move: Idle, Walk, Run, CrouchIdle, CrouchWalk, Fall, Swim, Seated, Drive, Restrained, Struggle, Carried, Work, Aim, AimWalk, Punch, Fire, Reload, Eat, Dance, Trumpet, Emote_Wave, Emote_Flex, Emote_Taunt, Emote_Beg, Emote_Cheer.
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
3. Leave a slot empty if you don't have that animation. Your character just keeps the closest one it has.

### 3b. Hairstyles and beards (BR | Look)
- **Hair Styles**: click **+** once per hairstyle and pick the `SK_Hair_...` meshes. The order here is the order you scroll through them in the menu.
- **Facial Hair Styles**: the same, with the `SK_Face_...` meshes.
- If your old hair or hat is set on the **OutfitHead** or **OutfitFace** component, set that component's Skeletal Mesh to **None**. Otherwise you'll wear two.

### 3c. Hats (BR | Hats)
**Hat Meshes**: click **+** six times and fill in these rows:

| Left box | Right box |
|---|---|
| Cowboy Hat | SK_Outfit_CowboyHat |
| Sombrero | SK_Hat_Sombrero |
| Patrol Cap | SK_Outfit_PatrolCap |
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

---

## Part 6: What's on the test map (built automatically)
| Where | What |
|---|---|
| Village (west) | jobs, shops (ladder, cutters, gas, tacos, **poncho**, **trumpet**, **shovel**, **cannonball suit**), coyote counter, cockfight, piñata, **chickens**, villagers |
| Open desert by the scrapyard (south-west) | the **Lucha Libre ring** with its red challenge counters and green/blue betting counters |
| Near bank | Human Cannon, tunnels, marina |
| Border Patrol side | 2 **watchtowers**, the Holding Pen and catapult, blue **GOV. SUPPLY** counters |

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
| **Left click** | punch (alternates fists, aims at whoever's in front of you) |
| **Left Ctrl, Left Alt or mouse thumb button** | **dive / tackle** |
| Z | grab people or things; Z again to throw; on a teammate: climb on their shoulders |
| E | interact (hold for jobs, zip-ties, ladders) |
| Mouse wheel / F / G | pick an item / use it / drop it |
| Hold T | emote wheel |
| H | drop $50 |
| Right mouse + left click | aim and shoot |
| V / B / Q / X | (Border Patrol) scan / trap / drone / taser or gun |
| Y | ready up in the lobby |
| N | watch other players while in the pen |
| K | mute mic |
| M | menu (team, customize, host settings) |

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

## Part 8: Playing together
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
