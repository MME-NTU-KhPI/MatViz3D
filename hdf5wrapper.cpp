#include "hdf5wrapper.h"
#include "stressresult.h"
#include "parameters.h"
#include "algorithmfactory.h"
#include "openglwidgetqml.h"
#include <QFile>
#include <QDebug>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonValue>

HDF5Wrapper::HDF5Wrapper(const std::string& fileName)
{
    // Silence HDF5's default error stack printing to stderr.
    // Missing datasets during probes are completely expected and handled by return codes.
    H5Eset_auto(H5E_DEFAULT, NULL, NULL);

    if (fileName.empty()) {
        qDebug() << "Error: HDF5 file name is empty!";
        return;
    }

    bool is_file_exist = QFile::exists(QString::fromStdString(fileName));

    if (is_file_exist) {
        qDebug() << "HDF5 file already exists, opening for appending:" << fileName.c_str();
        file = H5Fopen(fileName.c_str(), H5F_ACC_RDWR, H5P_DEFAULT);
        if (file < 0) {
            qDebug() << "Error: Cannot open existing file:" << fileName.c_str();
        }
        return;
    }

    file = H5Fcreate(fileName.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    if (file < 0) {
        qDebug() << "Error: Failed to create new HDF5 file:" << fileName.c_str();
        return;
    }
}


HDF5Wrapper::~HDF5Wrapper()
{
    // A failed open leaves a negative id, which "if (file)" treated as valid.
    if (file >= 0)
        H5Fclose(file);
}

// See stressresult.h for what this writes and why it lives here.
QString saveStiffnessMatrixToHDF5(const QString& filename,
                                  const StiffnessMatrixResult& r,
                                  const QString& solver,
                                  unsigned int seed)
{
    if (!r.ok) {
        qWarning() << "saveStiffnessMatrixToHDF5: refusing to write a failed result";
        return {};
    }

    HDF5Wrapper hdf5(filename.toStdString());

    int last_set = hdf5.readInt("/", "last_set");
    if (last_set == -1) { last_set = 1; hdf5.write("/", "last_set", last_set); }
    else                { last_set += 1; hdf5.update("/", "last_set", last_set); }

    const QString     group  = "/" + QString::number(last_set);
    const std::string prefix = group.toStdString();

    // Persist geometry if voxels are available so the dataset has a valid geometry section
    if (Parameters::voxels && Parameters::instance()->getSize() > 0) {
        const int size = Parameters::instance()->getSize();
        const int points = Parameters::instance()->getPoints();
        hdf5.write(prefix, "voxels", Parameters::voxels, size);
        hdf5.write(prefix, "cubeSize", size);
        hdf5.write(prefix, "numPoints", points);

        if (OpenGLWidgetQML* ogl = OpenGLWidgetQML::getInstance()) {
            const auto& orientations = ogl->getGrainOrientations();
            if (!orientations.empty()) {
                std::vector<std::vector<float>> local_cs;
                local_cs.reserve(orientations.size());
                for (const auto& arr : orientations) {
                    local_cs.push_back({arr[0], arr[1], arr[2]});
                }
                hdf5.write(prefix, "local_cs", local_cs);
            }
        }
        saveGeometryMetadataToHDF5(hdf5, prefix, solver);
    }

    std::vector<std::vector<float>> mat_S(6, std::vector<float>(6));
    std::vector<std::vector<float>> mat_C(6, std::vector<float>(6));
    std::vector<std::vector<float>> mat_P(6, std::vector<float>(6));
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j) {
            mat_S[i][j] = float(r.S[i][j]);
            mat_C[i][j] = float(r.C[i][j]);
            mat_P[i][j] = float(r.P[i][j]);
        }
    std::vector<float> moduli(r.moduli, r.moduli + 6);

    hdf5.write(prefix, "S_matrix",         mat_S);
    hdf5.write(prefix, "C_matrix",         mat_C);
    hdf5.write(prefix, "P_matrix",         mat_P);
    hdf5.write(prefix, "Effective_Moduli", moduli);
    hdf5.write(prefix, "seed",             int(seed));
    hdf5.write(prefix, "solver",           solver);
    if (r.isFFT) hdf5.write(prefix, "iterations_total", r.totalIterations);

    qDebug() << "Stiffness matrix ->" << filename << group;
    return group;
}

// "/" + "last_set" would otherwise give "//last_set".
std::string HDF5Wrapper::fullPath(const std::string& dataGroup, const std::string& dataSetName)
{
    if (dataGroup.empty() || dataGroup == "/")
        return "/" + dataSetName;
    if (dataGroup.back() == '/')
        return dataGroup + dataSetName;
    return dataGroup + "/" + dataSetName;
}

bool HDF5Wrapper::datasetExists(const std::string& dataGroup, const std::string& dataSetName)
{
    if (file < 0) return false;
    // Suppress HDF5's default error stack printer to stderr so intermediate
    // component traversal failures return <= 0 silently without terminal spam.
    H5Eset_auto(H5E_DEFAULT, NULL, NULL);
    return H5Lexists(file, fullPath(dataGroup, dataSetName).c_str(), H5P_DEFAULT) > 0;
}

bool HDF5Wrapper::checkError(hid_t id, const std::string& message)
{
    if (id == H5I_INVALID_HID) {
        qCritical() << message.c_str();
        return true;
    }
    return false;
}

hid_t HDF5Wrapper::createGroupIfNotExists(const std::string& groupName)
{
    if (file < 0) return H5I_INVALID_HID;
    if (groupName.empty() || groupName == "/") {
        return H5Gopen2(file, "/", H5P_DEFAULT);
    }

    H5Eset_auto(H5E_DEFAULT, NULL, NULL);

    hid_t group_id = H5Gopen2(file, groupName.c_str(), H5P_DEFAULT);
    if (group_id < 0)
    {
        hid_t lcpl = H5Pcreate(H5P_LINK_CREATE);
        H5Pset_create_intermediate_group(lcpl, 1);
        group_id = H5Gcreate2(file, groupName.c_str(), lcpl, H5P_DEFAULT, H5P_DEFAULT);
        H5Pclose(lcpl);
        if (group_id < 0)
        {
            qCritical() << "Failed to create group: " << QString::fromStdString(groupName);
        }
    }

    return group_id;
}


void HDF5Wrapper::write(const std::string& dataGroup, const std::string& dataSetName, const std::vector<float>& data) {
    hid_t group_id = createGroupIfNotExists(dataGroup);
    if (checkError(group_id, "write vector<float>: Failed to create group"))
        return;

    hsize_t dims[1] = { data.size() };
    hid_t dataspace = H5Screate_simple(1, dims, nullptr);
    if (checkError(dataspace, "write vector<float>: Failed to create dataspace"))
    {
        H5Gclose(group_id);
        return;
    }

    hid_t dataset = H5Dcreate(file, (dataGroup + "/" + dataSetName).c_str(), H5T_NATIVE_FLOAT, dataspace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    if (checkError(dataset, "write vector<float>: Failed to create dataset " + dataGroup + "/" + dataSetName))
    {
        H5Sclose(dataspace);
        H5Gclose(group_id);
        return;
    }
    H5Dwrite(dataset, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, data.data());

    H5Dclose(dataset);
    H5Sclose(dataspace);
    H5Gclose(group_id);

}

void HDF5Wrapper::write(const std::string& dataGroup, const std::string& dataSetName, const std::vector<std::vector<float>>& data) {
    hid_t group_id = createGroupIfNotExists(dataGroup);
    if (checkError(group_id, "write vector<vector<float>: Failed to create group"))
        return;
    hsize_t dims[2] = { data.size(), data[0].size() };
    hid_t dataspace = H5Screate_simple(2, dims, nullptr);

    if (checkError(dataspace, "write vector<vector<float>>: Failed to create dataspace"))
    {
        H5Gclose(group_id);
        return;
    }
    hid_t dataset = H5Dcreate(file, (dataGroup + "/" + dataSetName).c_str(), H5T_NATIVE_FLOAT, dataspace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    if (checkError(dataset, "write vector<vector<float>>: Failed to create dataset " + dataGroup + "/" + dataSetName))
    {
        H5Dclose(dataset);
        H5Gclose(group_id);
        return;
    }
    std::vector<float> flatData;
    for (const auto& row : data) {
        flatData.insert(flatData.end(), row.begin(), row.end());
    }

    H5Dwrite(dataset, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, flatData.data());

    H5Dclose(dataset);
    H5Sclose(dataspace);
    H5Gclose(group_id);
}

void HDF5Wrapper::write(const std::string& dataGroup, const std::string& dataSetName, int data)
{
    if (file < 0) {
        qCritical() << "HDF5 file is not opened correctly.";
        return;
    }

    qDebug() << "HDF5 file opened for writing: " << file;

    hid_t group_id = createGroupIfNotExists(dataGroup);
    if (checkError(group_id, "write int: Failed to create group")) {
        qCritical() << "Error creating group: " << QString::fromStdString(dataGroup);
        return;
    }

    // Create or open dataset
    hid_t dataset = -1;
    if (H5Lexists(group_id, dataSetName.c_str(), H5P_DEFAULT)) {
        // Dataset exists, open it
        dataset = H5Dopen(group_id, dataSetName.c_str(), H5P_DEFAULT);
        if (checkError(dataset, "write int: Failed to open existing dataset")) {
            H5Gclose(group_id);
            return;
        }
    } else {
        // Dataset doesn't exist, create a new one
        qDebug() << "Creating scalar dataspace...";
        hid_t dataspace = H5Screate(H5S_SCALAR);
        if (checkError(dataspace, "write int: Failed to create dataspace")) {
            H5Gclose(group_id);
            return;
        }

        qDebug() << "Creating dataset: " << QString::fromStdString(dataGroup + "/" + dataSetName);
        dataset = H5Dcreate(group_id, dataSetName.c_str(), H5T_NATIVE_INT, dataspace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        if (checkError(dataset, "write int: Failed to create dataset " + dataGroup + "/" + dataSetName)) {
            H5Sclose(dataspace);
            H5Gclose(group_id);
            return;
        }

        H5Sclose(dataspace);
    }

    qDebug() << "Writing integer value to dataset...";
    herr_t status = H5Dwrite(dataset, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, &data);
    if (status < 0) {
        qCritical() << "Error writing to dataset.";
    } else {
        qDebug() << "Integer value successfully written: " << data;
    }

    // Close resources
    H5Dclose(dataset);
    H5Gclose(group_id);

    qDebug() << "HDF5 file closed, writing process completed.";
}


void HDF5Wrapper::write(const std::string& dataGroup, const std::string& dataSetName, float data)
{
    hid_t group_id = createGroupIfNotExists(dataGroup);
    if (checkError(group_id, "write float: Failed to create group"))
        return;

    if (H5Lexists(group_id, dataSetName.c_str(), H5P_DEFAULT)) {
        H5Ldelete(group_id, dataSetName.c_str(), H5P_DEFAULT);
    }

    hid_t dataspace = H5Screate(H5S_SCALAR);
    if (checkError(dataspace, "write float: Failed to create dataspace"))
    {
        H5Gclose(group_id);
        return;
    }
    hid_t dataset = H5Dcreate(group_id, dataSetName.c_str(), H5T_NATIVE_FLOAT, dataspace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    if (checkError(dataset, "write float: Failed to create dataset " + dataGroup + "/" + dataSetName))
    {
        H5Sclose(dataspace);
        H5Gclose(group_id);
        return;
    }
    H5Dwrite(dataset, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, &data);
    H5Dclose(dataset);
    H5Sclose(dataspace);
    H5Gclose(group_id);
}

void HDF5Wrapper::write(const std::string& dataGroup, const std::string& dataSetName, double data)
{
    hid_t group_id = createGroupIfNotExists(dataGroup);
    if (checkError(group_id, "write double: Failed to create group"))
        return;

    if (H5Lexists(group_id, dataSetName.c_str(), H5P_DEFAULT)) {
        H5Ldelete(group_id, dataSetName.c_str(), H5P_DEFAULT);
    }

    hid_t dataspace = H5Screate(H5S_SCALAR);
    if (checkError(dataspace, "write double: Failed to create dataspace"))
    {
        H5Gclose(group_id);
        return;
    }
    hid_t dataset = H5Dcreate(group_id, dataSetName.c_str(), H5T_NATIVE_DOUBLE, dataspace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    if (checkError(dataset, "write double: Failed to create dataset " + dataGroup + "/" + dataSetName))
    {
        H5Sclose(dataspace);
        H5Gclose(group_id);
        return;
    }
    H5Dwrite(dataset, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &data);
    H5Dclose(dataset);
    H5Sclose(dataspace);
    H5Gclose(group_id);
}

void HDF5Wrapper::write(const std::string& dataGroup, const std::string& dataSetName, const QString& data)
{
    hid_t group_id = createGroupIfNotExists(dataGroup);
    if (checkError(group_id, "write QString: Failed to create group"))
        return;

    if (H5Lexists(group_id, dataSetName.c_str(), H5P_DEFAULT)) {
        H5Ldelete(group_id, dataSetName.c_str(), H5P_DEFAULT);
    }

    QByteArray byteArray = data.toUtf8();
    hsize_t dims[1] = { static_cast<hsize_t>(byteArray.size()) };
    hid_t dataspace = H5Screate_simple(1, dims, nullptr);
    if (checkError(dataspace, "write QString: Failed to create dataspace"))
    {
        H5Gclose(group_id);
        return;
    }
    hid_t dataset = H5Dcreate(group_id, dataSetName.c_str(), H5T_NATIVE_CHAR, dataspace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    if (checkError(dataset, "write QString: Failed to create dataset " + dataGroup + "/" + dataSetName))
    {
        H5Sclose(dataspace);
        H5Gclose(group_id);
        return;
    }
    H5Dwrite(dataset, H5T_NATIVE_CHAR, H5S_ALL, H5S_ALL, H5P_DEFAULT, byteArray.data());

    H5Dclose(dataset);
    H5Sclose(dataspace);
    H5Gclose(group_id);
}

void HDF5Wrapper::write(const std::string& dataGroup, const std::string& dataSetName, int32_t ***voxels, int size)
{
    if (file < 0) {
        qCritical() << "HDF5 file is not opened correctly.";
        return;
    }
    if (!voxels || size <= 0) {
        qWarning() << "write voxels: null voxels or invalid size" << size;
        return;
    }

    qDebug() << "HDF5 file opened for writing: " << file;

    hid_t group_id = createGroupIfNotExists(dataGroup);
    if (checkError(group_id, "write int32_t ***voxels: Failed to create group")) {
        qCritical() << "Error creating group: " << QString::fromStdString(dataGroup);
        return;
    }

    hsize_t dims[3];  // assuming a 3D array
    dims[0] = size;
    dims[1] = size;
    dims[2] = size;

    // Create dataspace
    hid_t dataspace = H5Screate_simple(3, dims, NULL);
    if (checkError(dataspace, "write int32_t ***voxels: Failed to create dataspace")) {
        H5Gclose(group_id);
        return;
    }

    // Check if dataset exists, if so, open it
    hid_t dataset_id = -1;
    if (H5Lexists(group_id, dataSetName.c_str(), H5P_DEFAULT)) {
        // Dataset exists, open it
        dataset_id = H5Dopen(group_id, dataSetName.c_str(), H5P_DEFAULT);
        if (checkError(dataset_id, "write int32_t ***voxels: Failed to open existing dataset")) {
            H5Sclose(dataspace);
            H5Gclose(group_id);
            return;
        }
    } else {
        // Dataset doesn't exist, create a new one
        qDebug() << "Creating dataset: " << QString::fromStdString(dataGroup + "/" + dataSetName);
        dataset_id = H5Dcreate(group_id, dataSetName.c_str(), H5T_STD_I32LE, dataspace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        if (checkError(dataset_id, "write int32_t ***voxels: Failed to create dataset " + dataGroup + "/" + dataSetName)) {
            H5Sclose(dataspace);
            H5Gclose(group_id);
            return;
        }
    }

    // Create buffer to store data in continuous memory
    std::vector<int32_t> buffer(size * size * size);
    for (int i = 0; i < size; i++)
        for (int j = 0; j < size; j++)
            for (int k = 0; k < size; k++)
                buffer[i * size * size + j * size + k] = voxels[i][j][k];

    qDebug() << "Writing data to dataset...";
    herr_t status = H5Dwrite(dataset_id, H5T_STD_I32LE, H5S_ALL, H5S_ALL, H5P_DEFAULT, buffer.data());
    if (status < 0) {
        qCritical() << "Error writing to dataset.";
    } else {
        qDebug() << "Data successfully written to dataset: " << QString::fromStdString(dataGroup + "/" + dataSetName);
    }

    // Close resources
    H5Dclose(dataset_id);
    H5Sclose(dataspace);
    H5Gclose(group_id);

    qDebug() << "HDF5 file closed, writing process completed.";
}


std::vector<float> HDF5Wrapper::readVectorFloat(const std::string& dataGroup, const std::string& dataSetName) {
    if (!datasetExists(dataGroup, dataSetName)) {
        qWarning() << "HDF5: no dataset" << fullPath(dataGroup, dataSetName).c_str() << "-- returning empty";
        return {};
    }
    hid_t dataset = H5Dopen(file, fullPath(dataGroup, dataSetName).c_str(), H5P_DEFAULT);
    hid_t dataspace = H5Dget_space(dataset);

    hsize_t dims[1];
    H5Sget_simple_extent_dims(dataspace, dims, nullptr);

    std::vector<float> data(dims[0]);
    H5Dread(dataset, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, data.data());

    H5Sclose(dataspace);
    H5Dclose(dataset);

    return data;
}

std::vector<std::vector<float>> HDF5Wrapper::readVectorVectorFloat(const std::string& dataGroup, const std::string& dataSetName) {
    if (!datasetExists(dataGroup, dataSetName)) {
        qWarning() << "HDF5: no dataset" << fullPath(dataGroup, dataSetName).c_str() << "-- returning empty";
        return {};
    }
    hid_t dataset = H5Dopen(file, fullPath(dataGroup, dataSetName).c_str(), H5P_DEFAULT);
    hid_t dataspace = H5Dget_space(dataset);

    hsize_t dims[2];
    H5Sget_simple_extent_dims(dataspace, dims, nullptr);

    std::vector<float> flatData(dims[0] * dims[1]);
    H5Dread(dataset, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, flatData.data());

    std::vector<std::vector<float>> data(dims[0], std::vector<float>(dims[1]));
    for (hsize_t i = 0; i < dims[0]; ++i) {
        for (hsize_t j = 0; j < dims[1]; ++j) {
            data[i][j] = flatData[i * dims[1] + j];
        }
    }

    H5Sclose(dataspace);
    H5Dclose(dataset);

    return data;
}

float HDF5Wrapper::readFloat(const std::string& dataGroup, const std::string& dataSetName)
{
    if (!datasetExists(dataGroup, dataSetName)) {
        qWarning() << "HDF5: no dataset" << fullPath(dataGroup, dataSetName).c_str() << "-- returning 0";
        return 0.0f;
    }
    hid_t dataset = H5Dopen(file, fullPath(dataGroup, dataSetName).c_str(), H5P_DEFAULT);
    float data = 0.0f;
    H5Dread(dataset, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, &data);
    H5Dclose(dataset);
    return data;
}

double HDF5Wrapper::readDouble(const std::string& dataGroup, const std::string& dataSetName)
{
    if (!datasetExists(dataGroup, dataSetName)) {
        return 0.0;
    }
    hid_t dataset = H5Dopen(file, fullPath(dataGroup, dataSetName).c_str(), H5P_DEFAULT);
    double data = 0.0;
    H5Dread(dataset, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &data);
    H5Dclose(dataset);
    return data;
}

int HDF5Wrapper::readInt(const std::string& dataGroup, const std::string& dataSetName)
{
    // -1 means "not there". Callers rely on it (the last_set counter starts at
    // 1 when absent), so an absent dataset is a normal answer, not an error:
    // probe the link rather than letting H5Dopen fail and print a stack.
    if (!datasetExists(dataGroup, dataSetName))
        return -1;

    hid_t dataset = H5Dopen(file, fullPath(dataGroup, dataSetName).c_str(), H5P_DEFAULT);
    if (dataset < 0) {
        qCritical() << "Can not open dataset " << fullPath(dataGroup, dataSetName).c_str();
        return -1;
    }
    int data = -1;
    H5Dread(dataset, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, &data);
    H5Dclose(dataset);
    return data;
}

QString HDF5Wrapper::readQString(const std::string& dataGroup, const std::string& dataSetName)
{
    if (!datasetExists(dataGroup, dataSetName)) {
        qWarning() << "HDF5: no dataset" << fullPath(dataGroup, dataSetName).c_str() << "-- returning empty";
        return {};
    }
    hid_t dataset = H5Dopen(file, fullPath(dataGroup, dataSetName).c_str(), H5P_DEFAULT);
    hid_t dataspace = H5Dget_space(dataset);

    hsize_t dims[1];
    H5Sget_simple_extent_dims(dataspace, dims, nullptr);

    QByteArray byteArray(dims[0], 0);
    H5Dread(dataset, H5T_NATIVE_CHAR, H5S_ALL, H5S_ALL, H5P_DEFAULT, byteArray.data());

    H5Sclose(dataspace);
    H5Dclose(dataset);

    return QString::fromUtf8(byteArray);
}

std::vector<std::vector<std::vector<int32_t>>> HDF5Wrapper::readVoxels(const std::string& dataGroup, const std::string& dataSetName) {
    if (file < 0) {
        qCritical() << "HDF5 file is not opened correctly.";
        return {};
    }

    if (!datasetExists(dataGroup, dataSetName)) {
        qWarning() << "HDF5: no dataset" << fullPath(dataGroup, dataSetName).c_str()
                   << "-- this group holds no geometry";
        return {};
    }

    hid_t dataset = H5Dopen(file, fullPath(dataGroup, dataSetName).c_str(), H5P_DEFAULT);
    if (checkError(dataset, "readVoxels: Failed to open dataset")) {
        return {};
    }

    hid_t dataspace = H5Dget_space(dataset);
    if (checkError(dataspace, "readVoxels: Failed to get dataspace")) {
        H5Dclose(dataset);
        return {};
    }

    hsize_t dims[3];
    int ndims = H5Sget_simple_extent_dims(dataspace, dims, nullptr);
    if (ndims != 3) {
        qCritical() << "readVoxels: Dataset is not 3D.";
        H5Sclose(dataspace);
        H5Dclose(dataset);
        return {};
    }

    std::vector<int32_t> flatData(dims[0] * dims[1] * dims[2]);
    H5Dread(dataset, H5T_STD_I32LE, H5S_ALL, H5S_ALL, H5P_DEFAULT, flatData.data());

    std::vector<std::vector<std::vector<int32_t>>> voxels(dims[0],
        std::vector<std::vector<int32_t>>(dims[1], std::vector<int32_t>(dims[2])));

    for (hsize_t i = 0; i < dims[0]; ++i) {
        for (hsize_t j = 0; j < dims[1]; ++j) {
            for (hsize_t k = 0; k < dims[2]; ++k) {
                voxels[i][j][k] = flatData[i * dims[1] * dims[2] + j * dims[2] + k];
            }
        }
    }

    H5Sclose(dataspace);
    H5Dclose(dataset);

    return voxels;
}

herr_t groupIterationCallback(hid_t loc_id, const char* name, const H5L_info_t* info, void* opdata)
{
    Q_UNUSED(loc_id);
    Q_UNUSED(info);
    std::vector<std::string>* groups = static_cast<std::vector<std::string>*>(opdata);
    groups->emplace_back(name);
    return 0;
}

// Updated listDataGroups to allow listing groups at a specific path
std::vector<std::string> HDF5Wrapper::listDataGroups(const std::string& path)
{
    std::vector<std::string> groups;

    hid_t group_id = H5Gopen(file, path.c_str(), H5P_DEFAULT);
    if (group_id < 0) {
        qCritical() << "Failed to open group:" << QString::fromStdString(path);
        return groups;
    }

    H5Literate(group_id, H5_INDEX_NAME, H5_ITER_NATIVE, nullptr, groupIterationCallback, &groups);

    H5Gclose(group_id);
    return groups;
}

// Overloaded version of listDataGroups for root-level groups
std::vector<std::string> HDF5Wrapper::listDataGroups()
{
    return listDataGroups("/");
}

void HDF5Wrapper::update(const std::string& dataGroup, const std::string& dataSetName, int newValue)
{
    hid_t dataset = H5Dopen(file, (dataGroup + "/" + dataSetName).c_str(), H5P_DEFAULT);
    H5Dwrite(dataset, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, &newValue);
    H5Dclose(dataset);
}

void saveGeometryMetadataToHDF5(HDF5Wrapper& hdf5, const std::string& prefix, const QString& solver)
{
    Parameters* p = Parameters::instance();
    if (!p) return;

    QString algo = p->getAlgorithm();
    if (algo.isEmpty()) algo = QStringLiteral("Voronoi");

    // Top-level dataset identification
    hdf5.write(prefix, "algorithm", algo);
    hdf5.write(prefix, "seed",      int(p->getSeed()));
    hdf5.write(prefix, "cubeSize",  p->getSize());
    hdf5.write(prefix, "numPoints", p->getPoints());
    if (!solver.isEmpty()) {
        hdf5.write(prefix, "solver", solver);
    }

    // Structured /geometry subgroup
    const std::string geomGroup = prefix + "/geometry";
    hdf5.write(geomGroup, "algorithm",    algo);
    hdf5.write(geomGroup, "seed",         int(p->getSeed()));
    hdf5.write(geomGroup, "cubeSize",     p->getSize());
    hdf5.write(geomGroup, "numPoints",    p->getPoints());
    hdf5.write(geomGroup, "points_mode",  p->getPointsMode());
    hdf5.write(geomGroup, "is_periodic",  p->getIsPeriodic() ? 1 : 0);
    hdf5.write(geomGroup, "minkowski_p",  float(p->getMinkowskiP()));

    if (!solver.isEmpty()) {
        hdf5.write(geomGroup, "solver", solver);
    }

    QJsonObject rootObj;
    rootObj["algorithm"]    = algo;
    rootObj["seed"]         = static_cast<qint64>(p->getSeed());
    rootObj["cubeSize"]     = p->getSize();
    rootObj["numPoints"]    = p->getPoints();
    rootObj["points_mode"]  = p->getPointsMode();
    rootObj["is_periodic"]  = p->getIsPeriodic();
    rootObj["minkowski_p"]  = p->getMinkowskiP();
    if (!solver.isEmpty()) rootObj["solver"] = solver;

    QJsonObject paramsObj;
    QString summaryStr = QString("%1 (Size: %2³, Points: %3, Seed: %4")
        .arg(algo).arg(p->getSize()).arg(p->getPoints()).arg(p->getSeed());

    std::vector<ParamField> schema = AlgorithmFactory::instance().schemaFor(algo);
    for (const auto& f : schema) {
        if (f.key.isEmpty() || f.type == ParamField::Action) continue;
        QVariant val = p->property(f.key.toUtf8().constData());
        if (!val.isValid()) continue;

        if (f.type == ParamField::Int || f.type == ParamField::Bool) {
            int v = val.toInt();
            hdf5.write(geomGroup, f.key.toStdString(), v);
            paramsObj[f.key] = v;
            summaryStr += QString(", %1: %2").arg(f.label).arg(v);
        } else if (f.type == ParamField::Double) {
            double v = val.toDouble();
            hdf5.write(geomGroup, f.key.toStdString(), float(v));
            paramsObj[f.key] = v;
            summaryStr += QString(", %1: %2").arg(f.label).arg(QString::number(v, 'g', 4));
        } else if (f.type == ParamField::Enum || f.type == ParamField::PointsMode) {
            QString v = val.toString();
            hdf5.write(geomGroup, f.key.toStdString(), v);
            paramsObj[f.key] = v;
            summaryStr += QString(", %1: %2").arg(f.label, v);
        }
    }

    if (!p->getDbMaterial().isEmpty()) {
        hdf5.write(geomGroup, "db_material", p->getDbMaterial());
        paramsObj["db_material"] = p->getDbMaterial();
    }
    if (!p->getTexturePreset().isEmpty()) {
        hdf5.write(geomGroup, "texture_preset", p->getTexturePreset());
        paramsObj["texture_preset"] = p->getTexturePreset();
    }
    hdf5.write(geomGroup, "texture_scatter", float(p->getTextureScatter()));
    paramsObj["texture_scatter"] = p->getTextureScatter();

    summaryStr += ")";

    rootObj["parameters"] = paramsObj;
    QJsonDocument doc(rootObj);
    QString jsonStr = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));

    hdf5.write(geomGroup, "parameters_json",    jsonStr);
    hdf5.write(geomGroup, "parameters_summary", summaryStr);

    qDebug() << "[HDF5] Saved geometry metadata for algorithm" << algo << "to" << geomGroup.c_str();
}

GeomMetadata readGeometryMetadataFromHDF5(HDF5Wrapper& hdf5, const std::string& prefix)
{
    GeomMetadata meta;
    const std::string geomGroup = prefix + "/geometry";
    const bool hasGeomGroup = hdf5.datasetExists(prefix, "geometry");

    // 1. Algorithm
    if (hdf5.datasetExists(prefix, "algorithm")) {
        meta.algorithm = hdf5.readQString(prefix, "algorithm");
    } else if (hasGeomGroup && hdf5.datasetExists(geomGroup, "algorithm")) {
        meta.algorithm = hdf5.readQString(geomGroup, "algorithm");
    }

    // 2. Solver
    if (hdf5.datasetExists(prefix, "solver")) {
        meta.solver = hdf5.readQString(prefix, "solver");
    } else if (hasGeomGroup && hdf5.datasetExists(geomGroup, "solver")) {
        meta.solver = hdf5.readQString(geomGroup, "solver");
    }

    // 3. Seed
    if (hdf5.datasetExists(prefix, "seed")) {
        meta.seed = hdf5.readInt(prefix, "seed");
    } else if (hasGeomGroup && hdf5.datasetExists(geomGroup, "seed")) {
        meta.seed = hdf5.readInt(geomGroup, "seed");
    }

    // 4. Dimensions & Points
    if (hdf5.datasetExists(prefix, "cubeSize")) {
        meta.cubeSize = hdf5.readInt(prefix, "cubeSize");
    } else if (hasGeomGroup && hdf5.datasetExists(geomGroup, "cubeSize")) {
        meta.cubeSize = hdf5.readInt(geomGroup, "cubeSize");
    }

    if (hdf5.datasetExists(prefix, "numPoints")) {
        meta.numPoints = hdf5.readInt(prefix, "numPoints");
    } else if (hasGeomGroup && hdf5.datasetExists(geomGroup, "numPoints")) {
        meta.numPoints = hdf5.readInt(geomGroup, "numPoints");
    }

    if (hasGeomGroup) {
        if (hdf5.datasetExists(geomGroup, "points_mode")) {
            meta.pointsMode = hdf5.readQString(geomGroup, "points_mode");
        }
        if (hdf5.datasetExists(geomGroup, "is_periodic")) {
            meta.isPeriodic = (hdf5.readInt(geomGroup, "is_periodic") != 0);
        }
        if (hdf5.datasetExists(geomGroup, "minkowski_p")) {
            meta.minkowskiP = double(hdf5.readFloat(geomGroup, "minkowski_p"));
        }

        // 5. JSON & Summary
        if (hdf5.datasetExists(geomGroup, "parameters_json")) {
            meta.parametersJson = hdf5.readQString(geomGroup, "parameters_json");
            QJsonDocument doc = QJsonDocument::fromJson(meta.parametersJson.toUtf8());
            if (!doc.isNull() && doc.isObject()) {
                QJsonObject root = doc.object();
                if (root.contains("parameters") && root["parameters"].isObject()) {
                    meta.parameters = root["parameters"].toObject().toVariantMap();
                }
            }
        }

        if (hdf5.datasetExists(geomGroup, "parameters_summary")) {
            meta.summary = hdf5.readQString(geomGroup, "parameters_summary");
        }
    }

    if (meta.summary.isEmpty() && !meta.algorithm.isEmpty()) {
        meta.summary = QString("%1 (Size: %2³, Points: %3, Seed: %4)")
            .arg(meta.algorithm).arg(meta.cubeSize).arg(meta.numPoints).arg(meta.seed);
    }

    return meta;
}

bool applyGeometryMetadataToParameters(const GeomMetadata& meta)
{
    Parameters* p = Parameters::instance();
    if (!p) return false;

    if (!meta.algorithm.isEmpty()) p->setAlgorithm(meta.algorithm);
    if (meta.cubeSize > 0)         p->setSize(meta.cubeSize);
    if (meta.numPoints > 0)        p->setPoints(meta.numPoints);
    if (meta.seed > 0)             p->setSeed(static_cast<unsigned int>(meta.seed));
    if (!meta.pointsMode.isEmpty()) p->setPointsMode(meta.pointsMode);
    p->setIsPeriodic(meta.isPeriodic);
    if (meta.minkowskiP > 0.0)     p->setMinkowskiP(meta.minkowskiP);

    for (auto it = meta.parameters.begin(); it != meta.parameters.end(); ++it) {
        const QString& key = it.key();
        const QVariant& val = it.value();
        p->setProperty(key.toUtf8().constData(), val);
    }

    qDebug() << "[Parameters] Applied geometry metadata for algorithm" << meta.algorithm << "from HDF5";
    return true;
}

