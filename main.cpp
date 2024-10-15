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

    std::cout << logo_full_qstr.toLatin1().constData();
    std::cout.flush();

    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(QCommandLineOption("size","Set the size of cube", "size"));
    parser.addOption(QCommandLineOption("points","Set the number of points", "points"));
    parser.addOption(QCommandLineOption("concentration","Set the concentration of initial points in the cube(%)", "concentration"));
    parser.addOption(QCommandLineOption("algorithm", "Set the algorithm of generation", "algorithm"));
    parser.addOption(QCommandLineOption("seed","Set the seed of generation","seed"));
    parser.addOption(QCommandLineOption("autostart","Running a program with auto-generation of a cube"));
    parser.addOption(QCommandLineOption("nogui","Running a program with no GUI"));
    parser.addOption(QCommandLineOption("output", "Specify output file for generated cube", "directory"));
    parser.addOption(QCommandLineOption("num_rnd_loads", "Set number of random loads (as eps) for stress analis", "num_rnd_loads"));
    parser.addOption(QCommandLineOption("run_stress_calc", "Run FEM to estimate stresses and strains"));
    parser.process(a);
    MainWindow w;
    Console::processOptions(parser,w);
    return a.exec();
}
