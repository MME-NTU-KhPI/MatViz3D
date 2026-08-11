#include "texturecontroller.h"
#include <QRandomGenerator>
#include <QDebug>
#include <QLoggingCategory>
#include <QElapsedTimer>
#include "parameters.h"
#include "texturemath.hpp"
#include <QtMath>
#include <algorithm>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
//  Debug tracing
//
//  Every state-changing entry point logs what came in and what it produced, so
//  the path "UI click -> m_components -> preview points -> Parameters::
//  textureComponents" can be followed in the console. Filter with
//      QT_LOGGING_RULES="matviz.texture.debug=false"
//  to silence it.
// ─────────────────────────────────────────────────────────────────────────────
Q_LOGGING_CATEGORY(lcTexture, "matviz.texture")

namespace {

const char* processName(int p)
{
    switch (p) {
    case 0: return "Random";
    case 1: return "Extrusion";
    case 2: return "Rolling";
    case 3: return "Recrystallization";
    case 4: return "Shear";
    case 5: return "ScatteredCube";
    default: return "?";
    }
}

const char* latticeName(int l) { return l == 0 ? "FCC" : (l == 1 ? "BCC" : "?"); }

QString describe(const TextureLibrary::Component& c)
{
    QString kind = c.is_random ? "random"
                 : c.is_capped ? "capped"
                 : c.is_fiber  ? "fiber"
                               : "ideal";
    return QString("%1 [%2] {%3%4%5}<%6%7%8> w=%9 sigma=%10deg")
        .arg(QString::fromStdString(c.name))
        .arg(kind)
        .arg(c.hkl[0]).arg(c.hkl[1]).arg(c.hkl[2])
        .arg(c.uvw[0]).arg(c.uvw[1]).arg(c.uvw[2])
        .arg(c.weight, 0, 'f', 2)
        .arg(c.scatter_deg, 0, 'f', 1);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Bunge ZXZ coordinate singularity ("gimbal lock")
//
//  At Phi = 0 the first and third rotations are both about Z, so only
//  phi1 + phi2 is physically defined (at Phi = 180, only phi1 - phi2).
//  bungeFromPassive() splits that sum arbitrarily -- e.g. a 0.5 deg tilt off
//  cube comes back as (165.1, 0.43, 194.6) -- which smears what is physically
//  one orientation across the whole phi1 width of the Euler plot.
//
//  For DISPLAY we adopt the usual convention: fold the whole in-plane rotation
//  into phi1 and set phi2 = 0, so near-cube grains collapse into one cluster
//  instead of a streak. This is preview-only -- TextureLibrary still hands the
//  raw angles to buildGrainOrientations() and both stress solvers.
// ─────────────────────────────────────────────────────────────────────────────
constexpr double kGimbalTolDeg = 1.0;

bool isGimbalLocked(double Phi_deg)
{
    return Phi_deg <= kGimbalTolDeg || Phi_deg >= 180.0 - kGimbalTolDeg;
}

// Returns true if the triple was rewritten.
bool canonicalizeGimbal(double b[3])
{
    if (!isGimbalLocked(b[1])) return false;

    // Phi ~ 0: the two Z rotations add. Phi ~ 180: they subtract.
    double inPlane = (b[1] <= kGimbalTolDeg) ? (b[0] + b[2]) : (b[0] - b[2]);
    inPlane = std::fmod(inPlane, 360.0);
    if (inPlane < 0.0) inPlane += 360.0;

    b[0] = inPlane;
    b[2] = 0.0;
    return true;
}

// Rotation angle between the crystal and the sample frame, in degrees -- the
// physically meaningful "how far from cube is this grain", independent of how
// phi1/phi2 happened to be split. trace(g) = (1+cos(Phi))*cos(phi1+phi2) + cos(Phi).
double angleFromCubeDeg(double phi1, double Phi, double phi2)
{
    const double D = M_PI / 180.0;
    const double c = std::cos(Phi * D);
    const double tr = (1.0 + c) * std::cos((phi1 + phi2) * D) + c;
    double h = (tr - 1.0) / 2.0;
    if (h >  1.0) h =  1.0;
    if (h < -1.0) h = -1.0;
    return std::acos(h) / D;
}

void dumpComponents(const std::vector<TextureLibrary::Component>& comps, const char* where)
{
    qCDebug(lcTexture) << "[TextureController]" << where << "-> components:" << (int)comps.size();
    for (size_t i = 0; i < comps.size(); ++i)
        qCDebug(lcTexture).noquote() << QString("[TextureController]   #%1 %2").arg(i).arg(describe(comps[i]));
}

} // namespace

TextureController::TextureController(QObject* parent)
    : QObject(parent), m_lib(42)
{
    qCDebug(lcTexture) << "[TextureController] ctor: process =" << processName(m_process)
                       << " lattice =" << latticeName(m_lattice)
                       << " grains =" << m_grainCount
                       << " scatter =" << m_scatterDeg << "deg";
    rebuildFromProcess();   // Rolling FCC
}

int TextureController::seed() const
{
    return static_cast<int>(Parameters::instance()->getSeed());
}

void TextureController::rebuildFromProcess()
{
    auto proc = static_cast<TextureLibrary::Process>(m_process);
    auto lat  = static_cast<TextureLibrary::Lattice>(m_lattice);
    qCDebug(lcTexture) << "[TextureController] rebuildFromProcess:" << processName(m_process)
                       << latticeName(m_lattice) << " scatter =" << m_scatterDeg << "deg";
    m_components = TextureLibrary::componentsForProcess(proc, lat, m_scatterDeg);
    dumpComponents(m_components, "rebuildFromProcess");
    emit componentsChanged();
    regenerate();
}

void TextureController::setProcess(int p)
{
    qCDebug(lcTexture) << "[TextureController] setProcess:" << processName(m_process)
                       << "->" << processName(p) << (m_process == p ? "(no change)" : "");
    if (m_process == p) return;
    m_process = p;
    emit processChanged();
    rebuildFromProcess();
}

void TextureController::setLattice(int l)
{
    qCDebug(lcTexture) << "[TextureController] setLattice:" << latticeName(m_lattice)
                       << "->" << latticeName(l) << (m_lattice == l ? "(no change)" : "");
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
    qCDebug(lcTexture) << "[TextureController] presets() ->" << out.size() << "entries";
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
    qCDebug(lcTexture) << "[TextureController] addCustom: {" << h << k << l << "}<" << u << v << w << ">";
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
    dumpComponents(m_components, "addCustom");
    emit componentsChanged();
    regenerate();
}

void TextureController::removeComponent(int index)
{
    if (index < 0 || index >= (int)m_components.size()) {
        qCDebug(lcTexture) << "[TextureController] removeComponent: index" << index
                           << "out of range (size" << (int)m_components.size() << ") -- ignored";
        return;
    }
    qCDebug(lcTexture).noquote() << QString("[TextureController] removeComponent: #%1 %2")
                                       .arg(index).arg(describe(m_components[index]));
    m_components.erase(m_components.begin() + index);
    emit componentsChanged();
    regenerate();
}

void TextureController::setWeight(int index, double weight)
{
    if (index < 0 || index >= (int)m_components.size()) {
        qCDebug(lcTexture) << "[TextureController] setWeight: index" << index
                           << "out of range (size" << (int)m_components.size() << ") -- ignored";
        return;
    }
    qCDebug(lcTexture) << "[TextureController] setWeight: #" << index
                       << QString::fromStdString(m_components[index].name)
                       << m_components[index].weight << "->" << weight;
    m_components[index].weight = weight;
    emit componentsChanged();
    regenerate();
}

void TextureController::setComponentScatter(int index, double scatter)
{
    if (index < 0 || index >= (int)m_components.size()) {
        qCDebug(lcTexture) << "[TextureController] setComponentScatter: index" << index
                           << "out of range (size" << (int)m_components.size() << ") -- ignored";
        return;
    }
    qCDebug(lcTexture) << "[TextureController] setComponentScatter: #" << index
                       << QString::fromStdString(m_components[index].name)
                       << m_components[index].scatter_deg << "->" << scatter << "deg";
    m_components[index].scatter_deg = scatter;
    emit componentsChanged();
    regenerate();
}

void TextureController::clearComponents()
{
    qCDebug(lcTexture) << "[TextureController] clearComponents: dropping"
                       << (int)m_components.size() << "component(s)";
    m_components.clear();
    emit componentsChanged();
    regenerate();
}

void TextureController::setGrainCount(int n)
{
    const int clamped = qBound(1, n, 100000);
    qCDebug(lcTexture) << "[TextureController] setGrainCount:" << m_grainCount << "->" << clamped
                       << (clamped != n ? QString("(requested %1, clamped)").arg(n) : QString())
                       << (m_grainCount == n ? "(no change)" : "");
    if (m_grainCount == n) return;
    m_grainCount = clamped;
    emit paramsChanged();
    regenerate();
}

void TextureController::setScatterDeg(double s)
{
    qCDebug(lcTexture) << "[TextureController] setScatterDeg:" << m_scatterDeg << "->" << s
                       << "deg (applied to" << (int)m_components.size() << "component(s))"
                       << (qFuzzyCompare(m_scatterDeg, s) ? "(no change)" : "");
    if (qFuzzyCompare(m_scatterDeg, s)) return;
    m_scatterDeg = s;
    for (auto& comp : m_components) comp.scatter_deg = s;
    emit paramsChanged();
    emit componentsChanged();
    regenerate();
}

void TextureController::regenerate()
{
    const unsigned int sd = Parameters::instance()->getSeed();
    qCDebug(lcTexture) << "[TextureController] regenerate: seed =" << sd
                       << " grains =" << m_grainCount
                       << " mode =" << (m_components.empty() ? "Random (no components)" : "Textured");

    m_lib.setSeed(sd);
    if (m_components.empty())
        m_lib.setMode(TextureLibrary::Mode::Random);
    else
        m_lib.setComponents(m_components);

    sampleOrientations();
    rebuildPolePoints();
    rebuildPoleFigure();
    rebuildOdf();
    emit previewChanged();
}

// Draw the grains once; every plot below is a different view of this same list.
void TextureController::sampleOrientations()
{
    QElapsedTimer t; t.start();

    m_orientations.clear();
    m_orientations.reserve(m_grainCount);
    m_fzVariants.clear();
    m_fzVariants.reserve(std::size_t(m_grainCount) * 3);

    int gimbalFixed = 0;
    for (int g = 0; g < m_grainCount; ++g) {
        double b[3];
        m_lib.sampleNextBunge(b);

        if (g < 3) {
            QString line = QString("[TextureController]   grain %1 Bunge ZXZ (deg) = "
                                   "%2 %3 %4  | %5 deg from cube")
                               .arg(g)
                               .arg(b[0], 0, 'f', 3).arg(b[1], 0, 'f', 3).arg(b[2], 0, 'f', 3)
                               .arg(angleFromCubeDeg(b[0], b[1], b[2]), 0, 'f', 3);
            if (isGimbalLocked(b[1]))
                line += QString("  [gimbal: Phi~%1, only phi1%2phi2 is meaningful -> canonicalized]")
                            .arg(b[1] <= kGimbalTolDeg ? 0 : 180)
                            .arg(b[1] <= kGimbalTolDeg ? "+" : "-");
            qCDebug(lcTexture).noquote() << line;
        }

        // Raw angles for the pole figure -- it works on rotation matrices and
        // has no coordinate singularity, so it must NOT get the canonicalized
        // triple (that would discard the tilt axis azimuth).
        m_orientations.push_back({ b[0], b[1], b[2] });

        // Euler-space views get the display convention (see canonicalizeGimbal).
        double bc[3] = { b[0], b[1], b[2] };
        if (canonicalizeGimbal(bc)) ++gimbalFixed;
        for (const auto& r : TextureLibrary::fundamentalZoneBunge(bc[0], bc[1], bc[2]))
            m_fzVariants.push_back({ r[0], r[1], r[2] });
    }

    qCDebug(lcTexture) << "[TextureController] sampleOrientations:" << m_grainCount << "grains ->"
                       << (int)m_fzVariants.size() << "symmetry variants in" << t.elapsed()
                       << "ms (" << gimbalFixed << "gimbal-canonicalized)";
}

void TextureController::rebuildPolePoints()
{
    m_eulerPoints.clear();
    m_eulerPoints.reserve(m_fzVariants.size());

    for (const auto& r : m_fzVariants) {
        QVariantMap ep;
        ep["x"]    = r[0] / 90.0;
        ep["y"]    = r[1] / 90.0;
        ep["phi2"] = r[2];
        m_eulerPoints.append(ep);
    }
}

void TextureController::rebuildPoleFigure()
{
    QElapsedTimer t; t.start();

    const auto fam = static_cast<texmath::PoleFamily>(m_poleFamily);
    const auto pts = texmath::poleFigure(m_orientations, fam);

    // Flat [x0,y0, x1,y1, ...] -- one QVariant per number instead of a map per
    // point keeps this cheap at 1000+ grains x up to 6 poles each.
    m_polePoints.clear();
    m_polePoints.reserve(int(pts.size()) * 2);
    for (const auto& p : pts) {
        m_polePoints.append(double(p[0]));
        m_polePoints.append(double(p[1]));
    }

    qCDebug(lcTexture) << "[TextureController] rebuildPoleFigure:" << poleFamilyName()
                       << "->" << (int)pts.size() << "poles from" << (int)m_orientations.size()
                       << "grains in" << t.elapsed() << "ms";
}

void TextureController::rebuildOdf()
{
    QElapsedTimer t; t.start();

    m_odfSections.clear();
    m_odfMax  = 0.0;
    m_odfBins = 0;
    if (m_fzVariants.empty()) return;

    m_odfBins = texmath::adaptiveBins(m_fzVariants.size());
    const auto grid = texmath::odfFromVariants(m_fzVariants, m_odfBins, 3);

    for (double v : grid.d) m_odfMax = std::max(m_odfMax, v);

    // The three conventional FCC sections.
    const double phi2List[3] = { 0.0, 45.0, 65.0 };
    // Contours are traced on a smoothly upsampled copy (drawing only -- the
    // density estimate itself stays at grid.n bins).
    const int kDrawGrid = 64;

    for (double phi2 : phi2List) {
        const auto sec  = texmath::odfSection(grid, phi2);
        const auto fine = texmath::resampleSection(sec, grid.n, kDrawGrid);

        QVariantList contours;
        int segCount = 0;
        for (double level : texmath::defaultLevels()) {
            const auto segs = texmath::contour(fine, kDrawGrid, level);
            if (segs.empty()) continue;

            QVariantList flat;
            flat.reserve(int(segs.size()) * 4);
            for (const auto& s : segs) {
                flat.append(double(s.x1)); flat.append(double(s.y1));
                flat.append(double(s.x2)); flat.append(double(s.y2));
            }
            QVariantMap c;
            c["level"] = level;
            c["segs"]  = flat;
            contours.append(c);
            segCount += int(segs.size());
        }

        QVariantMap sm;
        sm["phi2"]     = phi2;
        sm["max"]      = texmath::sectionMax(sec);
        sm["contours"] = contours;
        m_odfSections.append(sm);

        qCDebug(lcTexture) << "[TextureController]   ODF section phi2 =" << phi2
                           << "deg: peak" << QString::number(texmath::sectionMax(sec), 'f', 2)
                           << "x random," << segCount << "contour segments";
    }

    qCDebug(lcTexture) << "[TextureController] rebuildOdf:" << m_odfBins << "bins ("
                       << QString::number(90.0 / m_odfBins, 'f', 1) << "deg ), peak"
                       << QString::number(m_odfMax, 'f', 2) << "x random, in" << t.elapsed() << "ms";
}

QString TextureController::poleFamilyName() const
{
    switch (m_poleFamily) {
    case 0:  return "{100}";
    case 1:  return "{110}";
    default: return "{111}";
    }
}

void TextureController::setPoleFamily(int f)
{
    if (f < 0 || f > 2 || f == m_poleFamily) return;
    qCDebug(lcTexture) << "[TextureController] setPoleFamily:" << poleFamilyName() << "-> index" << f;
    m_poleFamily = f;
    emit poleFamilyChanged();
    rebuildPoleFigure();
    emit previewChanged();
}

QVariantList TextureController::componentLabels() const
{
    QVariantList out;
    for (const auto& comp : TextureLibrary::presetCatalog()) {
        double p1, P, p2;
        TextureLibrary::millerToBunge(comp.hkl, comp.uvw, p1, P, p2);

        // Same display convention as the sampled grains, so a marker and the
        // cloud around it land in the same place (matters for Cube, Phi = 0).
        double b[3] = { p1, P, p2 };
        canonicalizeGimbal(b);
        p1 = b[0]; P = b[1]; p2 = b[2];

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
    qCDebug(lcTexture) << "[TextureController] componentLabels() ->" << out.size() << "label markers";
    return out;
}

void TextureController::setSectionPhi2(double v)
{
    qCDebug(lcTexture) << "[TextureController] setSectionPhi2:" << m_sectionPhi2 << "->" << v << "deg"
                       << (qFuzzyCompare(m_sectionPhi2, v) ? "(no change)" : "");
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
    qCDebug(lcTexture) << "[TextureController] convertAngles: Bunge ZXZ (" << phi1 << Phi << phi2
                       << "deg ) -> ANSYS ZXY (" << tx << ty << tz << "deg )";
    emit converterChanged();
}

void TextureController::applyToStress()
{
    qCDebug(lcTexture) << "[TextureController] applyToStress: pushing"
                       << (int)m_components.size() << "component(s) into Parameters::textureComponents"
                       << " (seed =" << Parameters::instance()->getSeed() << ")";
    dumpComponents(m_components, "applyToStress");
    emit textureReady(m_components);   // ansysWrapper
}

void TextureController::reseed()
{
    const unsigned int old = Parameters::instance()->getSeed();
    Parameters::instance()->setSeed(QRandomGenerator::global()->bounded(1, 1000000));
    qCDebug(lcTexture) << "[TextureController] reseed:" << old << "->"
                       << Parameters::instance()->getSeed();
    emit paramsChanged();
    regenerate();
}
