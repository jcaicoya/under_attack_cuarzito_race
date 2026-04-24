#pragma once

#include <QPointF>
#include <QString>
#include <QVector>

class TunnelPath {
public:
    struct Sample {
        QPointF center;
        QPointF tangent;
        float radius     = 160.f;
        float innerRadius = 118.f;
        float occlusion  = 0.f;
        float curvatureH = 0.f;   // horizontal curvature at this z (radians/unit)
        float curvatureV = 0.f;   // vertical curvature at this z
    };

    explicit TunnelPath(const QString &resourcePath = QStringLiteral(":/tracks/first_tunnel.json"));

    Sample  sample(float z) const;
    QPointF gemOffset(int gemIndex, float z) const;

    // Total defined track length in world units
    float totalLength() const { return m_totalLength; }

private:
    // Precomputed state at the start of each segment
    struct Keyframe {
        float zStart;
        float x, y;         // tunnel centre world position
        float angleH;       // horizontal heading (radians, 0 = straight ahead)
        float angleV;       // vertical heading
        float curvH;        // curvature for this segment
        float curvV;
        float length;
    };

    struct Segment {
        float length = 200.f;
        float curvH = 0.f;
        float curvV = 0.f;
    };

    QVector<Segment> loadTrack(const QString &resourcePath) const;
    QVector<Segment> fallbackTrack() const;
    void precompute(const QVector<Segment> &segments);

    QPointF centerAt(float z) const;
    float   radiusAt(float z) const;

    QVector<Keyframe> m_keyframes;
    float             m_totalLength = 0.f;
};
