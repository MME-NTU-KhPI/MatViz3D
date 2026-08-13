#ifndef PARAMFIELD_H
#define PARAMFIELD_H

#include <QString>
#include <QVariant>

struct ParamField {
    QString key;
    QString label;
    enum Type { Int, Double, Bool, Enum, PointsMode, Action } type;
    QVariant defValue,
            minValue,
            maxValue;
    QStringList enumOptions;
    QString group = "main";
    QString invokeMethod = "";

    // Enum fields whose options are only knowable at run time (a database
    // table, a plugin list, ...). A schema is built during static
    // initialisation, long before QCoreApplication exists, so it cannot query
    // anything itself; it names a provider instead and SchemaController fills
    // enumOptions in when the panel is actually built.
    // Known providers: "materials" (material_properties.db).
    QString optionsProvider = "";

    // Action fields: the id SchemaController::actionTriggered() carries when
    // the button is pressed. QML decides what to open.
    // Enum fields: same id, fired additionally when the user picks
    // actionOnValue -- that is what makes a "Custom..." entry open an editor.
    QString action = "";
    QString actionOnValue = "";
};

#endif // PARAMFIELD_H
