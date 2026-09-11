#ifndef ALGORITHMFACORY_H
#define ALGORITHMFACORY_H

#include <algorithm>
#include <memory>
#include <map>
#include <vector>
#include <functional>
#include <QString>
#include <QStringList>
#include <QMessageBox>
#include "parent_algorithm.h"
#include "parameters.h"
#include "paramfield.h"
#include "algorithmplugin.h"


class AlgorithmFactory {
public:
    using AlgorithmCreator = std::function<std::shared_ptr<Parent_Algorithm>(const Parameters&)>;

    static AlgorithmFactory& instance() {
        static AlgorithmFactory factory;
        return factory;
    }

    // Preferred entry point: name, schema and factory arrive together, so an
    // algorithm is fully described by its own translation unit. Registering the
    // same name twice overwrites the previous plugin but keeps its list
    // position, so a re-registration cannot shuffle the UI combo box.
    void registerPlugin(AlgorithmPlugin plugin) {
        const QString name = plugin.name;
        auto it = plugins.find(name);
        const int seq = (it != plugins.end()) ? it->second.seq : nextSeq++;
        plugins[name] = Entry{ std::move(plugin), seq };
    }

    // Legacy half-registration, kept so the pre-plugin algorithms keep working
    // while they are migrated one at a time.
    void registerAlgorithm(const QString& name, AlgorithmCreator creator) {
        Entry& e = entryFor(name);
        e.plugin.create = std::move(creator);
    }

    // Registration of a parameter schema (static data, without object creation)
    void registerSchema(const QString& name, std::vector<ParamField> schema) {
        Entry& e = entryFor(name);
        e.plugin.schema = std::move(schema);
    }

    //Retrieving a schema by algorithm name
    std::vector<ParamField> schemaFor(const QString& name) const {
        auto it = plugins.find(name);
        if (it != plugins.end())
            return it->second.plugin.schema;
        if (name.compare("Probability Circle", Qt::CaseInsensitive) == 0 ||
            name.compare("Probability Ellipse", Qt::CaseInsensitive) == 0 ||
            name.compare("Probability Algorithm", Qt::CaseInsensitive) == 0) {
            return schemaFor("Probability");
        }
        if (name.compare("Moore", Qt::CaseInsensitive) == 0 ||
            name.compare("Neumann", Qt::CaseInsensitive) == 0 ||
            name.compare("von Neumann", Qt::CaseInsensitive) == 0 ||
            name.compare("Radial", Qt::CaseInsensitive) == 0) {
            return schemaFor("Polycrystall");
        }
        return {};
    }

    const AlgorithmPlugin* pluginFor(const QString& name) const {
        auto it = plugins.find(name);
        if (it != plugins.end())
            return &it->second.plugin;
        if (name.compare("Probability Circle", Qt::CaseInsensitive) == 0 ||
            name.compare("Probability Ellipse", Qt::CaseInsensitive) == 0 ||
            name.compare("Probability Algorithm", Qt::CaseInsensitive) == 0) {
            return pluginFor("Probability");
        }
        if (name.compare("Moore", Qt::CaseInsensitive) == 0 ||
            name.compare("Neumann", Qt::CaseInsensitive) == 0 ||
            name.compare("von Neumann", Qt::CaseInsensitive) == 0 ||
            name.compare("Radial", Qt::CaseInsensitive) == 0) {
            return pluginFor("Polycrystall");
        }
        return nullptr;
    }

    // Registered algorithms, ordered by the plugin's `order` then by
    // registration sequence. This is what the UI lists and what --help prints,
    // so no hardcoded name list has to be kept in sync anywhere.
    QStringList algorithmNames() const {
        std::vector<const Entry*> sorted;
        sorted.reserve(plugins.size());
        for (const auto& kv : plugins)
            if (kv.second.plugin.create) sorted.push_back(&kv.second);

        std::sort(sorted.begin(), sorted.end(), [](const Entry* a, const Entry* b) {
            if (a->plugin.order != b->plugin.order) return a->plugin.order < b->plugin.order;
            return a->seq < b->seq;
        });

        QStringList out;
        out.reserve(static_cast<int>(sorted.size()));
        for (const Entry* e : sorted) out << e->plugin.name;
        return out;
    }

    std::shared_ptr<Parent_Algorithm> createAlgorithm(const QString& name, const Parameters& params) {
        auto it = plugins.find(name);
        if (it != plugins.end() && it->second.plugin.create) {
            return it->second.plugin.create(params);
        }
        if (name.compare("Probability Circle", Qt::CaseInsensitive) == 0) {
            Parameters::instance()->setProbPreset("Sphere (Circle)");
            return createAlgorithm("Probability", params);
        }
        if (name.compare("Probability Ellipse", Qt::CaseInsensitive) == 0) {
            Parameters::instance()->setProbPreset("Triaxial Ellipsoid");
            return createAlgorithm("Probability", params);
        }
        if (name.compare("Probability Algorithm", Qt::CaseInsensitive) == 0) {
            return createAlgorithm("Probability", params);
        }
        if (name.compare("Moore", Qt::CaseInsensitive) == 0) {
            Parameters::instance()->setPolycrystallNeighborhood("Moore (26)");
            return createAlgorithm("Polycrystall", params);
        }
        if (name.compare("Neumann", Qt::CaseInsensitive) == 0 ||
            name.compare("von Neumann", Qt::CaseInsensitive) == 0) {
            Parameters::instance()->setPolycrystallNeighborhood("von Neumann (6)");
            return createAlgorithm("Polycrystall", params);
        }
        if (name.compare("Radial", Qt::CaseInsensitive) == 0) {
            Parameters::instance()->setPolycrystallNeighborhood("Radial (18)");
            return createAlgorithm("Polycrystall", params);
        }
        return nullptr;
    }

private:
    struct Entry {
        AlgorithmPlugin plugin;
        int seq = 0;
    };

    Entry& entryFor(const QString& name) {
        auto it = plugins.find(name);
        if (it == plugins.end()) {
            Entry e;
            e.plugin.name = name;
            e.seq = nextSeq++;
            it = plugins.emplace(name, std::move(e)).first;
        }
        return it->second;
    }

    std::map<QString, Entry> plugins;
    int nextSeq = 0;

    AlgorithmFactory() = default;
    ~AlgorithmFactory() = default;
    AlgorithmFactory(const AlgorithmFactory&) = delete;
    AlgorithmFactory& operator=(const AlgorithmFactory&) = delete;
};


void registerAlgorithms();

void registerSchemas();

#endif // ALGORITHMFACORY_H
