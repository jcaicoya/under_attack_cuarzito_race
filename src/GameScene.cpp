#define _USE_MATH_DEFINES
#include "GameScene.h"
#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <QFontMetricsF>
#include <QRandomGenerator>
#include <QRadialGradient>
#include <QLinearGradient>
#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

GameScene::GameScene(QObject *parent) : QObject(parent)
{
    m_audio.startAmbient();

    startAttract();
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

void GameScene::render(QPainter *painter)
{
    painter->setRenderHint(QPainter::Antialiasing);

    drawChaseGems(painter);

    if (m_state != GameState::Attract)
        drawPlayer(painter);

    drawBursts(painter);
    drawPopups(painter);
    drawImpactFlash(painter);
    drawHUD(painter);
}

float GameScene::playerLean() const
{
    if (m_input.isMovingLeft() == m_input.isMovingRight())
        return 0.f;
    return m_input.isMovingLeft() ? -1.f : 1.f;
}

float GameScene::turnOcclusion() const
{
    return m_tunnelPath.sample(m_player.z + 180.f).occlusion;
}

CaveRenderer::Mode GameScene::caveMode() const
{
    return m_state == GameState::Playing
        ? CaveRenderer::Mode::EnclosedTunnel
        : CaveRenderer::Mode::OpenMouth;
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
        if (m_state == GameState::Intro || m_state == GameState::Countdown) {
            ahead = 280.f + ref.index * 78.f;
            relativeCenter *= 0.35f;
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
}

void GameScene::drawPlayer(QPainter *painter) const
{
    const float cx = playerSX();
    const float bob = std::sin(m_time * 7.5f) * 2.2f;
    const float cy = playerSY() + bob;
    const float lean = playerLean();
    const float reveal = m_revealDuration > 0.f
        ? qBound(0.f, m_revealTimer / m_revealDuration, 1.f)
        : 0.f;
    constexpr float W = 50.f, H = 62.f;

    // Blue electric aura
    QRadialGradient aura(cx - lean * 2.f, cy + H * 0.05f, W * 1.45f);
    aura.setColorAt(0.0, QColor(  0,  30, 120,  16));
    aura.setColorAt(0.45, QColor(  0,  85, 215,  45));
    aura.setColorAt(0.72, QColor( 20, 175, 230,  34));
    aura.setColorAt(1.0, QColor(  0,   0,   0,   0));
    painter->setPen(Qt::NoPen);
    painter->setBrush(aura);
    painter->drawEllipse(QPointF(cx - lean * 2.f, cy + H * 0.05f), W * 1.45f, W * 1.05f);

    // Rear-view hooded cloak: Cuarzito is flying away from the camera.
    QPainterPath cloak;
    cloak.moveTo(cx + lean * 3.f, cy - H * 0.51f);
    cloak.cubicTo(cx + W * 0.35f + lean * 5.f, cy - H * 0.47f,
                  cx + W * 0.55f + lean * 6.f, cy - H * 0.22f,
                  cx + W * 0.47f + lean * 5.f, cy + H * 0.08f);
    cloak.cubicTo(cx + W * 0.55f + lean * 3.f, cy + H * 0.32f,
                  cx + W * 0.38f,              cy + H * 0.48f,
                  cx + lean * 3.f,             cy + H * 0.52f);
    cloak.cubicTo(cx - W * 0.38f,              cy + H * 0.48f,
                  cx - W * 0.55f + lean * 3.f, cy + H * 0.32f,
                  cx - W * 0.47f + lean * 5.f, cy + H * 0.08f);
    cloak.cubicTo(cx - W * 0.55f + lean * 6.f, cy - H * 0.22f,
                  cx - W * 0.35f + lean * 5.f, cy - H * 0.47f,
                  cx + lean * 3.f,             cy - H * 0.51f);
    cloak.closeSubpath();

    QRadialGradient bodyGrad(cx - lean * 4.f, cy - H * 0.04f, W * 0.86f);
    bodyGrad.setColorAt(0.0, QColor(24, 25, 34));
    bodyGrad.setColorAt(0.45, QColor(10, 10, 18));
    bodyGrad.setColorAt(1.0, QColor(3, 3, 8));

    painter->setBrush(bodyGrad);
    painter->setPen(QPen(QColor(42, 48, 68, 160), 1.0f));
    painter->drawPath(cloak);

    QPainterPath hoodRidge;
    hoodRidge.moveTo(cx - W * 0.27f + lean * 4.f, cy - H * 0.26f);
    hoodRidge.cubicTo(cx - W * 0.12f + lean * 2.f, cy - H * 0.36f,
                      cx + W * 0.12f + lean * 2.f, cy - H * 0.36f,
                      cx + W * 0.27f + lean * 4.f, cy - H * 0.26f);
    painter->setPen(QPen(QColor(64, 72, 96, 92), 1.3f));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(hoodRidge);

    if (reveal > 0.f) {
        drawVisorReveal(painter, cx, cy, W, H, reveal);
    } else if (std::abs(lean) > 0.01f) {
        const float side = lean > 0.f ? 1.f : -1.f;
        const float visorX = cx + side * W * 0.31f;
        const float visorY = cy - H * 0.19f;

        QRadialGradient sideGlow(visorX, visorY, 15.f);
        sideGlow.setColorAt(0.0, QColor(80, 255, 120, 125));
        sideGlow.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter->setPen(Qt::NoPen);
        painter->setBrush(sideGlow);
        painter->drawEllipse(QPointF(visorX, visorY), 15.f, 10.f);

        painter->setBrush(QColor(100, 255, 130, 230));
        painter->drawRoundedRect(QRectF(visorX - side * 2.f - (side > 0.f ? 2.f : 9.f),
                                        visorY - 2.f,
                                        11.f,
                                        3.4f),
                                 1.7f,
                                 1.7f);
    }
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

    QColor wash(110, 255, 210, static_cast<int>(t * 42.f));
    painter->setBrush(wash);
    painter->drawRect(QRectF(0, 0, SCENE_W, SCENE_H));

    QRadialGradient shock(playerSX(), playerSY(), 170.f * (1.15f - t * 0.15f));
    shock.setColorAt(0.00, QColor(120, 255, 180, static_cast<int>(t * 110.f)));
    shock.setColorAt(0.25, QColor(60, 180, 230, static_cast<int>(t * 65.f)));
    shock.setColorAt(1.00, QColor(0, 0, 0, 0));
    painter->setBrush(shock);
    painter->drawEllipse(QPointF(playerSX(), playerSY()), 190.f, 150.f);

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
    case GameState::GameOver: updateGameOver(dt); break;
    case GameState::HighScoreEntry:updateHighScoreEntry(dt);break;
    }

    m_input.endFrame();
}

void GameScene::updateAttract(float dt)
{
    // Gentle VP drift so the tunnel feels alive on the attract screen
    m_time += dt;
    m_vpX = CX + std::sin(m_time * 0.18f) * 90.f;
    m_vpY = CY + std::sin(m_time * 0.13f + 1.0f) * 65.f;

    setOverlay(attractOverlayText());
    if (m_input.isConfirmJustPressed())
        startIntro();
}

void GameScene::updateIntro(float dt)
{
    m_introTimer -= dt;
    m_time += dt;

    const float t = qBound(0.f, 1.f - m_introTimer / 2.8f, 1.f);
    m_vpX = CX + std::sin(m_time * 0.42f) * (55.f + t * 35.f);
    m_vpY = CY + std::sin(m_time * 0.31f + 1.0f) * (34.f + t * 22.f);

    setOverlay("THE GEMS ESCAPE\n\nGET READY");
    if (m_input.isConfirmJustPressed() || m_introTimer <= 0.f)
        startCountdown();
}

void GameScene::updateCountdown(float dt)
{
    m_countdownTimer -= dt;
    m_time += dt;

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
            m_chaseTimer += 20.f;
            m_score += gem.value + qMax(0.f, m_chaseTimer) * 18.f + m_cleanFlightTime * 8.f;
            m_audio.play(SoundCue::CollectSpecial);
            m_revealDuration = 0.52f;
            m_revealTimer = m_revealDuration;
            spawnBurst(playerSX(), playerSY() - 22.f, true);
            m_popups.append({playerSX(), playerSY() - 42.f, gem.value, 1.0f});
            m_cleanFlightTime += 3.f;
        }
    }

    const bool allGemsCollected = std::all_of(m_chaseGems.cbegin(), m_chaseGems.cend(),
                                             [](const ChaseGem &gem) { return gem.collected; });
    if (allGemsCollected) {
        m_score += qMax(0.f, m_chaseTimer) * 75.f + m_cleanFlightTime * 45.f;
        m_runWon = true;
        endGame();
        return;
    }

    m_chaseTimer -= dt;
    if (m_chaseTimer <= 0.f) {
        endGame();
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

    const float steerSpeed = PLAYER_SPEED * (0.72f + m_player.speed / CHASE_MAX_SPEED * 0.42f);
    m_player.offX -= m_input.isMovingLeft()  ? steerSpeed * dt : 0.f;
    m_player.offX += m_input.isMovingRight() ? steerSpeed * dt : 0.f;
    m_player.offY -= m_input.isMovingUp()    ? steerSpeed * dt : 0.f;
    m_player.offY += m_input.isMovingDown()  ? steerSpeed * dt : 0.f;

    if (m_input.isAccelerating())
        m_player.speed += CHASE_ACCEL * dt;
    if (m_input.isBraking())
        m_player.speed -= CHASE_BRAKE * dt;
    if (!m_input.isAccelerating())
        m_player.speed -= CHASE_DRAG * dt;

    const TunnelPath::Sample current = m_tunnelPath.sample(m_player.z);
    const float safeX = current.innerRadius * 1.28f;
    const float safeY = current.innerRadius * 0.88f;
    const float nx = safeX > 0.f ? m_player.offX / safeX : 0.f;
    const float ny = safeY > 0.f ? m_player.offY / safeY : 0.f;
    const float wallDistance = std::sqrt(nx * nx + ny * ny);
    m_player.wallContact = wallDistance > 1.f;

    if (m_player.wallContact) {
        if (!m_wasWallContact) {
            ++m_wallHitCount;
            m_score = qMax(0.f, m_score - 45.f);
        }
        const float clampScale = 1.f / wallDistance;
        m_player.offX *= clampScale;
        m_player.offY *= clampScale;
        m_player.speed -= 260.f * dt;
        m_impactFlash = qMax(m_impactFlash, 0.18f);
    }
    m_wasWallContact = m_player.wallContact;

    m_player.speed = qBound(CHASE_MIN_SPEED, m_player.speed, CHASE_MAX_SPEED);
    m_player.z += m_player.speed * dt;
    m_worldSpeed = m_player.speed;

    const TunnelPath::Sample lookAhead = m_tunnelPath.sample(m_player.z + 230.f);
    const float targetVpX = CX + lookAhead.center.x() * 0.88f;
    const float targetVpY = CY + lookAhead.center.y() * 0.74f;
    m_vpX += (targetVpX - m_vpX) * qMin(1.f, dt * 4.4f);
    m_vpY += (targetVpY - m_vpY) * qMin(1.f, dt * 4.4f);
}

void GameScene::updateGameOver(float dt)
{
    m_gameOverTimer -= dt;
    m_gameOverIdleTimer -= dt;
    m_time += dt;
    m_vpX = CX + std::sin(m_time * 0.18f) * 90.f;
    m_vpY = CY + std::sin(m_time * 0.13f + 1.0f) * 65.f;
    if (m_input.isJustPressed(Action::Cancel))
        startAttract();
    else if (m_gameOverTimer <= 0.f && m_input.isConfirmJustPressed())
        startIntro();
    else if (m_gameOverIdleTimer <= 0.f)
        startAttract();
}

void GameScene::updateHighScoreEntry(float dt)
{
    m_time += dt;
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
    m_countdownTimer  = 0.f;
    m_chaseTimer      = 20.f;
    m_cleanFlightTime = 0.f;
    m_revealTimer     = 0.f;
    m_revealDuration  = 0.f;
    m_impactFlash     = 0.f;
    m_scoreSubmitted  = false;
    m_runWon          = false;
    m_wasWallContact  = false;
    m_wallHitCount    = 0;
    m_pendingScore    = 0;
    m_initialIndex    = 0;
    m_state           = GameState::Playing;

    m_hudText.clear();
    m_overlayText.clear();
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
    m_chaseTimer      = 20.f;
    m_cleanFlightTime = 0.f;
    m_revealTimer     = 0.f;
    m_revealDuration  = 0.f;
    m_impactFlash     = 0.f;
    m_scoreSubmitted  = false;
    m_runWon          = false;
    m_wasWallContact  = false;
    m_wallHitCount    = 0;
    m_pendingScore    = 0;
    m_initialIndex    = 0;
    m_initials        = "AAA";
    m_state           = GameState::Attract;

    m_hudText.clear();
    setOverlay(attractOverlayText());
}

void GameScene::startIntro()
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
    m_introTimer      = 2.8f;
    m_countdownTimer  = 0.f;
    m_chaseTimer      = 20.f;
    m_cleanFlightTime = 0.f;
    m_revealTimer     = 0.f;
    m_revealDuration  = 0.f;
    m_impactFlash     = 0.f;
    m_scoreSubmitted  = false;
    m_runWon          = false;
    m_wasWallContact  = false;
    m_wallHitCount    = 0;
    m_pendingScore    = 0;
    m_initialIndex    = 0;
    m_state           = GameState::Intro;

    m_hudText.clear();
    setOverlay("THE GEMS ESCAPE\n\nGET READY");
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
    m_chaseTimer      = 20.f;
    m_cleanFlightTime = 0.f;
    m_revealTimer     = 0.f;
    m_revealDuration  = 0.f;
    m_impactFlash     = 0.f;
    m_scoreSubmitted  = false;
    m_runWon          = false;
    m_wasWallContact  = false;
    m_wallHitCount    = 0;
    m_pendingScore    = 0;
    m_initialIndex    = 0;
    m_state           = GameState::Countdown;

    m_hudText.clear();
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
    m_audio.play(SoundCue::GameOver);

    const int finalScore = static_cast<int>(m_score);
    if (!m_scoreSubmitted && m_highScores.qualifies(finalScore)) {
        startHighScoreEntry(finalScore);
        return;
    }

    setOverlay(QString("%1\n\nScore: %2    Time: %3s\n\nPRESS SPACE TO RESTART")
                   .arg(m_runWon ? "YOU WIN" : "GAME OVER")
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

void GameScene::resetChaseGems()
{
    m_chaseGems = {
        {"BLUE",   QColor( 65, 155, 255), QColor(150, 220, 255),  520.f, 205.f, 23.f,  500, false},
        {"ORANGE", QColor(255, 135,  35), QColor(255, 215, 105),  860.f, 205.f, 25.f,  750, false},
        {"YELLOW", QColor(255, 230,  65), QColor(255, 250, 170), 1230.f, 205.f, 27.f, 1000, false},
        {"CYAN",   QColor( 65, 245, 230), QColor(180, 255, 245), 1640.f, 205.f, 30.f, 1500, false},
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

void GameScene::setOverlay(const QString &text)
{
    m_overlayText = text;
}

QString GameScene::attractOverlayText() const
{
    return "CUARZITO\n\nPRESS SPACE TO START";
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
        QString("SCORE  %1    TIMER  %2s    GEM  %3/4    NEXT  %4    SPEED  %5%6")
            .arg(static_cast<int>(m_score), 5)
            .arg(static_cast<int>(qMax(0.f, m_chaseTimer)))
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
