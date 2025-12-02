#include "tensorcontroller.h"
#include <QDebug>
#include <QString>
#include <QVariantList>
#include <QVariant>

QVariantList vectorToVariantList(const std::vector<double>& vec) {
    QVariantList list;
    list.reserve(vec.size());
    for (double val : vec) {
        list.append(QVariant(val));
    }
    return list;
}

void TensorController::run()
{
    qDebug() << "\n[TensorController] Starting calculations...";
    qDebug() << " Using C11 =" << m_C11
             << " C12 =" << m_C12
             << " C44 =" << m_C44;

    QString dep = Parameters::instance()->dependence().trimmed();

    try {
        // створюємо калькулятор
        AnisotropicCalculator calc(m_C11, m_C12, m_C44, 200);
        calc.run();

        m_EData = vectorToVariantList(calc.getE());
        m_GData = vectorToVariantList(calc.getG());
        m_NuData = vectorToVariantList(calc.getNu());
        m_Thetas = vectorToVariantList(calc.getThetas());
        m_Phis = vectorToVariantList(calc.getPhis());
        m_N = calc.getGridSize();

        emit dataUpdated();
        // читаємо обрану залежність

        qDebug() << "[TensorController] Dependence selected:" << dep;

        // виконуємо відповідний розрахунок
        if (dep.contains("Young", Qt::CaseInsensitive)) {

            auto ex = calc.getExtrema(calc.getE());
            qDebug() << "\n--- Young's modulus E(n) ---\n";
            qDebug() << " Max =" << ex.maxValue
                     << " at theta=" << ex.thetaMax
                     << " phi="  << ex.phiMax;
            qDebug() << " Min =" << ex.minValue
                     << " at theta=" << ex.thetaMin
                     << " phi="  << ex.phiMin;
        }
        else if (dep.contains("Shear", Qt::CaseInsensitive)) {

            auto ex = calc.getExtrema(calc.getG());
            qDebug() << "\n--- Shear modulus G(n,m) ---\n";
            qDebug() << " Max =" << ex.maxValue
                     << " at theta=" << ex.thetaMax
                     << " phi="  << ex.phiMax;
            qDebug() << " Min =" << ex.minValue
                     << " at theta=" << ex.thetaMin
                     << " phi="  << ex.phiMin;
        }
        else if (dep.contains("Poisson", Qt::CaseInsensitive)) {

            auto ex = calc.getExtrema(calc.getNu());
            qDebug() << "\n--- Poisson's ratio ν(n,m) ---\n";
            qDebug() << " Max =" << ex.maxValue
                     << " at theta=" << ex.thetaMax
                     << " phi="  << ex.phiMax;
            qDebug() << " Min =" << ex.minValue
                     << " at theta=" << ex.thetaMin
                     << " phi="  << ex.phiMin;
        }
        else {
            qDebug() << "[TensorController] Unknown dependence. Nothing to output.";
        }

    }
    catch (std::exception &e)
    {
        qDebug() << "[TensorController] ERROR:" << e.what();
    }
}
