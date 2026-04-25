#pragma once

#include <QList>
#include <QPainterPath>
#include <QPointF>
#include <QSizeF>

class QPainter;

class CaveRenderer {
public:
    enum class Mode {
        OpenMouth,
        EnclosedTunnel,
    };

    struct Frame {
        QSizeF logicalSize;
        QPointF vanishingPoint;
        float time = 0.f;
        float worldSpeed = 0.f;
        float turnOcclusion = 0.f;
        float verticalOcclusion = 0.f; // -1 uphill/top hide, +1 downhill/bottom hide
        float playerZ = 0.f;
        float playerOffYNorm = 0.f;  // -1 = near ceiling, +1 = near floor
        float playerOffXNorm = 0.f;  // -1 = near left wall, +1 = near right wall
        Mode mode = Mode::OpenMouth;
        bool showGuides = true;
    };

    CaveRenderer();

    void render(QPainter *painter, const Frame &frame) const;

private:
    struct Star {
        QPointF pos;
        float radius = 1.f;
        float brightness = 1.f;
        float twinkle = 0.f;
    };

    void drawSpace(QPainter *painter, const Frame &frame) const;
    void drawCave(QPainter *painter, const Frame &frame) const;
    void drawFloorGlow(QPainter *painter, const Frame &frame) const;
    void drawWallProximity(QPainter *painter, const Frame &frame) const;

    static QList<QPointF> caveRing(const QPointF &vp, float halfW, float halfH,
                                   float phase, float roughness);
    static QList<QPointF> screenCoverRing(const QPointF &vp, const QSizeF &logicalSize);
    static QPainterPath polygonPath(const QList<QPointF> &points);

    QList<Star> m_stars;
};
