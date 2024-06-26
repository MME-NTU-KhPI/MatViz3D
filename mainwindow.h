
#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#define H5_BUILT_AS_DYNAMIC_LIB
#include <QMainWindow>
#include <QButtonGroup>
#include <QPushButton>
#include <QWidget>
#include <QSlider>
#include "animation.h"
#include "statistics.h"
#include "messagehandler.h"
#include "about.h"
#include <hdf5handler.h>


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow

{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    void setNumCubes(short int arg);
    void setNumColors(int arg);
    void setAlgorithms(QString arg);
    void callExportToCSV();
    int isAnimation = 0;
    int isWaveGeneration = 0;
    int isClosedCube;
    int delayAnimation;
    ~MainWindow();

public slots:
    void onStartClicked();

private slots:
    void checkStart();
    void onLogMessageWritten(const QString &message);
    void setupFileMenu();
    void saveAsImage();
    void exportToWRL();
    void estimateStressWithANSYS();
    void setupWindowMenu();
    void onConsoleCheckBoxChanged(int state);
    void onAnimationCheckBoxChanged(int state);
    void onDataCheckBoxChanged(int state);
    void onAllCheckBoxChanged(int state);
    void exportToCSV();
    void saveHDF();
    void openHDF();
    void on_Start_clicked();

    void on_statistics_clicked();

    void on_checkBoxAnimation_stateChanged(int arg1);

    void on_AboutButton_clicked();

    void on_SliderAnimationSpeed_valueChanged(int value);

    void on_checkBoxWiregrid_stateChanged(int arg1);

protected:
    void keyPressEvent(QKeyEvent *event);

private:
    Ui::MainWindow *ui;
    Statistics form;
    About *about;
    MessageHandler *messageHandlerInstance;
    QCheckBox *allCheckBox;
    QCheckBox *dataCheckBox;
    QCheckBox *consoleCheckBox;
    QCheckBox *animationCheckBox;
    bool startButtonPressed;
    HDF5Handler m_h5handler;
};

#endif // MAINWINDOW_H
