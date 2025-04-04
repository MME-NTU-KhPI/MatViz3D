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

void LoadStepManager::loadLoadStep(int num, const QString& filePath)
{
    const int numColumns = 19; // ID;X;Y;Z;UX;UY;UZ;SX;SY;SZ;SXY;SYZ;SXZ;EpsX;EpsY;EpsZ;EpsXY;EpsYZ;EpsXZ
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << "Cannot open file to read results. Path to file:" << filePath;
        return;
    }

    loadstepResults.clear();
    QByteArray parts[numColumns];

    while (!file.atEnd()) {
        QByteArray line = file.readLine();
        if (line.startsWith("ID")) continue; // Skip header line

        loadstepResults.emplace_back(numColumns);
        for (int j = 0; j < numColumns; j++) {
            parts[j] = line.mid(j * 17, 16).trimmed();
            loadstepResults.back()[j] = parts[j].toFloat();
        }
    }

    loadstepResultsAvg.assign(numColumns, 0.0f);
    loadstepResultsMax.assign(numColumns, -FLT_MAX);
    loadstepResultsMin.assign(numColumns, FLT_MAX);

    resultNodes.clear();
    for (size_t i = 0; i < loadstepResults.size(); i++) {
        for (int j = 0; j < numColumns; j++) {
            loadstepResultsAvg[j] += loadstepResults[i][j];
            loadstepResultsMin[j] = std::min(loadstepResultsMin[j], loadstepResults[i][j]);
            loadstepResultsMax[j] = std::max(loadstepResultsMax[j], loadstepResults[i][j]);
        }
        n3d::node3d key = {loadstepResults[i][1], loadstepResults[i][2], loadstepResults[i][3]};
        resultNodes.insert(key, i);
    }

    for (int j = 0; j < numColumns; j++) {
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

    return 0;
}


float LoadStepManager::getMinVal(size_t comp)
{
    if (isValid() && comp < this->loadstepResultsMin.size())
        return loadstepResultsMin[comp];

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
