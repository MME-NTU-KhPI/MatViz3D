#ifndef PARAMFIELD_H
#define PARAMFIELD_H

#include <QString>
#include <QVariant>

struct ParamField {
    QString key;
    QString label;
    enum Type { Int, Double, Bool, Enum, PointsMode } type;   // ← добавлен PointsMode
    QVariant defValue,
            minValue,
            maxValue;
    QStringList enumOptions;
    QString group = "main";
    QString invokeMethod = "";
};

#endif // PARAMFIELD_H
