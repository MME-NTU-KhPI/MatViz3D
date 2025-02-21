#include "probabilityalgorithmviewwrapper.h"
#include <QDebug>

ProbabilityAlgorithmViewWrapper::ProbabilityAlgorithmViewWrapper(QObject *parent) : QObject(parent) {}

void ProbabilityAlgorithmViewWrapper::showMessage()
{
    qDebug() << "Button pressed";
}
