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

float Parameters::wave_coefficient;

float Parameters::halfaxis_a;
float Parameters::halfaxis_b;
float Parameters::halfaxis_c;

float Parameters::orientation_angle_a = 0.0;
float Parameters::orientation_angle_b = 0.0;
float Parameters::orientation_angle_c = 0.0;
