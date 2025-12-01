#ifndef TENSORCONTROLLER_H
#define TENSORCONTROLLER_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include "AnisotropicCalculator.h"
#include "parameters.h"

class TensorController : public QObject
{
    Q_OBJECT

    // Константи передаються з Parameters через setProperty()
    Q_PROPERTY(double C11 READ C11 WRITE setC11 NOTIFY constantsChanged)
    Q_PROPERTY(double C12 READ C12 WRITE setC12 NOTIFY constantsChanged)
    Q_PROPERTY(double C44 READ C44 WRITE setC44 NOTIFY constantsChanged)
    Q_PROPERTY(QVariantList EData READ EData NOTIFY dataUpdated)
    Q_PROPERTY(QVariantList GData READ GData NOTIFY dataUpdated)
    Q_PROPERTY(QVariantList NuData READ NuData NOTIFY dataUpdated)
    Q_PROPERTY(QVariantList Thetas READ Thetas NOTIFY dataUpdated)
    Q_PROPERTY(QVariantList Phis READ Phis NOTIFY dataUpdated)
    Q_PROPERTY(int GridSize READ GridSize NOTIFY dataUpdated)

public:
    static TensorController* instance() {
        static TensorController inst;
        return &inst;
    }

    explicit TensorController(QObject* parent = nullptr) : QObject(parent) {}

    double C11() const { return m_C11; }
    double C12() const { return m_C12; }
    double C44() const { return m_C44; }

    void setC11(double v) { m_C11 = v; emit constantsChanged(); }
    void setC12(double v) { m_C12 = v; emit constantsChanged(); }
    void setC44(double v) { m_C44 = v; emit constantsChanged(); }
    QVariantList EData() const { return m_EData; }
    QVariantList GData() const { return m_GData; }
    QVariantList NuData() const { return m_NuData; }
    QVariantList Thetas() const { return m_Thetas; }
    QVariantList Phis() const { return m_Phis; }
    int GridSize() const { return m_N; }

    Q_INVOKABLE void run();  // запуск розрахунку

signals:
    void resultsReady(double maxValue, double minValue);
    void constantsChanged();
    void qmlConsole(const QString& text);
    void dataUpdated();


private:
    double m_C11 = 0;
    double m_C12 = 0;
    double m_C44 = 0;
    QVariantList m_EData;
    QVariantList m_GData;
    QVariantList m_NuData;
    QVariantList m_Thetas;
    QVariantList m_Phis;
    int m_N = 0;
};

#endif
