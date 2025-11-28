#ifndef ANISOTROPICCALCULATOR_H
#define ANISOTROPICCALCULATOR_H

#include <array>
#include <vector>
#include <string>

class AnisotropicCalculator {
public:
    using Vec6 = std::array<double,6>;
    using Mat6 = std::array<Vec6,6>;

    struct Results {
        double Emax, Emin;
        double Gmax, Gmin;
        double numax, numin;
    };
    struct ExtremumResult {
        double maxValue;
        double minValue;
        double thetaMax;
        double phiMax;
        double thetaMin;
        double phiMin;
    };

    AnisotropicCalculator(double C11, double C12, double C44, int Ngrid);

    ExtremumResult getExtrema(const std::vector<double>& arr) const;
    Results run();
    const std::vector<double>& getE() const { return E_arr; }
    const std::vector<double>& getG() const { return G_arr; }
    const std::vector<double>& getNu() const { return nu_arr; }


private:
    double C11, C12, C44;
    int N;

    std::vector<double> thetas, phis;
    std::vector<double> E_arr, one_over_E_arr, G_arr, nu_arr;

    Mat6 C{}, S{};

    void build_direction_grids();
    Mat6 initialize_C();
    static Vec6 mat_vec(const Mat6 &A, const Vec6 &v);
    static double dot6(const Vec6 &a, const Vec6 &b);
    static std::pair<bool, Mat6> invert6(const Mat6 &A);

    void compute_nm(double theta,double phi,
                    double &n1,double &n2,double &n3,
                    double &m1,double &m2,double &m3);

    std::pair<double,double> compute_E_oneoverE(double n1,double n2,double n3);
    double compute_G(double n1,double n2,double n3,
                     double m1,double m2,double m3);
    double compute_nu(double n1,double n2,double n3,
                      double m1,double m2,double m3,
                      double one_over_E);

    void print_initial() const;
    static void print_matrix(const Mat6 &A);
    void print_extrema();
};

#endif // ANISOTROPICCALCULATOR_H
