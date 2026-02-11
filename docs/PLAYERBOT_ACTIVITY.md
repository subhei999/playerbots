# Playerbot Activity System

Documentation of how playerbot activity/optimization works when running 2000+ bots.

## Overview

With many bots online, the server uses **multi-layer throttling** so only a subset of bots actively "move" and do AI work each tick. Bots near real players get priority; bots in empty zones are throttled.

---

## 1. Map-Level Update Gating (`Map.cpp`)

**Location:** `mangos/src/game/Maps/Map.cpp` ~line 818-876

Each map tick, the server decides **which bots get a full AI update** vs a minimal/skipped update.

### Active Zones (every 10 seconds)

- **Active zone** = zone where at least one **real player** (non-bot, non-AFK, GM-visible) is present
- The map maintains `m_activeZones` (list of zone IDs)
- Recomputed every 10 seconds

### Per-Bot Update Decision

For each bot on the map:

| Condition | `shouldUpdateBot` |
|----------|-------------------|
| Real player | Always true |
| Bot in group with real player | Always true |
| Bot in BG/combat | Always true |
| Bot in **active zone** (same zone as a real player) | true |
| Bot in **inactive zone** (no real player there) | **false** |
| Instances | Always true |

When `shouldUpdateBot` is false, the bot gets `UpdateAI(diff, minimal=true)` — a **minimal** update (reduced movement, delayed reactions).

**Note:** `Player::UpdateAI` currently receives `minimal` but does not pass it to `PlayerbotAI::UpdateAI`; the AI layer still uses its own `AllowActivity()` checks.

---

## 2. Activity Priorities (`PlayerbotAI.cpp`)

**Location:** `playerbot/PlayerbotAI.cpp` ~line 6077-6290

`AllowActivity()` and `AllowActive()` gate what bots can do:

### Priority Types (highest to lowest)

| Priority | When | Always Active? |
|----------|------|----------------|
| `HAS_REAL_PLAYER_MASTER` | Bot has a real player master, or `disableActivityPriorities` | Yes |
| `IN_GROUP_WITH_REAL_PLAYER` | In group with real player | Yes |
| `VISIBLE_FOR_PLAYER` | Real player nearby (visibility range) | Yes |
| `IN_BATTLEGROUND` | In BG or being teleported there | Yes |
| `IN_INSTANCE` | In dungeon/raid | Yes (0–5% threshold) |
| `IN_COMBAT` | In combat | Yes (0–10%) |
| `NEARBY_PLAYER` | Real player within visibility + react distance | Yes (0–40%) |
| `IN_ACTIVE_AREA` | Same continent, in active zone | Depends on activity % (50–100%) |
| `IN_ACTIVE_MAP` | Same map as real players, but different zone | Depends (70–100%) |
| `IN_INACTIVE_MAP` | Map with no real players | Depends (80–100%) |

### `DETAILED_MOVE_ACTIVITY` (movement)

**Fully active** only for: master, group with real player, instance, visible player, combat, nearby player.

Bots in `IN_ACTIVE_AREA` (same zone as players but >250 yd away) do **not** get detailed movement — they stay in a lighter “delay” mode.

### Distance Tiers (yards)

| Tier | Distance from you | Priority | Movement | Updates |
|------|-------------------|----------|----------|---------|
| **VISIBLE** | ≤ 150 yd | `VISIBLE_FOR_PLAYER` | Full | Full |
| **NEARBY** | 150–250 yd | `NEARBY_PLAYER` | Full | Full |
| **Same zone** | > 250 yd | `IN_ACTIVE_AREA` | **None** (teleport for long moves) | Scaled (50–100%) |

- **150 yd** = `AiPlayerbot.ReactDistance` (default)
- **250 yd** = `Visibility.Distance.Continents` (100) + ReactDistance (150)

### `REACT_ACTIVITY` (reactions)

**Fully active** only for: master, real player, group with real player, instance, always-active bots.

---

## 3. Diff Metrics (Avg Diff, Max Diff)

**Location:** `mangos/src/game/World/World.cpp` ~line 1573-1604

**Diff** = time in **milliseconds** for one world update tick. Lower = faster server.

| Metric | Meaning |
|--------|---------|
| **Current Diff** | Last tick duration (ms) |
| **Avg Diff** | Rolling average over last ~600 ticks (~10 min) |
| **Max Diff** | Highest single-tick diff in that window |

Example: `Avg Diff: 100` ≈ 100 ms per tick → ~10 updates/sec. Higher values = more lag.

### Log Output

Every 60 ticks the server logs:

```
Avg Diff: 100. Sessions online: 2005.
Max Diff: 250.
```

---

## 4. Diff-Based Activity Tuning (PID Controller)

**Location:** `playerbot/RandomPlayerbotMgr.cpp` ~line 731-809

Activity percentage is adjusted by a **PID controller**:

- **Setpoint (wanted diff):**
  - `diffWithPlayer` (default 100) when real players are online
  - `diffEmpty` (default 200) when no real players
- **Process variable:** `sWorld.GetAverageDiff()` (actual avg diff)

### Config

```conf
# aiplayerbot.conf
AiPlayerbot.DiffWithPlayer = 100   # Target avg diff (ms) with players online
AiPlayerbot.DiffEmpty = 200       # Target avg diff (ms) with no players
```

### How it works

- If **avg diff > target** → server is slow → PID **reduces** activity % → fewer bots active
- If **avg diff < target** → server is fast → PID **increases** activity % → more bots active

Output is clamped 0–100% and used as `activityMod` for `AllowActive()` scaling.

---

## 5. Log Messages

### Map (every 10 seconds, when active zones exist)

```
Map 0: Active Zones - 3
Map 0: Active Zone Players - 45 of 2000
```

- **Active Zones** = number of zones with at least one real player
- **Active Zone Players** = bots that received a full update this tick / total players on map

### World (every 60 ticks)

```
Avg Diff: 100. Sessions online: 2005.
Max Diff: 250.
```

---

## 6. Why Bots Far From Players Don’t Move

1. **Map layer:** Bots in zones without real players get `shouldUpdateBot = false` → minimal updates.
2. **Activity layer:** `DETAILED_MOVE_ACTIVITY` is only allowed for bots with master, in group with real player, in instance, visible, in combat, or nearby.
3. **Activity %:** Bots in `IN_ACTIVE_AREA` / `IN_ACTIVE_MAP` are scaled by `activityMod`; if it’s low, many are inactive.

So only bots near real players (or in special cases like BG/instance) get full movement and AI.

---

## 7. Bots at Grind Spots (Far From Players)

### Inactive zone (no real player there)

**No.** Bots are frozen: minimal updates, no movement, no combat. Zone must have a real player for any activity.

### Active zone (real player somewhere in zone, but >250 yd away)

**Limited.** Bots are `IN_ACTIVE_AREA`:

| Capability | Behavior |
|------------|----------|
| **Movement** | No normal walking. Long distances: teleport with delays. Short distances: can walk. |
| **Combat** | `GRIND_ACTIVITY` allowed for a scaled % of bots (50–100%). Rotating subset can pick targets and attack. |
| **Net effect** | Only a fraction grind at any time; travel to spot is slow (teleport + delays). |

### To make distant grind bots reliable

- `AiPlayerbot.DisableActivityPriorities = 1` — all bots fully active
- `AiPlayerbot.DisableBotOptimizations = 1` — no map-level throttling

Requires accepting higher server load and lag.

---

## 8. Lag Impact: 2000 Fully Active Bots

With `DisableActivityPriorities` and `DisableBotOptimizations`, all 2000 bots get full AI every tick.

### Load increase

| Component | With optimizations | All active |
|-----------|--------------------|------------|
| AI updates/tick | ~50–200 bots | 2000 bots |
| Grid loading | Active zones only | Every bot loads cells |
| Creature updates | Near active bots | Near all bots |

### Estimated impact

- **Target diff:** ~100 ms (~10 updates/sec)
- **All active:** diff often 500–2000 ms+ (0.5–2 updates/sec)
- **Result:** Noticeable stutter, possible freezes on weak hardware

### Safer alternatives

1. **Raise diff targets:** `AiPlayerbot.DiffWithPlayer = 200` or `300`
2. **Lower bot count:** e.g. 500–800 fully active
3. **Test incrementally:** Enable full activity and watch `Avg Diff` / `Max Diff` in logs

---

## 9. Making More Bots Active

### Config Options

| Option | Effect |
|-------|--------|
| `AiPlayerbot.DisableActivityPriorities = 1` | All bots always active (may cause lag) |
| `AiPlayerbot.DisableBotOptimizations = 1` | No map-level throttling; all bots get full updates |
| `AiPlayerbot.DiffWithPlayer` | Higher value = allow more lag before reducing activity |
| `AiPlayerbot.DiffEmpty` | Same, for empty server |

### Trade-offs

- More active bots → higher load → higher avg diff
- PID will lower activity again if diff exceeds target
- To keep more bots active, either reduce bot count or raise diff targets (accept more lag)

### Making same-zone bots move (without full disable)

Increase `AiPlayerbot.ReactDistance` (e.g. 300) so more bots fall into `NEARBY_PLAYER` and get `DETAILED_MOVE_ACTIVITY`.

---

## 10. Quick Reference

| Term | Meaning |
|------|---------|
| **Active zone** | Zone with at least one real player |
| **Active Zone Players** | Bots that got a full update this tick |
| **Avg Diff** | Rolling average world tick time (ms) |
| **Max Diff** | Max single-tick time in the window |
| **diffWithPlayer** | Target avg diff when players are online |
| **diffEmpty** | Target avg diff when no players |
| **activityMod** | 0–100% how many bots are allowed to be active |
| **ReactDistance** | VISIBLE range (default 150 yd); increase to make more bots "nearby" |
| **Visibility.Distance.Continents** | mangosd.conf; used with ReactDistance for NEARBY (250 yd = 100 + 150) |
