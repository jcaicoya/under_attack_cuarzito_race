#define _USE_MATH_DEFINES
#include "GameScene.h"
#include <QPainter>
#include <QGraphicsTextItem>
#include <QFont>
#include <QRandomGenerator>
#include <QRadialGradient>
#include <QLinearGradient>
#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

GameScene::GameScene(QObject *parent) : QGraphicsScene(parent)
{
    setSceneRect(0, 0, SCENE_W, SCENE_H);
    setBackgroundBrush(Qt::NoBrush);

    initSparks();
    initStars();

    m_hudText = new QGraphicsTextItem();
    m_hudText->setFont(QFont("Courier New", 18, QFont::Bold));
    m_hudText->setDefaultTextColor(QColor(100, 255, 200));
    m_hudText->setPos(20, 10);
    m_hudText->setZValue(20);
    addItem(m_hudText);

    m_overlayText = new QGraphicsTextItem();
    m_overlayText->setFont(QFont("Courier New", 30, QFont::Bold));
    m_overlayText->setDefaultTextColor(Qt::white);
    m_overlayText->setZValue(20);
    addItem(m_overlayText);
    setOverlay("CUARZITO\n\nPRESS SPACE TO START");

    m_clock.start();
    connect(&m_timer, &QTimer::timeout, this, &GameScene::tick);
    m_timer.start(16);
}

// ---------------------------------------------------------------------------
// Star initialisation (static — the sky visible through the cave opening)
// ---------------------------------------------------------------------------

void GameScene::initStars()
{
    auto *rng = QRandomGenerator::global();
    m_stars.clear();
    for (int i = 0; i < 85; ++i) {
        Star s;
        // Normally distributed around center-upper area (the cave opening)
        float angle  = static_cast<float>(rng->generateDouble() * 2.0 * M_PI);
        float radius = static_cast<float>(rng->generateDouble()) * 260.f;
        s.x      = CX + std::cos(angle) * radius * 1.3f;
        s.y      = (CY - 30.f) + std::sin(angle) * radius * 0.75f;
        s.r      = 0.4f + static_cast<float>(rng->generateDouble()) * 1.5f;
        s.bright = 0.25f + static_cast<float>(rng->generateDouble()) * 0.75f;
        m_stars.append(s);
    }
}

// ---------------------------------------------------------------------------
// Spark management
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
// Rendering — drawBackground paints everything; QGraphicsTextItems for HUD
// are normal scene items and render on top automatically.
// ---------------------------------------------------------------------------

void GameScene::drawBackground(QPainter *painter, const QRectF &)
{
    painter->setRenderHint(QPainter::Antialiasing);
    painter->fillRect(QRectF(0, 0, SCENE_W, SCENE_H), QColor(3, 5, 15));

    drawEnvironment(painter);
    drawSparks(painter);
    drawCollectibles(painter);
    drawObstacles(painter);

    if (m_state != GameState::Attract)
        drawPlayer(painter);

    drawPopups(painter);
}

void GameScene::drawEnvironment(QPainter *painter) const
{
    painter->setPen(Qt::NoPen);

    // --- Aurora / nebula behind the stars ---
    QRadialGradient aurora(CX + 40.f, CY * 0.42f, 310.f);
    aurora.setColorAt(0.00, QColor(20,  70, 110,  55));
    aurora.setColorAt(0.35, QColor(30,  50,  90,  30));
    aurora.setColorAt(0.65, QColor(55,  20,  90,  18));
    aurora.setColorAt(1.00, QColor(  0,  0,   0,   0));
    painter->setBrush(aurora);
    painter->drawEllipse(QPointF(CX + 40.f, CY * 0.42f), 310.f, 210.f);

    // --- Stars (static — sky through the cave opening) ---
    for (const auto &s : m_stars) {
        int alpha = static_cast<int>(s.bright * 200.f + 55.f);
        painter->setBrush(QColor(220, 230, 255, alpha));
        painter->drawEllipse(QPointF(s.x, s.y), s.r, s.r);
    }

    // --- Polaris — one brighter star with a small glow ---
    {
        constexpr float px = CX + 28.f, py = CY - 95.f;
        QRadialGradient starGlow(px, py, 12.f);
        starGlow.setColorAt(0.0, QColor(230, 240, 255, 180));
        starGlow.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter->setBrush(starGlow);
        painter->drawEllipse(QPointF(px, py), 12.f, 12.f);
        painter->setBrush(QColor(240, 248, 255, 240));
        painter->drawEllipse(QPointF(px, py), 2.2f, 2.2f);
    }

    // --- Low-poly cave rock edges at screen corners ---
    const QColor rockDeep(7, 4, 14, 255);
    const QColor rockMid(16, 10, 26, 255);
    painter->setBrush(rockDeep);

    painter->drawPolygon(QPolygonF({              // top-left
        QPointF(  0,   0), QPointF(400,   0),
        QPointF(310,  55), QPointF(210, 105),
        QPointF(110, 185), QPointF(  0, 270)}));

    painter->drawPolygon(QPolygonF({              // top-right
        QPointF(1280,   0), QPointF( 880,   0),
        QPointF( 970,  55), QPointF(1070, 105),
        QPointF(1170, 185), QPointF(1280, 270)}));

    painter->drawPolygon(QPolygonF({              // bottom-left
        QPointF(  0, 720), QPointF(320, 720),
        QPointF(210, 650), QPointF( 90, 565),
        QPointF(  0, 510)}));

    painter->drawPolygon(QPolygonF({              // bottom-right
        QPointF(1280, 720), QPointF( 960, 720),
        QPointF(1070, 650), QPointF(1190, 565),
        QPointF(1280, 510)}));

    // Lighter inner faces for low-poly shading illusion
    painter->setBrush(rockMid);
    painter->drawPolygon(QPolygonF({QPointF(310,  55), QPointF(400,   0), QPointF(210, 105)}));
    painter->drawPolygon(QPolygonF({QPointF(970,  55), QPointF(880,   0), QPointF(1070,105)}));
    painter->drawPolygon(QPolygonF({QPointF(210, 650), QPointF(320, 720), QPointF( 90, 565)}));
    painter->drawPolygon(QPolygonF({QPointF(1070,650), QPointF(960, 720), QPointF(1190,565)}));

    // --- Perspective convergence lines ---
    const QPointF vp(CX, CY);
    painter->setPen(QPen(QColor(15, 55, 40, 48), 1.0));
    for (float x = 0.f; x <= SCENE_W; x += 160.f) {
        painter->drawLine(vp, QPointF(x, 0.f));
        painter->drawLine(vp, QPointF(x, SCENE_H));
    }
    for (float y = 0.f; y <= SCENE_H; y += 120.f) {
        painter->drawLine(vp, QPointF(0.f, y));
        painter->drawLine(vp, QPointF(SCENE_W, y));
    }

    // --- Tunnel depth rings ---
    painter->setBrush(Qt::NoBrush);
    const float ringZs[] = { 700.f, 500.f, 340.f, 210.f, 120.f };
    for (float rz : ringZs) {
        float sc    = projScale(rz);
        float rw    = 210.f * sc;
        float rh    = 155.f * sc;
        float t     = 1.f - rz / SPAWN_Z;
        int   alpha = static_cast<int>(18.f + t * 62.f);
        painter->setPen(QPen(QColor(0, 170, 110, alpha), 1.0));
        painter->drawRect(QRectF(CX - rw, CY - rh, rw * 2.f, rh * 2.f));
    }

    // --- Vignette — blends rock edges smoothly into the playfield ---
    QRadialGradient vignette(CX, CY, 435.f);
    vignette.setColorAt(0.00, QColor(0, 0, 0,   0));
    vignette.setColorAt(0.50, QColor(0, 0, 0,   0));
    vignette.setColorAt(1.00, QColor(5, 2, 14, 235));
    painter->setPen(Qt::NoPen);
    painter->setBrush(vignette);
    painter->drawRect(QRectF(0, 0, SCENE_W, SCENE_H));
}

void GameScene::drawSparks(QPainter *painter) const
{
    for (const auto &s : m_sparks) {
        if (s.wz <= REMOVE_Z) continue;

        float px1  = projX(s.wx, s.wz);
        float py1  = projY(s.wy, s.wz);
        float tailZ = s.wz + s.speed * 0.035f;
        float px2  = projX(s.wx, tailZ);
        float py2  = projY(s.wy, tailZ);

        float t     = 1.f - s.wz / SPAWN_Z;
        int   alpha = static_cast<int>(t * 210.f + 25.f);
        float width = 0.8f + t * 1.2f;

        painter->setPen(QPen(QColor(255, 145, 20, qMin(alpha, 255)), width));
        painter->drawLine(QPointF(px1, py1), QPointF(px2, py2));
    }
}

void GameScene::drawObstacles(QPainter *painter) const
{
    // Back-to-front so nearer obstacles render on top of farther ones
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

        // Diamond / gem shape (4-point, slightly taller top half)
        QPainterPath gem;
        gem.moveTo(px,        py - hh);           // top
        gem.lineTo(px + hw,   py - hh * 0.05f);   // right
        gem.lineTo(px,        py + hh * 0.72f);   // bottom
        gem.lineTo(px - hw,   py - hh * 0.05f);   // left
        gem.closeSubpath();

        // Faceted gradient: bright top-left → dark bottom-right
        QLinearGradient facet(px - hw, py - hh, px + hw, py + hh * 0.72f);
        facet.setColorAt(0.00, QColor(195, 210, 228, alpha));
        facet.setColorAt(0.40, QColor(100, 115, 138, alpha));
        facet.setColorAt(1.00, QColor( 32,  40,  55, alpha));

        painter->setBrush(facet);
        painter->setPen(QPen(QColor(80, 200, 220, qMin(alpha + 45, 255)), 1.4f));
        painter->drawPath(gem);

        // Bright upper-left highlight facet
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

void GameScene::drawPlayer(QPainter *painter) const
{
    const float cx = m_player.sx;
    const float cy = m_player.sy;
    constexpr float W = 46.f, H = 58.f;

    // --- Blue electric aura ---
    QRadialGradient aura(cx, cy + H * 0.05f, W * 1.1f);
    aura.setColorAt(0.0, QColor( 0,  0,  0,   0));
    aura.setColorAt(0.5, QColor( 0, 60, 200,  18));
    aura.setColorAt(0.8, QColor( 0, 80, 220,  38));
    aura.setColorAt(1.0, QColor( 0,  0,  0,   0));
    painter->setPen(Qt::NoPen);
    painter->setBrush(aura);
    painter->drawEllipse(QPointF(cx, cy + H * 0.05f), W * 1.1f, W * 0.9f);

    // --- Dark hooded cloak body ---
    QPainterPath cloak;
    cloak.moveTo(cx, cy - H * 0.50f);                              // hood tip
    cloak.cubicTo(cx + W * 0.22f, cy - H * 0.44f,
                  cx + W * 0.50f, cy - H * 0.22f,
                  cx + W * 0.52f, cy + H * 0.08f);                // right shoulder
    cloak.cubicTo(cx + W * 0.56f, cy + H * 0.32f,
                  cx + W * 0.42f, cy + H * 0.46f,
                  cx,             cy + H * 0.50f);                 // bottom center
    cloak.cubicTo(cx - W * 0.42f, cy + H * 0.46f,
                  cx - W * 0.56f, cy + H * 0.32f,
                  cx - W * 0.52f, cy + H * 0.08f);                // left shoulder
    cloak.cubicTo(cx - W * 0.50f, cy - H * 0.22f,
                  cx - W * 0.22f, cy - H * 0.44f,
                  cx,             cy - H * 0.50f);                 // back to tip
    cloak.closeSubpath();

    QRadialGradient bodyGrad(cx, cy, W * 0.7f);
    bodyGrad.setColorAt(0.0, QColor(28, 22, 40));
    bodyGrad.setColorAt(1.0, QColor( 6,  4, 10));

    painter->setBrush(bodyGrad);
    painter->setPen(QPen(QColor(50, 40, 70, 180), 1.0f));
    painter->drawPath(cloak);

    // --- Neon green visor (eye slit) ---
    float visorY  = cy - H * 0.12f;
    float visorHW = W * 0.30f;
    float visorH  = 3.8f;

    // Outer glow (soft, wide)
    QLinearGradient visorGlow(cx - visorHW * 1.6f, visorY,
                              cx + visorHW * 1.6f, visorY);
    visorGlow.setColorAt(0.0, QColor( 0, 255, 80,   0));
    visorGlow.setColorAt(0.3, QColor( 0, 255, 80,  70));
    visorGlow.setColorAt(0.5, QColor(80, 255,120, 100));
    visorGlow.setColorAt(0.7, QColor( 0, 255, 80,  70));
    visorGlow.setColorAt(1.0, QColor( 0, 255, 80,   0));
    painter->setBrush(visorGlow);
    painter->drawRoundedRect(
        QRectF(cx - visorHW * 1.6f, visorY - 7.f, visorHW * 3.2f, 14.f), 7.f, 7.f);

    // Core bar
    painter->setBrush(QColor(100, 255, 130, 255));
    painter->drawRoundedRect(
        QRectF(cx - visorHW, visorY - visorH * 0.5f, visorHW * 2.f, visorH), 2.f, 2.f);
}

// ---------------------------------------------------------------------------
// Game loop tick
// ---------------------------------------------------------------------------

void GameScene::tick()
{
    qint64 now = m_clock.elapsed();
    float dt   = static_cast<float>(now - m_lastMs) / 1000.f;
    m_lastMs   = now;
    dt = qMin(dt, 0.05f);

    switch (m_state) {
    case GameState::Attract:  updateAttract(dt);  break;
    case GameState::Playing:  updatePlaying(dt);  break;
    case GameState::GameOver: updateGameOver(dt); break;
    }

    m_input.endFrame();
    update();
}

void GameScene::updateAttract(float dt)
{
    advanceSparks(dt, 0.6f);
    if (m_input.isConfirmJustPressed())
        startGame();
}

void GameScene::updatePlaying(float dt)
{
    float &sx = m_player.sx;
    float &sy = m_player.sy;

    if (m_input.isMovingLeft())  sx = std::max(sx - PLAYER_SPEED * dt, CX - PLAYER_BOUND_X);
    if (m_input.isMovingRight()) sx = std::min(sx + PLAYER_SPEED * dt, CX + PLAYER_BOUND_X);
    if (m_input.isMovingUp())    sy = std::max(sy - PLAYER_SPEED * dt, CY - PLAYER_BOUND_Y);
    if (m_input.isMovingDown())  sy = std::min(sy + PLAYER_SPEED * dt, CY + PLAYER_BOUND_Y);

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
    for (const auto &obs : m_obstacles) {
        if (obs.wz > COLLIDE_Z) continue;
        float sc = projScale(obs.wz);
        float px = projX(obs.wx, obs.wz);
        float py = projY(obs.wy, obs.wz);
        QRectF obsRect(px - obs.wHalfW * sc, py - obs.wHalfH * sc,
                       obs.wHalfW * sc * 2.f, obs.wHalfH * sc * 2.f);
        QRectF playerRect(sx - PLAYER_HITBOX, sy - PLAYER_HITBOX,
                          PLAYER_HITBOX * 2.f, PLAYER_HITBOX * 2.f);
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
        float threshold = pr + PLAYER_HITBOX * 1.8f;   // generous pickup radius
        if (dx * dx + dy * dy < threshold * threshold) {
            m_score += c.value;
            m_popups.append({px, py, c.value, 1.0f});
            return true;
        }
        return false;
    });

    // --- Spawn ---
    m_spawnTimer -= dt;
    if (m_spawnTimer <= 0.f) {
        spawnObstacle();
        m_spawnTimer = m_spawnInterval;
    }
    m_collectTimer -= dt;
    if (m_collectTimer <= 0.f) {
        spawnCollectible();
        m_collectTimer = m_collectInterval;
    }

    // --- Score & difficulty ---
    m_survivalTime    += dt;
    m_score           += dt;                                            // +1 pt/s survival
    m_worldSpeed       = qMin(640.f, 220.f + m_survivalTime * 16.f);
    m_spawnInterval    = qMax(0.45f, 2.0f  - m_survivalTime * 0.055f);
    m_collectInterval  = qMax(0.55f, 1.0f  - m_survivalTime * 0.008f);

    // --- Advance popups ---
    for (auto &pop : m_popups)
        pop.life -= dt * 1.6f;
    m_popups.removeIf([](const ScorePopup &p) { return p.life <= 0.f; });

    updateHUD();
}

void GameScene::updateGameOver(float dt)
{
    m_gameOverTimer -= dt;
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

    m_worldSpeed      = 220.f;
    m_spawnTimer      = 1.5f;
    m_spawnInterval   = 2.0f;
    m_collectTimer    = 1.0f;
    m_collectInterval = 1.0f;
    m_survivalTime    = 0.f;
    m_score           = 0.f;
    m_state           = GameState::Playing;

    m_hudText->setPlainText("");
    m_overlayText->setPlainText("");
}

void GameScene::endGame()
{
    m_state         = GameState::GameOver;
    m_gameOverTimer = 1.5f;

    setOverlay(QString("GAME OVER\n\nScore: %1    Time: %2s\n\nPRESS SPACE TO RESTART")
                   .arg(static_cast<int>(m_score))
                   .arg(static_cast<int>(m_survivalTime)));
    m_hudText->setPlainText("");
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
    c.wx      = (rng->generateDouble() * 2.0 - 1.0) * 185.f;
    c.wy      = (rng->generateDouble() * 2.0 - 1.0) * 140.f;
    c.wz      = SPAWN_Z;
    c.wRadius = 14.f + rng->generateDouble() * 8.f;
    c.special  = rng->generateDouble() < 0.25;    // 25% chance orange (+25)
    c.value    = c.special ? 25 : 10;
    m_collectibles.append(c);
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

        // Colours per type
        QColor glowCol  = c->special ? QColor(255, 140,  20) : QColor( 40, 220,  80);
        QColor coreCol  = c->special ? QColor(255, 170,  50, alpha) : QColor( 60, 230,  95, alpha);
        QColor darkCol  = c->special ? QColor(160,  70,  10, alpha) : QColor( 10, 120,  40, alpha);
        QColor lightCol = c->special ? QColor(255, 230, 140, alpha) : QColor(190, 255, 190, alpha);
        QColor edgeCol  = c->special ? QColor(255, 200,  80, qMin(alpha + 60, 255))
                                     : QColor( 80, 255, 120, qMin(alpha + 60, 255));

        // Soft glow halo
        QRadialGradient glow(px, py, r * 2.2f);
        glow.setColorAt(0.0, QColor(glowCol.red(), glowCol.green(), glowCol.blue(),
                                    static_cast<int>(t * 65.f)));
        glow.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter->setPen(Qt::NoPen);
        painter->setBrush(glow);
        painter->drawEllipse(QPointF(px, py), r * 2.2f, r * 2.2f);

        // Diamond body
        QPainterPath gem;
        gem.moveTo(px,      py - r);
        gem.lineTo(px + r,  py);
        gem.lineTo(px,      py + r * 0.72f);
        gem.lineTo(px - r,  py);
        gem.closeSubpath();

        QLinearGradient facet(px - r, py - r, px + r, py + r * 0.72f);
        facet.setColorAt(0.0, lightCol);
        facet.setColorAt(0.5, coreCol);
        facet.setColorAt(1.0, darkCol);

        painter->setBrush(facet);
        painter->setPen(QPen(edgeCol, 1.2f));
        painter->drawPath(gem);

        // Inner highlight facet
        QPainterPath hi;
        hi.moveTo(px,           py - r);
        hi.lineTo(px + r * 0.5f, py - r * 0.4f);
        hi.lineTo(px,           py - r * 0.05f);
        hi.lineTo(px - r * 0.5f, py - r * 0.4f);
        hi.closeSubpath();
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(255, 255, 255, static_cast<int>(t * 60.f + 10.f)));
        painter->drawPath(hi);
    }
}

void GameScene::drawPopups(QPainter *painter) const
{
    if (m_popups.isEmpty()) return;

    painter->save();
    QFont font("Courier New", 15, QFont::Bold);
    painter->setFont(font);

    for (const auto &pop : m_popups) {
        float rise  = (1.f - pop.life) * 35.f;
        int   alpha = static_cast<int>(pop.life * 255.f);
        QColor col  = pop.value > 10 ? QColor(255, 175, 40, alpha)
                                     : QColor(80, 255, 120, alpha);
        painter->setPen(QPen(col));
        QString text = QString("+%1").arg(pop.value);
        painter->drawText(QPointF(pop.sx - 14.f, pop.sy - rise), text);
    }

    painter->restore();
}

void GameScene::setOverlay(const QString &text)
{
    m_overlayText->setPlainText(text);
    QRectF br = m_overlayText->boundingRect();
    m_overlayText->setPos((SCENE_W - br.width())  / 2.0,
                          (SCENE_H - br.height()) / 2.5);
}

void GameScene::updateHUD()
{
    m_hudText->setPlainText(
        QString("SCORE  %1    TIME  %2s")
            .arg(static_cast<int>(m_score), 5)
            .arg(static_cast<int>(m_survivalTime)));
}
