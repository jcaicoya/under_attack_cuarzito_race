# Track Format

The tunnel track is loaded from Qt resources at startup. The first track lives at:

```text
resources/tracks/first_tunnel.json
```

It is embedded through:

```text
resources/resources.qrc
```

Runtime code reads it with:

```cpp
QFile file(":/tracks/first_tunnel.json");
```

## Units

World distance is defined from speed over time:

```text
distance = speed * time
distance = 200 units/second * 1 second
distance = 200 world units
```

The first prototype used `200` world units for every segment. The current first tunnel keeps the same unit scale, but uses:

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

## Physics

`TunnelPath::Sample` exposes `curvatureH` and `curvatureV`. `GameScene` uses those values for curve inertia:

```text
drift = -curvature * speed^2 * CURVE_INERTIA_K * dt
```

The negative sign is intentional: a left curve pushes Cuarzito toward the right wall, and a right curve pushes him toward the left wall. Sharper turns, longer held speed, and higher player speed push Cuarzito farther toward the outside wall. Braking before a turn should make the curve easier to hold.

## First Tunnel Pattern

The first track alternates:

```text
straight, left, straight, right, straight, left, straight, right...
```

Straight segments are `600` world units. Turn segments are `400` world units. Every turn is `45` degrees.

## Next Format Needs

The current JSON format is enough for the first hand-authored tunnel, but the next phase needs a GUI editor and richer tuning data. Expected additions:

- Per-segment visual preview in an editor.
- Easier editing of type, length, and angle.
- Stronger turn previews so left/right/up/down bends are visible on walls, ceiling, and floor.
- Optional segment metadata for obstacles.
- Optional per-segment difficulty or inertia multiplier if one global curve force is not enough.
- Export back to the same Qt resource JSON format.

The editor should preserve the current `segments` array so runtime loading remains simple.
