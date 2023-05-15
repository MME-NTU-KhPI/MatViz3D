
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QButtonGroup>
#include <QWidget>
#include <QSlider>
#include "probability_circle.h"


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow

{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    short int numCubes = 1;
    ~MainWindow();

private slots:
    void checkInputFields();

    void on_Algorithm1_clicked(bool checked);

    void on_Algorithm2_clicked(bool checked);


    void on_Algorithm3_clicked(bool checked);

    void on_Algorithm4_clicked(bool checked);

    void on_Colormap_stateChanged(int arg1);


    //void on_Start_clicked();

    //void on_Rectangle10_valueChanged(int value);

    void on_Start_clicked();

protected:
    void keyPressEvent(QKeyEvent *event);

private:
    Ui::MainWindow *ui;


};

#endif // MAINWINDOW_H
