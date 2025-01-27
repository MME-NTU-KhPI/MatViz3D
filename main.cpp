#include "mainwindow.h"
#include "console.h"
#include <QCommandLineParser>
#include <QApplication>
#include <QProcessEnvironment>
#include <QCoreApplication>
#include <QDir>

#include <iostream>

QString logo_full_qstr = R"(
                     **
                 **********
              #***************
           #*********************
        *****************************
     **********************************
  #*************************************#
  *++++*****************************###%%
  *+++++++***********************##%%%%%%   __  __       ___      ___     ____  _____
  *++++++++++*****************##%%%%%%%%%  |  \/  |     | \ \    / (_)   |___ \|  __ \
  *+++===++++++++**********##%%%%%%#**%%%  | \  / | __ _| |\ \  / / _ ____ __) | |  | |
  *+++=--=++++++++++****#%%%%%%%%%*++#%%%  | |\/| |/ _` | __\ \/ / | |_  /|__ <| |  | |
  *+++=---++++++++++++##%%%%%#%%%#+++%%%%  | |  | | (_| | |_ \  /  | |/ / ___) | |__| |
  *+++=-===++++====+++#####*++%%%#++#%%%%  |_|  |_|\__,_|\__| \/   |_/___|____/|_____/
  *+++=-==-=++=----=++####*+++#%%*++%%%%%
  *+++=-=+=-==-----=++#####++++##++#%%%%%
  *+++=-=+=----==--=++#####*+++*+++#%%%%%
  *+++===+=----+=--=++######*+++++*#####%
   ++++++++=--=+=--=++#######+++++#######
       +++++++++=--=++########+*#####
          ++++++==-=++############
             +++++++++#########
                *+++++######
                   +++###

)";

QString logo_qstr = R"(
                        **
                    **********
                 #***************
              #*********************
           *****************************
        **********************************
     #*************************************#
     *++++*****************************###%%
     *+++++++***********************##%%%%%%
     *++++++++++*****************##%%%%%%%%%
     *+++===++++++++**********##%%%%%%#**%%%
     *+++=--=++++++++++****#%%%%%%%%%*++#%%%
     *+++=---++++++++++++##%%%%%#%%%#+++%%%%
     *+++=-===++++====+++#####*++%%%#++#%%%%
     *+++=-==-=++=----=++####*+++#%%*++%%%%%
     *+++=-=+=-==-----=++#####++++##++#%%%%%
     *+++=-=+=----==--=++#####*+++*+++#%%%%%
     *+++===+=----+=--=++######*+++++*#####%
      ++++++++=--=+=--=++#######+++++#######
          +++++++++=--=++########+*#####
             ++++++==-=++############
                +++++++++#########
                   *+++++######
                      +++###

)";

QString textlogo_qstr = R"(
  __  __       ___      ___     ____  _____
 |  \/  |     | \ \    / (_)   |___ \|  __ \
 | \  / | __ _| |\ \  / / _ ____ __) | |  | |
 | |\/| |/ _` | __\ \/ / | |_  /|__ <| |  | |
 | |  | | (_| | |_ \  /  | |/ / ___) | |__| |
 |_|  |_|\__,_|\__| \/   |_/___|____/|_____/

)";

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setApplicationName("MatViz3D");
    QApplication::setApplicationVersion("2.01");
    QCommandLineParser parser;

    std::cout << logo_full_qstr.toLatin1().constData() << std::endl;
    std::cout.flush();

    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(QCommandLineOption("size","Set the size of cube", "size"));
    parser.addOption(QCommandLineOption("points","Set the number of points", "points"));
    parser.addOption(QCommandLineOption("concentration","Set the concentration of initial points in the cube(%)", "concentration"));
    parser.addOption(QCommandLineOption("algorithm", "Set the algorithm of generation", "algorithm"));
    parser.addOption(QCommandLineOption("seed","Set the seed of generation","seed"));
    parser.addOption(QCommandLineOption("np", "Set the number of processors for single or multi-threaded execution of algorithms.", "num_threads"));
    parser.addOption(QCommandLineOption("wave_coefficient", "Coefficient for wave generation", "value"));
    parser.addOption(QCommandLineOption("halfaxis_a", "The length of the semi-axis A for the Probability algorithm", "value"));
    parser.addOption(QCommandLineOption("halfaxis_b", "The length of the semi-axis B for the Probability algorithm", "value"));
    parser.addOption(QCommandLineOption("halfaxis_c", "The length of the semi-axis C for the Probability algorithm", "value"));
    parser.addOption(QCommandLineOption("orientation_angle_a", "Rotation angle of the x-axis for the Probability algorithm", "value"));
    parser.addOption(QCommandLineOption("orientation_angle_b", "Rotation angle of the y-axis for the Probability algorithm", "value"));
    parser.addOption(QCommandLineOption("orientation_angle_c", "Rotation angle of the z-axis for the Probability algorithm", "value"));
    parser.addOption(QCommandLineOption("autostart","Running a program with auto-generation of a cube"));
    parser.addOption(QCommandLineOption("nogui","Running a program with no GUI"));
    parser.addOption(QCommandLineOption("output", "Specify output file for generated cube", "directory"));
    parser.addOption(QCommandLineOption("num_rnd_loads", "Set number of random loads (as eps) for stress analis", "num_rnd_loads"));
    parser.addOption(QCommandLineOption("run_stress_calc", "Run FEM to estimate stresses and strains"));
    parser.addOption(QCommandLineOption("working_directory", "Set path where ansys working directory will be stored","working_directory"));
    parser.process(a);
    MainWindow w;
    Console::processOptions(parser,w);
    return a.exec();
}
