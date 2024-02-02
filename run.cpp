#include <iostream>
#include <cstdint>
#include "parent_algorithm.h"
#include "moore.h"
#include "neumann.h"
#include "probability_circle.h"
#include "probability_ellipse.h"
#include "radial.h"

int main(int argc, chat *argv[])
{
	int16_t*** voxels;
	if (argv[3] == "moore")
	{
		Moore start;
		voxels = start.Generate_Initial_Cube(argv[1]);
		std::vector<Parent_Algorithm::Coordinate> grains = start.Generate_Random_Starting_Points(voxels,argv[1],argv[2]);
		start.Generate_Filling(voxels,argv[1],0,grains);
	}
	else if (argv[3] == "neumann")
	{
		Neumann start;
		voxels = start.Generate_Initial_Cube(argv[1]);
		std::vector<Parent_Algorithm::Coordinate> grains = start.Generate_Random_Starting_Points(voxels,argv[1],argv[2]);
		start.Generate_Filling(voxels,argv[1],0,grains);
	}
	else if (argv[3] == "prob_circle")
	{
		Probability_Circle start;
		voxels = start.Generate_Initial_Cube(argv[1]);
		std::vector<Parent_Algorithm::Coordinate> grains = start.Generate_Random_Starting_Points(voxels,argv[1],argv[2]);
		start.Generate_Filling(voxels,argv[1],0,grains);
	}
	else if (argv[3] == "prob_ellipse")
	{
		Probability_Ellipse start;
		voxels = start.Generate_Initial_Cube(argv[1]);
		std::vector<Parent_Algorithm::Coordinate> grains = start.Generate_Random_Starting_Points(voxels,argv[1],argv[2]);
		start.Generate_Filling(voxels,argv[1],0,grains);
	}
	else if (argv[3] == "radial")
	{
		Radial start;
		voxels = start.Generate_Initial_Cube(argv[1]);
		std::vector<Parent_Algorithm::Coordinate> grains = start.Generate_Random_Starting_Points(voxels,argv[1],argv[2]);
		start.Generate_Filling(voxels,argv[1],0,grains);
	}
	return static_cast<int>(voxels);
}