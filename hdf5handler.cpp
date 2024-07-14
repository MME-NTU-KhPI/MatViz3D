#include "hdf5handler.h"
#include <QDebug>
#include <QFile>
#include <iostream>

#include <H5Cpp.h>
using namespace H5;

HDF5Handler::HDF5Handler()
{
    m_file = 0;
}

HDF5Handler::HDF5Handler(const QString& file_name)
{
    setFileName(file_name);
}

void HDF5Handler::setFileName(const QString &file_name)
{
    m_file_name = file_name;
    m_size = -1;
    this->openFile();
}

HDF5Handler::~HDF5Handler()
{
    if (m_file)
        H5Fclose(m_file);
}

QString HDF5Handler::getFileName()
{
    return m_file_name;
}

bool HDF5Handler::openFile()
{
    // Creta file if not exits else open file
    bool is_file_exist = QFile::exists("/home/pw/docs/file.txt");

    if (is_file_exist)
    {
        qDebug() << "HDF5 file is already exists, opening for appending" << m_file_name;
        m_file = H5Fopen(m_file_name.toStdString().c_str(), H5F_ACC_RDWR, H5P_DEFAULT);
        if (m_file == H5I_INVALID_HID)
        {
            qDebug() << "Can not open file " << m_file_name;
        }
        return false;
    }
    else
    {
        m_file = H5Fcreate(m_file_name.toStdString().c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
        if (m_file == H5I_INVALID_HID)
        {
            qDebug() << "HDF5 file is already exists" << m_file_name;
            m_file = H5Fopen(m_file_name.toStdString().c_str(), H5F_ACC_RDWR, H5P_DEFAULT);
            if (m_file == H5I_INVALID_HID)
            {
                qDebug() << "Can not open file " << m_file_name;
                return false;
            }
        }
    }
    return true;
}

void HDF5Handler::getVoxels(int16_t ***&voxels)
{
    if(m_file_name.isEmpty()) {qDebug() << "Empty path"; return;};
    hid_t       file_id;

    file_id = H5Fopen(m_file_name.toStdString().c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
    hid_t size_dset = H5Dopen2(file_id, "/Size", H5P_DEFAULT);
    if (size_dset < 0) {
        std::cerr << "Error opening dataset" << std::endl;
        H5Fclose(file_id);
        return;
    }

    H5Dread(size_dset, H5T_NATIVE_SHORT, H5S_ALL, H5S_ALL, H5P_DEFAULT,&m_size);
    H5Dclose(size_dset);

    hid_t voxel_dset = H5Dopen2(file_id, "/voxels", H5P_DEFAULT);
    if (voxel_dset < 0) {
        std::cerr << "Error opening dataset" << std::endl;
        H5Fclose(file_id);
        return;
    }
    int16_t * buffer = new int16_t[m_size * m_size * m_size];
    H5Dread(voxel_dset, H5T_NATIVE_INT16, H5S_ALL, H5S_ALL, H5P_DEFAULT,buffer);
    H5Dclose(voxel_dset);

    voxels = nullptr;

    voxels = new int16_t** [m_size];
    for (int i = 0; i < m_size; i++) {
        voxels[i] = new int16_t* [m_size];
        for (int j = 0; j < m_size; j++) {
            voxels[i][j] = new int16_t[m_size];
        }
    }
    for (int i = 0; i < m_size; i++) {
        for (int j = 0; j < m_size; j++) {
            for (int k = 0; k < m_size; k++) {
                voxels[i][j][k] = buffer[i * m_size * m_size + j * m_size + k];
            }
        }
    }
    delete []buffer;

    H5Fclose(file_id);
}

void HDF5Handler::getSize(short &_size)
{
    if(m_size>=0) {_size = m_size; return;}
    if(m_file_name.isEmpty()) {qDebug() << "Empty path"; return;};

    hid_t       file_id;

    file_id = H5Fopen(m_file_name.toStdString().c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
    hid_t size_dset = H5Dopen2(file_id, "/Size", H5P_DEFAULT);
    if (size_dset < 0) {
        std::cerr << "Error opening dataset" << std::endl;
        H5Fclose(file_id);
        return;
    }
    H5Dread(size_dset, H5T_NATIVE_SHORT, H5S_ALL, H5S_ALL, H5P_DEFAULT, &_size);
    H5Dclose(size_dset);
    H5Fclose(file_id);
}

void HDF5Handler::getPoints(int &_points)
{
    if(m_file_name.isEmpty()) {qDebug() << "Empty path"; return;};

    hid_t       file_id;
    file_id = H5Fopen(m_file_name.toStdString().c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);

    hid_t points_dset = H5Dopen2(file_id, "/Points", H5P_DEFAULT);
    if (points_dset < 0) {
        std::cerr << "Error opening dataset" << std::endl;
        H5Fclose(file_id);
        return;
    }
    H5Dread(points_dset, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT,&_points);
    H5Dclose(points_dset);
    H5Fclose(file_id);
}

void HDF5Handler::getAlgName(QString &to_write)
{
    if(m_file_name.isEmpty()) {qDebug() << "Empty path"; return;};
    hid_t       file_id;

    file_id = H5Fopen(m_file_name.toStdString().c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
    hid_t str_dset = H5Dopen2(file_id, "/Algo", H5P_DEFAULT);
    if (str_dset < 0) {
        std::cerr << "Error opening dataset" << std::endl;
        H5Fclose(file_id);
        return ;
    }
    hid_t str_type = H5Dget_type(str_dset);
    hid_t str_space = H5Dget_space(str_dset);
    hsize_t str_len;
    H5Sget_simple_extent_dims(str_space, &str_len, NULL);

    char *buf = new char[str_len];

    H5Dread(str_dset, str_type, H5S_ALL, H5S_ALL, H5P_DEFAULT,buf);
    H5Fclose(file_id);

    to_write = buf;
    delete[] buf;


}

void HDF5Handler::getAll(int16_t ***&voxels, short &_size, int &_points, QString &to_write)
{
    if(m_file_name.isEmpty()) {qDebug() << "Empty path"; return;};
    hid_t       file_id;

    file_id = H5Fopen(m_file_name.toStdString().c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);


    hid_t str_dset = H5Dopen2(file_id, "/Algo", H5P_DEFAULT);
    if (str_dset < 0) {
        std::cerr << "Error opening dataset" << std::endl;
        H5Fclose(file_id);
        return ;
    }
    hid_t str_type = H5Dget_type(str_dset);
    hid_t str_space = H5Dget_space(str_dset);
    hsize_t str_len;
    H5Sget_simple_extent_dims(str_space, &str_len, NULL);

    char *buf = new char[str_len];

    H5Dread(str_dset, str_type, H5S_ALL, H5S_ALL, H5P_DEFAULT,buf);
    to_write = buf;
    delete[] buf;



    hid_t points_dset = H5Dopen2(file_id, "/Points", H5P_DEFAULT);
    if (points_dset < 0) {
        std::cerr << "Error opening dataset" << std::endl;
        H5Fclose(file_id);
        return;
    }
    H5Dread(points_dset, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT,&_points);
    H5Dclose(points_dset);



    hid_t size_dset = H5Dopen2(file_id, "/Size", H5P_DEFAULT);
    if (size_dset < 0) {
        std::cerr << "Error opening dataset" << std::endl;
        H5Fclose(file_id);
        return;
    }

    H5Dread(size_dset, H5T_NATIVE_SHORT, H5S_ALL, H5S_ALL, H5P_DEFAULT,&_size);
    H5Dclose(size_dset);




    hid_t voxel_dset = H5Dopen2(file_id, "/voxels", H5P_DEFAULT);
    if (voxel_dset < 0) {
        std::cerr << "Error opening dataset" << std::endl;
        H5Fclose(file_id);
        return;
    }
    int16_t * buffer = new int16_t[_size * _size * _size];
    H5Dread(voxel_dset, H5T_NATIVE_INT16, H5S_ALL, H5S_ALL, H5P_DEFAULT,buffer);
    H5Dclose(voxel_dset);
    H5Fclose(file_id);

    voxels = nullptr;

    voxels = new int16_t** [_size];
    assert(voxels);
    for (int i = 0; i < _size; i++) {
        voxels[i] = new int16_t* [_size];
        assert(voxels[i]);
        for (int j = 0; j < _size; j++) {
            voxels[i][j] = new int16_t[_size];
            assert(voxels[i][j]);
        }
    }

    for (int i = 0; i < _size; i++) {
        for (int j = 0; j < _size; j++) {
            for (int k = 0; k < _size; k++) {
                voxels[i][j][k] = buffer[i * _size * _size + j * _size + k];
            }
        }
    }
    delete []buffer;

    for (int i = 0; i < _size; i++) {
        for (int j = 0; j < _size; j++) {
            for (int k = 0; k < _size; k++) {
                qDebug() << voxels[i][j][k] << " ";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;}


    m_size = _size;
}



void HDF5Handler::writeVoxels(int16_t ***voxels , int size)
{

    /*
     * Describe the size of the array and create the data space for fixed
     * size dataset.
     */

    hsize_t dims[3];  // assuming a 3D array
    dims[0] = size;
    dims[1] = size;
    dims[2] = size;

    // allocate space as dataspace
    hid_t dataspace = H5Screate_simple(3, dims, NULL);

    if (dataspace == H5I_INVALID_HID)
    {
        qCritical() << "Cannot create dataspace in hdf5 file. Saving aborted";
        return ;
    }

    hid_t lcpl;
    if ((lcpl = H5Pcreate(H5P_LINK_CREATE)) == H5I_INVALID_HID)
    {
        qCritical() << "Cannot create property list in hdf5 file. Saving aborted";
        return;
    }
    // create intermediate groups as needed
    if (H5Pset_create_intermediate_group(lcpl, 1) < 0)
    {
        qCritical() << "Cannot create intermediate_group in hdf5 file. Saving aborted";
        return;
    }
    hid_t  group = H5I_INVALID_HID;
    group = H5Gcreate(m_file, "/G1", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);


    /*
     * Create a new dataset within the file using defined dataspace and
     * datatype and default dataset creation properties.
     */
    hid_t dataset_id = H5Dcreate(m_file, "/G1/voxels", H5T_STD_I16LE, dataspace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    if (dataset_id == H5I_INVALID_HID)
    {
        qCritical() << "Cannot create dataset in hdf5 file. Saving aborted";
        H5Sclose(dataspace);
        return ;
    }
    /*
     * Write the data to the dataset using default transfer properties.
     */
    herr_t status = H5Dwrite(dataset_id, H5T_STD_I16LE, H5S_ALL, H5S_ALL, H5P_DEFAULT, voxels[0][0]);
    status = H5Gclose(group);
    H5Sclose(dataspace);
    H5Dclose(dataset_id);
}

void HDF5Handler::writeInfo(QString dataset_name, hid_t type, int *data , int sizeOfArray)
{
    hid_t file_id, dataset_id, dataspace_id;
    herr_t status;

    file_id = H5Fcreate(m_file_name.toStdString().c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    if (file_id < 0) {
        // Обработка ошибки создания файла
        return;
    }

    hsize_t dims[1] = {static_cast<hsize_t>(sizeOfArray)};
    dataspace_id = H5Screate_simple(1, dims, NULL);
    if (dataspace_id < 0) {
        // Обработка ошибки создания пространства данных
        H5Fclose(file_id);
        return;
    }

    dataset_id = H5Dcreate2(file_id, dataset_name.toStdString().c_str(), type, dataspace_id,
                            H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    if (dataset_id < 0) {
        // Обработка ошибки создания набора данных
        H5Sclose(dataspace_id);
        H5Fclose(file_id);
        return;
    }

    status = H5Dwrite(dataset_id, type, H5S_ALL, H5S_ALL, H5P_DEFAULT, data);
    if (status < 0) {
        // Обработка ошибки записи данных
        H5Dclose(dataset_id);
        H5Sclose(dataspace_id);
        H5Fclose(file_id);
        return;
    }

    H5Dclose(dataset_id);
    H5Sclose(dataspace_id);
    H5Fclose(file_id);
}

void HDF5Handler::writeInfo(QString dataset_name, hid_t type, int value)
{
    hid_t file_id, dataset_id, dataspace_id;
    herr_t status;

    file_id = H5Fcreate(m_file_name.toStdString().c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    if (file_id < 0) {
        // Обработка ошибки создания файла
        return;
    }

    hsize_t dims[1] = {1};
    dataspace_id = H5Screate_simple(1, dims, NULL);
    if (dataspace_id < 0) {
        // Обработка ошибки создания пространства данных
        H5Fclose(file_id);
        return;
    }

    dataset_id = H5Dcreate2(file_id, dataset_name.toStdString().c_str(), type, dataspace_id,
                            H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    if (dataset_id < 0) {
        // Обработка ошибки создания набора данных
        H5Sclose(dataspace_id);
        H5Fclose(file_id);
        return;
    }

    /* Set order of dataset datatype */
    if(H5Tset_order(type, H5T_ORDER_BE)<0) {
        printf("Error: fail to set endianness\n");
        return ;
    }

    status = H5Dwrite(dataset_id, type, H5S_ALL, H5S_ALL, H5P_DEFAULT, &value);
    if (status < 0) {
        // Обработка ошибки записи данных
        H5Dclose(dataset_id);
        H5Sclose(dataspace_id);
        H5Fclose(file_id);
        return;
    }

    H5Dclose(dataset_id);
    H5Sclose(dataspace_id);
    H5Fclose(file_id);
}

void HDF5Handler::writeInfo(QString dataset_name, QString data)
{
    hid_t file_id;
    herr_t status;

    file_id = H5Fcreate(m_file_name.toStdString().c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);

    std::string stdStr = data.toStdString();

    hsize_t str_dims[1] = { 1 };
    hid_t str_dataspace_id = H5Screate_simple(1, str_dims, NULL);
    if (str_dataspace_id < 0) {
        std::cerr << "Error creating new dataspace" << std::endl;
        H5Fclose(file_id);
        return;
    }

    hid_t strtype = H5Tcopy(H5T_C_S1);
    H5Tset_size(strtype, stdStr.size() + 1); // +1 для null-терминатора
    hid_t str_dataset_id = H5Dcreate2(file_id, dataset_name.toStdString().c_str(), strtype, str_dataspace_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    if (str_dataset_id < 0) {
        std::cerr << "Error creating new dataset" << std::endl;
        H5Sclose(str_dataspace_id);
        H5Fclose(file_id);
        return;
    }

    status = H5Dwrite(str_dataset_id, strtype, H5S_ALL, H5S_ALL, H5P_DEFAULT, stdStr.c_str());
    if (status < 0) {
        std::cerr << "Error writing new dataset" << std::endl;
        H5Dclose(str_dataset_id);
        H5Sclose(str_dataspace_id);
        H5Fclose(file_id);
        return;
    }

    H5Dclose(str_dataset_id);
    H5Sclose(str_dataspace_id);
    H5Tclose(strtype);
}




