#pragma once

#include <QPointF>
#include <QString>
#include <QVector>

class TunnelPath {
public:
    struct GemConfig {
        float startZ = 0.f;
        float speed = 0.f;
    };

    struct Sample {
        QPointF center;
        QPointF tangent;
        float radius     = 0.f;
        float innerRadius = 0.f;
        float occlusion  = 0.f;
        float curvatureH = 0.f;   // horizontal curvature at this z (radians/unit)
        float curvatureV = 0.f;   // vertical curvature at this z
    };

    explicit TunnelPath(const QString &resourcePath = QStringLiteral(":/tracks/demo_tunnel.json"));

    Sample  sample(float z) const;
    QPointF gemOffset(int gemIndex, float z) const;
    QVector<GemConfig> gemConfigs() const { return m_gemConfigs; }

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

    struct TrackData {
        QVector<Segment> segments;
        QVector<GemConfig> gems;
    };

    TrackData loadTrack(const QString &resourcePath) const;
    TrackData fallbackTrack() const;
    void precompute(const QVector<Segment> &segments);

    QPointF centerAt(float z) const;

    QVector<Keyframe> m_keyframes;
    QVector<GemConfig> m_gemConfigs;
    float             m_totalLength = 0.f;
};
