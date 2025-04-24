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

    static float wave_coefficient;

    static float halfaxis_a;
    static float halfaxis_b;
    static float halfaxis_c;

    static float orientation_angle_a;
    static float orientation_angle_b;
    static float orientation_angle_c;

    static bool nogui;
};

#endif // PARAMETERS_H
