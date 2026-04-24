# Track Format

Tracks are loaded from Qt resources at startup. The current built-in tracks are:

```text
resources/tracks/demo_tunnel.json
resources/tracks/live_tunnel.json
```

They are embedded through:

```text
resources/resources.qrc
```

Runtime uses the selected resource path, for example:

```cpp
TunnelPath(QStringLiteral(":/tracks/demo_tunnel.json"));
```

## Units

World distance is defined from speed over time:

```text
distance = speed * time
distance = 200 units/second * 1 second
distance = 200 world units
```

The first prototype used `200` world units for every segment. The current authored tracks keep the same unit scale, but usually use:

```text
straight distance = 200 units/second * 3 seconds = 600 world units
turn distance     = 200 units/second * 2 seconds = 400 world units
```

## Segment Types

The supported segment types are:

| Type | Meaning | Angle |
|---|---|---|
| `straight` | no bend | omitted or `0` |
| `left` | horizontal turn left | required |
| `right` | horizontal turn right | required |
| `uphill` | vertical turn upward | required |
| `downhill` | vertical turn downward | required |

Each segment has a `length`. Every non-straight segment also has `angleDegrees`.

Example:

```json
{ "type": "left", "length": 200, "angleDegrees": 30 }
```

## Curvature

The loader converts `angleDegrees` and `length` into curvature:

```text
curvature = angleRadians / length
```

For the original baseline:

```text
30 degrees = 0.523599 radians
curvature = 0.523599 / 200
curvature = 0.002618 radians per world unit
```

For the current first tunnel:

```text
45 degrees = 0.785398 radians
curvature = 0.785398 / 400
curvature = 0.001963 radians per world unit
```

Horizontal curvature drives left/right tunnel bending. Vertical curvature drives uphill/downhill bending.

## Track-Level Gem Config

Each track can also define a `gems` array. This is how gem pacing is tuned per route.

Supported fields:

| Field | Meaning |
|---|---|
| `startZ` | initial world-z position of the gem |
| `speed` | forward speed of the gem |

Example:

```json
"gems": [
  { "startZ": 380, "speed": 205 },
  { "startZ": 680, "speed": 212 },
  { "startZ": 1040, "speed": 219 },
  { "startZ": 1460, "speed": 226 }
]
```

If a track omits a gem config, runtime falls back to the built-in defaults for that gem slot.

## Runtime Behavior Notes

`TunnelPath::Sample` exposes:

- `center`
- `tangent`
- `radius`
- `innerRadius`
- `occlusion`
- `curvatureH`
- `curvatureV`

Current gameplay uses the sampled route for:

- camera look-ahead
- safe-zone placement
- route-driven drift
- turn occlusion
- vertical horizon hiding

Curve inertia is still intentionally disabled while the current tunnel-control model is being tuned, so do not treat the old inertia formula as active documentation.

## Current Track Intent

`demo_tunnel` is the short route for quick tests. Its current pattern is:

```text
straight -> uphill -> straight -> downhill -> straight -> left -> straight -> right -> straight
```

`live_tunnel` is the longer route for gameplay and longer balancing passes. It mixes several uphill, downhill, left, and right sections with straights in between.

## Next Format Needs

The current JSON format is enough for the first hand-authored tunnel, but the next phase needs a GUI editor and richer tuning data. Expected additions:

- Per-segment visual preview in an editor.
- Easier editing of type, length, and angle.
- Stronger turn previews so left/right/up/down bends are visible on walls, ceiling, and floor.
- Optional segment metadata for obstacles.
- Optional per-segment difficulty or inertia multiplier if one global curve force is not enough.
- Optional richer per-gem settings if later needed, such as value or pickup radius.
- Export back to the same Qt resource JSON format.

The editor should preserve the current `segments` array so runtime loading remains simple.
