#pragma once
#include <QColor>
#include <QList>
#include <QObject>
#include <QPointF>
#include <QString>
#include "AudioManager.h"
#include "CaveRenderer.h"
#include "HighScoreManager.h"
#include "InputManager.h"
#include "TunnelPath.h"

class QPainter;

enum class GameState { Attract, Intro, Countdown, Playing, GameOver, HighScoreEntry };

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
    float tunnelZ() const { return m_tunnelZ; }
    float turnOcclusion() const;
    CaveRenderer::Mode caveMode() const;
    void showDiagnostics(const QString &text);

private:
    // ---------------------------------------------------------------
    // Scene constants
    // ---------------------------------------------------------------
    static constexpr float SCENE_W = 1280.f;
    static constexpr float SCENE_H = 720.f;
    static constexpr float CX      = SCENE_W / 2.f;   // screen center X
    static constexpr float CY      = SCENE_H / 2.f;   // screen center Y
    static constexpr float FOCAL   = 400.f;
    static constexpr float PLAYER_SPEED  = 320.f;
    static constexpr float CHASE_MIN_SPEED = 135.f;
    static constexpr float CHASE_BASE_SPEED = 235.f;
    static constexpr float CHASE_MAX_SPEED = 520.f;
    static constexpr float CHASE_ACCEL = 260.f;
    static constexpr float CHASE_BRAKE = 430.f;
    static constexpr float CHASE_DRAG = 72.f;

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

    struct ChaseGem {
        QString name;
        QColor core;
        QColor edge;
        float z = 0.f;
        float speed = 205.f;
        float radius = 22.f;
        int value = 500;
        bool collected = false;
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
    void startIntro();
    void startCountdown();
    void startHighScoreEntry(int score);
    void endGame();
    void resetChaseGems();
    void spawnBurst(float sx, float sy, bool special);

    void updateAttract(float dt);
    void updateIntro(float dt);
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
    void drawChaseGems(QPainter *p) const;
    void drawPlayer(QPainter *p) const;
    void drawVisorReveal(QPainter *p, float cx, float cy, float width, float height, float amount) const;
    void drawBursts(QPainter *p) const;
    void drawPopups(QPainter *p) const;
    void drawHUD(QPainter *p) const;
    void drawHighScoreEntry(QPainter *p) const;
    void drawTopScores(QPainter *p, float x, float y, int maxRows) const;
    void drawImpactFlash(QPainter *p) const;
    void drawMiniMap(QPainter *p) const;

    // ---------------------------------------------------------------
    // State
    // ---------------------------------------------------------------
    GameState    m_state = GameState::Attract;
    InputManager m_input;
    AudioManager m_audio;
    HighScoreManager m_highScores;
    TunnelPath   m_tunnelPath;
    Player       m_player;

    QList<ChaseGem>    m_chaseGems;
    QList<ScorePopup>  m_popups;
    QList<BurstParticle> m_bursts;

    // Vanishing point (moves as the tunnel curves)
    float m_vpX  = CX;
    float m_vpY  = CY;
    float m_time = 0.f;

    float m_worldSpeed      = CHASE_BASE_SPEED;
    float m_tunnelZ         = 0.f;
    float m_introAnimT      = 0.f;
    float m_survivalTime    = 0.f;
    float m_score           = 0.f;
    float m_gameOverTimer   = 0.f;
    float m_gameOverIdleTimer = 0.f;
    float m_introTimer      = 0.f;
    float m_countdownTimer  = 0.f;
    float m_chaseTimer      = 20.f;
    float m_cleanFlightTime = 0.f;
    float m_revealTimer     = 0.f;
    float m_revealDuration  = 0.f;
    float m_impactFlash     = 0.f;
    bool  m_scoreSubmitted  = false;
    bool  m_runWon          = false;
    bool  m_wasWallContact  = false;
    int   m_wallHitCount    = 0;
    int   m_pendingScore    = 0;
    int   m_initialIndex    = 0;
    QString m_initials      = "AAA";

    QString m_hudText;
    QString m_overlayText;
    QString m_diagText;
    float   m_diagTimer = 0.f;
};
