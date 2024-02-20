#include "export.h"
#include <QFile>
#include <QDir>
#include <QFileDialog>

void Export::ExportToCSV(short int numCubes, int16_t ***voxels)
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

//void Export::ExportToVRML(const QString& filename, short int numCubes, int16_t ***voxels)
//{

//}
