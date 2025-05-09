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

    Console::setupParser(parser);
    parser.process(a);
    MainWindow w;
    Console::processOptions(parser,w);
    return a.exec();
}
