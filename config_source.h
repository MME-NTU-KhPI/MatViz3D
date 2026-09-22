#ifndef CONFIG_SOURCE_H
#define CONFIG_SOURCE_H

#include <QString>
#include <QVector>
#include <QPair>
#include <QSet>
#include <memory>

/**
 * @brief Abstract base class for configuration file format readers.
 *
 * Each ConfigSource is a thin format parser that converts file syntax
 * into flat (key, value) pairs. Sources perform NO semantic parameter validation
 * of their own, delegating all validation to applyParameter.
 */
class ConfigSource {
public:
    virtual ~ConfigSource() = default;

    /**
     * @brief Parses the file at @p path into flat key/value string pairs.
     * @param path  Path to configuration file.
     * @param error Output error message if parsing or I/O fails.
     * @return List of (key, value) string pairs.
     */
    virtual QVector<QPair<QString, QString>> read(const QString& path,
                                                 QString* error) = 0;
};

/**
 * @brief JSON configuration reader using Qt's QJsonDocument.
 *
 * Flattens nested JSON objects recursively into canonical CLI key names
 * (e.g. {"composite": {"dim": "2d"}} -> "composite_dim" = "2d").
 */
class JsonSource : public ConfigSource {
public:
    QVector<QPair<QString, QString>> read(const QString& path,
                                         QString* error) override;
};

/**
 * @brief YAML configuration reader for flat and 1-level nested configurations.
 *
 * Lightweight, zero-dependency parser that reads (key, value) pairs and flattens
 * 1-level nested mappings matching JsonSource's canonical key naming.
 * Values are returned in raw string form without type coercion.
 */
class YamlSource : public ConfigSource {
public:
    QVector<QPair<QString, QString>> read(const QString& path,
                                         QString* error) override;
};

/**
 * @brief Dispatches config file loading to the appropriate ConfigSource
 *        based on the file extension.
 */
class ConfigDispatcher {
public:
    /**
     * @brief Returns a ConfigSource instance matching the file extension of @p path.
     *        Returns nullptr and sets @p error if the format is unsupported.
     */
    static std::unique_ptr<ConfigSource> sourceForFile(const QString& path,
                                                       QString* error = nullptr);

    /**
     * @brief Dispatches the file by extension, reads flat pairs, and applies
     *        each pair via applyParameter.
     * @param path Path to config file.
     * @param error Optional error output string.
     * @param appliedKeys Optional output set to collect successfully applied normalized keys.
     * @return true on success; false on I/O, syntax, or parameter validation error.
     */
    static bool loadAndApply(const QString& path, QString* error = nullptr, QSet<QString>* appliedKeys = nullptr);
};

#endif // CONFIG_SOURCE_H
