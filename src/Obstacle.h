#pragma once
#include <QGraphicsItem>

class Obstacle : public QGraphicsItem {
public:
    enum { Type = UserType + 1 };

    Obstacle(float width, float height, QGraphicsItem *parent = nullptr);

    int          type()        const override { return Type; }
    QRectF       boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    void advance(float speed, float dt);
    bool isOffScreen(float sceneHeight) const;

private:
    float m_w;
    float m_h;
};
