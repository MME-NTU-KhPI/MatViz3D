#ifndef PARAMFIELD_H
#define PARAMFIELD_H

#include <QString>
#include <QVariant>

struct ParamField {
    QString key;         // example: "ellipse_order"
    QString label;       // "description"
    enum Type { Int, Double, Bool, Enum } type;
    QVariant defValue, minValue, maxValue;
    QStringList enumOptions;
};

#endif // PARAMFIELD_H
