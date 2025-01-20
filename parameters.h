#ifndef PARAMETERS_H
#define PARAMETERS_H

#include <QString>


class Parameters
{
public:
    static int32_t*** voxels;
    static short int size;
    static int points;
    static QString algorithm;
    static unsigned int seed;
    static QString filename;
    static int num_rnd_loads;
    static int num_threads;
    static QString working_directory; // save working directory
};

#endif // PARAMETERS_H
