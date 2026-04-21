# CLAUDE.md - Cuarzito Pre-Show Game

Context and working instructions for AI-assisted development on this project.

## What This Is

This is a short arcade pre-show game for the Cuarzito cyber-theatre experience.

The player controls **Cuarzito**, a small dark hooded figure with a neon green visor and blue aura, flying through a cosmic mineral cave. The design is pivoting from the original obstacle-dodge prototype into a chase game: Cuarzito pursues four flying gems through a winding tunnel before time runs out.

Primary requirements:

- Simple enough to understand instantly.
- Beautiful enough for a large screen or TV.
- Robust enough for live event use.
- Launch fullscreen by default for event use.
- Short, replayable runs of about 20 to 60 seconds.
- Keyboard fallback always available.
- Basic XInput gamepad support available on Windows.
- Optional SDL2 controller backend for DualSense. It loads `SDL2.dll` dynamically at runtime.
- Open issue: the user's DualSense still did not respond in the game after being confirmed working elsewhere. Keyboard remains the reliable fallback; later work should add diagnostics for SDL load status, detected controller count, GUID, and mapping.

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
- Keyboard, XInput, and optional SDL2 controller input through `InputManager`.
- Pseudo-3D projection using a moving vanishing point.
- Four-direction movement inside tunnel bounds.
- Four persistent chase gems.
- Chase redesign in progress around world `z`, tunnel path samples, acceleration/braking, and timer-based target captures.
- Scoring, HUD, score popups, burst effects, stars.
- Procedural placeholder Cuarzito.
- Dedicated `CaveRenderer` with a QPainter-based faceted cave/space background.

Important files:

| File | Current role |
|---|---|
| `src/main.cpp` | QApplication entry point and signal handling. |
| `src/MainWindow.*` | Creates the main window and installs `GameWidget`. |
| `src/GameWidget.*` | `QOpenGLWidget`, forwards keyboard events, owns timer, cave renderer, and aspect fit. |
| `src/CaveRenderer.*` | Draws dark faceted cave, space, stars, Polaris, aurora, and floor glow. |
| `src/GameScene.*` | Game state, entities, projection, updates, drawing. |
| `src/TunnelPath.*` | Provides deterministic tunnel center/radius samples by world `z` for the chase redesign. |
| `src/AudioManager.*` | Generates cue tones and a subtle ambient loop, then plays them through `QSoundEffect`. |
| `src/InputManager.*` | Maps keyboard, XInput, and optional SDL2 controller input to abstract actions. |
| `CMakeLists.txt` | Qt Widgets plus OpenGL/OpenGLWidgets build. |

The OpenGL widget shell and first `CaveRenderer` are now in place, but the current tunnel visual still does not deliver the intended traversal feel.

## Resume Priority

Stopped after a successful compile. The user reported that the result does **not** work as expected. The key point for the next session is:

> Get the walls, floor, and ceiling of the tunnel to move convincingly.

This is the top priority. Do not continue with gameplay, scoring, intro animation, DualSense diagnostics, or the parked 3D labyrinth idea until the tunnel movement feels right.

Next-session direction:

- Work in `CaveRenderer` first.
- Replace or augment the current static ring/facet look with moving tunnel segments/rings.
- The player should feel like Cuarzito is traveling through a cave tube: walls, ceiling, and floor pass by the camera.
- Gameplay mode should usually hide the far space exit. Space can remain visible in attract/intro.
- Use `frame.worldSpeed`, `frame.time`, `frame.vanishingPoint`, `frame.turnOcclusion`, and eventually `TunnelPath`-derived curve data to drive visual motion.
- It is acceptable to make a focused renderer prototype before tuning gameplay again.

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
- Blue electric aura.
- No visual clutter.

During normal gameplay, Cuarzito is facing away from the camera. Do not draw the full green visor in the default rear-view pose. The visor may be shown only as a small side glimpse during lateral movement, or in explicit start, pickup, and game-over turn/spin animations. At game size, the default pose should read as: dark hooded back silhouette + blue aura.

## Chase Game Design

Target loop:

- Cuarzito starts near `z = 0`.
- Four gems start ahead at increasing `z` distances.
- Gems move forward at constant speed.
- Cuarzito accelerates and brakes based on player input.
- The cave path bends in x/y as `z` increases, so the tunnel itself creates the 3D challenge.
- The tunnel should use sharp enough curves and steep enough rises/drops that the far exit is sometimes partly or fully hidden during turns.
- Current implementation exposes a first `TunnelPath::Sample::occlusion` value and a painter-based cave cap. Later camera/render work should make this more physically convincing.
- Presentation target: the intro can show the cave mouth and space behind it, with four stones floating before they enter the tunnel. Gameplay should then feel enclosed inside the tunnel, with no persistent far exit.
- Orange sparks are intentionally removed for now. The 3D feeling should come from moving walls, ceiling, and floor.
- Parked future idea: a true 3D labyrinth chase with branching tunnels. This is feasible later, but it should not distract from making the current tunnel traversal feel good.
- Wall, floor, and ceiling collisions reduce Cuarzito's speed and break clean-flight bonuses.
- Each captured gem adds time.
- Capturing all four gems wins.
- Running out of time loses.

Target controls:

- Steer with arrows/WASD, left stick, or D-pad.
- Accelerate with Space/Enter now, later A/R2 on gamepad.
- Brake with Shift/Ctrl, later B/L2 on gamepad.

Core model:

```text
TunnelPath.sample(z) -> center x/y, tangent x/y, radius

Player:
  z
  speed
  local x/y offset from tunnel center
  wall contact state

Gem:
  color
  z
  constant speed
  local x/y offset from tunnel center
  collected flag
```

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
- [ ] Animate Cuarzito approaching and the stones entering the tunnel during intro.

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
- [ ] Replace the current mostly static cave opening with moving tunnel walls, floor, and ceiling.
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
- [x] Add optional dynamic SDL2 backend for DualSense.
- Tune dead zone and sensitivity.

### Phase G - Polish

- [x] Add impact flash.
- [x] Add crystal pickup particle burst.
- [x] Add start/collect/game-over/high-score confirm sounds.
- [x] Add subtle generated ambient loop.
- [x] Add fullscreen/event mode toggle with F11.
- Package runtime dependencies cleanly.

## Immediate Next Step

Focus on the cave/tunnel renderer. The next concrete task is to make walls, ceiling, and floor move past the player convincingly during gameplay. Treat this as a visual prototype task inside `CaveRenderer` before doing more chase-game tuning.
