
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QButtonGroup>
#include <QPushButton>
#include <QWidget>
#include <QSlider>
#include "animation.h"
#include "probability_circle.h"
#include "probability_ellipse.h"
#include "statistics.h"
#include "messagehandler.h"
#include "about.h"


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow

{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    short int numCubes = 1;
    int isAnimation;
    int isClosedCube;
    int delayAnimation;

    int numColors;
    ~MainWindow();

private slots:
//    void checkInputFields();
    void checkStart();
    void onLogMessageWritten(const QString &message);
    void setupFileMenu();
    void saveAsImage();
    void setupWindowMenu();
    void onConsoleCheckBoxChanged(int state);
    void onAnimationCheckBoxChanged(int state);
    void onDataCheckBoxChanged(int state);
    void onAllCheckBoxChanged(int state);

//    void on_Algorithm1_clicked(bool checked);

//    void on_Algorithm2_clicked(bool checked);


//    void on_Algorithm3_clicked(bool checked);

//    void on_Algorithm4_clicked(bool checked);

    void on_Start_clicked();

    void on_statistics_clicked();

    void on_checkBoxAnimation_stateChanged(int arg1);

    void on_AboutButton_clicked();

protected:
    void keyPressEvent(QKeyEvent *event);

private:
    Ui::MainWindow *ui;
//    QPushButton *buttons[4];
    Statistics form;
    About *about;
    MessageHandler *messageHandlerInstance;
    QCheckBox *allCheckBox;
    QCheckBox *dataCheckBox;
    QCheckBox *consoleCheckBox;
    QCheckBox *animationCheckBox;


};

#endif // MAINWINDOW_H
