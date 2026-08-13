#include "schemacontroller.h"
#include "algorithmfactory.h"
#include "parameters.h"

SchemaController::SchemaController(QObject* parent)
    : QObject(parent)
{
}

void SchemaController::onAlgorithmSelected(const QString& name)
{
    m_schema = AlgorithmFactory::instance().schemaFor(name);
    emit schemaChanged();
}

QVariantList SchemaController::schemaForGroup(const QString& group) const
{
    QVariantList out;
    for (const ParamField& f : m_schema) {
        if (!group.isEmpty() && f.group != group)
            continue;

        QVariantMap m;
        m["key"]          = f.key;
        m["label"]        = f.label;
        m["type"]         = static_cast<int>(f.type);
        // Seed the field from the LIVE value, not the schema constant. The CLI
        // (--size, --points, --halfaxis_a, ...) writes into Parameters before
        // the panel is built, so a field showing the hardcoded default while
        // Parameters holds something else is simply lying about what will run.
        // Every schema key is spelled exactly like the matching Parameters
        // Q_PROPERTY, so the meta-object lookup covers all of them with no
        // per-key table; unknown keys return an invalid QVariant and fall back.
        QVariant live = Parameters::instance()->property(f.key.toUtf8().constData());
        if (live.isValid() &&
            (live.typeId() == QMetaType::Float || live.typeId() == QMetaType::Double)) {
            // A float widens to double exactly, so 0.7f stringifies in QML as
            // 0.699999988079071. Round-trip through a short representation so
            // the field shows what the user typed on the command line.
            live = QString::number(live.toDouble(), 'g', 6).toDouble();
        }
        m["defValue"]     = live.isValid() ? live : f.defValue;
        m["minValue"]     = f.minValue;
        m["maxValue"]     = f.maxValue;
        m["enumOptions"]  = f.enumOptions;
        m["invokeMethod"] = f.invokeMethod;
        out.append(m);
    }
    return out;
}
