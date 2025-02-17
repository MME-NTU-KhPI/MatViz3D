#include "hdf5wrapper.h"
#include <QFile>
#include <QDebug>

HDF5Wrapper::HDF5Wrapper(const std::string& fileName)
{
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
    if (file)
        H5Fclose(file);
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
    H5E_auto2_t old_func;
    void *old_client_data;

    H5Eget_auto(H5E_DEFAULT, &old_func, &old_client_data);
    H5Eset_auto(H5E_DEFAULT, NULL, NULL);

    hid_t group_id = H5Gopen2(file, groupName.c_str(), H5P_DEFAULT);
    if (group_id < 0)
    {
        group_id = H5Gcreate2(file, groupName.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        if (group_id < 0)
        {
            qCritical() << "Failed to create group: " << QString::fromStdString(groupName);
        }
    }

    H5Eset_auto(H5E_DEFAULT, old_func, old_client_data);

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

    hid_t dataspace = H5Screate(H5S_SCALAR);
    if (checkError(dataspace, "write float: Failed to create dataspace"))
    {
        H5Gclose(group_id);
        return;
    }
    hid_t dataset = H5Dcreate(file, (dataGroup + "/" + dataSetName).c_str(), H5T_NATIVE_FLOAT, dataspace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    if (checkError(dataset, "write float: Failed to create dataset " + dataGroup + "/" + dataSetName))
    {
        H5Sclose(dataspace);
        H5Gclose(group_id);
        return;
    }
    H5Dwrite(dataset, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, &data);
    H5Sclose(dataspace);
    H5Gclose(group_id);
    H5Dclose(dataset);
}

void HDF5Wrapper::write(const std::string& dataGroup, const std::string& dataSetName, const QString& data)
{
    hid_t group_id = createGroupIfNotExists(dataGroup);
    if (checkError(group_id, "write QString: Failed to create group"))
        return;
    QByteArray byteArray = data.toUtf8();
    hsize_t dims[1] = { static_cast<hsize_t>(byteArray.size()) };
    hid_t dataspace = H5Screate_simple(1, dims, nullptr);
    if (checkError(dataspace, "write QString: Failed to create dataspace"))
    {
        H5Gclose(group_id);
        return;
    }
    hid_t dataset = H5Dcreate(file, (dataGroup + "/" + dataSetName).c_str(), H5T_NATIVE_CHAR, dataspace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
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
    hid_t dataset = H5Dopen(file, (dataGroup + "/" + dataSetName).c_str(), H5P_DEFAULT);
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
    hid_t dataset = H5Dopen(file, (dataGroup + "/" + dataSetName).c_str(), H5P_DEFAULT);
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

float HDF5Wrapper::readFloat(const std::string& dataGroup, const std::string& dataSetName) {
    hid_t dataset = H5Dopen(file, (dataGroup + "/" + dataSetName).c_str(), H5P_DEFAULT);
    float data;
    H5Dread(dataset, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, &data);
    H5Dclose(dataset);
    return data;
}

int HDF5Wrapper::readInt(const std::string& dataGroup, const std::string& dataSetName)
{
    hid_t group_id = createGroupIfNotExists(dataGroup);
    if (group_id < 0) {
        // Handle error
        qCritical() << "Can not create group " << dataGroup.c_str();
        return -1;
    }
    hid_t dataset = H5Dopen(file, (dataGroup + "/" + dataSetName).c_str(), H5P_DEFAULT);
    if (dataset < 0) {
        // Handle error
        qCritical() << "Can not open dataset " << (dataGroup + "/" + dataSetName).c_str();
        H5Gclose(group_id);
        return -1;
    }
    int data;
    H5Dread(dataset, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, &data);
    H5Dclose(dataset);
    H5Gclose(group_id);
    return data;
}

QString HDF5Wrapper::readQString(const std::string& dataGroup, const std::string& dataSetName) {
    hid_t dataset = H5Dopen(file, (dataGroup + "/" + dataSetName).c_str(), H5P_DEFAULT);
    hid_t dataspace = H5Dget_space(dataset);

    hsize_t dims[1];
    H5Sget_simple_extent_dims(dataspace, dims, nullptr);

    QByteArray byteArray(dims[0], 0);
    H5Dread(dataset, H5T_NATIVE_CHAR, H5S_ALL, H5S_ALL, H5P_DEFAULT, byteArray.data());

    H5Sclose(dataspace);
    H5Dclose(dataset);

    return QString::fromUtf8(byteArray);
}

herr_t groupIterationCallback(hid_t loc_id, const char* name, const H5L_info_t* info, void* opdata) {
    std::vector<std::string>* groups = static_cast<std::vector<std::string>*>(opdata);
    groups->emplace_back(name);
    return 0;
}

std::vector<std::string> HDF5Wrapper::listDataGroups() {
    std::vector<std::string> groups;
    H5Literate(file, H5_INDEX_NAME, H5_ITER_NATIVE, nullptr, groupIterationCallback, &groups);
    return groups;
}

void HDF5Wrapper::update(const std::string& dataGroup, const std::string& dataSetName, int newValue)
{
    hid_t dataset = H5Dopen(file, (dataGroup + "/" + dataSetName).c_str(), H5P_DEFAULT);
    H5Dwrite(dataset, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, &newValue);
    H5Dclose(dataset);
}

