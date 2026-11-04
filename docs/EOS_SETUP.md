# Online multiplayer with Epic Online Services (P2P)

**What you get:**
- Players host and join each other directly (peer-to-peer). Epic's free relay servers connect them.
- **No Steam, no Radmin, no port forwarding.**
- Players sign in with a free Epic account **once**; the game remembers them after that.
- In the **M** menu:
  - **HOST ONLINE GAME**
  - **FIND GAMES** (server list: click a game to join)
  - **INVITE A FRIEND** (opens Epic's friends overlay)
  - **HOST GAME (LAN / direct IP)** and **JOIN IP** still work as a backup.

You only do this setup once. It takes about 20 minutes, and it's free.

---

## Step 1: Make your Epic developer product

1. Go to **https://dev.epicgames.com/portal** and sign in with your Epic account (make one if you don't have one).
2. If it asks, create an **Organization**. Any name works, like your own name.
3. Click **Create Product** and name it **Border Run**.
4. Open your new product. Go to **Product Settings** (in some versions this page is called **SDK Download & Credentials**). Write these down:
   - **Product ID**
   - **Sandbox ID**: use the one for your **Dev** sandbox (some versions call it **Development**).
   - **Deployment ID**: the one inside that same sandbox.

## Step 2: Make a client (the game's "key")

1. In your product, go to **Product Settings → Clients** and click **Add new client**.
2. Name it **BorderRunClient**.
3. For **Client policy**, click **Add new client policy** and pick the type **Peer2Peer**. Name it anything, then save.
4. Save the client and write down its **Client ID** and **Client Secret**.

## Step 3: Turn on Epic Account Services (the sign-in)

1. In your product, go to **Epic Account Services** and create or configure the **application**.
2. **Brand settings:** fill in the game name, your email, and anything else marked as required. A placeholder website is fine for now.
3. **Permissions:** tick **Basic Profile**, **Online Presence** and **Friends**.
4. **Linked clients:** pick **BorderRunClient**.
5. Save everything.

**Important:** until Epic approves your brand settings, only **members of your Organization** can sign in. So while you're testing, add your friends' Epic accounts: Organization → **Team members → Invite**. Getting the brand settings approved is free, and after that anyone can play.

## Step 4: Put the IDs into Unreal

1. Open the project and go to **Edit → Project Settings**.
2. On the left, under **Plugins**, click **Epic Online Services**.
3. In **Artifacts**, click **+** and fill in:
   - **Artifact Name:** `BorderRun` (exactly like that)
   - **Client Id** and **Client Secret** (from Step 2)
   - **Product Id**, **Sandbox Id**, **Deployment Id** (from Step 1)
   - **Encryption Key:** click the **Generate** button next to it. If there's no button, type any **64** characters made of **0-9 and A-F**.
4. Check these other settings (they should already be set for you):
   - **Default Artifact Name:** `BorderRun`
   - **Use Epic Account Services:** ✓
   - **Use EOS Connect:** ✓
   - **Use EOS Sessions:** ✓
5. Close Project Settings. Unreal saves this into `Config/DefaultEngine.ini` automatically.

## Step 5: Build and test with a friend

EOS **doesn't run in the editor's Play windows**, so test with a packaged game:

1. Go to **Platforms → Windows → Package Project**.
2. Send the packaged folder to your friend (zip it).
3. **You:** open the game, press **M**, click **HOST ONLINE GAME**.
   - The first time, an **Epic sign-in window** pops up. Sign in, and you're hosting.
   - Windows might ask to install "Epic Online Services" the first time. That's normal: it's the sign-in and friends overlay.
4. **Your friend:** opens the game, presses **M**, clicks **FIND GAMES** (signs in the first time), and **clicks your game** in the list.
5. Or you click **INVITE A FRIEND** and pick them from your Epic friends.
6. Everyone presses **Y** in the lobby to ready up.

You need **two PCs with two different Epic accounts** to test properly. You can't join your own game from the same account.

---

## If something goes wrong

| What you see | Fix |
|---|---|
| The status line says **"Online: NULL"** | EOS didn't start. Check every ID in Step 4 (no extra spaces), check the Artifact Name is `BorderRun`, and make sure you're running the **packaged** game. |
| **"Epic sign-in didn't work"** | Your friend isn't in your Organization yet (Step 3), or the Client isn't linked to the Epic Account Services application. |
| **FIND GAMES** shows nothing | The host must be in their game after clicking HOST ONLINE GAME. Both must be on the **same build** and the **same sandbox**. Click FIND GAMES again after a few seconds. |
| **Join fails** | Same build on both PCs? The host might have left. Try again. |
| **No sign-in window appears** | Run the game's `.exe` from the packaged folder, not from inside the editor. |

The log is in the packaged game folder under `Saved/Logs/`. Search it for **"[Online]"** and send me those lines if you're stuck.

**Going back to Steam:** in `Config/DefaultEngine.ini`, set `DefaultPlatformService=Steam`, set `bEnabled=true` under `[OnlineSubsystemSteam]`, and put the Steam net driver line back. Ask me and I'll switch it for you.
