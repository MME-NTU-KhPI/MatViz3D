#ifndef SCHEMACONTROLLER_H
#define SCHEMACONTROLLER_H

#include <QString>
#include <QStringList>
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

    // Names of every registered algorithm, so the UI list is the registry
    // rather than a hardcoded copy of it that has to be kept in sync.
    Q_PROPERTY(QStringList algorithmNames READ algorithmNames NOTIFY registryChanged)

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

    // Raised by Action fields and by Enum fields whose selected option matches
    // their actionOnValue. The controller stays UI-agnostic: it says what
    // happened, QML decides what to open.
    Q_INVOKABLE void triggerAction(const QString& action)
    {
        if (!action.isEmpty())
            emit actionTriggered(action);
    }

    QVariantList currentSchema()  const { return schemaForGroup(""); }
    QVariantList mainSchema()     const { return schemaForGroup("main"); }
    QStringList  algorithmNames() const;

signals:
    void schemaChanged();
    void registryChanged();
    void actionTriggered(const QString& action);

private:
    QVariantList schemaForGroup(const QString& group) const;

    // Resolves ParamField::optionsProvider to a live option list.
    static QStringList providedOptions(const QString& provider);

    std::vector<ParamField> m_schema;
};

#endif // SCHEMACONTROLLER_H
