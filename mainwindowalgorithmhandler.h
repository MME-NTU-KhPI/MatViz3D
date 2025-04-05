#ifndef MAINWINDOWALGORITHMHANDLER_H
#define MAINWINDOWALGORITHMHANDLER_H

#include "parameters.h"
#include "algorithmfactory.h"
#include <QObject>
#include <ctime>

class MainWindowAlgorithmHandler : public QObject
{
    Q_OBJECT

public:
    explicit MainWindowAlgorithmHandler(QObject *parent = nullptr);

    bool validateParameters();
    void runAlgorithm(const QString& algorithmName, bool isAnimation);
    void logExecutionTime(clock_t start_time);
    void setAlgorithmFlags(Parent_Algorithm& algorithm);

signals:
    void algorithmFinished();  // Сигнал для інформування UI

private:
    void startGifRecording();
    void stopGifRecording();
    void executeAlgorithm(Parent_Algorithm& algorithm, const QString& algorithmName);
};

#endif // MAINWINDOWALGORITHMHANDLER_H
