#include "AnisotropicCalculator.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <tuple>
using namespace std;

AnisotropicCalculator::AnisotropicCalculator(double C11, double C12, double C44, int Ngrid)
    : C11(C11), C12(C12), C44(C44), N(Ngrid)
{
    build_direction_grids();
    C = initialize_C();

    auto inv = invert6(C);
    if (!inv.first) {
        cerr << "Matrix C is singular!" << endl;
        throw runtime_error("Matrix C is singular!");
    }
    S = inv.second;
}

AnisotropicCalculator::Results AnisotropicCalculator::run() {
    // print_initial();

    int total = N*N;
    E_arr.resize(total);
    one_over_E_arr.resize(total);
    G_arr.resize(total);
    nu_arr.resize(total);

    for(int it=0; it<N; it++){
        for(int ip=0; ip<N; ip++){
            int idx = it*N + ip;

            double theta = thetas[it];
            double phi   = phis[ip];

            double n1,n2,n3,m1,m2,m3;
            compute_nm(theta, phi, n1,n2,n3, m1,m2,m3);

            auto [E,invE] = compute_E_oneoverE(n1,n2,n3);
            double G = compute_G(n1,n2,n3, m1,m2,m3);
            double nu = compute_nu(n1,n2,n3, m1,m2,m3, invE);

            E_arr[idx]  = E;
            one_over_E_arr[idx] = invE;
            G_arr[idx]  = G;
            nu_arr[idx] = nu;
        }
    }

    // Консольний вивід (оригінальна функція)
    // print_extrema();
    // Розрахунок результатів для повернення в GUI
    auto calc = [&](const vector<double>& arr){
        double mx=-1e100, mn=1e100;

        for(size_t i=0;i<arr.size();i++){
            if(arr[i] > mx){ mx=arr[i]; }
            if(arr[i] < mn){ mn=arr[i]; }
        }
        return tuple<double,double>(mx,mn);
    };

    auto [Emax,Emin] = calc(E_arr);
    auto [Gmax,Gmin] = calc(G_arr);
    auto [numax,numin] = calc(nu_arr);

    Results res;
    res.Emax = Emax;
    res.Emin = Emin;
    res.Gmax = Gmax;
    res.Gmin = Gmin;
    res.numax = numax;
    res.numin = numin;

    return res;
}

// ---------------------- PRIVATE METHODS ----------------------

void AnisotropicCalculator::build_direction_grids() {
    thetas.resize(N);
    phis.resize(N);
    for(int i=0;i<N;i++){
        // Використовуємо M_PI з <cmath>
        thetas[i] = (double)i * M_PI / (N-1);
        phis[i]   = (double)i * 2*M_PI / (N-1);
    }
}

AnisotropicCalculator::Mat6 AnisotropicCalculator::initialize_C() {
    Mat6 C{};
    for(int i=0;i<6;i++)
        for(int j=0;j<6;j++)
            C[i][j] = 0.0;

    C[0][0]=C11; C[0][1]=C12; C[0][2]=C12;
    C[1][0]=C12; C[1][1]=C11; C[1][2]=C12;
    C[2][0]=C12; C[2][1]=C12; C[2][2]=C11;

    C[3][3]=C44;
    C[4][4]=C44;
    C[5][5]=C44;

    return C;
}

AnisotropicCalculator::Vec6
AnisotropicCalculator::mat_vec(const Mat6 &A, const Vec6 &v) {
    Vec6 r{};
    for(int i=0;i<6;i++){
        double s=0.0;
        for(int j=0;j<6;j++) s += A[i][j]*v[j];
        r[i]=s;
    }
    return r;
}

double AnisotropicCalculator::dot6(const Vec6 &a, const Vec6 &b) {
    double s=0.0;
    for(int i=0;i<6;i++) s+=a[i]*b[i];
    return s;
}

pair<bool, AnisotropicCalculator::Mat6>
AnisotropicCalculator::invert6(const Mat6 &A) {

    double aug[6][12]{};

    for(int i=0;i<6;i++){
        for(int j=0;j<6;j++) aug[i][j]=A[i][j];
        for(int j=6;j<12;j++) aug[i][j]=(j-6==i?1.0:0.0);
    }

    for(int col=0; col<6; col++){
        int pivot = col;
        double maxv = fabs(aug[col][col]);

        for(int r=col+1;r<6;r++){
            double val=fabs(aug[r][col]);
            if(val>maxv){ maxv=val; pivot=r; }
        }

        if(maxv < 1e-15) return {false, Mat6{}};

        if(pivot != col)
            for(int c=0;c<12;c++)
                swap(aug[col][c], aug[pivot][c]);

        double diag=aug[col][col];
        for(int c=0;c<12;c++) aug[col][c]/=diag;

        for(int r=0;r<6;r++){
            if(r==col) continue;
            double factor = aug[r][col];
            for(int c=0;c<12;c++) aug[r][c] -= factor*aug[col][c];
        }
    }

    Mat6 Inv{};
    for(int i=0;i<6;i++)
        for(int j=0;j<6;j++)
            Inv[i][j]=aug[i][j+6];

    return {true, Inv};
}

void AnisotropicCalculator::compute_nm(double theta,double phi,
                                       double &n1,double &n2,double &n3,
                                       double &m1,double &m2,double &m3)
{
    n1 = sin(theta)*cos(phi);
    n2 = sin(theta)*sin(phi);
    n3 = cos(theta);

    m1 = cos(theta)*cos(phi);
    m2 = cos(theta)*sin(phi);
    m3 = -sin(theta);
}

pair<double,double>
AnisotropicCalculator::compute_E_oneoverE(double n1,double n2,double n3)
{
    Vec6 M{};
    M[0] = n1*n1;
    M[1] = n2*n2;
    M[2] = n3*n3;
    M[3] = n2*n3;
    M[4] = n1*n3;
    M[5] = n1*n2;

    Vec6 S_M = mat_vec(S, M);
    double invE = dot6(M, S_M);
    double E = (invE != 0.0 ? 1.0/invE : 1e30);

    return {E,invE};
}

double AnisotropicCalculator::compute_G(double n1,double n2,double n3,
                                        double m1,double m2,double m3)
{
    Vec6 Ms{};
    Ms[0]=n1*m1;
    Ms[1]=n2*m2;
    Ms[2]=n3*m3;
    Ms[3]=0.5*(n2*m3+n3*m2);
    Ms[4]=0.5*(n1*m3+n3*m1);
    Ms[5]=0.5*(n1*m2+n2*m1);

    Vec6 S_M = mat_vec(S, Ms);
    double invG = 4.0 * dot6(Ms, S_M);

    return (invG != 0.0 ? 1.0/invG : 1e30);
}

double AnisotropicCalculator::compute_nu(double n1,double n2,double n3,
                                         double m1,double m2,double m3,
                                         double invE)
{
    Vec6 Mn{}, Mm{};
    Mn[0]=n1*n1; Mn[1]=n2*n2; Mn[2]=n3*n3;
    Mn[3]=n2*n3; Mn[4]=n1*n3; Mn[5]=n1*n2;

    Mm[0]=m1*m1; Mm[1]=m2*m2; Mm[2]=m3*m3;
    Mm[3]=m2*m3; Mm[4]=m1*m3; Mm[5]=m1*m2;

    Vec6 S_Mn = mat_vec(S, Mn);
    double numer = dot6(Mm, S_Mn);

    return -numer / invE;
}

void AnisotropicCalculator::print_initial() const {
    cout << "C11="<<C11<<" C12="<<C12<<" C44="<<C44<<"\n\n";

    cout<<"Matrix C:\n";
    print_matrix(C);

    cout<<"Matrix S:\n";
    print_matrix(S);
}

void AnisotropicCalculator::print_matrix(const Mat6 &A){
    for(int i=0;i<6;i++){
        for(int j=0;j<6;j++)
            cout << setw(12) << A[i][j] << " ";
        cout << "\n";
    }
    cout<<"\n";
}

// void AnisotropicCalculator::print_extrema() {
//     auto calc = [&](const vector<double>& arr){
//         double mx=-1e100, mn=1e100;
//         int imax=0, imin=0;

//         for(size_t i=0;i<arr.size();i++){
//             if(arr[i] > mx){ mx=arr[i]; imax=i; }
//             if(arr[i] < mn){ mn=arr[i]; imin=i; }
//         }
//         return tuple<double,double,int,int>(mx,mn,imax,imin);
//     };

//     auto [Emax,Emin,iEmax,iEmin] = calc(E_arr);
//     auto [Gmax,Gmin,iGmax,iGmin] = calc(G_arr);
//     auto [numax,numin,inuMax,inuMin] = calc(nu_arr);

//     auto print = [&](string name,double mx,double mn,int imax,int imin){
//         int tmax = imax / N, pmax = imax % N;
//         int tmin = imin / N, pmin = imin % N;

//         cout << name << ":\n";
//         cout << " Max=" << fixed << setprecision(4) << mx
//              << " at θ=" << thetas[tmax] << "°"
//              << " φ=" << phis[pmax] << "°" << "\n";
//         cout << " Min=" << fixed << setprecision(4) << mn
//              << " at θ=" << thetas[tmin] << "°"
//              << " φ=" << phis[pmin] << "°" << "\n\n";
//     };

//     print("Young's modulus E(n)", Emax,Emin,iEmax,iEmin);
//     print("Shear modulus G(n,m)", Gmax,Gmin,iGmax,iGmin);
//     print("Poisson's ratio ν(n,m)", numax,numin,inuMax,inuMin);
// }

AnisotropicCalculator::ExtremumResult
AnisotropicCalculator::getExtrema(const std::vector<double>& arr) const
{
    ExtremumResult r{};

    double mx = -1e100, mn = 1e100;
    int imax = 0, imin = 0;

    for (int i = 0; i < arr.size(); i++)
    {
        if (arr[i] > mx) { mx = arr[i]; imax = i; }
        if (arr[i] < mn) { mn = arr[i]; imin = i; }
    }

    int tMax = imax / N;
    int pMax = imax % N;
    int tMin = imin / N;
    int pMin = imin % N;

    r.maxValue = mx;
    r.minValue = mn;
    r.thetaMax = thetas[tMax];
    r.phiMax   = phis[pMax];
    r.thetaMin = thetas[tMin];
    r.phiMin   = phis[pMin];


    return r;
}

