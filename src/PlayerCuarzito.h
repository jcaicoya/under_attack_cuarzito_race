#pragma once
#include <QGraphicsItem>

class PlayerCuarzito : public QGraphicsItem {
public:
    static constexpr float W = 44.f;
    static constexpr float H = 52.f;

    explicit PlayerCuarzito(QGraphicsItem *parent = nullptr);

    QRectF      boundingRect() const override;
    QPainterPath shape()       const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    void moveLeft(float dt);
    void moveRight(float dt);
    void setXBounds(float minX, float maxX);

private:
    float m_minX = 90.f;
    float m_maxX = 1190.f;
    static constexpr float SPEED = 350.f;
};
