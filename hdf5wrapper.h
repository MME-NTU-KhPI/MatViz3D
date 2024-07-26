#ifndef HDF5WRAPPER_H
#define HDF5WRAPPER_H

#include <vector>
#include <string>
#include <hdf5.h>
#include <QString>

class HDF5Wrapper {
public:
    HDF5Wrapper(const std::string& fileName);
    ~HDF5Wrapper();

    void write(const std::string& dataGroup, const std::string& dataSetName, const std::vector<float>& data);
    void write(const std::string& dataGroup, const std::string& dataSetName, const std::vector<std::vector<float>>& data);
    void write(const std::string& dataGroup, const std::string& dataSetName, float data);
    void write(const std::string& dataGroup, const std::string& dataSetName, int data);
    void write(const std::string& dataGroup, const std::string& dataSetName, const QString& data);
    void write(const std::string& dataGroup, const std::string& dataSetName, int16_t ***voxels , int size);

    std::vector<float> readVectorFloat(const std::string& dataGroup, const std::string& dataSetName);
    std::vector<std::vector<float>> readVectorVectorFloat(const std::string& dataGroup, const std::string& dataSetName);
    float readFloat(const std::string& dataGroup, const std::string& dataSetName);
    int readInt(const std::string& dataGroup, const std::string& dataSetName);
    QString readQString(const std::string& dataGroup, const std::string& dataSetName);

    std::vector<std::string> listDataGroups();

    void update(const std::string& dataGroup, const std::string& dataSetName, int newValue);

private:
    hid_t file;
    hid_t createGroupIfNotExists(const std::string& groupName);
};


#endif // HDF5WRAPPER_H
