#include "TunnelPath.h"

#include <QtMath>

TunnelPath::Sample TunnelPath::sample(float z) const
{
    constexpr float derivativeStep = 4.f;

    Sample result;
    result.center = centerAt(z);

    const QPointF before = centerAt(z - derivativeStep);
    const QPointF after = centerAt(z + derivativeStep);
    QPointF tangent = after - before;
    const float length = qSqrt(tangent.x() * tangent.x() + tangent.y() * tangent.y());
    if (length > 0.001f)
        tangent /= length;
    result.tangent = tangent;

    result.radius = radiusAt(z);
    result.innerRadius = result.radius * 0.74f;

    const QPointF nearCenter = centerAt(z + 90.f);
    const QPointF farCenter = centerAt(z + 430.f);
    const QPointF bend = farCenter - nearCenter;
    const float bendAmount = qSqrt(bend.x() * bend.x() + bend.y() * bend.y());
    result.occlusion = qBound(0.f, (bendAmount - 95.f) / 135.f, 1.f);
    return result;
}

QPointF TunnelPath::gemOffset(int gemIndex, float z) const
{
    const float phase = static_cast<float>(gemIndex) * 1.75f;
    const float radius = 28.f + static_cast<float>(gemIndex) * 9.f;
    const float x = qSin(z * 0.011f + phase) * radius;
    const float y = qCos(z * 0.008f + phase * 0.7f) * radius * 0.72f;
    return QPointF(x, y);
}

QPointF TunnelPath::centerAt(float z)
{
    const float x = qSin(z * 0.0084f) * 132.f
                  + qSin(z * 0.0031f + 1.4f) * 78.f
                  + qSin(z * 0.0170f + 0.8f) * 28.f;
    const float y = qCos(z * 0.0072f + 0.5f) * 92.f
                  + qSin(z * 0.0026f + 2.1f) * 58.f
                  + qCos(z * 0.0155f) * 24.f;
    return QPointF(x, y);
}

float TunnelPath::radiusAt(float z)
{
    const float radius = 166.f
                       + qSin(z * 0.0040f + 0.9f) * 18.f
                       + qCos(z * 0.0095f) * 10.f;
    return qBound(132.f, radius, 194.f);
}
