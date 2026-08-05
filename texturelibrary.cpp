#include "texturelibrary.h"
#include <cmath>
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

// {hkl}<uvw> -> active
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
    //RD: rd = td x nd
    rd[0] = td[1]*nd[2]-td[2]*nd[1];
    rd[1] = td[2]*nd[0]-td[0]*nd[2];
    rd[2] = td[0]*nd[1]-td[1]*nd[0];

    return Matrix3{{ {rd[0],rd[1],rd[2]},
                     {td[0],td[1],td[2]},
                     {nd[0],nd[1],nd[2]} }};
}


// {hkl}<uvw> -> ACTIVE
TextureLibrary::Matrix3 TextureLibrary::orientationFromMillerPassive(const int hkl[3], const int uvw[3])
{
    Matrix3 g = orientationFromMiller(hkl, uvw);   // rows = RD,TD,ND
    Matrix3 a{};
    for (int i=0;i<3;++i) for (int j=0;j<3;++j) a[i][j]=g[j][i];
    return a;
}

// Bunge (phi1,Phi,phi2) passive -> ACTIVE
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

// matrix -> ANSYS Z-X-Y (generate_random_angles)
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

TextureLibrary::Matrix3 TextureLibrary::fiberMatrix(const int uvw[3], std::mt19937& rng)
{
    double ax[3] = {(double)uvw[0],(double)uvw[1],(double)uvw[2]};
    double n = std::sqrt(ax[0]*ax[0]+ax[1]*ax[1]+ax[2]*ax[2]);
    if (n < EPS) return Matrix3{{ {1,0,0},{0,1,0},{0,0,1} }};
    ax[0]/=n; ax[1]/=n; ax[2]/=n;

    double t[3] = {1,0,0};
    if (std::fabs(ax[0]) > 0.9) { t[0]=0; t[1]=1; }
    double d = t[0]*ax[0]+t[1]*ax[1]+t[2]*ax[2];
    double e1[3] = { t[0]-d*ax[0], t[1]-d*ax[1], t[2]-d*ax[2] };
    double e1n = std::sqrt(e1[0]*e1[0]+e1[1]*e1[1]+e1[2]*e1[2]);
    e1[0]/=e1n; e1[1]/=e1n; e1[2]/=e1n;
    // e2 = ax x e1
    double e2[3] = { ax[1]*e1[2]-ax[2]*e1[1],
                     ax[2]*e1[0]-ax[0]*e1[2],
                     ax[0]*e1[1]-ax[1]*e1[0] };

    std::uniform_real_distribution<double> uni(0.0, 2.0*M_PI);
    double th = uni(rng);
    double ct = std::cos(th), st = std::sin(th);
    double rd[3] = { ct*e1[0]+st*e2[0], ct*e1[1]+st*e2[1], ct*e1[2]+st*e2[2] };
    double td[3] = {-st*e1[0]+ct*e2[0],-st*e1[1]+ct*e2[1],-st*e1[2]+ct*e2[2] };

    return Matrix3{{ {rd[0],rd[1],rd[2]},
                     {td[0],td[1],td[2]},
                     {ax[0],ax[1],ax[2]} }};
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
            }
            else if (c.is_fiber) {
                R = fiberMatrix(c.uvw, m_rng);
                R = applyScatter(R, c.scatter_deg, m_rng);
            }
            else {
                Matrix3 ideal = orientationFromMiller(c.hkl, c.uvw);
                R = applyScatter(ideal, c.scatter_deg, m_rng);
            }
            break;
        }
    }
    matrixToAnsys(R, angl[0], angl[1], angl[2], in_deg);
}

void TextureLibrary::bungeFromPassive(const Matrix3& g,
                                      double& phi1, double& Phi, double& phi2)
{
    const double GIMBAL = 1e-7;

    double c = std::max(-1.0, std::min(1.0, g[2][2]));
    double P = std::acos(c);
    double p1, p2;
    if (std::sin(P) < GIMBAL) {
        p1 = std::atan2(g[0][1], g[0][0]);
        p2 = 0.0;
    } else {
        p1 = std::atan2(g[2][0], -g[2][1]);
        p2 = std::atan2(g[0][2],  g[1][2]);
    }
    phi1 = p1 * RAD; if (phi1 < 0.0) phi1 += 360.0;
    Phi  = P  * RAD;
    phi2 = p2 * RAD; if (phi2 < 0.0) phi2 += 360.0;
}

// ═══════════════════════════════════════════════════════════════════
//  {hkl}<uvw> -> Bunge
// ═══════════════════════════════════════════════════════════════════
void TextureLibrary::millerToBunge(const int hkl[3], const int uvw[3],
                                   double& phi1, double& Phi, double& phi2)
{
    bungeFromPassive(orientationFromMillerPassive(hkl, uvw), phi1, Phi, phi2);
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

std::vector<TextureLibrary::Component>
TextureLibrary::componentsForProcess(Process p, Lattice lat, double scatter_deg)
{
    std::vector<Component> out;
    auto add = [&](int h,int k,int l,int u,int v,int w,double wt,const char* nm,
                   bool fiber=false){
        Component c;
        c.hkl[0]=h;c.hkl[1]=k;c.hkl[2]=l;
        c.uvw[0]=u;c.uvw[1]=v;c.uvw[2]=w;
        c.scatter_deg=scatter_deg; c.weight=wt; c.name=nm;
        c.is_fiber=fiber; c.is_random=false;
        out.push_back(c);
    };

    switch (p) {
    case Process::Random: {
        Component c; c.is_random=true; c.weight=1.0; c.scatter_deg=scatter_deg;
        c.name="Random"; out.push_back(c);
        break;
    }
    case Process::Extrusion:
        if (lat == Lattice::FCC) {
            add(0,0,0, 1,1,1, 0.6, "<111> fiber", true);
            add(0,0,0, 1,0,0, 0.4, "<100> fiber", true);
        } else {
            add(0,0,0, 1,1,0, 1.0, "<110> fiber", true);
        }
        break;

    case Process::Rolling:
        if (lat == Lattice::FCC) {
            add(1,1,2, 1,1,-1, 0.4, "Copper");
            add(1,2,3, 6,3,-4, 0.35,"S");
            add(1,1,0, -1,1,2, 0.25,"Brass");
        } else {
            // BCC: alpha-fiber <110>||RD + gamma-fiber <111>||ND
            add(0,0,1, 1,1,0, 0.5, "alpha <110>||RD", true);
            add(1,1,1, 1,1,0, 0.5, "gamma <111>||ND");
        }
        break;

    case Process::Recrystallization:
        add(0,0,1, 1,0,0, 0.6, "Cube");
        add(1,1,0, 0,0,1, 0.4, "Goss");
        break;

    case Process::Shear:
        add(0,0,1, 1,1,0, 0.5, "Shear A");
        add(1,1,1, 1,1,-2,0.5, "Shear C");
        break;
    }
    return out;
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
        Matrix3 R = orientationFromMiller(c.hkl, c.uvw);
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
        Matrix3 R=orientationFromMiller(hkl,uvw);
        double tx,ty,tz; matrixToAnsys(R,tx,ty,tz,true);
        bool cu = std::fabs(tx+39.23)<0.2 && std::fabs(ty-24.09)<0.2 && std::fabs(tz+26.57)<0.2;
        std::printf("[TextureTest] Copper canonical (%.2f,%.2f,%.2f) expect (-39.23,24.09,-26.57) %s\n",
                    tx,ty,tz, cu?"OK":"FAIL");
        ok = ok && cu;
    }
    return ok;
}

static const std::vector<TextureLibrary::Matrix3>& cubicOps()
{
    using Matrix3 = TextureLibrary::Matrix3;
    static std::vector<Matrix3> ops = []{
        std::vector<Matrix3> v;
        const int perm[6][3] = {{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
        for (int p = 0; p < 6; ++p)
            for (int s = 0; s < 8; ++s) {
                double sg[3] = { (s&1)?-1.0:1.0, (s&2)?-1.0:1.0, (s&4)?-1.0:1.0 };
                Matrix3 m{};
                for (int i = 0; i < 3; ++i) m[i][perm[p][i]] = sg[i];
                double det = m[0][0]*(m[1][1]*m[2][2]-m[1][2]*m[2][1])
                             - m[0][1]*(m[1][0]*m[2][2]-m[1][2]*m[2][0])
                             + m[0][2]*(m[1][0]*m[2][1]-m[1][1]*m[2][0]);
                if (std::fabs(det - 1.0) < 1e-9) v.push_back(m);
            }
        return v;
    }();
    return ops;
}

std::vector<std::array<double,3>>
TextureLibrary::fundamentalZoneBunge(double phi1, double Phi, double phi2)
{
    static const Matrix3 sampleOps[4] = {
        {{{ 1,0,0},{0, 1,0},{0,0, 1}}}, {{{ 1,0,0},{0,-1,0},{0,0,-1}}},
        {{{-1,0,0},{0, 1,0},{0,0,-1}}}, {{{-1,0,0},{0,-1,0},{0,0, 1}}}
    };
    Matrix3 a = bungeToMatrix(phi1, Phi, phi2), g{};
    for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) g[i][j] = a[j][i];

    std::vector<std::array<double,3>> out;
    const double T = 1e-6;
    for (const auto& c : cubicOps())
        for (const auto& s : sampleOps) {
            Matrix3 gg = matmul(matmul(c, g), s);
            double p1, P, p2;
            bungeFromPassive(gg, p1, P, p2);
            if (p1 > 90+T || P > 90+T || p2 > 90+T) continue;
            bool dup = false;
            for (const auto& e : out)
                if (std::fabs(e[0]-p1) < 0.1 && std::fabs(e[1]-P) < 0.1 && std::fabs(e[2]-p2) < 0.1)
                { dup = true; break; }
            if (!dup) out.push_back({p1, P, p2});
        }
    return out;
}

void TextureLibrary::sampleNextBunge(double bunge[3])
{
    Matrix3 R;
    switch (m_mode) {
    case Mode::Cube:
        bunge[0] = bunge[1] = bunge[2] = 0.0;
        return;
    case Mode::Random:
        R = randomMatrix(m_rng);
        break;
    case Mode::Textured: {
        if (m_components.empty()) { bunge[0]=bunge[1]=bunge[2]=0.0; return; }
        const Component& c = pickComponent();
        if (c.is_random)      R = randomMatrix(m_rng);
        else if (c.is_fiber)  R = applyScatter(fiberMatrix(c.uvw, m_rng), c.scatter_deg, m_rng);
        else                  R = applyScatter(orientationFromMiller(c.hkl, c.uvw), c.scatter_deg, m_rng);
        break;
    }
    }
    Matrix3 g{};
    for (int i=0;i<3;++i) for (int j=0;j<3;++j) g[i][j] = R[j][i];
    bungeFromPassive(g, bunge[0], bunge[1], bunge[2]);
}
