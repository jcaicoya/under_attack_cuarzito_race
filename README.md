# Cuarzito Pre-Show Game

A pre-show arcade minigame for the **Cuarzito** cyber-theatre experience. Built in **C++ with Qt 6**, designed to run on a large monitor or TV and be played with a wireless gamepad.

## Purpose

The game serves as entertainment for the audience while they wait for the show to begin. It needs to be:

- Readable and visually engaging from across the room
- Immediately understandable with no instructions
- Fun for kids and adults alike
- Stable and quick to restart for back-to-back plays
- Visually consistent with the Cuarzito universe

The player controls **Cuarzito** from a fixed rear-view camera through a cosmic crystal cave. The goal is to survive as long as possible by dodging obstacles and collecting glowing quartz crystals. Speed and difficulty increase over time until the run becomes nearly impossible.

## Gameplay

- **Move:** left/right only (gamepad stick or keyboard arrows/WASD)
- **Collect:** quartz crystals for points (+10 normal, +25 special)
- **Dodge:** rocks, mineral pillars, and energy barriers (one hit ends the run)
- **Scoring:** survival points per tick + collected crystals
- **Target session length:** 20 to 60 seconds

## Tech Stack

| Layer | Choice |
|---|---|
| Language | C++ 23 |
| Framework | Qt 6.7 |
| Rendering | QGraphicsView + QGraphicsScene |
| Game loop | QTimer (~60 fps) |
| Persistence | JSON (highscores) |
| Input | Qt keyboard + gamepad (fallback always available) |

## Implementation Plan

### Phase 1 — Playable Prototype
Validate the core movement feel and game loop.

- [ ] Project skeleton: `MainWindow`, `GameView`, `GameScene`
- [ ] Scrolling background (parallax layers: stars, cave walls)
- [ ] `PlayerCuarzito` placeholder moving left/right with keyboard
- [ ] Basic obstacle spawning and forward movement
- [ ] Collision detection → game over
- [ ] Instant restart

**Done when:** it already feels like a Cuarzito arcade runner.

---

### Phase 2 — Collection and Scoring
Make the run measurable and rewarding.

- [ ] Quartz collectibles with pickup detection
- [ ] Survival score (points per second) + crystal score
- [ ] HUD: current score, survival time
- [ ] Progressive speed and spawn rate scaling
- [ ] Difficulty curve tuning

**Done when:** runs are fun and score reflects performance.

---

### Phase 3 — Attract Mode and Ranking
Make it ready to drop into a live event.

- [ ] Attract screen: animated background, Cuarzito idle, "Press START" prompt
- [ ] Top-10 local highscore table (JSON persistence)
- [ ] High-score entry screen (3-letter initials via gamepad/keyboard)
- [ ] Complete flow: Attract → Countdown → Playing → Game Over → High Score Entry → Attract

**Done when:** the game can run unattended at an event.

---

### Phase 4 — Wireless Gamepad Support
Make it robust for public use.

- [ ] Gamepad detection and input mapping (Xbox / generic)
- [ ] Abstract input layer: `MoveLeft`, `MoveRight`, `Confirm`, `Cancel`
- [ ] Automatic fallback to keyboard
- [ ] Sensitivity and dead-zone tuning

**Done when:** playing with a wireless controller feels natural and reliable.

---

### Phase 5 — Visual and Audio Polish
Make it stage-ready.

- [ ] Final Cuarzito sprite and movement animations
- [ ] Particle effects (crystal trails, impact flash, floating debris)
- [ ] Visual feedback for collection and collision
- [ ] Sound effects: collect, game over, start confirmation, high-score entry
- [ ] Ambient loop music (subtle, spatial, non-intrusive)
- [ ] Smooth state transitions

**Done when:** it looks and sounds like it belongs on stage.

---

## Game States

```
Attract → Countdown → Playing → GameOver → HighScoreEntry → Attract
```

## Project Structure (target)

```
preshow-game/
├── main.cpp
├── CMakeLists.txt
├── MainWindow.h / .cpp
├── GameView.h / .cpp
├── GameScene.h / .cpp
├── PlayerCuarzito.h / .cpp
├── Obstacle.h / .cpp
├── Collectible.h / .cpp
├── Spawner.h / .cpp
├── ScoreManager.h / .cpp
├── HighScoreManager.h / .cpp
├── InputManager.h / .cpp
└── assets/
    ├── sprites/
    ├── sounds/
    └── fonts/
```

## Controls

| Action | Gamepad | Keyboard |
|---|---|---|
| Move left | Left stick / D-pad left | ← / A |
| Move right | Right stick / D-pad right | → / D |
| Start / Confirm | A / Start | Space / Enter |
| Quit (dev) | — | Escape |
