#ifndef TENSORCONTROLLER_H
#define TENSORCONTROLLER_H

#include <QObject>
#include <QString>
#include "AnisotropicCalculator.h"
#include "parameters.h"

class TensorController : public QObject
{
    Q_OBJECT

    // Константи передаються з Parameters через setProperty()
    Q_PROPERTY(double C11 READ C11 WRITE setC11 NOTIFY constantsChanged)
    Q_PROPERTY(double C12 READ C12 WRITE setC12 NOTIFY constantsChanged)
    Q_PROPERTY(double C44 READ C44 WRITE setC44 NOTIFY constantsChanged)

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

    Q_INVOKABLE void run();  // запуск розрахунку

signals:
    void resultsReady(double maxValue, double minValue);
    void constantsChanged();


private:
    double m_C11 = 0;
    double m_C12 = 0;
    double m_C44 = 0;
};

#endif
