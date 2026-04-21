# CLAUDE.md - Cuarzito Pre-Show Game

Context and working instructions for AI-assisted development on this project.

## What This Is

This is a short arcade pre-show game for the Cuarzito cyber-theatre experience.

The player controls **Cuarzito**, a small dark hooded figure with a neon green visor and blue aura, flying through a cosmic mineral cave. The player dodges obstacles, collects quartz crystals, and survives as long as possible while speed and difficulty ramp up.

Primary requirements:

- Simple enough to understand instantly.
- Beautiful enough for a large screen or TV.
- Robust enough for live event use.
- Launch fullscreen by default for event use.
- Short, replayable runs of about 20 to 60 seconds.
- Keyboard fallback always available.
- Basic XInput gamepad support available on Windows.

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
- Attract, Countdown, Playing, GameOver, and HighScoreEntry states.
- Keyboard and basic XInput gamepad input through `InputManager`.
- Pseudo-3D projection using a moving vanishing point.
- Four-direction movement inside tunnel bounds.
- Obstacle and collectible spawning.
- Scoring, HUD, score popups, sparks, stars.
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
| `src/InputManager.*` | Maps keyboard and XInput gamepad input to abstract actions. |
| `CMakeLists.txt` | Qt Widgets plus OpenGL/OpenGLWidgets build. |

The OpenGL widget shell and first `CaveRenderer` are now in place. The next target is visual tuning and, later, optional GLSL internals.

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

## Design Rules

Keep these unless the user explicitly changes the direction:

- One hit equals game over.
- Collision should be forgiving.
- The player hitbox should be smaller than the visual.
- Obstacles should not become active the instant they spawn.
- Curves/tunnel motion must be survivable by a skilled player.
- Player, obstacles, and collectibles must remain visually distinct.
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
       -> GameScene::drawSparks(...)
       -> GameScene::drawCollectibles(...)
       -> GameScene::drawObstacles(...)
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

Current important constants:

```cpp
SPAWN_Z   = 900;
REMOVE_Z  = 25;
COLLIDE_Z = 460;
FOCAL     = 400;
```

The moving vanishing point is a core part of the game feel. Keep the speed cap so the tunnel curve cannot outrun the player.

## Action List

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
- [x] Keep orange sparks.
- [x] Make cave movement follow the vanishing point.
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
- [x] Add 3-letter initials entry.
- Restart from game over goes through countdown.
- [x] Return automatically to attract mode after score entry or game-over timeout.

### Phase F - Input

- [x] Replace direct movement queries with action queries.
- [x] Keep keyboard mapping.
- [x] Add basic Windows XInput gamepad support.
- Tune dead zone and sensitivity.

### Phase G - Polish

- [x] Add impact flash.
- [x] Add crystal pickup particle burst.
- Add start/collect/game-over sounds.
- Add ambient loop.
- [x] Add fullscreen/event mode toggle with F11.
- Package runtime dependencies cleanly.

## Immediate Next Step

Start with **Phase A**, then move directly into **Phase B**. The current core gameplay is good enough to preserve; the biggest quality gain will come from the rendering refactor and a cave/tunnel that matches the reference art.
