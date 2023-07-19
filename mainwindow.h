
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QButtonGroup>
#include <QPushButton>
#include <QWidget>
#include <QSlider>
#include <QThread>
#include "probability_circle.h"
#include "probability_ellipse.h"
//#include "vonneumann.h"


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow

{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    short int numCubes = 1;
    int numColors;
    ~MainWindow();

private slots:
    void checkInputFields();
    void checkStart(bool algorithm1, bool algorithm2, bool algorithm3, bool algorithm4);

    void on_Algorithm1_clicked(bool checked);

    void on_Algorithm2_clicked(bool checked);


    void on_Algorithm3_clicked(bool checked);

    void on_Algorithm4_clicked(bool checked);

    void on_Colormap_stateChanged(int arg1);


    //void on_Start_clicked();

    //void on_Rectangle10_valueChanged(int value);

    void on_Start_clicked();

    void on_pushButton_clicked();

protected:
    void keyPressEvent(QKeyEvent *event);

private:
    Ui::MainWindow *ui;
    QPushButton *buttons[4];


};

#endif // MAINWINDOW_H
