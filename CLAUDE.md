# CLAUDE.md - Cuarzito Pre-Show Game

Context and working instructions for AI-assisted development on this project.

## What This Is

This is a short arcade pre-show game for the Cuarzito cyber-theatre experience.

The player controls **Cuarzito**, a small dark hooded figure with a neon green visor, flying through a cosmic mineral cave. The design is pivoting from the original obstacle-dodge prototype into a chase game: Cuarzito pursues four flying gems through a winding tunnel before time runs out.

Primary requirements:

- Simple enough to understand instantly.
- Beautiful enough for a large screen or TV.
- Robust enough for live event use.
- Launch fullscreen by default for event use.
- Short, replayable runs of about 20 to 60 seconds.
- Keyboard fallback always available.
- Basic XInput gamepad support available on Windows.
- SDL3 controller backend for DualSense. Loads `SDL3.dll` dynamically at runtime from the exe directory (deployed via CMake from `libs/SDL3.dll`). DualSense confirmed working.

## Current Code Reality

The current codebase is a playable Qt prototype with an OpenGL-ready widget shell.

Current architecture:

```text
MainWindow
  -> GameWidget : QOpenGLWidget
       -> GameScene : QObject
       -> InputManager
```

Implemented now:

- `QOpenGLWidget` main game widget.
- `QTimer` game loop in `GameWidget`.
- `QPainter` drawing on the OpenGL widget.
- Attract, Intro, Countdown, Playing, GameOver, and HighScoreEntry states.
- Keyboard, XInput, and optional SDL3 controller input through `InputManager`.
- Pseudo-3D projection using a moving vanishing point.
- Four-direction movement inside tunnel bounds.
- Four persistent chase gems.
- Chase redesign in progress around world `z`, tunnel path samples, acceleration/braking, and timer-based target captures.
- Scoring, HUD, score popups, burst effects, stars.
- Procedural placeholder Cuarzito.
- Dedicated `CaveRenderer` with a QPainter-based streaming faceted tunnel.
- Track data loaded from `resources/tracks/first_tunnel.json` through Qt resources.
- R restarts the current run immediately for testing.
- Fullscreen now uses fit scaling so the full 1280x720 authored frame remains visible on ultrawide displays.
- Cuarzito's persistent aura is currently disabled for tunnel visibility.
- Third-person / first-person view toggle with `V`. Third-person is default. First-person to be iterated later.
- `I` toggles invulnerability. Crash visuals still play; score/speed penalties are skipped while invulnerable.
- CLI test mode exists: `cuarzito-race.exe --test downhill|uphill|vertical`. It skips intro/countdown, loads a dedicated resource track, auto-starts in `Playing`, forces invulnerability, and exits when the test track ends.
- CLI test mode writes trace lines to `stderr` so automated runs can be inspected from the console.
- Speed-scaled crash feedback: impact flash, expanding shockwave, and camera shake all scale with player speed at time of wall contact.
- Turn wall cue: outer wall of curves receives a subtle amber tint in `CaveRenderer` proportional to VP displacement.
- Mini-map has been iterated into a cube-like 3D debug view, but it still needs readability tuning.
- Intro flow goes directly to `Playing` (no countdown after intro — intentional design decision).
- Wall proximity edge glow: each screen edge brightens amber as Cuarzito approaches that wall. Starts at 38% proximity, flashes near-white at contact. Drawn by `CaveRenderer::drawWallProximity()`.
- Wall contact sparks: on first contact, 12 amber rock-chip particles burst inward from the correct screen edge (`GameScene::spawnWallSparks()`).
- Floor/ceiling proximity: floor glow scales with both speed and player Y offset. Dark vignette creeps from the top of the screen when near the ceiling.
- Cuarzito projected at `PLAYER_DRAW_DEPTH = 200` world units using the same perspective formula as the tunnel rings (`cx = vpX + offX × FOCAL/200`, `cy = vpY + 30 + offY × FOCAL/200`). At the physics boundary, the character appears within 14–16px of the visible tunnel ring — wall contact is geometrically convincing. Impact shockwave, gem bursts, and score popups all use the same projected position.
- A persistent projected safe-zone rectangle is now drawn during gameplay. It is based on the same `safeX` / `safeY` factors used by collision and is intended for debugging the vertical movement contract.

Important files:

| File | Current role |
|---|---|
| `src/main.cpp` | QApplication entry point and signal handling. |
| `src/MainWindow.*` | Creates the main window and installs `GameWidget`. |
| `src/GameWidget.*` | `QOpenGLWidget`, forwards keyboard events, owns timer, cave renderer, and aspect fit. |
| `src/CaveRenderer.*` | Draws dark faceted cave, space, stars, Polaris, aurora, floor glow, wall proximity glow. |
| `src/GameScene.*` | Game state, entities, projection, updates, drawing. |
| `src/TunnelPath.*` | Provides deterministic tunnel center/radius samples by world `z` for the chase redesign. |
| `src/AudioManager.*` | Generates cue tones and a subtle ambient loop, then plays them through `QSoundEffect`. |
| `src/InputManager.*` | Maps keyboard, XInput, and optional SDL2 controller input to abstract actions. |
| `CMakeLists.txt` | Qt Widgets plus OpenGL/OpenGLWidgets build. |

The tunnel traversal is working and the walls, floor, and ceiling stream past the camera. The current priority is improving readability and game feel: the route ahead, turns, acceleration/braking, crashes, vertical motion, and gem pacing still need iteration.

### Current Vertical-Motion Problem

This is the exact resume point.

- Goal:
  - uphill/downhill should behave like left/right turns
  - `uphill`: the floor should feel like it rises toward the player
  - `downhill`: the ceiling should feel like it drops toward the player
  - pressing `Up` / `Down` should compensate tunnel motion and bring the player back toward the same apparent centered state as a straight section
- What was changed:
  - curve inertia has been temporarily reduced all the way to `0.0` to remove kinetic drift as a confounding factor
  - vertical VP sign and floor/ceiling shading were adjusted to better match the intended visual rule
  - the collision/safe-zone debug rectangle is now always visible during `Playing`
  - vertical safe range was widened so `Up` does not instantly hit the ceiling
  - direct input influence on `targetVpY` was increased substantially so manual compensation is easier to read
- What is still wrong:
  - on uphill sections, the safe-zone box and the actual collision state are still not fully aligned
  - pressing `Up` does not yet compensate the uphill motion as expected
  - a screenshot showed Cuarzito inside the drawn safe zone while still visually/behaviorally crashing
- Next debugging direction:
  1. keep inertia at zero until the compensation contract works
  2. verify that pressing `Up` on uphill moves both the player and the safe-zone framing toward the same apparent center
  3. if the safe-zone box says "safe" but contact still happens, inspect collision state and draw state side by side rather than adding more camera tuning

## Tunnel Renderer — How It Works

`CaveRenderer::drawCave()` uses a **streaming ring system** driven by `frame.playerZ`:

- 22 cross-section rings are placed at world-z intervals of 80 units ahead of the camera.
- Each ring's projected screen size is `TUNNEL_R * FOCAL / relZ` (155 × 400 / depth).
- A synthetic camera-plane ring is prepended so the nearest wall band extends past the viewport as rings cross the camera.
- Each ring's irregular cave-wall shape is keyed to its **absolute world-z** (`phase = worldZ * 0.015 + time * 0.03`), so the same cave geometry looks consistent as you approach it.
- Facet bands between adjacent rings are drawn far-to-near (painter's algorithm). The nearest band's outer ring extends off-screen; Qt clips it.
- In enclosed gameplay mode, a full-canvas dark rock backing layer is painted behind the facets as a defensive fallback against near-plane gaps.
- `frame.playerZ` is `m_player.z` during gameplay and a gentle 80 units/s drift in all other states.
- `m_tunnelZ` in `GameScene` carries this value across all states and is exposed as `tunnelZ()`.

Key constants in `CaveRenderer::drawCave()`:

| Constant | Value | Meaning |
|---|---|---|
| `TUNNEL_R` | 155 | World-space tunnel radius |
| `FOCAL` | 400 | Must match `GameScene::FOCAL` |
| `RING_SPACING` | 80 | World units between rings; keep ≤ 97 so rings[0] always fills the screen |
| `NUM_RINGS` | 22 | Rings ahead of camera |
| `NEAR_CLIP` | 1 | Only drops rings exactly at the camera plane |
| synthetic camera ring | screen-cover polygon | Prevents near-camera prism gaps when rings cross the camera plane |

## Visual References

Two root-level images define the style.

### `cave.png`

Environment direction:

- Dark low-poly cave.
- Large central opening into space.
- Almost black faceted rock walls.
- Blue/cyan/teal glow around the opening and floor.
- Orange sparks.
- Star field with one brighter Polaris-like point.
- Floating crystals/gems.

For gameplay, use this as inspiration rather than a literal static background. The tunnel must remain readable and collision-safe.

### `cuarzito.png`

Player direction:

- Small floating hooded figure.
- Black/dark cloak with compact silhouette.
- One neon green horizontal visor.
- Blue electric aura in reference art. Current gameplay omits the persistent aura so the tunnel and wall position stay readable.
- No visual clutter.

During normal gameplay, Cuarzito is facing away from the camera. Do not draw the full green visor in the default rear-view pose. The visor may be shown only as a small side glimpse during lateral movement, or in explicit start, pickup, and game-over turn/spin animations. At game size, the default pose should read as a dark hooded back silhouette without a persistent aura.

In third-person mode (default), Cuarzito is drawn using the tunnel perspective formula at `PLAYER_DRAW_DEPTH = 200` world units: `cx = vpX + offX × 2`, `cy = vpY + 30 + offY × 2`. This places him geometrically inside the tunnel — at the ceiling he appears near the top of the screen, at the floor near the bottom, matching the visible ring geometry within 14–16px. Size `W=92, H=112`. In first-person mode (`V` to toggle), Cuarzito is not drawn and player steering shifts the VP directly.

## Chase Game Design

Target loop:

- Cuarzito starts near `z = 0`.
- Four gems start ahead at increasing `z` distances.
- Gems move forward at constant speed.
- Cuarzito accelerates and brakes based on player input.
- The cave path bends in x/y as `z` increases, so the tunnel itself creates the 3D challenge.
- The tunnel uses discrete segments (straights, curves, inclines, declines) so the player can read what is coming and prepare. Continuous winding is avoided.
- Centripetal force is simulated: at higher speeds, curves push Cuarzito toward the outer wall. Skilled players brake into sharp corners, accelerate on straights — like a racing game.
- The tunnel should use sharp enough curves and steep enough rises/drops that the route is visible on the walls, ceiling, and floor before the player reaches it.
- Presentation target: the intro can show the cave mouth and space behind it, with four stones floating before they enter the tunnel. Gameplay should feel enclosed inside the tunnel, while still showing enough distance ahead to predict turns.
- Orange sparks are intentionally removed for now. The 3D feeling should come from moving walls, ceiling, and floor.
- Parked future idea: a true 3D labyrinth chase with branching tunnels and loops. Feasible later once the track editor is built.
- Wall, floor, and ceiling collisions reduce Cuarzito's speed and break clean-flight bonuses.
- Each captured gem adds time.
- Capturing all four gems wins.
- Running out of time loses.

Target controls:

- Steer with arrows/WASD, left stick, or D-pad.
- Accelerate with Space/Enter now, later A/R2 on gamepad.
- Brake with Shift/Ctrl, later B/L2 on gamepad.
- Restart the current run immediately with R for testing.

Core model:

```text
TunnelPath.sample(z) -> center x/y, tangent x/y, radius, curvatureH, curvatureV

Player:
  z
  speed
  local x/y offset from tunnel center (screen-space, relative to VP)
  wall contact state

Gem:
  color
  z
  constant speed (clearly slower than Cuarzito base speed — catchable)
  local x/y offset from tunnel center
  collected flag
```

## Track System

`TunnelPath` is segment-based. The first track is loaded from `:/tracks/first_tunnel.json`, embedded from `resources/tracks/first_tunnel.json` through `resources/resources.qrc`. See `docs/track-format.md` for the editable data format.

Each segment has:

| Field | Meaning |
|---|---|
| `type` | `straight`, `left`, `right`, `uphill`, or `downhill`. |
| `length` | World units along the tunnel path. At speed `200`, current straights are `600` units/3 seconds and turns are `400` units/2 seconds. |
| `angleDegrees` | Total segment bend. Required for non-straight segments. The current turn angle is `45`. |

The loader converts `angleDegrees / length` into curvature, then precomputes keyframe states at each segment boundary so `sample(z)` is O(log n). The path is deterministic and identical every run. If the Qt resource cannot be read, `TunnelPath` falls back to a small built-in straight/left/straight/right pattern so event builds remain playable.

**Editing the track:** change `resources/tracks/first_tunnel.json`. A future external visual editor should read/write the same table format. Loops are possible later by using large vertical angles over a segment.

**Curve inertia constant:** currently forced to `CURVE_INERTIA_K = 0.0f` in `GameScene.cpp` for vertical-motion debugging. Restore a non-zero value only after uphill/downhill compensation is behaving correctly. Historically it was `2.20f`, then `1.10f`.

**VP look-ahead:** `360` world units. Horizontal multiplier is `1.05x`. Vertical behavior is currently under active debugging and should not be treated as final documentation yet. Smoothing factor `4.4/s`.

**Gem balance:** gems run at progressive speeds (178 / 182 / 186 / 190 units/s), starting at z = 380 / 680 / 1040 / 1460. Player base speed is 235, max speed capped at 440. Catch-up at base speed is 45–57 units/s — requires deliberate acceleration. Each collected gem adds 20 s to the chase timer.

## Design Rules

Keep these unless the user explicitly changes the direction:

- Collision should be forgiving.
- Wall/floor/ceiling contact should slow the player, not instantly end the run.
- The player hitbox should be smaller than the visual.
- Tunnel curves must be survivable by a skilled player.
- Player, walls, and target gems must remain visually distinct.
- Avoid complex menus.
- Avoid shooting, enemies, power-ups, or long-form mechanics for now.
- Prefer robustness and readability over technical showiness.

## Recommended Target Architecture

Next major refactor:

```text
MainWindow
  -> GameWidget : QOpenGLWidget
       -> CaveRenderer
       -> GameScene
       -> InputManager
```

Target render order:

```text
GameWidget::paintGL()
       -> CaveRenderer::render(...)
  -> QPainter begin on GameWidget
       -> GameScene::drawChaseGems(...)
       -> GameScene::drawPlayer(...)
       -> GameScene::drawHUD(...)
```

Notes:

- `GameScene` should become pure game logic plus draw helpers. It should not subclass `QGraphicsScene`.
- HUD should be direct `QPainter` text, not `QGraphicsTextItem`.
- `CaveRenderer` should know nothing about scoring, collisions, or game state beyond simple uniforms such as time, vanishing point, speed, and survival time.
- Qt 6.7.3 is acceptable. Another Qt version can be used if it materially improves the result.

## OpenGL / Shader Direction

Preferred cave renderer:

- Fullscreen quad in `QOpenGLWidget`.
- GLSL fragment shader for cave walls, space, stars, aurora, depth fog, and faceted/noisy cave edge.
- Uniforms:

| Uniform | Meaning |
|---|---|
| `uResolution` | Widget size in pixels. |
| `uVP` | Vanishing point in normalized coordinates. |
| `uTime` | Elapsed seconds. |
| `uSurvivalTime` | Seconds survived. |
| `uWorldSpeed` | Current tunnel speed. |

If the shader path becomes too expensive, a strong `QPainter`/OpenGL-backed procedural cave is acceptable. The visual goal matters more than the specific rendering technique.

## Pseudo-3D Projection

The current projection model is useful and should be preserved initially:

```cpp
screenX = m_vpX + wx * FOCAL / wz;
screenY = m_vpY + wy * FOCAL / wz;
scale   = FOCAL / wz;
```

Current important constants for projection:

```cpp
FOCAL     = 400;
```

The moving vanishing point is a core part of the game feel. Keep the speed cap so the tunnel curve cannot outrun the player.

## Action List

### Phase 0 - Chase Redesign

- [x] Confirm the chase-game direction.
- [x] Document the world model around `z`, speed, tunnel path, and four gems.
- [x] Add `TunnelPath.h/.cpp`.
- [x] Sharpen the first tunnel path model and add a turn-occlusion signal for hiding the far opening.
- [x] Add first player physics pass with `z`, speed, acceleration, brake, and local tunnel offset.
- [x] Remove old survival movement assumptions from remaining obstacle/collectible code.
- [x] Replace random collectibles with four persistent gem targets.
- [x] Add gem capture time extensions.
- [x] Add wall/floor/ceiling speed penalties.
- [x] Add win state.
- [x] Retune score around time remaining, captures, wall contacts, and clean flight.
- [x] Limit Cuarzito's top speed for a more controlled tunnel traversal.
- [x] Remove orange spark particles from gameplay.
- [x] Add structural intro/pre-chase state before countdown.
- [x] Draw first intro screen pass where the cave exit and space are visible before the chase.
- [x] Make gameplay tunnel feel enclosed, with the far exit usually hidden.
- [x] Replace the static cave opening with streaming tunnel rings — walls, floor, and ceiling now move convincingly past the camera.
- [x] Animate Cuarzito approaching and the stones entering the tunnel during intro.

### Phase A - Baseline Verification

- Build the current prototype.
- Run it locally.
- Confirm keyboard input.
- Confirm collisions, scoring, game over, and restart.
- Note any build/runtime issues before refactoring.

### Phase B - Rendering Refactor

- [x] Add `GameWidget.h/.cpp`.
- [x] Add `CaveRenderer.h/.cpp`.
- [x] Change `MainWindow` to use `GameWidget`.
- [x] Move the timer/game loop from `GameScene`/`QGraphicsScene` into `GameWidget`.
- [x] Convert `GameScene` into a `QObject`.
- [x] Replace `drawBackground` override with explicit draw methods.
- [x] Replace `QGraphicsTextItem` with `QPainter` HUD/overlay drawing.
- [x] Update CMake to link `Qt::OpenGL` and `Qt::OpenGLWidgets`.

### Phase C - Cave/Tunnel Visuals

- [x] Implement a dark cave background close to `cave.png`.
- [x] Build an irregular angular cave opening, not a rectangle.
- [x] Add star field and one bright Polaris point.
- [x] Add subtle aurora/fog/glow.
- [x] Remove orange sparks so the tunnel walls carry the 3D effect.
- [x] Make cave movement follow the vanishing point.
- [x] Replace the current mostly static cave opening with moving tunnel walls, floor, and ceiling.
- Test readability at 1280x720 and fullscreen.

### Phase D - Cuarzito Visuals

- Improve the hood silhouette.
- Make the body darker and simpler.
- Make the default pose rear-facing; the full visor is not visible from behind.
- Use only small side visor glimpses during lateral movement.
- Use brief full visor reveals for crystal pickup and game-over feedback.
- Reserve a more explicit start turn/spin animation for later.
- Improve blue aura.
- Add subtle idle bob.
- Keep the hitbox forgiving and independent from the visual size.

### Phase E - Event Flow

- Expand `GameState` to:

```cpp
enum class GameState {
    Attract,
    Countdown,
    Playing,
    GameOver,
    HighScoreEntry
};
```

- [x] Add countdown before play.
- [x] Add top-10 local high score persistence.
- [x] Show top scores in attract and game-over overlays.
- [x] Draw top scores separately from the main centered overlay for readability.
- [x] Add 3-letter initials entry.
- Restart from game over goes through countdown.
- [x] Return automatically to attract mode after score entry or game-over timeout.

### Phase F - Input

- [x] Replace direct movement queries with action queries.
- [x] Keep keyboard mapping.
- [x] Add basic Windows XInput gamepad support.
- [x] Add SDL3 dynamic backend for DualSense (confirmed working).
- Tune dead zone and sensitivity.

### Phase G - Polish

- [x] Add impact flash.
- [x] Add crystal pickup particle burst.
- [x] Add start/collect/game-over/high-score confirm sounds.
- [x] Add subtle generated ambient loop.
- [x] Add fullscreen/event mode toggle with F11.
- Package runtime dependencies cleanly.

### Phase H - Track and Gameplay Overhaul

- [x] Replace sine-wave `TunnelPath` with segment-based track (straight/curve/incline/decline).
- [x] Expose `curvatureH` and `curvatureV` in `TunnelPath::Sample`.
- [x] Add centripetal force physics: curves push Cuarzito toward the outer wall proportional to speed².
- [x] Load the first tunnel from a Qt resource JSON file instead of hardcoding track values.
- [x] Rebalance gem speeds and starting distances so gems are clearly catchable.
- [x] Add 3D mini-map (bottom-right HUD): shows tunnel path ahead, Cuarzito, and gem positions.
- [x] Add CLI test mode (`--test downhill|uphill|vertical`) with dedicated resource tracks and automatic exit at track end.
- [x] Add CLI trace output to `stderr` for test-mode runs.
- [ ] Design and iterate on a first full track (~4000 world units, 8–10 segments).
- [ ] Track editor (future): external visual tool that reads/writes the segment table.
- [ ] Add loops and very sharp turns once the editor exists.

## Immediate Next Step

Resume with the next gameplay and visual pass:

- Finish the vertical compensation contract before restoring inertia:
  - uphill/downhill should move the corridor
  - `Up` / `Down` should compensate that movement
  - safe-zone overlay and actual collision state must agree
- Then iterate first-person mode (currently functional but not tuned): feel, gem visibility, HUD adaptation.
- Improve what can be seen at the end of the tunnel without turning gameplay into open space.
- Build a GUI editor for the tunnel JSON format.
- Make Cuarzito exit into the stars/space when the game ends.
- Add obstacles inside the tunnel.
- Improve the tunnel appearance: richer facets, better depth, stronger cave identity.
- Add proper sound and music beyond the current generated tones/ambient loop.
- Design and iterate on a first full track (~4000 world units, 8–10 segments).

Remaining from earlier phases:
- Readability test at event screen (needs physical screen).
- Dead zone / sensitivity tuning (Phase F).
