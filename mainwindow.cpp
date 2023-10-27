
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
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QSequentialAnimationGroup>

#include <qgifimage.h>
#include "statistics.h"

int16_t* createVoxelArray(int16_t*** voxels, int numCubes);
std::unordered_map<int16_t, int> countVoxels(int16_t* voxelArray, int numCubes, int numColors);

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Створіть QMenu та QAction
    QMenu *FileMenu = new QMenu(this);
    QAction *saveAction = new QAction("Save", this);
    QAction *openAction = new QAction("Open", this);


    // Додайте дії до меню
    FileMenu->addAction(saveAction);
    FileMenu->addAction(openAction);

    // Призначте це меню кнопці
    ui->FileButton->setMenu(FileMenu);


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
void MainWindow::checkStart(bool algorithm1, bool algorithm2, bool algorithm3, bool algorithm4)
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
            int16_t*** voxels = start.Generate_Initial_Cube(numCubes, numColors);
            bool answer = true;
            while (answer)
            {
                answer = start.Generate_Filling(voxels,numCubes);
                ui->myGLWidget->setVoxels(voxels, numCubes);
                ui->myGLWidget->repaint_function();
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
            int16_t*** voxels = start.Generate_Initial_Cube(numCubes, numColors);
            bool answer = true;
            while (answer)
            {
                answer = start.Generate_Filling(voxels,numCubes);
                ui->myGLWidget->setVoxels(voxels, numCubes);
                ui->myGLWidget->repaint_function();
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
            int16_t*** voxels = start.Generate_Initial_Cube(numCubes, numColors);
            bool answer = true;
            while (answer)
            {
                answer = start.Generate_Filling(voxels,numCubes);
                ui->myGLWidget->setVoxels(voxels, numCubes);
                ui->myGLWidget->repaint_function();
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
            int16_t*** voxels = start.Generate_Initial_Cube(numCubes, numColors);
            bool answer = true;
            while (answer)
            {
                answer = start.Generate_Filling(voxels,numCubes);
                ui->myGLWidget->setVoxels(voxels, numCubes);
                ui->myGLWidget->repaint_function();
            }
            ui->Start->setText("RELOAD");
            ui->Start->setStyleSheet("background: #282828; border-radius: 8px; color: #CFCECE; font-family: Inter; font-size: 20px; font-style: normal; font-weight: 700; line-height: normal; text-transform: uppercase;");
            qDebug() << "MOORE";

        }
    }
}


void MainWindow::on_imageSave_clicked()
{

    QRect rect = ui->Rectangle1->geometry();
    int width = rect.width();
    int height = rect.height();

    ui->Visibility->hide();
    ui->ConsoleWidget->hide();

    QPixmap pixmap(width, height);
    ui->myGLWidget->render(&pixmap);

    QString fileName = QFileDialog::getSaveFileName(this, "Save Image", "", "Image Files (*.png);;All Files (*.*)");
    if (!fileName.isEmpty()) {
        pixmap.save(fileName);
    }

    ui->Visibility->show();
    ui->ConsoleWidget->show();
}


void MainWindow::on_gifSave_clicked()
{
    QRect rect = ui->Rectangle1->geometry();
    int width = rect.width();
    int height = rect.height();

    ui->Visibility->hide();
    ui->ConsoleWidget->hide();

    QPixmap pixmap(width, height);
    ui->myGLWidget->render(&pixmap);

    QString fileName = QFileDialog::getSaveFileName(this, "Save GIF Image", "", "GIF Files (*.gif);;All Files (*.*)");
    if (!fileName.isEmpty()) {
        QImage gifImage = pixmap.toImage();
        gifImage.save(fileName);
    }

    ui->Visibility->show();
    ui->ConsoleWidget->show();
}


void MainWindow::on_statistics_clicked()
{
    QVector<int> colorCounts = ui->myGLWidget->countVoxelColors();
    form.setVoxelCounts(colorCounts);

    // Отримати вибраний алгоритм
//    QString algorithmName;
//    if (ui->Algorithm1->isChecked())
//    {
//        algorithmName = "Neumann";
//    }
//    else if (ui->Algorithm2->isChecked())
//    {
//        algorithmName = "Probability Circle";
//    }
//    else if (ui->Algorithm3->isChecked())
//    {
//        algorithmName = "Probability Ellipse";
//    }
//    else if (ui->Algorithm4->isChecked())
//    {
//        algorithmName = "Moore";
//    }

    // Встановити текст іконки вікна
//    form.setWindowTitle("Statistics - " + algorithmName);

    // Показати вікно
    form.show();
}
