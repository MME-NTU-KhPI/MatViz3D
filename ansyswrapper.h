#ifndef ANSYSWRAPPER_H_INCLUDED
#define ANSYSWRAPPER_H_INCLUDED

#include <QString>
#include <QTemporaryDir>
#include <QHash>
#include <vector>
#include <array>
#include "texturelibrary.h"

enum tensor_components{ID,X,Y,Z,UX,UY,UZ,SX,SY,SZ,SXY,SYZ,SXZ,EpsX,EpsY,EpsZ,EpsXY,EpsYZ,EpsXZ, USUM, SEQV, EpsEQV};

namespace n3d
{

class node3d
{
public:
    float data[3];
    float operator [](size_t i) const
    {
        return data[i];
    }
    bool operator ==(const node3d& rhs) const
    {
        return std::memcmp(this->data, rhs.data, 3*sizeof(float)) == 0;
    }
    friend inline uint qHash(const node3d &key, uint seed);
};


inline uint qHash(const node3d &key, uint seed)
{
    float x = key.data[0],
          y = key.data[1],
          z = key.data[2];
    return qHashMulti(seed, x, y, z);

}

} //end of namespace n3d

class ansysWrapper
{
protected:
    int m_numCubes = 0;
    double m_porosity = 0.0;       // = 1.0 - m_solid_fraction
    int m_ansVersion;
    QString m_pathToAns;
    QStringList m_arg;
    QString m_projectPath;
    QString m_jobName;
    int m_np;
    int m_kpid;
    int m_lcs = 11;
    bool m_isBatch;
    QString m_apdl;
    /// Grain id -> ANSYS material number; empty means "material 1 everywhere".
    std::vector<int> m_grainMaterial;

    void findPathVersion();
    void findNp();
    void defaultArgs();
    QString ansysProduct() const;
    QString ansysSysDir() const;

    QString mergeVector(QString prefix, std::vector<double> vec);

    static bool solveSystem21x21(double A[21][21], double b[21], double x[21]);
    static bool invert6x6(const double A[6][6], double inv[6][6]);

    QTemporaryDir tempDir;
    QString exitCodeToText(int retcode);
    float calc_avg(QVector<float> &x);

    QHash<int, int> m_node_weights;

    QHash <n3d::node3d, int> nodes;
    TextureLibrary m_texture;
    bool m_useCustomTexture = false;

    QHash <n3d::node3d, int> result_nodes;

    unsigned int seed;

public:
    ansysWrapper(bool isBatch);

    void setWorkingDirectory(QString path);
    void setSeed(unsigned int seed);

    std::vector<int> ansys_to_voxel_map;
    double m_solid_fraction = 1.0; // solid phase fraction (based on voxels)

    bool run(QString apdl);
    bool run();
    int kp(double x, double y);
    int spline(std::vector<int> kps, int left_boundary, int right_boundary);
    int spline(std::vector<double> x, std::vector<double> y, int left_boundary = 1, int right_boundary = 1);
    void setMaterial(double E, double nu, double rho );
    void setAnisoMaterial(double c11, double c12, double c13, double c22, double c23, double c33, double c44, double c55, double c66);

    /**
     * @brief Emit one anisotropic material definition from a full symmetric
     *        Voigt 6x6 (order 11,22,33,23,13,12) in Pa.
     *
     * Call once per constituent, with matId 1, 2, ... The orthotropic overload
     * above is the matId = 1 special case, and a cubic material produces the
     * identical deck through either.
     */
    void setAnisoMaterial(int matId, const double C[6][6]);

    /**
     * @brief Grain id -> ANSYS material number, for multi-phase structures.
     *
     * Index is the grain id (0 unused), value is the material number a
     * setAnisoMaterial(matId, ...) call defined. Elements pick their material
     * up from here when the mesh is written; left empty, every element gets
     * material 1, which is what every single-phase caller wants.
     */
    void setGrainMaterials(const std::vector<int>& grainMaterial) { m_grainMaterial = grainMaterial; }

    void setSectionASEC(double area, double Ix, double r_out);
    void setSectionCTUBE(double r_Out, double r_In);

    void DKAll(int kp_num);
    void setAccelGlobal(double gx, double gy, double gz);

    void setLDiv(int ndiv);
    void setElem(bool linear = false);
    void setElemByNum(int num);

    void setSeismicSpectrum(std::vector<double> freq, std::vector<double> ax, std::vector<double> ay, std::vector<double> az);

    void mesh();

    void solve();
    void solveLS(int start, int end);

    void clear();
    bool clear_temp_data();

    QString getAnsExec();
    void setAnsExec(QString exec);

    QString getJobName();
    void setJobName(QString jobName);

    QString getProjectPath();
    void setProjectPath(QString path);

    void setNP(int np);

    void createFEfromArray(int32_t*** voxels, short int numCubes,int numSeeds, bool is_random_orientation = true);
    // sharedOrientations (Bunge ZXZ radians, index == grain id, size ==
    // nGrains+1), if non-empty, is used verbatim instead of generating a
    // random orientation per grain -- lets ANSYS and the FFT solver see the
    // exact same microstructure realization for a given seed. Falls back to
    // the old per-grain random generation when left empty.
    void createFEfromArray8Node(int32_t*** voxels, short int numCubes, int numSeeds, bool is_random_orientation=true,
                                const std::vector<std::array<double,3>>& sharedOrientations = {});

    int createLocalCS(bool is_random_orientation = true, double x = 0.0, double y = 0.0, double z = 0.0);
    // Prescribed-orientation overload: (phi1,Phi,phi2) Bunge ZXZ radians,
    // converted to ANSYS's THXY/THYZ/THZX convention (see bungeZXZtoAnsysZXY
    // in stressresult.h) instead of drawing a random rotation.
    int createLocalCS(double phi1_bunge, double Phi_bunge, double phi2_bunge, double x, double y, double z);
    void generate_random_angles(double *angl, bool in_deg=false, double epsilon=1e-6);

    void addStrainToBCMacroBlob();
    void addStrainToBCMacro(double eps_xx, double eps_yy, double eps_zz,
                            double eps_xy, double eps_xz, double eps_yz, int CubeSize);

    void applyTensBC(double x1, double y1, double z1,
                     double x2, double y2, double z2,
                     double epsx, double epsy, double epsz);
    void applyPureShearBC(double x1, double y1, double z1,
                          double x2, double y2, double z2,
                          const QString& shear_plane, double shear_disp);

    n3d::node3d EstimateDisplacement(const n3d::node3d& node,
                                                double x_origin, double y_origin, double z_origin,
                                                double epsilon_x, double epsilon_y, double epsilon_z,
                                                double epsilon_xy, double epsilon_xz, double epsilon_yz);
    void applyComplexLoads(double x1, double y1, double z1,
                           double x2, double y2, double z2,
                           double eps_x, double eps_y, double eps_z,
                           double eps_xy, double eps_xz, double eps_yz);
    // Periodic (RVE) boundary conditions: opposite-face node pairs are
    // coupled with CE constraint equations enforcing u(master) - u(slave) =
    // eps . (lattice vector), instead of prescribing a fixed displacement on
    // every boundary node. The reference corner (x1,y1,z1) is pinned to
    // remove rigid-body translation; the other seven corners get their exact
    // displacement prescribed directly since their position relative to the
    // reference is known exactly.
    //
    // solveNow: LSWRITE + LSSOLVE canNOT be used to batch several periodic load
    // cases. LSWRITE stores only the *loads* of a load step -- constraint
    // equations are database-level model data, so replaying six load steps
    // would apply the CEs of whichever load case was written last to all six.
    // With solveNow=true this sets TIME to the load case index and emits
    // FINISH,/SOL,SOLVE,FINISH,/POST1 right here instead, so each case is
    // solved against its own CEs. Editing CEs in /PREP7 between solves
    // restarts the analysis, which REWRITES the .rst instead of appending, so
    // the caller must extract each case's results immediately:
    //
    //     for (load : loads) { wr.applyPeriodicBC(..., true);
    //                          wr.saveAll(); wr.saveElementAverages(); }
    //
    // TIME is what names the per-case ls_<n>.csv / lse_<n>.csv that
    // load_loadstep(n) reads back. solveNow=false keeps the LSWRITE behaviour
    // for single-load-case callers that follow up with solveLS().
    void applyPeriodicBC(double x1, double y1, double z1,
                         double x2, double y2, double z2,
                         double eps_x, double eps_y, double eps_z,
                         double eps_xy, double eps_xz, double eps_yz,
                         bool solveNow = false);
    bool IsFaceNode(const n3d::node3d& node,
                    double x1, double y1, double z1,
                    double x2, double y2, double z2);

    void prep7();
    void clearBC();
    void saveAll();
    void load_loadstep(int num);

    // Element-averaged S/EPTO, as a fix for the bias in load_loadstep()'s
    // node-weighted average (nodal S/EPTO is extrapolated-and-smoothed
    // across neighboring elements, which under-samples boundary regions).
    // Every element here has identical volume (uniform voxel grid), so a
    // plain per-element mean -- unmixed with neighbors -- is an exact volume
    // average. Independent of saveAll()/load_loadstep(): call
    // saveElementAverages() once before run() (alongside saveAll()), then
    // loadElementAveragedResults(num) after load_loadstep(num) to overwrite
    // loadstep_results_avg[SX..EpsXZ] with the corrected values.
    void saveElementAverages();
    void loadElementAveragedResults(int num);

    float scaleValue01(float val, int component);
    float getValByCoord(float x, float y, float z, int component);
    float getValByCoord(n3d::node3d &key, int component);

    void setTextureMode(TextureLibrary::Mode m) { m_texture.setMode(m); m_useCustomTexture = false; }
    void setTextureComponents(const std::vector<TextureLibrary::Component>& c) { m_texture.setComponents(c); m_useCustomTexture = true; }

    struct ElasticProperties {
        double S[6][6]; // Compliance
        double C[6][6]; // Stiffness
        double P[6][6]; // Poisson's ratio matrix
        bool isValid;   // Calculation success flag
    };

    ElasticProperties calculateElasticProperties();

    std::vector<std::vector<float>> loadstep_results;

    std::vector<float> loadstep_results_max;
    std::vector<float> loadstep_results_min;
    std::vector<float> loadstep_results_avg;
    std::vector<std::vector<float>> local_cs;
    std::vector<std::vector<float>> eps_as_loading;

};


#endif // ANSYSWRAPPER_H_INCLUDED
