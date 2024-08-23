#ifndef PARAMETERS_H
#define PARAMETERS_H

#include <mainwindow.h>


class Parameters
{
public:
    static int16_t*** voxels;
    static short int size;
    static int points;
    static QString algorithm;
    static unsigned int seed;
    static QString filename;
};

#endif // PARAMETERS_H
