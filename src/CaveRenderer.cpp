#define _USE_MATH_DEFINES
#include "CaveRenderer.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <algorithm>
#include <cmath>

CaveRenderer::CaveRenderer()
{
    constexpr float w = 1280.f;
    constexpr float h = 720.f;

    for (int i = 0; i < 130; ++i) {
        const float a = i * 12.9898f;
        const float b = i * 78.233f;
        const float rx = std::sin(a) * 0.5f + 0.5f;
        const float ry = std::sin(b) * 0.5f + 0.5f;
        const float cluster = std::pow(std::sin(i * 4.17f) * 0.5f + 0.5f, 1.8f);

        Star star;
        star.pos = QPointF(w * (0.18f + rx * 0.68f),
                           h * (0.14f + ry * 0.70f));
        star.radius = 0.45f + cluster * 1.45f;
        star.brightness = 0.25f + (std::sin(i * 6.71f) * 0.5f + 0.5f) * 0.75f;
        star.twinkle = i * 0.37f;
        m_stars.append(star);
    }
}

void CaveRenderer::render(QPainter *painter, const Frame &frame) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->fillRect(QRectF(QPointF(0, 0), frame.logicalSize), QColor(2, 4, 10));

    drawSpace(painter, frame);
    drawCave(painter, frame);
    drawFloorGlow(painter, frame);

    painter->restore();
}

void CaveRenderer::drawSpace(QPainter *painter, const Frame &frame) const
{
    const QPointF vp = frame.vanishingPoint;
    const bool enclosed = frame.mode == Mode::EnclosedTunnel;

    QRadialGradient deepSpace(vp.x() + 30.f, vp.y() - 45.f, enclosed ? 240.f : 360.f);
    deepSpace.setColorAt(0.00, enclosed ? QColor(6, 10, 15, 255) : QColor(15, 28, 42, 220));
    deepSpace.setColorAt(0.42, enclosed ? QColor(3, 6, 10, 255) : QColor(5, 12, 22, 210));
    deepSpace.setColorAt(1.00, QColor(1, 2, 6, 255));
    painter->setPen(Qt::NoPen);
    painter->setBrush(deepSpace);
    painter->drawRect(QRectF(QPointF(0, 0), frame.logicalSize));

    if (!enclosed) {
        QRadialGradient aurora(vp.x() - 20.f, vp.y() - 95.f, 330.f);
        aurora.setColorAt(0.00, QColor(45, 115, 135, 58));
        aurora.setColorAt(0.32, QColor(20, 75, 115, 36));
        aurora.setColorAt(0.62, QColor(55, 22, 95, 24));
        aurora.setColorAt(1.00, QColor(0, 0, 0, 0));
        painter->setBrush(aurora);
        painter->drawEllipse(QPointF(vp.x() - 20.f, vp.y() - 95.f), 360.f, 230.f);

        for (const Star &star : m_stars) {
            const float driftX = std::sin(frame.time * 0.08f + star.twinkle) * 8.f;
            const float driftY = std::cos(frame.time * 0.05f + star.twinkle) * 4.f;
            const float twinkle = 0.75f + std::sin(frame.time * 1.4f + star.twinkle) * 0.25f;
            const int alpha = qBound(35, static_cast<int>(star.brightness * twinkle * 220.f), 230);
            painter->setBrush(QColor(220, 232, 255, alpha));
            painter->drawEllipse(star.pos + QPointF(driftX, driftY), star.radius, star.radius);
        }

        const QPointF polaris(vp.x() + 26.f, vp.y() - 108.f);
        QRadialGradient polarisGlow(polaris, 22.f);
        polarisGlow.setColorAt(0.0, QColor(230, 248, 255, 175));
        polarisGlow.setColorAt(0.3, QColor(110, 190, 220, 70));
        polarisGlow.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter->setBrush(polarisGlow);
        painter->drawEllipse(polaris, 22.f, 22.f);
        painter->setBrush(QColor(240, 250, 255, 245));
        painter->drawEllipse(polaris, 2.4f, 2.4f);
    }
}

void CaveRenderer::drawCave(QPainter *painter, const Frame &frame) const
{
    const QPointF vp       = frame.vanishingPoint;
    const bool    enclosed = frame.mode == Mode::EnclosedTunnel;

    // Projection constants — must match GameScene::FOCAL and TunnelPath world scale.
    constexpr float FOCAL        = 400.f;
    constexpr float TUNNEL_R     = 155.f;   // world-space tunnel radius
    constexpr float ASPECT_Y     = 0.62f;   // vertical compression (cave is wider than tall)
    constexpr float RING_SPACING = 80.f;    // world units between ring cross-sections
    constexpr int   NUM_RINGS    = 22;      // rings to place ahead of the camera
    constexpr float NEAR_CLIP    = 1.f;     // only skip rings at/behind the camera

    // -----------------------------------------------------------------------
    // 1. Build streaming ring descriptors.
    //
    // scrollPhase maps playerZ into [0, RING_SPACING) so rings scroll
    // continuously without snapping.  rings[0] is the nearest valid ring,
    // rings[last] is the farthest.
    // -----------------------------------------------------------------------
    struct RingDesc {
        float relZ;      // depth from camera
        float worldZ;    // absolute world z (pins the shape to the cave geometry)
        float halfW;
        float halfH;
        float phase;     // shape seed — tied to worldZ for stable cave geometry
        float brightness;
        float roughness;
    };

    const float scrollPhase = std::fmod(frame.playerZ, RING_SPACING);
    const float firstRelZ   = RING_SPACING - scrollPhase;

    QList<RingDesc> rings;
    rings.reserve(NUM_RINGS);

    for (int i = 0; i < NUM_RINGS; ++i) {
        const float relZ   = firstRelZ + i * RING_SPACING;
        if (relZ < NEAR_CLIP) continue;

        const float worldZ = frame.playerZ + relZ;
        const float halfW  = TUNNEL_R * FOCAL / relZ;
        if (halfW < 4.f) continue;

        const float depth01 = relZ / (RING_SPACING * NUM_RINGS);
        // Enclosed mode: far rings fade to near-black quickly so the cave feels
        // like infinite darkness ahead, not a visible tube.
        const float brightnessExp = enclosed ? 2.8f : 1.1f;
        const float brightness = std::pow(1.f - std::min(depth01, 1.f), brightnessExp);
        // Near rings get more roughness for dramatic cave wall detail.
        const float roughness  = 0.24f - depth01 * 0.10f;
        // Phase: world-z based so the same cave "slice" looks consistent as you approach,
        // plus a tiny time drift so the walls feel alive.
        const float phase = worldZ * 0.015f + frame.time * 0.03f;

        rings.append({relZ, worldZ, halfW, halfW * ASPECT_Y, phase, brightness, roughness});
    }

    if (rings.isEmpty()) return;

    // -----------------------------------------------------------------------
    // 2. Wall mask — fill the screen with cave rock, punch out the tunnel.
    //
    // Use rings[0] (the nearest streaming ring) so the mask and the facets
    // always agree.  When rings[0] is very large (ring passing the camera)
    // the mask punch-out covers the whole screen = you are inside the tube,
    // which is the correct look.
    // -----------------------------------------------------------------------
    const RingDesc &maskRing = rings[0];
    // Cap the mask ring so the OddEvenFill polygon stays screen-sized.
    // When the nearest ring is larger than the screen, the full screen is
    // "inside the tunnel" — painting no outer rock, which is correct.
    constexpr float MASK_CAP = 1200.f;
    const QList<QPointF> maskPoints = caveRing(vp,
        std::min(maskRing.halfW, MASK_CAP),
        std::min(maskRing.halfH, MASK_CAP * ASPECT_Y),
        maskRing.phase, enclosed ? 0.26f : 0.20f);

    QPainterPath wallMask;
    wallMask.addRect(QRectF(QPointF(0, 0), frame.logicalSize));
    wallMask.addPath(polygonPath(maskPoints));
    wallMask.setFillRule(Qt::OddEvenFill);

    painter->setPen(Qt::NoPen);
    painter->fillPath(wallMask, QColor(4, 5, 9));

    QRadialGradient edgeShade(vp, 820.f);
    edgeShade.setColorAt(0.00, QColor(0, 0, 0,   0));
    edgeShade.setColorAt(0.50, QColor(0, 0, 0,   8));
    edgeShade.setColorAt(0.78, QColor(0, 0, 0,  90));
    edgeShade.setColorAt(1.00, QColor(0, 0, 0, 185));
    painter->fillPath(wallMask, edgeShade);

    // -----------------------------------------------------------------------
    // 3. Facet bands — draw from far to near (painter's algorithm).
    //
    // rings[r] is nearer (larger) than rings[r+1] (farther/smaller).
    // Each band's outer boundary = rings[r], inner boundary = rings[r+1].
    // -----------------------------------------------------------------------
    for (int r = rings.size() - 2; r >= 0; --r) {
        const RingDesc &outerRing = rings[r];       // nearer, larger
        const RingDesc &innerRing = rings[r + 1];   // farther, smaller

        // Qt clips off-screen geometry at the canvas boundary — no need to cap.
        const QList<QPointF> outerPts = caveRing(vp,
            outerRing.halfW, outerRing.halfH,
            outerRing.phase, outerRing.roughness);
        const QList<QPointF> innerPts = caveRing(vp,
            innerRing.halfW, innerRing.halfH,
            innerRing.phase, innerRing.roughness);

        const int n = std::min(outerPts.size(), innerPts.size());
        for (int i = 0; i < n; ++i) {
            const int next = (i + 1) % n;

            QPainterPath facet;
            facet.moveTo(outerPts[i]);
            facet.lineTo(outerPts[next]);
            facet.lineTo(innerPts[next]);
            facet.lineTo(innerPts[i]);
            facet.closeSubpath();

            const float avgBright = 0.5f * (outerRing.brightness + innerRing.brightness);
            const float side = std::sin(i * 1.73f + innerRing.phase * 0.9f) * 0.5f + 0.5f;
            const int   base = static_cast<int>(6.f + avgBright * 20.f + side * 10.f);
            const QColor rock(base / 2, base / 2 + 2, base + 8, 240);
            const QColor edge(15, 35 + static_cast<int>(side * 20.f + avgBright * 18.f), 52, 72);

            QLinearGradient grad(outerPts[i], innerPts[next]);
            grad.setColorAt(0.0, rock.darker(148));
            grad.setColorAt(0.55, rock);
            grad.setColorAt(1.0, rock.darker(188));

            painter->setBrush(grad);
            painter->setPen(QPen(edge, 0.7f));
            painter->drawPath(facet);
        }
    }

    // -----------------------------------------------------------------------
    // 4. Far end cap — fill the innermost (farthest) ring with deep darkness.
    // -----------------------------------------------------------------------
    const RingDesc &farEnd = rings.last();
    const QList<QPointF> endPts = caveRing(vp, farEnd.halfW, farEnd.halfH,
                                            farEnd.phase, farEnd.roughness);
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(2, 3, 7, 228));
    painter->drawPath(polygonPath(endPts));

    // -----------------------------------------------------------------------
    // 5. Depth fog — seals the far end of the tunnel.
    //    Enclosed (gameplay): solid black core fading out, so it feels like
    //    infinite dark cave ahead rather than a visible tube.
    //    Open mouth (intro/attract): faint — lets stars show through.
    // -----------------------------------------------------------------------
    if (enclosed) {
        // Large solid-centre fog: covers most of the visible ring area so
        // only the nearest, brightest wall sections are visible.
        QRadialGradient fog(vp, 420.f);
        fog.setColorAt(0.00, QColor(1, 2, 4, 255));
        fog.setColorAt(0.25, QColor(1, 2, 5, 252));
        fog.setColorAt(0.55, QColor(2, 3, 7, 200));
        fog.setColorAt(0.80, QColor(2, 4, 8,  90));
        fog.setColorAt(1.00, QColor(0, 0, 0,   0));
        painter->setPen(Qt::NoPen);
        painter->setBrush(fog);
        painter->drawEllipse(vp, 420.f, 260.f);
    } else {
        // Open-mouth mode: subtle haze only, stars visible through centre
        QRadialGradient haze(vp, 200.f);
        haze.setColorAt(0.0, QColor(2, 4, 8, 80));
        haze.setColorAt(1.0, QColor(0, 0, 0,  0));
        painter->setPen(Qt::NoPen);
        painter->setBrush(haze);
        painter->drawEllipse(vp, 200.f, 120.f);
    }

    // -----------------------------------------------------------------------
    // 6. Turn-occlusion cap (unchanged — blocks the far exit on curves).
    // -----------------------------------------------------------------------
    if (frame.turnOcclusion > 0.02f || enclosed) {
        QPointF turnDir = vp - QPointF(640.f, 360.f);
        const float len = std::hypot(turnDir.x(), turnDir.y());
        if (len > 0.001f)
            turnDir /= len;
        else
            turnDir = QPointF(std::sin(frame.time * 0.4f), std::cos(frame.time * 0.33f));

        const float occ = enclosed ? qMax(0.72f, frame.turnOcclusion) : frame.turnOcclusion;
        const QPointF capCenter = vp + turnDir * (38.f + occ * 74.f);
        QRadialGradient cap(capCenter, 78.f + occ * 125.f);
        cap.setColorAt(0.0, QColor(3, 5, 9, static_cast<int>(190 + occ * 55.f)));
        cap.setColorAt(0.55, QColor(8, 12, 18, static_cast<int>(130 + occ * 75.f)));
        cap.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter->setPen(Qt::NoPen);
        painter->setBrush(cap);
        painter->drawEllipse(capCenter, 92.f + occ * 190.f, 58.f + occ * 128.f);
    }
}

void CaveRenderer::drawFloorGlow(QPainter *painter, const Frame &frame) const
{
    const QPointF vp = frame.vanishingPoint;
    const float speedBoost = std::min(frame.worldSpeed / 640.f, 1.f);

    QRadialGradient floorGlow(vp.x(), vp.y() + 245.f, 240.f);
    floorGlow.setColorAt(0.00, QColor(20, 225, 190, static_cast<int>(70 + speedBoost * 45)));
    floorGlow.setColorAt(0.24, QColor(20, 120, 135, 42));
    floorGlow.setColorAt(0.62, QColor(20, 55, 75, 20));
    floorGlow.setColorAt(1.00, QColor(0, 0, 0, 0));
    painter->setPen(Qt::NoPen);
    painter->setBrush(floorGlow);
    painter->drawEllipse(QPointF(vp.x(), vp.y() + 245.f), 250.f, 95.f);

    QLinearGradient floorPlane(0.f, vp.y() + 180.f, 0.f, 720.f);
    floorPlane.setColorAt(0.0, QColor(0, 0, 0, 0));
    floorPlane.setColorAt(0.55, QColor(12, 26, 35, 105));
    floorPlane.setColorAt(1.0, QColor(2, 3, 8, 185));
    painter->setBrush(floorPlane);
    painter->drawRect(QRectF(0.f, vp.y() + 160.f, 1280.f, 560.f));
}

QList<QPointF> CaveRenderer::caveRing(const QPointF &vp, float halfW, float halfH,
                                      float phase, float roughness)
{
    QList<QPointF> points;
    points.reserve(18);

    constexpr int count = 18;
    for (int i = 0; i < count; ++i) {
        const float a = static_cast<float>(i) / static_cast<float>(count) * 2.f * static_cast<float>(M_PI);
        const float n1 = std::sin(a * 3.0f + phase * 1.7f);
        const float n2 = std::sin(a * 7.0f - phase * 0.9f);
        const float radial = 1.f + (n1 * 0.65f + n2 * 0.35f) * roughness;
        const float x = vp.x() + std::cos(a) * halfW * radial;
        const float y = vp.y() + std::sin(a) * halfH * radial;
        points.append(QPointF(x, y));
    }

    return points;
}

QPainterPath CaveRenderer::polygonPath(const QList<QPointF> &points)
{
    QPainterPath path;
    if (points.isEmpty())
        return path;

    path.moveTo(points.first());
    for (int i = 1; i < points.size(); ++i)
        path.lineTo(points[i]);
    path.closeSubpath();
    return path;
}
