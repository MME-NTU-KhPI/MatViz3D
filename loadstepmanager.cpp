#include "loadstepmanager.h"
#include "hdf5wrapper.h"
#include "parent_algorithm.h"
#include <QFile>
#include <QTextStream>
#include <cfloat>
#include <cmath>

// Static instance of the singleton
LoadStepManager& LoadStepManager::getInstance()
{
    static LoadStepManager instance;
    return instance;
}

LoadStepManager::LoadStepManager()
{

}


bool LoadStepManager::isValid()
{
    return this->m_isValid;
}


void LoadStepManager::calculateVonMisesStressAndStrain()
{
    const int numColumns = 19; // Original number of columns
    const int usumIndex = numColumns; // Index for von Mises stress
    const int stressIndex = numColumns + 1; // Index for von Mises stress
    const int strainIndex = numColumns + 2; // Index for von Mises strain
    const int num_new_rows = 3;
    for (auto& row : loadstepResults) {
        row.resize(numColumns + 3); // Add three new columns for displ sum, stress and strain

        // Calculate von Mises stress
        float sx = row[SX], sy = row[SY], sz = row[SZ];
        float sxy = row[SXY], syz = row[SYZ], sxz = row[SXZ];
        row[stressIndex] = std::sqrt(0.5f * ((sx - sy) * (sx - sy) + (sy - sz) * (sy - sz) + (sz - sx) * (sz - sx)) +
                                     3.0f * (sxy * sxy + syz * syz + sxz * sxz));

        // Calculate von Mises strain
        float epsX = row[EpsX], epsY = row[EpsY], epsZ = row[EpsZ];
        float epsXY = row[EpsXY], epsYZ = row[EpsYZ], epsXZ = row[EpsXZ];
        row[strainIndex] = std::sqrt(0.5f * ((epsX - epsY) * (epsX - epsY) + (epsY - epsZ) * (epsY - epsZ) + (epsZ - epsX) * (epsZ - epsX)) +
                                     3.0f * (epsXY * epsXY + epsYZ * epsYZ + epsXZ * epsXZ));

        // Calculate displacement sum
        float ux = row[UX], uy = row[UY], uz = row[UZ];
        row[usumIndex] = std::sqrt(ux * ux + uy * uy + uz * uz);
    }

    // Update avg, max, and min arrays
    loadstepResultsAvg.resize(numColumns + num_new_rows, 0.0f);
    loadstepResultsMax.resize(numColumns + num_new_rows, -FLT_MAX);
    loadstepResultsMin.resize(numColumns + num_new_rows, FLT_MAX);

    for (const auto& row : loadstepResults) {
        for (int j = 0; j < numColumns + num_new_rows; j++) {
            loadstepResultsAvg[j] += row[j];
            loadstepResultsMin[j] = std::min(loadstepResultsMin[j], row[j]);
            loadstepResultsMax[j] = std::max(loadstepResultsMax[j], row[j]);
        }
    }

    for (int j = 0; j < numColumns + num_new_rows; j++) {
        loadstepResultsAvg[j] /= static_cast<float>(loadstepResults.size());
    }
}

float LoadStepManager::scaleValue01(float val, int component) const
{
    if (fabs(loadstepResultsMax[component] - loadstepResultsMin[component]) > 0) {
        return (val - loadstepResultsMin[component]) / (loadstepResultsMax[component] - loadstepResultsMin[component]);
    }
    return 1.0f;
}


float LoadStepManager::getValByCoord(float x, float y, float z, int component) const
{
    n3d::node3d key = {x, y, z};
    return getValByCoord(key, component);
}


float LoadStepManager::getValByCoord(const n3d::node3d& key, int component) const
{
    if (resultNodes.contains(key)) {
        int lineId = resultNodes[key];
        return loadstepResults[lineId][component];
    }
    qDebug() << "Coordinate not found:" << key[0] << key[1] << key[2];
    return 0.0f;
}


void LoadStepManager::createNodesHash()
{
    n3d::node3d key;
    for (size_t i = 0; i < loadstepResults.size(); i++)
    {
        key.data[0] = loadstepResults[i][X];
        key.data[1] = loadstepResults[i][Y];
        key.data[2] = loadstepResults[i][Z];
        resultNodes.insert(key, i);
    }
}


void LoadStepManager::clearData()
{
    m_isValid = false;
    loadstepResults.clear();
    loadstepResultsAvg.clear();
    loadstepResultsMax.clear();
    loadstepResultsMin.clear();
    resultNodes.clear();
    geom_list.clear();
    geom_sub_list.clear();
}


bool LoadStepManager::LoadFromHDF5(const QString& filePath)
{
    this->m_filePath = filePath;
    HDF5Wrapper hdf5(filePath.toStdString());

    this->clearData();

    last_set = -1;
    // Read the last set number
    this->last_set = hdf5.readInt("/", "last_set");
    if (last_set == -1)
    {
        qCritical() << "No data found in the HDF5 file.";
        return false;
    }

    qDebug() << "Find geometry sets:" << last_set;

    auto s_list = hdf5.listDataGroups("/");

    for (const auto& s : s_list)
    {
        if (s != "last_set")
        {
            geom_list.push_back(QString::fromStdString(s));
        }
    }

    bool res = LoadGeomSet(1, hdf5);
    m_isValid = res;
    return res;
}

bool LoadStepManager::LoadGeomSet(int geom_set_num)
{
    HDF5Wrapper hdf5(m_filePath.toStdString());
    return LoadGeomSet(geom_set_num, hdf5);
}

bool LoadStepManager::LoadGeomSet(int geom_set_num, HDF5Wrapper& hdf5)
{
    qDebug() << "Loaing geom set" << geom_set_num;
    current_geom_set_num = geom_set_num;
    std::string set_prefix = "/" + std::to_string(geom_set_num);
    // Read voxels
    this->cubeSize = hdf5.readInt(set_prefix, "cubeSize");
    this->numPoints = hdf5.readInt(set_prefix, "numPoints");
    this->local_cs = hdf5.readVectorVectorFloat(set_prefix, "local_cs");
    this->voxels_vector = hdf5.readVoxels(set_prefix, "voxels");
    qDebug() << "\tGeom Set:" << geom_set_num;
    qDebug() << "\tCube Size:" << cubeSize;
    qDebug() << "\tNum Points:" << numPoints;
    qDebug() << "\tLocal Coordinate System:" << local_cs.size() << "elements";
    qDebug() << "\tVoxels:" << voxels_vector.size() << "elements";

    if (voxels)
    {
        Parent_Algorithm::Delete3D<int32_t>(this->voxels);
    }

    voxels = Parent_Algorithm::Create3D<int32_t>(cubeSize, cubeSize, cubeSize);
    for (int i = 0; i < cubeSize; i++)
        for (int j = 0; j < cubeSize; j++)
            for (int k = 0; k < cubeSize; k++)
            {
                voxels[i][j][k] = voxels_vector[i][j][k];
            }

    return LoadGeomSubStep(geom_set_num, 1, hdf5);
}

bool LoadStepManager::LoadGeomSubStep(int sub_set_num)
{
    HDF5Wrapper hdf5(m_filePath.toStdString());
    return this->LoadGeomSubStep(current_geom_set_num, sub_set_num, hdf5);
}

bool LoadStepManager::LoadGeomSubStep(int geom_set_num, int sub_set_num, HDF5Wrapper& hdf5)
{
    qDebug() << "Loading geom sub step. Geom id = " << geom_set_num << "; Sub_set id " << sub_set_num;
    std::string set_prefix = "/" + std::to_string(geom_set_num);
    // Iterate through load steps
    std::vector<std::string> loadSteps = hdf5.listDataGroups(set_prefix);
    bool res = false;
    geom_sub_list.clear();
    for (const auto& group : loadSteps)
    {
        std::string ls_name = "ls_" + std::to_string(sub_set_num);
        if (group == ls_name) // Check if the group is a load step
        {
            std::string ls_prefix = set_prefix + "/" + group;

            this->loadstepResultsAvg = hdf5.readVectorFloat(ls_prefix, "results_avg");
            this->loadstepResultsMax = hdf5.readVectorFloat(ls_prefix, "results_max");
            this->loadstepResultsMin = hdf5.readVectorFloat(ls_prefix, "results_min");
            this->loadstepResults = hdf5.readVectorVectorFloat(ls_prefix, "results");
            this->eps_as_loading = hdf5.readVectorFloat(ls_prefix, "eps_as_loading");

            qDebug() << "\tLoad Step:" << group.c_str();
            qDebug() << "\tResults Avg:" << loadstepResultsAvg.size() << "elements";
            qDebug() << "\tResults Max:" << loadstepResultsMax.size() << "elements";
            qDebug() << "\tResults Min:" << loadstepResultsMin.size() << "elements";
            qDebug() << "\tResults:" << loadstepResults.size() << "rows";
            qDebug() << "\tEps as Loading:" << eps_as_loading.size() << "elements";

            this->createNodesHash();

            res =  true;
            calculateVonMisesStressAndStrain();
        }

        if (group.find("ls_") != std::string::npos)
        {
            geom_sub_list.push_back(QString::fromStdString(group));
        }
    }
    return res;
}


QStringList LoadStepManager::getGeomSetList()
{
    return geom_list;
}


QStringList LoadStepManager::getGeomSetSubList()
{
    return geom_sub_list;
}


float LoadStepManager::getMaxVal(size_t comp)
{
    if (isValid() && comp < this->loadstepResultsMax.size())
        return loadstepResultsMax[comp];

    qCritical() << "LoadStepManager::getMaxVal. Index comp is out of range. Comp = " << comp << "; size() = " << this->loadstepResultsMax.size();
    return 0;
}


float LoadStepManager::getMinVal(size_t comp)
{
    if (isValid() && comp < this->loadstepResultsMin.size())
        return loadstepResultsMin[comp];

    qCritical() << "LoadStepManager::getMinVal. Index comp is out of range. Comp = " << comp << "; size() = " << this->loadstepResultsMin.size();
    return 0;
}


int32_t *** LoadStepManager::getVoxelPtr()
{
    return voxels;
}


int LoadStepManager::getCubeSize()
{
    return cubeSize;
}


int LoadStepManager::getNumPoints()
{
    return numPoints;
}
