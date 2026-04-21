#pragma once
#include <QGraphicsScene>
#include <QElapsedTimer>
#include <QList>
#include <QTimer>
#include "InputManager.h"

class PlayerCuarzito;
class Obstacle;
class QGraphicsTextItem;

enum class GameState { Attract, Playing, GameOver };

class GameScene : public QGraphicsScene {
    Q_OBJECT
public:
    explicit GameScene(QObject *parent = nullptr);

    InputManager *inputManager() { return &m_input; }

protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override;

private slots:
    void tick();

private:
    void startGame();
    void endGame();
    void spawnObstacle();
    void clearObstacles();

    void updateAttract(float dt);
    void updatePlaying(float dt);
    void updateGameOver(float dt);

    void initStars();
    void updateHUD();

    GameState        m_state = GameState::Attract;
    InputManager     m_input;
    PlayerCuarzito  *m_player = nullptr;
    QList<Obstacle *> m_obstacles;

    QTimer        m_timer;
    QElapsedTimer m_clock;
    qint64        m_lastMs = 0;

    float m_worldSpeed    = 220.f;
    float m_spawnTimer    = 1.5f;
    float m_spawnInterval = 2.0f;
    float m_survivalTime  = 0.f;
    float m_gameOverTimer = 0.f;

    float m_starScroll = 0.f;
    struct Star { float x, y, r, bright; };
    QList<Star> m_stars;

    QGraphicsTextItem *m_hudText     = nullptr;
    QGraphicsTextItem *m_overlayText = nullptr;

    static constexpr float SCENE_W    = 1280.f;
    static constexpr float SCENE_H    = 720.f;
    static constexpr float PLAYER_Y   = 595.f;
    static constexpr float PLAY_MIN_X = 90.f;
    static constexpr float PLAY_MAX_X = 1190.f;
};
