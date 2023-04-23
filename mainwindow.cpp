
#include "mainwindow.h"
#include "ui_mainwindow.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->Rectangle8, &QLineEdit::textChanged, this, &MainWindow::checkInputFields);
    connect(ui->Rectangle9, &QLineEdit::textChanged, this, &MainWindow::checkInputFields);
    checkInputFields();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::checkInputFields()
{
    if (ui->Rectangle8->text().isEmpty() || ui->Rectangle9->text().isEmpty()) {
        ui->Algoritm1->setEnabled(false);
        ui->Algoritm1->setStyleSheet("background: #282828; border: 1px solid #000000; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #171616;");

        ui->Algoritm2->setEnabled(false);
        ui->Algoritm2->setStyleSheet("background: #282828; border: 1px solid #000000; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #171616;");

        ui->Algoritm3->setEnabled(false);
        ui->Algoritm3->setStyleSheet("background: #282828; border: 1px solid #000000; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #171616;");

        ui->Algoritm4->setEnabled(false);
        ui->Algoritm4->setStyleSheet("background: #282828; border: 1px solid #000000; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #171616;");
    } else {
        ui->Algoritm1->setEnabled(true);
        ui->Algoritm1->setStyleSheet("background: #282828; border: 1px solid #969696; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #969696;");

        ui->Algoritm2->setEnabled(true);
        ui->Algoritm2->setStyleSheet("background: #282828; border: 1px solid #969696; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #969696;");

        ui->Algoritm3->setEnabled(true);
        ui->Algoritm3->setStyleSheet("background: #282828; border: 1px solid #969696; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #969696;");

        ui->Algoritm4->setEnabled(true);
        ui->Algoritm4->setStyleSheet("background: #282828; border: 1px solid #969696; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #969696;");
    }
}

void MainWindow::on_Algoritm1_clicked(bool checked)
{
    ui->Algoritm1->setStyleSheet("font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; background: #969696; color: #000000; border: 1px solid #969696; border-radius: 15px;");

}


void MainWindow::on_Algoritm2_clicked(bool checked)
{
    ui->Algoritm2->setStyleSheet("font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; background: #969696; color: #000000; border: 1px solid #969696; border-radius: 15px;");
}



void MainWindow::on_Algoritm3_clicked(bool checked)
{
    ui->Algoritm3->setStyleSheet("font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; background: #969696; color: #000000; border: 1px solid #969696; border-radius: 15px;");
}


void MainWindow::on_Algoritm4_clicked(bool checked)
{
    ui->Algoritm4->setStyleSheet("font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; background: #969696; color: #000000; border: 1px solid #969696; border-radius: 15px;");
}

