#ifndef LOADSTEPMANAGER_H
#define LOADSTEPMANAGER_H

#include <vector>
#include <QHash>
#include <QString>
#include <QDebug>
#include "ansyswrapper.h"
#include "hdf5wrapper.h"

class LoadStepManager {
public:
    static LoadStepManager& getInstance(); // Singleton accessor

    void loadLoadStep(int num, const QString& filePath);
    bool LoadGeomSubStep(int sub_set_num);
    bool LoadGeomSet(int geom_set_num);
    float scaleValue01(float val, int component) const;
    float getValByCoord(float x, float y, float z, int component) const;
    float getValByCoord(const n3d::node3d& key, int component) const;

    QStringList getGeomSetList();
    QStringList getGeomSetSubList();

    void clearData();
    bool LoadFromHDF5(const QString& filePath);
    bool isValid();
    float getMaxVal(size_t comp);
    float getMinVal(size_t comp);
    int getCubeSize();
    int getNumPoints();

    int32_t *** getVoxelPtr();

    void calculateVonMisesStressAndStrain();

private:
    LoadStepManager(); // Private constructor
    LoadStepManager(const LoadStepManager&) = delete; // Delete copy constructor
    LoadStepManager& operator=(const LoadStepManager&) = delete; // Delete assignment operator



protected:
    bool LoadGeomSet(int geom_set_num, HDF5Wrapper& hdf5);
    bool LoadGeomSubStep(int geom_set_num, int sub_set_num, HDF5Wrapper& hdf5);

    void createNodesHash();

    int last_set = 0;
    int selected_geom_set = 0;
    int cubeSize = 0;
    int numPoints = 0;

    bool m_isValid = false;
    QString m_filePath;

    int current_geom_set_num = 0;

    int32_t ***voxels = nullptr;
    std::vector<std::vector<std::vector<int32_t>>> voxels_vector;
    std::vector<std::vector<float>> local_cs;
    std::vector<std::vector<float>> loadstepResults;
    std::vector<float> loadstepResultsAvg;
    std::vector<float> loadstepResultsMax;
    std::vector<float> loadstepResultsMin;
    std::vector<float> eps_as_loading;
    QHash<n3d::node3d, int> resultNodes;

    QStringList geom_list;
    QStringList geom_sub_list;

};

#endif // LOADSTEPMANAGER_H
