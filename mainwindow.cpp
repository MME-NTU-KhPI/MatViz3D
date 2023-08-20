
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

    connect(ui->statistics, &QPushButton::clicked, this, &MainWindow::on_statistics_clicked);


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
    ui->Rectangle10->setMaximum(20);
    ui->Rectangle10->setSingleStep(1);
    ui->Rectangle10->setTickInterval(5);
    connect(ui->Rectangle10, &QSlider::valueChanged, ui->myGLWidget, &MyGLWidget::setDistanceFactor);

    checkInputFields();
    buttons[0] = ui->Algorithm1;
    buttons[1] = ui->Algorithm2;
    buttons[2] = ui->Algorithm3;
    buttons[3] = ui->Algorithm4;

    // Перша анімація - зникнення Rectangle4 і збільшення ширини інших віджетів на сітці
    connect(ui->menuVertical, &QPushButton::clicked, this, &MainWindow::on_menuVertical_clicked);
    connect(ui->ConsoleButton, &QPushButton::clicked, this, &MainWindow::on_ConsoleButton_clicked);

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
            QMessageBox::information(nullptr, "Warning!", "Invalid cube size value entered! This may lead to incorrect program operation.");
        }
        else if(std::isdigit(numColors) == 0 && numColors <= 0)
        {
            QMessageBox::information(nullptr, "Warning!", "Invalid initial points value entered! This will result in incorrect program operation!");
        }
        else
        {
            ui->Start->setText("Loading...");
            ui->Start->setStyleSheet("background: transparent; color: #969696; font-size: 48px; font-family: Inter; font-style: normal; font-weight: 700; line-height: normal;");
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
            ui->Start->setStyleSheet("background: #282828; border-radius: 8px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 48px; line-height: 58px; color: rgba(150, 150, 150, 0.5);");
            qDebug() << "NEUMANN";

        }

    }
    else if (ui->Algorithm2->isChecked())
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
            ui->Start->setStyleSheet("background: transparent; color: #969696; font-size: 48px; font-family: Inter; font-style: normal; font-weight: 700; line-height: normal;");
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
            ui->Start->setStyleSheet("background: #282828; border-radius: 8px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 48px; line-height: 58px; color: rgba(150, 150, 150, 0.5);");
            qDebug() << "PROBABILITY CIRCLE";

        }
    }
    else if (ui->Algorithm3->isChecked())
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
            ui->Start->setStyleSheet("background: transparent; color: #969696; font-size: 48px; font-family: Inter; font-style: normal; font-weight: 700; line-height: normal;");
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
            ui->Start->setStyleSheet("background: #282828; border-radius: 8px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 48px; line-height: 58px; color: rgba(150, 150, 150, 0.5);");
            qDebug() << "PROBABILITY ELLIPSE";

        }
    }
    else if (ui->Algorithm4->isChecked())
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
            ui->Start->setStyleSheet("background: transparent; color: #969696; font-size: 48px; font-family: Inter; font-style: normal; font-weight: 700; line-height: normal;");
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
            ui->Start->setStyleSheet("background: #282828; border-radius: 8px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 48px; line-height: 58px; color: rgba(150, 150, 150, 0.5);");
            qDebug() << "MOORE";

        }
    }
}


void MainWindow::on_imageSave_clicked()
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

    QString fileName = QFileDialog::getSaveFileName(this, "Save Image", "", "Image Files (*.png);;All Files (*.*)");
    if (!fileName.isEmpty()) {
        pixmap.save(fileName);
    }

    ui->Visibility->show();
    ui->wid_start->show();

}


void MainWindow::on_gifSave_clicked()
{

}


void MainWindow::on_statistics_clicked()
{
    QVector<int> colorCounts = ui->myGLWidget->countVoxelColors();
    form.setVoxelCounts(colorCounts);
    form.show();
}



void MainWindow::on_menuVertical_clicked()
{
    if (ui->Rectangle4->pos().x() == 0) {
        // Анімація зміни положення Rectangle4 з лівого боку на видиму позицію
        QPropertyAnimation* showAnimation = new QPropertyAnimation(ui->Rectangle4, "geometry", this);
        showAnimation->setStartValue(QRect(-ui->Rectangle4->width(), ui->Rectangle4->y(), ui->Rectangle4->width(), ui->Rectangle4->height()));
        showAnimation->setEndValue(QRect(0, ui->Rectangle4->y(), ui->Rectangle4->width(), ui->Rectangle4->height()));
        showAnimation->setDuration(500);

        // Анімація збільшення ширини інших віджетів на сітці
        QParallelAnimationGroup* expandAnimation = new QParallelAnimationGroup(this);

        QPropertyAnimation* expandAnimation1 = new QPropertyAnimation(ui->Rectangle1, "geometry", this);
        expandAnimation1->setStartValue(QRect(ui->Rectangle1->pos().x(), ui->Rectangle1->y(), ui->Rectangle1->width(), ui->Rectangle1->height()));
        expandAnimation1->setEndValue(QRect(ui->Rectangle1->pos().x() + ui->Rectangle4->width(), ui->Rectangle1->y(), ui->Rectangle1->width() - ui->Rectangle4->width(), ui->Rectangle1->height()));
        expandAnimation1->setDuration(500);

        QPropertyAnimation* expandAnimation2 = new QPropertyAnimation(ui->widget, "geometry", this);
        expandAnimation2->setStartValue(QRect(ui->widget->pos().x(), ui->widget->y(), ui->widget->width(), ui->widget->height()));
        expandAnimation2->setEndValue(QRect(ui->widget->pos().x() + ui->Rectangle4->width(), ui->widget->y(), ui->widget->width() - ui->Rectangle4->width(), ui->widget->height()));
        expandAnimation2->setDuration(500);

        QPropertyAnimation* expandAnimation3 = new QPropertyAnimation(ui->ConsoleWidget, "geometry", this);
        expandAnimation3->setStartValue(QRect(ui->ConsoleWidget->pos().x(), ui->ConsoleWidget->y(), ui->ConsoleWidget->width(), ui->ConsoleWidget->height()));
        expandAnimation3->setEndValue(QRect(ui->ConsoleWidget->pos().x() + ui->Rectangle4->width(), ui->ConsoleWidget->y(), ui->ConsoleWidget->width() - ui->Rectangle4->width(), ui->ConsoleWidget->height()));
        expandAnimation3->setDuration(500);

        expandAnimation->addAnimation(expandAnimation1);
        expandAnimation->addAnimation(expandAnimation2);
        expandAnimation->addAnimation(expandAnimation3);

        // Запускаємо обидві анімації одночасно
        QParallelAnimationGroup* animationGroup = new QParallelAnimationGroup(this);
        animationGroup->addAnimation(showAnimation);
        animationGroup->addAnimation(expandAnimation);

        // Запускаємо обидві анімації одночасно
        animationGroup->start(QAbstractAnimation::DeleteWhenStopped);

    } else {
        // Анімація зміни положення Rectangle4 з видимої позиції на лівий бік
        QPropertyAnimation* hideAnimation = new QPropertyAnimation(ui->Rectangle4, "geometry", this);
        hideAnimation->setStartValue(QRect(0, ui->Rectangle4->y(), ui->Rectangle4->width(), ui->Rectangle4->height()));
        hideAnimation->setEndValue(QRect(-ui->Rectangle4->width(), ui->Rectangle4->y(), ui->Rectangle4->width(), ui->Rectangle4->height()));
        hideAnimation->setDuration(500);

        // Анімація зменшення ширини інших віджетів на сітці
        QParallelAnimationGroup* shrinkAnimation = new QParallelAnimationGroup(this);

        QPropertyAnimation* shrinkAnimation1 = new QPropertyAnimation(ui->Rectangle1, "geometry", this);
        shrinkAnimation1->setStartValue(QRect(ui->Rectangle1->pos().x(), ui->Rectangle1->y(), ui->Rectangle1->width(), ui->Rectangle1->height()));
        shrinkAnimation1->setEndValue(QRect(ui->Rectangle1->pos().x() - ui->Rectangle4->width(), ui->Rectangle1->y(), ui->Rectangle1->width() + ui->Rectangle4->width(), ui->Rectangle1->height()));
        shrinkAnimation1->setDuration(500);

        QPropertyAnimation* shrinkAnimation2 = new QPropertyAnimation(ui->widget, "geometry", this);
        shrinkAnimation2->setStartValue(QRect(ui->widget->pos().x(), ui->widget->y(), ui->widget->width(), ui->widget->height()));
        shrinkAnimation2->setEndValue(QRect(ui->widget->pos().x() - ui->Rectangle4->width(), ui->widget->y(), ui->widget->width() + ui->Rectangle4->width(), ui->widget->height()));
        shrinkAnimation2->setDuration(500);

        QPropertyAnimation* shrinkAnimation3 = new QPropertyAnimation(ui->ConsoleWidget, "geometry", this);
        shrinkAnimation3->setStartValue(QRect(ui->ConsoleWidget->pos().x(), ui->ConsoleWidget->y(), ui->ConsoleWidget->width(), ui->ConsoleWidget->height()));
        shrinkAnimation3->setEndValue(QRect(ui->ConsoleWidget->pos().x() - ui->Rectangle4->width(), ui->ConsoleWidget->y(), ui->ConsoleWidget->width() + ui->Rectangle4->width(), ui->ConsoleWidget->height()));
        shrinkAnimation3->setDuration(500);

        shrinkAnimation->addAnimation(shrinkAnimation1);
        shrinkAnimation->addAnimation(shrinkAnimation2);
        shrinkAnimation->addAnimation(shrinkAnimation3);

        // Запускаємо обидві анімації одночасно
        QParallelAnimationGroup* animationGroup = new QParallelAnimationGroup(this);
        animationGroup->addAnimation(hideAnimation);
        animationGroup->addAnimation(shrinkAnimation);

        // Запускаємо обидві анімації одночасно
        animationGroup->start(QAbstractAnimation::DeleteWhenStopped);
    }
}



void MainWindow::on_ConsoleButton_clicked()
{
    if (ui->ConsoleWidget->pos().y() == height() - ui->ConsoleWidget->height()) {
        // Анімація зміни положення ConsoleWidget вниз
        QPropertyAnimation* hideAnimation = new QPropertyAnimation(ui->ConsoleWidget, "geometry", this);
        hideAnimation->setStartValue(ui->ConsoleWidget->geometry());
        hideAnimation->setEndValue(QRect(ui->ConsoleWidget->pos().x(), height(), ui->ConsoleWidget->width(), ui->ConsoleWidget->height()));
        hideAnimation->setDuration(500);

        // Анімація збільшення висоти Rectangle1 вниз
        QPropertyAnimation* expandAnimation = new QPropertyAnimation(ui->Rectangle1, "geometry", this);
        expandAnimation->setStartValue(ui->Rectangle1->geometry());
        expandAnimation->setEndValue(QRect(ui->Rectangle1->pos().x(), ui->Rectangle1->pos().y(), ui->Rectangle1->width(), ui->Rectangle1->height() + ui->ConsoleWidget->height()));
        expandAnimation->setDuration(500);

        // Запускаємо обидві анімації одночасно
        QParallelAnimationGroup* animationGroup = new QParallelAnimationGroup(this);
        animationGroup->addAnimation(hideAnimation);
        animationGroup->addAnimation(expandAnimation);

        // Запускаємо обидві анімації одночасно
        animationGroup->start(QAbstractAnimation::DeleteWhenStopped);

    } else {
        // Анімація зміни положення ConsoleWidget назад вниз
        QPropertyAnimation* showAnimation = new QPropertyAnimation(ui->ConsoleWidget, "geometry", this);
        showAnimation->setStartValue(ui->ConsoleWidget->geometry());
        showAnimation->setEndValue(QRect(ui->ConsoleWidget->pos().x(), height() - ui->ConsoleWidget->height(), ui->ConsoleWidget->width(), ui->ConsoleWidget->height()));
        showAnimation->setDuration(500);

        // Анімація зменшення висоти Rectangle1 назад
        QPropertyAnimation* shrinkAnimation = new QPropertyAnimation(ui->Rectangle1, "geometry", this);
        shrinkAnimation->setStartValue(ui->Rectangle1->geometry());
        shrinkAnimation->setEndValue(QRect(ui->Rectangle1->pos().x(), ui->Rectangle1->pos().y(), ui->Rectangle1->width(), ui->Rectangle1->height() - ui->ConsoleWidget->height()));
        shrinkAnimation->setDuration(500);

        // Запускаємо обидві анімації одночасно
        QParallelAnimationGroup* animationGroup = new QParallelAnimationGroup(this);
        animationGroup->addAnimation(showAnimation);
        animationGroup->addAnimation(shrinkAnimation);

        // Запускаємо обидві анімації одночасно
        animationGroup->start(QAbstractAnimation::DeleteWhenStopped);
    }
}



