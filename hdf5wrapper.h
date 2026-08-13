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
    void write(const std::string& dataGroup, const std::string& dataSetName, int32_t ***voxels , int size);

    std::vector<float> readVectorFloat(const std::string& dataGroup, const std::string& dataSetName);
    std::vector<std::vector<float>> readVectorVectorFloat(const std::string& dataGroup, const std::string& dataSetName);
    float readFloat(const std::string& dataGroup, const std::string& dataSetName);
    int readInt(const std::string& dataGroup, const std::string& dataSetName);
    QString readQString(const std::string& dataGroup, const std::string& dataSetName);
    std::vector<std::vector<std::vector<int32_t>>> readVoxels(const std::string& dataGroup, const std::string& dataSetName);

    // True if the dataset is present. Probing with this instead of letting
    // H5Dopen fail keeps HDF5's default error handler from dumping a ten-frame
    // error stack for outcomes that are entirely expected -- "last_set" does
    // not exist until the first write to a fresh file, and a stiffness-only
    // result group deliberately has no voxels.
    bool datasetExists(const std::string& dataGroup, const std::string& dataSetName);

    std::vector<std::string> listDataGroups(const std::string& path);
    std::vector<std::string> listDataGroups();

    void update(const std::string& dataGroup, const std::string& dataSetName, int newValue);

private:
    // Initialised: the constructor returns early on an empty filename, and both
    // the destructor's H5Fclose and datasetExists()'s guard read this.
    hid_t file = -1;
    hid_t createGroupIfNotExists(const std::string& groupName);
    bool checkError(hid_t id, const std::string& message);
    static std::string fullPath(const std::string& dataGroup, const std::string& dataSetName);
};


#endif // HDF5WRAPPER_H
