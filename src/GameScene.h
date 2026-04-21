#pragma once
#include <QGraphicsScene>
#include <QElapsedTimer>
#include <QList>
#include <QTimer>
#include "InputManager.h"

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
    // ---------------------------------------------------------------
    // Scene constants
    // ---------------------------------------------------------------
    static constexpr float SCENE_W   = 1280.f;
    static constexpr float SCENE_H   = 720.f;
    static constexpr float CX        = SCENE_W / 2.f;  // vanishing point X
    static constexpr float CY        = SCENE_H / 2.f;  // vanishing point Y
    static constexpr float FOCAL     = 400.f;
    static constexpr float SPAWN_Z   = 900.f;
    static constexpr float REMOVE_Z  = 25.f;
    static constexpr float COLLIDE_Z = 460.f;           // collision check begins here

    static constexpr float PLAYER_SPEED   = 320.f;
    static constexpr float PLAYER_BOUND_X = 270.f;
    static constexpr float PLAYER_BOUND_Y = 195.f;
    static constexpr float PLAYER_HITBOX  = 15.f;

    // ---------------------------------------------------------------
    // Entity structs
    // ---------------------------------------------------------------
    struct Player {
        float sx = CX;
        float sy = CY;
    };

    struct Obstacle {
        float wx, wy, wz;       // world position
        float wHalfW, wHalfH;   // half-extents in world units (= pixels when wz == FOCAL)
    };

    struct Spark {
        float wx, wy, wz;
        float speed;
    };

    // ---------------------------------------------------------------
    // Projection helpers
    // ---------------------------------------------------------------
    static float projX(float wx, float wz)  { return CX + wx * FOCAL / wz; }
    static float projY(float wy, float wz)  { return CY + wy * FOCAL / wz; }
    static float projScale(float wz)        { return FOCAL / wz; }

    // ---------------------------------------------------------------
    // Game flow
    // ---------------------------------------------------------------
    void startGame();
    void endGame();
    void spawnObstacle();
    void initSparks();
    void advanceSparks(float dt, float speedMult = 1.f);

    void updateAttract(float dt);
    void updatePlaying(float dt);
    void updateGameOver(float dt);
    void updateHUD();
    void setOverlay(const QString &text);

    // ---------------------------------------------------------------
    // Rendering passes (all called from drawBackground)
    // ---------------------------------------------------------------
    void drawTunnel(QPainter *p) const;
    void drawSparks(QPainter *p) const;
    void drawObstacles(QPainter *p) const;
    void drawPlayer(QPainter *p) const;

    // ---------------------------------------------------------------
    // State
    // ---------------------------------------------------------------
    GameState    m_state = GameState::Attract;
    InputManager m_input;
    Player       m_player;

    QList<Obstacle> m_obstacles;
    QList<Spark>    m_sparks;

    QTimer        m_timer;
    QElapsedTimer m_clock;
    qint64        m_lastMs = 0;

    float m_worldSpeed    = 220.f;
    float m_spawnTimer    = 1.5f;
    float m_spawnInterval = 2.0f;
    float m_survivalTime  = 0.f;
    float m_gameOverTimer = 0.f;

    QGraphicsTextItem *m_hudText     = nullptr;
    QGraphicsTextItem *m_overlayText = nullptr;
};
