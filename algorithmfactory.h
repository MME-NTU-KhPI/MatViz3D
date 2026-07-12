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

    // Регистрация схемы параметров (статические данные, без создания объекта)
    void registerSchema(const QString& name, std::vector<ParamField> schema) {
        schemas[name] = std::move(schema);
    }

    // Получение схемы по имени алгоритма
    std::vector<ParamField> schemaFor(const QString& name) const {
        auto it = schemas.find(name);
        if (it != schemas.end())
            return it->second;
        return {};   // алгоритм без параметров — пустая схема
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
