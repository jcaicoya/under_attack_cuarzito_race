#include "Obstacle.h"
#include <QPainter>
#include <QLinearGradient>

Obstacle::Obstacle(float width, float height, QGraphicsItem *parent)
    : QGraphicsItem(parent), m_w(width), m_h(height)
{
    setZValue(5);
}

QRectF Obstacle::boundingRect() const
{
    return QRectF(0, 0, m_w, m_h);
}

void Obstacle::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing);

    QLinearGradient grad(0, 0, 0, m_h);
    grad.setColorAt(0.0, QColor(200, 60,  255, 230));
    grad.setColorAt(1.0, QColor(80,  10,  130, 210));

    painter->setBrush(grad);
    painter->setPen(QPen(QColor(230, 120, 255, 200), 1.5));
    painter->drawRoundedRect(QRectF(0, 0, m_w, m_h), 5, 5);

    // Energy edge glow
    painter->setPen(QPen(QColor(255, 200, 255, 160), 2.0));
    painter->drawLine(QPointF(4, 2), QPointF(m_w - 4, 2));
}

void Obstacle::advance(float speed, float dt)
{
    setY(y() + speed * dt);
}

bool Obstacle::isOffScreen(float sceneHeight) const
{
    return y() > sceneHeight + m_h;
}
