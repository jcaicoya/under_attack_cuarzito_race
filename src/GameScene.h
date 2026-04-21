#pragma once
#include <QColor>
#include <QList>
#include <QObject>
#include <QPointF>
#include <QString>
#include "AudioManager.h"
#include "HighScoreManager.h"
#include "InputManager.h"
#include "TunnelPath.h"

class QPainter;

enum class GameState { Attract, Countdown, Playing, GameOver, HighScoreEntry };

class GameScene : public QObject {
    Q_OBJECT
public:
    explicit GameScene(QObject *parent = nullptr);
    InputManager *inputManager() { return &m_input; }
    void update(float dt);
    void render(QPainter *painter);
    QPointF vanishingPoint() const { return QPointF(m_vpX, m_vpY); }
    float time() const { return m_time; }
    float survivalTime() const { return m_survivalTime; }
    float worldSpeed() const { return m_worldSpeed; }

private:
    // ---------------------------------------------------------------
    // Scene constants
    // ---------------------------------------------------------------
    static constexpr float SCENE_W = 1280.f;
    static constexpr float SCENE_H = 720.f;
    static constexpr float CX      = SCENE_W / 2.f;   // screen center X
    static constexpr float CY      = SCENE_H / 2.f;   // screen center Y
    static constexpr float FOCAL   = 400.f;
    static constexpr float SPAWN_Z = 900.f;
    static constexpr float REMOVE_Z  = 25.f;
    static constexpr float COLLIDE_Z = 460.f;

    // Player movement speed and tunnel bounds (screen-space px relative to VP)
    static constexpr float PLAYER_SPEED  = 320.f;
    static constexpr float TUNNEL_HALF_W = 220.f;   // gameplay wall boundary
    static constexpr float TUNNEL_HALF_H = 152.f;
    static constexpr float CHASE_MIN_SPEED = 135.f;
    static constexpr float CHASE_BASE_SPEED = 260.f;
    static constexpr float CHASE_MAX_SPEED = 760.f;
    static constexpr float CHASE_ACCEL = 410.f;
    static constexpr float CHASE_BRAKE = 520.f;
    static constexpr float CHASE_DRAG = 64.f;

    // ---------------------------------------------------------------
    // Entity structs
    // ---------------------------------------------------------------
    struct Player {
        float offX = 0.f;   // screen-space offset from VP (tunnel center)
        float offY = 0.f;
        float z = 0.f;
        float speed = CHASE_BASE_SPEED;
        bool wallContact = false;
    };

    struct Obstacle {
        float wx, wy, wz;
        float wHalfW, wHalfH;
    };

    struct Collectible {
        float wx, wy, wz;
        float wRadius;
        int   value;
        bool  special;
    };

    struct ScorePopup {
        float sx, sy;
        int   value;
        float life;
    };

    struct BurstParticle {
        float sx, sy;
        float vx, vy;
        float radius;
        float life;
        QColor color;
    };

    struct Spark {
        float wx, wy, wz;
        float speed;
    };

    // ---------------------------------------------------------------
    // Projection helpers — non-static, use moving vanishing point
    // ---------------------------------------------------------------
    float projX(float wx, float wz) const { return m_vpX + wx * FOCAL / wz; }
    float projY(float wy, float wz) const { return m_vpY + wy * FOCAL / wz; }
    static float projScale(float wz)      { return FOCAL / wz; }

    // Player absolute screen position (derived from VP + offset)
    float playerSX() const { return m_vpX + m_player.offX; }
    float playerSY() const { return m_vpY + m_player.offY; }
    float playerLean() const;

    // ---------------------------------------------------------------
    // Game flow
    // ---------------------------------------------------------------
    void startGame();
    void startAttract();
    void startCountdown();
    void startHighScoreEntry(int score);
    void endGame();
    void spawnObstacle();
    void spawnCollectible();
    void spawnBurst(float sx, float sy, bool special);
    void initSparks();
    void advanceSparks(float dt, float speedMult = 1.f);
    void updateVP(float dt);

    void updateAttract(float dt);
    void updateCountdown(float dt);
    void updatePlaying(float dt);
    void updateChasePhysics(float dt);
    void updateGameOver(float dt);
    void updateHighScoreEntry(float dt);
    void updateHUD();
    void setOverlay(const QString &text);
    QString attractOverlayText() const;
    QString initialsEntryText() const;

    // ---------------------------------------------------------------
    // Rendering passes
    // ---------------------------------------------------------------
    void drawSparks(QPainter *p) const;
    void drawCollectibles(QPainter *p) const;
    void drawObstacles(QPainter *p) const;
    void drawPlayer(QPainter *p) const;
    void drawVisorReveal(QPainter *p, float cx, float cy, float width, float height, float amount) const;
    void drawBursts(QPainter *p) const;
    void drawPopups(QPainter *p) const;
    void drawHUD(QPainter *p) const;
    void drawHighScoreEntry(QPainter *p) const;
    void drawTopScores(QPainter *p, float x, float y, int maxRows) const;
    void drawImpactFlash(QPainter *p) const;

    // ---------------------------------------------------------------
    // State
    // ---------------------------------------------------------------
    GameState    m_state = GameState::Attract;
    InputManager m_input;
    AudioManager m_audio;
    HighScoreManager m_highScores;
    TunnelPath   m_tunnelPath;
    Player       m_player;

    QList<Obstacle>    m_obstacles;
    QList<Collectible> m_collectibles;
    QList<ScorePopup>  m_popups;
    QList<BurstParticle> m_bursts;
    QList<Spark>       m_sparks;

    // Vanishing point (moves as the tunnel curves)
    float m_vpX  = CX;
    float m_vpY  = CY;
    float m_time = 0.f;

    float m_worldSpeed      = 220.f;
    float m_spawnTimer      = 1.5f;
    float m_spawnInterval   = 2.0f;
    float m_collectTimer    = 1.0f;
    float m_collectInterval = 1.0f;
    float m_survivalTime    = 0.f;
    float m_score           = 0.f;
    float m_gameOverTimer   = 0.f;
    float m_gameOverIdleTimer = 0.f;
    float m_countdownTimer  = 0.f;
    float m_revealTimer     = 0.f;
    float m_revealDuration  = 0.f;
    float m_impactFlash     = 0.f;
    bool  m_scoreSubmitted  = false;
    int   m_pendingScore    = 0;
    int   m_initialIndex    = 0;
    QString m_initials      = "AAA";

    QString m_hudText;
    QString m_overlayText;
};
