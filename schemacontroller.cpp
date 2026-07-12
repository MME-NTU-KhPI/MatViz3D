#include "schemacontroller.h"
#include "algorithmfactory.h"

SchemaController::SchemaController(QObject* parent)
    : QObject(parent)
{
}

void SchemaController::onAlgorithmSelected(const QString& name)
{
    m_schema = AlgorithmFactory::instance().schemaFor(name);
    emit schemaChanged();
}

QVariantList SchemaController::currentSchema() const
{
    QVariantList out;
    for (const ParamField& f : m_schema) {
        QVariantMap m;
        m["key"]         = f.key;
        m["label"]       = f.label;
        m["type"]        = static_cast<int>(f.type);
        m["defValue"]    = f.defValue;
        m["minValue"]    = f.minValue;
        m["maxValue"]    = f.maxValue;
        m["enumOptions"] = f.enumOptions;
        out.append(m);
    }
    return out;
}
