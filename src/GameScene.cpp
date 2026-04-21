#include "GameScene.h"
#include "PlayerCuarzito.h"
#include "Obstacle.h"
#include <QPainter>
#include <QGraphicsTextItem>
#include <QFont>
#include <QRandomGenerator>
#include <QLinearGradient>
#include <cmath>

GameScene::GameScene(QObject *parent) : QGraphicsScene(parent)
{
    setSceneRect(0, 0, SCENE_W, SCENE_H);
    setBackgroundBrush(Qt::NoBrush);

    initStars();

    m_hudText = new QGraphicsTextItem();
    m_hudText->setDefaultTextColor(QColor(100, 255, 200));
    m_hudText->setFont(QFont("Courier New", 18, QFont::Bold));
    m_hudText->setPos(20, 10);
    m_hudText->setZValue(20);
    addItem(m_hudText);

    m_overlayText = new QGraphicsTextItem();
    m_overlayText->setDefaultTextColor(Qt::white);
    m_overlayText->setFont(QFont("Courier New", 28, QFont::Bold));
    m_overlayText->setZValue(20);
    addItem(m_overlayText);

    m_overlayText->setPlainText("CUARZITO\n\nPRESS SPACE TO START");
    QRectF br = m_overlayText->boundingRect();
    m_overlayText->setPos((SCENE_W - br.width()) / 2.0, (SCENE_H - br.height()) / 2.5);

    m_clock.start();

    connect(&m_timer, &QTimer::timeout, this, &GameScene::tick);
    m_timer.start(16);
}

void GameScene::initStars()
{
    auto *rng = QRandomGenerator::global();
    for (int i = 0; i < 160; ++i) {
        Star s;
        s.x      = rng->bounded(static_cast<int>(SCENE_W));
        s.y      = rng->bounded(static_cast<int>(SCENE_H));
        s.r      = 0.5f + static_cast<float>(rng->generateDouble()) * 1.8f;
        s.bright = 0.3f + static_cast<float>(rng->generateDouble()) * 0.7f;
        m_stars.append(s);
    }
}

void GameScene::drawBackground(QPainter *painter, const QRectF &)
{
    painter->setRenderHint(QPainter::Antialiasing);
    painter->fillRect(QRectF(0, 0, SCENE_W, SCENE_H), QColor(4, 7, 18));

    painter->setPen(Qt::NoPen);
    for (const auto &s : m_stars) {
        float y = std::fmod(s.y + m_starScroll, SCENE_H);
        int alpha = static_cast<int>(s.bright * 210.f + 45.f);
        painter->setBrush(QColor(200, 220, 255, alpha));
        painter->drawEllipse(QPointF(s.x, y), s.r, s.r);
    }

    // Left cave wall gradient
    QLinearGradient left(0, 0, 180, 0);
    left.setColorAt(0.0, QColor(35, 8, 55, 235));
    left.setColorAt(1.0, QColor(0,  0,  0,  0));
    painter->fillRect(QRectF(0, 0, 180, SCENE_H), left);

    // Right cave wall gradient
    QLinearGradient right(SCENE_W, 0, SCENE_W - 180, 0);
    right.setColorAt(0.0, QColor(35, 8, 55, 235));
    right.setColorAt(1.0, QColor(0,  0,  0,  0));
    painter->fillRect(QRectF(SCENE_W - 180, 0, 180, SCENE_H), right);

    // Floor ambient glow near the player
    QLinearGradient floor(0, SCENE_H - 110, 0, SCENE_H);
    floor.setColorAt(0.0, QColor(0, 0, 0, 0));
    floor.setColorAt(1.0, QColor(0, 90, 65, 90));
    painter->fillRect(QRectF(0, SCENE_H - 110, SCENE_W, 110), floor);
}

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
    m_starScroll = std::fmod(m_starScroll + 30.f * dt, SCENE_H);
    if (m_input.isConfirmJustPressed())
        startGame();
}

void GameScene::updatePlaying(float dt)
{
    if (m_input.isMovingLeft())  m_player->moveLeft(dt);
    if (m_input.isMovingRight()) m_player->moveRight(dt);

    m_starScroll = std::fmod(m_starScroll + m_worldSpeed * 0.12f * dt, SCENE_H);

    for (auto *obs : m_obstacles)
        obs->advance(m_worldSpeed, dt);

    m_obstacles.removeIf([this](Obstacle *obs) {
        if (obs->isOffScreen(SCENE_H)) {
            removeItem(obs);
            delete obs;
            return true;
        }
        return false;
    });

    for (auto *obs : m_obstacles) {
        if (m_player->collidesWithItem(obs)) {
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
    m_worldSpeed     = qMin(620.f, 220.f + m_survivalTime * 16.f);
    m_spawnInterval  = qMax(0.45f, 2.0f  - m_survivalTime * 0.055f);

    updateHUD();
}

void GameScene::updateGameOver(float dt)
{
    m_gameOverTimer -= dt;
    m_starScroll = std::fmod(m_starScroll + 15.f * dt, SCENE_H);
    if (m_gameOverTimer <= 0.f && m_input.isConfirmJustPressed())
        startGame();
}

void GameScene::startGame()
{
    clearObstacles();

    if (!m_player) {
        m_player = new PlayerCuarzito();
        m_player->setXBounds(PLAY_MIN_X, PLAY_MAX_X);
        addItem(m_player);
    }
    m_player->setPos((SCENE_W - PlayerCuarzito::W) / 2.0, PLAYER_Y);
    m_player->setVisible(true);

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

    QString msg = QString("GAME OVER\n\nSurvived: %1s\n\nPRESS SPACE TO RESTART")
                      .arg(static_cast<int>(m_survivalTime));
    m_overlayText->setPlainText(msg);
    QRectF br = m_overlayText->boundingRect();
    m_overlayText->setPos((SCENE_W - br.width()) / 2.0, (SCENE_H - br.height()) / 2.5);
    m_hudText->setPlainText("");
}

void GameScene::spawnObstacle()
{
    auto *rng = QRandomGenerator::global();
    float obsW = 80.f  + static_cast<float>(rng->bounded(200));
    float obsH = 35.f  + static_cast<float>(rng->bounded(30));
    float span = PLAY_MAX_X - PLAY_MIN_X - obsW;
    float obsX = PLAY_MIN_X + static_cast<float>(rng->bounded(static_cast<int>(span)));

    auto *obs = new Obstacle(obsW, obsH);
    obs->setPos(obsX, -obsH - 5.f);
    addItem(obs);
    m_obstacles.append(obs);
}

void GameScene::clearObstacles()
{
    for (auto *obs : m_obstacles) {
        removeItem(obs);
        delete obs;
    }
    m_obstacles.clear();
}

void GameScene::updateHUD()
{
    m_hudText->setPlainText(QString("TIME  %1s").arg(static_cast<int>(m_survivalTime)));
}
