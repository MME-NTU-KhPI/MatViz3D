#include "schemacontroller.h"
#include "algorithmfactory.h"
#include "dbmanager.h"
#include "parameters.h"
#include <QDebug>

SchemaController::SchemaController(QObject* parent)
    : QObject(parent)
{
}

void SchemaController::onAlgorithmSelected(const QString& name)
{
    m_schema = AlgorithmFactory::instance().schemaFor(name);
    emit schemaChanged();
}

QStringList SchemaController::algorithmNames() const
{
    return AlgorithmFactory::instance().algorithmNames();
}

QStringList SchemaController::providedOptions(const QString& provider)
{
    if (provider == QLatin1String("materials"))
        return DBManager::materialNames();

    qWarning() << "unknown schema options provider" << provider;
    return {};
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
        QVariant live = f.key.isEmpty()
                            ? QVariant()
                            : Parameters::instance()->property(f.key.toUtf8().constData());
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

        // Options that only exist at run time (the material table) are pulled
        // in here -- the schema itself is built during static initialisation
        // and cannot query anything.
        QStringList options = f.enumOptions;
        if (!f.optionsProvider.isEmpty()) {
            options = providedOptions(f.optionsProvider);

            // Resolve what the combo will actually show: the live value if the
            // provider still offers it, else the schema's preferred default,
            // else whatever the provider does offer.
            QString current = m["defValue"].toString();
            if (current.isEmpty())
                current = f.defValue.toString();          // nothing chosen yet
            if (!options.isEmpty() && !options.contains(current))
                current = options.first();                // renamed or missing

            // Then push it back, so the panel is not showing one material
            // while the solvers are using the constants of another.
            m["defValue"] = current;
            if (!f.key.isEmpty() &&
                Parameters::instance()->property(f.key.toUtf8().constData()).toString() != current) {
                Parameters::instance()->setProperty(f.key.toUtf8().constData(), current);
            }
        }
        m["enumOptions"]  = options;

        m["invokeMethod"]  = f.invokeMethod;
        m["action"]        = f.action;
        m["actionOnValue"] = f.actionOnValue;
        out.append(m);
    }
    return out;
}
