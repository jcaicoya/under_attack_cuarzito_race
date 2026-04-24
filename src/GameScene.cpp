#define _USE_MATH_DEFINES
#include "GameScene.h"
#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <QFontMetricsF>
#include <QRandomGenerator>
#include <QRadialGradient>
#include <QLinearGradient>
#include <QCoreApplication>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
constexpr float kSafeXFactor = 1.28f;
constexpr float kSafeYFactor = 1.18f;
constexpr float kCameraLookAhead = 360.f;
constexpr float kCameraVerticalLookAhead = 220.f;
constexpr float kCameraVerticalGain = 0.48f;
constexpr float kCameraVpYBias = -12.f;
constexpr float kCameraVpYClamp = 82.f;
constexpr float kPhysicsLookAhead = 180.f;
constexpr float kPhysicsLeadScaleX = 160.f;
constexpr float kPhysicsLeadScaleY = 150.f;
constexpr float kPhysicsDriftGain = 1.45f;
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

GameScene::GameScene(const GameLaunchOptions &options, QObject *parent)
    : QObject(parent)
    , m_launchOptions(options)
    , m_tunnelPath(options.trackResource)
{
    m_trackOptions = {
        {QStringLiteral("DEMO TUNNEL"),
         QStringLiteral(":/tracks/demo_tunnel.json"),
         QStringLiteral("Short test route")},
        {QStringLiteral("LIVE TUNNEL"),
         QStringLiteral(":/tracks/live_tunnel.json"),
         QStringLiteral("Longer event route")},
    };

    if (!m_launchOptions.testMode) {
        for (int i = 0; i < m_trackOptions.size(); ++i) {
            if (m_trackOptions[i].resource == m_launchOptions.trackResource) {
                m_selectedTrackIndex = i;
                break;
            }
        }
        applySelectedTrack();
    }

    m_audio.startAmbient();
    if (m_launchOptions.testMode)
        startGame();
    else
        startAttract();
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

void GameScene::render(QPainter *painter)
{
    painter->setRenderHint(QPainter::Antialiasing);

    drawChaseGems(painter);
    if (m_state == GameState::Playing)
        drawSafeZone(painter);

    if (m_state != GameState::Attract && m_viewMode == ViewMode::ThirdPerson)
        drawPlayer(painter);

    drawBursts(painter);
    drawPopups(painter);
    drawImpactFlash(painter);
    if (m_state == GameState::Playing)
        drawMiniMap(painter);
    drawHUD(painter);
}

void GameScene::toggleViewMode()
{
    m_viewMode = (m_viewMode == ViewMode::ThirdPerson)
        ? ViewMode::FirstPerson
        : ViewMode::ThirdPerson;
}

void GameScene::toggleInvulnerability()
{
    m_invulnerable = !m_invulnerable;
    showDiagnostics(QString("Invulnerability: %1")
        .arg(m_invulnerable ? "ON" : "OFF"));
}

QPointF GameScene::cameraShakeOffset() const
{
    if (m_cameraShake <= 0.001f)
        return QPointF(0, 0);
    const float sx = std::sin(m_time * 53.f) * m_cameraShake * 10.f;
    const float sy = std::cos(m_time * 71.f) * m_cameraShake *  8.f;
    return QPointF(sx, sy);
}

float GameScene::playerLean() const
{
    if (m_input.isMovingLeft() == m_input.isMovingRight())
        return 0.f;
    return m_input.isMovingLeft() ? -1.f : 1.f;
}

QPointF GameScene::playerDrawCenter() const
{
    const float scale = FOCAL / PLAYER_DRAW_DEPTH;
    const float cx = m_vpX + m_player.offX * scale;
    const float anchorY = m_vpY + 30.f;
    const float rawCy = anchorY + m_player.offY * scale;

    const TunnelPath::Sample drawSample = m_tunnelPath.sample(m_player.z);
    const float projectedSafeY = qMax(drawSample.innerRadius * kSafeYFactor * scale, 24.f);
    float topLimit = qMax(72.f, anchorY - projectedSafeY + 12.f);
    float bottomLimit = qMin(SCENE_H - 72.f, anchorY + projectedSafeY - 12.f);

    if (topLimit > bottomLimit) {
        const float center = qBound(72.f, anchorY, SCENE_H - 72.f);
        topLimit = center;
        bottomLimit = center;
    }

    const float cy = qBound(topLimit, rawCy, bottomLimit);
    return QPointF(cx, cy);
}

float GameScene::testVerticalIntent() const
{
    if (!m_launchOptions.testMode)
        return 0.f;

    if (m_launchOptions.testName == QStringLiteral("downhill"))
        return 1.f;
    if (m_launchOptions.testName == QStringLiteral("uphill"))
        return -1.f;
    if (m_launchOptions.testName == QStringLiteral("vertical")) {
        const float phase = std::fmod(m_time, 4.f);
        if (phase < 1.f) return 0.f;
        if (phase < 2.f) return 1.f;
        if (phase < 3.f) return 0.f;
        return -1.f;
    }

    return 0.f;
}

QString GameScene::currentTestSegmentLabel() const
{
    const TunnelPath::Sample sample = m_tunnelPath.sample(m_player.z);
    const float h = sample.curvatureH;
    const float v = sample.curvatureV;
    constexpr float eps = 1e-4f;

    if (std::abs(h) <= eps && std::abs(v) <= eps)
        return QStringLiteral("straight");
    if (std::abs(v) > std::abs(h))
        return v < 0.f ? QStringLiteral("uphill") : QStringLiteral("downhill");
    return h > 0.f ? QStringLiteral("right") : QStringLiteral("left");
}

void GameScene::logTestTrace(bool force)
{
    if (!m_launchOptions.traceMode)
        return;

    const QString segmentLabel = currentTestSegmentLabel();
    const bool segmentChanged = segmentLabel != m_lastTestSegmentLabel;
    const bool wallChanged = m_player.wallContact != m_lastTestWallContact;
    const bool timerElapsed = m_testTraceTimer >= 1.f;

    if (!force && !segmentChanged && !wallChanged && !timerElapsed)
        return;

    const QPointF drawCenter = playerDrawCenter();
    const TunnelPath::Sample sample = m_tunnelPath.sample(m_player.z);
    const float safeY = sample.innerRadius * kSafeYFactor;
    const float safeX = sample.innerRadius * kSafeXFactor;
    const float nx = safeX > 0.f ? m_player.offX / safeX : 0.f;
    const float ny = safeY > 0.f ? m_player.offY / safeY : 0.f;

    const QString line = QStringLiteral("[test:%1] t=%2 z=%3/%4 seg=%5 vpY=%6 offY=%7 drawY=%8 normY=%9 speed=%10 wall=%11 hits=%12")
                             .arg(m_launchOptions.testName)
                             .arg(m_time, 0, 'f', 2)
                             .arg(m_player.z, 0, 'f', 1)
                             .arg(m_tunnelPath.totalLength(), 0, 'f', 1)
                             .arg(segmentLabel)
                             .arg(m_vpY, 0, 'f', 1)
                             .arg(m_player.offY, 0, 'f', 1)
                             .arg(drawCenter.y(), 0, 'f', 1)
                             .arg(ny, 0, 'f', 2)
                             .arg(m_player.speed, 0, 'f', 1)
                             .arg(m_player.wallContact ? QStringLiteral("yes") : QStringLiteral("no"))
                             .arg(m_wallHitCount);
    std::fprintf(stderr, "%s\n", line.toLocal8Bit().constData());
    std::fflush(stderr);

    m_testTraceTimer = 0.f;
    m_lastTestSegmentLabel = segmentLabel;
    m_lastTestWallContact = m_player.wallContact;
}

void GameScene::logTestCompletion() const
{
    if (!m_launchOptions.traceMode)
        return;

    const QString line = QStringLiteral("[test:%1] completed t=%2 z=%3/%4 score=%5 hits=%6 speed=%7 invulnerable=%8")
                             .arg(m_launchOptions.testName)
                             .arg(m_time, 0, 'f', 2)
                             .arg(m_player.z, 0, 'f', 1)
                             .arg(m_tunnelPath.totalLength(), 0, 'f', 1)
                             .arg(m_score, 0, 'f', 1)
                             .arg(m_wallHitCount)
                             .arg(m_player.speed, 0, 'f', 1)
                             .arg(m_invulnerable ? QStringLiteral("yes") : QStringLiteral("no"));
    std::fprintf(stderr, "%s\n", line.toLocal8Bit().constData());
    std::fflush(stderr);
}

void GameScene::applySelectedTrack()
{
    if (m_launchOptions.testMode || m_trackOptions.isEmpty())
        return;

    m_selectedTrackIndex = qBound(0, m_selectedTrackIndex, m_trackOptions.size() - 1);
    m_launchOptions.trackResource = m_trackOptions[m_selectedTrackIndex].resource;
    m_tunnelPath = TunnelPath(m_launchOptions.trackResource);
}

void GameScene::cycleTrackSelection(int dir)
{
    if (m_launchOptions.testMode || m_trackOptions.isEmpty() || dir == 0)
        return;

    m_selectedTrackIndex += dir;
    if (m_selectedTrackIndex < 0)
        m_selectedTrackIndex = m_trackOptions.size() - 1;
    else if (m_selectedTrackIndex >= m_trackOptions.size())
        m_selectedTrackIndex = 0;

    applySelectedTrack();
}

float GameScene::turnOcclusion() const
{
    return m_tunnelPath.sample(m_player.z + 180.f).occlusion;
}

float GameScene::verticalOcclusion() const
{
    const QPointF nearC = m_tunnelPath.sample(m_player.z + 90.f).center;
    const QPointF farC  = m_tunnelPath.sample(m_player.z + 430.f).center;
    const float dy = static_cast<float>(farC.y() - nearC.y());
    const float absDy = std::abs(dy);
    const float t = qBound(0.f, (absDy - 35.f) / 170.f, 1.f);
    const float amt = t * t * (3.f - 2.f * t);
    if (amt <= 0.f)
        return 0.f;
    return dy < 0.f ? -amt : amt;
}

float GameScene::playerOffYNorm() const
{
    const float safeY = m_tunnelPath.sample(m_player.z).innerRadius * kSafeYFactor;
    return safeY > 0.f ? qBound(-1.f, m_player.offY / safeY, 1.f) : 0.f;
}

float GameScene::playerOffXNorm() const
{
    const float safeX = m_tunnelPath.sample(m_player.z).innerRadius * kSafeXFactor;
    return safeX > 0.f ? qBound(-1.f, m_player.offX / safeX, 1.f) : 0.f;
}

CaveRenderer::Mode GameScene::caveMode() const
{
    if (m_state == GameState::Playing || m_state == GameState::FailureCrash)
        return CaveRenderer::Mode::EnclosedTunnel;
    if (m_state == GameState::SuccessFlyout)
        return m_endOpenTimer > 0.f
            ? CaveRenderer::Mode::OpenMouth
            : CaveRenderer::Mode::EnclosedTunnel;
    return CaveRenderer::Mode::OpenMouth;
}

void GameScene::drawChaseGems(QPainter *painter) const
{
    if (m_state == GameState::Attract)
        return;

    struct GemDrawRef {
        const ChaseGem *gem = nullptr;
        int index = 0;
    };

    QList<GemDrawRef> sorted;
    sorted.reserve(m_chaseGems.size());
    for (int i = 0; i < m_chaseGems.size(); ++i) {
        const ChaseGem &gem = m_chaseGems[i];
        if (!gem.collected)
            sorted.append({&gem, i});
    }
    std::sort(sorted.begin(), sorted.end(),
              [](const GemDrawRef &a, const GemDrawRef &b) { return a.gem->z > b.gem->z; });

    const TunnelPath::Sample playerSample = m_tunnelPath.sample(m_player.z);
    for (const GemDrawRef &ref : sorted) {
        const ChaseGem *gem = ref.gem;
        float ahead = gem->z - m_player.z;
        if (ahead < -90.f || ahead > 980.f)
            continue;

        const TunnelPath::Sample gemSample = m_tunnelPath.sample(gem->z);
        const QPointF offset = m_tunnelPath.gemOffset(ref.index, gem->z);
        QPointF relativeCenter = gemSample.center - playerSample.center;

        if (m_state == GameState::Countdown) {
            ahead = 280.f + ref.index * 78.f;
            relativeCenter *= 0.35f;
        } else if (m_state == GameState::Intro) {
            // Four gems float in a loose diamond, then dart into the tunnel.
            constexpr float DART_START_T = 0.58f;
            // Spread offsets: each gem has a distinct world-space position
            // relative to the tunnel centre during the showcase phase.
            static const float spreadX[4] = {-68.f,  60.f, -50.f,  55.f};
            static const float spreadY[4] = { 16.f, -28.f, -34.f,  24.f};

            if (m_introAnimT < DART_START_T) {
                // Showcase: gems appear with a staggered fade-in and float bob
                const float appearDelay = ref.index * 0.08f;
                const float appear = qBound(0.f,
                    (m_introAnimT - appearDelay) / 0.14f, 1.f);
                const float bob = std::sin(m_time * 2.6f + ref.index * 1.57f) * 9.f;
                ahead          = 220.f + ref.index * 32.f;
                relativeCenter = QPointF(spreadX[ref.index] * appear,
                                         spreadY[ref.index] * appear + bob);
            } else {
                // Dart: gems accelerate into the tunnel, spread collapses to centre
                const float dartT    = (m_introAnimT - DART_START_T) / (1.f - DART_START_T);
                const float dartEase = std::pow(dartT, 1.8f);
                const float converge = std::pow(dartT, 0.7f);
                const float bob      = std::sin(m_time * 2.6f + ref.index * 1.57f)
                                       * 9.f * (1.f - dartT);
                ahead = 220.f + ref.index * 32.f
                      + dartEase * (260.f + ref.index * 95.f);
                relativeCenter = QPointF(spreadX[ref.index] * (1.f - converge),
                                         spreadY[ref.index] * (1.f - converge) + bob);
            }
        }
        const float renderZ = qBound(90.f, ahead + 245.f, 980.f);
        const float wx = relativeCenter.x() + offset.x() - m_player.offX * 0.22f;
        const float wy = relativeCenter.y() + offset.y() - m_player.offY * 0.22f;
        const float px = projX(wx, renderZ);
        const float py = projY(wy, renderZ);
        const float sc = projScale(renderZ);
        const float r = qBound(7.f, gem->radius * sc, 46.f);
        const float visible = 1.f - qBound(0.f, gemSample.occlusion * (ahead / 720.f), 0.68f);
        const int alpha = static_cast<int>(qBound(0.18f, visible, 1.f) * 255.f);

        QRadialGradient glow(px, py, r * 3.0f);
        glow.setColorAt(0.0, QColor(gem->core.red(), gem->core.green(), gem->core.blue(), alpha / 2));
        glow.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter->setPen(Qt::NoPen);
        painter->setBrush(glow);
        painter->drawEllipse(QPointF(px, py), r * 3.0f, r * 3.0f);

        QPainterPath body;
        body.moveTo(px, py - r * 1.18f);
        body.lineTo(px + r * 0.92f, py - r * 0.12f);
        body.lineTo(px + r * 0.34f, py + r * 0.94f);
        body.lineTo(px - r * 0.34f, py + r * 0.94f);
        body.lineTo(px - r * 0.92f, py - r * 0.12f);
        body.closeSubpath();

        QLinearGradient facet(px - r, py - r, px + r, py + r);
        facet.setColorAt(0.0, gem->core.lighter(175));
        facet.setColorAt(0.45, QColor(gem->core.red(), gem->core.green(), gem->core.blue(), alpha));
        facet.setColorAt(1.0, gem->core.darker(210));
        painter->setBrush(facet);
        painter->setPen(QPen(QColor(gem->edge.red(), gem->edge.green(), gem->edge.blue(), alpha), 1.4f));
        painter->drawPath(body);

        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(255, 255, 255, alpha / 3));
        painter->drawEllipse(QPointF(px - r * 0.22f, py - r * 0.35f), r * 0.22f, r * 0.16f);
    }

    if (m_state == GameState::SuccessFlyout && m_endSequenceKind == EndSequenceKind::AllGemsCaptured) {
        static const QPointF escortOffsets[4] = {
            QPointF(-110.f, -42.f),
            QPointF(110.f, -42.f),
            QPointF(-72.f, 42.f),
            QPointF(72.f, 42.f),
        };

        for (int i = 0; i < m_chaseGems.size() && i < 4; ++i) {
            const ChaseGem &gem = m_chaseGems[i];
            const QPointF escort = escortOffsets[i];
            const float px = m_vpX + escort.x() + std::sin(m_time * 2.8f + i * 1.6f) * 10.f;
            const float py = m_vpY + escort.y() + std::cos(m_time * 2.2f + i * 1.3f) * 8.f;
            const float r = gem.radius * 0.65f;

            QRadialGradient glow(px, py, r * 2.8f);
            glow.setColorAt(0.0, QColor(gem.core.red(), gem.core.green(), gem.core.blue(), 130));
            glow.setColorAt(1.0, QColor(0, 0, 0, 0));
            painter->setPen(Qt::NoPen);
            painter->setBrush(glow);
            painter->drawEllipse(QPointF(px, py), r * 2.8f, r * 2.8f);

            QPainterPath body;
            body.moveTo(px, py - r * 1.18f);
            body.lineTo(px + r * 0.92f, py - r * 0.12f);
            body.lineTo(px + r * 0.34f, py + r * 0.94f);
            body.lineTo(px - r * 0.34f, py + r * 0.94f);
            body.lineTo(px - r * 0.92f, py - r * 0.12f);
            body.closeSubpath();

            QLinearGradient facet(px - r, py - r, px + r, py + r);
            facet.setColorAt(0.0, gem.core.lighter(175));
            facet.setColorAt(0.45, gem.core);
            facet.setColorAt(1.0, gem.core.darker(210));
            painter->setBrush(facet);
            painter->setPen(QPen(gem.edge, 1.2f));
            painter->drawPath(body);
        }
    }
}

void GameScene::drawPlayer(QPainter *painter) const
{
    // Project Cuarzito at PLAYER_DRAW_DEPTH using the same perspective formula
    // as the tunnel rings. This places him geometrically inside the tunnel so
    // wall contact is visually convincing — near the ceiling he appears near
    // the top of the screen, near the floor he appears near the bottom.
    const QPointF drawCenter = playerDrawCenter();
    const float cx    = drawCenter.x();
    const float bobY  = std::sin(m_time * 6.8f) * 3.8f;
    const float bobX  = std::sin(m_time * 4.1f + 1.2f) * 1.4f;
    const float cy    = drawCenter.y() + bobY;
    const float lean  = playerLean();
    const float reveal = m_revealDuration > 0.f
        ? qBound(0.f, m_revealTimer / m_revealDuration, 1.f)
        : 0.f;
    constexpr float W = 92.f, H = 112.f;

    // -----------------------------------------------------------------------
    // Cloak — rear-view hooded figure flying away from camera.
    //    Hood is a rounded dome, cloak fans out and flows below.
    // -----------------------------------------------------------------------
    const float hoodPeakX = cx + bobX + lean * 2.f;
    const float hoodPeakY = cy - H * 0.50f;

    QPainterPath cloak;
    // Hood top — broad rounded dome, not sharply pointed
    cloak.moveTo(hoodPeakX - W * 0.14f, hoodPeakY);
    cloak.cubicTo(hoodPeakX - W * 0.04f, hoodPeakY - H * 0.05f,
                  hoodPeakX + W * 0.04f, hoodPeakY - H * 0.05f,
                  hoodPeakX + W * 0.14f, hoodPeakY);
    // Right hood side flares outward to shoulder
    cloak.cubicTo(cx + W * 0.42f + lean * 5.f, hoodPeakY + H * 0.08f,
                  cx + W * 0.60f + lean * 6.f, cy - H * 0.16f,
                  cx + W * 0.52f + lean * 5.f, cy + H * 0.12f);
    // Right cloak fans outward, then hem
    cloak.cubicTo(cx + W * 0.60f + lean * 3.f, cy + H * 0.35f,
                  cx + W * 0.46f,              cy + H * 0.52f,
                  cx + lean * 3.f,             cy + H * 0.56f);
    // Left hem mirror
    cloak.cubicTo(cx - W * 0.46f,              cy + H * 0.52f,
                  cx - W * 0.60f + lean * 3.f, cy + H * 0.35f,
                  cx - W * 0.52f + lean * 5.f, cy + H * 0.12f);
    // Left hood side
    cloak.cubicTo(cx - W * 0.60f + lean * 6.f, cy - H * 0.16f,
                  cx - W * 0.42f + lean * 5.f, hoodPeakY + H * 0.08f,
                  hoodPeakX - W * 0.14f,       hoodPeakY);
    cloak.closeSubpath();

    // Dark cloak body: near-black with a very faint cool tint for depth
    QRadialGradient bodyGrad(cx + bobX - lean * 4.f, cy - H * 0.06f, W * 0.92f);
    bodyGrad.setColorAt(0.00, QColor(22, 22, 32));
    bodyGrad.setColorAt(0.45, QColor( 9,  9, 17));
    bodyGrad.setColorAt(1.00, QColor( 2,  2,  6));
    painter->setBrush(bodyGrad);
    painter->setPen(QPen(QColor(38, 48, 78, 150), 1.1f));
    painter->drawPath(cloak);

    // -----------------------------------------------------------------------
    // Hood detail lines — center-back seam and shoulder rim
    // -----------------------------------------------------------------------
    // Center seam: a faint crease down the back of the hood dome
    QPainterPath seam;
    seam.moveTo(hoodPeakX, hoodPeakY + H * 0.02f);
    seam.cubicTo(hoodPeakX + lean * 1.5f, cy - H * 0.28f,
                 hoodPeakX + lean * 2.0f, cy - H * 0.14f,
                 cx + bobX + lean * 2.5f, cy - H * 0.02f);
    painter->setPen(QPen(QColor(52, 58, 88, 75), 0.9f));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(seam);

    // Shoulder rim: arc marking where hood meets cloak
    QPainterPath rim;
    rim.moveTo(cx - W * 0.30f + lean * 4.f, cy - H * 0.24f);
    rim.cubicTo(cx - W * 0.12f + lean * 2.f, cy - H * 0.36f,
                cx + W * 0.12f + lean * 2.f, cy - H * 0.36f,
                cx + W * 0.30f + lean * 4.f, cy - H * 0.24f);
    painter->setPen(QPen(QColor(62, 72, 108, 82), 1.0f));
    painter->drawPath(rim);

    // -----------------------------------------------------------------------
    // Visor — side glimpse when leaning, full reveal on collect / game-over
    // -----------------------------------------------------------------------
    if (reveal > 0.f) {
        drawVisorReveal(painter, cx + bobX, cy, W, H, reveal);
    } else if (std::abs(lean) > 0.01f) {
        const float side   = lean > 0.f ? 1.f : -1.f;
        const float visorX = cx + bobX + side * W * 0.32f;
        const float visorY = cy - H * 0.18f;

        // Glow behind the visor sliver
        QRadialGradient sideGlow(visorX, visorY, 18.f);
        sideGlow.setColorAt(0.0, QColor(60, 255, 100, 140));
        sideGlow.setColorAt(1.0, QColor(  0,   0,   0,   0));
        painter->setPen(Qt::NoPen);
        painter->setBrush(sideGlow);
        painter->drawEllipse(QPointF(visorX, visorY), 18.f, 12.f);

        // The visor bar itself
        const float barX = visorX - (side > 0.f ? 3.f : 10.f);
        painter->setBrush(QColor(110, 255, 140, 235));
        painter->drawRoundedRect(QRectF(barX, visorY - 2.2f, 13.f, 4.0f), 2.f, 2.f);
    }
}

void GameScene::drawSafeZone(QPainter *painter) const
{
    const TunnelPath::Sample sample = m_tunnelPath.sample(m_player.z);
    const float scale = FOCAL / PLAYER_DRAW_DEPTH;
    const float safeX = sample.innerRadius * kSafeXFactor * scale;
    const float safeY = sample.innerRadius * kSafeYFactor * scale;
    const float anchorY = m_vpY + 30.f;
    const QRectF zoneRect(m_vpX - safeX,
                          anchorY - safeY,
                          safeX * 2.f,
                          safeY * 2.f);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    const bool contact = m_player.wallContact;
    const QColor outline = contact
        ? QColor(255, 170, 90, 210)
        : QColor(90, 235, 215, 155);
    const QColor fill = contact
        ? QColor(255, 120, 45, 26)
        : QColor(40, 180, 165, 18);

    painter->setPen(QPen(outline, 1.6f, Qt::DashLine));
    painter->setBrush(fill);
    painter->drawRoundedRect(zoneRect, 6.f, 6.f);

    // Midlines make the visible center and vertical/horizontal travel easier to read.
    painter->setPen(QPen(QColor(outline.red(), outline.green(), outline.blue(), 90), 1.0f, Qt::DotLine));
    painter->drawLine(QPointF(zoneRect.center().x(), zoneRect.top()),
                      QPointF(zoneRect.center().x(), zoneRect.bottom()));
    painter->drawLine(QPointF(zoneRect.left(), zoneRect.center().y()),
                      QPointF(zoneRect.right(), zoneRect.center().y()));

    painter->restore();
}

void GameScene::drawVisorReveal(QPainter *painter, float cx, float cy, float width, float height, float amount) const
{
    const float pulse = std::sin((1.f - amount) * static_cast<float>(M_PI));
    const float turn = 0.36f + pulse * 0.64f;
    const float visorY = cy - height * 0.16f;
    const float visorHW = width * (0.12f + turn * 0.22f);
    const float visorH = 3.5f + pulse * 1.5f;
    const int glowAlpha = static_cast<int>((70.f + pulse * 85.f) * amount);
    const int coreAlpha = static_cast<int>((170.f + pulse * 85.f) * amount);

    QLinearGradient glow(cx - visorHW * 2.0f, visorY, cx + visorHW * 2.0f, visorY);
    glow.setColorAt(0.0, QColor(0, 255, 80, 0));
    glow.setColorAt(0.35, QColor(0, 255, 80, glowAlpha));
    glow.setColorAt(0.5, QColor(120, 255, 145, qMin(glowAlpha + 35, 210)));
    glow.setColorAt(0.65, QColor(0, 255, 80, glowAlpha));
    glow.setColorAt(1.0, QColor(0, 255, 80, 0));

    painter->setPen(Qt::NoPen);
    painter->setBrush(glow);
    painter->drawRoundedRect(QRectF(cx - visorHW * 2.0f, visorY - 8.f,
                                    visorHW * 4.0f, 16.f),
                             8.f, 8.f);

    painter->setBrush(QColor(100, 255, 130, coreAlpha));
    painter->drawRoundedRect(QRectF(cx - visorHW, visorY - visorH * 0.5f,
                                    visorHW * 2.f, visorH),
                             2.f, 2.f);
}

void GameScene::drawPopups(QPainter *painter) const
{
    if (m_popups.isEmpty()) return;
    painter->save();
    painter->setFont(QFont("Courier New", 15, QFont::Bold));
    for (const auto &pop : m_popups) {
        float rise  = (1.f - pop.life) * 35.f;
        int   alpha = static_cast<int>(pop.life * 255.f);
        QColor col  = pop.value > 10 ? QColor(255, 175, 40, alpha)
                                     : QColor(80, 255, 120, alpha);
        painter->setPen(QPen(col));
        painter->drawText(QPointF(pop.sx - 14.f, pop.sy - rise),
                          QString("+%1").arg(pop.value));
    }
    painter->restore();
}

void GameScene::drawBursts(QPainter *painter) const
{
    if (m_bursts.isEmpty())
        return;

    painter->save();
    painter->setPen(Qt::NoPen);
    for (const auto &particle : m_bursts) {
        const int alpha = static_cast<int>(qBound(0.f, particle.life, 1.f) * 190.f);
        QColor color = particle.color;
        color.setAlpha(alpha);

        QRadialGradient glow(particle.sx, particle.sy, particle.radius * 2.7f);
        glow.setColorAt(0.0, color.lighter(150));
        glow.setColorAt(0.45, color);
        glow.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter->setBrush(glow);
        painter->drawEllipse(QPointF(particle.sx, particle.sy),
                             particle.radius * 2.7f,
                             particle.radius * 2.7f);

        color.setAlpha(qMin(alpha + 45, 230));
        painter->setBrush(color);
        painter->drawEllipse(QPointF(particle.sx, particle.sy),
                             particle.radius,
                             particle.radius);
    }
    painter->restore();
}

void GameScene::drawImpactFlash(QPainter *painter) const
{
    if (m_impactFlash <= 0.f)
        return;

    const float t = qBound(0.f, m_impactFlash, 1.f);
    painter->save();
    painter->setPen(Qt::NoPen);

    // Chromatic wash — more visible than before
    QColor wash(180, 255, 220, static_cast<int>(t * 80.f));
    painter->setBrush(wash);
    painter->drawRect(QRectF(0, 0, SCENE_W, SCENE_H));

    // Centre the shockwave on the projected character position so it aligns with
    // the visual. In first-person there is no character — use the VP instead.
    const QPointF drawCenter = playerDrawCenter();
    const float fsx = (m_viewMode == ViewMode::FirstPerson)
        ? m_vpX
        : drawCenter.x();
    const float fsy = (m_viewMode == ViewMode::FirstPerson)
        ? m_vpY
        : drawCenter.y();

    // Large shockwave expanding from impact point
    const float shockR = 220.f + (1.f - t) * 160.f;
    QRadialGradient shock(fsx, fsy, shockR);
    shock.setColorAt(0.00, QColor(140, 255, 190, static_cast<int>(t * 160.f)));
    shock.setColorAt(0.20, QColor(70, 200, 240, static_cast<int>(t * 100.f)));
    shock.setColorAt(0.55, QColor(20, 80, 130,  static_cast<int>(t *  40.f)));
    shock.setColorAt(1.00, QColor(0, 0, 0, 0));
    painter->setBrush(shock);
    painter->drawEllipse(QPointF(fsx, fsy), shockR, shockR * 0.78f);

    painter->restore();
}

// ---------------------------------------------------------------------------
// Game loop tick
// ---------------------------------------------------------------------------

void GameScene::update(float dt)
{
    m_input.updateGamepad();

    if (m_revealTimer > 0.f)
        m_revealTimer = qMax(0.f, m_revealTimer - dt);
    if (m_impactFlash > 0.f)
        m_impactFlash = qMax(0.f, m_impactFlash - dt * 2.8f);
    if (m_cameraShake > 0.f)
        m_cameraShake = qMax(0.f, m_cameraShake - dt * 5.5f);
    if (m_diagTimer > 0.f)
        m_diagTimer = qMax(0.f, m_diagTimer - dt);
    if (m_launchOptions.traceMode)
        m_testTraceTimer += dt;

    for (auto &burst : m_bursts) {
        burst.sx += burst.vx * dt;
        burst.sy += burst.vy * dt;
        burst.vy += 80.f * dt;
        burst.life -= dt * 2.2f;
        burst.radius *= (1.f + dt * 1.8f);
    }
    m_bursts.removeIf([](const BurstParticle &p) { return p.life <= 0.f; });

    switch (m_state) {
    case GameState::Attract:  updateAttract(dt);  break;
    case GameState::Intro:    updateIntro(dt);    break;
    case GameState::Countdown:updateCountdown(dt);break;
    case GameState::Playing:  updatePlaying(dt);  break;
    case GameState::SuccessFlyout:updateSuccessFlyout(dt); break;
    case GameState::FailureCrash:updateFailureCrash(dt); break;
    case GameState::GameOver: updateGameOver(dt); break;
    case GameState::HighScoreEntry:updateHighScoreEntry(dt);break;
    }

    m_input.endFrame();
}

void GameScene::updateAttract(float dt)
{
    m_time += dt;
    m_tunnelZ += 80.f * dt;
    m_vpX = CX + std::sin(m_time * 0.18f) * 90.f;
    m_vpY = CY + std::sin(m_time * 0.13f + 1.0f) * 65.f;

    if (m_input.isLeftJustPressed())
        cycleTrackSelection(-1);
    if (m_input.isRightJustPressed())
        cycleTrackSelection(1);

    setOverlay(attractOverlayText());
    if (m_input.isConfirmJustPressed())
        startIntro();
}

void GameScene::updateIntro(float dt)
{
    m_introTimer -= dt;
    m_time       += dt;
    m_tunnelZ    += 80.f * dt;

    constexpr float INTRO_TOTAL  = 4.2f;
    constexpr float DART_START_T = 0.58f; // normalised t at which gems begin darting

    m_introAnimT = qBound(0.f, 1.f - m_introTimer / INTRO_TOTAL, 1.f);

    // Cuarzito rises into frame: smooth ease-out over first 38% of intro
    const float riseT    = qBound(0.f, m_introAnimT / 0.38f, 1.f);
    const float riseEase = 1.f - std::pow(1.f - riseT, 2.8f);
    m_player.offY = 210.f * (1.f - riseEase);

    // VP: gentle oscillation, amplitude grows slightly as tension builds
    m_vpX = CX + std::sin(m_time * 0.38f) * (48.f + m_introAnimT * 28.f);
    m_vpY = CY + std::sin(m_time * 0.28f + 1.0f) * (30.f + m_introAnimT * 16.f);

    // Timed text: silent while rising, then set up context
    if (m_introAnimT < 0.25f) {
        setOverlay("");
    } else if (m_introAnimT < DART_START_T) {
        setOverlay("THE GEMS ESCAPE");
    } else {
        setOverlay("GET READY");
    }

    if (m_input.isConfirmJustPressed() || m_introTimer <= 0.f)
        startGame();
}

void GameScene::updateCountdown(float dt)
{
    m_countdownTimer -= dt;
    m_time += dt;
    m_tunnelZ += 80.f * dt;

    m_vpX = CX + std::sin(m_time * 0.18f) * 70.f;
    m_vpY = CY + std::sin(m_time * 0.13f + 1.0f) * 50.f;

    const int beat = static_cast<int>(std::ceil(m_countdownTimer));
    if (beat > 0) {
        setOverlay(QString::number(beat));
    } else if (m_countdownTimer > -0.35f) {
        setOverlay("GO");
    } else {
        startGame();
    }
}

void GameScene::updatePlaying(float dt)
{
    updateChasePhysics(dt);
    if (m_launchOptions.traceMode)
        logTestTrace();

    if (m_launchOptions.testMode && m_player.z >= m_tunnelPath.totalLength()) {
        logTestCompletion();
        QCoreApplication::quit();
        return;
    }

    for (auto &gem : m_chaseGems) {
        if (!gem.collected)
            gem.z += gem.speed * dt;
    }

    for (int i = 0; i < m_chaseGems.size(); ++i) {
        ChaseGem &gem = m_chaseGems[i];
        if (gem.collected)
            continue;

        const float ahead = gem.z - m_player.z;
        const QPointF targetOffset = m_tunnelPath.gemOffset(i, gem.z);
        const float dx = m_player.offX - targetOffset.x();
        const float dy = m_player.offY - targetOffset.y();
        const float catchRadius = 82.f;
        if (ahead <= 34.f && ahead > -90.f && dx * dx + dy * dy <= catchRadius * catchRadius) {
            gem.collected = true;
            m_score += gem.value + qMax(0.f, m_energy) * 6.f + m_cleanFlightTime * 8.f;
            m_audio.play(SoundCue::CollectSpecial);
            m_revealDuration = 0.52f;
            m_revealTimer = m_revealDuration;
            {
                const QPointF drawCenter = playerDrawCenter();
                const float bsx = drawCenter.x();
                const float bsy = drawCenter.y();
                spawnBurst(bsx, bsy - 22.f, true);
                m_popups.append({bsx, bsy - 42.f, gem.value, 1.0f});
            }
            m_cleanFlightTime += 3.f;
        }
    }

    const bool allGemsCollected = std::all_of(m_chaseGems.cbegin(), m_chaseGems.cend(),
                                             [](const ChaseGem &gem) { return gem.collected; });
    if (allGemsCollected) {
        m_score += qMax(0.f, m_energy) * 30.f + m_cleanFlightTime * 45.f;
        m_runWon = true;
        startSuccessFlyout(EndSequenceKind::AllGemsCaptured,
                           QStringLiteral("ALL GEMS CAPTURED"),
                           QStringLiteral("Cuarzito escapes into space"));
        return;
    }

    if (m_player.z >= m_tunnelPath.totalLength()) {
        m_score += qMax(0.f, m_energy) * 20.f + static_cast<float>(allGemsCollected ? 500 : 0);
        m_runWon = true;
        const int collected = static_cast<int>(std::count_if(m_chaseGems.cbegin(), m_chaseGems.cend(),
                                                             [](const ChaseGem &gem) { return gem.collected; }));
        startSuccessFlyout(EndSequenceKind::TrackComplete,
                           QStringLiteral("TRACK COMPLETE"),
                           QStringLiteral("%1/4 gems collected").arg(collected));
        return;
    }

    if (m_energy <= 0.f) {
        m_runWon = false;
        startFailureCrash(QStringLiteral("OUT OF ENERGY"),
                          QStringLiteral("Cuarzito loses control in the tunnel"));
        return;
    }

    // --- Score and difficulty ramp ---
    m_survivalTime   += dt;
    if (m_player.wallContact)
        m_cleanFlightTime = 0.f;
    else
        m_cleanFlightTime += dt;

    const float scoreRate = qMax(1.f, m_player.speed / 190.f) * (m_player.wallContact ? 0.25f : 1.f);
    m_score += dt * scoreRate;
    m_worldSpeed      = m_player.speed;

    // --- Advance popups ---
    for (auto &pop : m_popups) pop.life -= dt * 1.6f;
    m_popups.removeIf([](const ScorePopup &p) { return p.life <= 0.f; });

    updateHUD();
}

void GameScene::updateChasePhysics(float dt)
{
    m_time += dt;

    const TunnelPath::Sample physSample = m_tunnelPath.sample(m_player.z);
    const float safeX = physSample.innerRadius * kSafeXFactor;
    const float safeY = physSample.innerRadius * kSafeYFactor;

    // --- Input: move Cuarzito inside the tunnel cross-section ---
    const float steerSpeed = PLAYER_SPEED * (0.72f + m_player.speed / CHASE_MAX_SPEED * 0.42f);
    const float scriptedVerticalIntent = testVerticalIntent();
    const float inputX =
        (m_input.isMovingRight() ? 1.f : 0.f) -
        (m_input.isMovingLeft() ? 1.f : 0.f);
    const bool moveUp = scriptedVerticalIntent < -0.5f || m_input.isMovingUp();
    const bool moveDown = scriptedVerticalIntent > 0.5f || m_input.isMovingDown();
    const float inputY = (moveUp ? -1.f : 0.f) + (moveDown ? 1.f : 0.f);
    m_player.offX += inputX * steerSpeed * dt;
    m_player.offY += inputY * steerSpeed * dt;

    const bool accelerating = m_launchOptions.testMode || m_input.isAccelerating();
    const bool braking = !m_launchOptions.testMode && m_input.isBraking();
    if (accelerating)
        m_player.speed += CHASE_ACCEL * dt;
    if (braking)
        m_player.speed -= CHASE_BRAKE * dt;
    if (!accelerating)
        m_player.speed -= CHASE_DRAG * dt;

    m_player.speed = qBound(CHASE_MIN_SPEED, m_player.speed, CHASE_MAX_SPEED);

    // --- Tunnel-induced drift ---
    // The camera follows the tunnel axis. Cuarzito stays in local tunnel space
    // and must be steered back toward the safe center.
    //
    // Route-induced drift: use the same tunnel look-ahead idea as the camera
    // instead of raw curvature alone, so the visible route, safe corridor, and
    // kinetic push all react to the same path signal.
    const QPointF routeLead = m_tunnelPath.sample(m_player.z + kPhysicsLookAhead).center
                            - physSample.center;
    const float leadX = qBound(-1.f, routeLead.x() / kPhysicsLeadScaleX, 1.f);
    const float leadY = qBound(-1.f, -routeLead.y() / kPhysicsLeadScaleY, 1.f);
    const float driftScale = (m_player.speed / CHASE_MAX_SPEED) * kPhysicsDriftGain;
    m_player.offX -= leadX * safeX * driftScale * dt;
    m_player.offY += leadY * safeY * driftScale * dt;

    // --- Wall collision and clamp ---
    const float nx = safeX > 0.f ? m_player.offX / safeX : 0.f;
    const float ny = safeY > 0.f ? m_player.offY / safeY : 0.f;
    m_player.wallContact = std::abs(nx) > 1.f || std::abs(ny) > 1.f;

    if (m_player.wallContact) {
        if (!m_wasWallContact) {
            ++m_wallHitCount;
            const float hitT = m_player.speed / CHASE_MAX_SPEED;
            m_impactFlash  = qMax(m_impactFlash,  0.30f + hitT * 0.55f);
            m_cameraShake  = qMax(m_cameraShake,  0.22f + hitT * 0.38f);
            spawnWallSparks();
            if (!m_invulnerable) {
                m_score = qMax(0.f, m_score - 45.f);
                m_energy = qMax(0.f, m_energy - 25.f);
            }
        }
        m_player.offX = qBound(-safeX, m_player.offX, safeX);
        m_player.offY = qBound(-safeY, m_player.offY, safeY);
        if (!m_invulnerable)
            m_player.speed -= 260.f * dt;
    }
    m_wasWallContact = m_player.wallContact;

    m_player.speed = qBound(CHASE_MIN_SPEED, m_player.speed, CHASE_MAX_SPEED);

    // --- Advance position ---
    m_player.z   += m_player.speed * dt;
    m_worldSpeed  = m_player.speed;

    // --- VP tracks tunnel direction ahead (relative, not absolute world pos) ---
    // Using lookAhead - current keeps VP bounded regardless of how far the
    // tunnel has drifted in world space across many segments.
    // Look further ahead so upcoming turns displace the VP earlier, making
    // curves visually obvious on the walls before Cuarzito enters them.
    const QPointF currentCenter = physSample.center;
    const QPointF lookAheadCenter = m_tunnelPath.sample(m_player.z + kCameraLookAhead).center;
    const QPointF lookAheadCenterV = m_tunnelPath.sample(m_player.z + kCameraVerticalLookAhead).center;
    const QPointF relDir = lookAheadCenter - currentCenter;
    const QPointF relDirV = lookAheadCenterV - currentCenter;
    float targetVpX = CX + relDir.x() * 1.05f;
    // Vertical framing is intentionally more conservative than horizontal.
    // Large uphill/downhill displacements can destroy tunnel perspective if
    // the VP is allowed to climb or drop as aggressively as the raw path delta.
    // Path Y uses negative values for uphill, so add the look-ahead delta here:
    // uphill  -> VP rises
    // downhill -> VP falls
    float targetVpY = CY + kCameraVpYBias + relDirV.y() * kCameraVerticalGain;
    targetVpY = qBound(CY - kCameraVpYClamp, targetVpY, CY + kCameraVpYClamp);
    if (m_viewMode == ViewMode::FirstPerson) {
        // First-person can lean a little with Cuarzito, but the tunnel still
        // owns the main framing so controls remain consistent with third-person.
        targetVpX += m_player.offX * 0.14f;
        targetVpY += m_player.offY * 0.06f;
        targetVpY = qBound(CY - kCameraVpYClamp, targetVpY, CY + kCameraVpYClamp);
    }
    m_vpX += (targetVpX - m_vpX) * qMin(1.f, dt * 4.4f);
    m_vpY += (targetVpY - m_vpY) * qMin(1.f, dt * 4.4f);

    m_tunnelZ = m_player.z;
}

void GameScene::updateGameOver(float dt)
{
    m_gameOverTimer -= dt;
    m_gameOverIdleTimer -= dt;
    m_time += dt;
    m_tunnelZ += 80.f * dt;
    m_vpX = CX + std::sin(m_time * 0.18f) * 90.f;
    m_vpY = CY + std::sin(m_time * 0.13f + 1.0f) * 65.f;
    if (m_input.isJustPressed(Action::Cancel))
        startAttract();
    else if (m_gameOverTimer <= 0.f && m_input.isConfirmJustPressed())
        startIntro();
    else if (m_gameOverIdleTimer <= 0.f)
        startAttract();
}

void GameScene::updateSuccessFlyout(float dt)
{
    m_endSequenceTimer -= dt;
    m_time += dt;

    if (m_endSequenceKind == EndSequenceKind::AllGemsCaptured) {
        if (m_player.z < m_tunnelPath.totalLength()) {
            m_player.speed = CHASE_MAX_SPEED;
            m_tunnelZ = m_player.z;
            m_worldSpeed = m_player.speed;
            m_player.z += m_player.speed * dt;
            m_endOpenTimer = 0.f;
        } else {
            m_endOpenTimer += dt;
            m_player.speed += (CHASE_MIN_SPEED - m_player.speed) * qMin(1.f, dt * 1.2f);
            m_player.z += m_player.speed * dt;
            m_tunnelZ = m_player.z;
            m_worldSpeed = m_player.speed;
        }
    } else {
        if (m_endOpenTimer < 2.0f) {
            m_endOpenTimer += dt;
            m_player.speed += (CHASE_MIN_SPEED - m_player.speed) * qMin(1.f, dt * 1.35f);
        } else {
            m_player.speed = qMax(CHASE_MIN_SPEED, m_player.speed - 14.f * dt);
        }
        m_player.z += m_player.speed * dt;
        m_tunnelZ = m_player.z;
        m_worldSpeed = m_player.speed;
    }

    m_player.offX += (0.f - m_player.offX) * qMin(1.f, dt * 2.8f);
    m_player.offY += (-26.f - m_player.offY) * qMin(1.f, dt * 1.8f);

    const float targetY = m_endOpenTimer > 0.f ? CY - 18.f : CY + 10.f;
    m_vpX += (CX - m_vpX) * qMin(1.f, dt * 2.0f);
    m_vpY += (targetY - m_vpY) * qMin(1.f, dt * 1.8f);

    setOverlay(QString("%1\n\n%2\n\nScore: %3")
                   .arg(m_endTitle, m_endDetail)
                   .arg(static_cast<int>(m_score)));
    m_hudText.clear();

    if (m_endSequenceTimer <= 0.f)
        endGame();
}

void GameScene::updateFailureCrash(float dt)
{
    m_endSequenceTimer -= dt;
    m_time += dt;

    m_player.speed += (CHASE_MIN_SPEED - m_player.speed) * qMin(1.f, dt * 1.6f);
    m_tunnelZ += m_player.speed * dt;
    m_worldSpeed = m_player.speed;

    m_player.offX += (0.f - m_player.offX) * qMin(1.f, dt * 2.0f);
    m_player.offY += (36.f - m_player.offY) * qMin(1.f, dt * 1.2f);

    m_vpX += (CX - m_vpX) * qMin(1.f, dt * 2.6f);
    m_vpY += ((CY + 28.f) - m_vpY) * qMin(1.f, dt * 2.0f);
    m_cameraShake = qMax(m_cameraShake, 0.12f);

    setOverlay(QString("%1\n\n%2\n\nScore: %3")
                   .arg(m_endTitle, m_endDetail)
                   .arg(static_cast<int>(m_score)));
    m_hudText.clear();

    if (m_endSequenceTimer <= 0.f)
        endGame();
}

void GameScene::updateHighScoreEntry(float dt)
{
    m_time += dt;
    m_tunnelZ += 80.f * dt;
    m_vpX = CX + std::sin(m_time * 0.18f) * 70.f;
    m_vpY = CY + std::sin(m_time * 0.13f + 1.0f) * 50.f;
    if (m_input.isJustPressed(Action::Cancel)) {
        startAttract();
        return;
    }

    if (m_input.isLeftJustPressed())
        m_initialIndex = qMax(0, m_initialIndex - 1);
    if (m_input.isRightJustPressed())
        m_initialIndex = qMin(2, m_initialIndex + 1);

    if (m_input.isUpJustPressed() || m_input.isDownJustPressed()) {
        const int dir = m_input.isUpJustPressed() ? 1 : -1;
        QChar ch = m_initials[m_initialIndex];
        int offset = ch.unicode() - QChar('A').unicode();
        offset = (offset + dir + 26) % 26;
        m_initials[m_initialIndex] = QChar(QChar('A').unicode() + offset);
    }

    if (m_input.isConfirmJustPressed()) {
        if (m_initialIndex < 2) {
            m_audio.play(SoundCue::Confirm);
            ++m_initialIndex;
        } else {
            m_highScores.addScore(m_initials, m_pendingScore);
            m_audio.play(SoundCue::Confirm);
            m_scoreSubmitted = true;
            m_pendingScore = 0;
            startAttract();
            return;
        }
    }

    setOverlay(initialsEntryText());
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void GameScene::startGame()
{
    applySelectedTrack();
    resetChaseGems();
    m_popups.clear();
    m_bursts.clear();
    m_player = Player{};

    m_vpX     = CX;
    m_vpY     = CY;
    m_time    = 0.f;
    m_tunnelZ = 0.f;

    m_player.speed     = CHASE_MIN_SPEED;
    m_worldSpeed       = CHASE_MIN_SPEED;
    m_survivalTime    = 0.f;
    m_score           = 0.f;
    m_gameOverTimer   = 0.f;
    m_gameOverIdleTimer = 0.f;
    m_introTimer      = 0.f;
    m_countdownTimer  = 0.f;
    m_energy         = 100.f;
    m_cleanFlightTime = 0.f;
    m_revealTimer     = 0.f;
    m_revealDuration  = 0.f;
    m_impactFlash     = 0.f;
    m_endSequenceTimer = 0.f;
    m_endOpenTimer    = 0.f;
    m_cameraShake     = 0.f;
    m_scoreSubmitted  = false;
    m_runWon          = false;
    m_wasWallContact  = false;
    m_wallHitCount    = 0;
    m_pendingScore    = 0;
    m_initialIndex    = 0;
    m_state           = GameState::Playing;

    m_hudText.clear();
    m_overlayText.clear();
    m_endTitle.clear();
    m_endDetail.clear();
    m_endSequenceKind = EndSequenceKind::None;
    m_invulnerable = true;
    if (m_launchOptions.testMode) {
        m_overlayText = QString("TEST MODE: %1").arg(m_launchOptions.testName);
        m_testTraceTimer = 1.f;
        m_lastTestSegmentLabel.clear();
        m_lastTestWallContact = false;
        if (m_launchOptions.traceMode) {
            const QString line = QStringLiteral("[test:%1] start track=%2 length=%3 invulnerable=yes")
                                     .arg(m_launchOptions.testName)
                                     .arg(m_launchOptions.trackResource)
                                     .arg(m_tunnelPath.totalLength(), 0, 'f', 1);
            std::fprintf(stderr, "%s\n", line.toLocal8Bit().constData());
            std::fflush(stderr);
        }
    }
}

void GameScene::startAttract()
{
    m_chaseGems.clear();
    m_popups.clear();
    m_bursts.clear();
    m_player = Player{};

    m_vpX = CX;
    m_vpY = CY;
    m_time = 0.f;

    m_worldSpeed      = CHASE_BASE_SPEED;
    m_survivalTime    = 0.f;
    m_score           = 0.f;
    m_gameOverTimer   = 0.f;
    m_gameOverIdleTimer = 0.f;
    m_introTimer      = 0.f;
    m_countdownTimer  = 0.f;
    m_energy         = 100.f;
    m_cleanFlightTime = 0.f;
    m_revealTimer     = 0.f;
    m_revealDuration  = 0.f;
    m_impactFlash     = 0.f;
    m_endSequenceTimer = 0.f;
    m_endOpenTimer    = 0.f;
    m_cameraShake     = 0.f;
    m_scoreSubmitted  = false;
    m_runWon          = false;
    m_wasWallContact  = false;
    m_wallHitCount    = 0;
    m_pendingScore    = 0;
    m_initialIndex    = 0;
    m_initials        = "AAA";
    m_state           = GameState::Attract;

    m_hudText.clear();
    m_endTitle.clear();
    m_endDetail.clear();
    m_endSequenceKind = EndSequenceKind::None;
    setOverlay(attractOverlayText());
}

void GameScene::startIntro()
{
    applySelectedTrack();
    resetChaseGems();
    m_popups.clear();
    m_bursts.clear();
    m_player = Player{};

    m_vpX  = CX;
    m_vpY  = CY;
    m_time = 0.f;

    m_worldSpeed      = CHASE_BASE_SPEED;
    m_survivalTime    = 0.f;
    m_score           = 0.f;
    m_gameOverTimer   = 0.f;
    m_gameOverIdleTimer = 0.f;
    m_introTimer      = 4.2f;
    m_countdownTimer  = 0.f;
    m_energy         = 100.f;
    m_cleanFlightTime = 0.f;
    m_introAnimT      = 0.f;
    m_impactFlash     = 0.f;
    m_endSequenceTimer = 0.f;
    m_endOpenTimer    = 0.f;
    m_cameraShake     = 0.f;
    m_scoreSubmitted  = false;
    m_runWon          = false;
    m_wasWallContact  = false;
    m_wallHitCount    = 0;
    m_pendingScore    = 0;
    m_initialIndex    = 0;
    m_state           = GameState::Intro;

    // Cuarzito starts off-screen below; visor reveal fires immediately as he rises
    m_player.offY     = 210.f;
    m_revealDuration  = 0.65f;
    m_revealTimer     = m_revealDuration;

    m_hudText.clear();
    m_endTitle.clear();
    m_endDetail.clear();
    m_endSequenceKind = EndSequenceKind::None;
    setOverlay("");
    m_audio.play(SoundCue::Start);
}

void GameScene::startCountdown()
{
    resetChaseGems();
    m_popups.clear();
    m_bursts.clear();
    m_player = Player{};

    m_vpX  = CX;
    m_vpY  = CY;
    m_time = 0.f;

    m_worldSpeed      = CHASE_BASE_SPEED;
    m_survivalTime    = 0.f;
    m_score           = 0.f;
    m_gameOverTimer   = 0.f;
    m_gameOverIdleTimer = 0.f;
    m_introTimer      = 0.f;
    m_countdownTimer  = 3.0f;
    m_energy         = 100.f;
    m_cleanFlightTime = 0.f;
    m_revealTimer     = 0.f;
    m_revealDuration  = 0.f;
    m_impactFlash     = 0.f;
    m_endSequenceTimer = 0.f;
    m_endOpenTimer    = 0.f;
    m_cameraShake     = 0.f;
    m_scoreSubmitted  = false;
    m_runWon          = false;
    m_wasWallContact  = false;
    m_wallHitCount    = 0;
    m_pendingScore    = 0;
    m_initialIndex    = 0;
    m_state           = GameState::Countdown;

    m_hudText.clear();
    m_endTitle.clear();
    m_endDetail.clear();
    m_endSequenceKind = EndSequenceKind::None;
    setOverlay("3");
}

void GameScene::endGame()
{
    m_state         = GameState::GameOver;
    m_gameOverTimer = 1.5f;
    m_gameOverIdleTimer = 8.0f;
    m_revealDuration = 1.5f;
    m_revealTimer    = m_revealDuration;
    m_impactFlash    = 1.f;

    const int finalScore = static_cast<int>(m_score);
    if (!m_scoreSubmitted && m_highScores.qualifies(finalScore)) {
        startHighScoreEntry(finalScore);
        return;
    }

    setOverlay(QString("%1\n\n%2\n\nScore: %3    Time: %4s\n\nPRESS SPACE TO RESTART")
                   .arg(m_endTitle.isEmpty() ? (m_runWon ? "YOU WIN" : "GAME OVER") : m_endTitle)
                   .arg(m_endDetail)
                   .arg(finalScore)
                   .arg(static_cast<int>(m_survivalTime)));
    m_hudText.clear();
}

void GameScene::startHighScoreEntry(int score)
{
    m_state = GameState::HighScoreEntry;
    m_gameOverTimer = 0.f;
    m_revealDuration = 1.2f;
    m_revealTimer = m_revealDuration;
    m_pendingScore = score;
    m_initialIndex = 0;
    m_initials = "AAA";
    m_hudText.clear();
    setOverlay(initialsEntryText());
}

void GameScene::startSuccessFlyout(EndSequenceKind kind, const QString &title, const QString &detail)
{
    m_state = GameState::SuccessFlyout;
    m_endSequenceTimer = 3.6f;
    m_endOpenTimer = (kind == EndSequenceKind::TrackComplete) ? 0.f : -1.f;
    m_endSequenceKind = kind;
    m_endTitle = title;
    m_endDetail = detail;
    m_revealDuration = 0.9f;
    m_revealTimer = m_revealDuration;
    m_impactFlash = qMax(m_impactFlash, 0.45f);
    m_cameraShake = qMax(m_cameraShake, 0.12f);
    m_audio.play(SoundCue::CollectSpecial);
}

void GameScene::startFailureCrash(const QString &title, const QString &detail)
{
    m_state = GameState::FailureCrash;
    m_endSequenceTimer = 2.8f;
    m_endOpenTimer = 0.f;
    m_endSequenceKind = EndSequenceKind::OutOfEnergy;
    m_endTitle = title;
    m_endDetail = detail;
    m_revealDuration = 1.1f;
    m_revealTimer = m_revealDuration;
    m_impactFlash = 1.f;
    m_cameraShake = qMax(m_cameraShake, 0.45f);
    m_audio.play(SoundCue::GameOver);
}

void GameScene::resetChaseGems()
{
    const QVector<TunnelPath::GemConfig> configs = m_tunnelPath.gemConfigs();
    const auto gemConfigAt = [&](int index, float fallbackZ, float fallbackSpeed) {
        if (index < configs.size())
            return configs[index];
        return TunnelPath::GemConfig{fallbackZ, fallbackSpeed};
    };

    const TunnelPath::GemConfig blueCfg   = gemConfigAt(0,  380.f, 205.f);
    const TunnelPath::GemConfig orangeCfg = gemConfigAt(1,  680.f, 212.f);
    const TunnelPath::GemConfig yellowCfg = gemConfigAt(2, 1040.f, 219.f);
    const TunnelPath::GemConfig cyanCfg   = gemConfigAt(3, 1460.f, 226.f);

    m_chaseGems = {
        {"BLUE",   QColor( 65, 155, 255), QColor(150, 220, 255), blueCfg.startZ,   blueCfg.speed,   23.f,  500, false},
        {"ORANGE", QColor(255, 135,  35), QColor(255, 215, 105), orangeCfg.startZ, orangeCfg.speed, 25.f,  750, false},
        {"YELLOW", QColor(255, 230,  65), QColor(255, 250, 170), yellowCfg.startZ, yellowCfg.speed, 27.f, 1000, false},
        {"CYAN",   QColor( 65, 245, 230), QColor(180, 255, 245), cyanCfg.startZ,   cyanCfg.speed,   30.f, 1500, false},
    };
}

void GameScene::spawnBurst(float sx, float sy, bool special)
{
    auto *rng = QRandomGenerator::global();
    const int count = special ? 18 : 12;
    const QColor base = special ? QColor(255, 160, 40) : QColor(75, 255, 120);

    for (int i = 0; i < count; ++i) {
        const float angle = static_cast<float>(rng->generateDouble() * 2.0 * M_PI);
        const float speed = special
            ? 70.f + static_cast<float>(rng->generateDouble()) * 150.f
            : 55.f + static_cast<float>(rng->generateDouble()) * 115.f;
        BurstParticle particle;
        particle.sx = sx;
        particle.sy = sy;
        particle.vx = std::cos(angle) * speed;
        particle.vy = std::sin(angle) * speed;
        particle.radius = 2.2f + static_cast<float>(rng->generateDouble()) * (special ? 3.6f : 2.6f);
        particle.life = 1.f;
        particle.color = base.lighter(95 + static_cast<int>(rng->generateDouble() * 55.0));
        m_bursts.append(particle);
    }
}

void GameScene::spawnWallSparks()
{
    // Spawn rock-chip sparks from the screen edge on the contact side,
    // flying inward — makes it feel like Cuarzito scraped the cave wall.
    const float safeX = m_tunnelPath.sample(m_player.z).innerRadius * kSafeXFactor;
    const float safeY = m_tunnelPath.sample(m_player.z).innerRadius * kSafeYFactor;
    const float nx    = safeX > 0.f ? m_player.offX / safeX : 0.f;
    const float ny    = safeY > 0.f ? m_player.offY / safeY : 0.f;

    // Which wall did we hit? Dominant axis.
    float inX = 0.f, inY = 0.f, spawnX = 0.f, spawnY = 0.f;
    if (std::abs(nx) >= std::abs(ny)) {
        inX    = nx > 0.f ? -1.f : 1.f;   // inward direction
        spawnX = nx > 0.f ? SCENE_W - 24.f : 24.f;
        spawnY = qBound(60.f, playerSY(), SCENE_H - 60.f);
    } else {
        inY    = ny > 0.f ? -1.f : 1.f;
        spawnX = qBound(60.f, playerSX(), SCENE_W - 60.f);
        spawnY = ny > 0.f ? SCENE_H - 24.f : 24.f;
    }

    auto *rng = QRandomGenerator::global();
    constexpr int COUNT = 12;
    for (int i = 0; i < COUNT; ++i) {
        const float spread = static_cast<float>((rng->generateDouble() - 0.5) * M_PI * 0.75);
        const float spd    = 70.f + static_cast<float>(rng->generateDouble()) * 160.f;
        const float ca = std::cos(spread), sa = std::sin(spread);
        BurstParticle p;
        p.sx     = spawnX + static_cast<float>((rng->generateDouble() - 0.5) * 50.f);
        p.sy     = spawnY + static_cast<float>((rng->generateDouble() - 0.5) * 50.f);
        p.vx     = (inX * ca - inY * sa) * spd;
        p.vy     = (inX * sa + inY * ca) * spd;
        p.radius = 1.4f + static_cast<float>(rng->generateDouble() * 2.8f);
        p.life   = 1.f;
        const int r = 190 + static_cast<int>(rng->generateDouble() * 65.0);
        const int g = 90  + static_cast<int>(rng->generateDouble() * 70.0);
        p.color  = QColor(r, g, 28);
        m_bursts.append(p);
    }
}

void GameScene::setOverlay(const QString &text)
{
    m_overlayText = text;
}

void GameScene::showDiagnostics(const QString &text)
{
    m_diagText  = text;
    m_diagTimer = 10.f;
}

void GameScene::restartRun()
{
    startGame();
}

QString GameScene::attractOverlayText() const
{
    if (m_launchOptions.testMode)
        return "CUARZITO\n\nPRESS SPACE TO START";

    const QString trackLabel = selectedTrackLabel();
    const QString trackSubtitle = m_trackOptions.isEmpty()
        ? QString()
        : m_trackOptions[m_selectedTrackIndex].subtitle;
    return QString("CUARZITO\n\nTRACK  %1\n%2\n\nLEFT / RIGHT TO CHANGE\nSPACE TO START")
        .arg(trackLabel, trackSubtitle);
}

QString GameScene::selectedTrackLabel() const
{
    if (m_trackOptions.isEmpty())
        return QStringLiteral("NONE");
    return m_trackOptions[qBound(0, m_selectedTrackIndex, m_trackOptions.size() - 1)].label;
}

QString GameScene::initialsEntryText() const
{
    return QString("NEW HIGH SCORE\n\nScore: %1\n\n\n\n\nUP/DOWN LETTER   LEFT/RIGHT SLOT\nSPACE CONFIRM")
        .arg(m_pendingScore)
        ;
}

void GameScene::updateHUD()
{
    const int collected = static_cast<int>(std::count_if(m_chaseGems.cbegin(), m_chaseGems.cend(),
                                                        [](const ChaseGem &gem) { return gem.collected; }));
    float nextDistance = 0.f;
    for (const ChaseGem &gem : m_chaseGems) {
        if (!gem.collected) {
            nextDistance = qMax(0.f, gem.z - m_player.z);
            break;
        }
    }

    m_hudText =
        QString("SCORE  %1    ENERGY %2    GEM  %3/4    NEXT  %4    SPEED  %5%6")
            .arg(static_cast<int>(m_score), 5)
            .arg(static_cast<int>(qMax(0.f, m_energy)), 3)
            .arg(collected)
            .arg(static_cast<int>(nextDistance), 4)
            .arg(static_cast<int>(m_player.speed), 3)
            .arg(m_player.wallContact
                 ? QString("  WALL %1").arg(m_wallHitCount)
                 : QString("  CLEAN %1s").arg(static_cast<int>(m_cleanFlightTime)));
}

void GameScene::drawHUD(QPainter *painter) const
{
    painter->save();

    if (!m_hudText.isEmpty()) {
        QFont hudFont("Courier New", 18, QFont::Bold);
        painter->setFont(hudFont);
        painter->setPen(QColor(100, 255, 200));
        painter->drawText(QPointF(20.f, 34.f), m_hudText);
    }


    if (!m_overlayText.isEmpty()) {
        QFont overlayFont("Courier New", 30, QFont::Bold);
        painter->setFont(overlayFont);
        painter->setPen(Qt::white);

        QFontMetricsF metrics(overlayFont);
        const QRectF bounds = metrics.boundingRect(QRectF(0, 0, SCENE_W, SCENE_H),
                                                   Qt::AlignCenter,
                                                   m_overlayText);
        const QRectF target((SCENE_W - bounds.width()) / 2.f,
                            (SCENE_H - bounds.height()) / 2.5f,
                            bounds.width(),
                            bounds.height());
        painter->drawText(target, Qt::AlignCenter, m_overlayText);
    }

    if (m_state == GameState::HighScoreEntry)
        drawHighScoreEntry(painter);
    else if (m_state == GameState::Attract)
        drawTopScores(painter, SCENE_W - 360.f, 76.f, 5);
    else if (m_state == GameState::GameOver)
        drawTopScores(painter, SCENE_W - 360.f, 76.f, 5);

    // Diagnostics overlay (F1) — shown for 10 seconds, fades last 2 seconds
    if (m_diagTimer > 0.f && !m_diagText.isEmpty()) {
        const float alpha = qBound(0.f, m_diagTimer / 2.f, 1.f);
        const QStringList lines = m_diagText.split('\n');
        const float lineH = 18.f;
        const float padX  = 12.f, padY = 10.f;
        const float boxW  = 560.f;
        const float boxH  = padY * 2.f + lines.size() * lineH;

        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0, 0, 0, static_cast<int>(180 * alpha)));
        painter->drawRoundedRect(QRectF(8.f, 8.f, boxW, boxH), 6.f, 6.f);

        painter->setFont(QFont("Courier New", 11));
        for (int i = 0; i < lines.size(); ++i) {
            const QString &line = lines[i];
            const bool isHeader = line.startsWith("===");
            painter->setPen(QColor(isHeader ? 100 : 200,
                                   isHeader ? 255 : 240,
                                   isHeader ? 200 : 200,
                                   static_cast<int>(220 * alpha)));
            painter->drawText(QPointF(8.f + padX, 8.f + padY + (i + 1) * lineH), line);
        }
    }

    painter->restore();
}

void GameScene::drawTopScores(QPainter *painter, float x, float y, int maxRows) const
{
    painter->save();

    QFont titleFont("Courier New", 18, QFont::Bold);
    painter->setFont(titleFont);
    painter->setPen(QColor(110, 255, 210, 230));
    painter->drawText(QPointF(x, y), "TOP SCORES");

    QFont rowFont("Courier New", 15, QFont::Bold);
    painter->setFont(rowFont);
    painter->setPen(QColor(225, 245, 255, 215));

    const QList<HighScoreManager::Entry> &entries = m_highScores.entries();
    const int rows = qMin(maxRows, entries.size());
    for (int i = 0; i < rows; ++i) {
        const auto &entry = entries[i];
        const QString row = QString("%1. %2  %3")
            .arg(i + 1, 2)
            .arg(entry.name.leftJustified(3, ' '))
            .arg(entry.score, 5);
        painter->drawText(QPointF(x, y + 32.f + i * 25.f), row);
    }

    painter->restore();
}

void GameScene::drawMiniMap(QPainter *painter) const
{
    constexpr float MW = 206.f;
    constexpr float MH = 154.f;
    constexpr float MX = SCENE_W - MW - 38.f;
    constexpr float MY = SCENE_H - MH - 34.f;

    constexpr float Z_BEHIND = 260.f;
    constexpr float Z_AHEAD  = 2000.f;
    constexpr int   SAMPLES  = 72;

    const float zMin = m_player.z - Z_BEHIND;
    const float zMax = qMin(m_player.z + Z_AHEAD, m_tunnelPath.totalLength());
    const float STEP = qMax(1.f, (zMax - zMin) / SAMPLES);

    float xMin = 0.f, xMax = 0.f;
    float yMin = 0.f, yMax = 0.f;
    bool firstSample = true;
    for (int i = 0; i <= SAMPLES; ++i) {
        const QPointF c = m_tunnelPath.sample(zMin + i * STEP).center;
        if (firstSample) {
            xMin = xMax = c.x();
            yMin = yMax = c.y();
            firstSample = false;
        } else {
            xMin = qMin(xMin, c.x());
            xMax = qMax(xMax, c.x());
            yMin = qMin(yMin, c.y());
            yMax = qMax(yMax, c.y());
        }
    }

    const float xPad = qMax(40.f, (xMax - xMin) * 0.22f);
    const float yPad = qMax(32.f, (yMax - yMin) * 0.30f);
    xMin -= xPad; xMax += xPad;
    yMin -= yPad; yMax += yPad;

    const float xRange = qMax(1.f, xMax - xMin);
    const float yRange = qMax(1.f, yMax - yMin);
    const float zRange = qMax(1.f, zMax - zMin);

    const float isoX = 0.46f;
    const float isoY = 0.22f;
    const float isoZ = 0.78f;

    auto project3D = [&](float wx, float wy, float wz) {
        const float nx = ((wx - xMin) / xRange - 0.5f) * 2.f;
        const float ny = ((wy - yMin) / yRange - 0.5f) * 2.f;
        const float nz = ((wz - zMin) / zRange - 0.5f) * 2.f;
        const float px = MX + MW * 0.5f + (nx - nz * isoX) * (MW * 0.34f);
        const float py = MY + MH * 0.72f + ny * isoZ * (MH * 0.34f)
                       - nz * isoY * (MH * 0.34f);
        return QPointF(px, py);
    };

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    // Background panel.
    painter->setPen(QPen(QColor(40, 140, 160, 200), 1.5f));
    painter->setBrush(QColor(4, 10, 18, 210));
    painter->drawRoundedRect(QRectF(MX - 4, MY - 4, MW + 8, MH + 8), 6.f, 6.f);

    painter->setClipRect(QRectF(MX, MY, MW, MH));

    const QPointF backTopLeft     = project3D(xMin, yMax, zMin);
    const QPointF backTopRight    = project3D(xMax, yMax, zMin);
    const QPointF backBottomRight = project3D(xMax, yMin, zMin);
    const QPointF backBottomLeft  = project3D(xMin, yMin, zMin);
    const QPointF frontTopLeft     = project3D(xMin, yMax, zMax);
    const QPointF frontTopRight    = project3D(xMax, yMax, zMax);
    const QPointF frontBottomRight = project3D(xMax, yMin, zMax);
    const QPointF frontBottomLeft  = project3D(xMin, yMin, zMax);

    painter->setPen(QPen(QColor(55, 95, 110, 120), 1.f));
    painter->setBrush(Qt::NoBrush);
    painter->drawPolygon(QPolygonF() << backTopLeft << backTopRight << backBottomRight << backBottomLeft);
    painter->drawPolygon(QPolygonF() << frontTopLeft << frontTopRight << frontBottomRight << frontBottomLeft);
    painter->drawLine(backTopLeft, frontTopLeft);
    painter->drawLine(backTopRight, frontTopRight);
    painter->drawLine(backBottomRight, frontBottomRight);
    painter->drawLine(backBottomLeft, frontBottomLeft);

    QPainterPath path;
    bool first = true;
    for (int i = 0; i <= SAMPLES; ++i) {
        const float wz = zMin + i * STEP;
        const QPointF c = m_tunnelPath.sample(wz).center;
        const QPointF pt = project3D(c.x(), c.y(), wz);
        if (first) {
            path.moveTo(pt);
            first = false;
        } else {
            path.lineTo(pt);
        }
    }

    painter->setPen(QPen(QColor(90, 220, 220, 215), 2.f));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path);

    // Player position
    const QPointF playerCenter = m_tunnelPath.sample(m_player.z).center;
    const QPointF playerDot = project3D(playerCenter.x(),
                                        playerCenter.y(),
                                        m_player.z);
    painter->setPen(Qt::NoPen);
    QRadialGradient playerGlow(playerDot, 10.f);
    playerGlow.setColorAt(0.0, QColor(100, 255, 180, 210));
    playerGlow.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter->setBrush(playerGlow);
    painter->drawEllipse(playerDot, 10.f, 10.f);
    painter->setBrush(QColor(160, 255, 210, 255));
    painter->drawEllipse(playerDot, 4.f, 4.f);

    // Gem dots
    for (int i = 0; i < m_chaseGems.size(); ++i) {
        const ChaseGem &gem = m_chaseGems[i];
        if (gem.collected) continue;
        if (gem.z < zMin || gem.z > zMax) continue;

        const QPointF gemCenter = m_tunnelPath.sample(gem.z).center;
        const QPointF gemDot = project3D(gemCenter.x(),
                                         gemCenter.y(),
                                         gem.z);

        QRadialGradient gemGlow(gemDot, 6.f);
        gemGlow.setColorAt(0.0, QColor(gem.core.red(), gem.core.green(), gem.core.blue(), 200));
        gemGlow.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter->setBrush(gemGlow);
        painter->drawEllipse(gemDot, 6.f, 6.f);
        painter->setBrush(gem.core.lighter(140));
        painter->drawEllipse(gemDot, 3.f, 3.f);
    }

    painter->setClipping(false);

    // Label
    painter->setPen(QColor(80, 200, 200, 220));
    painter->setFont(QFont("Courier New", 9, QFont::Bold));
    painter->drawText(QPointF(MX, MY - 5.f), "MAP 3D");

    painter->restore();
}

void GameScene::drawHighScoreEntry(QPainter *painter) const
{
    painter->save();

    const float centerX = SCENE_W * 0.5f;
    const float y = SCENE_H * 0.48f;
    const float spacing = 76.f;
    const float startX = centerX - spacing;

    QFont letterFont("Courier New", 46, QFont::Bold);
    painter->setFont(letterFont);
    painter->setPen(QColor(240, 255, 248));

    for (int i = 0; i < 3; ++i) {
        const float x = startX + spacing * i;
        QRectF letterRect(x - 30.f, y - 46.f, 60.f, 58.f);
        painter->drawText(letterRect, Qt::AlignCenter, QString(m_initials[i]));

        if (i == m_initialIndex) {
            painter->setPen(Qt::NoPen);
            QRadialGradient glow(x, y + 25.f, 32.f);
            glow.setColorAt(0.0, QColor(100, 255, 150, 115));
            glow.setColorAt(1.0, QColor(0, 0, 0, 0));
            painter->setBrush(glow);
            painter->drawEllipse(QPointF(x, y + 25.f), 34.f, 16.f);

            painter->setPen(QPen(QColor(100, 255, 150, 240), 4.f));
            painter->drawLine(QPointF(x - 22.f, y + 25.f), QPointF(x + 22.f, y + 25.f));
            painter->setPen(QColor(240, 255, 248));
        }
    }

    painter->restore();
}
