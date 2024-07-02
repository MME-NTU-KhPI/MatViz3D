#include "legendview.h"
#include <QGraphicsTextItem>
#include <QString>

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
    QFont font;
    font.setFamily("Inter");
    font.setPixelSize(20);
    const int lineHeight = 30;
    for (int i = 0; i < numLevels; ++i)
    {
        QGraphicsRectItem *rect = this->addRect(10, 15 + i * lineHeight, lineHeight, lineHeight, QPen(Qt::darkGray, 2), QBrush(colors[numLevels - i - 1]));
    }
    for (int i = 0; i <= numLevels; ++i)
    {
        if (i == 0 || i == numLevels)
            font.setBold(true);
        else
            font.setBold(false);

        QGraphicsTextItem *text = this->addText(labels[i], font);
        text->setPos(lineHeight + 20, 0 + i * lineHeight);
    }

}

QVector<QColor> LegendView::getCmap()
{
    return this->colors;
}
void LegendView::setCmap(QVector<QColor> cmap)
{
    this->colors=cmap;
}
