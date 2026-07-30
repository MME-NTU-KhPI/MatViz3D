#include "texturecontroller.h"
#include <QtMath>
#include <QStringList>
#include <cmath>

static void eulerFromMatrix(const double R[3][3], double& phi1, double& Phi, double& phi2);

TextureController::TextureController(QObject* parent)
    : QObject(parent), m_lib(42)
{
    setProcess(static_cast<int>(TextureLibrary::Process::Rolling));
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

void TextureController::setProcess(int processIndex)
{
    if (processIndex < 0 || processIndex >= TextureLibrary::processCount()) return;
    if (m_process == processIndex) return;
    m_process = processIndex;

    m_components = TextureLibrary::processComponents(static_cast<TextureLibrary::Process>(processIndex));
    for (auto& c : m_components) c.scatter_deg = m_scatterDeg;

    emit processChanged();
    emit componentsChanged();
    regenerate();
}

QVariantList TextureController::processNames() const
{
    static const QStringList icons = { "➡", "🧊", "✨", "🔄", "🎲" }; // Extrusion, Rolling, Recrystallization, Shear, Random
    QVariantList out;
    for (int i = 0; i < TextureLibrary::processCount(); ++i) {
        auto p = static_cast<TextureLibrary::Process>(i);
        QVariantMap m;
        m["name"] = QString::fromStdString(TextureLibrary::processName(p));
        m["desc"] = QString::fromStdString(TextureLibrary::processDesc(p));
        m["icon"] = (i < icons.size()) ? icons[i] : QString("•");
        out.append(m);
    }
    return out;
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
    for (auto& c : m_components) c.scatter_deg = s;
    emit paramsChanged();
    emit componentsChanged();
    regenerate();
}

void TextureController::regenerate()
{
    if (m_components.empty()) {
        m_eulerPoints.clear();
        emit previewChanged();
        return;
    }
    m_lib.setComponents(m_components);
    rebuildPolePoints();
    emit previewChanged();
}

void TextureController::rebuildPolePoints()
{
    m_eulerPoints.clear();

    for (int g = 0; g < m_grainCount; ++g) {
        double ang[3];
        m_lib.sampleNext(ang, false);   // THXY(Z),THYZ(X),THZX(Y)

        // Z-X-Y
        double z=ang[0], x=ang[1], y=ang[2];
        double cz=std::cos(z),sz=std::sin(z);
        double cx=std::cos(x),sx=std::sin(x);
        double cy=std::cos(y),sy=std::sin(y);
        // Rz*Rx*Ry (intrinsic Z-X-Y)
        double R[3][3];
        // Rz
        double Rz[3][3]={{cz,-sz,0},{sz,cz,0},{0,0,1}};
        double Rx[3][3]={{1,0,0},{0,cx,-sx},{0,sx,cx}};
        double Ry[3][3]={{cy,0,sy},{0,1,0},{-sy,0,cy}};
        double RzRx[3][3];
        for(int i=0;i<3;++i)for(int j=0;j<3;++j){double s=0;for(int k=0;k<3;++k)s+=Rz[i][k]*Rx[k][j];RzRx[i][j]=s;}
        for(int i=0;i<3;++i)for(int j=0;j<3;++j){double s=0;for(int k=0;k<3;++k)s+=RzRx[i][k]*Ry[k][j];R[i][j]=s;}

        {
            double phi1, Phi, phi2;
            eulerFromMatrix(R, phi1, Phi, phi2);
            double px = phi1 / 360.0;
            double py = Phi  / 90.0;
            if (py > 1.0) py = 1.0;
            QVariantMap ep;
            ep["x"] = px; ep["y"] = py; ep["phi2"] = phi2;
            m_eulerPoints.append(ep);
        }

    }
}

static void eulerFromMatrix(const double R[3][3], double& phi1, double& Phi, double& phi2)
{
    const double RAD = 180.0/M_PI;
    // passive g = R^T
    double g[3][3];
    for (int i=0;i<3;++i) for (int j=0;j<3;++j) g[i][j]=R[j][i];

    double c = std::max(-1.0, std::min(1.0, g[2][2]));
    Phi = std::acos(c);
    double s = std::sin(Phi);
    double p1, p2;
    if (s < 1e-6) {
        p1 = std::atan2(g[0][1], g[0][0]);
        p2 = 0.0;
    } else {
        p1 = std::atan2(g[2][0], -g[2][1]);
        p2 = std::atan2(g[0][2],  g[1][2]);
    }
    phi1 = p1*RAD; if (phi1 < 0) phi1 += 360.0;
    Phi  = Phi*RAD;
    phi2 = p2*RAD; if (phi2 < 0) phi2 += 360.0;
}

QVariantList TextureController::componentLabels() const
{
    QVariantList out;
    for (const auto& comp : TextureLibrary::presetCatalog()) {
        if (comp.is_random) continue;
        double p1,P,p2;
        TextureLibrary::millerToBunge(comp.hkl, comp.uvw, p1,P,p2);
        if (p1 < 0) p1 += 360.0;
        if (p2 < 0) p2 += 360.0;
        QVariantMap m;
        m["name"] = QString::fromStdString(comp.name).section(' ',0,0);
        m["x"]    = p1 / 360.0;
        m["y"]    = P  / 90.0;
        m["phi2"] = p2;
        out.append(m);
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
    emit textureReady(m_components);
}
