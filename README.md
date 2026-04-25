# Cuarzito Pre-Show Game

A short arcade pre-show minigame for the **Cuarzito** cyber-theatre experience. It is built in **C++ / Qt** for Windows and intended to run on a large monitor or TV with keyboard fallback, XInput support, and optional SDL3 controller support for DualSense.

The game launches fullscreen by default for event use. Press `F11` to toggle fullscreen/windowed mode during development.

The player controls **Cuarzito**, a small dark hooded figure with a neon green visor, while flying through a cosmic crystal cave. The game is now a chase game: Cuarzito pursues four flying gems through a winding tunnel, trying to finish the route, catch all gems, and avoid running out of energy from crashes.

## Current State

The repository currently contains a playable Qt prototype with an OpenGL-ready widget shell:

- `GameWidget : QOpenGLWidget` as the active main game widget.
- `GameScene : QObject` for game state, updates, projection, and draw helpers.
- `QPainter` rendering on top of the OpenGL widget.
- Fixed 1280x720 gameplay canvas.
- Attract, intro, countdown, playing, success-flyout, failure-glide, game-over, and high-score entry states.
- Keyboard input: arrows/WASD, Space/Enter, Shift/Ctrl, R, Escape, F11.
- Pseudo-3D projection using a moving vanishing point.
- Four-direction player movement inside the tunnel.
- Four persistent chase gems, score popups, burst effects, and basic HUD.
- Chase-game infrastructure around a persistent tunnel path and world `z` positions.
- Procedural placeholder Cuarzito drawn with `QPainter`.
- Dedicated `CaveRenderer` with a QPainter-based streaming faceted tunnel.
- Track data loaded from `resources/tracks/demo_tunnel.json` and `resources/tracks/live_tunnel.json` through Qt resources.
- Attract-mode track selection with Left/Right and confirm.
- Per-track gem `startZ` and `speed` configuration loaded from each track JSON.
- Keyboard, XInput, and optional SDL3 controller input through `InputManager`; backend-specific runtime loading/polling lives in `XInputControllerBackend` and `SdlControllerBackend`.
- Energy-based failure in place of the old countdown-loss model.
- Route-driven vertical horizon hiding for uphill/downhill in enclosed tunnel gameplay.
- Fullscreen uses fit scaling so the full authored 1280x720 frame stays visible on ultrawide screens.
- Cuarzito is currently drawn without a persistent aura to preserve tunnel visibility.

The prototype proves the core loop works. The current cave renderer is intentionally isolated so it can later be upgraded internally to GLSL without changing gameplay code.

## Current Resume Note

Current implemented state:

- The selectable tracks are:
  - `demo_tunnel`: short route for quick tests
  - `live_tunnel`: longer route for gameplay and long tests
- The game ends when one of these happens:
  - Cuarzito reaches the end of the current track
  - all four gems are collected
  - energy reaches `0`
- Endings are now split into three non-interactive variants before the final score/high-score screen:
  - `TRACK COMPLETE`
  - `ALL GEMS CAPTURED`
  - `OUT OF ENERGY`
- Mini-map markers now show route position only:
  - Cuarzito steering inside the tunnel does not move the map marker
  - gem local orbit offsets are ignored on the mini-map too
- Uphill/downhill horizon hiding now works by deforming the enclosed far box-stack itself
- The visible size pop near center was removed by making that vertical deformation proportional to the current occlusion amount

Still intentionally true:

- default runs start invulnerable for testing/demo convenience
- invulnerability can also be toggled manually with `I`
- the safe-zone rectangle is still drawn during gameplay for debugging
- curve inertia is still intentionally set to `0.0`
- CLI test mode still exists:
  - `cuarzito-race.exe --test downhill`
  - `cuarzito-race.exe --test uphill`
  - `cuarzito-race.exe --test vertical`

## Visual References

PNG reference assets live under `resources/images/` when checked in.

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

### `resources/images/cuarzito.png`

Reference for the player character:

- Small floating dark hooded figure, drawn without a persistent aura during gameplay for tunnel visibility.
- Compact black cloak silhouette.
- No visible legs.
- One horizontal neon green visor/eye slit.
- Blue electric aura in reference art, but gameplay currently omits the persistent aura for visibility.
- Reads instantly at small sizes.

During normal gameplay, Cuarzito is mostly seen from behind, flying away from the camera. The green visor should not be visible as a full front-facing line in the default rear view. It can appear as a brief side glimpse while moving sideways, or in simple start, pickup, and game-over turn/spin animations. The character should stay simple and iconic; the rear silhouette and blue aura matter more than fine detail.

## Gameplay Direction

The target game is a short chase through a cave/tunnel:

- **Steer:** arrows/WASD, left stick, or D-pad.
- **Accelerate:** one button/key increases Cuarzito's speed.
- **Brake:** one button/key reduces speed for tighter control.
- **Chase:** four colored gems fly ahead of Cuarzito at constant speed.
- **Catch:** reaching a gem adds score and advances the chase.
- **Avoid walls:** scraping the wall, floor, or ceiling reduces energy and slows Cuarzito instead of instantly killing him.
- **Win:** either collect all four gems or finish the track.
- **Lose:** run out of energy.
- **Session target:** `demo_tunnel` is short for iteration; `live_tunnel` is the current longer gameplay route.

Presentation target:

- The attract/intro screen can show the cave mouth opening into space, with the four stones floating in the distance.
- When the chase begins, the gems and Cuarzito enter the tunnel.
- During gameplay, the player should feel enclosed inside the tunnel, but the tunnel ahead must remain visible enough to predict turns.
- The 3D effect should come primarily from moving cave walls, ceiling, and floor, not from orange particles.
- Orange particles are removed for now and can return later only if they support readability.

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
Sharp turns and steep rises/drops should sometimes hide the far tunnel opening, so the player feels the cave wrapping around them instead of seeing a straight exit at all times.
```

Future idea to keep parked: a real 3D labyrinth chase, where gems choose paths through branching tunnels. This is feasible later, but it should wait until the core straight-path tunnel chase feels good.

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
├── docs/
│   └── track-format.md
├── resources/
│   ├── resources.qrc
│   └── tracks/
│       ├── demo_tunnel.json
│       └── live_tunnel.json
├── cuarzito_preshow_game_design.md
├── resources/images/
│   └── cuarzito.png
└── src/
    ├── main.cpp
    ├── MainWindow.h / .cpp
    ├── GameWidget.h / .cpp
    ├── GameScene.h / .cpp
    ├── SdlControllerBackend.h / .cpp
    ├── XInputControllerBackend.h / .cpp
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
├── InputAction.h             # shared abstract input action enum
├── InputManager.h / .cpp     # action aggregation and input state
├── KeyboardActionMap.h / .cpp # keyboard key-to-action mapping
├── SdlControllerBackend.h / .cpp # optional SDL3 runtime backend for DualSense/gamepads
├── XInputControllerBackend.h / .cpp # Windows XInput runtime backend
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
- [x] Sharpen the first tunnel path model and add a turn-occlusion signal for hiding the far opening.
- [x] Load the first segment-based tunnel from a Qt resource JSON file.
- [x] Tune first tunnel to 600-unit straights, 400-unit turns, and 45-degree bends.
- [x] Add first chase movement pass: `z`, speed, acceleration, braking, and local tunnel offset.
- [x] Remove old survival movement assumptions from remaining obstacle/collectible code.
- [x] Replace random collectible spawning with four persistent flying gems.
- [x] Replace the old timer-loss model with energy-based failure.
- [x] Add wall/floor/ceiling speed penalties.
- [x] Add win state for collecting all gems.
- [x] Add alternate win state for reaching the end of the track.
- [x] Retune scoring for time, clean flight, wall contacts, and gem captures.
- [x] Limit Cuarzito's top speed for a more controlled tunnel traversal.
- [x] Remove orange spark particles from gameplay.
- [x] Add structural intro/pre-chase state before countdown.
- [x] Draw first intro screen pass: cave exit visible, four stones floating before the chase.
- [x] Make gameplay tunnel feel enclosed, with the far exit usually hidden.
- [x] Split short and long routes into `demo_tunnel` and `live_tunnel`.
- [x] Add attract-mode track selection.
- [x] Make gem pacing configurable per track JSON.
- [x] Add non-interactive ending variants before the final score screen.
- [ ] Animate Cuarzito approaching and the stones entering the tunnel during intro.

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
- [x] Remove orange spark motion so the tunnel walls carry the 3D effect.
- [ ] Replace the current mostly static cave opening with moving tunnel walls, floor, and ceiling.
- Prefer shader/OpenGL if it gives a clear visual win.
- Keep gameplay readability above visual detail.

### 4. Upgrade Cuarzito

- Improve the procedural character silhouette.
- Make the hood and cloak closer to `resources/images/cuarzito.png`.
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
- [x] Add optional dynamic SDL3 controller backend for DualSense.
- [x] Move SDL3 runtime loading and polling into `SdlControllerBackend`.
- [x] Move XInput runtime loading and polling into `XInputControllerBackend`.
- [x] Move keyboard key-to-action mapping into `KeyboardActionMap`.
- [x] Move the shared `Action` enum into `InputAction.h`.
- Tune dead zone and sensitivity.

### 7. Polish for Live Use

- [x] Impact flash.
- [x] Crystal collection burst.
- [x] Start, collect, game-over, and high-score confirm sounds.
- [x] Subtle generated ambient loop.
- [x] Fullscreen/event mode toggle with F11.
- Reliable startup with no missing runtime assets.

### 8. Next Gameplay And Visual Pass

- [ ] Improve what can be seen at the end of the tunnel so the player can read the route without losing the enclosed cave feeling.
- [ ] Make left/right/up/down turns visible directly on the walls, ceiling, and floor; current turns are not pronounced enough.
- [ ] Restore and retune curve inertia after the current route-driven control model is fully signed off. It is intentionally set to zero right now.
- [ ] Add stronger acceleration and braking feel: speed lines, tunnel pulse, FOV/projection response, engine/air sound, or equivalent feedback.
- [ ] Simplify/tune the 3D cube mini-map so left/right/up/down track motion is readable without clutter.
- [ ] Build a GUI tunnel editor for the selectable track JSON files under `resources/tracks/`.
- [ ] Rebalance speeds so Cuarzito cannot trivially overtake gems; he may need to brake or manage distance to let gems pass/settle ahead.
- [ ] Polish the three ending variants further: track complete, all gems captured, and out of energy.
- [ ] Add obstacles inside the tunnel.
- [ ] Improve the tunnel appearance: richer facets, better material contrast, more readable depth, and stronger cave identity.
- [ ] Make crashes much more obvious with stronger visual, motion, and audio feedback.
- [ ] Fix the vertical compensation contract: uphill/downhill move the safe corridor, `Up`/`Down` compensate that motion, and the safe-zone overlay must match actual collision behavior.
- [ ] Add proper sound and music beyond the current generated cue tones/ambient loop.

## Controls

| Action | Keyboard now | Gamepad target |
|---|---|---|
| Move | Arrows / WASD | Left stick / D-pad |
| Accelerate / Start / Confirm | Space / Enter | A / R2 / Start |
| Brake | Shift / Ctrl | B / L2 target |
| Cancel | Escape | B / Back |
| Restart run | R | Optional |
| Fullscreen toggle | F11 | Optional |
| Quit development build | Escape | Optional |

DualSense note: PlayStation controllers usually do not expose themselves as XInput devices. For the show controller, place `SDL3.dll` beside `cuarzito-race.exe` or make it available on `PATH`; the game will load it dynamically through `SdlControllerBackend` and use SDL's controller mapping when present.

Keep keyboard as the reliable fallback for live-event use. Gamepad diagnostics are available through `InputManager::gamepadDiagnostics()` and include SDL3/XInput DLL load status plus detected controller names/IDs where available.

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
