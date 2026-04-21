#pragma once

#include <QPointF>

class TunnelPath {
public:
    struct Sample {
        QPointF center;
        QPointF tangent;
        float radius = 160.f;
        float innerRadius = 118.f;
        float occlusion = 0.f;
    };

    Sample sample(float z) const;
    QPointF gemOffset(int gemIndex, float z) const;

private:
    static QPointF centerAt(float z);
    static float radiusAt(float z);
};
