#ifndef SCHEMACONTROLLER_H
#define SCHEMACONTROLLER_H

#include <QString>
#include <QObject>
#include <QVariantList>
#include <vector>
#include "parameters.h"
#include "paramfield.h"

class SchemaController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList currentSchema READ currentSchema NOTIFY schemaChanged)

public:
    explicit SchemaController(QObject* parent = nullptr);

    Q_INVOKABLE void onAlgorithmSelected(const QString& name);

    Q_INVOKABLE void applyValue(const QString& key, const QVariant& v) {
        Parameters::instance()->setProperty(key.toUtf8().constData(), v);
    }

    QVariantList currentSchema() const;

signals:
    void schemaChanged();

private:
    std::vector<ParamField> m_schema;
};

#endif // SCHEMACONTROLLER_H
