// ============================================================================
//  matviz_homog.hpp  -  MatViz3D adapter for the FFT homogenizer.
//
//  Turns a MatViz3D voxel microstructure (grain-id field + per-grain Bunge ZXZ
//  Euler angles) into inputs for ffth::FFTHomogenizer, and reports the
//  effective (homogenized) stiffness plus a few derived engineering scalars.
//
//  Depends only on fft_homog.hpp (header-only).  Pure C++17, no Qt required
//  here so it stays unit-testable; the Qt bridge lives in a separate file.
// ============================================================================
#pragma once
#include "fft_homog.hpp"
#include <array>
#include <vector>
#include <functional>
#include <cmath>

namespace mvh {

using ffth::Mat6;
using Mat3 = std::array<std::array<double,3>,3>;
using C4   = std::array<std::array<std::array<std::array<double,3>,3>,3>,3>;

// --- Bunge ZXZ orientation matrix g:  [v]_crystal = g [v]_sample ------------
//  g = Rz(phi2) * Rx(Phi) * Rz(phi1)   (angles in radians)
inline Mat3 bunge_zxz(double phi1, double Phi, double phi2) {
    const double c1=std::cos(phi1), s1=std::sin(phi1);
    const double c =std::cos(Phi),  s =std::sin(Phi);
    const double c2=std::cos(phi2), s2=std::sin(phi2);
    Mat3 g;
    g[0][0]= c1*c2 - s1*s2*c;  g[0][1]= s1*c2 + c1*s2*c;  g[0][2]= s2*s;
    g[1][0]=-c1*s2 - s1*c2*c;  g[1][1]=-s1*s2 + c1*c2*c;  g[1][2]= c2*s;
    g[2][0]= s1*s;             g[2][1]=-c1*s;             g[2][2]= c;
    return g;
}

// --- Cubic single-crystal stiffness (crystal axes) as a 4th-order tensor -----
inline C4 cubic_C4(double C11, double C12, double C44) {
    C4 C{};
    for (int i=0;i<3;++i)for(int j=0;j<3;++j)for(int k=0;k<3;++k)for(int l=0;l<3;++l)
        C[i][j][k][l]=0.0;
    auto d=[](int a,int b){return a==b?1.0:0.0;};
    // cubic: C_ijkl = C12 d_ij d_kl + C44 (d_ik d_jl + d_il d_jk)
    //                 + (C11 - C12 - 2 C44) * delta on coincident axes
    const double H = C11 - C12 - 2.0*C44;
    for(int i=0;i<3;++i)for(int j=0;j<3;++j)for(int k=0;k<3;++k)for(int l=0;l<3;++l){
        C[i][j][k][l] = C12*d(i,j)*d(k,l)
                      + C44*(d(i,k)*d(j,l)+d(i,l)*d(j,k))
                      + (i==j&&j==k&&k==l ? H : 0.0);
    }
    return C;
}

// --- Rotate a 4th-order tensor from crystal frame into the sample frame ------
//  C^sample_abcd = g_ia g_jb g_kc g_ld C^crystal_ijkl
inline C4 rotate_C4(const C4& Cc, const Mat3& g) {
    C4 Cs{};
    for(int a=0;a<3;++a)for(int b=0;b<3;++b)for(int c=0;c<3;++c)for(int d=0;d<3;++d){
        double s=0.0;
        for(int i=0;i<3;++i)for(int j=0;j<3;++j)for(int k=0;k<3;++k)for(int l=0;l<3;++l)
            s += g[i][a]*g[j][b]*g[k][c]*g[l][d]*Cc[i][j][k][l];
        Cs[a][b][c][d]=s;
    }
    return Cs;
}

// --- 4th-order tensor -> Mandel 6x6 -----------------------------------------
inline Mat6 to_mandel(const C4& C) {
    static const int I[6]={0,1,2,1,0,0};
    static const int J[6]={0,1,2,2,2,1};
    const double f[6]={1,1,1,std::sqrt(2.0),std::sqrt(2.0),std::sqrt(2.0)};
    Mat6 M{};
    for(int a=0;a<6;++a)for(int b=0;b<6;++b)
        M[a][b]=f[a]*f[b]*C[I[a]][J[a]][I[b]][J[b]];
    return M;
}

// One-shot: cubic grain with given orientation -> Mandel 6x6 (sample frame)
inline Mat6 cubic_grain_mandel(double C11,double C12,double C44,
                               double phi1,double Phi,double phi2) {
    return to_mandel(rotate_C4(cubic_C4(C11,C12,C44), bunge_zxz(phi1,Phi,phi2)));
}

// Zener anisotropy of a cubic crystal (=1 for isotropic)
inline double zener(double C11,double C12,double C44){ return 2.0*C44/(C11-C12); }

} // namespace mvh
