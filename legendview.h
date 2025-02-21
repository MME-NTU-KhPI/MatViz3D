#ifndef LEGENDVIEW_H
#define LEGENDVIEW_H

#include <QGraphicsScene>
#include <QObject>

class LegendView : public QGraphicsScene
{
protected:
    int numLevels;
    QVector<QString> labels;
    QVector<QColor> colors;
    float minValue, maxValue;

public:
    explicit LegendView(QObject *parent = nullptr);
    int getNumLevels();
    void setNumLevels(int numLevels);
    QVector<QColor> getCmap();
    void setCmap(QVector<QColor> cmap);
    void draw();
    void setMinMax(float minv, float maxv);

};

#endif // LEGENDVIEW_H
