# Mega Update: hats, bots, lobby, emotes, budget, spotlights, chickens, sounds

This one is big. Do the steps in order. Parts you skip just stay off; nothing breaks.

---

## Step 0: Install (about 15 min)
1. **Back up** your project folder (copy it somewhere).
2. Download the latest code. Copy the new **Source** folder into your project and click **Replace**.
   **Don't copy Config.** There are no new config lines this time.
3. Copy the new files from **Art/Characters** somewhere you can find them. You'll import them in Step 1.
4. Delete **Binaries** and **Intermediate**.
5. Right-click the .uproject → **Generate Visual Studio project files**.
6. Double-click the .uproject → **Yes**.
7. If it fails, send me `%localappdata%\UnrealBuildTool\Log.txt`.
   If it crashes while playing, send me `Saved\Logs\PiratesVsVikings.log`.

---

## Step 1: Import the new art (about 15 min)
In the Content Browser, open the folder where your outfits are.

**Hats and poncho.** Drag in these 4 files:
- `SK_Hat_Sombrero.glb`
- `SK_Hat_HardHat.glb`
- `SK_Hat_CannonHelmet.glb`
- `SK_Hat_LuchaMask.glb`
- `SK_Outfit_Poncho.glb`

In the import window:
- **Skeleton** = your body's skeleton (the same one the cowboy hat uses)
- **Import Morph Targets** ✓ (so the hats grow with the Head slider)
- Click **Import All**.

**Trumpet.** Drag in `SM_Trumpet.glb`. It's a normal static mesh, so no skeleton is needed.

**Animations.** Make a folder **Animations**. Drag in `SK_PVV_Animations.glb`.
- **Skeleton** = your body's skeleton
- Tick **Import Animations**
- You get one Animation Sequence per move (Idle, Walk, Run, CrouchIdle, CrouchWalk, Aim, AimWalk, Punch, Dance, Trumpet, Emote_Wave, ...).
- If it also makes an extra copy of the body mesh, you can delete that mesh. Keep the animations.

---

## Step 2: Fill the slots in BP_Runner AND BP_Warden (about 10 min per Blueprint)
Open **BP_Runner** → **Class Defaults** (top bar). Do the same in **BP_Warden**.
Tip: type the category name in the Details search box.

### BR | Hats
**Hat Meshes**: click **+** six times. Set each row's left box to a hat and the right box to its mesh:

| Hat | Mesh |
|---|---|
| Cowboy Hat | SK_Outfit_CowboyHat |
| Sombrero | SK_Hat_Sombrero |
| Patrol Cap | SK_Outfit_PatrolCap |
| Hard Hat | SK_Hat_HardHat |
| Cannon Helmet | SK_Hat_CannonHelmet |
| Lucha Mask | SK_Hat_LuchaMask |

Fill in all six in **both** Blueprints, because hats fly off and anyone can pick them up.

| Setting | BP_Runner | BP_Warden |
|---|---|---|
| **Default Hat** | Cowboy Hat | Patrol Cap (or leave it on Cowboy Hat; Border Patrol switches to the Patrol Cap automatically) |
| **Hat Knock Off Chance** | 0.6 (60% chance your hat flies off when you get KO'd) | same |
| **Poncho Mesh** | SK_Outfit_Poncho | SK_Outfit_Poncho |

**Held Item Meshes**: click **+**, set Item = **Mariachi Trumpet**, Mesh = **SM_Trumpet**.
Other items show a simple shape until you add your own models the same way.

**Held Item Offset** controls where the item sits in the right hand. If the trumpet sits wrong, change **Location** and **Rotation** here.

### BR | Animations
Drag in the animations: Idle, Walk, Run, Crouch Idle, Crouch Walk, Aim, Aim Walk, Fall, Swim, Seated, Drive, Restrained, Struggle, Carried, Work, Punch, Fire, Reload, Eat, Dance, Trumpet, plus the new emotes:
- **Emote Wave Anim** = Emote_Wave
- **Emote Flex Anim** = Emote_Flex
- **Emote Taunt Anim** = Emote_Taunt
- **Emote Beg Anim** = Emote_Beg
- **Emote Cheer Anim** = Emote_Cheer

> The animation slots only work when your mesh has **no Anim Class** set (Mesh component → Anim Class = None). If you made your own Animation Blueprint, keep it. The slots are a shortcut, not a must.

**Compile** and **Save** both Blueprints.

---

## Step 3: Sounds (whenever you want)
Global sounds now live on one page: **Edit → Project Settings → (Game) Border Run → Sounds**.
Drag a Sound Wave into any slot. Empty slots stay silent. This page saves into DefaultGame.ini on its own.

| Slot | What to find |
|---|---|
| Buy Sound | cash register "ka-ching" |
| Sell Sound | coins / small cash |
| Cash Pickup Sound | money rustle |
| Payday Sound | big cash / slot machine win |
| Zip Tie Sound | zip-tie zip |
| Jail Door Sound | cell door clang |
| Splash Sound | big water splash |
| Scan OK Sound | friendly beep |
| Scan Alarm Sound | alarm buzz |
| Trap Snap Sound | metal snap |
| Tripwire Alarm Sound | alarm bell |
| Hat Pop Sound | cartoon "pop" |
| Spotted Sound | dramatic sting |
| Bounty Sound | "WANTED" sting / western whistle |
| Round Start Sound | air horn |
| Round End Sound | buzzer |
| Countdown Sound | short beep (lobby countdown, last 5 seconds of a phase) |
| Safe Zone Sound | celebration jingle |
| Whistle Sound | police whistle (good catch) |
| Landing Sound | heavy thud |
| Requisition Sound | rubber stamp |

Other sounds still live on the Blueprints:
- **BP_Runner / BP_Warden → BR | Brawl**: Punch Swing, Punch Hit, Knockout, Grab, Slap, Tackle, Throw
- **BR | Mariachi**: Trumpet Sound
- **BR | Weapons**: Gun, Taser, Reload
- On placed pieces: Catapult **Launch Sound**, Human Cannon **Fire Sound**, Piñata **Whack/Burst**, Dog **Bark**, Chicken **Cluck Sound**, Truck **Horn** and radio stations

---

## New controls
| Key | What it does |
|---|---|
| **Hold T** | Emote wheel. Move the mouse toward an emote and let go (Wave, Flex, Taunt, Beg, Cheer, Dance). |
| **H** | Drop $50 on the ground. Anyone can choose to pick it up with **E**. If Border Patrol takes it, it goes into their budget. |
| **Y** | Lobby: ready / not ready |
| **N** | In the Holding Pen: watch other players |
| **Z** | Grab. This also works on **chickens**: grab one, then Z again to throw it at someone. |
| **E** on a hat on the ground | Wear it (yours drops off) |

---

## Hats: every hat has a skill
| Hat | Skill | How you get it |
|---|---|---|
| Cowboy Hat | +12% speed | Runners start with it |
| Patrol Cap | scanner is twice as fast with longer range | Border Patrol starts with it |
| Sombrero | 40% slower hunger, harder to spot in daytime | **unlock:** eat 30 tacos |
| Hard Hat | 40% less damage from hits | **unlock:** knock out 25 people (or Border Patrol buys one at a supply counter) |
| Lucha Mask | dives fly 40% farther, tackles hit 50% harder | **win it** by beating the luchador in the ring |
| Cannonball Helmet | land from the cannon or catapult on your feet | **unlock:** 10 cannon shots. **Buying the Cannonball Suit puts it on right away.** Wearing it lets you use the cannon. |

- Pick your hat in **M → Customize → Hat**. Locked hats show your progress ("Eat 30 tacos (12/30)").
- Hat changes in the menu only count during Prep.
- Progress is saved on your PC and counted at the end of each round.
- **Getting knocked out can make your hat fly off.** Anyone can pick it up and wear it.

## Poncho disguise
- Buy one at the **PONCHO DISGUISE $200** counter. You're disguised while it's in your pockets.
- About 1 in 4 locals wear ponchos too.
- While you're disguised, scanners say "papers in order", spotlights ignore you, and Border Patrol bots ignore you.
- **Sprinting, punching, diving, grabbing or getting hit rips the poncho**, and the disguise is blown.

## Lucha Libre: fight the luchador for money
There's a wrestling ring next to the cockfight (test map), with counters around it:
- **Red counters: FIGHT THE LUCHADOR** for **$50, $100 or $250**. Bigger fights mean a tougher luchador.
- You're put in your corner. For **15 seconds** everyone else can bet at the counters (**BET ON THE CHALLENGER** / **BET ON THE LUCHADOR**, $20 or $100).
- **DING DING!** The first fighter knocked down **twice** loses. Throwing him out of the ring counts as a knockdown.
- **Leaving the ring = you lose.** If time runs out (90 s), the luchador wins on points unless you knocked him down more.
- **Win: you get double your money back AND his Lucha Mask** (it unlocks for good). Everyone who bet on the winner gets double their bet.
- The board above the ring shows the score and the betting pots.
- Settings: click the ring in the level → **BR | Lucha** (luchador names, fight length, knockdowns to win, toughness, your own **Luchador Class** Blueprint).
- **On your own map:** place a **BR Lucha Ring**. Its counters appear by themselves.

## Border Patrol government budget
- Border Patrol shares a budget, shown at the bottom left. It starts at **$3000**.
- **Clearing a legal truck: +$400 budget (+1 point). Each good catch: +$150. Catching the bounty target: +$1000.**
- Spend it at the blue **GOV. SUPPLY** counters at the Border Patrol base:

| Supply | Cost |
|---|---|
| +2 team traps | $600 |
| drone recharge | $300 |
| Hard Hat | $400 |
| patrol dog | $1500 |
| taco | $20 |

- To tune these numbers: **BP_BRGameMode → BR | Budget**.
- **On your own map:** place a **BR Station** → **Kind = Warden Supply** → pick **Warden Gear** (or set Gear = None and pick an **Item**) → set **Price**.

## Bounty
- Every 20 seconds of the Invasion, the Runner with the most points (at least 6) becomes **WANTED**.
- A green **$ WANTED $** shows over their head, and everyone sees a "BOUNTY" marker.
- The Border Patrol agent who catches them gets **+3 points** and **+$1000** for the budget.
- To tune it: **BP_BRGameMode → BR | Bounty**.

## Watchtowers with spotlights
- There are 2 towers on the Border Patrol side of the test map. The light sweeps by itself.
- **A Border Patrol agent can climb up (E at the tower) and the light follows where they look.**
- Runners caught in the beam are **SPOTTED** (Border Patrol sees them for 4 seconds). Cover blocks the beam.
- **On your own map:** place **BR Watchtower** pieces and rotate them so the arrow points where they should sweep.

## Chickens
- The chickens in the village are real now. They hop around, run away from people, and can be grabbed and **thrown** (a thrown chicken knocks people over).
- **On your own map:** place **BR Chicken** pieces and add a **Cluck Sound** to them.

## Bots (computer players)
Two ways to add them:
1. **Host settings** (below): turn **Fill with bots** on and set the team sizes. Bots join to fill each team and leave again as people join.
2. **Console** (press **~**): `AddBots 3`, `AddBot Runners`, `AddBot Wardens`, `RemoveBots`.

What the bots do:
- **Runner bots** work their job, buy a ladder and a taco during Prep, then run for the Safe Zone. They climb ladders over the Wall, dodge Border Patrol and mash out of zip-ties.
- **Border Patrol bots** chase Runners who crossed, punch or tackle them, zip-tie them, and carry them to the pen. They never arrest people who haven't crossed.

They aren't geniuses, but they're good for testing and for filling out small games.

## Lobby, host settings and best of 3
- **Multiplayer games start in a LOBBY.** You can walk around, but shops are closed and the clock waits. Everyone presses **Y** when ready, and a 5-second countdown starts Prep.
- **Host settings**: the host presses **M**. A **HOST SETTINGS** panel shows on the right:
  - Prep time
  - Invasion time
  - Ready-up lobby on/off
  - **Best of 3** on/off
  - Fill with bots on/off
  - Runner team size and Patrol team size
  - **START NOW** (skips the lobby)
- The settings are saved on the host's PC and used every round.
- **Best of 3:** the round score shows at the top ("BEST OF 3: 1 - 0"). The first team to win 2 rounds wins the series.
- **Switching teams:** press **M** and click the other team (in the lobby or any time).

## Other new things on screen
- **Tips** at the bottom during your first 3 rounds
- A **SAFE ZONE** marker with the distance (Runners during the Invasion)
- A **HOLDING PEN** marker while you carry someone (Border Patrol)
- **+$150** and **+3** popups when you earn money or points
- Dust when you land from a flight or belly-flop, a splash when you hit deep water, and smoke when the cannon fires

---

## If something looks wrong
| Problem | Fix |
|---|---|
| Hats don't change / everyone has the same hat | **Hat Meshes** is empty in that Blueprint. Fill all 5 rows (Step 2). |
| Hats float or sit inside the head | They were imported without your body's skeleton. Re-import with **Skeleton** = your body skeleton. |
| Trumpet sits in the wrong spot | Change **Held Item Offset** (BR \| Hats). |
| Luchador has no mask | Add the **Lucha Mask** row to **Hat Meshes** (Step 2). |
| Emotes only show "*waves*" text | Put the Emote animations in the **BR \| Animations** slots. |
| No lobby | It's skipped when you play solo. In multiplayer, check that **Ready-up lobby** is ON in the host settings. |
| Bots stand still | They need the Line to be open for most things (`SkipPhase`). Runner bots also need a ladder shop on your map. |
