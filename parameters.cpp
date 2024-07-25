#include "parameters.h"
#include <QDir>

int16_t*** Parameters::voxels;
short int Parameters::size;
int Parameters::points;
QString Parameters::algorithm;
QString Parameters::filename;
int Parameters::num_rnd_loads; // number of loading poinst in stressanalis.cpp
