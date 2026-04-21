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
    const QPointF vp = frame.vanishingPoint;
    const bool enclosed = frame.mode == Mode::EnclosedTunnel;
    const float difficulty = std::min(frame.survivalTime / 60.f, 1.f);
    const float breathing = std::sin(frame.time * 0.55f) * 6.f;
    const float nearW = (enclosed ? 455.f : 505.f) - difficulty * 42.f + breathing;
    const float nearH = (enclosed ? 258.f : 295.f) - difficulty * 28.f + breathing * 0.45f;

    const QList<QPointF> near = caveRing(vp, nearW, nearH, frame.time * 0.18f, enclosed ? 0.28f : 0.22f);

    QPainterPath wallMask;
    wallMask.addRect(QRectF(QPointF(0, 0), frame.logicalSize));
    wallMask.addPath(polygonPath(near));
    wallMask.setFillRule(Qt::OddEvenFill);

    painter->setPen(Qt::NoPen);
    painter->fillPath(wallMask, QColor(4, 5, 9));

    QRadialGradient edgeShade(vp, 820.f);
    edgeShade.setColorAt(0.00, QColor(0, 0, 0, 0));
    edgeShade.setColorAt(0.48, QColor(0, 0, 0, 10));
    edgeShade.setColorAt(0.78, QColor(0, 0, 0, 95));
    edgeShade.setColorAt(1.00, QColor(0, 0, 0, 190));
    painter->fillPath(wallMask, edgeShade);

    const float openScales[] = {1.00f, 0.78f, 0.58f, 0.42f, 0.29f, 0.19f, 0.11f};
    const float enclosedScales[] = {1.00f, 0.82f, 0.66f, 0.51f, 0.38f, 0.27f, 0.18f};
    constexpr int ringCount = 7;

    QList<QPointF> rings[ringCount];
    for (int i = 0; i < ringCount; ++i) {
        const float roughness = (enclosed ? 0.29f : 0.22f) - i * 0.016f;
        const float scale = enclosed ? enclosedScales[i] : openScales[i];
        const float swayX = enclosed ? std::sin(frame.time * 0.55f + i * 0.95f) * i * 7.0f : 0.f;
        const float swayY = enclosed ? std::cos(frame.time * 0.42f + i * 0.85f) * i * 4.6f : 0.f;
        const QPointF ringVp = vp + QPointF(swayX, swayY);
        rings[i] = caveRing(ringVp, nearW * scale, nearH * scale,
                            frame.time * 0.18f + i * 0.63f,
                            std::max(0.10f, roughness));
    }

    for (int r = ringCount - 2; r >= 0; --r) {
        const QList<QPointF> &outer = rings[r];
        const QList<QPointF> &inner = rings[r + 1];
        const int n = std::min(outer.size(), inner.size());
        for (int i = 0; i < n; ++i) {
            const int next = (i + 1) % n;
            QPainterPath facet;
            facet.moveTo(outer[i]);
            facet.lineTo(outer[next]);
            facet.lineTo(inner[next]);
            facet.lineTo(inner[i]);
            facet.closeSubpath();

            const float depth = static_cast<float>(r) / static_cast<float>(ringCount - 1);
            const float side = std::sin(i * 1.73f + r * 0.8f) * 0.5f + 0.5f;
            const int base = static_cast<int>(9.f + depth * 13.f + side * 8.f);
            const QColor rock(base / 2, base / 2 + 2, base + 7, 238);
            const QColor edge(18, 42 + static_cast<int>(side * 22.f), 58, 82);

            QLinearGradient grad(outer[i], inner[next]);
            grad.setColorAt(0.0, rock.darker(150));
            grad.setColorAt(0.55, rock);
            grad.setColorAt(1.0, rock.darker(190));

            painter->setBrush(grad);
            painter->setPen(QPen(edge, 0.8f));
            painter->drawPath(facet);
        }
    }

    QPainterPath opening = polygonPath(rings[ringCount - 1]);
    painter->setBrush(Qt::NoBrush);
    const float baseOpening = enclosed ? 22.f : 70.f;
    const int openingAlpha = static_cast<int>(baseOpening * (1.f - frame.turnOcclusion * 0.75f));
    painter->setPen(QPen(QColor(50, 150, 145, openingAlpha), 1.2f));
    painter->drawPath(opening);

    if (enclosed) {
        QRadialGradient sealed(vp, 260.f);
        sealed.setColorAt(0.0, QColor(4, 7, 11, 235));
        sealed.setColorAt(0.46, QColor(2, 4, 8, 215));
        sealed.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter->setPen(Qt::NoPen);
        painter->setBrush(sealed);
        painter->drawEllipse(vp, 235.f, 138.f);
    }

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
