#ifndef HDF5HANDLER_H
#define HDF5HANDLER_H
#include <QString>
#include <hdf5.h>


class HDF5Handler
{
public:
    HDF5Handler();
    void setPath(const QString& new_path);
    QString getPath();
    void getVoxels(int16_t ***&voxels);
    void getSize(short& _size);
    void getPoints(int& _points);
    void getAlgName(QString& to_write);
    void getAll(int16_t *** &voxels ,short& _size ,int& _points, QString& to_write);
    void writeInfo(QString dataset_name,  hid_t type , int* data , int sizeOfArray);
    void writeInfo(QString dataset_name, hid_t type, int value);
    void writeInfo(QString dataset_name, QString data);
private:
    QString m_path;
    short m_size = -1;
};

#endif // HDF5HANDLER_H
