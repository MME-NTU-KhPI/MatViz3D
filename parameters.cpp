#include "parameters.h"
#include <QDir>

int16_t*** Parameters::voxels = 0;
short int Parameters::size = 0;
int Parameters::points = 0;
int Parameters::generations = 1;
QString Parameters::algorithm;
QString Parameters::filename = QDir::currentPath();
