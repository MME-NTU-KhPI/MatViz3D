
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "myglwidget.h"
#include <QtWidgets>
#include "probability_circle.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->Rectangle8, &QLineEdit::textChanged, this, &MainWindow::checkInputFields);
    connect(ui->Rectangle9, &QLineEdit::textChanged, this, &MainWindow::checkInputFields);

    connect(ui->Rectangle8, &QLineEdit::editingFinished, this, [=]() {
        bool ok;
        numCubes = ui->Rectangle8->text().toInt(&ok);
        if (ok) {
            ui->myGLWidget->setNumCubes(numCubes);
        }
    });

    ui->Rectangle10->setSingleStep(0.0001);
    ui->Rectangle10->setTickInterval(0.05);
    connect(ui->Rectangle10, &QSlider::valueChanged, ui->myGLWidget, &MyGLWidget::setDistanceFactor);


    checkInputFields();

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::checkInputFields()
{
    if (ui->Rectangle8->text().isEmpty() || ui->Rectangle9->text().isEmpty()) {
        ui->Algorithm1->setEnabled(false);
        ui->Algorithm1->setStyleSheet("background: #282828; border: 1px solid #000000; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #171616;");

        ui->Algorithm2->setEnabled(false);
        ui->Algorithm2->setStyleSheet("background: #282828; border: 1px solid #000000; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #171616;");

        ui->Algorithm3->setEnabled(false);
        ui->Algorithm3->setStyleSheet("background: #282828; border: 1px solid #000000; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #171616;");

        ui->Algorithm4->setEnabled(false);
        ui->Algorithm4->setStyleSheet("background: #282828; border: 1px solid #000000; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #171616;");
    } else {
        ui->Algorithm1->setEnabled(true);
        ui->Algorithm1->setStyleSheet("background: #282828; border: 1px solid #969696; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #969696;");

        ui->Algorithm2->setEnabled(true);
        ui->Algorithm2->setStyleSheet("background: #282828; border: 1px solid #969696; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #969696;");

        ui->Algorithm3->setEnabled(true);
        ui->Algorithm3->setStyleSheet("background: #282828; border: 1px solid #969696; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #969696;");

        ui->Algorithm4->setEnabled(true);
        ui->Algorithm4->setStyleSheet("background: #282828; border: 1px solid #969696; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #969696;");
    }
}

void MainWindow::on_Algorithm1_clicked(bool checked)
{
    ui->Algorithm1->setStyleSheet("font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; background: #969696; color: #000000; border: 1px solid #969696; border-radius: 15px;");

}


void MainWindow::on_Algorithm2_clicked(bool checked)
{
    ui->Algorithm2->setStyleSheet("font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; background: #969696; color: #000000; border: 1px solid #969696; border-radius: 15px;");
}



void MainWindow::on_Algorithm3_clicked(bool checked)
{
    ui->Algorithm3->setStyleSheet("font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; background: #969696; color: #000000; border: 1px solid #969696; border-radius: 15px;");
}


void MainWindow::on_Algorithm4_clicked(bool checked)
{
    ui->Algorithm4->setStyleSheet("font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; background: #969696; color: #000000; border: 1px solid #969696; border-radius: 15px;");
}


void MainWindow::on_Colormap_stateChanged(int arg1)
{

}


void MainWindow::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape)
        close();
    else
        QWidget::keyPressEvent(e);
}

void MainWindow::on_Start_clicked()
{

    Probability_Circle start;
    uint16_t*** voxel = start.Generate_Initial_Cube(numCubes);
    ui->myGLWidget->setVoxel(voxel, numCubes);
    ui->myGLWidget->update();
}

