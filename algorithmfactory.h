#ifndef ALGORITHMFACORY_H
#define ALGORITHMFACORY_H

#include <memory>
#include <map>
#include <vector>
#include <functional>
#include <QString>
#include <QMessageBox>
#include "parent_algorithm.h"
#include "parameters.h"
#include "paramfield.h"


class AlgorithmFactory {
public:
    using AlgorithmCreator = std::function<std::shared_ptr<Parent_Algorithm>(const Parameters&)>;

    static AlgorithmFactory& instance() {
        static AlgorithmFactory factory;
        return factory;
    }

    void registerAlgorithm(const QString& name, AlgorithmCreator creator) {
        creators[name] = std::move(creator);
    }

    // Registration of a parameter schema (static data, without object creation)
    void registerSchema(const QString& name, std::vector<ParamField> schema) {
        schemas[name] = std::move(schema);
    }

    //Retrieving a schema by algorithm name
    std::vector<ParamField> schemaFor(const QString& name) const {
        auto it = schemas.find(name);
        if (it != schemas.end())
            return it->second;
        return {};
    }

    std::shared_ptr<Parent_Algorithm> createAlgorithm(const QString& name, const Parameters& params) {
        auto it = creators.find(name);
        if (it != creators.end()) {
            return it->second(params);
        }
        return nullptr;
    }

private:
    std::map<QString, AlgorithmCreator> creators;
    std::map<QString, std::vector<ParamField>> schemas;
    AlgorithmFactory() = default;
    ~AlgorithmFactory() = default;
    AlgorithmFactory(const AlgorithmFactory&) = delete;
    AlgorithmFactory& operator=(const AlgorithmFactory&) = delete;
};


void registerAlgorithms();

void registerSchemas();

#endif // ALGORITHMFACORY_H
