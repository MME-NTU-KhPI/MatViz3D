#ifndef ANSYSWRAPPER_H_INCLUDED
#define ANSYSWRAPPER_H_INCLUDED

#include <QString>
#include <QTemporaryDir>
#include <QHash>
#include <vector>

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

    void findPathVersion();
    void findNp();
    void defaultArgs();

    QString mergeVector(QString prefix, std::vector<double> vec);

    QTemporaryDir tempDir;
    QString exitCodeToText(int retcode);
    float calc_avg(QVector<float> &x);

    QHash <n3d::node3d, int> nodes;

    QHash <n3d::node3d, int> result_nodes;

    unsigned int seed;

public:
    ansysWrapper(bool isBatch);

    void setWorkingDirectory(QString path);
    void setSeed(unsigned int seed);

    bool run(QString apdl);
    bool run();
    int kp(double x, double y);
    int spline(std::vector<int> kps, int left_boundary, int right_boundary);
    int spline(std::vector<double> x, std::vector<double> y, int left_boundary = 1, int right_boundary = 1);
    void setMaterial(double E, double nu, double rho );
    void setAnisoMaterial(double c11, double c12, double c13, double c22, double c23, double c33, double c44, double c55, double c66);

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
    void createFEfromArray8Node(int32_t*** voxels, short int numCubes, int numSeeds, bool is_random_orientation=true);

    int createLocalCS(bool is_random_orientation = true, double x = 0.0, double y = 0.0, double z = 0.0);
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
    bool IsFaceNode(const n3d::node3d& node,
                    double x1, double y1, double z1,
                    double x2, double y2, double z2);

    void prep7();
    void clearBC();
    void saveAll();
    void load_loadstep(int num);

    float scaleValue01(float val, int component);
    float getValByCoord(float x, float y, float z, int component);
    float getValByCoord(n3d::node3d &key, int component);

    std::vector<std::vector<float>> loadstep_results;

    std::vector<float> loadstep_results_max;
    std::vector<float> loadstep_results_min;
    std::vector<float> loadstep_results_avg;
    std::vector<std::vector<float>> local_cs;
    std::vector<std::vector<float>> eps_as_loading;

};


#endif // ANSYSWRAPPER_H_INCLUDED
