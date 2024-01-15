
#include <iostream>
#include <chrono>
#include <Windows.h>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "myglwidget.h"
#include <QtWidgets>
#include <QMessageBox>
#include <QPushButton>
#include "radial.h"
#include "neumann.h"
#include "moore.h"
#include "probability_ellipse.h"
#include "probability_circle.h"
#include "parent_algorithm.h"
#include <QFileDialog>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QSequentialAnimationGroup>
#include <ctime>

#include <qgifimage.h>
#include "statistics.h"

int16_t* createVoxelArray(int16_t*** voxels, int numCubes);
std::unordered_map<int16_t, int> countVoxels(int16_t* voxelArray, int numCubes, int numColors);

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setupFileMenu();
    setupWindowMenu();

    connect(ui->statistics, &QPushButton::clicked, this, &MainWindow::on_statistics_clicked);


    //    connect(ui->Rectangle8, &QLineEdit::textChanged, this, &MainWindow::checkInputFields);
    //    connect(ui->Rectangle9, &QLineEdit::textChanged, this, &MainWindow::checkInputFields);

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
    ui->Rectangle10->setMaximum(20);
    ui->Rectangle10->setSingleStep(1);
    ui->Rectangle10->setTickInterval(5);
    connect(ui->Rectangle10, &QSlider::valueChanged, ui->myGLWidget, &MyGLWidget::setDistanceFactor);

    //    checkInputFields();
    //    buttons[0] = ui->Algorithm1;
    //    buttons[1] = ui->Algorithm2;
    //    buttons[2] = ui->Algorithm3;
    //    buttons[3] = ui->Algorithm4;

    messageHandlerInstance = new MessageHandler(ui->textEdit);
    connect(messageHandlerInstance, &MessageHandler::messageWrittenSignal, this, &MainWindow::onLogMessageWritten);
    //connect(saveAsImageAction, &QAction::triggered, this, &MainWindow::saveAsImage);

    startButtonPressed = false;
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onLogMessageWritten(const QString &message)
{
    ui->textEdit->append(message); // Вивід повідомлень в textEdit
}

//void MainWindow::checkInputFields()
//{
//    if (ui->Rectangle8->text().isEmpty() || ui->Rectangle9->text().isEmpty()) {
//        ui->Algorithm1->setEnabled(false);
//        ui->Algorithm1->setStyleSheet("background: #282828; border: 1px solid #000000; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #171616;");

//        ui->Algorithm2->setEnabled(false);
//        ui->Algorithm2->setStyleSheet("background: #282828; border: 1px solid #000000; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #171616;");

//        ui->Algorithm3->setEnabled(false);
//        ui->Algorithm3->setStyleSheet("background: #282828; border: 1px solid #000000; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #171616;");

//        ui->Algorithm4->setEnabled(false);
//        ui->Algorithm4->setStyleSheet("background: #282828; border: 1px solid #000000; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #171616;");
//    } else {
//        ui->Algorithm1->setEnabled(true);
//        ui->Algorithm1->setStyleSheet("background: #282828; border: 1px solid #545252; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #545252;");

//        ui->Algorithm2->setEnabled(true);
//        ui->Algorithm2->setStyleSheet("background: #282828; border: 1px solid #545252; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #545252;");

//        ui->Algorithm3->setEnabled(true);
//        ui->Algorithm3->setStyleSheet("background: #282828; border: 1px solid #545252; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #545252;");

//        ui->Algorithm4->setEnabled(true);
//        ui->Algorithm4->setStyleSheet("background: #282828; border: 1px solid #545252; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #545252;");
//    }
//}

// Функція перевірки для старт
void MainWindow::checkStart()
{
    bool checked = ui->AlgorithmsBox->currentIndex() != -1; // Перевірте, чи обраний елемент
    //bool checked = algorithm1 || algorithm2 || algorithm3 || algorithm4;

    if (checked) {
        ui->Start->setEnabled(true);
        ui->Start->setStyleSheet("background: #282828; border-radius: 8px; width: 280px; height: 93px; color: #CFCECE; font-family: Inter; font-size: 20px; font-style: normal; font-weight: 700; line-height: normal; text-transform: uppercase;");
    } else if(!checked) {
        ui->Start->setEnabled(false);
        ui->Start->setStyleSheet("background: #282828; border-radius: 8px; width: 280px; height: 93px; color: rgba(150, 150, 150, 0.5); font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 48px; line-height: 58px;");
    }
}

//void MainWindow::on_Algorithm1_clicked(bool checked)
//{
//    if(checked)
//    {
//        ui->Algorithm1->setStyleSheet("font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; background: #282828; color: #969696; border: 1px solid #969696; border-radius: 15px;");

//        // перебор всех кнопок и установка состояния "unchecked" для всех, кроме текущей
//        for(int i = 0; i < 4; i++)
//        {
//            if(buttons[i] != ui->Algorithm1)
//            {
//                buttons[i]->setChecked(false);
//                buttons[i]->setStyleSheet("background: #282828; border: 1px solid #545252; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #545252;");
//            }
//        }
//    }
//    else
//    {
//        ui->Algorithm1->setStyleSheet("background: #282828; border: 1px solid #545252; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #545252;");
//    }
//    checkStart(ui->Algorithm1->isChecked(), ui->Algorithm2->isChecked(), ui->Algorithm3->isChecked(), ui->Algorithm4->isChecked());
//}


//void MainWindow::on_Algorithm2_clicked(bool checked)
//{
//    if(checked)
//    {
//        ui->Algorithm2->setStyleSheet("font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; background: #282828; color: #969696; border: 1px solid #969696; border-radius: 15px;");

//        // перебор всех кнопок и установка состояния "unchecked" для всех, кроме текущей
//        for(int i = 0; i < 4; i++)
//        {
//            if(buttons[i] != ui->Algorithm2)
//            {
//                buttons[i]->setChecked(false);
//                buttons[i]->setStyleSheet("background: #282828; border: 1px solid #545252; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #545252;");
//            }
//        }
//    }
//    else
//    {
//        ui->Algorithm2->setStyleSheet("background: #282828; border: 1px solid #545252; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #545252;");
//    }
//    checkStart(ui->Algorithm1->isChecked(), ui->Algorithm2->isChecked(), ui->Algorithm3->isChecked(), ui->Algorithm4->isChecked());
//}



//void MainWindow::on_Algorithm3_clicked(bool checked)
//{
//    if(checked)
//    {
//        ui->Algorithm3->setStyleSheet("font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; background: #282828; color: #969696; border: 1px solid #969696; border-radius: 15px;");

//        // перебор всех кнопок и установка состояния "unchecked" для всех, кроме текущей
//        for(int i = 0; i < 4; i++)
//        {
//            if(buttons[i] != ui->Algorithm3)
//            {
//                buttons[i]->setChecked(false);
//                buttons[i]->setStyleSheet("background: #282828; border: 1px solid #545252; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #545252;");
//            }
//        }
//    }
//    else
//    {
//        ui->Algorithm3->setStyleSheet("background: #282828; border: 1px solid #545252; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #545252;");
//    }
//    checkStart(ui->Algorithm1->isChecked(), ui->Algorithm2->isChecked(), ui->Algorithm3->isChecked(), ui->Algorithm4->isChecked());
//}


//void MainWindow::on_Algorithm4_clicked(bool checked)
//{
//    if(checked)
//    {
//        ui->Algorithm4->setStyleSheet("font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; background: #282828; color: #969696; border: 1px solid #969696; border-radius: 15px;");

//        // перебор всех кнопок и установка состояния "unchecked" для всех, кроме текущей
//        for(int i = 0; i < 4; i++)
//        {
//            if(buttons[i] != ui->Algorithm4)
//            {
//                buttons[i]->setChecked(false);
//                buttons[i]->setStyleSheet("background: #282828; border: 1px solid #545252; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #545252;");
//            }
//        }
//    }
//    else
//    {
//        ui->Algorithm4->setStyleSheet("background: #282828; border: 1px solid #545252; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #545252;");
//    }
//    checkStart(ui->Algorithm1->isChecked(), ui->Algorithm2->isChecked(), ui->Algorithm3->isChecked(), ui->Algorithm4->isChecked());
//}


void MainWindow::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape)
        close();
    else
        QWidget::keyPressEvent(e);
}


void MainWindow::on_Start_clicked()
{
    clock_t start_time = clock(); // Фіксація часу початку виконання
    QString selectedAlgorithm = ui->AlgorithmsBox->currentText();
    if (selectedAlgorithm == "Neumann")
    {

        if(std::isdigit(numCubes) == 0 && numCubes <= 0)
        {
            QMessageBox::information(nullptr, "Warning!", "Invalid cube size value entered! This may lead to incorrect program operation.");
        }
        else if(std::isdigit(numColors) == 0 && numColors <= 0)
        {
            QMessageBox::information(nullptr, "Warning!", "Invalid initial points value entered! This will result in incorrect program operation!");
        }
        else
        {
            ui->Start->setText("Loading...");
            ui->Start->setStyleSheet("background: transparent; color: #CFCECE; font-family: Inter; font-size: 20px; font-style: normal; font-weight: 700; line-height: normal; text-transform: uppercase;");
            QApplication::processEvents();

            Neumann start;
            int16_t*** voxels = start.Generate_Initial_Cube(numCubes);
            std::vector<Parent_Algorithm::Coordinate> grains = start.Generate_Random_Starting_Points(voxels,numCubes,numColors);
            bool answer = true;
            if (isAnimation == 0)
            {
                start.Generate_Filling(voxels,numCubes,isAnimation,grains);
                ui->myGLWidget->setVoxels(voxels,numCubes);
                ui->myGLWidget->update();
            }
            else
            {
                //                QThread* thread = new QThread;
                //                Animation* go = new Animation(voxels,&start,ui->myGLWidget,isAnimation,numCubes,answer);
                //                go->moveToThread(thread);
                //                // Подключаем сигнал о завершении потока к удалению объекта
                //                connect(thread, &QThread::finished, go, &Animation::deleteLater);
                //                // Соединяем старт потока с методом animate
                //                connect(thread, &QThread::started, go, &Animation::animate);
                //                connect(go,&Animation::updateRequested,ui->myGLWidget,&MyGLWidget::updateGLWidget);
                //                thread->start();
                Animation* go = new Animation(voxels,&start,ui->myGLWidget,isAnimation,numCubes,answer,grains);
                connect(go,&Animation::updateRequested,ui->myGLWidget,&MyGLWidget::updateGLWidget);
                go->animate();
            }
            ui->Start->setText("RELOAD");
            ui->Start->setStyleSheet("background: #282828; border-radius: 8px; color: #CFCECE; font-family: Inter; font-size: 20px; font-style: normal; font-weight: 700; line-height: normal; text-transform: uppercase;");
            qDebug() << "NEUMANN";

        }

    }
    else if (selectedAlgorithm == "Probability Circle")
    {
        if(std::isdigit(numCubes) == 0 && numCubes <= 0)
        {
            QMessageBox::information(nullptr, "Warning!", "Invalid cube size value entered! This may result in incorrect program operation.");
        }
        else if(std::isdigit(numColors) == 0 && numColors <= 0)
        {
            QMessageBox::information(nullptr, "Warning!", "Invalid initial points value entered! This will result in incorrect program operation!");
        }
        else
        {
            ui->Start->setText("Loading...");
            ui->Start->setStyleSheet("background: transparent; color: #CFCECE; font-family: Inter; font-size: 20px; font-style: normal; font-weight: 700; line-height: normal; text-transform: uppercase;");
            QApplication::processEvents();

            Probability_Circle start;
            int16_t*** voxels = start.Generate_Initial_Cube(numCubes);
            std::vector<Parent_Algorithm::Coordinate> grains = start.Generate_Random_Starting_Points(voxels,numCubes,numColors);
            bool answer = true;
            if (isAnimation == 0)
            {
                start.Generate_Filling(voxels,numCubes,isAnimation,grains);
                ui->myGLWidget->setVoxels(voxels, numCubes);
                ui->myGLWidget->update();
            }
            else
            {
                //QThread* thread = new QThread(this);
                //connect(this,SIGNAL(destroyed()),thread,SLOT(quit()));
                Animation* go = new Animation(voxels,&start,ui->myGLWidget,isAnimation,numCubes,answer,grains);
                //go->moveToThread(thread);
                connect(go,&Animation::updateRequested,ui->myGLWidget,&MyGLWidget::updateGLWidget);
                //thread->start();
                go->animate();
            }
            ui->Start->setText("RELOAD");
            ui->Start->setStyleSheet("background: #282828; border-radius: 8px; color: #CFCECE; font-family: Inter; font-size: 20px; font-style: normal; font-weight: 700; line-height: normal; text-transform: uppercase;");
            qDebug() << "PROBABILITY CIRCLE";

        }
    }
    else if (selectedAlgorithm == "Probability Ellipse")
    {
        if(std::isdigit(numCubes) == 0 && numCubes <= 0)
        {
            QMessageBox::information(nullptr, "Warning!", "Invalid cube size value entered! This may result in incorrect program operation.");
        }
        else if(std::isdigit(numColors) == 0 && numColors <= 0)
        {
            QMessageBox::information(nullptr, "Warning!", "Invalid initial points value entered!\nThis will result in incorrect program operation!");
        }
        else
        {
            ui->Start->setText("Loading...");
            ui->Start->setStyleSheet("background: transparent; color: #CFCECE; font-family: Inter; font-size: 20px; font-style: normal; font-weight: 700; line-height: normal; text-transform: uppercase;");
            QApplication::processEvents();

            Probability_Ellipse start;
            int16_t*** voxels = start.Generate_Initial_Cube(numCubes);
            std::vector<Parent_Algorithm::Coordinate> grains = start.Generate_Random_Starting_Points(voxels,numCubes,numColors);
            bool answer = true;
            if (isAnimation == 0)
            {
                start.Generate_Filling(voxels,numCubes,isAnimation,grains);
                ui->myGLWidget->setVoxels(voxels, numCubes);
                ui->myGLWidget->update();
            }
            else
            {
                Animation* go = new Animation(voxels,&start,ui->myGLWidget,isAnimation,numCubes,answer,grains);
                connect(go,&Animation::updateRequested,ui->myGLWidget,&MyGLWidget::updateGLWidget);
                go->animate();
            }

            ui->Start->setText("RELOAD");
            ui->Start->setStyleSheet("background: #282828; border-radius: 8px; color: #CFCECE; font-family: Inter; font-size: 20px; font-style: normal; font-weight: 700; line-height: normal; text-transform: uppercase;");
            qDebug() << "PROBABILITY ELLIPSE";

        }
    }
    else if (selectedAlgorithm == "Moore")
    {
        if(std::isdigit(numCubes) == 0 && numCubes <= 0)
        {
            QMessageBox::information(nullptr, "Warning!", "Entered cube size is less than or equal to zero!");
        }
        else if(std::isdigit(numColors) == 0 && numColors <= 0)
        {
            QMessageBox::information(nullptr, "Warning!", "Invalid initial points value entered!\nThis will result in incorrect program operation!");
        }
        else
        {
            ui->Start->setText("Loading...");
            ui->Start->setStyleSheet("background: transparent; color: #CFCECE; font-family: Inter; font-size: 20px; font-style: normal; font-weight: 700; line-height: normal; text-transform: uppercase;");
            QApplication::processEvents();

            Moore start;
            int16_t*** voxels = start.Generate_Initial_Cube(numCubes);
            std::vector<Parent_Algorithm::Coordinate> grains = start.Generate_Random_Starting_Points(voxels,numCubes,numColors);
            bool answer = true;
            if (isAnimation == 0)
            {
                start.Generate_Filling(voxels,numCubes,isAnimation,grains);
                ui->myGLWidget->setVoxels(voxels, numCubes);
                ui->myGLWidget->update();
            }
            else
            {
                Animation* go = new Animation(voxels,&start,ui->myGLWidget,isAnimation,numCubes,answer,grains);
                connect(go,&Animation::updateRequested,ui->myGLWidget,&MyGLWidget::updateGLWidget);
                go->animate();
            }

            ui->Start->setText("RELOAD");
            ui->Start->setStyleSheet("background: #282828; border-radius: 8px; color: #CFCECE; font-family: Inter; font-size: 20px; font-style: normal; font-weight: 700; line-height: normal; text-transform: uppercase;");
            qDebug() << "MOORE";
        }
    }
    else if(selectedAlgorithm == "Radial")
    {
        if(std::isdigit(numCubes) == 0 && numCubes <= 0)
        {
            QMessageBox::information(nullptr, "Warning!", "Entered cube size is less than or equal to zero!");
        }
        else if(std::isdigit(numColors) == 0 && numColors <= 0)
        {
            QMessageBox::information(nullptr, "Warning!", "Invalid initial points value entered!\nThis will result in incorrect program operation!");
        }
        else
        {
            ui->Start->setText("Loading...");
            ui->Start->setStyleSheet("background: transparent; color: #CFCECE; font-family: Inter; font-size: 20px; font-style: normal; font-weight: 700; line-height: normal; text-transform: uppercase;");
            QApplication::processEvents();

            Radial start;
            int16_t*** voxels = start.Generate_Initial_Cube(numCubes);
            std::vector<Parent_Algorithm::Coordinate> grains = start.Generate_Random_Starting_Points(voxels,numCubes,numColors);
            bool answer = true;
            if (isAnimation == 0)
            {
                start.Generate_Filling(voxels,numCubes,isAnimation,grains);
                ui->myGLWidget->setVoxels(voxels, numCubes);
                ui->myGLWidget->update();
            }
            else
            {
                Animation* go = new Animation(voxels,&start,ui->myGLWidget,isAnimation,numCubes,answer,grains);
                connect(go,&Animation::updateRequested,ui->myGLWidget,&MyGLWidget::updateGLWidget);
                go->animate();
            }

            ui->Start->setText("RELOAD");
            ui->Start->setStyleSheet("background: #282828; border-radius: 8px; color: #CFCECE; font-family: Inter; font-size: 20px; font-style: normal; font-weight: 700; line-height: normal; text-transform: uppercase;");
            qDebug() << "RADIAL";
        }
    }

    clock_t end_time = clock(); // Фіксація часу завершення виконання
    double elapsed_time = double(end_time - start_time) / CLOCKS_PER_SEC; // Обчислення часу виконання в секундах

    qDebug() << "Total execution time: " << elapsed_time << " seconds";

    ui->myGLWidget->calculateSurfaceArea();

    startButtonPressed = true;
}

void MainWindow::on_statistics_clicked()
{
    if(startButtonPressed == false)
    {
        QMessageBox::information(nullptr, "Warning!", "The structure was not generated.");
    }
    else{
        QVector<int> colorCounts = ui->myGLWidget->countVoxelColors();
        form.setVoxelCounts(colorCounts);

        QString selectedAlgorithm = ui->AlgorithmsBox->currentText();

        // Отримати вибраний алгоритм
        QString algorithmName;
        if (selectedAlgorithm == "Neumann")
        {
            algorithmName = "Neumann";
        }
        else if (selectedAlgorithm == "Probability Circle")
        {
            algorithmName = "Probability Circle";
        }
        else if (selectedAlgorithm == "Probability Ellipse")
        {
            algorithmName = "Probability Ellipse";
        }
        else if (selectedAlgorithm == "Moore")
        {
            algorithmName = "Moore";
        }

        // Встановити текст іконки вікна
        form.setWindowTitle("Statistics - " + algorithmName);

        // Показати вікно
        form.show();
    }
}


void MainWindow::setupFileMenu() {
    // Створіть QMenu та QAction для FileMenu
    QMenu *fileMenu = new QMenu(this);

    // Додайте Action для кнопки Project та його підменю
    //    QAction *projectAction = new QAction("Project", this);
    //    QMenu *projectMenu = new QMenu(this);
    //    QAction *newAction = new QAction("New", this);
    //    QAction *openProjectAction = new QAction("Open", this);

    // Додайте дії до підменю Project
    //    projectMenu->addAction(newAction);
    //    projectMenu->addAction(openProjectAction);

    // Додайте підменю до Action "Project"
    //    projectAction->setMenu(projectMenu);
    //    fileMenu->addAction(projectAction);

    // Додайте інші дії до FileMenu
    QAction *saveAsImageAction = new QAction("Save as image", this);
    //    QAction *saveAllDataAction = new QAction("Save all data", this);
    //    QAction *importAction = new QAction("Import", this);
    QAction *exportWRLAction = new QAction("Export to wrl", this);
    QAction *exportCSVAction = new QAction("Export to csv", this);

    fileMenu->addAction(saveAsImageAction);
    //    fileMenu->addAction(saveAllDataAction);
    //    fileMenu->addAction(importAction);
    fileMenu->addAction(exportWRLAction);
    fileMenu->addAction(exportCSVAction);

    // Кастомізація Project Menu за допомогою CSS
    //    projectMenu->setStyleSheet("QMenu {"
    //                               "    width: 140px;"
    //                               "    height: 45px;"
    //                               "    background-color: #414141;" // фон меню
    //                               "    color: rgba(217, 217, 217, 0.70);" // колір тексту
    //                               "    spacing: 30px;"              // відступи між пунктами меню
    //                               "    border-radius: 15px;"
    //                               "}"
    //                               "QMenu::item {"
    //                               "    background-color: transparent;"
    //                               "    border-radius: 15px;"
    //                               "    color: #969696;"
    //                               "    font-family: Inter;"
    //                               "    font-size: 13px;"
    //                               "}"
    //                               "QMenu::item:selected {"
    //                               "    background-color: rgba(40, 40, 40, 0.24);"  // фон для вибраного елемента
    //                               "}"
    //                               "QMenu::down-arrow {"
    //                               "    width: 0; height: 0;"  // Зробити стрілку невидимою
    //                               "}"
    //                               "QMenu::indicator {"
    //                               "    width: 0; height: 0;"  // Зробити стрілку невидимою
    //                               "}");


    // Кастомізація FileMenu за допомогою CSS
    fileMenu->setStyleSheet("QMenu {"
                            "    width: 255px;"
                            "    height: 260px;"
                            "    background-color: #282828;" // фон меню
                            "    color: rgba(217, 217, 217, 0.70);" // колір тексту
                            "    margin: 0px;"
                            "    padding: 0px;"
                            "}"
                            "QMenu::item {"
                            "    width: 170px;"
                            "    height: 54px;"
                            "    background-color: transparent;"
                            "    padding: 8px 16px;"           // відступи для тексту
                            "    border: 1px solid #969696;"  // обводка для кожного пункту
                            "    border-radius: 15px;"
                            "    font-size: 20px;"
                            "    padding-top: 5px;"
                            "    padding-left: 30px;"
                            "    margin-top: 15px;"
                            "    margin-left: 20px;"
                            "}"
                            "QMenu::item:selected {"
                            "    background-color: rgba(217, 217, 217, 0.30);"  // фон для вибраного елемента
                            "}"
                            "QMenu::drop-down {"
                            "    width: 0; height: 0;"  // Зробити стрілку невидимою
                            "}"
                            "QMenu::indicator {"
                            "    width: 0; height: 0;"  // Зробити стрілку невидимою
                            "}");

    // Кастомізація QAction
    QFont actionFont;
    actionFont.setPointSize(14);  // розмір тексту
    //    newAction->setFont(actionFont);
    //    openProjectAction->setFont(actionFont);
    saveAsImageAction->setFont(actionFont);
    //    saveAllDataAction->setFont(actionFont);
    //    importAction->setFont(actionFont);
    exportWRLAction->setFont(actionFont);
    exportCSVAction->setFont(actionFont);

    // Призначте це меню кнопці
    ui->FileButton->setMenu(fileMenu);

    connect(saveAsImageAction, &QAction::triggered, this, &MainWindow::saveAsImage);
    connect(exportWRLAction, &QAction::triggered, this, &MainWindow::exportToWRL);
    connect(exportCSVAction, &QAction::triggered, this, &MainWindow::exportToCSV);
}

void MainWindow::setupWindowMenu() {
    // Створіть QMenu для WindowMenu
    QMenu *windowMenu = new QMenu(this);

    // Створіть чекбокси для WindowMenu
    QCheckBox *allCheckBox = new QCheckBox("All", this);
    //QCheckBox *statisticsCheckBox = new QCheckBox("Statistics", this);
    //QCheckBox *animationCheckBox = new QCheckBox("Animation", this);
    animationCheckBox = new QCheckBox("Animation", this);
    dataCheckBox = new QCheckBox("Data", this);
    consoleCheckBox = new QCheckBox("Console", this);

    // Створіть QWidgetAction для кожного чекбоксу
    QWidgetAction *allCheckBoxAction = new QWidgetAction(this);
    //QWidgetAction *statisticsCheckBoxAction = new QWidgetAction(this);
    QWidgetAction *animationCheckBoxAction = new QWidgetAction(this);
    QWidgetAction *consoleCheckBoxAction = new QWidgetAction(this);
    QWidgetAction *dataCheckBoxAction = new QWidgetAction(this);

    // Встановіть віджети для QWidgetAction
    allCheckBoxAction->setDefaultWidget(allCheckBox);
    //statisticsCheckBoxAction->setDefaultWidget(statisticsCheckBox);
    animationCheckBoxAction->setDefaultWidget(animationCheckBox);
    consoleCheckBoxAction->setDefaultWidget(consoleCheckBox);
    dataCheckBoxAction->setDefaultWidget(dataCheckBox);

    // Додайте QWidgetAction до WindowMenu
    windowMenu->addAction(allCheckBoxAction);
    //windowMenu->addAction(statisticsCheckBoxAction);
    windowMenu->addAction(animationCheckBoxAction);
    windowMenu->addAction(consoleCheckBoxAction);
    windowMenu->addAction(dataCheckBoxAction);

    // Встановіть стани чекбоксів за замовчуванням
    dataCheckBox->setChecked(true);
    consoleCheckBox->setChecked(true);
    animationCheckBox->setChecked(true);
    allCheckBox->setChecked(true);

    // Кастомізація WindowMenu за допомогою CSS (можете вказати власні стилі)
    windowMenu->setStyleSheet("QMenu {"
                              "    width: 195px;"
                              "    height: 200px;"
                              "    background-color: #282828;" // фон меню
                              "    color: rgba(217, 217, 217, 0.70);" // колір тексту
                              "}"
                              "QCheckBox {"
                              "    padding: 8px 16px;"           // відступи для тексту
                              "    font-size: 14px;"
                              "    font-weight: 500;"
                              "    color: #CFCECE;"
                              "    background-color: transparent;"
                              "    margin-left: 20px;"
                              "}"
                              "QCheckBox::indicator {"
                              "    width: 30px;"
                              "    height: 30px;"
                              "    background-color: transparent;"
                              "}"
                              "QCheckBox::indicator:unchecked {"
                              "    image: url(:/icon/checkOff.png);"  // зображення для невибраного чекбоксу
                              "}"
                              "QCheckBox::indicator:checked {"
                              "    image: url(:/icon/check.png);"  // зображення для вибраного чекбоксу
                              "}");

    // Призначте це меню кнопці
    ui->WindowButton->setMenu(windowMenu);

    // Приєднайте слоти до зміни стану чекбоксів
    connect(allCheckBox, &QCheckBox::stateChanged, this, &MainWindow::onAllCheckBoxChanged);
    //connect(statisticsCheckBox, &QCheckBox::stateChanged, this, &MainWindow::onStatisticsCheckBoxChanged);
    connect(animationCheckBox, &QCheckBox::stateChanged, this, &MainWindow::onAnimationCheckBoxChanged);
    connect(consoleCheckBox, &QCheckBox::stateChanged, this, &MainWindow::onConsoleCheckBoxChanged);
    connect(dataCheckBox, &QCheckBox::stateChanged, this, &MainWindow::onDataCheckBoxChanged);
}

void MainWindow::saveAsImage() {
    QRect rect = ui->Rectangle1->geometry();
    int width = rect.width();
    int height = rect.height();

    ui->backgrAnim->hide();
    ui->ConsoleWidget->hide();
    ui->DataWidget->hide();

    QPixmap pixmap(width, height);
    ui->myGLWidget->render(&pixmap);

    QString fileName = QFileDialog::getSaveFileName(this, "Save Image", "", "Image Files (*.png);;All Files (*.*)");
    if (!fileName.isEmpty()) {
        pixmap.save(fileName);
    }

    ui->backgrAnim->show();
    ui->ConsoleWidget->show();
    ui->DataWidget->show();
}

void MainWindow::exportToWRL(){
    if(startButtonPressed == false)
    {
        QMessageBox::information(nullptr, "Warning!", "The structure was not generated.");
    }
    else
    {
        //MyGLWidget voxelCube;
        QString fileName = QFileDialog::getSaveFileName(this, tr("Save VRML File"), QDir::homePath(), tr("VRML Files (*.wrl);;All Files (*)"));
        if (!fileName.isEmpty()) {
            //pixmap.save(fileName);
            MyGLWidget *voxelCube = ui->myGLWidget;
            std::vector<std::array<GLubyte, 4>> colors = voxelCube->generateDistinctColors();
            voxelCube->exportVRML(fileName, colors);
        };
    }
}

void MainWindow::onAllCheckBoxChanged(int state) {
    // Обробка зміни стану чекбоксу All
    if (state == Qt::Checked) {
        dataCheckBox->setChecked(true);
        consoleCheckBox->setChecked(true);
        animationCheckBox->setChecked(true);
    } else {
        dataCheckBox->setChecked(false);
        consoleCheckBox->setChecked(false);
        animationCheckBox->setChecked(false);
    }
}

void MainWindow::onDataCheckBoxChanged(int state) {
    if (state == Qt::Checked) {
        ui->DataWidget->show();
    } else {
        ui->DataWidget->hide();
    }
}

void MainWindow::onConsoleCheckBoxChanged(int state) {
    if (state == Qt::Checked) {
        ui->ConsoleWidget->show();
    } else {
        ui->ConsoleWidget->hide();
    }
}

void MainWindow::onAnimationCheckBoxChanged(int state) {
    // Обробка зміни стану чекбоксу All
    if (state == Qt::Checked) {
        ui->frameAnimation->show();
    } else {
        ui->frameAnimation->hide();
    }
}



void MainWindow::on_checkBoxAnimation_stateChanged(int arg1)
{
    if(arg1 == Qt::Checked) {
        isAnimation = 1;
        isClosedCube = 1;
        //qDebug() << "Checkbox is checked";
    } else {
        isAnimation = 0;
        isClosedCube = 0;
        //qDebug() << "Checkbox is unchecked";
    }
}


void MainWindow::on_AboutButton_clicked()
{
    about = new About;
    about->show();
}



void MainWindow::on_SliderAnimationSpeed_valueChanged(int value)
{
    double minValueSlider = 1.0;
    double maxValueSlider = 100.0;
    double minValueConverted = 1.0;
    double maxValueConverted = 0.01;

    // Конвертація значення слайдера
    double delayAnimation = ((maxValueConverted - minValueConverted) * (value - minValueSlider) / (maxValueSlider - minValueSlider)) + minValueConverted;

    ui->myGLWidget->setDelayAnimation(delayAnimation);
}

// Cube csv export button
void MainWindow::exportToCSV(){

}
