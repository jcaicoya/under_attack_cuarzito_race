# Cuarzito Pre-Show Game

A short arcade pre-show minigame for the **Cuarzito** cyber-theatre experience. It is built in **C++ / Qt** for Windows and intended to run on a large monitor or TV with keyboard fallback, XInput support, and optional SDL2 controller support for DualSense.

The game launches fullscreen by default for event use. Press `F11` to toggle fullscreen/windowed mode during development.

The player controls **Cuarzito**, a small dark hooded figure with a neon green visor, while flying through a cosmic crystal cave. The game is pivoting from a survival/dodge prototype into a chase game: Cuarzito pursues four flying gems through a winding tunnel before time runs out.

## Current State

The repository currently contains a playable Qt prototype with an OpenGL-ready widget shell:

- `GameWidget : QOpenGLWidget` as the active main game widget.
- `GameScene : QObject` for game state, updates, projection, and draw helpers.
- `QPainter` rendering on top of the OpenGL widget.
- Fixed 1280x720 gameplay canvas.
- Attract screen, countdown state, playing state, and game-over state.
- Keyboard input: arrows/WASD, Space/Enter, Escape.
- Pseudo-3D projection using a moving vanishing point.
- Four-direction player movement inside the tunnel.
- Obstacle spawning, collectible spawning, scoring, popups, sparks, and basic HUD from the previous prototype.
- First chase-game infrastructure is being added around a persistent tunnel path and world `z` positions.
- Procedural placeholder Cuarzito drawn with `QPainter`.
- Dedicated `CaveRenderer` with a QPainter-based faceted cave/space background.

The prototype proves the core loop works. The current cave renderer is intentionally isolated so it can later be upgraded internally to GLSL without changing gameplay code.

## Visual References

Two root-level PNG files define the desired style.

### `cave.png`

Reference for the cave/tunnel environment:

- Very dark low-poly mineral cave.
- Central opening into deep space.
- Stars visible through the cave, including one brighter Polaris-like point.
- Subtle blue, cyan, and teal glow.
- Orange spark particles.
- Floating crystal/gem shapes.
- Angular rock silhouettes rather than clean rectangular walls.

The game cave should not be photorealistic. It should be stage-readable, dark, atmospheric, and clearly playable from across a room.

### `cuarzito.png`

Reference for the player character:

- Small floating dark hooded figure.
- Compact black cloak silhouette.
- No visible legs.
- One horizontal neon green visor/eye slit.
- Blue electric aura.
- Reads instantly at small sizes.

During normal gameplay, Cuarzito is mostly seen from behind, flying away from the camera. The green visor should not be visible as a full front-facing line in the default rear view. It can appear as a brief side glimpse while moving sideways, or in simple start, pickup, and game-over turn/spin animations. The character should stay simple and iconic; the rear silhouette and blue aura matter more than fine detail.

## Gameplay Direction

The target game is a short chase through a cave/tunnel:

- **Steer:** arrows/WASD, left stick, or D-pad.
- **Accelerate:** one button/key increases Cuarzito's speed.
- **Brake:** one button/key reduces speed for tighter control.
- **Chase:** four colored gems fly ahead of Cuarzito at constant speed.
- **Catch:** reaching a gem adds time and advances the chase.
- **Avoid walls:** scraping the wall, floor, or ceiling slows Cuarzito instead of instantly killing him.
- **Win:** collect all four gems.
- **Lose:** timer reaches zero before all gems are collected.
- **Session target:** successful runs should still feel short, around 60 to 90 seconds.

Initial chase model:

```text
Cuarzito starts near z = 0.
Blue gem starts ahead.
Orange gem starts farther ahead.
Yellow gem starts farther ahead again.
Cyan gem is the final target.

All gems move forward at constant speed.
Cuarzito's z speed depends on player acceleration, braking, and wall collisions.
The tunnel path bends through x/y as z increases, like a worm-shaped cave.
```

Design rules to preserve:

- Collisions should be forgiving.
- Wall/floor/ceiling impacts should reduce speed and break score bonuses, not instantly end the run.
- The player must always have enough reaction time.
- The tunnel/curve motion must be readable and survivable by a skilled player.
- The screen must clearly distinguish player, danger, and collectibles.
- Keyboard fallback must always work in live-event conditions.

## Tech Stack

Current prototype:

| Layer | Current choice |
|---|---|
| Language | C++23 |
| Framework | Qt 6.7.3, MSVC, Windows |
| Main widget | `QOpenGLWidget` |
| Drawing | `QPainter` on the OpenGL widget |
| Game logic | `GameScene : QObject` |
| Game loop | `QTimer` around 60 FPS in `GameWidget` |
| Input | Qt keyboard events |
| Audio | `Qt::Multimedia` generated tone effects |

Recommended next architecture:

| Layer | Target choice |
|---|---|
| Main widget | `QOpenGLWidget` |
| Cave/tunnel | `CaveRenderer`, currently QPainter-based and OpenGL-widget backed |
| Entities/HUD | `QPainter` drawn on top of the OpenGL surface |
| Game logic | Plain `QObject`/C++ class, not `QGraphicsScene` |
| Persistence | JSON high scores |
| Input | Action abstraction with keyboard and gamepad |

Qt 6.7.3 is enough for the next stage. Moving to another Qt version is acceptable if it solves a real problem, but the game should stay simple and robust.

## Project Structure

Current files:

```text
preshow-game/
├── CMakeLists.txt
├── README.md
├── CLAUDE.md
├── cuarzito_preshow_game_design.md
├── cave.png
├── cuarzito.png
└── src/
    ├── main.cpp
    ├── MainWindow.h / .cpp
    ├── GameWidget.h / .cpp
    ├── GameScene.h / .cpp
    └── InputManager.h / .cpp
```

Target files after the rendering refactor:

```text
src/
├── main.cpp
├── MainWindow.h / .cpp
├── GameWidget.h / .cpp       # QOpenGLWidget, owns rendering loop
├── CaveRenderer.h / .cpp     # cave/space/tunnel renderer
├── GameScene.h / .cpp        # game state, entities, update, draw passes
├── TunnelPath.h / .cpp       # world-z tunnel center, radius, and curve samples
├── InputManager.h / .cpp
├── HighScoreManager.h / .cpp
└── shaders/
    ├── cave.vert
    └── cave.frag
```

## Action Plan

### 0. Chase-Game Redesign

- [x] Replace the old long-term goal with a chase concept.
- [x] Define the world model around `z`, speed, tunnel path, and four target gems.
- [x] Add `TunnelPath` as the source of tunnel center/radius samples by `z`.
- [ ] Refactor player state from screen-survival movement to chase movement: `z`, speed, acceleration, braking, and local tunnel offset.
- [ ] Replace random collectible spawning with four persistent flying gems.
- [ ] Add timer extensions per captured gem.
- [ ] Add wall/floor/ceiling speed penalties.
- [ ] Add win state for collecting all gems.
- [ ] Retune scoring for time, clean flight, and gem captures.

### 1. Verify and Preserve the Prototype

- Build and run the current game.
- Confirm keyboard input, restart flow, scoring, collisions, and frame pacing.
- Keep the current gameplay constants as the first tuning baseline.

### 2. Refactor Rendering

- [x] Replace `GameView : QGraphicsView` with `GameWidget : QOpenGLWidget`.
- [x] Convert `GameScene` from `QGraphicsScene` into a `QObject` game/render helper.
- [x] Replace `QGraphicsTextItem` HUD/overlay with direct `QPainter` text.
- [x] Keep entity structs and projection math.
- [x] Update CMake to link `Qt::OpenGL` and `Qt::OpenGLWidgets`.
- [x] Extract/replace the old `drawEnvironment()` path with `CaveRenderer`.

### 3. Build the Cave/Tunnel Look

- [x] Implement a first cave renderer that matches `cave.png`:
  - irregular angular cave boundary,
  - deep space center,
  - stars and one bright Polaris point,
  - subtle aurora/blue-teal glow,
  - dark faceted rock walls.
- [x] Keep orange spark motion as a gameplay-layer draw pass.
- Prefer shader/OpenGL if it gives a clear visual win.
- Keep gameplay readability above visual detail.

### 4. Upgrade Cuarzito

- Improve the procedural character silhouette.
- Make the hood and cloak closer to `cuarzito.png`.
- Keep the default pose rear-facing, with only side glimpses of the neon green visor.
- Add blue electric aura and subtle idle bob.
- Add brief visor reveal on crystal pickup and game over.
- Later option: use a transparent PNG sprite if the procedural version is not close enough.

### 5. Complete Event Flow

- [x] Add `Countdown`.
- Improve attract mode.
- [x] Add top-10 local high scores in JSON.
- [x] Show top scores in attract and game-over screens.
- [x] Draw top scores as a separate side panel for readability.
- [x] Add 3-letter initials entry.
- Current flow: `Attract -> Countdown -> Playing -> GameOver -> HighScoreEntry -> Attract`.
- Game over restarts with confirm or returns to attract after an idle timeout.

### 6. Add Gamepad Support

- [x] Introduce abstract actions: move, confirm, cancel, fullscreen.
- [x] Map keyboard to actions first.
- [x] Add basic Windows XInput gamepad support.
- [x] Add optional dynamic SDL2 controller backend for DualSense.
- Tune dead zone and sensitivity.

### 7. Polish for Live Use

- [x] Impact flash.
- [x] Crystal collection burst.
- [x] Start, collect, game-over, and high-score confirm sounds.
- [x] Subtle generated ambient loop.
- [x] Fullscreen/event mode toggle with F11.
- Reliable startup with no missing runtime assets.

## Controls

| Action | Keyboard now | Gamepad target |
|---|---|---|
| Move | Arrows / WASD | Left stick / D-pad |
| Accelerate / Start / Confirm | Space / Enter | A / R2 / Start |
| Brake | Shift / Ctrl target | B / L2 target |
| Cancel | Escape | B / Back |
| Fullscreen toggle | F11 | Optional |
| Quit development build | Escape | Optional |

DualSense note: PlayStation controllers usually do not expose themselves as XInput devices. For the show controller, place `SDL2.dll` beside `cuarzito-race.exe` or make it available on `PATH`; the game will load it dynamically and use SDL's controller mapping when present.

Open issue: the current DualSense controller still did not respond in local testing even after the controller was updated and confirmed working elsewhere. Keep keyboard as the reliable fallback for now. Later work should add controller diagnostics, verify whether SDL2 is actually loaded, and inspect the controller GUID/mapping.

## Build Notes

The current CMake configuration assumes Qt at:

```text
C:/Qt/6.7.3/msvc2022_64
```

Current target:

```text
cuarzito-race
```

The OpenGL refactor will require:

```cmake
find_package(Qt6 COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets REQUIRED)

target_link_libraries(cuarzito-race
    Qt::Core
    Qt::Gui
    Qt::Widgets
    Qt::OpenGL
    Qt::OpenGLWidgets)
```
