#include "parameters.h"
#include <QDir>

int32_t*** Parameters::voxels;
short int Parameters::size = 1;
int Parameters::points;
unsigned int Parameters::seed;
int Parameters::num_threads;
QString Parameters::algorithm;
QString Parameters::filename;
int Parameters::num_rnd_loads; // number of loading poinst in stressanalis.cpp

QString Parameters::working_directory;

float Parameters::wave_coefficient;

float Parameters::halfaxis_a = 0.0f;
float Parameters::halfaxis_b = 0.0f;
float Parameters::halfaxis_c = 0.0f;

float Parameters::orientation_angle_a = 0.0f;
float Parameters::orientation_angle_b = 0.0f;
float Parameters::orientation_angle_c = 0.0f;

bool Parameters::nogui = false;
