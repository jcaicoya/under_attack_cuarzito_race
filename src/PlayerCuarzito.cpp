#include "PlayerCuarzito.h"
#include <QPainter>
#include <QRadialGradient>
#include <algorithm>

PlayerCuarzito::PlayerCuarzito(QGraphicsItem *parent) : QGraphicsItem(parent)
{
    setZValue(10);
}

QRectF PlayerCuarzito::boundingRect() const
{
    return QRectF(0, 0, W, H);
}

QPainterPath PlayerCuarzito::shape() const
{
    float margin = W * 0.18f;
    QPainterPath p;
    p.addRect(QRectF(margin, margin, W - margin * 2.f, H - margin * 2.f));
    return p;
}

void PlayerCuarzito::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing);

    float cx = W * 0.5f;

    // Crystal body (gem viewed from behind)
    QPainterPath body;
    body.moveTo(cx, 0);
    body.lineTo(W, H * 0.40f);
    body.lineTo(W * 0.80f, H);
    body.lineTo(W * 0.20f, H);
    body.lineTo(0, H * 0.40f);
    body.closeSubpath();

    QRadialGradient grad(cx, H * 0.5f, W * 0.85f);
    grad.setColorAt(0.0, QColor(160, 255, 230));
    grad.setColorAt(0.5, QColor(0,   210, 180));
    grad.setColorAt(1.0, QColor(0,   80,  130, 180));

    painter->setPen(QPen(QColor(200, 255, 245), 1.5));
    painter->setBrush(grad);
    painter->drawPath(body);

    // Specular highlight facet
    QPainterPath highlight;
    highlight.moveTo(cx, H * 0.05f);
    highlight.lineTo(W * 0.72f, H * 0.35f);
    highlight.lineTo(cx, H * 0.44f);
    highlight.closeSubpath();

    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(255, 255, 255, 70));
    painter->drawPath(highlight);
}

void PlayerCuarzito::moveLeft(float dt)
{
    float nx = static_cast<float>(x()) - SPEED * dt;
    setX(std::max(nx, m_minX));
}

void PlayerCuarzito::moveRight(float dt)
{
    float nx = static_cast<float>(x()) + SPEED * dt;
    setX(std::min(nx, m_maxX - W));
}

void PlayerCuarzito::setXBounds(float minX, float maxX)
{
    m_minX = minX;
    m_maxX = maxX;
}
