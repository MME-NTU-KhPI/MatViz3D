#include "parameters.h"
#include <QDir>

int32_t*** Parameters::voxels;
short int Parameters::size;
int Parameters::points;
unsigned int Parameters::seed;
QString Parameters::algorithm;
QString Parameters::filename;
int Parameters::num_rnd_loads; // number of loading poinst in stressanalis.cpp

QString Parameters::working_directory;

float Parameters::wave_coefficient = 1.0f; // Значення за замовчуванням
