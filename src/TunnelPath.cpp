#include "TunnelPath.h"

#include <QtMath>
#include <cmath>

// ---------------------------------------------------------------------------
// Track definition
//
// Edit this table to change the course.  Each row is one segment:
//   { length,  curvH,    curvV   }
//
// length : world units along the z axis
// curvH  : horizontal curvature, radians/unit  (+ = curves right)
// curvV  : vertical curvature,   radians/unit  (+ = curves up)
//
// Design rules:
//   - Alternate S-curves so the heading stays roughly forward.
//   - Keep |curvH/V * length| (total angle change) ≤ ~1.4 rad per segment
//     so the turn is never invisible from the entrance.
//   - A full vertical loop needs curvV = 2π / length (future feature).
// ---------------------------------------------------------------------------
static const struct { float len, cH, cV; } k_track[] = {
//  length   curvH     curvV
  { 500.f,  0.000f,  0.000f },  //  0: straight approach
  { 300.f,  0.004f,  0.000f },  //  1: gentle right
  { 300.f, -0.004f,  0.000f },  //  2: left — straightens heading
  { 450.f,  0.000f,  0.000f },  //  3: straight
  { 220.f,  0.000f,  0.004f },  //  4: uphill
  { 220.f,  0.000f, -0.004f },  //  5: downhill — straightens heading
  { 400.f,  0.000f,  0.000f },  //  6: straight
  { 260.f, -0.005f,  0.000f },  //  7: medium left
  { 260.f,  0.005f,  0.000f },  //  8: right — straightens
  { 350.f,  0.000f,  0.000f },  //  9: straight
  { 200.f,  0.006f,  0.003f },  // 10: right + uphill
  { 200.f, -0.006f, -0.003f },  // 11: straightens both axes
  { 400.f,  0.000f,  0.000f },  // 12: straight
  { 240.f, -0.007f,  0.000f },  // 13: sharp left
  { 240.f,  0.007f,  0.000f },  // 14: sharp right — straightens
  { 500.f,  0.000f,  0.000f },  // 15: long straight to finish
};
static constexpr int k_segCount = static_cast<int>(sizeof(k_track) / sizeof(k_track[0]));

// ---------------------------------------------------------------------------
// Constructor — precompute keyframe states at each segment boundary
// ---------------------------------------------------------------------------
TunnelPath::TunnelPath()
{
    m_keyframes.reserve(k_segCount);

    float z = 0.f, x = 0.f, y = 0.f, aH = 0.f, aV = 0.f;

    for (int i = 0; i < k_segCount; ++i) {
        Keyframe kf;
        kf.zStart = z;
        kf.x      = x;
        kf.y      = y;
        kf.angleH = aH;
        kf.angleV = aV;
        kf.curvH  = k_track[i].cH;
        kf.curvV  = k_track[i].cV;
        kf.length = k_track[i].len;
        m_keyframes.append(kf);

        // Advance state to end of segment using exact arc integration
        const float L  = k_track[i].len;
        const float cH = k_track[i].cH;
        const float cV = k_track[i].cV;

        if (std::abs(cH) > 1e-6f)
            x += (std::sin(aH + cH * L) - std::sin(aH)) / cH;
        else
            x += std::sin(aH) * L;

        if (std::abs(cV) > 1e-6f)
            y += (std::sin(aV + cV * L) - std::sin(aV)) / cV;
        else
            y += std::sin(aV) * L;

        aH += cH * L;
        aV += cV * L;
        z  += L;
    }
    m_totalLength = z;
}

// ---------------------------------------------------------------------------
// centerAt — exact integration within the containing segment
// ---------------------------------------------------------------------------
QPointF TunnelPath::centerAt(float z) const
{
    // Wrap z beyond end of defined track (repeat last segment indefinitely)
    if (z >= m_totalLength)
        z = m_totalLength - 1.f;
    if (z < 0.f)
        z = 0.f;

    // Binary search for segment
    int lo = 0, hi = m_keyframes.size() - 1;
    while (lo < hi) {
        const int mid = (lo + hi + 1) / 2;
        if (m_keyframes[mid].zStart <= z) lo = mid; else hi = mid - 1;
    }
    const Keyframe &kf = m_keyframes[lo];
    const float t  = z - kf.zStart;
    const float aH = kf.angleH;
    const float aV = kf.angleV;
    const float cH = kf.curvH;
    const float cV = kf.curvV;

    float x, y;
    if (std::abs(cH) > 1e-6f)
        x = kf.x + (std::sin(aH + cH * t) - std::sin(aH)) / cH;
    else
        x = kf.x + std::sin(aH) * t;

    if (std::abs(cV) > 1e-6f)
        y = kf.y + (std::sin(aV + cV * t) - std::sin(aV)) / cV;
    else
        y = kf.y + std::sin(aV) * t;

    return QPointF(x, y);
}

// ---------------------------------------------------------------------------
// radiusAt — tunnel cross-section radius (constant for now, easily varied)
// ---------------------------------------------------------------------------
float TunnelPath::radiusAt(float /*z*/) const
{
    return 160.f;
}

// ---------------------------------------------------------------------------
// sample — public API
// ---------------------------------------------------------------------------
TunnelPath::Sample TunnelPath::sample(float z) const
{
    constexpr float step = 4.f;

    Sample result;
    result.center = centerAt(z);

    // Tangent from finite difference
    const QPointF before = centerAt(z - step);
    const QPointF after  = centerAt(z + step);
    QPointF tangent = after - before;
    const float len = qSqrt(tangent.x() * tangent.x() + tangent.y() * tangent.y());
    if (len > 1e-6f) tangent /= len;
    result.tangent = tangent;

    result.radius      = radiusAt(z);
    result.innerRadius = result.radius * 0.74f;

    // Curvature from the containing segment
    int lo = 0, hi = m_keyframes.size() - 1;
    while (lo < hi) {
        const int mid = (lo + hi + 1) / 2;
        if (m_keyframes[mid].zStart <= z) lo = mid; else hi = mid - 1;
    }
    result.curvatureH = m_keyframes[lo].curvH;
    result.curvatureV = m_keyframes[lo].curvV;

    // Occlusion: bend between a near and far look-ahead point
    const QPointF nearC = centerAt(z + 90.f);
    const QPointF farC  = centerAt(z + 430.f);
    const QPointF bend  = farC - nearC;
    const float bendAmt = qSqrt(bend.x() * bend.x() + bend.y() * bend.y());
    result.occlusion = qBound(0.f, (bendAmt - 95.f) / 135.f, 1.f);

    return result;
}

// ---------------------------------------------------------------------------
// gemOffset — lateral orbit so gems aren't all stacked on the tunnel centre
// ---------------------------------------------------------------------------
QPointF TunnelPath::gemOffset(int gemIndex, float z) const
{
    const float phase  = static_cast<float>(gemIndex) * 1.75f;
    const float radius = 28.f + static_cast<float>(gemIndex) * 9.f;
    const float x = qSin(z * 0.011f + phase) * radius;
    const float y = qCos(z * 0.008f + phase * 0.7f) * radius * 0.72f;
    return QPointF(x, y);
}
