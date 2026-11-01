# Pirates vs. Vikings — Blueprint-Only Build Guide

This rebuilds everything from the C++ prototype using **only Blueprints**: teams, movement, punching,
knockdowns, zip-ties, carrying, boats, detention, jailbreaks, scoring and HUD.

**How to read the node steps:** `A → B → C` means wire the white execution pins from A to B to C.
`(Server)` means a Custom Event with **Replicates: Run on Server**. `(Multicast)` = **Multicast**.
`(Client)` = **Run on owning Client**. Variables marked **RepNotify** use *Replication: RepNotify*.

**Golden rule of multiplayer:** clients only *ask* (Run on Server events). The **server decides** and
changes variables. **RepNotify** functions update the visuals on every machine. In Blueprint, RepNotify
functions also run on the server when the server sets the variable, so you never call them by hand.

**Build order:** do the steps in order and **test with 2 players after each step**. To test, go to Play ▸ ⋮ ▸
Number of Players **2** ▸ Net Mode **Play As Listen Server**.

---

## Step 0 — Project and basic assets

1. New project: **Games → Third Person → Blueprint**, UE 5.6. Name it `PiratesVsVikings`.
2. Create folders: `Content/PVV/{Core, Characters, Boats, World, UI, Input, Data}`.
3. **Enums** (right-click → Blueprint → Enumeration):
   - `E_Faction`: None, Pirates, Vikings
   - `E_Condition`: Normal, KnockedDown, Restrained, Detained
   - `E_MatchPhase`: InProgress, Ended
4. **Blueprint Interface** `BPI_Interactable` with 4 functions. Create `BP_PVVCharacter` (Step 3) first so you
   can pick it as a type, or use the type *Character* and cast later.
   - `CanInteract(Interactor: BP_PVVCharacter)` → returns `Result (bool)`
   - `GetInteractDuration(Interactor)` → returns `Seconds (float)`. 0 = instant tap, more than 0 = hold.
   - `GetInteractPrompt(Interactor)` → returns `Prompt (Text)`
   - `Interact(Interactor)` (no output)
5. **Input** (`Content/PVV/Input`). The template already has `IA_Move`, `IA_Look`, `IA_Jump` and `IMC_Default`. Add:
   - `IA_Sprint` (Digital bool) → Left Shift
   - `IA_Attack` (Digital bool) → Left Mouse Button
   - `IA_Interact` (Digital bool) → E
   - Add those 3 to **IMC_Default**.
   - Boat: `IA_Throttle` (Axis1D): W = +1, S = Negate. `IA_Steer` (Axis1D): D = +1, A = Negate.
     `IA_ExitBoat` (bool): E. Put these plus `IA_Look` in a new **IMC_Boat**.
6. **Collision channel:** Project Settings → Collision → New Object Channel **`Boat`** (Default: Block).

---

## Step 1 — PlayerState, GameState, PlayerStart

### BP_PVVPlayerState (parent: PlayerState)
| Variable | Type | Replication |
|---|---|---|
| Faction | E_Faction | RepNotify |
| Captures, Detentions, Rescues, TimesDetained | Integer | Replicated |

- **OnRep_Faction:** leave it empty for now. It's handy later for the HUD.

### BP_PVVGameState (parent: GameStateBase)
| Variable | Type | Replication |
|---|---|---|
| PirateScore, VikingScore, RemainingSeconds | Integer | Replicated |
| Winner | E_Faction | Replicated |
| MatchPhase | E_MatchPhase | RepNotify |

- **Event Dispatcher** `OnAnnouncement (Message: Text)` and `OnMatchEnded (Winner: E_Faction)`.
- Custom Event **`MC_Announce (Message: Text)`** (Multicast, Reliable) → *Call OnAnnouncement*.
- **OnRep_MatchPhase:** `Branch (MatchPhase == Ended)` → *Call OnMatchEnded (Winner)*.
- Function **`AddScore(Faction, Points)`:** Switch on Faction → Pirates: `PirateScore += Points` / Vikings: `VikingScore += Points`.

### BP_PVVPlayerStart (parent: PlayerStart)
- Variable `Faction` (E_Faction), **Instance Editable** ✓. Place 3–4 per team in your map.

---

## Step 2 — GameMode and PlayerController

### BP_PVVPlayerController (parent: PlayerController)
- Custom Event **`SR_RequestFaction (Faction)`** (Server, Reliable) → Get Game Mode → Cast to BP_PVVGameMode → `RequestFaction(Self, Faction)`.
- Custom Event **`CL_ShowFactionSelect`** (Client, Reliable) → Create Widget `WBP_FactionSelect` → Add to Viewport →
  Set Show Mouse Cursor ✓ → Set Input Mode UI Only. Make the widget in Step 9. Skip this until then.
- **Testing keys:** in Event Graph, add Keyboard **F1** → `SR_RequestFaction(Pirates)`, Keyboard **F2** → `SR_RequestFaction(Vikings)`.

### BP_PVVGameMode (parent: GameModeBase)
**Class Defaults:** Default Pawn = `BP_PVVCharacter`, Player Controller = `BP_PVVPlayerController`,
Player State = `BP_PVVPlayerState`, Game State = `BP_PVVGameState`.
Then Project Settings → Maps & Modes → Default GameMode = `BP_PVVGameMode`.

| Variable | Type | Default |
|---|---|---|
| PirateCharacterClass, VikingCharacterClass | Class ref (Character) | your BP_Pirate / BP_Viking (or BP_PVVCharacter) |
| AutoAssignFactions | Bool | ✓ |
| MatchDurationSeconds | Integer | 600 |
| PointsCapture / PointsDetain / PointsRescue / PointsJailbreak | Integer | 1 / 3 / 2 / 2 |

**Override `HandleStartingNewPlayer`** (Functions ▸ Override). Do **not** call the parent, because we spawn people ourselves:
`Branch (AutoAssignFactions)` → True: `RequestFaction(NewPlayer, PickBalancedFaction())` / False: Cast to BP_PVVPlayerController → `CL_ShowFactionSelect`.

**Function `PickBalancedFaction` → E_Faction:**
Get Game State → Player Array → For Each: Cast to BP_PVVPlayerState → count Pirates and Vikings into two local ints →
`Select (Vikings < Pirates ? Vikings : Pirates)`.

**Function `RequestFaction(PC: PlayerController, Faction)`:**
1. PS = PC → Get Player State → Cast to BP_PVVPlayerState.
2. If PC → Get Controlled Pawn is a **BP_Boat** → Boat → `Unseat(Boat.GetDriver)`. That gives the player their body back.
3. If PC → Get Controlled Pawn is valid → Destroy Actor.
4. PS → Set **Faction** (w/ Notify).
5. **Restart Player** (PC).
6. GameState → `MC_Announce( Format Text "{0} joined the {1}" )`.

**Override `GetDefaultPawnClassForController`:** Cast Player State → Switch on Faction → return PirateCharacterClass / VikingCharacterClass.

**Override `ChoosePlayerStart`:**
Get All Actors of Class `BP_PVVPlayerStart` → filter where `Faction == Player's PS Faction` → Random Array Item → Return.
If none, return Parent: ChoosePlayerStart.

**Match clock:** Event BeginPlay → GameState `RemainingSeconds = MatchDurationSeconds` → **Set Timer by Event** (1.0 s, Looping) →
event `TickClock`: if MatchPhase == InProgress → `RemainingSeconds -= 1` → if 0 → `EndMatch(higher score, or None if tied)`.

**Function `EndMatch(Winner)`:** GS Set Winner → GS Set **MatchPhase = Ended** (w/ Notify) → Announce "The {Winner} win!" →
Delay 15 → **Execute Console Command** `ServerTravel YourMapName`.

**Scoring functions** (called by characters, server only):
- `NotifyCaptured(Victim, Captor)`: GS AddScore(Captor.Faction, PointsCapture) → Captor PS Captures +1 → Announce "{Captor} zip-tied {Victim}!"
- `NotifyFreed(Victim, Rescuer)`: if Rescuer valid → AddScore + Rescues +1 + "cut loose" message; else "{Victim} wriggled free!"
- `NotifyDetained(Victim, Captor)`: AddScore(PointsDetain) → stats → Announce → **CheckWin**
- `NotifyJailbreak(Rescuer, Count)`: AddScore(PointsJailbreak × Count) → Announce "JAILBREAK!"
- `CheckWin`: Get All Actors of Class BP_PVVCharacter → count Pirates and Detained Pirates → if Pirates > 0 and equal → `EndMatch(Vikings)`.
- `Respawn(Character)`: pick a start like ChoosePlayerStart → Character → `ResetForRespawn(Start transform)`.

Player names: `Pawn → Get Player State → Get Player Name`.

✅ **Test:** 2 players spawn at different team starts. F1 and F2 switch teams.

---

## Step 3 — The character (movement and state)

Make **BP_PVVCharacter**: duplicate `BP_ThirdPersonCharacter`, or create it with parent BP_ThirdPersonCharacter so it keeps the move/look/jump setup.
Add a **Text Render** component `StatusLabel` above the head (Z = 125, centred).

### Variables
| Variable | Type | Replication |
|---|---|---|
| Faction | E_Faction | RepNotify |
| Condition | E_Condition | RepNotify |
| CarriedBy | BP_PVVCharacter (object ref) | RepNotify |
| CarriedCharacter | BP_PVVCharacter | RepNotify |
| CurrentBoat | BP_Boat | RepNotify |
| Balance | Float (100) | Replicated |
| StruggleProgress | Float | Replicated |
| HoldProgress | Float | Replicated, Condition **Owner Only** |
| IsSprinting | Bool | Replicated, Condition **Skip Owner** (RepNotify → UpdateSpeed) |
| HoldTarget, HoldElapsed, HoldDuration, LastAttackTime, LastDamageTime, LastStruggleTime | various | not replicated (server only) |
| FocusedActor (Actor), FocusPrompt (Text) | | not replicated (local only) |
| Tuning: WalkSpeed 500, SprintSpeed 800, RestrainedSpeed 140, CarrySpeedMult 0.7, MaxBalance 100, AttackDamage 25, AttackCooldown 0.55, KnockdownTime 8, CaptureTime 1.5, RescueTime 3, HelpUpTime 2, InteractRadius 250 | Float | — |

**Every OnRep_ function** (Faction, Condition, CarriedBy, CarriedCharacter, CurrentBoat) just calls **`RefreshState`**.

### Getting the faction
**Event Possessed** (server) → Get Player State → Cast BP_PVVPlayerState → Set **Faction** (w/ Notify) = PS.Faction.

### Function `RefreshState` (runs on every machine)
1. `Attached = IsValid(CarriedBy) OR IsValid(CurrentBoat)`
2. **Movement:** Branch Attached → True: Character Movement → **Disable Movement** / False: if Movement Mode == None → **Set Movement Mode (Walking)**.
3. **Collision:** Capsule → **Set Collision Response to Channel** (Pawn → Ignore if CarriedBy valid, else Block), and (Boat → Ignore if CurrentBoat valid, else Block).
4. **Ragdoll:** `WantRagdoll = Condition == KnockedDown AND NOT Attached`.
   - Turning on: Mesh → Set Collision Profile Name **"Ragdoll"** → Mesh → **Set Simulate Physics ✓**.
   - Turning off: Mesh → Set Simulate Physics ✗ → Set Collision Profile **"CharacterMesh"** →
     **Attach Component To Component** (Mesh → Capsule, Snap to Target) → Set Relative Location (0,0,-90) and Rotation (0,0,-90).
   - Keep a bool `RagdollOn` so you only switch when it changes. You can swap this part for the physical-animation body later (see `Art/Characters/README.md`).
5. **UpdateSpeed** (function): Speed = IsSprinting ? Sprint : Walk. If KnockedDown → 0. If Restrained → RestrainedSpeed.
   If CarriedCharacter valid → × CarrySpeedMult. → Character Movement **Set Max Walk Speed**.
6. **Label:** StatusLabel Set Text = Faction name + condition. Set Text Render Color: Pirates teal, Vikings orange.

### Functions `CanMove` / `CanAttack` (pure)
- CanMove = NOT CarriedBy valid AND NOT CurrentBoat valid AND Condition ≠ KnockedDown
- CanAttack = Condition == Normal AND NOT CarriedCharacter valid AND NOT CarriedBy valid

### Input
- **IA_Move** (Triggered): Branch **CanMove** → (template move nodes).
- **IA_Jump** (Started): `Condition == Restrained` → **SR_Struggle** / else if CurrentBoat valid → **SR_LeaveSeat** / else if CanMove → **Jump**.
- **IA_Sprint** Started → Set IsSprinting ✓ → UpdateSpeed → **SR_SetSprinting(true)**. Completed → same with false.
  `SR_SetSprinting` (Server) → Set IsSprinting → UpdateSpeed.
- **IA_Attack** Started → Branch CanAttack → **SR_Attack**.
- **IA_Interact** Started → **SR_InteractPressed** · Completed → **SR_InteractReleased**.

### Mapping context (so boats can swap it)
**Event Receive Controller Changed** → Is Locally Controlled → Cast New Controller to PlayerController →
Get **Enhanced Input Local Player Subsystem** → **Clear All Mappings** → **Add Mapping Context (IMC_Default, 0)**.
Delete the template's BeginPlay "Add Mapping Context" so it's only done here.

✅ **Test:** walking, sprinting and jumping work for both players, and the labels show the team.

---

## Step 4 — Punching, balance, knockdown

### `SR_Attack` (Server, Reliable)
`Branch (CanAttack AND GameTime − LastAttackTime ≥ AttackCooldown)` → Set LastAttackTime → **MC_AttackStarted** →
Delay 0.15 → Branch CanAttack → **AttackSweep**.

`MC_AttackStarted` (Multicast, Unreliable) → **Play Montage** (a punch montage; a mannequin attack anim works).

### Function `AttackSweep`
1. Aim = CurrentBoat valid ? (Get Control Rotation yaw) : Get Actor Rotation yaw → forward vector.
2. **Sphere Overlap Actors**: Location = ActorLocation + Forward × 90, Radius 70, Object Types [Pawn], Class Filter BP_PVVCharacter, Actors to Ignore [Self].
3. For Each Victim:
   - Dir = (Victim loc − my loc) normalized, Z = 0
   - Hostile = Faction ≠ Victim.Faction (and neither is None)
   - Victim → **ReceiveHit**(Self, Hostile ? AttackDamage : 0, Dir × 650 + (0,0,250))

### Function `ReceiveHit(Attacker, Damage, Impulse)` (server)
1. If CarriedBy valid → return (carried prisoners are immune).
2. **MC_HitReceived** (Multicast → play hit sound or flinch) → **CancelHold**.
3. If Damage > 0 AND Condition == Normal: Balance −= Damage → Set LastDamageTime → if Balance ≤ 0 → **Knockdown**(Attacker, Impulse) → return.
4. If NOT CurrentBoat valid → **Launch Character**(Impulse, XY Override ✓, Z Override ✓).

### Function `Knockdown(Cause, Impulse)` (server)
Branch (Condition == Normal AND NOT CarriedBy valid) →
if CurrentBoat valid → CurrentBoat.**Unseat**(Self) →
if CarriedCharacter valid → **DropCarried** → CancelHold → Balance = 0 →
Set **Condition = KnockedDown** (w/ Notify) → Launch Character(Impulse) →
**Set Timer by Event** (KnockdownTime) → event `RecoverTimer` → **Recover(0.5)**. Save the handle as `KnockdownTimer`.

### Function `Recover(BalanceFraction)`
Branch Condition == KnockedDown → Clear and Invalidate Timer (KnockdownTimer) → Balance = MaxBalance × Fraction → Set **Condition = Normal**.

### Event Tick (server part)
`Switch Has Authority` → Authority:
- If Condition == Normal AND Balance < Max AND (GameTime − LastDamageTime > 3) → Balance = Min(Max, Balance + 15 × Delta)
- StruggleProgress = Max(0, StruggleProgress − 0.12 × Delta)
- **TickHold(Delta)** (Step 5)
- If Actor Location Z < −2000 → GameMode **Respawn(Self)**

✅ **Test:** punch the other team 4 times. They ragdoll and get up after 8 s. Teammates only get pushed.

---

## Step 5 — Interaction, zip-ties, carrying, struggling

### Function `FindBestInteractable` → Actor
Sphere Overlap Actors (ActorLocation, InteractRadius, Object Types [Pawn, Boat, WorldDynamic, PhysicsBody], Ignore [Self, CarriedCharacter]) →
For Each → Branch **Does Implement Interface (BPI_Interactable)** AND `CanInteract(Self)` (interface message) →
Score = Dot(Get Base Aim Rotation forward (Z=0, normalized), direction to actor) − Distance / InteractRadius
(+0.5 if it's a BP_PVVCharacter, so people beat boats) → keep the highest → return it.

### Local focus (for the HUD)
Event Tick → Branch **Is Locally Controlled** → FocusedActor = (Condition == Normal AND NOT CarriedBy valid) ? FindBestInteractable : None →
FocusPrompt = FocusedActor valid ? `GetInteractPrompt(Self)` : (CarriedCharacter valid ? "Drop prisoner" : empty).

### `SR_InteractPressed` (Server, Reliable)
Branch (Condition == Normal AND NOT CarriedBy valid AND NOT HoldTarget valid) →
Target = FindBestInteractable →
- Not valid → if CarriedCharacter valid → **DropCarried**. Done.
- Duration = Target.`GetInteractDuration(Self)` → ≤ 0 → Target.`Interact(Self)`
- else → HoldTarget = Target, HoldDuration = Duration, HoldElapsed = 0, HoldProgress = 0

`SR_InteractReleased` (Server) → **CancelHold** (HoldTarget = None, HoldElapsed = 0, HoldProgress = 0).

### Function `TickHold(Delta)` (server)
If HoldTarget valid:
- Still OK = Condition == Normal AND HoldTarget.CanInteract(Self) AND (HoldTarget isn't a character OR distance ≤ 375)
- Not OK → CancelHold
- OK → HoldElapsed += Delta → HoldProgress = HoldElapsed / HoldDuration → if ≥ 1 → save target → CancelHold → target.**Interact(Self)**

### The character as an interact target
Class Settings → Interfaces → Add **BPI_Interactable**. Implement:

**CanInteract(Interactor):**
- False if: Interactor == Self, CarriedBy valid, Interactor.CarriedCharacter valid, Interactor.Condition ≠ Normal, or my Condition is not KnockedDown/Restrained.
- Teammate = Interactor.Faction == Faction
- Captor = Interactor.Faction == **Vikings** AND Faction == **Pirates**
- If Captor AND Condition == Restrained AND Interactor.CurrentBoat valid → False
- Return Teammate OR Captor

**GetInteractDuration:** KnockedDown → Teammate ? HelpUpTime : CaptureTime · Restrained → Teammate ? RescueTime : 0

**GetInteractPrompt:** KnockedDown → "Help up (hold)" / "Zip-tie (hold)" · Restrained → "Cut loose (hold)" / "Pick up"

**Interact(Interactor):**
- KnockedDown + Teammate → **Recover(1.0)**
- KnockedDown + Captor → **Restrain(Interactor)**
- Restrained + Teammate → **Release(Interactor)**
- Restrained + Captor → Interactor.**StartCarrying(Self)**

### Server functions
- **Restrain(Captor):** if KnockedDown → Clear KnockdownTimer → StruggleProgress = 0 → Set **Condition = Restrained** → GameMode NotifyCaptured(Self, Captor)
- **Release(Rescuer):** if Restrained → if CarriedBy valid → CarriedBy.**TakeCarried** → if CurrentBoat valid → CurrentBoat.**Unseat**(Self) →
  StruggleProgress = 0, Balance = 50 → Set **Condition = Normal** → GameMode NotifyFreed(Self, Rescuer)
- **StartCarrying(Prisoner):** Branch (Prisoner.Condition == Restrained, not already carrying or carried, not in a boat) →
  if Prisoner.CurrentBoat valid → Unseat(Prisoner) → CancelHold → Set CarriedCharacter = Prisoner (w/ Notify) →
  Prisoner Set CarriedBy = Self (w/ Notify) → Prisoner **Attach Actor To Component** (Parent: my Capsule, Snap to Target) →
  Prisoner Set Actor Relative Location (10, 0, 110) and Rotation (90, 0, 90)
- **TakeCarried → BP_PVVCharacter:** P = CarriedCharacter → Set CarriedCharacter = None → P Set CarriedBy = None →
  P **Detach From Actor** (Keep World) → P Set Actor Rotation (0, my yaw, 0) → return P
- **DropCarried:** P = TakeCarried → if valid → P **Teleport** (ActorLocation + Forward × 90, my rotation)

### Struggling
`SR_Struggle` (Server, **Unreliable**): Branch (Condition == Restrained AND GameTime − LastStruggleTime ≥ 0.06) → set time →
StruggleProgress += 0.07 × (CarriedBy valid ? 0.6 : 1) → if ≥ 1 → **BreakFree**.

**BreakFree:** Carrier = CarriedBy → **Release(None)** → if Carrier valid → Launch Self (−Carrier forward × 300 + (0,0,450)) and
Launch Carrier (Carrier forward × 400 + (0,0,150)) so the carrier stumbles.

✅ **Test:** knock a Pirate down as a Viking → hold E (zip-tie) → tap E (carry) → the Pirate mashes Space and escapes.
A second Pirate can hold E to cut a tied teammate loose.

---

## Step 6 — Boats

### BP_Boat (parent: **Pawn**)
Components:
- **Hull** (Static Mesh, **root**): Simulate Physics ✓, Mass override 600 kg, Collision Preset Custom → Object Type **Boat**, Camera: Ignore,
  **Simulation Generates Hit Events ✓**, Linear Damping 0.1, Angular Damping 1.5.
- **Seat0** (driver), **Seat1**, **Seat2**, **Seat3**: Scene components placed on the deck, about 95 cm above it (capsule centre).
- **Pontoon0…5**: Scene components at the hull's bottom corners and middle sides.
- SpringArm (Use Pawn Control Rotation ✓, length 900, Camera Lag ✓) + Camera.

Class Defaults: Replicates ✓, **Replicate Movement ✓**, Auto Possess AI: Disabled, Use Controller Rotation Yaw ✗.

| Variable | Type | Replication |
|---|---|---|
| Occupants | Array of BP_PVVCharacter (4 entries, all None) | Replicated |
| Throttle, Steer | Float | Replicated, **Skip Owner** |
| WaterZ | Float | — |
| Tuning: Buoyancy 2.0, PontoonDepth 50, VertDamping 2, ForwardAccel 900, ReverseAccel 400, ForwardDrag 0.7, LateralGrip 2.5, MaxTurnRate 1.4, TurnResponse 3, FullSteerSpeed 600, RamKnockdownSpeed 700 | Float | — |

**BeginPlay:** Get Actor of Class `BP_Lake` → WaterZ = its Z. Make a `GetSeats` function that returns [Seat0..3] and `GetPontoons` that returns [Pontoon0..5].

### Floating (Event Tick, on every machine)
For each Pontoon P:
- Depth = WaterZ − P.WorldLocation.Z → if > 0:
- Lift = (Hull Mass × 980 × Buoyancy / 6) × Clamp(Depth / PontoonDepth, 0, 1) − (Hull **Get Physics Linear Velocity at Point**(P).Z × Hull Mass / 6 × VertDamping)
- Hull **Add Force at Location** ((0,0,Lift), P location). Count the wet pontoons.

If any are wet (Wet = count / 6):
- Fwd = Hull forward with Z = 0, normalized; Right likewise. Vel = Hull Get Physics Linear Velocity.
- FwdSpeed = Vel · Fwd, LatSpeed = Vel · Right
- Accel = Fwd × (Throttle ≥ 0 ? Throttle × ForwardAccel : Throttle × ReverseAccel) − Fwd × FwdSpeed × ForwardDrag − Right × LatSpeed × LateralGrip
- Hull **Add Force** (Accel × Wet, **Accel Change ✓**)
- Authority = Clamp(|FwdSpeed| / FullSteerSpeed, 0.3, 1). Dir = FwdSpeed < −50 ? −1 : 1
- TargetYaw = Steer × MaxTurnRate × Authority × Dir; CurrentYaw = Hull Get Physics Angular Velocity in Radians . Z
- Hull **Add Torque in Radians** ((0,0,(TargetYaw − CurrentYaw) × TurnResponse × Wet), **Accel Change ✓**)

Easier alternative: enable the **Water** plugin, use a **Water Body Lake**, and add a **Buoyancy** component with pontoons instead of the lift part.

### Driving input
- **Event Receive Controller Changed** → Is Locally Controlled → Subsystem → Clear All Mappings → Add Mapping Context **IMC_Boat**.
- IA_Throttle Triggered → Set Throttle = value → **SendInput** · Completed → Throttle = 0 → SendInput (same for Steer).
- `SendInput`: if values changed since last send → **SR_SetDriveInput**(Throttle, Steer) (Server, **Unreliable**) → sets both on the server.
- IA_Look → Add Controller Yaw/Pitch Input. IA_ExitBoat Started → **SR_Exit** (Server) → **Unseat**(GetDriver).
- `GetDriver` = Occupants[0].

### Seats (server functions)
- **FindFreePassengerSeat → int:** indices 1–3, first None, else −1.
- **SeatCharacter(C, Index):** Occupants[Index] = C → Ctrl = C.Get Controller → C.**EnterSeat**(Self, Seats[Index]) → if Index == 0 AND Ctrl valid → Ctrl **Possess**(Self).
- **Board(C):** if Normal AND controlled AND Occupants[0] is None → SeatCharacter(C, 0), else a free passenger seat.
- **LoadPrisoner(Carrier):** Seat = FindFreePassengerSeat → P = Carrier.TakeCarried → SeatCharacter(P, Seat).
- **Unseat(C):** Index = Occupants Find C → Ctrl = (Index == 0) ? Get Controller : None → Occupants[Index] = None →
  if Index == 0 → Throttle = Steer = 0 → Exit = seat location + Right × (±180) + (0,0,40) → C.**ExitSeat**(Exit, my yaw) →
  if Ctrl valid → **UnPossess** → **Possess**(C).

On the character (server):
- **EnterSeat(Boat, Seat):** CancelHold → Set CurrentBoat = Boat (w/ Notify) → **Attach Actor To Component** (Seat, Snap to Target).
- **ExitSeat(Location, Yaw):** Set CurrentBoat = None → Detach From Actor (Keep World) → Teleport(Location, (0,Yaw,0)).
- **SR_LeaveSeat** (Server): if CurrentBoat valid AND Condition ≠ Restrained → CurrentBoat.Unseat(Self).

### Boat as an interact target (add BPI_Interactable)
- **CanInteract:** Interactor not in a boat AND Normal → carrying ? FreePassengerSeat ≠ −1 : (driver seat free OR a passenger seat free)
- **Duration:** 0 · **Prompt:** carrying "Load prisoner" / driver free "Drive" / else "Board"
- **Interact:** carrying → LoadPrisoner(Interactor) / else Board(Interactor)

### Ramming and capsizing
- **Event Hit** (Hull) → Switch Has Authority → Other is BP_PVVCharacter AND its CurrentBoat ≠ Self AND Hull velocity length ≥ RamKnockdownSpeed →
  Other.**Knockdown**(GetDriver or Self, Velocity normalized × 900 + (0,0,400)).
- Other is BP_Boat AND closing speed ≥ 900 AND I'm faster → Other → **Unseat** a random occupant → Launch them up.
- Tick (Authority): if Hull Up Vector Z < 0.3 for 0.5 s → Unseat everyone; after 3 s → **Set Actor Location and Rotation** (+100 Z, yaw only, Teleport ✓) and zero the velocities.

✅ **Test:** drive with W/S/A/D, jump out with E, ride as a passenger and punch from the boat, run people over, and load a tied Pirate into a seat.

---

## Step 7 — Detention facility

### BP_DetentionZone (parent: Actor, Replicates ✓)
Components: **DepositArea** (Box, root, extent 300×300×150, Trigger, *Hidden in Game ✗* while testing), **DoorArea** (Box 100×150×150),
**CellPoint** and **ReleasePoint** (Scene). Build cell walls around CellPoint in the level.
Variables: `Detainees` (Array of BP_PVVCharacter, Replicated), `JailbreakTime` 5.

**BPI_Interactable:**
- **CanInteract:** Interactor Normal → carrying ? (Interactor is a Viking AND **DepositArea Is Overlapping Actor** Interactor) :
  (Interactor is a Pirate AND number detained > 0 AND **DoorArea Is Overlapping Actor** Interactor)
- **Duration:** carrying 0, else JailbreakTime · **Prompt:** "Process detainee" / "Break them out! (hold)"
- **Interact:**
  - Carrying → P = Interactor.TakeCarried → P.**Detain**(Self, CellPoint location + random 150 cm offset) → Add to Detainees → GameMode NotifyDetained(P, Interactor)
  - Else → For Each Detainee (Condition == Detained) → **ReleaseFromDetention**(ReleasePoint + random offset) → count → Clear array → NotifyJailbreak(Interactor, count)

On the character (server):
- **Detain(Zone, Loc):** if CarriedBy valid → TakeCarried · if CurrentBoat valid → Unseat · StruggleProgress = 0 → Set **Condition = Detained** → Teleport(Loc)
- **ReleaseFromDetention(Loc):** Balance = Max → Set **Condition = Normal** → Teleport(Loc)
- **ResetForRespawn(Transform):** DropCarried / TakeCarried from carrier / Unseat → clear timers → Balance = Max → Condition = Normal → Stop Movement Immediately → Teleport

✅ **Test:** full loop. Knock down → zip-tie → carry → (boat ride) → drop in the cell. Another Pirate breaks them out. Detaining every Pirate ends the match.

---

## Step 8 — The lake and test map

1. **BP_Lake** (Actor): a huge **Plane** (scale 300×200) with a blue material, no collision. Boats read its Z.
2. Place it at Z = 0. Add a **Physics Volume** under the surface with **Water Volume ✓** so people swim.
   Or make a waist-deep invisible floor at Z −70 that only blocks **Pawn**, so people wade.
3. Two shores (scaled cubes, top at about +60) with sloped cube beaches, docks, and walls around the edge.
4. Pirate shore: **The Learning Center** (4 walls, open front) + Pirate starts. Viking shore: **processing facility** + BP_DetentionZone + cell walls + Viking starts.
5. 3 boats per side.

---

## Step 9 — HUD and faction select

### WBP_HUD
Create it in **BP_PVVPlayerController BeginPlay** → Is Local Player Controller → Create Widget → Add to Viewport.
Use **bindings** (or a Tick that sets text):
- Top: `"PIRATES {PirateScore} | {mm}:{ss} | {VikingScore} VIKINGS"` from Get Game State → Cast BP_PVVGameState.
- Kill feed: on Construct → GameState **Bind Event to OnAnnouncement** → add a Text to a Vertical Box → Remove after 8 s (Delay).
- Prompt: Get Owning Player Pawn → Cast BP_PVVCharacter → `"[E] " + FocusPrompt` (hidden if empty).
- Hold bar: Progress Bar = HoldProgress (visible when > 0).
- Balance bar = Balance / MaxBalance.
- Status: Restrained → "ZIP-TIED! MASH SPACE" + StruggleProgress bar. KnockedDown / Detained messages.
- Pawn is a BP_Boat → "DRIVING — W/S A/D, E to jump out".
- Bind OnMatchEnded → big "PIRATES WIN!" / "VIKINGS WIN!".

### WBP_FactionSelect
Two buttons → Get Owning Player → Cast BP_PVVPlayerController → **SR_RequestFaction(Pirates / Vikings)** → Remove from Parent →
Show Mouse Cursor ✗ → Set Input Mode Game Only. Turn off **AutoAssignFactions** on the GameMode to use it.

---

## Step 10 — Character art and customization

Follow `Art/Characters/README.md`:
- Import the body, then hair, hats and outfits with **the same skeleton**.
- Customization: Skeletal Mesh components with **Set Leader Pose Component**, a **Data Table** of cosmetics locked by faction,
  colour slots via **Dynamic Material Instances**, and sliders via **Set Morph Target**.
- Make one `S_Look` struct on the PlayerState (RepNotify) so everyone sees each player's outfit.
- Physics body: **Physical Animation** component (the README has the exact nodes).

---

## Common multiplayer mistakes (check these first when something "only works for the host")
1. You changed a variable on the client. It must happen inside a **Run on Server** event.
2. You forgot **Replicates ✓** on an actor, or **Replicate Movement ✓** on the boat.
3. You used **Get Player Character 0** / **Get Player Controller 0** in gameplay code. Use **Self**, the **Owner**, or **Get Controller** instead.
   Index 0 is only OK in widgets.
4. You set a RepNotify variable with the plain "Set" node. Use the one that says **w/ Notify**.
5. You called a GameMode function on a client. The GameMode only exists on the server.
