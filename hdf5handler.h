#ifndef HDF5HANDLER_H
#define HDF5HANDLER_H
#include <QString>
#include <hdf5.h>


class HDF5Handler
{
public:
    HDF5Handler();
    HDF5Handler(const QString& file_name);
    ~HDF5Handler();
    void setFileName(const QString& file_name);
    QString getFileName();

    bool openFile();

    void getVoxels(int16_t ***&voxels);
    void getSize(short& _size);
    void getPoints(int& _points);
    void getAlgName(QString& to_write);
    void getAll(int16_t *** &voxels ,short& _size ,int& _points, QString& to_write);
    void writeInfo(QString dataset_name,  hid_t type , int* data , int sizeOfArray);
    void writeInfo(QString dataset_name, hid_t type, int value);
    void writeInfo(QString dataset_name, QString data);

    void writeVoxels(int16_t ***voxels , int size);
private:
    QString m_file_name;
    hid_t   m_file;
    short m_size = -1;
};

#endif // HDF5HANDLER_H
