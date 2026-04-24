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
    drawWallProximity(painter, frame);

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

    if (enclosed) {
        struct BoxRing {
            float relZ;
            float worldZ;
            float half;
            float depth01;
            QPointF center;
            QList<QPointF> pts;
            bool screenCover;
        };

        auto squareRing = [](const QPointF &center, float half) {
            return QList<QPointF>{
                QPointF(center.x() - half, center.y() - half),
                QPointF(center.x() + half, center.y() - half),
                QPointF(center.x() + half, center.y() + half),
                QPointF(center.x() - half, center.y() + half),
            };
        };

        auto bandPath = [](const QList<QPointF> &outerPts,
                           const QList<QPointF> &innerPts,
                           int side) {
            const int next = (side + 1) % 4;
            QPainterPath path;
            path.moveTo(outerPts[side]);
            path.lineTo(outerPts[next]);
            path.lineTo(innerPts[next]);
            path.lineTo(innerPts[side]);
            path.closeSubpath();
            return path;
        };

        auto screenCoverSquare = [](const QSizeF &logicalSize) {
            constexpr float margin = 260.f;
            return QList<QPointF>{
                QPointF(-margin, -margin),
                QPointF(static_cast<float>(logicalSize.width()) + margin, -margin),
                QPointF(static_cast<float>(logicalSize.width()) + margin,
                        static_cast<float>(logicalSize.height()) + margin),
                QPointF(-margin, static_cast<float>(logicalSize.height()) + margin),
            };
        };

        const QPointF worldCenter(640.f, 360.f);
        const QPointF frontCenter = vp;
        const QPointF farShift = vp - worldCenter;
        const float vOccSigned = frame.verticalOcclusion;
        const float vOcc = std::abs(vOccSigned);
        const float vDir = vOccSigned < 0.f ? -1.f : 1.f;
        const float scrollPhase = std::fmod(frame.playerZ, RING_SPACING);
        const float firstRelZ = RING_SPACING - scrollPhase;

        constexpr int EXTRA_BOXES = 10;
        constexpr int TOTAL_BOXES = NUM_RINGS + EXTRA_BOXES;

        QList<BoxRing> boxes;
        boxes.reserve(TOTAL_BOXES + 1);
        boxes.append({0.f, frame.playerZ, 0.f, 0.f, frontCenter,
                      screenCoverSquare(frame.logicalSize), true});

        for (int i = 0; i < TOTAL_BOXES; ++i) {
            const float relZ = firstRelZ + i * RING_SPACING;
            if (relZ < NEAR_CLIP) continue;

            const float depth01 = qBound(0.f, relZ / (RING_SPACING * TOTAL_BOXES), 1.f);
            const float curveT = std::pow(depth01, 0.72f);
            QPointF center = frontCenter + farShift * curveT;
            float half = TUNNEL_R * FOCAL / relZ;
            if (half < 2.5f) continue;

            const float farWeight = std::pow(depth01, 1.35f);
            center.ry() += vDir * farWeight * (vOcc * 86.f);
            half *= 1.f - farWeight * (vOcc * 0.24f);

            boxes.append({relZ, frame.playerZ + relZ, half, depth01,
                          center, squareRing(center, half), false});
        }

        if (boxes.size() < 2) return;

        painter->fillRect(QRectF(QPointF(0, 0), frame.logicalSize), QColor(3, 5, 10));

        for (int r = boxes.size() - 2; r >= 0; --r) {
            const BoxRing &outer = boxes[r];
            const BoxRing &inner = boxes[r + 1];
            const float bright = std::pow(1.f - inner.depth01, 0.75f);
            const float pulse = std::sin(inner.worldZ * 0.018f) * 0.5f + 0.5f;

            const QColor sideWall(7 + static_cast<int>(bright * 16.f),
                                  13 + static_cast<int>(bright * 20.f),
                                  25 + static_cast<int>(bright * 30.f),
                                  246);
            const QColor ceiling(5 + static_cast<int>(bright * 11.f),
                                  8 + static_cast<int>(bright * 13.f),
                                  18 + static_cast<int>(bright * 22.f),
                                  250);
            const QColor floor(5 + static_cast<int>(bright * 10.f),
                               18 + static_cast<int>(bright * 25.f + pulse * 8.f),
                               26 + static_cast<int>(bright * 28.f + pulse * 10.f),
                               250);

            const QColor fills[4] = {ceiling, sideWall.lighter(112), floor, sideWall};
            for (int side = 0; side < 4; ++side) {
                QPainterPath panel = bandPath(outer.pts, inner.pts, side);
                QLinearGradient grad(outer.pts[side], inner.pts[(side + 1) % 4]);
                grad.setColorAt(0.0, fills[side].lighter(112));
                grad.setColorAt(0.62, fills[side]);
                grad.setColorAt(1.0, fills[side].darker(135));
                painter->setBrush(grad);
                painter->setPen(QPen(QColor(20, 92, 108, static_cast<int>(42 + bright * 80.f)), 1.0f));
                painter->drawPath(panel);
            }
        }

        const float turnCue = qBound(0.f,
            static_cast<float>(std::hypot(farShift.x() / 220.f, farShift.y() / 150.f)),
            1.f);

        const BoxRing &frontBox = boxes[1];
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(QColor(42, 126, 138, 58 + static_cast<int>(turnCue * 16.f)), 1.1f));
        painter->drawPath(polygonPath(frontBox.pts));

        return;
    }

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
        bool screenCover;
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
        // Flatter falloff so far rings stay readable — the player must be able
        // to see turns coming.  Open-mouth uses a slightly steeper drop-off
        // so the cave throat still frames the space view.
        const float brightnessExp = enclosed ? 0.85f : 1.1f;
        const float brightness = std::pow(1.f - std::min(depth01, 1.f), brightnessExp);
        // Near rings get more roughness for dramatic cave wall detail.
        const float roughness  = 0.24f - depth01 * 0.10f;
        // Phase: world-z based so the same cave "slice" looks consistent as you approach,
        // plus a tiny time drift so the walls feel alive.
        const float phase = worldZ * 0.015f + frame.time * 0.03f;

        rings.append({relZ, worldZ, halfW, halfW * ASPECT_Y, phase, brightness, roughness, false});
    }

    if (rings.isEmpty()) return;

    // Add an artificial ring at the camera plane. This is not an ellipse:
    // it is generated by casting rays to an expanded screen rectangle, so the
    // nearest wall band always covers every viewport corner.
    rings.prepend({0.f,
                   frame.playerZ,
                   0.f,
                   0.f,
                   frame.playerZ * 0.015f + frame.time * 0.03f,
                   1.f,
                   0.26f,
                   true});

    // -----------------------------------------------------------------------
    // 2. Wall mask — fill the screen with cave rock, punch out the tunnel.
    //
    // Use rings[0] (the nearest streaming ring) so the mask and the facets
    // always agree.  When rings[0] is very large (ring passing the camera)
    // the mask punch-out covers the whole screen = you are inside the tube,
    // which is the correct look.
    // -----------------------------------------------------------------------
    // Use the first real tunnel ring for the mask. The artificial camera ring
    // covers the whole viewport, so using it here would erase the base wall
    // fill and expose black anywhere a facet has a subpixel/shape gap.
    const RingDesc &maskRing = rings.size() > 1 ? rings[1] : rings[0];
    const QList<QPointF> maskPoints = caveRing(vp,
        maskRing.halfW,
        maskRing.halfH,
        maskRing.phase, enclosed ? 0.26f : 0.20f);

    QPainterPath wallMask;
    wallMask.addRect(QRectF(QPointF(0, 0), frame.logicalSize));
    wallMask.addPath(polygonPath(maskPoints));
    wallMask.setFillRule(Qt::OddEvenFill);

    painter->setPen(Qt::NoPen);
    if (enclosed) {
        // Defensive backing layer for full-screen/ultrawide scaling. The
        // streaming wall facets should cover the viewport, but tiny misses at
        // the near camera plane must fall back to cave rock, not black space.
        painter->fillRect(QRectF(QPointF(0, 0), frame.logicalSize), QColor(4, 5, 9));
    }
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
        const QList<QPointF> outerPts = outerRing.screenCover
            ? screenCoverRing(vp, frame.logicalSize)
            : caveRing(vp, outerRing.halfW, outerRing.halfH,
                       outerRing.phase, outerRing.roughness);
        const QList<QPointF> innerPts = innerRing.screenCover
            ? screenCoverRing(vp, frame.logicalSize)
            : caveRing(vp, innerRing.halfW, innerRing.halfH,
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
            // Higher base and multiplier so far facets are visibly textured rock,
            // not just near-black smears.
            const int   base = static_cast<int>(8.f + avgBright * 28.f + side * 12.f);
            const QColor rock(base / 2, base / 2 + 2, base + 10, 240);
            const QColor edge(15, 38 + static_cast<int>(side * 22.f + avgBright * 20.f), 55, 80);

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
    // 3b. Route-reading structure.
    //     The facets make the cave feel rocky; these thin contour/ridge lines
    //     make the tunnel direction readable at speed, especially in curves.
    // -----------------------------------------------------------------------
    if (enclosed && rings.size() > 4) {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        const float vpOffX = static_cast<float>(vp.x()) - 640.f;
        const float vpOffY = static_cast<float>(vp.y()) - 360.f;
        const float turnCue = qBound(0.f,
            static_cast<float>(std::hypot(vpOffX / 220.f, vpOffY / 150.f)),
            1.f);

        for (int r = 2; r < rings.size(); r += 3) {
            const RingDesc &ring = rings[r];
            const QList<QPointF> pts = caveRing(vp, ring.halfW, ring.halfH,
                                                ring.phase, ring.roughness);
            const float depth01 = qBound(0.f, ring.relZ / (RING_SPACING * NUM_RINGS), 1.f);
            const int alpha = static_cast<int>((1.f - depth01) * 46.f + turnCue * 22.f);
            painter->setPen(QPen(QColor(55, 150, 165, alpha), 1.0f));
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(polygonPath(pts));
        }

        constexpr int RIDGE_COUNT = 6;
        constexpr int SAMPLE_COUNT = 18;
        for (int ridge = 0; ridge < RIDGE_COUNT; ++ridge) {
            const int idx = (ridge * SAMPLE_COUNT) / RIDGE_COUNT;
            QPainterPath ridgePath;
            bool started = false;

            for (int r = rings.size() - 1; r >= 1; --r) {
                const RingDesc &ring = rings[r];
                const QList<QPointF> pts = caveRing(vp, ring.halfW, ring.halfH,
                                                    ring.phase, ring.roughness);
                if (pts.isEmpty()) continue;

                const QPointF p = pts[idx % pts.size()];
                if (!started) {
                    ridgePath.moveTo(p);
                    started = true;
                } else {
                    ridgePath.lineTo(p);
                }
            }

            const float angle = static_cast<float>(ridge) / static_cast<float>(RIDGE_COUNT)
                * 2.f * static_cast<float>(M_PI);
            const float wallFacing = std::max(0.f, static_cast<float>(
                std::cos(angle) * (vpOffX / 240.f) + std::sin(angle) * (vpOffY / 170.f)));
            const int alpha = static_cast<int>(22.f + turnCue * 18.f + wallFacing * 38.f);
            const QColor ridgeColor = wallFacing > 0.08f
                ? QColor(210, 125, 35, alpha)
                : QColor(45, 130, 150, alpha);

            painter->setPen(QPen(ridgeColor, wallFacing > 0.08f ? 1.5f : 1.0f));
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(ridgePath);
        }

        painter->restore();
    }

    // -----------------------------------------------------------------------
    // 4. Far end cap — fills the farthest ring so stars never bleed through.
    //    Kept opaque enough to prevent open-space bleed but slightly translucent
    //    so it doesn't read as a hard wall.
    // -----------------------------------------------------------------------
    const RingDesc &farEnd = rings.last();
    const float horizonScale = enclosed ? 0.52f : 1.f;
    const QList<QPointF> endPts = caveRing(vp,
                                            farEnd.halfW * horizonScale,
                                            farEnd.halfH * horizonScale,
                                            farEnd.phase,
                                            farEnd.roughness + 0.04f);
    painter->setPen(Qt::NoPen);
    painter->setBrush(enclosed ? QColor(1, 2, 5, 218) : QColor(2, 3, 7, 200));
    painter->drawPath(polygonPath(endPts));

    // Faint cave-ambient glow at the tunnel vanishing point — suggests depth
    // and "more cave ahead" without looking like an exit to open space.
    if (enclosed) {
        QRadialGradient ambient(vp, 42.f);
        ambient.setColorAt(0.0, QColor(18, 55, 80, 30));
        ambient.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter->setBrush(ambient);
        painter->drawEllipse(vp, 42.f, 28.f);
    }

    // -----------------------------------------------------------------------
    // 5. Depth fog — seals the far end of the tunnel.
    //    Enclosed (gameplay): light haze only, so upcoming tunnel bends remain
    //    visible enough to react to.
    //    Open mouth (intro/attract): faint — lets stars show through.
    // -----------------------------------------------------------------------
    if (enclosed) {
        // Lighter haze — just enough to give atmospheric depth without
        // burying the far-ring structure the player needs to read turns.
        QRadialGradient fog(vp, 340.f);
        fog.setColorAt(0.00, QColor(1, 2, 4,  45));
        fog.setColorAt(0.28, QColor(1, 2, 5,  36));
        fog.setColorAt(0.58, QColor(2, 3, 7,  20));
        fog.setColorAt(0.84, QColor(2, 4, 8,   8));
        fog.setColorAt(1.00, QColor(0, 0, 0,   0));
        painter->setPen(Qt::NoPen);
        painter->setBrush(fog);
        painter->drawEllipse(vp, 340.f, 210.f);
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
    QPointF turnDir = vp - QPointF(640.f, 360.f);
    const float turnDirLen = std::hypot(turnDir.x(), turnDir.y());
    if (turnDirLen > 0.001f)
        turnDir /= turnDirLen;
    else
        turnDir = QPointF(std::sin(frame.time * 0.4f), std::cos(frame.time * 0.33f));

    if (frame.turnOcclusion > 0.02f) {

        const float occ = frame.turnOcclusion;
        if (enclosed) {
            // On a bend the visible "horizon" should vanish behind the cave
            // wall. Draw an offset, elongated lobe instead of a centered hole.
            const QPointF capCenter = vp + turnDir * (72.f + occ * 128.f);
            const float angleDeg = std::atan2(turnDir.y(), turnDir.x()) * 180.f / static_cast<float>(M_PI);
            const float longR = 88.f + occ * 230.f;
            const float shortR = 34.f + occ * 78.f;

            painter->save();
            painter->translate(capCenter);
            painter->rotate(angleDeg);
            QRadialGradient cap(QPointF(0.f, 0.f), longR);
            cap.setColorAt(0.0, QColor(2, 4, 8, static_cast<int>(176 + occ * 46.f)));
            cap.setColorAt(0.48, QColor(5, 8, 13, static_cast<int>(112 + occ * 60.f)));
            cap.setColorAt(1.0, QColor(0, 0, 0, 0));
            painter->setPen(Qt::NoPen);
            painter->setBrush(cap);
            painter->drawEllipse(QPointF(0.f, 0.f), longR, shortR);
            painter->restore();

            const QPointF shoulder = vp + turnDir * (28.f + occ * 48.f);
            QRadialGradient shoulderShade(shoulder, 110.f + occ * 145.f);
            shoulderShade.setColorAt(0.0, QColor(1, 2, 5, static_cast<int>(58 + occ * 70.f)));
            shoulderShade.setColorAt(0.72, QColor(3, 5, 9, static_cast<int>(30 + occ * 46.f)));
            shoulderShade.setColorAt(1.0, QColor(0, 0, 0, 0));
            painter->setPen(Qt::NoPen);
            painter->setBrush(shoulderShade);
            painter->drawEllipse(shoulder, 104.f + occ * 120.f, 70.f + occ * 86.f);
        } else {
            const QPointF capCenter = vp + turnDir * (38.f + occ * 74.f);
            QRadialGradient cap(capCenter, 78.f + occ * 125.f);
            cap.setColorAt(0.0, QColor(3, 5, 9, static_cast<int>(145 + occ * 40.f)));
            cap.setColorAt(0.55, QColor(8, 12, 18, static_cast<int>(90 + occ * 55.f)));
            cap.setColorAt(1.0, QColor(0, 0, 0, 0));
            painter->setPen(Qt::NoPen);
            painter->setBrush(cap);
            painter->drawEllipse(capCenter, 92.f + occ * 190.f, 58.f + occ * 128.f);
        }
    }

    const float vOccSigned = frame.verticalOcclusion;
    const float vOcc = std::abs(vOccSigned);
    if (enclosed && vOcc > 0.02f) {
        const float dir = vOccSigned < 0.f ? -1.f : 1.f;
        const QPointF capCenter(vp.x(), vp.y() + dir * (6.f + vOcc * 18.f));
        const float wideR = 176.f + vOcc * 340.f;
        const float tallR = 78.f + vOcc * 168.f;

        QRadialGradient cap(capCenter, wideR);
        cap.setColorAt(0.0, QColor(2, 4, 8, static_cast<int>(188 + vOcc * 56.f)));
        cap.setColorAt(0.42, QColor(5, 8, 13, static_cast<int>(124 + vOcc * 68.f)));
        cap.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter->setPen(Qt::NoPen);
        painter->setBrush(cap);
        painter->drawEllipse(capCenter, wideR, tallR);

        const QPointF shoulder = capCenter + QPointF(0.f, dir * (12.f + vOcc * 24.f));
        QRadialGradient shoulderShade(shoulder, 168.f + vOcc * 210.f);
        shoulderShade.setColorAt(0.0, QColor(1, 2, 5, static_cast<int>(72 + vOcc * 80.f)));
        shoulderShade.setColorAt(0.72, QColor(3, 5, 9, static_cast<int>(38 + vOcc * 54.f)));
        shoulderShade.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter->setBrush(shoulderShade);
        painter->drawEllipse(shoulder, 188.f + vOcc * 220.f, 122.f + vOcc * 162.f);
    }

    // -----------------------------------------------------------------------
    // 7. Turn-direction wall highlight.
    //    The VP shifts left/right as the tunnel curves.  Use that displacement
    //    to tint the outer wall of the curve (the danger side) with a warm
    //    amber gradient, giving the player an instinctive directional cue.
    // -----------------------------------------------------------------------
    if (enclosed) {
        const float vpOffX = static_cast<float>(vp.x()) - 640.f;
        const float vpOffY = static_cast<float>(vp.y()) - 360.f;
        const float hAmt   = qBound(-1.f, vpOffX / 200.f, 1.f);
        const float vAmt   = qBound(-1.f, vpOffY / 140.f, 1.f);

        if (std::abs(hAmt) > 0.08f) {
            // Amber glow on the OUTER wall (opposite side from VP displacement)
            // VP moves right → tunnel bends right → outer wall is LEFT
            const float gx0 = hAmt > 0.f ? 0.f : 1280.f;
            const int   alpha = static_cast<int>(std::abs(hAmt) * 52.f);
            QLinearGradient hGrad(gx0, 360.f, 640.f, 360.f);
            hGrad.setColorAt(0.0, QColor(200, 120, 30, alpha));
            hGrad.setColorAt(1.0, QColor(0, 0, 0, 0));
            painter->fillPath(wallMask, QBrush(hGrad));
        }
        if (std::abs(vAmt) > 0.08f) {
            const float gy0 = vAmt > 0.f ? 0.f : 720.f;
            const int   alpha = static_cast<int>(std::abs(vAmt) * 42.f);
            QLinearGradient vGrad(640.f, gy0, 640.f, 360.f);
            vGrad.setColorAt(0.0, QColor(200, 120, 30, alpha));
            vGrad.setColorAt(1.0, QColor(0, 0, 0, 0));
            painter->fillPath(wallMask, QBrush(vGrad));
        }
    }
}

void CaveRenderer::drawFloorGlow(QPainter *painter, const Frame &frame) const
{
    const QPointF vp = frame.vanishingPoint;
    // speedBoost: 0 at min speed (135), 1 at ~500+
    const float speedBoost = std::min(std::max(0.f, (frame.worldSpeed - 135.f) / 365.f), 1.f);
    // yBoost: near-floor brightens the glow; near-ceiling dims it
    const float yBoost = frame.playerOffYNorm;   // -1 ceiling … +1 floor
    const float tunnelV = qBound(-1.f, static_cast<float>(vp.y() - 360.f) / 145.f, 1.f);

    const float baseAlpha  = 55.f + speedBoost * 95.f + yBoost * 55.f
                           + qMax(0.f, -tunnelV) * 34.f;
    const float glowRadius = 250.f + speedBoost * 60.f + std::max(0.f, yBoost) * 40.f;

    QRadialGradient floorGlow(vp.x(), vp.y() + 245.f, glowRadius);
    floorGlow.setColorAt(0.00, QColor(20, 230, 195, static_cast<int>(qBound(0.f, baseAlpha, 255.f))));
    floorGlow.setColorAt(0.24, QColor(20, 130, 145, static_cast<int>(qBound(0.f, baseAlpha * 0.46f, 255.f))));
    floorGlow.setColorAt(0.62, QColor(20, 60, 80,  static_cast<int>(qBound(0.f, baseAlpha * 0.20f, 255.f))));
    floorGlow.setColorAt(1.00, QColor(0, 0, 0, 0));
    painter->setPen(Qt::NoPen);
    painter->setBrush(floorGlow);
    painter->drawEllipse(QPointF(vp.x(), vp.y() + 245.f), glowRadius, 95.f + speedBoost * 25.f + std::max(0.f, yBoost) * 20.f);

    // Uphill: the floor should feel like it rises toward the player.
    const float uphillAmt = qMax(0.f, -tunnelV);
    if (uphillAmt > 0.001f) {
        QLinearGradient floorRise(0.f, vp.y() + 110.f, 0.f, 720.f);
        floorRise.setColorAt(0.0, QColor(0, 0, 0, 0));
        floorRise.setColorAt(0.38, QColor(18, 54, 66, static_cast<int>(uphillAmt * 52.f)));
        floorRise.setColorAt(0.78, QColor(8, 20, 28, static_cast<int>(uphillAmt * 108.f)));
        floorRise.setColorAt(1.0, QColor(2, 5, 10, static_cast<int>(uphillAmt * 156.f)));
        painter->setBrush(floorRise);
        painter->drawRect(QRectF(0.f, vp.y() + 92.f, 1280.f, 720.f - (vp.y() + 92.f)));
    }

    // Ceiling proximity vignette — dark gradient creeping down from the top
    // when Cuarzito is near the ceiling (playerOffYNorm strongly negative).
    const float ceilAmt = std::max(0.f, -frame.playerOffYNorm - 0.20f) / 0.80f;
    const float downhillAmt = qMax(0.f, tunnelV);
    if (ceilAmt > 0.001f) {
        QLinearGradient ceilGrad(0.f, 0.f, 0.f, 260.f);
        ceilGrad.setColorAt(0.0, QColor(8, 18, 32, static_cast<int>(ceilAmt * 160.f)));
        ceilGrad.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter->setBrush(ceilGrad);
        painter->drawRect(QRectF(0.f, 0.f, 1280.f, 260.f));
    }
    if (downhillAmt > 0.001f) {
        QLinearGradient ceilingDrop(0.f, 0.f, 0.f, 300.f + downhillAmt * 120.f);
        ceilingDrop.setColorAt(0.0, QColor(10, 24, 38, static_cast<int>(downhillAmt * 168.f)));
        ceilingDrop.setColorAt(0.38, QColor(8, 18, 28, static_cast<int>(downhillAmt * 110.f)));
        ceilingDrop.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter->setBrush(ceilingDrop);
        painter->drawRect(QRectF(0.f, 0.f, 1280.f, 320.f + downhillAmt * 120.f));
    }

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

QList<QPointF> CaveRenderer::screenCoverRing(const QPointF &vp, const QSizeF &logicalSize)
{
    QList<QPointF> points;
    points.reserve(18);

    constexpr int count = 18;
    constexpr float margin = 260.f;
    const float left = -margin;
    const float right = static_cast<float>(logicalSize.width()) + margin;
    const float top = -margin;
    const float bottom = static_cast<float>(logicalSize.height()) + margin;
    const float vpX = static_cast<float>(vp.x());
    const float vpY = static_cast<float>(vp.y());

    for (int i = 0; i < count; ++i) {
        const float a = static_cast<float>(i) / static_cast<float>(count) * 2.f * static_cast<float>(M_PI);
        const float dx = std::cos(a);
        const float dy = std::sin(a);
        float t = 100000.f;

        if (dx > 0.001f)
            t = std::min(t, (right - vpX) / dx);
        else if (dx < -0.001f)
            t = std::min(t, (left - vpX) / dx);

        if (dy > 0.001f)
            t = std::min(t, (bottom - vpY) / dy);
        else if (dy < -0.001f)
            t = std::min(t, (top - vpY) / dy);

        points.append(QPointF(vpX + dx * (t + margin),
                              vpY + dy * (t + margin)));
    }

    return points;
}

void CaveRenderer::drawWallProximity(QPainter *painter, const Frame &frame) const
{
    if (frame.mode != Mode::EnclosedTunnel) return;

    painter->save();
    painter->setPen(Qt::NoPen);

    const float xn = frame.playerOffXNorm;  // -1 left wall … +1 right wall
    const float yn = frame.playerOffYNorm;  // -1 ceiling  … +1 floor

    // For each wall: compute proximity (0–1 above threshold), then draw a
    // gradient band from the screen edge inward.  At contact (norm ≈ 1) the
    // band is bright amber-white; approaching it it's a dim cave-rock orange.
    constexpr float THRESHOLD  = 0.38f;   // start showing glow here
    constexpr float MAX_WIDTH  = 220.f;   // pixels from edge at full proximity
    const float     W          = static_cast<float>(frame.logicalSize.width());
    const float     H          = static_cast<float>(frame.logicalSize.height());

    auto edge = [&](float norm, bool horizontal, bool positiveEdge) {
        const float abs_n = std::abs(norm);
        const float prox  = std::max(0.f, abs_n - THRESHOLD) / (1.f - THRESHOLD);
        if (prox < 0.005f) return;

        const bool  contact   = abs_n > 0.92f;
        const float bandW     = prox * MAX_WIDTH;
        const int   peakAlpha = contact ? 210 : static_cast<int>(prox * 140.f);
        // Rock amber at distance, white-hot at contact
        const QColor peak = contact
            ? QColor(255, 210, 120, peakAlpha)
            : QColor(210, 110,  30, peakAlpha);

        QLinearGradient grad;
        QRectF rect;
        if (horizontal) {
            const float ex = positiveEdge ? W : 0.f;
            const float dx = positiveEdge ? -bandW : bandW;
            grad = QLinearGradient(ex, H * 0.5f, ex + dx, H * 0.5f);
            rect = positiveEdge ? QRectF(W - bandW, 0.f, bandW, H)
                                : QRectF(0.f, 0.f, bandW, H);
        } else {
            const float ey = positiveEdge ? H : 0.f;
            const float dy = positiveEdge ? -bandW : bandW;
            grad = QLinearGradient(W * 0.5f, ey, W * 0.5f, ey + dy);
            rect = positiveEdge ? QRectF(0.f, H - bandW, W, bandW)
                                : QRectF(0.f, 0.f, W, bandW);
        }
        grad.setColorAt(0.0, peak);
        grad.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter->setBrush(QBrush(grad));
        painter->drawRect(rect);
    };

    edge(xn,  true,  true);   // right wall
    edge(-xn, true,  false);  // left wall
    edge(yn,  false, true);   // floor
    edge(-yn, false, false);  // ceiling

    painter->restore();
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
