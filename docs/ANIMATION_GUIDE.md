# Animation Guide — Gang Beasts wobble + real gun animations

**How it works:** animations say what the body *tries* to do. The physics body (`PhysicalAnimation`, built into
BRCharacter) makes it *loosely* follow, with floppy arms, a wobbly spine and firm legs, so it reads as a drunk Gang Beasts body.
**While aiming, the arms go stiff**, so shooting animations stay crisp.

---

## 1. Physics feel (no animation needed)

Open **BP_Runner / BP_Warden → Class Defaults → BR → Physics**:

| Setting | Default | Gang Beasts tip |
|---|---|---|
| Wobbly Body | ✓ | Needs a Physics Asset on your body mesh |
| Wobble Strength | 1.0 | 0.5 = very drunk, 2.0 = stiff |
| Leg Strength | 1600 | Keep high so they stay standing |
| Spine Strength | 700 | Lower = more sway |
| Head Strength | 350 | Lower = bobble-head |
| Arm Strength | 180 | Floppy noodle arms |
| Aim Arm Strength | 5000 | Arms lock onto the aim animation |
| Hit Wobble | 350 | How hard punches/shots shove the upper body |
| Sway Strength | 40 | Random drunken shoves; doubles when starving or nearly KO'd |

**Physics Asset tips** (double-click `SK_..._PhysicsAsset`):
- Select all bodies → **Linear Damping 0.2, Angular Damping 1.0** (stops jitter).
- Constraints on **calf** and **lowerarm**: Swing 1 **Limited 90°**, Swing 2 **Locked**, Twist **Limited 10°** (knees and elbows only bend one way).
- **Head/neck:** Limited 30°.
- If the body explodes or shakes, make the capsules slightly smaller so they don't overlap.

---

## 2. Where to get animations (free)

1. **Mixamo** (mixamo.com, free with an Adobe account). Huge library, including **drunk** ones. Download **FBX, Without Skin, 30 fps**.
2. **Unreal's Third Person pack** (Add → Add Feature or Content Pack → Third Person): idle/walk/run/jump.
3. **Game Animation Sample** (free on Fab, by Epic): hundreds of high-quality locomotion animations.
4. **Lyra Starter Game** (free on Fab, by Epic): **pistol idle, aim, fire, reload** montages. Copy them out of Lyra with **Asset Actions → Migrate**.

### Retargeting Mixamo animations to your body
1. Import one Mixamo FBX **with skin** first (e.g. "Y Bot"). That gives you the Mixamo skeleton.
2. Right-click the Mixamo skeletal mesh → **Create → IK Rig** → open it → **Auto Create Retarget Chains** → **Auto Create IK** → Save.
3. Right-click **your body** → **Create → IK Rig** → same two buttons → Save.
4. Right-click → **Animation → IK Retargeter** → Source = Mixamo IK Rig, Target = your IK Rig → open it.
   If the poses don't match, click **Auto Align** in the toolbar.
5. Select the animations in the Asset Browser at the bottom → **Export Selected Animations**. They now work on your body.

(Mannequin/Lyra/Game Animation Sample animations: right-click the animation → **Retarget Animations** → Target = your body → Export.)

---

## 3. Every action → which animation → how it plays

Search these names on Mixamo (or use the listed pack).

| In the game | Animation to get | How it's triggered |
|---|---|---|
| Standing around | "Drunk Idle" | Locomotion state (speed 0) |
| Walking | "Drunk Walk" | Locomotion (speed < 400) |
| Sprinting | "Drunk Run Forward" / "Running" | Locomotion (speed > 400) |
| Crouch sneaking | "Crouched Walking", "Crouch Idle" | `Is Crouching` (Character Movement) |
| Jumping / falling / **catapult flight** | "Jumping", "Falling Idle" (arms flailing) | `Is Falling` |
| Wading in the river | "Wading" / slow walk | `Is Wading` |
| Deep water | "Treading Water", "Swimming" | `Is In Deep Water` |
| Punch | "Punching", "Hook Punch" | **Event On Attack Started** → Play Montage |
| Getting punched | none: physics does it | `Hit Wobble` |
| Knocked down | none: full ragdoll by code | automatic |
| Zip-tied (shuffling) | "Standing Arms Behind Back", "Sad Walk" | `Is Restrained` |
| Struggling to break free | "Struggle", "Shaking It Off" | `Is Restrained` and `Get Struggle Progress > 0` |
| Being carried | "Hanging Idle" (limp) | `Is Being Carried` |
| Carrying someone | "Carry Box Walk" (upper body only) | `Is Carrying Someone` (Layered blend, see 4c) |
| Sitting in the truck bed / passenger | "Sitting Idle" | `Is Seated` |
| Holding E (jobs, cutting fence, zip-tying, refuelling, jailbreak) | "Working On Device", "Wire Cutting" (search "Cutting"), "Kneeling Working" | `Get Hold Progress > 0` |
| Aiming the pistol/taser | **Lyra Pistol Aim** pose / Mixamo "Pistol Idle" | `Is Aiming` |
| Aiming the revolver | Mixamo "Pistol Idle" (one-handed) | `Is Aiming` and `Get Weapon Type == 3` |
| Firing | **Lyra Pistol Fire** / Mixamo "Shooting" | **Event On Weapon Fired** → Play Montage |
| Reloading | **Lyra Pistol Reload** / Mixamo "Reloading" | **Event On Reload Started** → Play Montage |
| Eating a taco | "Eating" (Mixamo) | **Event On Ate** → Play Montage |
| Climbing over the wall (ladder) | "Climbing Ladder" | (teleport today; play the montage from Event On Hit/Interact later) |
| Cheering at the cockfight | "Cheering", "Fist Pump" | Your choice (e.g. in BP when the pit announces a winner) |
| Celebrating in the Safe Zone | "Victory", "Silly Dancing" | Your choice |

---

## 4. Build the Animation Blueprint (ABP_BorderRun)

Right-click → **Animation → Animation Blueprint** → skeleton = your body's skeleton → **ABP_BorderRun**.

### 4a. Event Graph: read the character every frame
`Event Blueprint Update Animation` → **Try Get Pawn Owner** → **Cast to BRCharacter** → save into variables:
- `Speed` = Get Velocity → Vector Length XY
- `IsFalling` = Get Character Movement → Is Falling
- `IsCrouching` = Get Character Movement → Is Crouching
- `IsAiming`, `WeaponType`, `IsRestrained`, `IsSeated`, `IsBeingCarried`, `IsCarrying`, `InDeepWater`, `IsWading`, `HoldProgress`, `StruggleProgress`
  (all are BRCharacter functions under **BR → Anim** / **PVV → Anim**)

### 4b. Anim Graph: base state machine
**State Machine "Body"** with states and transitions:
- **Locomotion** (Blend Space 1D on Speed: Drunk Idle → Drunk Walk → Run). Make a Crouch version too.
- **Air** (IsFalling): flail loop
- **Swim** (InDeepWater): treading water
- **Restrained** (IsRestrained): arms behind back; blend in a struggle loop by StruggleProgress
- **Seated** (IsSeated): sitting idle
- **Carried** (IsBeingCarried): limp hang
- **Working** (HoldProgress > 0): working loop

### 4c. Upper-body layer for aiming and carrying
State Machine output → **Layered blend per bone** (Branch Filter: Bone Name `spine_01`, Blend Depth 1):
- Blend Pose 0 = **Aim pose** (pick by WeaponType using **Blend Poses by int**), weight = `IsAiming ? 1 : 0`
- Add a second layer for **Carrying** with the carry pose, weight = `IsCarrying`

### 4d. Montages (punch, fire, reload, eat)
- Before the Output Pose, add a **Slot 'DefaultSlot'** node (or a "UpperBody" slot inside the layered blend so you can shoot while running).
- Right-click each animation → **Create → Create AnimMontage**.
- In **BP_Runner / BP_Warden** Event Graph:
  - **Event On Attack Started** → Get Mesh → **Play Montage** (Punch)
  - **Event On Weapon Fired** → Play Montage (Fire)
  - **Event On Reload Started** → Play Montage (Reload)
  - **Event On Ate** → Play Montage (Eat)

### 4e. Use it
BP_Runner / BP_Warden → **Mesh → Anim Class = ABP_BorderRun**. The physics body follows it automatically.

---

## 5. Suggested order
1. Locomotion only (idle/walk/run, drunk versions) → playtest.
2. Aim layer + fire/reload montages (Lyra pistol set).
3. Punch montage.
4. Restrained / carried / seated / swim states.
5. Working loop, eat, cheer, victory.
