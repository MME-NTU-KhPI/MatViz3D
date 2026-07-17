#ifndef SCHEMACONTROLLER_H
#define SCHEMACONTROLLER_H

#include <QString>
#include <QObject>
#include <QVariantList>
#include <QMetaObject>
#include <vector>
#include "parameters.h"
#include "paramfield.h"

class SchemaController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList currentSchema  READ currentSchema  NOTIFY schemaChanged)
    Q_PROPERTY(QVariantList mainSchema     READ mainSchema     NOTIFY schemaChanged)
    Q_PROPERTY(QVariantList advancedSchema READ advancedSchema NOTIFY schemaChanged)

public:
    explicit SchemaController(QObject* parent = nullptr);

    Q_INVOKABLE void onAlgorithmSelected(const QString& name);

    Q_INVOKABLE void applyValue(const QString& key,
                                const QVariant& v,
                                const QString& method = "")
    {
        if (!method.isEmpty()) {
            QMetaObject::invokeMethod(
                Parameters::instance(),
                method.toUtf8().constData(),
                Q_ARG(QString, v.toString()));
        } else {
            Parameters::instance()->setProperty(key.toUtf8().constData(), v);
        }
    }

    QVariantList currentSchema()  const { return schemaForGroup(""); }
    QVariantList mainSchema()     const { return schemaForGroup("main"); }
    QVariantList advancedSchema() const { return schemaForGroup("advanced"); }

signals:
    void schemaChanged();

private:
    QVariantList schemaForGroup(const QString& group) const;
    std::vector<ParamField> m_schema;
};

#endif // SCHEMACONTROLLER_H
