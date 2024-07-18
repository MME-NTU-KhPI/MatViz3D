#ifndef EXPORT_H
#define EXPORT_H

#include <mainwindow.h>
#include "parameters.h"

class Export
{
public:
    static void ExportToCSV(short int numCubes, int32_t ***voxels);
    static void ExportToVRML(const std::vector<std::array<GLubyte, 4>>& colors, short int numCubes, int32_t ***voxels);
};

#endif // EXPORT_H
