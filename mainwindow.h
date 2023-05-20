
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QButtonGroup>
#include <QPushButton>
#include <QScreen>
#include <QFileDialog>
#include <QPixmap>
#include <QPainter>
#include <QGraphicsDropShadowEffect>


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow

{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void checkInputFields();
    void checkStart(bool algorithm1, bool algorithm2, bool algorithm3, bool algorithm4);
    void setWidgetVisibility(QWidget* widget, bool visible);

    void on_Algorithm1_clicked(bool checked);

    void on_Algorithm2_clicked(bool checked);


    void on_Algorithm3_clicked(bool checked);

    void on_Algorithm4_clicked(bool checked);


    void on_Start_clicked();

    void on_Save_clicked();

private:
    Ui::MainWindow *ui;
    QPushButton *buttons[4];
};

#endif // MAINWINDOW_H
