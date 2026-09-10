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
    loadstepResultsAvg.assign(numColumns + num_new_rows, 0.0f);
    loadstepResultsMax.assign(numColumns + num_new_rows, -FLT_MAX);
    loadstepResultsMin.assign(numColumns + num_new_rows, FLT_MAX);

    for (const auto& row : loadstepResults) {
        for (int j = 0; j < numColumns + num_new_rows; j++) {
            loadstepResultsAvg[j] += row[j];
            loadstepResultsMin[j] = std::min(loadstepResultsMin[j], row[j]);
            loadstepResultsMax[j] = std::max(loadstepResultsMax[j], row[j]);
        }
    }

    if (!loadstepResults.empty()) {
        for (int j = 0; j < numColumns + num_new_rows; j++) {
            loadstepResultsAvg[j] /= static_cast<float>(loadstepResults.size());
        }
    }
}

float LoadStepManager::scaleValue01(float val, int component) const
{
    if (component < 0 || component >= static_cast<int>(loadstepResultsMax.size()) || component >= static_cast<int>(loadstepResultsMin.size()))
        return 1.0f;
    float range = loadstepResultsMax[component] - loadstepResultsMin[component];
    if (std::fabs(range) > 1e-12f) {
        return (val - loadstepResultsMin[component]) / range;
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
    auto it = resultNodes.constFind(key);
    if (it != resultNodes.constEnd()) {
        int lineId = it.value();
        if (lineId >= 0 && lineId < static_cast<int>(loadstepResults.size())) {
            const auto& row = loadstepResults[lineId];
            if (component >= 0 && component < static_cast<int>(row.size())) {
                return row[component];
            }
        }
    }
    return 0.0f;
}


void LoadStepManager::createNodesHash()
{
    resultNodes.clear();
    n3d::node3d key;
    for (size_t i = 0; i < loadstepResults.size(); i++)
    {
        key.data[0] = loadstepResults[i][X];
        key.data[1] = loadstepResults[i][Y];
        key.data[2] = loadstepResults[i][Z];
        resultNodes.insert(key, static_cast<int>(i));
    }
}


void LoadStepManager::clearData()
{
    m_isValid = false;
    current_geom_set_num = 0;
    current_sub_set_num = 0;
    loadstepResults.clear();
    loadstepResultsAvg.clear();
    loadstepResultsMax.clear();
    loadstepResultsMin.clear();
    resultNodes.clear();
    geom_list.clear();
    geom_sub_list.clear();
    local_cs.clear();
    voxels_vector.clear();
    if (voxels) {
        Parent_Algorithm::Delete3D<int32_t>(voxels);
        voxels = nullptr;
    }
    cubeSize = 0;
    numPoints = 0;
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

    // Sort geom_list numerically if names are integers
    std::sort(geom_list.begin(), geom_list.end(), [](const QString& a, const QString& b) {
        bool okA = false, okB = false;
        int intA = a.toInt(&okA);
        int intB = b.toInt(&okB);
        if (okA && okB) return intA < intB;
        return a < b;
    });

    int firstSet = 1;
    if (!geom_list.isEmpty()) {
        bool ok = false;
        int parsed = geom_list.first().toInt(&ok);
        if (ok) firstSet = parsed;

        // Prefer the first set that actually contains geometry
        for (const auto& g : geom_list) {
            std::string p = "/" + g.toStdString();
            if (hdf5.datasetExists(p, "voxels") || hdf5.datasetExists(p + "/geometry", "voxels")) {
                firstSet = g.toInt();
                break;
            }
        }
    }

    bool res = LoadGeomSet(firstSet, hdf5);
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
    qDebug() << "Loading geom set" << geom_set_num;
    current_geom_set_num = geom_set_num;
    geom_sub_list.clear();
    loadstepResults.clear();
    loadstepResultsAvg.clear();
    loadstepResultsMax.clear();
    loadstepResultsMin.clear();
    resultNodes.clear();
    eps_as_loading.clear();
    local_cs.clear();
    voxels_vector.clear();

    std::string set_prefix = "/" + std::to_string(geom_set_num);
    std::string geom_sub = set_prefix + "/geometry";
    const bool has_geom_sub = hdf5.datasetExists(set_prefix, "geometry");

    // Read cubeSize and numPoints from prefix or geometry subgroup
    if (hdf5.datasetExists(set_prefix, "cubeSize")) {
        this->cubeSize = hdf5.readInt(set_prefix, "cubeSize");
    } else if (has_geom_sub && hdf5.datasetExists(geom_sub, "cubeSize")) {
        this->cubeSize = hdf5.readInt(geom_sub, "cubeSize");
    } else {
        this->cubeSize = 0;
    }

    if (hdf5.datasetExists(set_prefix, "numPoints")) {
        this->numPoints = hdf5.readInt(set_prefix, "numPoints");
    } else if (has_geom_sub && hdf5.datasetExists(geom_sub, "numPoints")) {
        this->numPoints = hdf5.readInt(geom_sub, "numPoints");
    } else {
        this->numPoints = 0;
    }

    // Read local coordinate system
    std::string cs_group = hdf5.datasetExists(set_prefix, "local_cs") ? set_prefix
                         : ((has_geom_sub && hdf5.datasetExists(geom_sub, "local_cs")) ? geom_sub : "");
    if (!cs_group.empty()) {
        this->local_cs = hdf5.readVectorVectorFloat(cs_group, "local_cs");
    }

    // Read voxels
    std::string vox_group = hdf5.datasetExists(set_prefix, "voxels") ? set_prefix
                          : ((has_geom_sub && hdf5.datasetExists(geom_sub, "voxels")) ? geom_sub : "");

    if (!vox_group.empty()) {
        this->voxels_vector = hdf5.readVoxels(vox_group, "voxels");
        if (this->cubeSize <= 0 && !this->voxels_vector.empty()) {
            this->cubeSize = static_cast<int>(this->voxels_vector.size());
        }

        if (voxels)
        {
            Parent_Algorithm::Delete3D<int32_t>(this->voxels);
            voxels = nullptr;
        }

        if (cubeSize > 0 && !voxels_vector.empty()) {
            voxels = Parent_Algorithm::Create3D<int32_t>(cubeSize, cubeSize, cubeSize);
            for (int i = 0; i < cubeSize; i++)
                for (int j = 0; j < cubeSize; j++)
                    for (int k = 0; k < cubeSize; k++)
                    {
                        voxels[i][j][k] = voxels_vector[i][j][k];
                    }
        }
    } else {
        qWarning() << "Geom set" << geom_set_num << "has no voxel geometry (cubeSize ="
                   << cubeSize << ") -- viewing without 3D voxels";
        if (voxels) {
            Parent_Algorithm::Delete3D<int32_t>(this->voxels);
            voxels = nullptr;
        }
    }

    qDebug() << "\tGeom Set:" << geom_set_num;
    qDebug() << "\tCube Size:" << cubeSize;
    qDebug() << "\tNum Points:" << numPoints;
    qDebug() << "\tLocal Coordinate System:" << local_cs.size() << "elements";
    qDebug() << "\tVoxels:" << voxels_vector.size() << "elements";

    LoadGeomSubStep(geom_set_num, 1, hdf5);
    m_isValid = (voxels != nullptr || hasLoadStepData() || hdf5.datasetExists(set_prefix, "C_matrix") || hdf5.datasetExists(set_prefix, "S_matrix"));
    return m_isValid;
}

bool LoadStepManager::LoadGeomSubStep(int sub_set_num)
{
    HDF5Wrapper hdf5(m_filePath.toStdString());
    return this->LoadGeomSubStep(current_geom_set_num, sub_set_num, hdf5);
}

bool LoadStepManager::LoadGeomSubStep(int geom_set_num, int sub_set_num)
{
    HDF5Wrapper hdf5(m_filePath.toStdString());
    return this->LoadGeomSubStep(geom_set_num, sub_set_num, hdf5);
}

bool LoadStepManager::LoadGeomSubStep(int geom_set_num, int sub_set_num, HDF5Wrapper& hdf5)
{
    qDebug() << "Loading geom sub step. Geom id = " << geom_set_num << "; Sub_set id " << sub_set_num;
    current_geom_set_num = geom_set_num;
    current_sub_set_num = sub_set_num;
    std::string set_prefix = "/" + std::to_string(geom_set_num);
    std::string ls_name = "ls_" + std::to_string(sub_set_num);
    std::string ls_prefix = set_prefix + "/" + ls_name;

    bool res = false;
    if (hdf5.datasetExists(ls_prefix, "results"))
    {
        this->loadstepResultsAvg = hdf5.readVectorFloat(ls_prefix, "results_avg");
        this->loadstepResultsMax = hdf5.readVectorFloat(ls_prefix, "results_max");
        this->loadstepResultsMin = hdf5.readVectorFloat(ls_prefix, "results_min");
        this->loadstepResults = hdf5.readVectorVectorFloat(ls_prefix, "results");
        this->eps_as_loading = hdf5.readVectorFloat(ls_prefix, "eps_as_loading");

        this->createNodesHash();
        res = true;
        calculateVonMisesStressAndStrain();
    }

    if (geom_sub_list.isEmpty() || !res)
    {
        std::vector<std::string> loadSteps = hdf5.listDataGroups(set_prefix);
        if (!res)
        {
            for (const auto& group : loadSteps)
            {
                if (group == ls_name)
                {
                    std::string group_prefix = set_prefix + "/" + group;
                    this->loadstepResultsAvg = hdf5.readVectorFloat(group_prefix, "results_avg");
                    this->loadstepResultsMax = hdf5.readVectorFloat(group_prefix, "results_max");
                    this->loadstepResultsMin = hdf5.readVectorFloat(group_prefix, "results_min");
                    this->loadstepResults = hdf5.readVectorVectorFloat(group_prefix, "results");
                    this->eps_as_loading = hdf5.readVectorFloat(group_prefix, "eps_as_loading");

                    this->createNodesHash();
                    res = true;
                    calculateVonMisesStressAndStrain();
                    break;
                }
            }
        }

        if (geom_sub_list.isEmpty())
        {
            for (const auto& group : loadSteps)
            {
                if (group.find("ls_") != std::string::npos)
                {
                    geom_sub_list.push_back(QString::fromStdString(group));
                }
            }
            std::sort(geom_sub_list.begin(), geom_sub_list.end(), [](const QString& a, const QString& b) {
                int numA = 0, numB = 0;
                if (a.startsWith("ls_")) numA = a.mid(3).toInt();
                if (b.startsWith("ls_")) numB = b.mid(3).toInt();
                return numA < numB;
            });
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
