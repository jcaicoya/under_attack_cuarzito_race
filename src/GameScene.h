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
    static constexpr float CX        = SCENE_W / 2.f;
    static constexpr float CY        = SCENE_H / 2.f;
    static constexpr float FOCAL     = 400.f;
    static constexpr float SPAWN_Z   = 900.f;
    static constexpr float REMOVE_Z  = 25.f;
    static constexpr float COLLIDE_Z = 460.f;

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
        float wx, wy, wz;
        float wHalfW, wHalfH;
    };

    struct Collectible {
        float wx, wy, wz;
        float wRadius;          // world-space radius
        int   value;            // 10 (green) or 25 (orange)
        bool  special;          // false = green, true = orange
    };

    struct ScorePopup {
        float sx, sy;           // screen position at moment of collection
        int   value;
        float life;             // 1.0 → 0.0
    };

    struct Spark {
        float wx, wy, wz;
        float speed;
    };

    struct Star {
        float x, y, r, bright;
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
    void spawnCollectible();
    void initSparks();
    void initStars();
    void advanceSparks(float dt, float speedMult = 1.f);

    void updateAttract(float dt);
    void updatePlaying(float dt);
    void updateGameOver(float dt);
    void updateHUD();
    void setOverlay(const QString &text);

    // ---------------------------------------------------------------
    // Rendering passes
    // ---------------------------------------------------------------
    void drawEnvironment(QPainter *p) const;
    void drawSparks(QPainter *p) const;
    void drawCollectibles(QPainter *p) const;
    void drawObstacles(QPainter *p) const;
    void drawPlayer(QPainter *p) const;
    void drawPopups(QPainter *p) const;

    // ---------------------------------------------------------------
    // State
    // ---------------------------------------------------------------
    GameState    m_state = GameState::Attract;
    InputManager m_input;
    Player       m_player;

    QList<Obstacle>    m_obstacles;
    QList<Collectible> m_collectibles;
    QList<ScorePopup>  m_popups;
    QList<Spark>       m_sparks;
    QList<Star>        m_stars;

    QTimer        m_timer;
    QElapsedTimer m_clock;
    qint64        m_lastMs = 0;

    float m_worldSpeed       = 220.f;
    float m_spawnTimer       = 1.5f;
    float m_spawnInterval    = 2.0f;
    float m_collectTimer     = 1.0f;
    float m_collectInterval  = 1.0f;
    float m_survivalTime     = 0.f;
    float m_score            = 0.f;
    float m_gameOverTimer    = 0.f;

    QGraphicsTextItem *m_hudText     = nullptr;
    QGraphicsTextItem *m_overlayText = nullptr;
};
