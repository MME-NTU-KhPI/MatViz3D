#include "export.h"
#include <QFile>
#include <QDir>
#include <QFileDialog>

void Export::ExportToCSV(short int numCubes, int32_t ***voxels)
{
    if (Parameters::filename.isNull())
    {
        Parameters::filename = QFileDialog::getSaveFileName(nullptr, QFileDialog::tr("Save CSV File"), QDir::homePath(), QFileDialog::tr("CSV Files (*.csv);;All Files (*)"));
    }
    QFile file(Parameters::filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Cannot open file!";
        return;
    }
    QTextStream out(&file);
    out << "X;Y;Z;Colors\n";
    for(int k = 0; k < numCubes; k++)
    {
        for (int i = 0; i < numCubes; i++)
        {
            for(int j = 0; j < numCubes; j++)
            {
                out << k << ";" << i << ";" << j << ";" << voxels[k][i][j] << "\n";
            }
        }
    }
    file.close();
}

void Export::ExportToVRML(const std::vector<std::array<GLubyte, 4>>& colors, short int numCubes, int32_t ***voxels)
{
    QString fileName = QFileDialog::getSaveFileName(nullptr, QFileDialog::tr("Save VRML File"), QDir::homePath(), QFileDialog::tr("VRML Files (*.wrl);;All Files (*)"));
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Cannot open file!";
        return;
    }

    QTextStream out(&file);
    out << "#VRML V2.0 utf8\n\n";

    for (int k = 0; k < numCubes; k++) {
        for (int i = 0; i < numCubes; i++) {
            for (int j = 0; j < numCubes; j++) {
                out << "Transform {\n";
                out << "  translation " << k << " " << i << " " << j << "\n";
                out << "  children Shape {\n";
                out << "    appearance Appearance {\n";
                out << "      material Material {\n";
                out << "        diffuseColor " << (colors[voxels[k][i][j] - 1][0] / 255.0) << " "
                    << (colors[voxels[k][i][j] - 1][1] / 255.0) << " "
                    << (colors[voxels[k][i][j] - 1][2] / 255.0) << "\n";
                out << "      }\n";
                out << "    }\n";
                out << "    geometry Box {\n";
                out << "      size 1.0 1.0 1.0\n";
                out << "    }\n";
                out << "  }\n";
                out << "}\n";
            }
        }
    }
    file.close();
}
