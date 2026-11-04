# Making the effects (Niagara) — step by step

Every effect slot lives in **Edit → Project Settings → Border Run → Effects**.

**You don't *have* to make any of these.** The game has built-in versions of all of them: a dust puff, a splash, smoke, a quick white flash for punch hits, little orange sparks, red blood droplets, rain, sand, and a blood puddle. Make Niagara versions only for the ones you want to look fancier. You make an effect once, drag it into its slot, and the game uses it from then on. Empty slots keep the simple built-in look, so you can do these one at a time.

You'll make **9 Niagara Systems** and **1 decal material** (for the blood puddle).

---

## Part A — The basics (read this once)

### A1. Make a new effect from a template
1. In the **Content Browser**, make a folder called **FX**.
2. Right-click inside it → **FX → Niagara System**.
3. Choose **"New system from selected emitter(s)"** → **Next**.
4. You'll see a list of **templates**. Click the one the recipe below names (for example **Omnidirectional Burst**), then click the **green +** so it shows in the bottom list → **Finish**.
5. Name it like the recipe says (for example `NS_Dust`) and double-click it to open.

### A2. Find your way around the Niagara editor
- **Top left:** the **Preview** window. The effect plays here on a loop so you can see your changes.
- **Middle:** the **System Overview**. You'll see a box for your emitter with sections, from top to bottom:
  - **Emitter Spawn / Emitter Update:** *how many* particles, and *when* they appear.
  - **Particle Spawn:** what each particle looks like *when it's born* (size, color, speed, lifetime).
  - **Particle Update:** what happens to it *while it's alive* (gravity, fading, growing).
  - **Render:** *how it's drawn* (the sprite and its material).
- **Right:** the **Selection** panel. Click any module in the middle and its settings show here.
- To **add** a module: click the **+** next to a section (for example Particle Update) and type its name.
- To **remove** a module: right-click it → **Delete**.
- **Save:** Ctrl+S.

### A3. The modules you'll use over and over
| Module (where it lives) | What it does |
|---|---|
| **Emitter State** (Emitter Update) | **Loop Behavior**: **Once** = one burst and done (most of our effects). **Infinite** = keeps going (rain, sandstorm). |
| **Spawn Burst Instantaneous** (Emitter Update) | **Spawn Count** = how many particles appear at once. |
| **Spawn Rate** (Emitter Update) | Particles per second, for constant effects like rain. |
| **Initialize Particle** (Particle Spawn) | **Lifetime** (seconds), **Color**, **Sprite Size**. Set **Sprite Size Mode** to **Random Uniform** to get a min/max size. |
| **Shape Location** (Particle Spawn) | Where particles start: a point, a **Sphere**, or a **Box**. |
| **Add Velocity** (Particle Spawn) | How fast and which way they fly. **Velocity Mode → In Cone** sprays in a direction. |
| **Gravity Force** (Particle Update) | Pulls them down (-980 = normal gravity). |
| **Drag** (Particle Update) | Slows them down over time (puffs that drift to a stop). |
| **Scale Color** (Particle Update) | Fading out: set the alpha (A) curve from 1 down to 0. |
| **Scale Sprite Size** (Particle Update) | Growing or shrinking over its life (smoke puffs grow). |
| **Sprite Renderer** (Render) | **Alignment → Velocity Aligned** stretches fast particles into streaks (sparks, rain, blood). |

**Tip:** if a template already has a module from the table, edit it rather than adding a second one. If a "Once" effect has **Spawn Rate**, delete it and use **Spawn Burst Instantaneous** instead.

**Colors above 1 glow.** In a Color box, `R 8, G 4, B 1` makes a glowing orange. Use this for sparks and the punch flash.

### A4. Plug it in
**Edit → Project Settings → Border Run → Effects** → drag your `NS_...` asset from the Content Browser onto the matching slot. Done.

---

## Part B — The recipes

Numbers are good starting points. Tweak them while watching the Preview.

### 1. Dust — `NS_Dust` (slot: **Dust Effect**)
Hard landings, belly-flops, falling off walls, tunnel digging.
- **Template:** Omnidirectional Burst
- **Emitter State:** Loop Behavior **Once**
- **Spawn Burst Instantaneous:** Spawn Count **18**
- **Initialize Particle:**
  - Lifetime **0.6–1.2**
  - Color **R 0.75 G 0.62 B 0.42 A 0.6** (sandy)
  - Sprite Size (Random Uniform) **40–90**
- **Shape Location:** Sphere, radius **30**
- **Add Velocity:** In Cone, cone pointing **up** (Z), angle **70**, speed **100–250**
- **Particle Update:**
  - **Drag 2**
  - **Gravity Force -150** (light)
  - **Scale Sprite Size** grows **1 → 2.5**
  - **Scale Color** alpha **1 → 0**

### 2. Splash — `NS_Splash` (slot: **Splash Effect**)
Someone hits deep water.
- **Template:** Fountain (or Directional Burst)
- **Emitter State:** **Once**. Swap Spawn Rate for **Spawn Burst Instantaneous: 40**.
- **Initialize Particle:**
  - Lifetime **0.6–1.0**
  - Color **R 0.8 G 0.9 B 1 A 0.8**
  - Sprite Size **8–20**
- **Add Velocity:** In Cone, **up**, angle **35**, speed **400–700**
- **Gravity Force -980**
- **Scale Color** alpha fade at the end

### 3. Smoke — `NS_Smoke` (slot: **Smoke Effect**)
Cannon shot, smoke bombs, balloon pop.
- Same as **Dust**, but:
  - Color **R 0.35 G 0.35 B 0.37 A 0.5**
  - Sprite Size **120–250**
  - Lifetime **2–3**
  - Speed **50–150** up
  - Scale Sprite Size **1 → 3**
  - No gravity (delete **Gravity Force**)
  - Spawn Count **25**

### 4. Punch hit — `NS_PunchHit` (slot: **Punch Hit Effect**)
A quick flash where a punch, kick or headbutt lands.
- **Template:** Simple Sprite Burst
- **Emitter State:** **Once**
- **Spawn Burst:** **8**
- **Initialize Particle:**
  - Lifetime **0.12–0.25** (very short)
  - Color **R 6 G 5 B 3** (glowing white-yellow)
  - Sprite Size **25–60**
- **Shape Location:** Sphere, radius **5**
- **Add Velocity:** In Sphere / Omnidirectional, speed **200–400**
- **Scale Sprite Size:** **1 → 0** (shrinks away fast)

### 5. Cigarette smoke — `NS_CigSmoke` (slot: **Cigarette Smoke Effect**)
A small puff for the smoking emote.
- Copy `NS_Smoke` (right-click → **Duplicate**), then change:
  - Spawn Count **3**
  - Sprite Size **10–25**
  - Color alpha **0.3**
  - Lifetime **1.5**
  - Speed **30–60** up
  - Scale Sprite Size **1 → 3**

### 6. Sparks — `NS_Sparks` (slot: **Sparks Effect**)
Cannon fuse, and punches that bounce off a guard.
- **Template:** Directional Burst
- **Emitter State:** **Once**
- **Spawn Burst:** **25**
- **Initialize Particle:**
  - Lifetime **0.3–0.7**
  - Color **R 8 G 4 B 1** (glowing orange)
  - **Sprite Size Mode: Non-Uniform**: **X 2, Y 10** (thin streaks)
- **Add Velocity:** In Cone, angle **50**, speed **300–800**
- **Gravity Force -980**
- **Sprite Renderer:** Alignment **Velocity Aligned**

### 7. Blood spurt — `NS_Blood` (slot: **Blood Effect**)
Where a knife or bullet goes in.
- **Template:** Directional Burst
- **Emitter State:** **Once**
- **Spawn Burst:** **30**
- **Initialize Particle:**
  - Lifetime **0.4–0.8**
  - Color **R 0.35 G 0 B 0 A 1** (dark red)
  - Sprite Size **Non-Uniform: X 3, Y 8**
- **Shape Location:** Sphere, radius **4**
- **Add Velocity:** In Cone, angle **35**, speed **200–500**
- **Gravity Force -980**
- **Drag 1**
- **Sprite Renderer:** Alignment **Velocity Aligned** (droplets streak as they fly)
- **Optional:** make a second, smaller burst of slower round drops for a mist. Copy the emitter and set speed **50–150**, size **2–4**, no velocity alignment.

### 8. Rain — `NS_Rain` (slot: **Rain Effect**)
This one **loops**. The game moves it with your camera and switches it on and off with the weather.
- **Template:** Fountain (we'll change almost everything)
- **Emitter Properties** (click the emitter's name at the top of its stack):
  - **Sim Target: GPUCompute Sim** (handles thousands of drops)
  - Tick **Fixed Bounds** and set them big: Min **(-3000, -3000, -2000)**, Max **(3000, 3000, 1000)**
- **Emitter State:** Loop Behavior **Infinite**
- **Spawn Rate:** **3000**
- **Initialize Particle:**
  - Lifetime **1.0–1.2**
  - Color **R 0.7 G 0.75 B 0.85 A 0.35**
  - Sprite Size **Non-Uniform: X 1.5, Y 60**
- **Shape Location:** **Box**, size **X 4000, Y 4000, Z 10**
- **Add Velocity:** Linear, **(150, 0, -2500)** (straight down with a little slant)
- Delete **Gravity Force** (the velocity already does it)
- **Sprite Renderer:** Alignment **Velocity Aligned**

### 9. Sandstorm — `NS_Sandstorm` (slot: **Sandstorm Effect**)
Loops too, centered on your camera.
- **Template:** Blowing Particles (or Fountain)
- **Emitter Properties:**
  - Sim Target **GPUCompute Sim**
  - Fixed Bounds **(-3000, -3000, -500)** to **(3000, 3000, 1500)**
- **Emitter State:** **Infinite**
- **Spawn Rate:** **600**
- **Initialize Particle:**
  - Lifetime **3–5**
  - Color **R 0.8 G 0.62 B 0.4 A 0.12** (very see-through)
  - Sprite Size **150–400**
- **Shape Location:** **Box**, **X 5000, Y 5000, Z 1500**
- **Add Velocity:** Linear, **(900, 300, 0)** (wind blowing sideways), with some random spread
- **Particle Update:**
  - **Scale Sprite Size** grows a little
  - **Scale Color** fades in at the start and out at the end (alpha **0 → 1 → 0**)

---

## Part C — The blood puddle (a decal)

The puddle under someone who's stabbed, shot or killed isn't a particle effect. It's a **decal**: a picture projected onto the ground.

### C0. Shortcut: let the script build it (1 click)
1. In the editor, go to **Tools → Execute Python Script...** (older versions: **File → Execute Python Script**).
2. Pick `Tools/Editor/make_blood_pool.py` from the project folder.
3. It creates **/Game/FX/M_BloodPool** for you. Skip to **C2**.
(If you don't see that menu item, go to **Edit → Plugins**, search **Python Editor Script Plugin**, tick it and restart. The project turns it on already, though.)

### C1. Make the material by hand (if you'd rather)
1. Content Browser → right-click → **Material** → name it `M_BloodPool` and open it.
2. Click the big result node, and in **Details**:
   - **Material Domain:** **Deferred Decal**
   - **Blend Mode:** **Translucent**
3. **Color:** hold **3** and click the empty graph to make a color node. Set it to **R 0.15 G 0 B 0** (very dark red) and connect it to **Base Color**.
4. **Wet shine:** hold **1** and click to make a number, set it to **0.15**, and connect it to **Roughness**.
5. **Round, soft edge:** right-click the graph → search **RadialGradientExponential** → add it.
   - Set **Radius 0.45** and **Density 3**.
   - Connect its output to **Opacity**.
6. **Optional, splatter edges:** add a **Noise** node (Scale **6**), multiply it with the gradient (**Multiply** node), and plug that into **Opacity** instead.
7. **Save.** The preview sphere looks odd for decals. That's normal.

### C2. Plug it in
**Project Settings → Border Run → Effects:**
- **Blood Pools:** ✓ (untick it to turn puddles and drips off completely)
- **Blood Pool Decal:** `M_BloodPool`
- **Blood Pool Seconds:** how long puddles last (default **45**)

The game sizes each puddle randomly, spins it, and fades it out at the end.

---

## If something looks wrong
| Problem | Fix |
|---|---|
| The effect never stops | **Emitter State → Loop Behavior → Once** (everything except rain and sandstorm). |
| Rain or sand only shows in a small area, or flickers | Turn on **Fixed Bounds** and make them big (see the recipes). |
| Particles are black squares | The Sprite Renderer has no material. Use the template's default (`DefaultSpriteMaterial`). |
| Too bright or too dark | Change the **Color** in Initialize Particle (values above 1 glow). |
| The blood puddle doesn't show | Check the material's **Material Domain** is **Deferred Decal**, and **Blood Pools** is ticked. |
| Effect too big or too small in game | Change **Sprite Size** and the speeds. The Preview grid squares are 100 cm (1 m). |
