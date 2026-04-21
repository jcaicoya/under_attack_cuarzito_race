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
    initSparks();
    m_audio.startAmbient();

    startAttract();
}

// ---------------------------------------------------------------------------
// Initialisers
// ---------------------------------------------------------------------------

void GameScene::initSparks()
{
    auto *rng = QRandomGenerator::global();
    m_sparks.clear();
    for (int i = 0; i < 240; ++i) {
        Spark s;
        s.wx    = (rng->generateDouble() * 2.0 - 1.0) * 190.f;
        s.wy    = (rng->generateDouble() * 2.0 - 1.0) * 145.f;
        s.wz    = REMOVE_Z + rng->generateDouble() * (SPAWN_Z - REMOVE_Z);
        s.speed = 180.f + rng->generateDouble() * 420.f;
        m_sparks.append(s);
    }
}

void GameScene::advanceSparks(float dt, float speedMult)
{
    auto *rng = QRandomGenerator::global();
    for (auto &s : m_sparks) {
        s.wz -= s.speed * speedMult * dt;
        if (s.wz < REMOVE_Z) {
            s.wz    = SPAWN_Z * 0.75f + rng->generateDouble() * SPAWN_Z * 0.25f;
            s.wx    = (rng->generateDouble() * 2.0 - 1.0) * 190.f;
            s.wy    = (rng->generateDouble() * 2.0 - 1.0) * 145.f;
            s.speed = 180.f + rng->generateDouble() * 420.f;
        }
    }
}

// ---------------------------------------------------------------------------
// Vanishing point — drives the tunnel curve
// ---------------------------------------------------------------------------

void GameScene::updateVP(float dt)
{
    m_time += dt;
    float curveMag = qMin(195.f, 75.f + m_survivalTime * 2.0f);

    float targetVpX = CX + std::sin(m_time * 0.28f) * curveMag * 0.85f
                         + std::sin(m_time * 0.11f) * curveMag * 0.38f;
    float targetVpY = CY + std::sin(m_time * 0.20f + 1.2f) * curveMag * 0.62f
                         + std::sin(m_time * 0.08f + 0.5f) * curveMag * 0.28f;

    // VP moves toward target but capped so the player can always outrun the curve
    float cap = PLAYER_SPEED * 0.62f * dt;
    m_vpX += qBound(-cap, (targetVpX - m_vpX) * 2.8f * dt, cap);
    m_vpY += qBound(-cap, (targetVpY - m_vpY) * 2.8f * dt, cap);
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

void GameScene::render(QPainter *painter)
{
    painter->setRenderHint(QPainter::Antialiasing);

    drawSparks(painter);
    drawCollectibles(painter);
    drawObstacles(painter);

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

void GameScene::drawSparks(QPainter *painter) const
{
    for (const auto &s : m_sparks) {
        if (s.wz <= REMOVE_Z) continue;
        float px1   = projX(s.wx, s.wz);
        float py1   = projY(s.wy, s.wz);
        float tailZ = s.wz + s.speed * 0.035f;
        float px2   = projX(s.wx, tailZ);
        float py2   = projY(s.wy, tailZ);

        float t     = 1.f - s.wz / SPAWN_Z;
        int   alpha = static_cast<int>(t * 210.f + 25.f);
        float width = 0.8f + t * 1.2f;

        painter->setPen(QPen(QColor(255, 145, 20, qMin(alpha, 255)), width));
        painter->drawLine(QPointF(px1, py1), QPointF(px2, py2));
    }
}

void GameScene::drawObstacles(QPainter *painter) const
{
    QList<const Obstacle *> sorted;
    sorted.reserve(m_obstacles.size());
    for (const auto &o : m_obstacles) sorted.append(&o);
    std::sort(sorted.begin(), sorted.end(),
              [](const Obstacle *a, const Obstacle *b) { return a->wz > b->wz; });

    for (const Obstacle *obs : sorted) {
        float sc = projScale(obs->wz);
        float px = projX(obs->wx, obs->wz);
        float py = projY(obs->wy, obs->wz);
        float hw = obs->wHalfW * sc;
        float hh = obs->wHalfH * sc;

        float t     = 1.f - obs->wz / SPAWN_Z;
        int   alpha = static_cast<int>(35.f + t * 200.f);

        QPainterPath gem;
        gem.moveTo(px,      py - hh);
        gem.lineTo(px + hw, py - hh * 0.05f);
        gem.lineTo(px,      py + hh * 0.72f);
        gem.lineTo(px - hw, py - hh * 0.05f);
        gem.closeSubpath();

        QLinearGradient facet(px - hw, py - hh, px + hw, py + hh * 0.72f);
        facet.setColorAt(0.00, QColor(195, 210, 228, alpha));
        facet.setColorAt(0.40, QColor(100, 115, 138, alpha));
        facet.setColorAt(1.00, QColor( 32,  40,  55, alpha));

        painter->setBrush(facet);
        painter->setPen(QPen(QColor(80, 200, 220, qMin(alpha + 45, 255)), 1.4f));
        painter->drawPath(gem);

        QPainterPath hi;
        hi.moveTo(px,            py - hh);
        hi.lineTo(px + hw * 0.5f, py - hh * 0.45f);
        hi.lineTo(px,            py - hh * 0.05f);
        hi.lineTo(px - hw * 0.5f, py - hh * 0.45f);
        hi.closeSubpath();
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(240, 248, 255, static_cast<int>(t * 55.f + 10.f)));
        painter->drawPath(hi);
    }
}

void GameScene::drawCollectibles(QPainter *painter) const
{
    QList<const Collectible *> sorted;
    sorted.reserve(m_collectibles.size());
    for (const auto &c : m_collectibles) sorted.append(&c);
    std::sort(sorted.begin(), sorted.end(),
              [](const Collectible *a, const Collectible *b) { return a->wz > b->wz; });

    for (const Collectible *c : sorted) {
        float sc = projScale(c->wz);
        float px = projX(c->wx, c->wz);
        float py = projY(c->wy, c->wz);
        float r  = c->wRadius * sc;

        float t     = 1.f - c->wz / SPAWN_Z;
        int   alpha = static_cast<int>(30.f + t * 210.f);

        QColor glowCol  = c->special ? QColor(255, 140,  20) : QColor( 40, 220,  80);
        QColor coreCol  = c->special ? QColor(255, 170,  50, alpha) : QColor( 60, 230,  95, alpha);
        QColor darkCol  = c->special ? QColor(160,  70,  10, alpha) : QColor( 10, 120,  40, alpha);
        QColor lightCol = c->special ? QColor(255, 230, 140, alpha) : QColor(190, 255, 190, alpha);
        QColor edgeCol  = c->special ? QColor(255, 200,  80, qMin(alpha + 60, 255))
                                     : QColor( 80, 255, 120, qMin(alpha + 60, 255));

        QRadialGradient glow(px, py, r * 2.2f);
        glow.setColorAt(0.0, QColor(glowCol.red(), glowCol.green(), glowCol.blue(),
                                    static_cast<int>(t * 65.f)));
        glow.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter->setPen(Qt::NoPen);
        painter->setBrush(glow);
        painter->drawEllipse(QPointF(px, py), r * 2.2f, r * 2.2f);

        QPainterPath gem;
        gem.moveTo(px,     py - r);
        gem.lineTo(px + r, py);
        gem.lineTo(px,     py + r * 0.72f);
        gem.lineTo(px - r, py);
        gem.closeSubpath();

        QLinearGradient facet(px - r, py - r, px + r, py + r * 0.72f);
        facet.setColorAt(0.0, lightCol);
        facet.setColorAt(0.5, coreCol);
        facet.setColorAt(1.0, darkCol);

        painter->setBrush(facet);
        painter->setPen(QPen(edgeCol, 1.2f));
        painter->drawPath(gem);

        QPainterPath hi;
        hi.moveTo(px,            py - r);
        hi.lineTo(px + r * 0.5f, py - r * 0.4f);
        hi.lineTo(px,            py - r * 0.05f);
        hi.lineTo(px - r * 0.5f, py - r * 0.4f);
        hi.closeSubpath();
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(255, 255, 255, static_cast<int>(t * 60.f + 10.f)));
        painter->drawPath(hi);
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

    advanceSparks(dt, 0.6f);
    setOverlay(attractOverlayText());
    if (m_input.isConfirmJustPressed())
        startCountdown();
}

void GameScene::updateCountdown(float dt)
{
    m_countdownTimer -= dt;
    m_time += dt;

    m_vpX = CX + std::sin(m_time * 0.18f) * 70.f;
    m_vpY = CY + std::sin(m_time * 0.13f + 1.0f) * 50.f;
    advanceSparks(dt, 0.75f);

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
    // --- Tunnel curve: move the vanishing point ---
    updateVP(dt);

    // --- Player movement (relative to VP) ---
    m_player.offX -= m_input.isMovingLeft()  ? PLAYER_SPEED * dt : 0.f;
    m_player.offX += m_input.isMovingRight() ? PLAYER_SPEED * dt : 0.f;
    m_player.offY -= m_input.isMovingUp()    ? PLAYER_SPEED * dt : 0.f;
    m_player.offY += m_input.isMovingDown()  ? PLAYER_SPEED * dt : 0.f;

    // --- Wall collision ---
    if (std::abs(m_player.offX) > TUNNEL_HALF_W ||
        std::abs(m_player.offY) > TUNNEL_HALF_H) {
        endGame();
        return;
    }

    advanceSparks(dt);

    // --- Advance obstacles ---
    for (auto &obs : m_obstacles)
        obs.wz -= m_worldSpeed * dt;
    m_obstacles.removeIf([](const Obstacle &o) { return o.wz < REMOVE_Z; });

    // --- Advance collectibles ---
    for (auto &c : m_collectibles)
        c.wz -= m_worldSpeed * dt;
    m_collectibles.removeIf([](const Collectible &c) { return c.wz < REMOVE_Z; });

    // --- Obstacle collision ---
    const float sx = playerSX(), sy = playerSY();
    for (const auto &obs : m_obstacles) {
        if (obs.wz > COLLIDE_Z) continue;
        float sc = projScale(obs.wz);
        float px = projX(obs.wx, obs.wz);
        float py = projY(obs.wy, obs.wz);
        QRectF obsRect(px - obs.wHalfW * sc, py - obs.wHalfH * sc,
                       obs.wHalfW * sc * 2.f, obs.wHalfH * sc * 2.f);
        QRectF playerRect(sx - 15.f, sy - 15.f, 30.f, 30.f);
        if (obsRect.intersects(playerRect)) {
            endGame();
            return;
        }
    }

    // --- Collectible pickup ---
    m_collectibles.removeIf([&](const Collectible &c) {
        float sc = projScale(c.wz);
        float px = projX(c.wx, c.wz);
        float py = projY(c.wy, c.wz);
        float pr = c.wRadius * sc;
        float dx = px - sx, dy = py - sy;
        float threshold = pr + 15.f * 1.8f;
        if (dx * dx + dy * dy < threshold * threshold) {
            m_score += c.value;
            m_audio.play(c.special ? SoundCue::CollectSpecial : SoundCue::Collect);
            m_revealDuration = c.special ? 0.52f : 0.34f;
            m_revealTimer = m_revealDuration;
            spawnBurst(px, py, c.special);
            m_popups.append({px, py, c.value, 1.0f});
            return true;
        }
        return false;
    });

    // --- Spawn ---
    m_spawnTimer -= dt;
    if (m_spawnTimer <= 0.f) { spawnObstacle();    m_spawnTimer   = m_spawnInterval;   }
    m_collectTimer -= dt;
    if (m_collectTimer <= 0.f) { spawnCollectible(); m_collectTimer = m_collectInterval; }

    // --- Score and difficulty ramp ---
    m_survivalTime   += dt;
    m_score          += dt;
    m_worldSpeed      = qMin(640.f, 220.f + m_survivalTime * 16.f);
    m_spawnInterval   = qMax(0.45f, 2.0f  - m_survivalTime * 0.055f);
    m_collectInterval = qMax(0.55f, 1.0f  - m_survivalTime * 0.008f);

    // --- Advance popups ---
    for (auto &pop : m_popups) pop.life -= dt * 1.6f;
    m_popups.removeIf([](const ScorePopup &p) { return p.life <= 0.f; });

    updateHUD();
}

void GameScene::updateGameOver(float dt)
{
    m_gameOverTimer -= dt;
    m_gameOverIdleTimer -= dt;
    m_time += dt;
    m_vpX = CX + std::sin(m_time * 0.18f) * 90.f;
    m_vpY = CY + std::sin(m_time * 0.13f + 1.0f) * 65.f;
    advanceSparks(dt, 0.3f);
    if (m_input.isJustPressed(Action::Cancel))
        startAttract();
    else if (m_gameOverTimer <= 0.f && m_input.isConfirmJustPressed())
        startCountdown();
    else if (m_gameOverIdleTimer <= 0.f)
        startAttract();
}

void GameScene::updateHighScoreEntry(float dt)
{
    m_time += dt;
    m_vpX = CX + std::sin(m_time * 0.18f) * 70.f;
    m_vpY = CY + std::sin(m_time * 0.13f + 1.0f) * 50.f;
    advanceSparks(dt, 0.45f);

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
    m_obstacles.clear();
    m_collectibles.clear();
    m_popups.clear();
    m_bursts.clear();
    m_player = Player{};

    m_vpX  = CX;
    m_vpY  = CY;
    m_time = 0.f;

    m_worldSpeed      = 220.f;
    m_spawnTimer      = 1.5f;
    m_spawnInterval   = 2.0f;
    m_collectTimer    = 1.0f;
    m_collectInterval = 1.0f;
    m_survivalTime    = 0.f;
    m_score           = 0.f;
    m_gameOverTimer   = 0.f;
    m_gameOverIdleTimer = 0.f;
    m_countdownTimer  = 0.f;
    m_revealTimer     = 0.f;
    m_revealDuration  = 0.f;
    m_impactFlash     = 0.f;
    m_scoreSubmitted  = false;
    m_pendingScore    = 0;
    m_initialIndex    = 0;
    m_state           = GameState::Playing;

    m_hudText.clear();
    m_overlayText.clear();
}

void GameScene::startAttract()
{
    m_obstacles.clear();
    m_collectibles.clear();
    m_popups.clear();
    m_bursts.clear();
    m_player = Player{};

    m_vpX = CX;
    m_vpY = CY;
    m_time = 0.f;

    m_worldSpeed      = 220.f;
    m_spawnTimer      = 1.5f;
    m_spawnInterval   = 2.0f;
    m_collectTimer    = 1.0f;
    m_collectInterval = 1.0f;
    m_survivalTime    = 0.f;
    m_score           = 0.f;
    m_gameOverTimer   = 0.f;
    m_gameOverIdleTimer = 0.f;
    m_countdownTimer  = 0.f;
    m_revealTimer     = 0.f;
    m_revealDuration  = 0.f;
    m_impactFlash     = 0.f;
    m_scoreSubmitted  = false;
    m_pendingScore    = 0;
    m_initialIndex    = 0;
    m_initials        = "AAA";
    m_state           = GameState::Attract;

    m_hudText.clear();
    setOverlay(attractOverlayText());
}

void GameScene::startCountdown()
{
    m_obstacles.clear();
    m_collectibles.clear();
    m_popups.clear();
    m_bursts.clear();
    m_player = Player{};

    m_vpX  = CX;
    m_vpY  = CY;
    m_time = 0.f;

    m_worldSpeed      = 220.f;
    m_spawnTimer      = 1.5f;
    m_spawnInterval   = 2.0f;
    m_collectTimer    = 1.0f;
    m_collectInterval = 1.0f;
    m_survivalTime    = 0.f;
    m_score           = 0.f;
    m_gameOverTimer   = 0.f;
    m_gameOverIdleTimer = 0.f;
    m_countdownTimer  = 3.0f;
    m_revealTimer     = 0.f;
    m_revealDuration  = 0.f;
    m_impactFlash     = 0.f;
    m_scoreSubmitted  = false;
    m_pendingScore    = 0;
    m_initialIndex    = 0;
    m_state           = GameState::Countdown;

    m_hudText.clear();
    setOverlay("3");
    m_audio.play(SoundCue::Start);
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

    setOverlay(QString("GAME OVER\n\nScore: %1    Time: %2s\n\nPRESS SPACE TO RESTART")
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

void GameScene::spawnObstacle()
{
    auto *rng = QRandomGenerator::global();
    Obstacle obs;
    obs.wx     = (rng->generateDouble() * 2.0 - 1.0) * 200.f;
    obs.wy     = (rng->generateDouble() * 2.0 - 1.0) * 150.f;
    obs.wz     = SPAWN_Z;
    obs.wHalfW = 28.f + rng->generateDouble() * 62.f;
    obs.wHalfH = 18.f + rng->generateDouble() * 48.f;
    m_obstacles.append(obs);
}

void GameScene::spawnCollectible()
{
    auto *rng = QRandomGenerator::global();
    Collectible c;
    c.wx     = (rng->generateDouble() * 2.0 - 1.0) * 185.f;
    c.wy     = (rng->generateDouble() * 2.0 - 1.0) * 140.f;
    c.wz     = SPAWN_Z;
    c.wRadius = 14.f + rng->generateDouble() * 8.f;
    c.special = rng->generateDouble() < 0.25;
    c.value   = c.special ? 25 : 10;
    m_collectibles.append(c);
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
    m_hudText =
        QString("SCORE  %1    TIME  %2s")
            .arg(static_cast<int>(m_score), 5)
            .arg(static_cast<int>(m_survivalTime));
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
