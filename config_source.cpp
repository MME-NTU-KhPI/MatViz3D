#include "config_source.h"
#include "commandline_parser.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QTextStream>

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

QString unquoteYamlScalar(const QString& str)
{
    const QString trimmed = str.trimmed();
    if (trimmed.length() >= 2) {
        if (trimmed.startsWith(QLatin1Char('"')) && trimmed.endsWith(QLatin1Char('"'))) {
            const QString inner = trimmed.mid(1, trimmed.length() - 2);
            QString unescaped;
            unescaped.reserve(inner.length());
            for (int i = 0; i < inner.length(); ++i) {
                if (inner[i] == QLatin1Char('\\') && i + 1 < inner.length()) {
                    const QChar next = inner[++i];
                    if (next == QLatin1Char('n')) unescaped.append(QLatin1Char('\n'));
                    else if (next == QLatin1Char('t')) unescaped.append(QLatin1Char('\t'));
                    else if (next == QLatin1Char('r')) unescaped.append(QLatin1Char('\r'));
                    else if (next == QLatin1Char('"')) unescaped.append(QLatin1Char('"'));
                    else if (next == QLatin1Char('\\')) unescaped.append(QLatin1Char('\\'));
                    else {
                        unescaped.append(QLatin1Char('\\'));
                        unescaped.append(next);
                    }
                } else {
                    unescaped.append(inner[i]);
                }
            }
            return unescaped;
        } else if (trimmed.startsWith(QLatin1Char('\'')) && trimmed.endsWith(QLatin1Char('\''))) {
            QString inner = trimmed.mid(1, trimmed.length() - 2);
            inner.replace(QLatin1String("''"), QLatin1String("'"));
            return inner;
        }
    }
    return trimmed;
}

bool parseYamlInlineList(const QString& str, QString* outVal, QString* outErr)
{
    const QString trimmed = str.trimmed();
    if (!trimmed.startsWith(QLatin1Char('[')) || !trimmed.endsWith(QLatin1Char(']'))) {
        if (outErr) *outErr = QStringLiteral("syntax error in inline list: missing matching brackets");
        return false;
    }

    const QString inner = trimmed.mid(1, trimmed.length() - 2).trimmed();
    if (inner.isEmpty()) {
        if (outVal) *outVal = QString();
        return true;
    }

    QStringList items;
    bool inSingle = false;
    bool inDouble = false;
    int itemStart = 0;

    for (int i = 0; i < inner.length(); ++i) {
        const QChar c = inner[i];
        if (c == QLatin1Char('\\') && inDouble && i + 1 < inner.length()) {
            ++i;
            continue;
        }
        if (c == QLatin1Char('\'') && !inDouble) {
            inSingle = !inSingle;
        } else if (c == QLatin1Char('"') && !inSingle) {
            inDouble = !inDouble;
        } else if (c == QLatin1Char(',') && !inSingle && !inDouble) {
            const QString item = inner.mid(itemStart, i - itemStart).trimmed();
            items.append(unquoteYamlScalar(item));
            itemStart = i + 1;
        }
    }

    if (inSingle || inDouble) {
        if (outErr) *outErr = QStringLiteral("unclosed quote inside inline list");
        return false;
    }

    const QString lastItem = inner.mid(itemStart).trimmed();
    if (!lastItem.isEmpty() || items.isEmpty()) {
        items.append(unquoteYamlScalar(lastItem));
    }

    if (outVal) *outVal = items.join(QLatin1Char(','));
    return true;
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
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            *error = QStringLiteral("Unable to open configuration file '%1': %2")
                         .arg(path, file.errorString());
        }
        return {};
    }

    QTextStream stream(&file);
    int lineNumber = 0;
    QString currentSection;
    int sectionIndent = -1;
    QVector<QPair<QString, QString>> out;

    auto setError = [&](const QString& msg) -> QVector<QPair<QString, QString>> {
        if (error) {
            *error = QStringLiteral("YAML parse error in '%1' at line %2: %3")
                         .arg(path)
                         .arg(lineNumber)
                         .arg(msg);
        }
        return {};
    };

    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        ++lineNumber;

        // 1. Check for tab characters in indentation (YAML strictly forbids tabs in indentation)
        for (int i = 0; i < line.length(); ++i) {
            if (line[i] == QLatin1Char('\t')) {
                return setError(QStringLiteral("tabs are not allowed for indentation (YAML forbids tabs)"));
            }
            if (!line[i].isSpace()) {
                break;
            }
        }

        // 2. Check for document markers
        const QString trimmedFull = line.trimmed();
        if (trimmedFull.isEmpty()) {
            continue; // Blank lines ignored
        }
        if (trimmedFull == QLatin1String("---") || trimmedFull.startsWith(QLatin1String("--- ")) ||
            trimmedFull == QLatin1String("...") || trimmedFull.startsWith(QLatin1String("... "))) {
            return setError(QStringLiteral("unsupported YAML construct (document marker)"));
        }

        // 3. Measure indentation
        int indent = 0;
        while (indent < line.length() && line[indent] == QLatin1Char(' ')) {
            ++indent;
        }
        const QString lineAfterIndent = line.mid(indent);

        // 4. Check for block sequence (only if line after indent starts with "- " or is "-")
        if (lineAfterIndent == QLatin1String("-") || lineAfterIndent.startsWith(QLatin1String("- "))) {
            return setError(QStringLiteral("unsupported YAML construct (block sequence)"));
        }

        // 5. Scan line to strip comments and locate key ':' separator.
        // Ignore '#' and ':' inside quotes AND inside inline lists [ ... ]
        bool inSingle = false;
        bool inDouble = false;
        int bracketDepth = 0;
        int commentIdx = -1;
        int colonIdx = -1;

        for (int i = indent; i < line.length(); ++i) {
            const QChar c = line[i];
            if (c == QLatin1Char('\\') && inDouble && i + 1 < line.length()) {
                ++i; // Skip escaped character
                continue;
            }
            if (c == QLatin1Char('\'') && !inDouble) {
                inSingle = !inSingle;
            } else if (c == QLatin1Char('"') && !inSingle) {
                inDouble = !inDouble;
            } else if (!inSingle && !inDouble) {
                if (c == QLatin1Char('[')) {
                    ++bracketDepth;
                } else if (c == QLatin1Char(']')) {
                    if (bracketDepth > 0) {
                        --bracketDepth;
                    }
                } else if (c == QLatin1Char('#') && bracketDepth == 0) {
                    commentIdx = i;
                    break;
                } else if (c == QLatin1Char(':') && colonIdx == -1 && bracketDepth == 0) {
                    colonIdx = i;
                }
            }
        }

        if (inSingle || inDouble) {
            return setError(QStringLiteral("unclosed quote or unsupported multi-line scalar"));
        }
        if (bracketDepth != 0) {
            return setError(QStringLiteral("unclosed bracket in inline list"));
        }

        // Strip comment if present
        const QString effectiveLine = (commentIdx >= 0) ? line.left(commentIdx) : line;
        if (effectiveLine.mid(indent).trimmed().isEmpty()) {
            continue; // Line was only comments / spaces
        }

        // Verify key separator ':' was found
        if (colonIdx == -1 || colonIdx >= effectiveLine.length()) {
            return setError(QStringLiteral("missing ':' key-value separator"));
        }

        // 6. Extract key and raw value
        const QString rawKey = effectiveLine.mid(indent, colonIdx - indent).trimmed();
        const QString rawVal = effectiveLine.mid(colonIdx + 1);

        if (rawKey.isEmpty()) {
            return setError(QStringLiteral("empty key"));
        }

        // Check unsupported anchor/alias on key
        if (rawKey.startsWith(QLatin1Char('&')) || rawKey.startsWith(QLatin1Char('*'))) {
            return setError(QStringLiteral("unsupported YAML construct (anchor/alias)"));
        }

        const QString key = unquoteYamlScalar(rawKey);
        const QString trimmedVal = rawVal.trimmed();

        // Check that non-empty mapping value is separated by whitespace from ':'
        if (!trimmedVal.isEmpty() && !rawVal.at(0).isSpace()) {
            return setError(QStringLiteral("syntax error: mapping value must be separated from ':' by a space"));
        }

        // Check unsupported multi-line scalars: |/> only as the sole scalar indicator right after ':'
        if (trimmedVal == QLatin1String("|") || trimmedVal == QLatin1String(">") ||
            trimmedVal.startsWith(QLatin1String("| ")) || trimmedVal.startsWith(QLatin1String("> ")) ||
            trimmedVal.startsWith(QLatin1String("|\t")) || trimmedVal.startsWith(QLatin1String(">\t"))) {
            return setError(QStringLiteral("unsupported YAML construct (multi-line scalar)"));
        }

        // Check unsupported anchor/alias: &/* only at the start of the value, unquoted
        if (trimmedVal.startsWith(QLatin1Char('&')) || trimmedVal.startsWith(QLatin1Char('*'))) {
            return setError(QStringLiteral("unsupported YAML construct (anchor/alias)"));
        }

        // 7. Handle indentation and 1-level nesting
        if (indent == 0) {
            if (trimmedVal.isEmpty()) {
                // Parent section header (e.g. "voronoi:")
                currentSection = key;
                sectionIndent = -1;
                continue;
            } else {
                // Top-level scalar / list
                currentSection.clear();
                sectionIndent = -1;
            }
        } else {
            // Indented line (indent > 0)
            if (currentSection.isEmpty()) {
                return setError(QStringLiteral("unexpected indentation without a parent section"));
            }
            if (trimmedVal.isEmpty()) {
                return setError(QStringLiteral("unsupported YAML construct (nesting deeper than 1 level)"));
            }
            if (sectionIndent == -1) {
                sectionIndent = indent;
            } else if (indent != sectionIndent) {
                return setError(QStringLiteral("inconsistent indentation under section '%1'").arg(currentSection));
            }
        }

        // Compute flattened canonical key (anti-duplicate prefix rule)
        QString fullKey;
        if (currentSection.isEmpty()) {
            fullKey = key;
        } else {
            if (key.startsWith(currentSection + QLatin1Char('_'), Qt::CaseInsensitive) ||
                key.startsWith(currentSection + QLatin1Char('-'), Qt::CaseInsensitive)) {
                fullKey = key;
            } else {
                fullKey = currentSection + QLatin1Char('_') + key;
            }
        }

        // 8. Process value (inline list vs raw scalar)
        // NO YAML type coercion: values are returned as raw strings.
        if (trimmedVal.startsWith(QLatin1Char('['))) {
            QString listVal;
            QString listErr;
            if (!parseYamlInlineList(trimmedVal, &listVal, &listErr)) {
                return setError(listErr);
            }
            out.append(qMakePair(fullKey, listVal));
        } else {
            const QString val = unquoteYamlScalar(trimmedVal);
            out.append(qMakePair(fullKey, val));
        }
    }

    file.close();
    return out;
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
