#ifndef ALGORITHMFACORY_H
#define ALGORITHMFACORY_H

#include <memory>
#include <map>
#include <functional>
#include <QString>
#include <QMessageBox>
#include "parent_algorithm.h"
#include "parameters.h"

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

    std::shared_ptr<Parent_Algorithm> createAlgorithm(const QString& name, const Parameters& params) {
        auto it = creators.find(name);
        if (it != creators.end()) {
            return it->second(params);
        }
        return nullptr;
    }

private:
    std::map<QString, AlgorithmCreator> creators;

    AlgorithmFactory() = default;
    ~AlgorithmFactory() = default;
    AlgorithmFactory(const AlgorithmFactory&) = delete;
    AlgorithmFactory& operator=(const AlgorithmFactory&) = delete;
};


void registerAlgorithms();

#endif // ALGORITHMFACORY_H
