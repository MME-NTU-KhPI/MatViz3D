#include "hdf5wrapper.h"
#include <QFile>
#include <QDebug>

HDF5Wrapper::HDF5Wrapper(const std::string& fileName)
{
   // file = H5Fcreate(fileName.c_str(), H5F_ACC_RDWR, H5P_DEFAULT, H5P_DEFAULT);

    // Creta file if not exits else open file
    bool is_file_exist = QFile::exists(QString::fromStdString(fileName));

    if (is_file_exist)
    {
        qDebug() << "HDF5 file is already exists, opening for appending" << fileName.c_str();
        file = H5Fopen(fileName.c_str(), H5F_ACC_RDWR, H5P_DEFAULT);
        if (file == H5I_INVALID_HID)
        {
            qDebug() << "Can not open file " << fileName.c_str();
        }
        return;
    }
    else
    {
        file = H5Fcreate(fileName.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
        if (file == H5I_INVALID_HID)
        {
            qDebug() << "HDF5 file is already exists" << fileName.c_str();
            file = H5Fopen(fileName.c_str(), H5F_ACC_RDWR, H5P_DEFAULT);
            if (file == H5I_INVALID_HID)
            {
                qDebug() << "Can not open file " << fileName.c_str();
                return ;
            }
        }
    }
    return ;
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
    H5Eset_auto(H5E_DEFAULT, NULL, NULL); // Use updated error handler
    hid_t group_id = H5Gopen2(file, groupName.c_str(), H5P_DEFAULT); // Use H5Gopen2
    if (group_id == H5I_INVALID_HID)
    {
        group_id = H5Gcreate2(file, groupName.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT); // Use H5Gcreate2
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
    hid_t group_id = createGroupIfNotExists(dataGroup);
    if (checkError(group_id, "write int: Failed to create group"))
        return;

    hid_t dataspace = H5Screate(H5S_SCALAR);
    if (checkError(dataspace, "write int: Failed to create dataspace"))
    {
        H5Gclose(group_id);
        return;
    }

    hid_t dataset = H5Dcreate(file, (dataGroup + "/" + dataSetName).c_str(), H5T_NATIVE_INT, dataspace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    if (checkError(dataset, "write int: Failed to create dataset " + dataGroup + "/" + dataSetName))
    {
        H5Sclose(dataspace);
        H5Gclose(group_id);
        return;
    }
    H5Dwrite(dataset, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, &data);
    H5Dclose(dataset);
    H5Sclose(dataspace);
    H5Gclose(group_id);
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

void HDF5Wrapper::write(const std::string& dataGroup, const std::string& dataSetName, int32_t ***voxels , int size)
{
    hid_t group_id = createGroupIfNotExists(dataGroup);
    if (checkError(group_id, "write int32_t ***voxels: Failed to create group"))
        return;

    hsize_t dims[3];  // assuming a 3D array
    dims[0] = size;
    dims[1] = size;
    dims[2] = size;

    // allocate space as dataspace
    hid_t dataspace = H5Screate_simple(3, dims, NULL);
    if (checkError(dataspace, "write int32_t ***voxels: Failed to create dataspace"))
    {
        H5Gclose(group_id);
        return;
    }

    hid_t lcpl = H5Pcreate(H5P_LINK_CREATE);
    if (checkError(lcpl, "write int32_t ***voxels: Cannot create property list in hdf5 file. Saving aborted"))
    {
        H5Gclose(group_id);
        return;
    }

    // create intermediate groups as needed

    if ((H5Pset_create_intermediate_group(lcpl, 1)) < 0)
    {
        qCritical() << "Cannot create intermediate_group in hdf5 file. Saving aborted";
        return;
    }

    /*
     * Create a new dataset within the file using defined dataspace and
     * datatype and default dataset creation properties.
     */
    std::string dataset_name = (dataGroup + "/" + dataSetName);
    hid_t dataset_id = H5Dcreate(file, dataset_name.c_str(), H5T_STD_I32LE, dataspace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    if (checkError(dataset_id, "write int32_t ***voxels: Failed to create dataset " + dataGroup + "/" + dataSetName))
    {
        H5Pclose(lcpl);
        H5Sclose(dataspace);
        H5Gclose(group_id);
        return;
    }
    /*
     * Write the data to the dataset using default transfer properties.
     */
    herr_t status = H5Dwrite(dataset_id, H5T_STD_I32LE, H5S_ALL, H5S_ALL, H5P_DEFAULT, voxels[0][0]);
    if (status < 0)
        qCritical() << "Cannot write dataset";

    H5Dclose(dataset_id);
    H5Pclose(lcpl);
    H5Sclose(dataspace);
    H5Gclose(group_id);
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

