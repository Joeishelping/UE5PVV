# Border Run vs Gang Beasts: combat & physics checklist

How each core Gang Beasts mechanic maps to Border Run right now. I checked this against the code; it hasn't been playtested yet.

**Status:** ✅ in and working the Gang Beasts way · 🟡 in, but simpler or different · ❌ missing

## Bodies & movement
| Gang Beasts | Border Run | Status |
|---|---|---|
| Active-ragdoll bodies that wobble, lean and react to every push | Physical-animation muscles, a balance lean spring, wobblier when hurt or drunk | ✅ |
| Heavy, slidey, momentum-y walking | Heavy acceleration and braking, lean into the move, step bob | ✅ |
| Running into people knocks them about | Body checks (both stagger; already-rocked people get bowled over) | ✅ |
| Arms dangle and swing loosely | Free arms follow the animation; reaching arms are physics | 🟡 |
| Crouch / duck under punches | Not in (no crouch key) | ❌ |

## Hands: punch & grab
| Gang Beasts | Border Run | Status |
|---|---|---|
| Tap = punch with that hand, hold = grab with that hand | Tap = punch (thrown on release), hold = arm stays out | ✅ |
| Hands grab whatever they touch (people, walls, ledges, props) and hold until you let go | Sticky hands: people (the body part you touched), walls, props; held until you release the button | ✅ |
| Mashing still punches at a steady rhythm | Per-hand cooldown (0.42 s) and **clicks are queued, never dropped** | ✅ (fixed now) |
| Punches go in the general facing direction and find the target | General aim/facing direction, locks onto anyone within ~45°: the head, or the body if their guard is up | ✅ |
| Damage comes from the physical hit; no damage if it doesn't touch | Contact only: speed into the target × location (head / body / arm) | ✅ |
| Hard head shots knock out; light body taps don't | KO meter: head shots fill it fast, body barely; a very hard clean head shot = instant knockdown | ✅ |
| Getting hit interrupts your swing | Hit-stun: a clean hit cancels their punch and delays their next one | ✅ (new) |

## Grappling & throwing
| Gang Beasts | Border Run | Status |
|---|---|---|
| Grab and drag people around | Grab and drag; tug of war when both of you hold each other | ✅ |
| Lift people over your head | Both hands on them + Space = lift; Space again = body slam | ✅ |
| Throws use your momentum (spin and let go) | **Spin throw:** let go while spinning or charging and they keep the momentum | ✅ (new) |
| Throw with a button | Z = throw (overhead throws go further) | ✅ |
| Grab a leg to trip | Leg pull | ✅ |
| Choke from behind | Chokehold (both hands from behind) | ✅ |
| Wriggle out of grabs | Mash Space (no instant re-grab for 1.2 s afterwards) | ✅ |

## Defense
| Gang Beasts | Border Run | Status |
|---|---|---|
| Arms up to cover your head | Physical guard: forearms up in front of your face; punches that land on them are caught; body shots and hits from behind get through; guard break | ✅ |
| Dodging comes from moving your wobbly body | Movement, dives, and NPC slips | 🟡 |

## Knockouts
| Gang Beasts | Border Run | Status |
|---|---|---|
| Out cold: fully limp, can be dragged or thrown off the map | Knocked down = full ragdoll; drag / carry / lift / throw | ✅ |
| Wake up after a while (more hits keep you out) | Mash Space to wake (minimum time down); being hauled around keeps you groggy | ✅ |
| Dizzy birds before going out | Dazed / birds at a high KO meter; stumbling when rocked | ✅ |

## Climbing & ledges
| Gang Beasts | Border Run | Status |
|---|---|---|
| Grab a wall or ledge with your hands and hang | Sticky hands hold you like ropes (two hands hang, one hand slips) | ✅ |
| Lift your arms to pull up, jump to get over | Hold W or look up to pull; regrab higher, hand over hand; Space = a real jump | ✅ |
| Grip runs out | Grip meter drains (faster on one hand) | ✅ |
| Grab a ledge as you fall off | Works if your hands are out when you go past a wall | 🟡 |

## Kicks & specials
| Gang Beasts | Border Run | Status |
|---|---|---|
| Kick | Physics kick aimed at the nearest leg; only counts on foot contact (trip / dead leg / body shove) | ✅ |
| Headbutt | Headbutt (breaks grips, big KO) | ✅ |
| Drop kick (run + jump + kick) | Drop kick (kick in the air, or Ctrl) | ✅ |
| Dive | Dive tackle / dive punch | ✅ |

## Levels & hazards
| Gang Beasts | Border Run | Status |
|---|---|---|
| Every level is about throwing people off edges or into hazards | River (drowning), the Wall, bridge, trucks (run over), spike fences, cannon | 🟡 (low bridge railings, truck-hanging and cactus are still on the list) |

## NPC AI
| Gang Beasts | Border Run | Status |
|---|---|---|
| Bots use the same moves as players | NPCs use the same punches, grabs, kicks, headbutts, lifts, throws and haymaker arm sweeps | ✅ |
| Bots space out, attack, back off and gang up | Step-in / step-out rhythm, circling, guard reactions, counter slips; personalities: brawler, grappler (drags you to the river), sneak, coward | ✅ |
| Bots grab ledges / protect themselves near edges | Back off when rattled; no edge awareness yet | 🟡 |

## Balance vs NPCs
- NPC hits on players count **55%** (damage and KO meter), and an NPC needs a near-perfect head shot to drop you in one punch.
- Player hits on NPCs count **145%**, and a slightly less perfect head shot drops an NPC in one.
- Player vs player and NPC vs NPC are unchanged.
- Tune it in BP_Runner / BP_Warden and your NPC Blueprint under **BR | Brawl**: **NPC Hits On Players Scale** and **Player Hits On NPCs Scale**.

## Next on the list (in order)
1. **Crouch / duck** (a key, and NPCs ducking punches).
2. **Edge-aware NPCs** that grab railings and walls when they're shoved near a drop.
3. **Map dangers:** low bridge railings to tip people over, hanging off moving trucks, cactus.
4. **Hit sounds** whose volume and pitch scale with how hard the impact was.
