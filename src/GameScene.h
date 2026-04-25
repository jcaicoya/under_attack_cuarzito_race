#pragma once
#include <QColor>
#include <QList>
#include <QObject>
#include <QPointF>
#include <QString>
#include "AudioManager.h"
#include "CaveRenderer.h"
#include "GameLaunchOptions.h"
#include "HighScoreManager.h"
#include "InputManager.h"
#include "TunnelPath.h"

class QPainter;

enum class GameState { Attract, Intro, Playing, SuccessFlyout, FailureCrash, GameOver, HighScoreEntry };
enum class EndSequenceKind { None, TrackComplete, AllGemsCaptured, OutOfEnergy };

class GameScene : public QObject {
    Q_OBJECT
public:
    explicit GameScene(const GameLaunchOptions &options = {}, QObject *parent = nullptr);
    InputManager *inputManager() { return &m_input; }
    void update(float dt);
    void render(QPainter *painter);
    QPointF vanishingPoint() const { return QPointF(m_vpX, m_vpY); }
    float time() const { return m_time; }
    float worldSpeed() const { return m_worldSpeed; }
    float tunnelZ() const { return m_tunnelZ; }
    float turnOcclusion() const;
    float verticalOcclusion() const;
    float playerOffYNorm() const;
    float playerOffXNorm() const;
    CaveRenderer::Mode caveMode() const;
    QPointF cameraShakeOffset() const;
    void showDiagnostics(const QString &text);
    void restartRun();

    enum class ViewMode { ThirdPerson, FirstPerson };
    void toggleViewMode();
    void toggleInvulnerability();

private:
    // ---------------------------------------------------------------
    // Scene constants
    // ---------------------------------------------------------------
    static constexpr float SCENE_W = 1280.f;
    static constexpr float SCENE_H = 720.f;
    static constexpr float CX      = SCENE_W / 2.f;   // screen center X
    static constexpr float CY      = SCENE_H / 2.f;   // screen center Y
    static constexpr float FOCAL            = 400.f;
    // Virtual depth at which Cuarzito is drawn in third-person.
    // Chosen so his physics boundary (safeY≈104) projects to within 16px of
    // the visible tunnel ring boundary at that depth — wall contact looks real.
    static constexpr float PLAYER_DRAW_DEPTH = 200.f;
    static constexpr float PLAYER_SPEED  = 320.f;
    static constexpr float CHASE_MIN_SPEED = 100.f;
    static constexpr float CHASE_BASE_SPEED = 235.f;
    static constexpr float CHASE_MAX_SPEED = 440.f;
    static constexpr float CHASE_ACCEL = 260.f;
    static constexpr float CHASE_BRAKE = 430.f;
    static constexpr float CHASE_DRAG = 72.f;

    // ---------------------------------------------------------------
    // Entity structs
    // ---------------------------------------------------------------
    struct Player {
        float offX = 0.f;   // local tunnel offset from the safe center
        float offY = 0.f;   // negative = up on screen, positive = down
        float z = 0.f;
        float speed = CHASE_MIN_SPEED;
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

    struct TrackOption {
        QString label;
        QString resource;
        QString subtitle;
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
    QPointF playerDrawCenter() const;
    float playerLean() const;
    float testVerticalIntent() const;
    QString currentTestSegmentLabel() const;
    void logTestTrace(bool force = false);
    void logTestCompletion() const;
    void applySelectedTrack();
    void cycleTrackSelection(int dir);

    // ---------------------------------------------------------------
    // Game flow
    // ---------------------------------------------------------------
    void startGame();
    void startAttract();
    void startIntro();
    void startHighScoreEntry(int score);
    void startSuccessFlyout(EndSequenceKind kind, const QString &title, const QString &detail);
    void startFailureCrash(const QString &title, const QString &detail);
    void endGame();
    void resetChaseGems();
    void spawnBurst(float sx, float sy, bool special);
    void spawnWallSparks();

    void updateAttract(float dt);
    void updateIntro(float dt);
    void updatePlaying(float dt);
    void updateChasePhysics(float dt);
    void updateSuccessFlyout(float dt);
    void updateFailureCrash(float dt);
    void updateGameOver(float dt);
    void updateHighScoreEntry(float dt);
    void updateHUD();
    void setOverlay(const QString &text);
    QString attractOverlayText() const;
    QString initialsEntryText() const;
    QString selectedTrackLabel() const;

    // ---------------------------------------------------------------
    // Rendering passes
    // ---------------------------------------------------------------
    void drawChaseGems(QPainter *p) const;
    void drawSafeZone(QPainter *p) const;
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
    GameLaunchOptions m_launchOptions;
    InputManager m_input;
    AudioManager m_audio;
    HighScoreManager m_highScores;
    TunnelPath   m_tunnelPath;
    Player       m_player;

    QList<ChaseGem>    m_chaseGems;
    QList<ScorePopup>  m_popups;
    QList<BurstParticle> m_bursts;
    QList<TrackOption> m_trackOptions;
    int                m_selectedTrackIndex = 0;

    // Vanishing point (moves as the tunnel curves)
    float m_vpX  = CX;
    float m_vpY  = CY;
    float m_time = 0.f;

    float m_worldSpeed      = CHASE_MIN_SPEED;
    float m_tunnelZ         = 0.f;
    float m_introAnimT      = 0.f;
    float m_survivalTime    = 0.f;
    float m_score           = 0.f;
    float m_gameOverTimer   = 0.f;
    float m_gameOverIdleTimer = 0.f;
    float m_introTimer      = 0.f;
    float m_countdownTimer  = 0.f;
    float m_energy         = 100.f;
    float m_cleanFlightTime = 0.f;
    float m_revealTimer     = 0.f;
    float m_revealDuration  = 0.f;
    float m_impactFlash     = 0.f;
    float m_endSequenceTimer = 0.f;
    float m_endOpenTimer    = 0.f;
    bool  m_scoreSubmitted  = false;
    bool  m_runWon          = false;
    bool  m_wasWallContact  = false;
    int   m_wallHitCount    = 0;
    int   m_pendingScore    = 0;
    int   m_initialIndex    = 0;
    QString m_initials      = "AAA";

    QString m_hudText;
    QString m_overlayText;
    QString m_endTitle;
    QString m_endDetail;
    QString m_diagText;
    float   m_diagTimer = 0.f;
    float    m_cameraShake = 0.f;
    ViewMode m_viewMode    = ViewMode::ThirdPerson;
    bool     m_invulnerable = false;
    float    m_testTraceTimer = 0.f;
    QString  m_lastTestSegmentLabel;
    bool     m_lastTestWallContact = false;
    EndSequenceKind m_endSequenceKind = EndSequenceKind::None;
};
