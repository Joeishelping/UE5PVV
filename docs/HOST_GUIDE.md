# HOST GUIDE (you)

## A. One-time setup (do once)

### 1. Make sure the game opens on the right map
1. In Unreal: **File → New Level → Empty Level**.
2. **File → Save Current Level As…** → name it **`Desert`** → Save.
3. **Edit → Project Settings → Maps & Modes**:
   - **Editor Startup Map** = `Desert`
   - **Game Default Map** = `Desert`
   - **Default GameMode** = `BP_BRGameMode`
4. **Window → World Settings → GameMode Override** = **None**.
5. **File → Save All.**

### 2. Test multiplayer inside Unreal first
1. Next to the green **Play** button: click **⋮** → **Number of Players: 2** → **Net Mode: Play As Listen Server** → **Play**.
2. Two windows open. One says **RUNNER**, one says **WARDEN**. Walk around in both. ✅ If this works, the game's multiplayer works.

### 3. Package the game (turn it into a real .exe)
1. Top toolbar: **Platforms → Windows → Package Project**.
2. Make a new empty folder like **`C:\Games\Build`** → select it.
3. Wait (10–30 min the first time). Bottom right must say **"Packaging complete"**.
   - If it says **failed**: Window → Output Log → screenshot the red lines → send to Claude.
4. In `C:\Games\Build` there's now a folder called **`Windows`**. Inside it: **`PiratesVsVikings.exe`**.

### 4. Test it ALONE on your own PC (before involving your brother)
1. Double-click **`PiratesVsVikings.exe`** → the menu opens → click **HOST GAME (Radmin / LAN)**.
2. Double-click **`PiratesVsVikings.exe` again** (a second copy) → in the menu, type **`127.0.0.1`** in the IP box → **JOIN IP**.
3. If the second window joins the first one: ✅ **multiplayer works**. Close both.
   - If Windows asks about the **firewall**: click **Allow** (tick both Private and Public).

### 5. Send the game to your brother
1. Right-click the **`Windows`** folder → **Compress to ZIP file**.
2. Upload the zip to **Google Drive** (or WeTransfer) → send him the link **and** the file **BROTHER_GUIDE.md**.
3. ⚠️ **Every time you change the game, package again and send him the new zip.** Different versions can't play together.

### 6. Radmin VPN (once)
1. Install **Radmin VPN** (radmin-vpn.com) → open it.
2. **Network → Create Network** → name + password → **Create**.
3. Send your brother the **network name + password**.
4. Your **Radmin IP** is shown at the top of the Radmin window (starts with **26.**). Send it to him too.

---

## B. Every time you play together

### Option 1 — Radmin (most reliable)
1. Open **Radmin VPN** (make sure you're both online in your network — green dots).
2. Open **`PiratesVsVikings.exe`**.
3. Menu: pick your side (**RUNNER** or **BORDER PATROL**).
4. Click **HOST GAME (Radmin / LAN)**.
5. Tell your brother: "I'm hosting" → he joins with your **26.x.x.x** IP.

### Option 2 — Steam invite (try this after Radmin works)
1. Open **Steam** and log in (both of you).
2. Open **`PiratesVsVikings.exe`** → pick your side → **HOST GAME (invite Steam friends)**.
3. Press **Shift+Tab** → Steam overlay → right-click your brother → **Invite to Game**.
   (Steam calls the game "Spacewar" — that's normal.)
4. If the invite doesn't come through, use Option 1.

### In-game
- **M** = menu (switch teams) · **~** = console
- Console commands: `SkipPhase` (skip prep), `GiveMoney 500`, `SetTime 22` (night)

---

## C. If he can't join
| Problem | Fix |
|---|---|
| Nothing happens / "connection timed out" | Both check Radmin shows each other **online**. Re-check the IP (yours, starting with 26.). |
| Firewall | Windows Security → Firewall → **Allow an app** → tick **PiratesVsVikings** (Private + Public) on **both** PCs. |
| Joins then gets kicked / "version mismatch" | You're on different builds. Package again, send the new zip. |
| Works with 127.0.0.1 but not with him | It's the network: Radmin offline, wrong IP, or firewall on his side. |
