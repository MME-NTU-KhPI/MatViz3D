#include "config_source.h"
#include "commandline_parser.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace {

void flattenJsonValue(const QString& prefix,
                      const QJsonValue& val,
                      QVector<QPair<QString, QString>>& out);

void flattenJsonObject(const QString& prefix,
                       const QJsonObject& obj,
                       QVector<QPair<QString, QString>>& out)
{
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        const QString& key = it.key();
        QString fullKey;
        if (prefix.isEmpty()) {
            fullKey = key;
        } else {
            // If child key already starts with prefix (e.g. "voronoi_mxx" inside "voronoi"),
            // avoid duplicating the prefix.
            if (key.startsWith(prefix + "_", Qt::CaseInsensitive) ||
                key.startsWith(prefix + "-", Qt::CaseInsensitive)) {
                fullKey = key;
            } else {
                fullKey = prefix + "_" + key;
            }
        }
        flattenJsonValue(fullKey, it.value(), out);
    }
}

void flattenJsonValue(const QString& prefix,
                      const QJsonValue& val,
                      QVector<QPair<QString, QString>>& out)
{
    if (val.isObject()) {
        flattenJsonObject(prefix, val.toObject(), out);
    } else if (val.isArray()) {
        const QJsonArray arr = val.toArray();
        QStringList items;
        items.reserve(arr.size());
        for (const QJsonValue& elem : arr) {
            if (elem.isDouble()) {
                items.append(elem.toVariant().toString());
            } else if (elem.isBool()) {
                items.append(elem.toBool() ? QStringLiteral("true") : QStringLiteral("false"));
            } else {
                items.append(elem.toString());
            }
        }
        out.append(qMakePair(prefix, items.join(',')));
    } else if (val.isBool()) {
        out.append(qMakePair(prefix, val.toBool() ? QStringLiteral("true") : QStringLiteral("false")));
    } else if (val.isDouble()) {
        out.append(qMakePair(prefix, val.toVariant().toString()));
    } else if (val.isString()) {
        out.append(qMakePair(prefix, val.toString()));
    } else if (!val.isNull() && !val.isUndefined()) {
        out.append(qMakePair(prefix, val.toVariant().toString()));
    }
}

} // namespace

QVector<QPair<QString, QString>> JsonSource::read(const QString& path, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            *error = QStringLiteral("Unable to open configuration file '%1': %2")
                         .arg(path, file.errorString());
        }
        return {};
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError) {
        if (error) {
            *error = QStringLiteral("JSON syntax error in '%1' at offset %2: %3")
                         .arg(path, QString::number(parseErr.offset), parseErr.errorString());
        }
        return {};
    }

    if (!doc.isObject()) {
        if (error) {
            *error = QStringLiteral("Root JSON entity in '%1' must be an object")
                         .arg(path);
        }
        return {};
    }

    QVector<QPair<QString, QString>> out;
    flattenJsonObject(QString(), doc.object(), out);
    return out;
}

QVector<QPair<QString, QString>> YamlSource::read(const QString& path, QString* error)
{
    Q_UNUSED(path);
    // TODO: Integrate yaml-cpp library when YAML support is enabled in MatViz3D.
    if (error) {
        *error = QStringLiteral("YAML configuration support is not built yet (yaml-cpp is not enabled).");
    }
    return {};
}

std::unique_ptr<ConfigSource> ConfigDispatcher::sourceForFile(const QString& path, QString* error)
{
    const QFileInfo fi(path);
    const QString ext = fi.suffix().trimmed().toLower();

    if (ext == QLatin1String("json")) {
        return std::make_unique<JsonSource>();
    } else if (ext == QLatin1String("yaml") || ext == QLatin1String("yml")) {
        return std::make_unique<YamlSource>();
    } else {
        if (error) {
            *error = QStringLiteral("Unsupported configuration file extension '.%1' in '%2'. "
                                    "Supported formats: .json, .yaml, .yml")
                         .arg(ext, path);
        }
        return nullptr;
    }
}

bool ConfigDispatcher::loadAndApply(const QString& path, QString* error, QSet<QString>* appliedKeys)
{
    QString err;
    std::unique_ptr<ConfigSource> source = sourceForFile(path, &err);
    if (!source) {
        if (error) *error = QStringLiteral("Configuration error: %1").arg(err);
        return false;
    }

    const QVector<QPair<QString, QString>> pairs = source->read(path, &err);
    if (!err.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Failed to read configuration file '%1': %2")
                         .arg(path, err);
        }
        return false;
    }

    for (const auto& pair : pairs) {
        QString applyErr;
        if (!applyParameter(pair.first, pair.second, &applyErr)) {
            if (error) {
                *error = QStringLiteral("Configuration error in '%1' for key '%2': %3")
                             .arg(path, pair.first, applyErr);
            }
            return false;
        }
        if (appliedKeys) {
            QString s = pair.first.trimmed().toLower();
            s.replace('-', '_');
            s.replace('.', '_');
            appliedKeys->insert(s);
        }
        qInfo() << "[config]" << pair.first << ":" << pair.second;
    }

    return true;
}
