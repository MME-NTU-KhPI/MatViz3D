
#include <iostream>
#include <chrono>
#include <Windows.h>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "myglwidget.h"
#include <QtWidgets>
#include "probability_circle.h"
#include <QMessageBox>
#include <QPushButton>
#include "neumann.h"
#include "moore.h"
#include "probability_ellipse.h"
#include "parent_algorithm.h"
#include <QFileDialog>


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

    connect(ui->Rectangle9, &QLineEdit::editingFinished, this, [=]() {
        bool ok;
        numColors = ui->Rectangle9->text().toInt(&ok);
        if (ok) {
            ui->myGLWidget->setNumColors(numColors);
        }
    });

    ui->Rectangle10->setMinimum(0);
    ui->Rectangle10->setMaximum(10);
    ui->Rectangle10->setSingleStep(1);
    ui->Rectangle10->setTickInterval(5);
    connect(ui->Rectangle10, &QSlider::valueChanged, ui->myGLWidget, &MyGLWidget::setDistanceFactor);

    checkInputFields();
    buttons[0] = ui->Algorithm1;
    buttons[1] = ui->Algorithm2;
    buttons[2] = ui->Algorithm3;
    buttons[3] = ui->Algorithm4;

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
        ui->Algorithm1->setStyleSheet("background: #282828; border: 1px solid #545252; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #545252;");

        ui->Algorithm2->setEnabled(true);
        ui->Algorithm2->setStyleSheet("background: #282828; border: 1px solid #545252; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #545252;");

        ui->Algorithm3->setEnabled(true);
        ui->Algorithm3->setStyleSheet("background: #282828; border: 1px solid #545252; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #545252;");

        ui->Algorithm4->setEnabled(true);
        ui->Algorithm4->setStyleSheet("background: #282828; border: 1px solid #545252; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #545252;");
    }
}

// Функція перевірки для старт
void MainWindow::checkStart(bool algorithm1, bool algorithm2, bool algorithm3, bool algorithm4)
{
    bool checked = algorithm1 || algorithm2 || algorithm3 || algorithm4;

    if (checked) {
        ui->Start->setEnabled(true);
        ui->Start->setStyleSheet("background: #282828; border-radius: 8px; width: 280px; height: 93px; color: #969696; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 48px; line-height: 58px;");
    } else if(!checked) {
        ui->Start->setEnabled(false);
        ui->Start->setStyleSheet("background: #282828; border-radius: 8px; width: 280px; height: 93px; color: rgba(150, 150, 150, 0.5); font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 48px; line-height: 58px;");
    }
}

void MainWindow::on_Algorithm1_clicked(bool checked)
{
    if(checked)
    {
        ui->Algorithm1->setStyleSheet("font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; background: #282828; color: #969696; border: 1px solid #969696; border-radius: 15px;");

        // перебор всех кнопок и установка состояния "unchecked" для всех, кроме текущей
        for(int i = 0; i < 4; i++)
        {
            if(buttons[i] != ui->Algorithm1)
            {
                buttons[i]->setChecked(false);
                buttons[i]->setStyleSheet("background: #282828; border: 1px solid #545252; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #545252;");
            }
        }
    }
    else
    {
        ui->Algorithm1->setStyleSheet("background: #282828; border: 1px solid #545252; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #545252;");
    }
    checkStart(ui->Algorithm1->isChecked(), ui->Algorithm2->isChecked(), ui->Algorithm3->isChecked(), ui->Algorithm4->isChecked());
}


void MainWindow::on_Algorithm2_clicked(bool checked)
{
    if(checked)
    {
        ui->Algorithm2->setStyleSheet("font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; background: #282828; color: #969696; border: 1px solid #969696; border-radius: 15px;");

        // перебор всех кнопок и установка состояния "unchecked" для всех, кроме текущей
        for(int i = 0; i < 4; i++)
        {
            if(buttons[i] != ui->Algorithm2)
            {
                buttons[i]->setChecked(false);
                buttons[i]->setStyleSheet("background: #282828; border: 1px solid #545252; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #545252;");
            }
        }
    }
    else
    {
        ui->Algorithm2->setStyleSheet("background: #282828; border: 1px solid #545252; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #545252;");
    }
    checkStart(ui->Algorithm1->isChecked(), ui->Algorithm2->isChecked(), ui->Algorithm3->isChecked(), ui->Algorithm4->isChecked());
}



void MainWindow::on_Algorithm3_clicked(bool checked)
{
    if(checked)
    {
        ui->Algorithm3->setStyleSheet("font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; background: #282828; color: #969696; border: 1px solid #969696; border-radius: 15px;");

        // перебор всех кнопок и установка состояния "unchecked" для всех, кроме текущей
        for(int i = 0; i < 4; i++)
        {
            if(buttons[i] != ui->Algorithm3)
            {
                buttons[i]->setChecked(false);
                buttons[i]->setStyleSheet("background: #282828; border: 1px solid #545252; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #545252;");
            }
        }
    }
    else
    {
        ui->Algorithm3->setStyleSheet("background: #282828; border: 1px solid #545252; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #545252;");
    }
    checkStart(ui->Algorithm1->isChecked(), ui->Algorithm2->isChecked(), ui->Algorithm3->isChecked(), ui->Algorithm4->isChecked());
}


void MainWindow::on_Algorithm4_clicked(bool checked)
{
    if(checked)
    {
        ui->Algorithm4->setStyleSheet("font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; background: #282828; color: #969696; border: 1px solid #969696; border-radius: 15px;");

        // перебор всех кнопок и установка состояния "unchecked" для всех, кроме текущей
        for(int i = 0; i < 4; i++)
        {
            if(buttons[i] != ui->Algorithm4)
            {
                buttons[i]->setChecked(false);
                buttons[i]->setStyleSheet("background: #282828; border: 1px solid #545252; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #545252;");
            }
        }
    }
    else
    {
        ui->Algorithm4->setStyleSheet("background: #282828; border: 1px solid #545252; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #545252;");
    }
    checkStart(ui->Algorithm1->isChecked(), ui->Algorithm2->isChecked(), ui->Algorithm3->isChecked(), ui->Algorithm4->isChecked());
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
    if (ui->Algorithm1->isChecked())
    {

        if(std::isdigit(numCubes) == 0 && numCubes <= 0)
        {
            QMessageBox::information(nullptr,"Попередження!","Введено неправильне значення розміру куба! Це може призвести до неправильної роботи програми.");
        }
        else if(std::isdigit(numColors) == 0 && numColors <= 0)
        {
            QMessageBox::information(nullptr,"Попередження!","Введено неправильне значення кількості початкових точок!\nЦе призведе до неправильної роботи програми!");
        }
        else
        {
            Neumann start;
            MyGLWidget* glWidget = ui->myGLWidget;
            int16_t*** voxels = start.Generate_Initial_Cube(numCubes, numColors);
            start.Generate_Filling(voxels,numCubes,glWidget);
//            ui->myGLWidget->setVoxels(voxels, numCubes);
//            ui->myGLWidget->update();
            ui->Start->setText("RELOAD");
        }

    }
    else if (ui->Algorithm2->isChecked())
    {
        if(std::isdigit(numCubes) == 0 && numCubes <= 0)
        {
            QMessageBox::information(nullptr,"Попередження!","Введено неправильне значення розміру куба! Це може призвести до неправильної роботи програми.");
        }
        else if(std::isdigit(numColors) == 0 && numColors <= 0)
        {
            QMessageBox::information(nullptr,"Попередження!","Введено неправильне значення кількості початкових точок!\nЦе призведе до неправильної роботи програми!");
        }
        else
        {
            Probability_Circle start;
            MyGLWidget* glWidget = ui->myGLWidget;
            int16_t*** voxels = start.Generate_Initial_Cube(numCubes, numColors);
            start.Generate_Filling(voxels,numCubes,glWidget);
//            ui->myGLWidget->setVoxels(voxels, numCubes);
//            ui->myGLWidget->update();
            ui->Start->setText("RELOAD");
        }
    }
    else if (ui->Algorithm3->isChecked())
    {
        if(std::isdigit(numCubes) == 0 && numCubes <= 0)
        {
            QMessageBox::information(nullptr,"Попередження!","Введено неправильне значення розміру куба! Це може призвести до неправильної роботи програми.");
        }
        else if(std::isdigit(numColors) == 0 && numColors <= 0)
        {
            QMessageBox::information(nullptr,"Попередження!","Введено неправильне значення кількості початкових точок!\nЦе призведе до неправильної роботи програми!");
        }
        else
        {
            Probability_Ellipse start;
            MyGLWidget* glWidget = ui->myGLWidget;
            int16_t*** voxels = start.Generate_Initial_Cube(numCubes, numColors);
            start.Generate_Filling(voxels,numCubes,glWidget);
//            ui->myGLWidget->setVoxels(voxels, numCubes);
//            ui->myGLWidget->update();
            ui->Start->setText("RELOAD");
        }
    }
    else if (ui->Algorithm4->isChecked())
    {
        if(std::isdigit(numCubes) == 0 && numCubes <= 0)
        {
            QMessageBox::information(nullptr,"Попередження!","Введено розмір кубу, який менше або дорівнює нулю!");
        }
        else if(std::isdigit(numColors) == 0 && numColors <= 0)
        {
            QMessageBox::information(nullptr,"Попередження!","Введено неправильне значення кількості початкових точок!\nЦе призведе до неправильної роботи програми!");
        }
        else
        {
            Moore start;
            MyGLWidget* glWidget = ui->myGLWidget;
            int16_t*** voxels = start.Generate_Initial_Cube(numCubes, numColors);
            start.Generate_Filling(voxels,numCubes,glWidget);
//            ui->myGLWidget->setVoxels(voxels, numCubes);
//            ui->myGLWidget->update();
            ui->Start->setText("RELOAD");
        }
    }
}


void MainWindow::on_pushButton_clicked()
{

    QRect rect = ui->Rectangle1->geometry();
    int x = rect.x();
    int y = rect.y();
    int width = rect.width();
    int height = rect.height();

    ui->Visibility->hide();
    ui->wid_start->hide();

    QPixmap pixmap(width, height);
    ui->myGLWidget->render(&pixmap);

    QString fileName = QFileDialog::getSaveFileName(this, "Зберегти зображення", "", "Зображення (*.png);;Всі файли (*.*)");
    if (!fileName.isEmpty()) {
        pixmap.save(fileName);
    }

    ui->Visibility->show();
    ui->wid_start->show();

}

