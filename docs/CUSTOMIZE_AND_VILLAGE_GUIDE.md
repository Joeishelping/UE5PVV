# What's new: customization, team names, more NPCs, aiming, driving, roosters, village

Do **Step 0** first. Every other section is a separate job you can do whenever you like.

---

## Step 0 - Get the update (≈15 min)

1. **Back up** your whole project folder (copy and paste it somewhere safe).
2. Download the latest code from GitHub (same branch as always).
3. Copy the new **Source** folder into your project and click **Replace**.
4. Copy the new **Config** folder too and click **Replace**.
5. In your project folder, delete **Binaries** and **Intermediate**.
6. Right-click `PiratesVsVikings.uproject` → **Generate Visual Studio project files**.
7. Double-click `PiratesVsVikings.uproject` → **Yes** to rebuild.
8. If it fails: send me `%localappdata%\UnrealBuildTool\Log.txt`.

---

## 1. Rename the teams

1. **Edit → Project Settings**.
2. In the left list, scroll down to the **Game** section and click **Border Run**.
3. Under **Team Names**, change any of these:
   - **Runner Team Name**: the menu button and announcements (default `Runners`)
   - **Warden Team Name**: default `Border Patrol`
   - **Runner Singular**: the HUD line "YOU ARE: ..." (default `Runner`)
   - **Warden Singular**: default `Border Patrol Agent`
4. It saves automatically. Package again so your brother's copy gets the new names.

---

## 2. Character customization (skin, shirt, pants, hat)

**In the game:** open the menu (it opens at startup, or press **M**) and click a team. A **Customize your character** box appears with:
- a **Skin** slider (lightest to darkest)
- **Shirt** and **Pants** sliders (all the way left = the outfit's own colour)
- a **Hat** checkbox
- a **RANDOM** button

Your choice is saved on your PC and everyone in the match sees it.

Both teams get the full skin range. I didn't lock skin colours to a team.

### Make the skin slider work on YOUR body (recommended, 3 min)
1. Open your character's body mesh (double-click it). In the **Material Slots** panel on the right, find the body material and double-click it to open it.
2. In the material graph, right-click on empty space → type **Vector Parameter** → press Enter.
3. With it selected, go to the **Details** panel and set **Parameter Name** to `SkinColor` (exactly like that). Set its **Default Value** to white.
4. Connect it:
   - **No texture** on the body: drag from the parameter's top output pin into **Base Color**.
   - **Body has a texture**: right-click → add a **Multiply** node. Plug the texture into **A** and `SkinColor` into **B**, then plug Multiply into **Base Color**.
5. Click **Save** (top left).

> **If you skip this:** the game swaps the body's first material slot for a plain colour, so the slider still works but any skin texture is hidden. To turn that off: **BP_Runner / BP_Warden → Class Defaults → BR | Look → uncheck Auto Skin Material**. You can also change **Skin Material Slot** there if your skin isn't slot 0.

### Shirt and pants colours
Do the same thing on the **shirt** material with a parameter named `ShirtColor`, and on the **pants** material with `PantsColor`. Use the Multiply method so logos and text (like "BORDER PATROL") stay visible.

---

## 3. More NPC trucks + NPCs that look like your Runner

Open **BP_BRGameMode → Class Defaults**.

**Make the NPCs use your Runner model:**
1. Find **PVV | Factions → Pirate Character Class**. It must be **BP_Runner**.
2. Leave **BR | Traffic → NPC Passenger Class** on **BRCharacter**. With that default, the game automatically uses your BP_Runner, so NPCs get your mesh, outfits and animations, plus a random skin, shirt and hat each.

**More trucks** (BR | Traffic):

| Setting | New default | What it does |
|---|---|---|
| Max NPC Trucks | **8** (was 3) | Most trucks on the road at once |
| NPC Truck Every Seconds | **12 - 25** | Time between new trucks |
| NPCs Per Truck | **1 - 3** | People in each truck (4 seats max, so empty seats are hiding spots) |

> NPC trucks only drive during the **Invasion** phase. To test right away: press **~** and type `SkipPhase`.

---

## 4. A livelier village (automatic)

The Runner town now builds by itself:
- 9 colourful houses with doors, windows, shutters, awnings and flower pots
- a church with a bell tower
- a plaza with a fountain, benches and palm trees
- papel picado flags across the plaza
- 4 market stalls with goods
- street lamps that **turn on at night**
- clutter: barrels, crates, a well, a wrecked car and a laundry line
- cacti
- a chicken coop with **chickens walking around and pecking**
- **12 villagers wandering** between the plaza, market, taco stand, cockfight, church and houses

**Settings:**
- **Number of villagers:** BP_BRGameMode → **BR | Village → Villager Count** (0 = none).
- **Turn the decorations off:** your Desert Arena Blueprint → **BR | Your Blueprints → uncheck Build Village**.
- **Recolour a piece:**
  1. Right-click in the Content Browser → **Blueprint Class** → search **BRVillage** → name it `BP_Village`.
  2. Open it → **Class Defaults → BR | Village → Color Overrides** → click **+**.
  3. Type the piece's name, for example `House3_Body`, `Church_Tower`, `Stall2_Awning` or `House5_Roof`, and pick a colour.
  4. In your Desert Arena Blueprint → **BR | Your Blueprints → Village Class = BP_Village**.
- **Add your own props** (Fab houses, carts, people and so on): drag them straight into your level from the Content Browser. Anything you place in the level stays put.

---

## 5. Cockfight: clearly RED vs BLUE + your own rooster model

Already in the update:
- Each bird has a **team-coloured collar** with a tag, coloured **wings and tail**, and a big **RED / BLUE** sign floating over it that always faces you.
- The pit floor is split into a **red half** and a **blue half**, with a red flag on one side and a blue flag on the other.

**Put a real chicken model on them:**
1. Get a chicken or rooster **Static Mesh** (Fab: search "rooster" or "chicken", free filter) and add it to your project.
2. Right-click in the Content Browser → **Blueprint Class** → search **BRCockfightPit** → name it `BP_CockfightPit`.
3. Open it → **Class Defaults → BR | Cockfight**:
   - **Rooster Model** = your chicken mesh
   - **Rooster Model Scale** = for example `1, 1, 1`. If it's tiny or huge, try `0.5` or `2`.
   - **Rooster Model Rotation**: if it faces the wrong way, set **Yaw** to `90`, `-90` or `180`.
   - **Rooster Model Offset**: raise or lower Z if it floats or sinks.
   - **Collar Location** / **Collar Size**: move the coloured collar onto your model's neck.
   - **Red Team Color** / **Blue Team Color**: change the team colours if you want.
4. Open your Desert Arena Blueprint → **BR | Your Blueprints → Cockfight Pit Class = BP_CockfightPit**.

---

## 6. Aiming with both hands on the gun

From now on, **if you give the game an aim animation, the gun stays in your hand while aiming.** Before, the gun floated in front of you.

1. **Mixamo** (mixamo.com) → search and download these. Choose **FBX**, **Without Skin**, **30 fps**:
   - **"Pistol Idle"**: two hands on a pistol, standing
   - **"Pistol Walk"**: walking while aiming
   - (optional) **"Pistol Kneeling Idle"** for crouch, **"Shooting"** for the fire one-shot
2. Import them and **retarget them to your body** (same steps you used for the walk animations: IK Retargeter → Export Selected Animations).
3. Open **BP_Runner** and **BP_Warden** → **Class Defaults → BR | Animations**:
   - **Aim Anim** = Pistol Idle
   - **Aim Walk Anim** = Pistol Walk
   - **Fire Anim** = Shooting (optional)
4. **Line the gun up with the hand:** BP_Runner → Class Defaults → **BR | Weapons → Hand Grip Offset**. Play, aim (hold right mouse), and adjust **Location** and **Rotation** until the gun sits in the palm. Usual starting point: Rotation **X = 0, Y = 0, Z = 90**.
5. If the arms still wobble while aiming, raise **BR | Physics → Aim Arm Strength** (try `8000`).

> Don't want this? Uncheck **BR | Weapons → Gun In Hand While Aiming** to go back to the floating gun.

---

## 7. Looking like you're actually driving

The truck now has a **driver's bench and a steering wheel that turns when you steer.**

1. **Mixamo:** download **"Driving"** (hands on a wheel, sitting) and **"Sitting Idle"** (for passengers). Use **Without Skin**, then retarget them to your body.
2. **BP_Runner / BP_Warden → Class Defaults → BR | Animations**:
   - **Drive Anim** = Driving
   - **Seated Anim** = Sitting Idle
3. While driving, the arms go stiff so the hands stay where the animation puts them (**BR | Physics → Drive Arm Strength**, default 5000).
4. **Make the hands touch the wheel:** open your truck Blueprint (or make one: Blueprint Class → **BRTruck**). In the **Components** list, select **SteeringWheel** and move it with the gizmo until it sits where the animation's hands are. Use your Runner as a reference: drop BP_Runner into the truck Blueprint viewport temporarily, then delete it.
5. **How far the wheel turns:** **BR | Truck → Steering Wheel Turn** (default 120°).

---

## More ideas for the village (tell me which ones you want)
- **Mariachi band** in the plaza that plays your Ambient music positionally, so it gets louder as you walk up
- **Villagers react**: they scatter and scream when a gun goes off or a Warden truck drives through
- **Stray dogs** that follow Runners around and bark, giving away your position
- **Fiesta at night**: fireworks over the church at 9 pm and string lights on the flags
- **Taco stand line**: villagers queue at the taco stand, and you can hide in the line
- **Piñata** in the plaza: hit it for money or a random item
- **Wanted posters** on houses with the names of Runners who got caught
- **Village dogs and chickens** that panic and run from trucks
