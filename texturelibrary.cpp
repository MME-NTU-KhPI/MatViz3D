#include "texturelibrary.h"
#include <cmath>
#include <numeric>
#include <cstdio>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
    constexpr double DEG = M_PI / 180.0;
    constexpr double RAD = 180.0 / M_PI;
    constexpr double EPS = 1e-9;
}

TextureLibrary::TextureLibrary(unsigned int seed) : m_rng(seed) {}

void TextureLibrary::setSeed(unsigned int seed) { m_rng.seed(seed); }
void TextureLibrary::setMode(Mode mode)         { m_mode = mode; }
void TextureLibrary::clear() { m_components.clear(); m_cumWeights.clear(); m_mode = Mode::Cube; }

void TextureLibrary::setComponents(const std::vector<Component>& comps)
{
    m_components = comps;
    m_mode = Mode::Textured;

    m_cumWeights.clear();
    double acc = 0.0;
    for (const auto& c : comps) { acc += (c.weight > 0 ? c.weight : 0); m_cumWeights.push_back(acc); }
    if (acc < EPS) { m_cumWeights.clear(); }
}

TextureLibrary::Matrix3 TextureLibrary::matmul(const Matrix3& A, const Matrix3& B)
{
    Matrix3 C{};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            double s = 0;
            for (int k = 0; k < 3; ++k) s += A[i][k] * B[k][j];
            C[i][j] = s;
        }
    return C;
}

TextureLibrary::Matrix3 TextureLibrary::orientationFromMiller(const int hkl[3], const int uvw[3])
{
    auto norm3 = [](double v[3]) {
        double n = std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
        if (n > EPS) { v[0]/=n; v[1]/=n; v[2]/=n; }
    };
    double nd[3] = {(double)hkl[0],(double)hkl[1],(double)hkl[2]};
    double rd[3] = {(double)uvw[0],(double)uvw[1],(double)uvw[2]};
    norm3(nd); norm3(rd);

    double td[3] = { nd[1]*rd[2]-nd[2]*rd[1],
                     nd[2]*rd[0]-nd[0]*rd[2],
                     nd[0]*rd[1]-nd[1]*rd[0] };
    norm3(td);
    rd[0] = td[1]*nd[2]-td[2]*nd[1];
    rd[1] = td[2]*nd[0]-td[0]*nd[2];
    rd[2] = td[0]*nd[1]-td[1]*nd[0];

    return Matrix3{{ {rd[0],rd[1],rd[2]},
                     {td[0],td[1],td[2]},
                     {nd[0],nd[1],nd[2]} }};
}


TextureLibrary::Matrix3 TextureLibrary::orientationFromMillerActive(const int hkl[3], const int uvw[3])
{
    Matrix3 g = orientationFromMiller(hkl, uvw);   // rows = RD,TD,ND
    Matrix3 a{};
    for (int i=0;i<3;++i) for (int j=0;j<3;++j) a[i][j]=g[j][i];
    return a;
}

TextureLibrary::Matrix3 TextureLibrary::orientationFromFiberAxis(const int axis[3], double azimuth)
{
    auto norm3 = [](double v[3]) {
        double n = std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
        if (n > EPS) { v[0]/=n; v[1]/=n; v[2]/=n; }
    };
    auto cross3 = [](const double a[3], const double b[3], double out[3]) {
        out[0]=a[1]*b[2]-a[2]*b[1];
        out[1]=a[2]*b[0]-a[0]*b[2];
        out[2]=a[0]*b[1]-a[1]*b[0];
    };

    double rd[3] = { (double)axis[0], (double)axis[1], (double)axis[2] };
    norm3(rd);

    double ref[3] = { std::fabs(rd[0]) < 0.9 ? 1.0 : 0.0,
                       std::fabs(rd[0]) < 0.9 ? 0.0 : 1.0,
                       0.0 };
    double dot = ref[0]*rd[0] + ref[1]*rd[1] + ref[2]*rd[2];
    double u1[3] = { ref[0]-dot*rd[0], ref[1]-dot*rd[1], ref[2]-dot*rd[2] };
    norm3(u1);
    double u2[3]; cross3(rd, u1, u2); norm3(u2);

    double ca = std::cos(azimuth), sa = std::sin(azimuth);
    double td[3] = { ca*u1[0]+sa*u2[0], ca*u1[1]+sa*u2[1], ca*u1[2]+sa*u2[2] };
    norm3(td);
    double nd[3]; cross3(rd, td, nd); norm3(nd);

    Matrix3 g{{ {rd[0],rd[1],rd[2]},
                {td[0],td[1],td[2]},
                {nd[0],nd[1],nd[2]} }};
    Matrix3 a{};
    for (int i=0;i<3;++i) for (int j=0;j<3;++j) a[i][j]=g[j][i];
    return a;
}

TextureLibrary::Matrix3 TextureLibrary::bungeToMatrix(double phi1, double Phi, double phi2)
{
    double p1 = phi1*DEG, P = Phi*DEG, p2 = phi2*DEG;
    double c1=std::cos(p1), s1=std::sin(p1);
    double c =std::cos(P),  s =std::sin(P);
    double c2=std::cos(p2), s2=std::sin(p2);

    // passive g (Bunge)
    Matrix3 g{{
        { c1*c2 - s1*s2*c,   s1*c2 + c1*s2*c,   s2*s },
        {-c1*s2 - s1*c2*c,  -s1*s2 + c1*c2*c,   c2*s },
        { s1*s,             -c1*s,              c    }
    }};
    // active = g^T
    Matrix3 a{};
    for (int i=0;i<3;++i) for (int j=0;j<3;++j) a[i][j]=g[j][i];
    return a;
}

void TextureLibrary::matrixToAnsys(const Matrix3& R,
                                   double& thxy, double& thyz, double& thzx,
                                   bool in_deg)
{
    double m12 = R[0][1], m22 = R[1][1];
    double m32 = R[2][1], m31 = R[2][0], m33 = R[2][2];
    if (m32 >  1.0) m32 =  1.0;
    if (m32 < -1.0) m32 = -1.0;

    double t1 = std::atan2(-m12, m22);
    double c2 = std::sqrt(1.0 - m32*m32);
    double t2, t3;
    if (c2 < EPS) {
        t2 = (m32 > 0) ? (M_PI/2.0) : (-M_PI/2.0);
        t1 = 0.0;
        t3 = 0.0;
    } else {
        t2 = std::atan2(m32, c2);
        t3 = std::atan2(-m31, m33);
    }
    thxy = t1; thyz = t2; thzx = t3;
    if (in_deg) { thxy*=RAD; thyz*=RAD; thzx*=RAD; }
}

TextureLibrary::Matrix3 TextureLibrary::randomMatrix(std::mt19937& rng)
{
    std::normal_distribution<double> nd(0.0, 1.0);
    double q0=nd(rng),q1=nd(rng),q2=nd(rng),q3=nd(rng);
    double n=std::sqrt(q0*q0+q1*q1+q2*q2+q3*q3);
    if (n<EPS) return Matrix3{{ {1,0,0},{0,1,0},{0,0,1} }};
    double w=q0/n,x=q1/n,y=q2/n,z=q3/n;
    return Matrix3{{
        { 1-2*(y*y+z*z),  2*(x*y - w*z),  2*(x*z + w*y) },
        { 2*(x*y + w*z),  1-2*(x*x+z*z),  2*(y*z - w*x) },
        { 2*(x*z - w*y),  2*(y*z + w*x),  1-2*(x*x+y*y) }
    }};
}

TextureLibrary::Matrix3 TextureLibrary::applyScatter(const Matrix3& ideal,
                                                     double scatter_deg,
                                                     std::mt19937& rng)
{
    if (scatter_deg < EPS) return ideal;

    std::normal_distribution<double> gauss(0.0, scatter_deg*DEG);
    std::uniform_real_distribution<double> uni(-1.0, 1.0);
    double ax[3]={uni(rng),uni(rng),uni(rng)};
    double an=std::sqrt(ax[0]*ax[0]+ax[1]*ax[1]+ax[2]*ax[2]);
    if (an<EPS) return ideal;
    ax[0]/=an; ax[1]/=an; ax[2]/=an;
    double ang=gauss(rng);
    double c=std::cos(ang), s=std::sin(ang), t=1-c;

    Matrix3 Rp{{
        { c+ax[0]*ax[0]*t,       ax[0]*ax[1]*t-ax[2]*s,  ax[0]*ax[2]*t+ax[1]*s },
        { ax[1]*ax[0]*t+ax[2]*s, c+ax[1]*ax[1]*t,        ax[1]*ax[2]*t-ax[0]*s },
        { ax[2]*ax[0]*t-ax[1]*s, ax[2]*ax[1]*t+ax[0]*s,  c+ax[2]*ax[2]*t       }
    }};
    return matmul(Rp, ideal);
}

const TextureLibrary::Component& TextureLibrary::pickComponent()
{
    if (m_components.size() == 1 || m_cumWeights.empty())
        return m_components[ std::uniform_int_distribution<size_t>(0, m_components.size()-1)(m_rng) ];

    std::uniform_real_distribution<double> uni(0.0, m_cumWeights.back());
    double r = uni(m_rng);
    for (size_t i=0;i<m_cumWeights.size();++i)
        if (r <= m_cumWeights[i]) return m_components[i];
    return m_components.back();
}

void TextureLibrary::sampleNext(double angl[3], bool in_deg)
{
    Matrix3 R;
    switch (m_mode) {
        case Mode::Cube:
            angl[0]=angl[1]=angl[2]=0.0;
            return;
        case Mode::Random:
            R = randomMatrix(m_rng);
            break;
        case Mode::Textured: {
            if (m_components.empty()) { angl[0]=angl[1]=angl[2]=0.0; return; }
            const Component& c = pickComponent();
            if (c.is_random) {
                R = randomMatrix(m_rng);
            } else if (c.is_fiber) {
                std::uniform_real_distribution<double> azi(0.0, 2.0*M_PI);
                Matrix3 ideal = orientationFromFiberAxis(c.uvw, azi(m_rng));
                R = applyScatter(ideal, c.scatter_deg, m_rng);
            } else {
                Matrix3 ideal = orientationFromMillerActive(c.hkl, c.uvw);
                R = applyScatter(ideal, c.scatter_deg, m_rng);
            }
            break;
        }
    }
    matrixToAnsys(R, angl[0], angl[1], angl[2], in_deg);
}

// ═══════════════════════════════════════════════════════════════════
//  {hkl}<uvw> -> Bunge
// ═══════════════════════════════════════════════════════════════════
void TextureLibrary::millerToBunge(const int hkl[3], const int uvw[3],
                                   double& phi1, double& Phi, double& phi2)
{
    Matrix3 g = orientationFromMiller(hkl, uvw);   // active (rows = RD,TD,ND)
    double gp[3][3];
    for (int i=0;i<3;++i) for (int j=0;j<3;++j) gp[i][j]=g[j][i];

    double P = std::acos(std::max(-1.0,std::min(1.0,gp[2][2])));
    double p1,p2;
    if (std::sin(P) < EPS) {
        p1 = std::atan2(gp[0][1], gp[0][0]);
        p2 = 0.0;
    } else {
        p1 = std::atan2(gp[2][0], -gp[2][1]);
        p2 = std::atan2(gp[0][2],  gp[1][2]);
    }
    phi1 = p1*RAD; Phi = P*RAD; phi2 = p2*RAD;
}

void TextureLibrary::bungeToAnsys(double phi1, double Phi, double phi2,
                                  double& thxy, double& thyz, double& thzx)
{
    Matrix3 R = bungeToMatrix(phi1, Phi, phi2);
    matrixToAnsys(R, thxy, thyz, thzx, true);
}

std::vector<TextureLibrary::Component> TextureLibrary::presetCatalog()
{
    return {
        { {0,0,1}, {1,0,0},  8.0, 1.0, "Cube {001}<100>"   },
        { {1,1,0}, {0,0,1},  8.0, 1.0, "Goss {110}<001>"   },
        { {1,1,2}, {1,1,-1}, 8.0, 1.0, "Copper {112}<11-1>"},
        { {1,1,0}, {-1,1,2}, 8.0, 1.0, "Brass {110}<112>"  },
        { {1,2,3}, {6,3,-4}, 8.0, 1.0, "S {123}<634>"      },
    };
}

std::vector<TextureLibrary::Component> TextureLibrary::processComponents(Process p)
{
    switch (p) {
    case Process::Extrusion:
        return {
            { {0,0,0}, {1,1,0}, 6.0, 1.0, "Fiber <110>||ED", false, true },
        };

    case Process::Rolling:
        return {
            { {1,1,2}, {1,1,-1}, 8.0, 1.0, "Copper {112}<11-1>" },
            { {1,2,3}, {6,3,-4}, 8.0, 1.0, "S {123}<634>"       },
            { {1,1,0}, {-1,1,2}, 8.0, 0.8, "Brass {110}<112>"   },
            { {0,0,1}, {1,0,0},  8.0, 0.3, "Cube {001}<100>"    },
        };

    case Process::Recrystallization:
        return {
            { {0,0,1}, {1,0,0}, 6.0, 1.0, "Cube {001}<100>" },
            { {1,1,0}, {0,0,1}, 6.0, 0.4, "Goss {110}<001>" },
        };

    case Process::Shear:
        return {
            { {1,1,1}, {1,-1,0}, 8.0, 1.0, "A {111}<1-10>" },
            { {1,1,2}, {1,-1,0}, 8.0, 0.8, "B {112}<1-10>" },
            { {0,0,1}, {1,1,0},  8.0, 0.8, "C {001}<110>"  },
        };

    case Process::Random:
        return {
            { {0,0,0}, {0,0,0}, 0.0, 1.0, "Random", true },
        };
    }
    return {};
}

std::string TextureLibrary::processName(Process p)
{
    switch (p) {
    case Process::Extrusion:         return "Extrusion";
    case Process::Rolling:           return "Rolling";
    case Process::Recrystallization: return "Recrystallization";
    case Process::Shear:             return "Torsion / Shear";
    case Process::Random:            return "Random";
    }
    return "";
}

std::string TextureLibrary::processDesc(Process p)
{
    switch (p) {
    case Process::Extrusion:         return "<110>||ED fiber";
    case Process::Rolling:           return "Sheet forming, earing";
    case Process::Recrystallization: return "Annealing, Cube / Goss";
    case Process::Shear:             return "A, B, C shear components";
    case Process::Random:            return "Uniform random grain orientations";
    }
    return "";
}

bool TextureLibrary::runSelfTest()
{
    struct Case { const char* name; int hkl[3]; int uvw[3]; };
    Case cases[] = {
        { "Cube",   {0,0,1},{1,0,0} },
        { "Goss",   {1,1,0},{0,0,1} },
        { "Brass",  {1,1,0},{-1,1,2} },
        { "Copper", {1,1,2},{1,1,-1} },
        { "S",      {1,2,3},{6,3,-4} },
    };
    bool ok = true;
    for (auto& c : cases) {
        Matrix3 R = orientationFromMillerActive(c.hkl, c.uvw);
        double tx,ty,tz;
        matrixToAnsys(R, tx,ty,tz, true);

        double dot = c.hkl[0]*c.uvw[0]+c.hkl[1]*c.uvw[1]+c.hkl[2]*c.uvw[2];
        bool valid = std::fabs(dot) < 1e-9;

        double det =
            R[0][0]*(R[1][1]*R[2][2]-R[1][2]*R[2][1])
          - R[0][1]*(R[1][0]*R[2][2]-R[1][2]*R[2][0])
          + R[0][2]*(R[1][0]*R[2][1]-R[1][1]*R[2][0]);
        bool ortho = std::fabs(det - 1.0) < 1e-6;

        bool pass = valid && ortho;
        std::printf("[TextureTest] %-6s {%d%d%d}<%d%d%d> -> ANSYS(%.3f,%.3f,%.3f) dot=%.2f det=%.4f %s\n",
                    c.name, c.hkl[0],c.hkl[1],c.hkl[2], c.uvw[0],c.uvw[1],c.uvw[2],
                    tx,ty,tz, dot, det, pass?"OK":"FAIL");
        ok = ok && pass;
    }

    {
        int hkl[3]={1,1,2}, uvw[3]={1,1,-1};
        Matrix3 R=orientationFromMillerActive(hkl,uvw);
        double tx,ty,tz; matrixToAnsys(R,tx,ty,tz,true);
        bool cu = std::fabs(tx-45)<0.2 && std::fabs(ty-0)<0.2 && std::fabs(tz-35.26)<0.2;
        std::printf("[TextureTest] Copper canonical (%.2f,%.2f,%.2f) expect (45,0,35.26) %s\n",
                    tx,ty,tz, cu?"OK":"FAIL");
        ok = ok && cu;
    }
    return ok;
}
