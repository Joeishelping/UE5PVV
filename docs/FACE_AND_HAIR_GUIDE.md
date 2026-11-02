# Face shapes, hairstyles, mustaches & beards

I already did the Blender part for you. In `Art/Characters` you'll find:
- **SK_PVV_Body_Custom.glb**: your body, now with **8 shape keys**:
  - Head_Big, Head_Small
  - Nose_Big, Nose_Small
  - Lips_Big, Lips_Small
  - Ears_Big, Ears_Small
- **15 hairstyles** (`SK_Hair_...`):
  - Buzz, Fade, SlickBack, CurlyTop, Mullet, LongWavy
  - HighAndTight, CrewCut, FlatTop, SidePart
  - Afro, Waves, Pompadour, Twists, Cornrows
- **8 mustaches and beards** (`SK_Face_...`):
  - Mustache, Pencil, Handlebar, Horseshoe
  - FullBeard, Stubble, Goatee, ChinStrap
- Pictures of all of them in `Art/Characters/Previews`: **FaceShapes.png**, **Hairstyles.png** and **FacialHair.png**.

Hair, hats and glasses grow and shrink with the Head slider, so they always fit.

In the game menu, after you pick a team, you get **Head, Nose, Lips and Ears** sliders (middle = normal), **Hair < >**, **Beard < >** and **Hair color**.

---

## Step 0: Get the update
Same as always:
1. Copy the new **Source** folder into your project. Also copy the **Art** folder somewhere you can find it, for example your Desktop.
2. Delete **Binaries** and **Intermediate**.
3. Right-click the .uproject → **Generate Visual Studio project files**.
4. Double-click the .uproject → **Yes** to rebuild.

---

## Step 1: Re-import your body with the shape keys (5 min)
1. In the Content Browser, find your body mesh **SK_PVV_Body_Custom**.
2. Right-click it → **Reimport With New File** → pick the new `Art/Characters/SK_PVV_Body_Custom.glb`.
3. In the import window, make sure **Import Morph Targets** is ticked (under Mesh). Click **Import**.
4. **Check it worked:**
   1. Double-click the mesh.
   2. Open **Window → Morph Target Previewer** (or the **Morph Target Previewer** tab).
   3. You should see the 8 names. Drag the sliders and watch the face change.

> If "Reimport With New File" isn't there, just drag the new .glb into the same folder and click **Yes / Replace**. Keep the same skeleton.

## Step 2: Import the hair and beards (5 min)
1. In the Content Browser, make a folder **Characters → Hair**.
2. Drag in **all** the `SK_Hair_*.glb` and `SK_Face_*.glb` files. In the import window:
   - **Skeleton** = your body's skeleton (the same one your outfits use). This part matters.
   - **Import Morph Targets** ✓
   - Click **Import All**.
3. Also re-import **SK_Outfit_CowboyHat**, **SK_Outfit_PatrolCap**, **SK_Outfit_Sunglasses** and **SK_PVV_Hair_01** the same way as Step 1. They now have the Head_Big/Head_Small shape keys, so they grow with the head.

## Step 3: Give the hairstyles to your characters (5 min)
1. Open **BP_Runner** → **Class Defaults** (top toolbar) → search **Hair Styles**.
2. Click **+** once for each style you want, then pick a mesh in each box (`SK_Hair_Fade`, `SK_Hair_Mullet`...). The order you add them is the order you scroll through in the menu.
3. Under it, **Facial Hair Styles**: same thing with the `SK_Face_...` meshes.
4. Do the same in **BP_Warden**. Every style works for both teams, so add whatever you want each side to have.
5. If your old hair is in the **OutfitHead** or **OutfitFace** component, clear it there (Skeletal Mesh = None). Otherwise you'll have two haircuts on.
6. **Compile** and **Save**.

> NPC villagers and truck passengers automatically get random hair, beards and faces from your BP_Runner lists.

## Step 4 (optional): Make the hair colour slider work
The game looks for a colour setting called `HairColor`, `Tint` or `BaseColorFactor` on the hair material. Glb materials often already have `BaseColorFactor`, so it may just work. If the colour doesn't change:
1. Open one hair mesh → double-click its material.
2. Right-click → **Vector Parameter** → name it `HairColor` → plug it into **Base Color**.
3. Save. Use that same material on all the hair meshes (in each mesh's Material Slots).

## Step 5: Test
Play → **M** → pick a team. Drag **Head / Nose / Lips / Ears**, click **Hair < >** and **Beard < >**, and drag **Hair color**.

---

## Doing your own shape keys in Blender (if you want more later)
1. **File → Import → glTF** → pick `SK_PVV_Body_Custom.glb`.
2. Click the body. In the right panel, open the **green triangle tab (Object Data)** → **Shape Keys**. You'll see Basis plus my 8 keys.
3. Click **+** to add a new key. Double-click it to rename it, for example `Chin_Big`.
4. With the new key selected, press **Tab** (Edit Mode). Turn on **Proportional Editing** (press **O**), select the chin vertices, and press **S** to scale or **G** to move. Scroll the mouse wheel to change how far the effect spreads.
5. Press **Tab** to go back. Drag the key's **Value** from 0 to 1 to test it.
6. **File → Export → glTF 2.0** → Format **glb** → tick **Shape Keys** under Data → Mesh → **Export**.
7. Re-import it in Unreal (Step 1).

To get a menu slider for a new shape key, tell me the names. Use a pair like `Chin_Small` and `Chin_Big` and I'll add a slider for it.
