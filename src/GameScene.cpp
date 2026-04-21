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

    drawTunnel(painter);
    drawSparks(painter);
    drawObstacles(painter);

    if (m_state != GameState::Attract)
        drawPlayer(painter);
}

void GameScene::drawTunnel(QPainter *painter) const
{
    const QPointF vp(CX, CY);

    // Perspective grid lines converging at the vanishing point
    painter->setPen(QPen(QColor(15, 55, 40, 55), 1.0));
    for (float x = 0.f; x <= SCENE_W; x += 160.f) {
        painter->drawLine(vp, QPointF(x, 0.f));
        painter->drawLine(vp, QPointF(x, SCENE_H));
    }
    for (float y = 0.f; y <= SCENE_H; y += 120.f) {
        painter->drawLine(vp, QPointF(0.f, y));
        painter->drawLine(vp, QPointF(SCENE_W, y));
    }

    // Rectangular cross-sections of the tunnel at increasing depths
    painter->setBrush(Qt::NoBrush);
    const float ringZs[] = { 700.f, 500.f, 340.f, 210.f, 120.f };
    for (float rz : ringZs) {
        float sc    = projScale(rz);
        float rw    = 210.f * sc;
        float rh    = 155.f * sc;
        float t     = 1.f - rz / SPAWN_Z;
        int   alpha = static_cast<int>(25.f + t * 70.f);
        painter->setPen(QPen(QColor(0, 170, 110, alpha), 1.0));
        painter->drawRect(QRectF(CX - rw, CY - rh, rw * 2.f, rh * 2.f));
    }

    // Radial vignette to darken edges and suggest cave walls
    QRadialGradient vignette(CX, CY, 430.f);
    vignette.setColorAt(0.00, QColor(0, 0, 0, 0));
    vignette.setColorAt(0.55, QColor(0, 0, 0, 0));
    vignette.setColorAt(1.00, QColor(6, 2, 18, 230));
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

        QRectF rect(px - hw, py - hh, hw * 2.f, hh * 2.f);
        float t     = 1.f - obs->wz / SPAWN_Z;
        int   alpha = static_cast<int>(40.f + t * 195.f);

        QLinearGradient grad(rect.topLeft(), rect.bottomLeft());
        grad.setColorAt(0.0, QColor(210, 65,  255, alpha));
        grad.setColorAt(1.0, QColor(85,  10,  140, alpha));

        painter->setBrush(grad);
        painter->setPen(QPen(QColor(240, 130, 255, qMin(alpha + 40, 255)), 1.5));
        painter->drawRoundedRect(rect, 5, 5);

        // Bright top edge to suggest depth
        painter->setPen(QPen(QColor(255, 210, 255, qMin(alpha, 200)), 2.0));
        painter->drawLine(QPointF(rect.left() + 4, rect.top() + 2),
                          QPointF(rect.right() - 4, rect.top() + 2));
    }
}

void GameScene::drawPlayer(QPainter *painter) const
{
    const float cx = m_player.sx;
    const float cy = m_player.sy;
    constexpr float W = 38.f, H = 46.f;

    // Soft aura glow
    QRadialGradient aura(cx, cy, 52.f);
    aura.setColorAt(0.0, QColor(0, 255, 200, 55));
    aura.setColorAt(1.0, QColor(0,   0,   0,  0));
    painter->setPen(Qt::NoPen);
    painter->setBrush(aura);
    painter->drawEllipse(QPointF(cx, cy), 52.f, 52.f);

    // Crystal body (gem seen from behind)
    QPainterPath body;
    body.moveTo(cx,             cy - H * 0.50f);
    body.lineTo(cx + W * 0.50f, cy - H * 0.10f);
    body.lineTo(cx + W * 0.40f, cy + H * 0.50f);
    body.lineTo(cx - W * 0.40f, cy + H * 0.50f);
    body.lineTo(cx - W * 0.50f, cy - H * 0.10f);
    body.closeSubpath();

    QRadialGradient grad(cx, cy, W * 0.85f);
    grad.setColorAt(0.0, QColor(160, 255, 230));
    grad.setColorAt(0.5, QColor(0,   210, 180));
    grad.setColorAt(1.0, QColor(0,   80,  130, 180));

    painter->setPen(QPen(QColor(200, 255, 245), 1.5));
    painter->setBrush(grad);
    painter->drawPath(body);

    // Specular highlight
    QPainterPath hi;
    hi.moveTo(cx,             cy - H * 0.45f);
    hi.lineTo(cx + W * 0.36f, cy - H * 0.05f);
    hi.lineTo(cx,             cy + H * 0.05f);
    hi.closeSubpath();
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(255, 255, 255, 65));
    painter->drawPath(hi);
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

    for (auto &obs : m_obstacles)
        obs.wz -= m_worldSpeed * dt;

    m_obstacles.removeIf([](const Obstacle &o) { return o.wz < REMOVE_Z; });

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

    m_spawnTimer -= dt;
    if (m_spawnTimer <= 0.f) {
        spawnObstacle();
        m_spawnTimer = m_spawnInterval;
    }

    m_survivalTime  += dt;
    m_worldSpeed     = qMin(640.f, 220.f + m_survivalTime * 16.f);
    m_spawnInterval  = qMax(0.45f, 2.0f  - m_survivalTime * 0.055f);

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
    m_player = Player{};

    m_worldSpeed    = 220.f;
    m_spawnTimer    = 1.5f;
    m_spawnInterval = 2.0f;
    m_survivalTime  = 0.f;
    m_state         = GameState::Playing;

    m_hudText->setPlainText("");
    m_overlayText->setPlainText("");
}

void GameScene::endGame()
{
    m_state         = GameState::GameOver;
    m_gameOverTimer = 1.5f;

    setOverlay(QString("GAME OVER\n\nSurvived: %1s\n\nPRESS SPACE TO RESTART")
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

void GameScene::setOverlay(const QString &text)
{
    m_overlayText->setPlainText(text);
    QRectF br = m_overlayText->boundingRect();
    m_overlayText->setPos((SCENE_W - br.width())  / 2.0,
                          (SCENE_H - br.height()) / 2.5);
}

void GameScene::updateHUD()
{
    m_hudText->setPlainText(QString("TIME  %1s").arg(static_cast<int>(m_survivalTime)));
}
