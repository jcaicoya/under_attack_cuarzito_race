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
        float survivalTime = 0.f;
        float worldSpeed = 0.f;
        float turnOcclusion = 0.f;
        Mode mode = Mode::OpenMouth;
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

    static QList<QPointF> caveRing(const QPointF &vp, float halfW, float halfH,
                                   float phase, float roughness);
    static QPainterPath polygonPath(const QList<QPointF> &points);

    QList<Star> m_stars;
};
