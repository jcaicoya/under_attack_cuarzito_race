# CLAUDE.md — Cuarzito Pre-Show Game

Context for AI-assisted development on this project.

## What this is

A pseudo-3D arcade minigame built in **C++23 / Qt 6.7** (MSVC, Windows). It runs as a pre-show experience at Cuarzito cyber-theatre events — shown on a large screen, played with a wireless gamepad or keyboard. Sessions are short (20–60 s), audience is mixed (kids + adults), stability and simplicity matter more than features.

## Visual reference

Two reference images live in the project root:

- **`cave.png`** — the Cuarzo Polar website hero scene. A dark low-poly cave with the opening revealing deep space: stars, Polaris (one brighter star near center), and a soft aurora/nebula. The cave walls are angular, almost black, with subtle face-lighting. The floor has a glowing cyan light at center. Floating gems of 4 types: grey/silver diamond, purple/magenta diamond, green cube-crystal, orange cube-crystal. Orange sparks pepper the scene. This is the direct visual reference for the game environment.

- **`cuarzito.png`** — the player character. A small **dark hooded figure** (ghost/specter silhouette), flowing black cloak, no legs visible, floating. The only bright element is a **horizontal neon green visor/eye slit** (think Daft Punk, but green). A blue electric aura surrounds the body. Reads instantly at any size: dark mass + one green line.

## Platform

- OS: Windows 11
- Compiler: MSVC 2022 (64-bit)
- Qt: 6.7.3 at `C:/Qt/6.7.3/msvc2022_64`
- Build system: CMake 4.2+
- IDE: CLion

## Architecture

All source files live under `src/`. No subdirectories inside `src/` yet.

### Files

| File | Role |
|---|---|
| `main.cpp` | Entry point. Installs SIGINT/SIGTERM handlers so IDE stop gives exit 0. |
| `MainWindow` | `QMainWindow`, 1280×720, holds `GameView` as central widget. |
| `GameView` | `QGraphicsView` subclass. Forwards key events to `InputManager`. Calls `fitInView` on resize so the game scales to any window size. `CacheMode = CacheNone` so `drawBackground` fires every frame. |
| `GameScene` | **Everything lives here.** `QGraphicsScene` subclass. Owns the game loop (`QTimer` ~60 fps, delta-time), all entity data, state machine, and all rendering via `drawBackground`. Two `QGraphicsTextItem`s for HUD and overlay sit as normal scene items on top. |
| `InputManager` | Tracks held keys and "just pressed" confirm. `endFrame()` must be called each tick to clear the just-pressed state. |

### Rendering approach

All game visuals are painted inside `GameScene::drawBackground`. No `QGraphicsItem` subclasses for game entities — they are plain structs updated and drawn every frame. The two `QGraphicsTextItem` members (`m_hudText`, `m_overlayText`) are ordinary scene items and render on top of `drawBackground` automatically.

Draw order inside `drawBackground`:
1. Background fill (near-black)
2. `drawEnvironment` — stars, Polaris, aurora, low-poly cave rock edges, perspective grid lines, depth rings, radial vignette
3. `drawSparks` — orange streak particles
4. `drawObstacles` — grey-blue diamond gem shapes, back-to-front sorted
5. `drawPlayer` — dark hooded Cuarzito figure with neon green visor (skipped in Attract state)

### Pseudo-3D engine

All entities live in world space `(wx, wy, wz)`. Projection:

```cpp
screenX = CX + wx * FOCAL / wz   // CX = 640, FOCAL = 400
screenY = CY + wy * FOCAL / wz   // CY = 360
scale   = FOCAL / wz
```

Key Z constants (all in `GameScene.h`):
- `SPAWN_Z = 900` — obstacles/sparks born here (tiny, near vanishing point)
- `REMOVE_Z = 25` — past the camera, entity removed or recycled
- `COLLIDE_Z = 460` — collision check only starts below this Z (gives ~1.5 s reaction time)

### Entity structs (all private in `GameScene`)

```cpp
struct Player   { float sx, sy; };                           // screen-space position
struct Obstacle { float wx, wy, wz, wHalfW, wHalfH; };      // world-space
struct Spark    { float wx, wy, wz, speed; };                // world-space
struct Star     { float x, y, r, bright; };                  // screen-space, static
```

Stars are static (no scrolling) — they represent the distant sky visible through the cave opening. Concentrated in the center of the screen, thinning toward the dark edges.

### State machine

```cpp
enum class GameState { Attract, Playing, GameOver };
// Future: Countdown, HighScoreEntry
```

### Input

`InputManager` maps physical keys to logical actions. Current mappings:

| Action | Keys |
|---|---|
| Move left | `Key_Left`, `Key_A` |
| Move right | `Key_Right`, `Key_D` |
| Move up | `Key_Up`, `Key_W` |
| Move down | `Key_Down`, `Key_S` |
| Confirm | `Key_Space`, `Key_Return`, `Key_Enter` |
| Quit | `Key_Escape` (handled in `GameView`) |

Gamepad support is planned for Phase 4 — the abstract action layer is already in place.

## Visual identity

Derived from `cave.png` and `cuarzito.png`.

### Palette

| Element | Colour | Notes |
|---|---|---|
| Background | `#030509` (near-black navy) | Deep space / cave interior |
| Cave rock edges | `#07040e` – `#120b1c` | Very dark purple-grey polygons at screen corners |
| Stars | `#dce6ff` (low alpha) | Cool white, concentrated around screen center |
| Aurora | blue-teal-purple soft glow | Behind stars, upper-center area |
| Sparks | `#ff9114` orange-amber | Streak particles rushing toward camera |
| Obstacles | `#c8d2e1` → `#232a38` gradient, `#50c8dc` outline | Grey-blue diamond gem shapes |
| Player body | `#08080f` → `#1a1628` | Dark hooded cloak, near-black |
| Player visor | `#64ff82` bright + glow | Neon green horizontal eye slit |
| Player aura | `#0050c8` low alpha | Blue electric glow around body |
| Tunnel rings | `#00aa6e` low alpha | Dark teal depth cue rectangles |
| Grid lines | `#0f3728` very low alpha | Convergence lines to vanishing point |

### Player (Cuarzito) — `drawPlayer`

Dark hooded spectre. Render order:
1. Blue radial aura (larger than body, `#0050c8`, ~40 alpha)
2. Dark cloak body drawn with cubic bezier curves — narrow hood tip at top, widens at shoulders, tapers at bottom
3. Neon green visor: two layers — soft wide glow rect, then crisp bright bar (`#64ff82`)

### Obstacles — `drawObstacles`

Diamond (4-point) gem shapes. Projected from world space. Render:
1. Diamond path: top vertex, right, bottom (shorter than top), left
2. Linear gradient fill: light grey-blue at top-left → dark at bottom-right (illusion of 3D facet)
3. Top highlight triangle (brighter inner facet)
4. Cyan-teal outline glow

### Environment — `drawEnvironment`

1. Stars: ~80 static dots, normally distributed around center (the sky through the cave)
2. Polaris: one slightly larger, brighter star near center-upper area, with a small cross-glow
3. Aurora: soft radial gradient in blue-teal tones, elliptical, upper-center
4. Cave rock edges: 4 dark low-poly polygon chunks at screen corners (suggesting cave walls)
5. Perspective convergence lines from vanishing point to all screen edges
6. Rectangular depth rings at 5 Z-levels
7. Radial vignette (dark at edges, transparent at center)

## Design rules to preserve

- **One hit = game over.** No shields, no lives.
- **Collision is forgiving by design.** `PLAYER_HITBOX = 15px` (smaller than the visual). Keep it that way.
- **Never generate impossible patterns.** Always ensure at least one safe corridor exists.
- **Collision gate (`COLLIDE_Z`).** Keep it — players must see obstacles before they can be hit.
- **No QGraphicsItem for game entities.** Plain structs + `drawBackground` is the correct pattern.
- **`CacheMode = CacheNone` on `GameView`.** Required so `drawBackground` redraws every frame.
- **Palette contrast rule.** Player (dark + green) must remain clearly distinct from obstacles (grey-blue gem) and future collectibles (green crystal, orange crystal). Never make two entity types the same hue.

## What comes next (in order)

1. ~~**Phase 2**~~ ✅ — Collectibles, scoring, HUD, popups — done.
2. **Phase 3** — Countdown state (`3 2 1`), attract idle animation, `HighScoreManager` (JSON top-10, `highscores.json`), initials entry screen
3. **Phase 4** — Gamepad support via Qt 6 gamepad API; dead-zone tuning
4. **Phase 5** — Sprites/animations for Cuarzito, impact flash on collision, sound effects, ambient loop

_(old Phase 2 notes kept below for reference)_

1. **Phase 2** — Collectibles: `Collectible` struct in `GameScene`, two types matching cave.png gems:
   - Normal quartz: **green** faceted cube-crystal, `+10` pts
   - Special quartz: **orange** faceted cube-crystal, `+25` pts
   - Survival score (pts/s), HUD score display
2. **Phase 3** — Countdown state (`3 2 1`), attract idle animation, `HighScoreManager` (JSON top-10, `highscores.json`), initials entry screen
3. **Phase 4** — Gamepad support via Qt 6 gamepad API; dead-zone tuning
4. **Phase 5** — Sprites/animations for Cuarzito, impact flash on collision, sound effects, ambient loop

## Things to watch out for

- `QList::removeIf` requires Qt 6 — do not backport to Qt 5 patterns.
- `QRandomGenerator::global()` is thread-safe and used from main thread only — fine.
- `drawBackground` always repaints the full scene rect (not just the dirty `rect` param). Intentional.
- Obstacle `removeIf` happens **before** the collision loop — order matters, do not swap.
- `m_gameOverTimer` (1.5 s delay) prevents accidental instant restarts. Keep it.
- Stars are **static** (initialized once, not updated per frame). Do not add per-frame movement to them.
