#include "legendview.h"
#include <QPainter>
#include <QGraphicsTextItem>
#include <QString>
#include <QDebug>
#include <QGraphicsRectItem>
#include <QGraphicsPixmapItem>
#include <QLinearGradient>
#include <QPixmap>
#include <QColor>

LegendView::LegendView(QObject *parent)
    : QGraphicsScene{parent}
{
    numLevels = 9;
    labels = {"Label 1", "Label 2", "Label 3", "Label 4", "Label 5", "Label 6", "Label 7", "Label 8", "Label 9", "Label 10"};
    colors = {Qt::red, Qt::green, Qt::blue, Qt::yellow, Qt::cyan, Qt::magenta, Qt::gray, Qt::black, Qt::white};
    draw();
}

int LegendView::getNumLevels()
{
    return this->numLevels;
}

void LegendView::setNumLevels(int numLevels)
{
    this->numLevels = numLevels;
}

void LegendView::setMinMax(float minv, float maxv)
{
    this->minValue = minv;
    this->maxValue = maxv;
    labels.clear();
    labels.resize(numLevels + 1);
    float step = (maxv - minv)/(numLevels);
    for (int i = 0; i < numLevels + 1; i++)
    {
        float value = maxv - i*step;
        labels[i] = QString::number(value, 'g', 2);
    }
    this->draw();
}

void LegendView::draw()
{
    this->clear();

    const int fullWidth = 600;
    const int gradientWidth = 550;
    const int height = 40;
    const int margin = 20;

    if (numLevels <= 0 || colors.size() < numLevels) {
        qCritical() << "Incorrect data in LegendView::draw()";
        return;
    }

    setSceneRect(0, 0, fullWidth + 2 * margin, height + 80);

    QPixmap pixmap(gradientWidth, height);
    QPainter painter(&pixmap);
    QLinearGradient gradient(0, 0, gradientWidth, 0);

    for (int i = 0; i < numLevels; ++i) {
        float position = float(i) / (numLevels - 1);
        gradient.setColorAt(position, colors[i]);
    }

    painter.fillRect(0, 0, gradientWidth, height, gradient);
    painter.end();

    int gradientX = margin + (fullWidth - gradientWidth) / 2;
    int gradientY = margin / 2;

    QGraphicsPixmapItem *gradientItem = addPixmap(pixmap);
    gradientItem->setPos(gradientX, gradientY);

    QFont font("Inter", 14);
    QPen textPen(Qt::gray);

    const int numTicks = numLevels + 1;
    for (int i = 0; i < numTicks; ++i) {
        float value = minValue + i * (maxValue - minValue) / (numTicks - 1);
        QString textValue = QString::number(value, 'g', 2);
        QGraphicsTextItem *text = addText(textValue, font);
        text->setDefaultTextColor(Qt::gray);

        int textX = gradientX + i * (gradientWidth / (numTicks - 1)) - text->boundingRect().width() * 0.6;
        int textY = gradientY + height + 25;

        text->setPos(textX, textY);
        text->setTransformOriginPoint(text->boundingRect().center());
        text->setRotation(-45);
    }

    update();
}


QVector<QColor> LegendView::getCmap()
{
    return this->colors;
}
void LegendView::setCmap(QVector<QColor> cmap)
{
    this->colors=cmap;
}
