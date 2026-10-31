# PVV Character Kit

A smooth, rigged rebuild of the original bean character (`Tools/original_base_model.fbx`), with
swappable cosmetics for both factions. Everything here is meant to be used from **Blueprints only**.

| File | What it is |
|---|---|
| `SK_PVV_Body.glb` | Base body, 1.8 m tall, 4 colour slots: `M_Skin`, `M_Top`, `M_Bottom`, `M_Shoes` |
| `SK_PVV_VikingHelmet.glb` | Horned helmet (Viking) |
| `SK_PVV_Beard.glb` | Big ginger beard (Viking) |
| `SK_PVV_PirateHat.glb` | Tricorn with gold trim (Pirate) |
| `SK_PVV_Bandana.glb` | Knotted bandana (Pirate) |
| `SK_PVV_Eyepatch.glb` | Eyepatch (Pirate) |

The skeleton uses **UE mannequin bone names** (`pelvis`, `spine_01-03`, `neck_01`, `head`,
`clavicle/upperarm/lowerarm/hand_l/r`, `thigh/calf/foot/ball_l/r`). That makes the IK Retargeter
auto-map it to Manny, so you can reuse mannequin animations.

Every cosmetic is skinned to the **same skeleton**, so it follows the body perfectly, including during ragdoll.

---

## 1. Import (do the body first)

1. Make a folder `Content/Characters/PVV`.
2. Drag **SK_PVV_Body.glb** into it. In the import window:
   - Make sure it imports as a **Skeletal Mesh**.
   - Leave **Skeleton** empty so it creates a new one.
   - Tick **Create Physics Asset** if you see it.
3. You get `SK_PVV_Body`, `SK_PVV_Body_Skeleton` and (maybe) `SK_PVV_Body_PhysicsAsset`.
   If there's no physics asset, right-click the mesh → **Create → Physics Asset**.
4. Drag in each cosmetic `.glb`. In the import window set **Skeleton = SK_PVV_Body_Skeleton**.
   This is important: it's what lets them follow the body.

**If the character faces sideways or lies down:** in your character Blueprint select the **Mesh**
component and set Rotation Z = -90 (the same as Manny), then Location Z = -90 so the feet touch the
bottom of the capsule. Set the capsule to Half Height 90 and Radius 35.

---

## 2. Physics body (wobbly "Gang Beasts" style)

The idea: animations still play, but every body part is a physics body that is *pulled toward* the
animation. Hits, bumps and boats knock limbs around, and knockdowns become a full ragdoll.

### 2a. Tune the Physics Asset
Open `SK_PVV_Body_PhysicsAsset`:
- Make sure every bone below `pelvis` has a body. The auto-generated capsules are fine to start.
  Make the **head** a sphere about 28 cm across.
- Constraints: give `calf_l/r` and `lowerarm_l/r` a **Limited** swing on one axis only, so knees
  and elbows hinge. Limit `neck_01`/`head` to about 30°.
- Select all bodies → **Physics Type = Simulated**.

### 2b. Character Blueprint
1. **Add Component → Physical Animation**.
2. **Event BeginPlay**:
   - `Physical Animation` → **Set Skeletal Mesh Component** (Mesh)
   - `Physical Animation` → **Apply Physical Animation Settings Below**
     - Body Name: `pelvis`, Include Self: **false**
     - Orientation Strength **1000**, Angular Velocity Strength **100**, Position Strength **0**, Velocity Strength **0**
   - `Mesh` → **Set All Bodies Below Simulate Physics** (`pelvis`, true, Include Self: false)
   - `Mesh` → **Set All Bodies Below Physics Blend Weight** (`pelvis`, 1.0)
3. **Collision:** set Mesh → Collision Presets to **Custom**, with Collision Enabled = *Query and Physics*,
   Object Type = *PhysicsBody*, and **Pawn: Ignore**, so limbs don't fight your own capsule.

Lower strengths (300 / 30) make it floppier, and higher ones make it stiffer. Tweak to taste.

### 2c. Hits and knockdowns
- **Getting punched:** `Mesh → Add Impulse to All Bodies Below` (Bone `spine_02`, Impulse = punch direction × 300, Vel Change ✓).
- **Knocked down (full ragdoll):** run *Apply Physical Animation Settings Below* again with **all strengths 0**,
  then `Mesh → Set All Bodies Below Simulate Physics (pelvis, true, Include Self ✓)`.
- **Getting back up:** set Include Self back to false for pelvis, then restore the strengths over about 0.5 s with a Timeline for a funny wobbly stand-up.

**Multiplayer rule:** the wobble is **cosmetic**. Each machine simulates its own. The **capsule**
position, the `Condition` RepNotify (Normal/KnockedDown/…) and hits are decided on the server, as in the
C++ prototype. Trigger the ragdoll from **OnRep_Condition** and the hit wobble from a **Multicast** event.

---

## 3. Customization (both factions)

### 3a. Colour material
Create one material, `M_PVV_Color`:
- **Vector Parameter** named `Color` → Base Color
- Scalar `Roughness` (0.8) and `Metallic` (0)

In `SK_PVV_Body`, set all 4 slots to `M_PVV_Color`. Do the same for the cosmetics. The helmet can use a
metallic copy.

### 3b. Data
- **Enum `E_CosmeticSlot`:** Hat, Face, Beard
- **Struct `S_Cosmetic`:** Name (Text), Mesh (Skeletal Mesh), Slot (E_CosmeticSlot), Faction (E_Faction, None = both), Icon (Texture2D)
- **Data Table `DT_Cosmetics`** (row = S_Cosmetic). Starter rows:

| Row | Mesh | Slot | Faction |
|---|---|---|---|
| VikingHelmet | SK_PVV_VikingHelmet | Hat | Vikings |
| PirateHat | SK_PVV_PirateHat | Hat | Pirates |
| Bandana | SK_PVV_Bandana | Hat | Pirates |
| Eyepatch | SK_PVV_Eyepatch | Face | Pirates |
| Beard | SK_PVV_Beard | Beard | None (anyone) |

- **Struct `S_Look`:** SkinColor, TopColor, BottomColor, ShoesColor (Linear Color); Hat, Face, Beard (Name = row in DT_Cosmetics)

### 3c. Character Blueprint
1. Add 3 **Skeletal Mesh** components as children of `Mesh`: `HatMesh`, `FaceMesh`, `BeardMesh`.
2. In **BeginPlay**, for each one: **Set Leader Pose Component** (New Leader = Mesh). They now copy
   every bone, including ragdoll.
3. Make a function **ApplyLook(S_Look)**:
   - For each slot: Get Data Table Row → **Set Skeletal Mesh Asset** (none = hide).
   - Body colours: `Mesh → Get Material Index("M_Skin")` → **Create Dynamic Material Instance** (that index)
     → **Set Vector Parameter Value** ("Color", SkinColor). Repeat for Top, Bottom and Shoes. Cache the 4 MIDs.

### 3d. Replication (so everyone sees your outfit)
- Put a **`Look` (S_Look) RepNotify** variable on **BP_PVVPlayerState**. The look belongs to the player, not the body.
- In the customization menu: **Server_SetLook(S_Look)** (Run on Server) → sets `Look` (check that items match the player's faction).
- **OnRep_Look** → get the Pawn → `ApplyLook`. Also call `ApplyLook` from the character's **Possessed / OnRep_PlayerState**,
  so late joiners and respawns get it too.
- Save the player's choice locally with **Save Game to Slot** and send it with `Server_SetLook` on join.

### 3e. Faction defaults and ideas
- Vikings: helmet + beard, top in Viking orange (ICE-style "VIKING" lettering can be a decal later).
- Pirates: tricorn or bandana + eyepatch, teal top.
- **Body shape sliders:** in the **Anim Blueprint**, add **Transform (Modify) Bone** nodes driven by
  variables, such as `BellySize` → scale `spine_02`, `HeadSize` → scale `head`, `LegLength` → scale `thigh_l/r`.
  Store them in `S_Look` as floats.
- More cosmetics: model them in Blender **on this skeleton** (import `SK_PVV_Body.glb` into Blender,
  parent the new piece to the armature, weight it 100% to `head` for hats). Export glTF, then import with
  `SK_PVV_Body_Skeleton`.

---

## Regenerating the meshes

Everything is generated by `Tools/build_character.py` (Python 3 + numpy). Tweak the body shape
numbers and re-run it to get new `.glb` files. `Tools/preview_character.py` renders the preview images.
