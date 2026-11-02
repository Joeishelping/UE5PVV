# Party Update: points race, smuggling, brawling, voice chat

## Step 0: Install (≈15 min)
1. Download the latest code. **Back up** your project folder.
2. Copy the new **Source** folder into your project and click **Replace**.
3. **Voice chat needs 2 config lines** (don't copy the whole Config folder; it would wipe your map settings):
   - Open `Config\DefaultEngine.ini` in Notepad and add this at the very bottom:
     ```
     [Voice]
     bEnabled=true
     ```
   - Open `Config\DefaultGame.ini`. Under the line `[/Script/Engine.GameSession]`, add:
     ```
     bRequiresPushToTalk=false
     ```
   - Save both files.
4. Delete **Binaries** and **Intermediate**.
5. Right-click the .uproject → **Generate Visual Studio project files**.
6. Double-click the .uproject → **Yes**.
7. If it fails: send me `%localappdata%\UnrealBuildTool\Log.txt`.

---

## New controls
| Key | What it does |
|---|---|
| **Mouse wheel** | Pick an item in your pockets (yellow box) |
| **F** | **Use** the selected item: eat a taco, play the trumpet, put down a taco platter |
| **G** | **Drop** the selected item. Anyone can pick it up with **E**. |
| **Z** | **Grab** the person or object in front of you. Press **Z** again to **THROW** it. On a **teammate**: climb onto their **shoulders**. |
| **Space** (on shoulders) | **Leap** off, high enough to get over the Wall |
| **Left Alt** | **Dive / tackle.** Hit someone and they go flying; miss and you belly-flop. |
| **Left click** (holding an object) | Swing it. It hits much harder than a fist. |
| **K** | Mute / unmute your mic |

---

## How a round works now: the points race
- Prep (shopping and planning), then the **Invasion**. **Most points when the timer runs out wins.**
- The top of the screen shows **RUNNERS 12 - 9 BORDER PATROL**.
- **Runners reaching the Safe Zone** get paid **$150**, score **+3**, and get **sent back to Mexico** to go again.

| Runners score for | Points | Money |
|---|---|---|
| Reaching the Safe Zone | +3 | $150 |
| Each **contraband** item carried in (drug package, illegal gun) | +2 | **$500** |
| Each **smuggling client** delivered | +2 | **$250** |

| Border Patrol score for | Points |
|---|---|
| Catching a Runner who **crossed** (pen or catapult) | +2 |
| Seizing contraband (catch someone carrying it, find it in a truck, pick up dropped contraband) | +1 each |
| Catching an NPC with **no papers** | +1 |
| **Clearing a truck** at the checkpoint (everyone scanned and clean) | +1 |
| Arresting someone who **never crossed** | **-1**, plus an approval hit |

All of these numbers are in **BP_BRGameMode → Class Defaults → BR | Points**.
To make rounds shorter, change **Prep Seconds** and **Invasion Seconds** under **BR | Timers** (try 120 and 420).

---

## Smuggling
- **Contraband:** buy a **??? Package** ($150) at the shady dealer by the black market, or an illegal gun at night. If you're caught with it, it's confiscated.
- **Coyote clients:** walk up to the **COYOTE** counter in town and press **E**. An NPC client starts following you, up to 3 at a time.
  - They follow you everywhere: into trucks, through tunnels, over the Wall.
  - If you get caught, your clients get lost.
  - **Border Patrol can arrest your clients** for points.

## The checkpoint (harder for Border Patrol now)
- NPC trucks **stop at the gate and won't drive through just because the gate is open.**
- Border Patrol has to **get out, walk up to each person** in the truck, and **scan them one by one** (**V**, from close range).
- Each scan shows one of:
  - "papers in order"
  - "NO PAPERS!"
  - "CONTRABAND!"
  - "RUNNER!" (a player hiding in the bed)
- **Everyone clean:** the truck is cleared and drives through, and Border Patrol scores +1.
- **Someone flagged:** hold **E** on the truck ("Order everyone out"). Only the suspects get pulled out. Contraband is seized automatically, then arrest the illegals.
- If nobody checks a truck for 2 minutes, it pushes through and **approval drops**. Runners hiding in it get through.
- About 15% of NPCs have no papers, and 10% carry contraband (**BR | Traffic** in BP_BRGameMode).

## Money: no more spamming jobs
- Every Runner gets **one job** at the start: "Your job in town: Work the register". It shows on your HUD.
- Other jobs say "Not your job". After each shift you get a **20-second break**.
- About 1 in 4 Runners are **unemployed**. They dig through junk for scrap and sell it.
- **Taco Platter** ($40 at the taco stand): select it with the wheel and press **F** to put it down. Your crew grabs free tacos from it with **E** (8 servings).

## Brawling (Gang Beasts style)
- **Your floppy arms smack people.** Fast-moving hands knock people back, and enough hits knock them over. It works on players and NPCs, so watch out for approval as Border Patrol.
- **Grab & throw (Z):** grab anyone and throw them into the river, over a fence, or off the bridge. People who aren't zip-tied wriggle free after 3 seconds.
- **Tackle (Left Alt):** dive into someone to flatten them.
- **Shoulders:** press **Z** on a teammate to climb on. They walk you to the Wall, and you press **Space** to leap over.
- **Objects:** crates are scattered around the test map. **Z** to pick one up, **left click** to swing, **Z** to throw. Anything hit hard falls over.
- Everyone sees a **kill feed** in the top-right ("Jorge BODY-SLAMMED Dave", "Ana YEETED Jorge back to Mexico").
- **Screen shake** when you get hit, knocked down, or land a big flight.

**On your own map:** place **BR Physics Prop** pieces (choose your own **Prop Mesh**, like barrels, chairs or cactus pots) and **BR Item Pickup** (free loot). For a coyote counter, use a **BR Station** with **Kind = Hire Client** and **Price = 250**.

## End of round: awards
When time runs out, everyone sees **MVP**, **Border Hopper**, **Coyote of the Year**, **Narco Legend**, **Top Cop**, **Paper Pusher**, **Wrongful Arrest King**, **Most Catapulted**, **Human Cannonball**, **Body Slammer**, **Linebacker**, **Yeet Master**, **Mariachi Menace** and **Taco Addict**.

## Proximity voice chat
- Your mic is **on** when you're in a multiplayer game. You hear people **from their character's position**, and voices **fade out beyond about 25 m**.
- **K** mutes or unmutes you.
- Range: **BP_Runner / BP_Warden → BR | Voice → Voice Range**.
- Needs the 2 config lines from Step 0. It works best when you join through **Steam**. Over Radmin it should work too.

## Sounds you can add (drag in any sound)
- **BP_Runner / BP_Warden → BR | Brawl:** Slap Sound, Tackle Sound, Throw Sound
- **BR | Mariachi:** Trumpet Sound
- **Catapult:** Launch Sound
- **Human Cannon:** Fire Sound
- **Piñata:** Whack Sound, Burst Sound
