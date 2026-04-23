#include "GameWidget.h"

#include "GameScene.h"

#include <QKeyEvent>
#include <QPainter>

GameWidget::GameWidget(QWidget *parent) : QOpenGLWidget(parent)
{
    m_scene = new GameScene(this);

    setFocusPolicy(Qt::StrongFocus);
    setFocus();

    m_clock.start();
    connect(&m_timer, &QTimer::timeout, this, &GameWidget::tick);
    m_timer.start(16);
}

GameWidget::~GameWidget() = default;

void GameWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

void GameWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    constexpr float logicalW = 1280.f;
    constexpr float logicalH = 720.f;
    const float sx = width() / logicalW;
    const float sy = height() / logicalH;
    // Cover the whole physical screen. On ultrawide displays this crops a
    // little vertically instead of showing black side bars.
    const float scale = qMax(sx, sy);
    const float targetW = logicalW * scale;
    const float targetH = logicalH * scale;
    const float ox = (width() - targetW) * 0.5f;
    const float oy = (height() - targetH) * 0.5f;

    painter.fillRect(rect(), QColor(0, 0, 0));
    painter.translate(ox, oy);
    painter.scale(scale, scale);

    const QPointF shake = m_scene->cameraShakeOffset();
    if (!shake.isNull())
        painter.translate(shake);

    m_caveRenderer.render(&painter, {
        QSizeF(logicalW, logicalH),
        m_scene->vanishingPoint(),
        m_scene->time(),
        m_scene->survivalTime(),
        m_scene->worldSpeed(),
        m_scene->turnOcclusion(),
        m_scene->tunnelZ(),
        m_scene->caveMode()
    });
    m_scene->render(&painter);
}

void GameWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat())
        return;

    m_scene->inputManager()->keyPressed(static_cast<Qt::Key>(event->key()));

    if (m_scene->inputManager()->isJustPressed(Action::Fullscreen)) {
        if (window()->isFullScreen())
            window()->showNormal();
        else
            window()->showFullScreen();
        return;
    }
    if (m_scene->inputManager()->isJustPressed(Action::Restart)) {
        m_scene->restartRun();
        return;
    }
    if (m_scene->inputManager()->isJustPressed(Action::Cancel)) {
        window()->close();
        return;
    }
    if (event->key() == Qt::Key_F1) {
        const QString diag = m_scene->inputManager()->gamepadDiagnostics();
        qDebug().noquote() << diag;
        m_scene->showDiagnostics(diag);
        return;
    }
}

void GameWidget::keyReleaseEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat())
        return;
    m_scene->inputManager()->keyReleased(static_cast<Qt::Key>(event->key()));
}

void GameWidget::tick()
{
    const qint64 now = m_clock.elapsed();
    float dt = static_cast<float>(now - m_lastMs) / 1000.f;
    m_lastMs = now;
    dt = qMin(dt, 0.05f);

    m_scene->update(dt);
    update();
}
