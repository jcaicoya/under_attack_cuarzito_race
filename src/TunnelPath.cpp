#include "TunnelPath.h"

#include <QFile>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLoggingCategory>
#include <QtMath>
#include <cmath>

Q_LOGGING_CATEGORY(trackLog, "cuarzito.track")

namespace {
constexpr float kDefaultSegmentLength = 200.f;
constexpr float kDefaultTurnAngleDegrees = 30.f;

float curvatureFor(float angleDegrees, float length)
{
    if (length <= 0.f)
        return 0.f;
    return qDegreesToRadians(angleDegrees) / length;
}
}

// ---------------------------------------------------------------------------
// Constructor — load the resource track, then precompute segment boundaries
// ---------------------------------------------------------------------------
TunnelPath::TunnelPath(const QString &resourcePath)
{
    TrackData data = loadTrack(resourcePath);
    if (data.segments.isEmpty()) {
        qCWarning(trackLog) << "Using fallback tunnel track";
        data = fallbackTrack();
    }

    m_gemConfigs = data.gems;
    precompute(data.segments);
}

TunnelPath::TrackData TunnelPath::loadTrack(const QString &resourcePath) const
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(trackLog) << "Could not open track resource" << resourcePath << file.errorString();
        return {};
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qCWarning(trackLog) << "Invalid track JSON" << resourcePath << parseError.errorString();
        return {};
    }

    TrackData data;
    const QJsonObject root = doc.object();
    const QJsonArray segmentArray = root.value(QStringLiteral("segments")).toArray();
    if (segmentArray.isEmpty()) {
        qCWarning(trackLog) << "Track has no segments" << resourcePath;
        return {};
    }

    data.segments.reserve(segmentArray.size());

    for (int i = 0; i < segmentArray.size(); ++i) {
        const QJsonObject obj = segmentArray.at(i).toObject();
        const QString type = obj.value(QStringLiteral("type")).toString().toLower();
        const float length = static_cast<float>(
            obj.value(QStringLiteral("length")).toDouble(kDefaultSegmentLength));
        const bool isStraight = type == QStringLiteral("straight");
        const float defaultAngle = isStraight ? 0.f : kDefaultTurnAngleDegrees;
        const float angle = static_cast<float>(
            obj.value(QStringLiteral("angleDegrees")).toDouble(defaultAngle));

        if (length <= 0.f) {
            qCWarning(trackLog) << "Skipping segment with invalid length" << i << length;
            continue;
        }

        Segment segment;
        segment.length = length;

        if (isStraight) {
            // No curvature.
        } else if (type == QStringLiteral("left")) {
            segment.curvH = -curvatureFor(angle, length);
        } else if (type == QStringLiteral("right")) {
            segment.curvH = curvatureFor(angle, length);
        } else if (type == QStringLiteral("uphill")) {
            // World/path Y is "down" relative to the camera framing used by
            // gameplay and the mini-map, so uphill must bend toward negative Y.
            segment.curvV = -curvatureFor(angle, length);
        } else if (type == QStringLiteral("downhill")) {
            segment.curvV = curvatureFor(angle, length);
        } else {
            qCWarning(trackLog) << "Skipping segment with unknown type" << i << type;
            continue;
        }

        data.segments.append(segment);
    }

    const QJsonArray gemArray = root.value(QStringLiteral("gems")).toArray();
    data.gems.reserve(gemArray.size());
    for (int i = 0; i < gemArray.size(); ++i) {
        const QJsonObject obj = gemArray.at(i).toObject();
        GemConfig gem;
        gem.startZ = static_cast<float>(obj.value(QStringLiteral("startZ")).toDouble());
        gem.speed = static_cast<float>(obj.value(QStringLiteral("speed")).toDouble());
        if (gem.startZ <= 0.f || gem.speed <= 0.f) {
            qCWarning(trackLog) << "Skipping gem config with invalid values" << i
                                << gem.startZ << gem.speed;
            continue;
        }
        data.gems.append(gem);
    }

    return data;
}

TunnelPath::TrackData TunnelPath::fallbackTrack() const
{
    const float turn = curvatureFor(kDefaultTurnAngleDegrees, kDefaultSegmentLength);
    TrackData data;
    data.segments = {
        {kDefaultSegmentLength, 0.f, 0.f},
        {kDefaultSegmentLength, -turn, 0.f},
        {kDefaultSegmentLength, 0.f, 0.f},
        {kDefaultSegmentLength, turn, 0.f},
        {kDefaultSegmentLength, 0.f, 0.f},
        {kDefaultSegmentLength, -turn, 0.f},
        {kDefaultSegmentLength, 0.f, 0.f},
        {kDefaultSegmentLength, turn, 0.f},
        {kDefaultSegmentLength, 0.f, 0.f},
    };
    data.gems = {
        {380.f, 205.f},
        {680.f, 212.f},
        {1040.f, 219.f},
        {1460.f, 226.f},
    };
    return data;
}

void TunnelPath::precompute(const QVector<Segment> &segments)
{
    m_keyframes.clear();
    m_keyframes.reserve(segments.size());

    float z = 0.f, x = 0.f, y = 0.f, aH = 0.f, aV = 0.f;

    for (const Segment &segment : segments) {
        Keyframe kf;
        kf.zStart = z;
        kf.x      = x;
        kf.y      = y;
        kf.angleH = aH;
        kf.angleV = aV;
        kf.curvH  = segment.curvH;
        kf.curvV  = segment.curvV;
        kf.length = segment.length;
        m_keyframes.append(kf);

        // Advance state to end of segment using exact arc integration
        const float L  = segment.length;
        const float cH = segment.curvH;
        const float cV = segment.curvV;

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

    result.radius      = 160.f;
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
