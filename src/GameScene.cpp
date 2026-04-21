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

    setOverlay("CUARZITO\n\nPRESS SPACE TO START");
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

    drawPopups(painter);
    drawHUD(painter);
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
    const float cy = playerSY();
    constexpr float W = 46.f, H = 58.f;

    // Blue electric aura
    QRadialGradient aura(cx, cy + H * 0.05f, W * 1.1f);
    aura.setColorAt(0.0, QColor( 0,  0,  0,   0));
    aura.setColorAt(0.5, QColor( 0, 60, 200,  18));
    aura.setColorAt(0.8, QColor( 0, 80, 220,  38));
    aura.setColorAt(1.0, QColor( 0,  0,  0,   0));
    painter->setPen(Qt::NoPen);
    painter->setBrush(aura);
    painter->drawEllipse(QPointF(cx, cy + H * 0.05f), W * 1.1f, W * 0.9f);

    // Dark hooded cloak
    QPainterPath cloak;
    cloak.moveTo(cx,             cy - H * 0.50f);
    cloak.cubicTo(cx + W * 0.22f, cy - H * 0.44f,
                  cx + W * 0.50f, cy - H * 0.22f,
                  cx + W * 0.52f, cy + H * 0.08f);
    cloak.cubicTo(cx + W * 0.56f, cy + H * 0.32f,
                  cx + W * 0.42f, cy + H * 0.46f,
                  cx,             cy + H * 0.50f);
    cloak.cubicTo(cx - W * 0.42f, cy + H * 0.46f,
                  cx - W * 0.56f, cy + H * 0.32f,
                  cx - W * 0.52f, cy + H * 0.08f);
    cloak.cubicTo(cx - W * 0.50f, cy - H * 0.22f,
                  cx - W * 0.22f, cy - H * 0.44f,
                  cx,             cy - H * 0.50f);
    cloak.closeSubpath();

    QRadialGradient bodyGrad(cx, cy, W * 0.7f);
    bodyGrad.setColorAt(0.0, QColor(28, 22, 40));
    bodyGrad.setColorAt(1.0, QColor( 6,  4, 10));

    painter->setBrush(bodyGrad);
    painter->setPen(QPen(QColor(50, 40, 70, 180), 1.0f));
    painter->drawPath(cloak);

    // Neon green visor
    float visorY  = cy - H * 0.12f;
    float visorHW = W * 0.30f;
    float visorH  = 3.8f;

    QLinearGradient visorGlow(cx - visorHW * 1.6f, visorY,
                              cx + visorHW * 1.6f, visorY);
    visorGlow.setColorAt(0.0, QColor( 0, 255,  80,   0));
    visorGlow.setColorAt(0.3, QColor( 0, 255,  80,  70));
    visorGlow.setColorAt(0.5, QColor(80, 255, 120, 100));
    visorGlow.setColorAt(0.7, QColor( 0, 255,  80,  70));
    visorGlow.setColorAt(1.0, QColor( 0, 255,  80,   0));
    painter->setBrush(visorGlow);
    painter->drawRoundedRect(
        QRectF(cx - visorHW * 1.6f, visorY - 7.f, visorHW * 3.2f, 14.f), 7.f, 7.f);

    painter->setBrush(QColor(100, 255, 130, 255));
    painter->drawRoundedRect(
        QRectF(cx - visorHW, visorY - visorH * 0.5f, visorHW * 2.f, visorH), 2.f, 2.f);
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

// ---------------------------------------------------------------------------
// Game loop tick
// ---------------------------------------------------------------------------

void GameScene::update(float dt)
{
    switch (m_state) {
    case GameState::Attract:  updateAttract(dt);  break;
    case GameState::Playing:  updatePlaying(dt);  break;
    case GameState::GameOver: updateGameOver(dt); break;
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
    if (m_input.isConfirmJustPressed())
        startGame();
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
    m_time += dt;
    m_vpX = CX + std::sin(m_time * 0.18f) * 90.f;
    m_vpY = CY + std::sin(m_time * 0.13f + 1.0f) * 65.f;
    advanceSparks(dt, 0.3f);
    if (m_gameOverTimer <= 0.f && m_input.isConfirmJustPressed())
        startGame();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void GameScene::startGame()
{
    m_obstacles.clear();
    m_collectibles.clear();
    m_popups.clear();
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
    m_state           = GameState::Playing;

    m_hudText.clear();
    m_overlayText.clear();
}

void GameScene::endGame()
{
    m_state         = GameState::GameOver;
    m_gameOverTimer = 1.5f;

    setOverlay(QString("GAME OVER\n\nScore: %1    Time: %2s\n\nPRESS SPACE TO RESTART")
                   .arg(static_cast<int>(m_score))
                   .arg(static_cast<int>(m_survivalTime)));
    m_hudText.clear();
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

void GameScene::setOverlay(const QString &text)
{
    m_overlayText = text;
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

    painter->restore();
}
