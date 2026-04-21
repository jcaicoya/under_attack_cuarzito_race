# Cuarzito Pre-Show Game

A pre-show arcade minigame for the **Cuarzito** cyber-theatre experience. Built in **C++ with Qt 6**, designed to run on a large monitor or TV and be played with a wireless gamepad.

## Purpose

Entertainment for the audience while they wait for the show to begin. It needs to be:

- Readable and visually engaging from across the room
- Immediately understandable with no instructions
- Fun for kids and adults alike
- Stable and quick to restart for back-to-back plays
- Visually consistent with the Cuarzito universe

The player controls **Cuarzito** from a fixed rear-view camera flying through a cosmic crystal cave. The goal is to survive as long as possible by dodging obstacles that rush toward the camera. Speed and difficulty increase over time until the run becomes nearly impossible.

## Gameplay

- **Move:** all 4 directions around the screen center (gamepad stick or keyboard arrows/WASD)
- **Dodge:** energy barriers and rock formations that grow from a tiny point at the vanishing point and fill the screen as they approach
- **Survive:** as long as possible — speed ramps up until the run is nearly unplayable
- **Collect:** quartz crystals for bonus points *(Phase 2)*
- **Target session length:** 20 to 60 seconds

## Visual concept

Pseudo-3D perspective tunnel: camera fixed behind Cuarzito, looking forward into a dark cosmic cave. Obstacles spawn as tiny shapes at the vanishing point and grow toward the player. Hundreds of orange sparks streak outward from the center, giving the sensation of flying at high speed. Perspective grid lines and rectangular depth rings reinforce the tunnel shape.

## Tech Stack

| Layer | Choice |
|---|---|
| Language | C++ 23 |
| Framework | Qt 6.7 (MSVC, Windows) |
| Rendering | `QGraphicsView` + `QGraphicsScene`, custom `drawBackground` |
| 3D engine | Hand-rolled perspective projection (`screenX = CX + wx × FOCAL / wz`) |
| Game loop | `QTimer` ~60 fps, delta-time based |
| Persistence | JSON (highscores) *(Phase 3)* |
| Input | Qt keyboard + gamepad fallback *(Phase 4)* |

## Implementation Plan

### Phase 1 — Playable Prototype ✅
Validate the core movement feel and game loop.

- [x] Project skeleton: `MainWindow`, `GameView`, `GameScene`
- [x] Pseudo-3D perspective tunnel with convergence lines, depth rings, vignette
- [x] Cuarzito crystal flying in center, 4-directional movement
- [x] Orange spark particles streaming toward the camera
- [x] Obstacles growing from vanishing point, back-to-front rendering
- [x] Collision detection → game over
- [x] Difficulty ramp (speed + spawn rate over time)
- [x] Instant restart

**Done when:** it already feels like a Cuarzito arcade runner. ✅

---

### Phase 2 — Collection and Scoring ✅
Make the run measurable and rewarding.

- [x] Quartz collectibles spawning in world space — **green** gem (+10 pts) and **orange** gem (+25 pts), matching the visual reference
- [x] Survival score (+1 pt/s) + crystal bonus
- [x] HUD: `SCORE` + `TIME` display
- [x] Floating score popups on collection (+10 / +25)
- [x] Collectible spawn rate scales with difficulty

**Done when:** runs are fun and the score reflects how well you played. ✅

---

### Phase 3 — Attract Mode and Ranking
Make it ready to drop into a live event.

- [ ] Attract screen idle state: animated Cuarzito, "PRESS START" prompt, top scores visible
- [ ] Countdown `3 2 1` before gameplay starts
- [ ] Top-10 local highscore table (JSON persistence, `highscores.json`)
- [ ] High-score entry screen (3-letter initials, gamepad-friendly)
- [ ] Complete flow: Attract → Countdown → Playing → Game Over → High Score Entry → Attract

**Done when:** the game can run unattended at an event.

---

### Phase 4 — Wireless Gamepad Support
Make it robust for public use.

- [ ] Gamepad detection and input mapping (Xbox / generic)
- [ ] Abstract action layer already in place (`MoveLeft`, `MoveRight`, `MoveUp`, `MoveDown`, `Confirm`, `Cancel`)
- [ ] Automatic fallback to keyboard
- [ ] Dead-zone and sensitivity tuning

**Done when:** playing with a wireless controller feels natural and reliable.

---

### Phase 5 — Visual and Audio Polish
Make it stage-ready.

- [ ] Final Cuarzito sprite and idle/hit animations
- [ ] Impact flash on collision
- [ ] Crystal collection particle burst
- [ ] Sound effects: collect, game over, start, high-score entry
- [ ] Ambient loop music (subtle, spatial, non-intrusive)
- [ ] Smooth state transitions

**Done when:** it looks and sounds like it belongs on stage.

---

## Game States

```
Attract → Countdown → Playing → GameOver → HighScoreEntry → Attract
```

Currently implemented: `Attract`, `Playing`, `GameOver`.

## Project Structure

```
preshow-game/
├── CMakeLists.txt
├── CLAUDE.md
├── README.md
├── .gitignore
└── src/
    ├── main.cpp
    ├── MainWindow.h / .cpp
    ├── GameView.h / .cpp
    ├── GameScene.h / .cpp      ← core loop, rendering, all game logic
    └── InputManager.h / .cpp
```

Future additions will live under `src/` and optionally `assets/`.

## Controls

| Action | Gamepad | Keyboard |
|---|---|---|
| Move | Left stick / D-pad | ← → ↑ ↓ / WASD |
| Start / Confirm | A / Start | Space / Enter |
| Quit (dev) | — | Escape |
