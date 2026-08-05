#include "texturecontroller.h"
#include <QRandomGenerator>
#include "parameters.h"
#include <QtMath>
#include <cmath>

TextureController::TextureController(QObject* parent)
    : QObject(parent), m_lib(42)
{
    rebuildFromProcess();   // Rolling FCC
}

void TextureController::rebuildFromProcess()
{
    auto proc = static_cast<TextureLibrary::Process>(m_process);
    auto lat  = static_cast<TextureLibrary::Lattice>(m_lattice);
    m_components = TextureLibrary::componentsForProcess(proc, lat, m_scatterDeg);
    emit componentsChanged();
    regenerate();
}

void TextureController::setProcess(int p)
{
    if (m_process == p) return;
    m_process = p;
    emit processChanged();
    rebuildFromProcess();
}

void TextureController::setLattice(int l)
{
    if (m_lattice == l) return;
    m_lattice = l;
    emit processChanged();
    rebuildFromProcess();
}

QVariantList TextureController::presets() const
{
    QVariantList out;
    for (const auto& c : TextureLibrary::presetCatalog()) {
        QVariantMap m;
        m["name"] = QString::fromStdString(c.name);
        m["hkl"]  = QString("{%1%2%3}").arg(c.hkl[0]).arg(c.hkl[1]).arg(c.hkl[2]);
        m["uvw"]  = QString("<%1%2%3>").arg(c.uvw[0]).arg(c.uvw[1]).arg(c.uvw[2]);
        out.append(m);
    }
    return out;
}

QVariantList TextureController::components() const
{
    QVariantList out;
    for (const auto& c : m_components) {
        QVariantMap m;
        m["name"]    = QString::fromStdString(c.name);
        m["weight"]  = c.weight;
        m["scatter"] = c.scatter_deg;
        out.append(m);
    }
    return out;
}

void TextureController::addCustom(int h,int k,int l, int u,int v,int w)
{
    if (h*u + k*v + l*w != 0) {
        qWarning() << "TextureController: {hkl} not perpendicular to <uvw>, skipped";
        return;
    }
    TextureLibrary::Component c;
    c.hkl[0]=h; c.hkl[1]=k; c.hkl[2]=l;
    c.uvw[0]=u; c.uvw[1]=v; c.uvw[2]=w;
    c.scatter_deg = m_scatterDeg;
    c.weight = 1.0;
    c.name = QString("{%1%2%3}<%4%5%6>").arg(h).arg(k).arg(l).arg(u).arg(v).arg(w).toStdString();
    m_components.push_back(c);
    emit componentsChanged();
    regenerate();
}

void TextureController::removeComponent(int index)
{
    if (index < 0 || index >= (int)m_components.size()) return;
    m_components.erase(m_components.begin() + index);
    emit componentsChanged();
    regenerate();
}

void TextureController::setWeight(int index, double weight)
{
    if (index < 0 || index >= (int)m_components.size()) return;
    m_components[index].weight = weight;
    emit componentsChanged();
    regenerate();
}

void TextureController::setComponentScatter(int index, double scatter)
{
    if (index < 0 || index >= (int)m_components.size()) return;
    m_components[index].scatter_deg = scatter;
    emit componentsChanged();
    regenerate();
}

void TextureController::clearComponents()
{
    m_components.clear();
    emit componentsChanged();
    regenerate();
}

void TextureController::setGrainCount(int n)
{
    if (m_grainCount == n) return;
    m_grainCount = qBound(1, n, 100000);
    emit paramsChanged();
    regenerate();
}

void TextureController::setScatterDeg(double s)
{
    if (qFuzzyCompare(m_scatterDeg, s)) return;
    m_scatterDeg = s;
    for (auto& comp : m_components) comp.scatter_deg = s;
    emit paramsChanged();
    emit componentsChanged();
    regenerate();
}

void TextureController::regenerate()
{
    m_lib.setSeed(Parameters::instance()->getSeed());
    if (m_components.empty())
        m_lib.setMode(TextureLibrary::Mode::Random);
    else
        m_lib.setComponents(m_components);

    rebuildPolePoints();
    emit previewChanged();
}

void TextureController::rebuildPolePoints()
{
    m_eulerPoints.clear();
    m_eulerPoints.reserve(m_grainCount * 3);

    for (int g = 0; g < m_grainCount; ++g) {
        double b[3];
        m_lib.sampleNextBunge(b);

        for (const auto& r : TextureLibrary::fundamentalZoneBunge(b[0], b[1], b[2])) {
            QVariantMap ep;
            ep["x"]    = r[0] / 90.0;
            ep["y"]    = r[1] / 90.0;
            ep["phi2"] = r[2];
            m_eulerPoints.append(ep);
        }
    }
}

QVariantList TextureController::componentLabels() const
{
    QVariantList out;
    for (const auto& comp : TextureLibrary::presetCatalog()) {
        double p1, P, p2;
        TextureLibrary::millerToBunge(comp.hkl, comp.uvw, p1, P, p2);

        const QString nm = QString::fromStdString(comp.name).section(' ', 0, 0);
        for (const auto& r : TextureLibrary::fundamentalZoneBunge(p1, P, p2)) {
            QVariantMap m;
            m["name"] = nm;
            m["x"]    = r[0] / 90.0;
            m["y"]    = r[1] / 90.0;
            m["phi2"] = r[2];
            out.append(m);
        }
    }
    return out;
}

void TextureController::setSectionPhi2(double v)
{
    if (qFuzzyCompare(m_sectionPhi2, v)) return;
    m_sectionPhi2 = v;
    emit sectionChanged();
}

void TextureController::convertAngles(double phi1, double Phi, double phi2)
{
    double tx, ty, tz;
    TextureLibrary::bungeToAnsys(phi1, Phi, phi2, tx, ty, tz);
    m_thxy = QString::number(tx, 'f', 4);
    m_thyz = QString::number(ty, 'f', 4);
    m_thzx = QString::number(tz, 'f', 4);
    emit converterChanged();
}

void TextureController::applyToStress()
{
    emit textureReady(m_components);   // ansysWrapper
}

void TextureController::reseed()
{
    Parameters::instance()->setSeed(QRandomGenerator::global()->bounded(1, 1000000));
    emit paramsChanged();
    regenerate();
}
