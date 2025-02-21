#ifndef PROBABILITYALGORITHMVIEWWRAPPER_H
#define PROBABILITYALGORITHMVIEWWRAPPER_H
#include <QObject>

class ProbabilityAlgorithmViewWrapper : public QObject
{
    Q_OBJECT
public:
    explicit ProbabilityAlgorithmViewWrapper(QObject *parent = nullptr);

    Q_INVOKABLE void showMessage();
};

#endif // PROBABILITYALGORITHMVIEWWRAPPER_H
