//#include <iostream>
//#include <windows.h>
//#include <ctime>
//#include <list>
//#include <cmath>
//#include <probability_circle.h>
//#include <myglwidget.h>
//#include "mainwindow.h"
//#include "ui_mainwindow.h"
//#include "vonneumann.h"
//using namespace std;

//VonNeumann::VonNeumann()
//{

//}

//void VonNeumann::Generate_Filling(uint16_t***voxels, short numCubes)
//{
//    bool answer = true;

//    while (answer) {
//        for (int z = 0; z < numCubes; z++)
//        {
//            for (int i = 0; i < numCubes; i++)
//            {
//                for (int j = 0; j < numCubes; j++)
//                {
//                    if (voxels[z][i][j] > 0)
//                    {
//                        for (int k = -1; k < 2; k++)
//                        {
//                            if (voxels[z + k][i][j] == 0 && ((z + k >= 0)  (z + k <= numCubes - 1)))
//                            {
//                                if (k == 0) continue;
//                                voxels[z + k][i][j] = -1;
//                            }
//                            else if (voxels[z][i + k][j] == 0 && (i + k >= 0)  (i + k <= numCubes - 1))
//                            {
//                                if (k == 0) continue;
//                                voxels[z][i + k][j] = -1;
//                            }
//                            else if (voxels[z][i][j + k] == 0 && (j + k >= 0)  (j + k <= numCubes - 1))
//                            {
//                                if (k == 0) continue;
//                                voxels[z][i][j + k] = -1;
//                            }
//                        }
//                    }
//                }
//            }
//        }

//        for (int z = 0; z < numCubes; z++)
//        {
//            for (int i = 0; i < numCubes; i++)
//            {
//                for (int j = 0; j < numCubes; j++)
//                {
//                    srand(time(NULL));
//                    if (voxels[z][i][j] >= 1 /*&& (1 + rand() % 100 < *(*(*(*(voxels + z) + i) + j)) * 100)*/)
//                    {
//                        for (int k = -1; k < 2; k++)
//                        {
//                            if (voxels[z + k][i][j] == -1 && ((z + k >= 0)  (z + k <= numCubes - 1)))
//                            {
//                                if (1 + rand() % 100 < voxels[z + k][i][j] * 100)
//                                {
//                                    if (k == 0) continue;
//                                    voxels[z + k][i][j] = voxels[z][i][j];
//                                }
//                            }
//                            else if (voxels[z][i + k][j] == -1 && ((i + k >= 0)  (i + k <= numCubes - 1)))
//                            {
//                                if (1 + rand() % 100 < voxels[z][i + k][j] * 100)
//                                {
//                                    if (k == 0) continue;
//                                    voxels[z][i + k][j] = voxels[z][i][j];
//                                }
//                            }
//                            else if (voxels[z][i][j + k] == -1 && ((j + k >= 0)  (j + k <= numCubes - 1)))
//                            {
//                                if (1 + rand() % 100 < voxels[z][i][j + k] * 100)
//                                {
//                                    if (k == 0) continue;
//                                    voxels[z][i][j + k] = voxels[z][i][j];
//                                }
//                            }
//                        }
//                    }
//                }
//            }
//        }
//        int k = 0;
//        for (; k < numCubes; k++)
//        {
//            int i = 0;
//            for (; i < numCubes; i++)
//            {
//                int j = 0;
//                for (; j < numCubes; j++)
//                {
//                    if (voxels[k][i][j] == 0)
//                    {
//                        answer = true;
//                        break;
//                    }
//                }

//                if (j < numCubes)
//                {
//                    break;
//                }
//            }

//            if (i < numCubes)
//            {
//                break;
//            }
//        }

//        if (k == numCubes)
//        {
//            answer = false;
//        }
//    }
//}
