#ifndef ANSYSWRAPPER_H_INCLUDED
#define ANSYSWRAPPER_H_INCLUDED

#include <vector>
#include <QString>
#include <QTemporaryDir>

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

	public:
		ansysWrapper(bool isBatch);
        void run(QString apdl);
		void run();
        int kp(double x, double y);
        int spline(std::vector<int> kps, int left_boundary, int right_boundary);
        int spline(std::vector<double> x, std::vector<double> y, int left_boundary = 1, int right_boundary = 1);
        void setMaterial(double E, double nu, double rho );

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

        QString getAnsExec();
        void setAnsExec(QString exec);

        QString getJobName();
        void setJobName(QString jobName);

        QString getProjectPath();
        void setProjectPath(QString path);

        void setNP(int np);

        void createFEfromArray(int16_t*** voxels, short int numCubes,int numSeeds);
        int createLocalCS();
        static void generate_random_angles(double *angl, bool in_deg=false, double epsilon=1e-6);
        void applyTensBC(double x1, double y1, double z1,
                         double x2, double y2, double z2,
                         double epsx, double epsy, double epsz);

        void prep7();
        void clearBC();
        void saveAll();
};


#endif // ANSYSWRAPPER_H_INCLUDED
