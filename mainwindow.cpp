
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QGraphicsDropShadowEffect>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->Rectangle8, &QLineEdit::textChanged, this, &MainWindow::checkInputFields);
    connect(ui->Rectangle9, &QLineEdit::textChanged, this, &MainWindow::checkInputFields);
    checkInputFields();


    buttons[0] = ui->Algorithm1; // додати
    buttons[1] = ui->Algorithm2;
    buttons[2] = ui->Algorithm3;
    buttons[3] = ui->Algorithm4;
}

MainWindow::~MainWindow()
{
    delete ui;
}


//Функція перевірки для алгоритмів
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


//перенести все що всередині алгоритмів
void MainWindow::on_Algorithm1_clicked(bool checked)
{
    if(checked)
    {
        ui->Algorithm1->setStyleSheet("font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; background: #969696; color: #000000; border: 1px solid #969696; border-radius: 15px;");

        // перебор всех кнопок и установка состояния "unchecked" для всех, кроме текущей
        for(int i = 0; i < 4; i++)
        {
            if(buttons[i] != ui->Algorithm1)
            {
                buttons[i]->setChecked(false);
                buttons[i]->setStyleSheet("background: #282828; border: 1px solid #969696; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #969696;");
            }
        }
    }
    else
    {
        ui->Algorithm1->setStyleSheet("background: #282828; border: 1px solid #969696; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #969696;");
    }
    checkStart(ui->Algorithm1->isChecked(), ui->Algorithm2->isChecked(), ui->Algorithm3->isChecked(), ui->Algorithm4->isChecked());

}


void MainWindow::on_Algorithm2_clicked(bool checked)
{
    if(checked)
    {
        ui->Algorithm2->setStyleSheet("font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; background: #969696; color: #000000; border: 1px solid #969696; border-radius: 15px;");

        // перебор всех кнопок и установка состояния "unchecked" для всех, кроме текущей
        for(int i = 0; i < 4; i++)
        {
            if(buttons[i] != ui->Algorithm2)
            {
                buttons[i]->setChecked(false);
                buttons[i]->setStyleSheet("background: #282828; border: 1px solid #969696; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #969696;");
            }
        }
    }
    else
    {
        ui->Algorithm2->setStyleSheet("background: #282828; border: 1px solid #969696; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #969696;");
    }
    checkStart(ui->Algorithm1->isChecked(), ui->Algorithm2->isChecked(), ui->Algorithm3->isChecked(), ui->Algorithm4->isChecked());
}



void MainWindow::on_Algorithm3_clicked(bool checked)
{
    if(checked)
    {
        ui->Algorithm3->setStyleSheet("font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; background: #969696; color: #000000; border: 1px solid #969696; border-radius: 15px;");

        // перебор всех кнопок и установка состояния "unchecked" для всех, кроме текущей
        for(int i = 0; i < 4; i++)
        {
            if(buttons[i] != ui->Algorithm3)
            {
                buttons[i]->setChecked(false);
                buttons[i]->setStyleSheet("background: #282828; border: 1px solid #969696; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #969696;");
            }
        }
    }
    else
    {
        ui->Algorithm3->setStyleSheet("background: #282828; border: 1px solid #969696; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #969696;");
    }
    checkStart(ui->Algorithm1->isChecked(), ui->Algorithm2->isChecked(), ui->Algorithm3->isChecked(), ui->Algorithm4->isChecked());
}


void MainWindow::on_Algorithm4_clicked(bool checked)
{
    if(checked)
    {
        ui->Algorithm4->setStyleSheet("font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; background: #969696; color: #000000; border: 1px solid #969696; border-radius: 15px;");

        // перебор всех кнопок и установка состояния "unchecked" для всех, кроме текущей
        for(int i = 0; i < 4; i++)
        {
            if(buttons[i] != ui->Algorithm4)
            {
                buttons[i]->setChecked(false);
                buttons[i]->setStyleSheet("background: #282828; border: 1px solid #969696; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #969696;");
            }
        }
    }
    else
    {
        ui->Algorithm4->setStyleSheet("background: #282828; border: 1px solid #969696; border-radius: 15px; font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 16px; line-height: 19px; color: #969696;");
    }
    checkStart(ui->Algorithm1->isChecked(), ui->Algorithm2->isChecked(), ui->Algorithm3->isChecked(), ui->Algorithm4->isChecked());
}



void MainWindow::on_Start_clicked()
{
    if (ui->Algorithm1->isChecked()) {
        QMessageBox::information(this, "Повідомлення", "Ви обрали кнопку 1");
    }
    else if (ui->Algorithm2->isChecked()) {
        QMessageBox::information(this, "Повідомлення", "Ви обрали кнопку 2");
    }
    else if (ui->Algorithm3->isChecked()) {
        QMessageBox::information(this, "Повідомлення", "Ви обрали кнопку 3");
    }
    else if (ui->Algorithm4->isChecked()) {
        QMessageBox::information(this, "Повідомлення", "Ви обрали кнопку 4");
    }
}

//кнопка для збереження зображення + функція для вилимості віджетів після збереження
void MainWindow::on_Save_clicked()
{
    // Получаем указатель на виджет Rectangle1
    QWidget *widget = findChild<QWidget*>("Rectangle1");

    // Определяем геометрию, которую нужно захватить
    QRect captureRect = widget->geometry();

    // Исключаем дочерние элементы виджета Rectangle1 из захватываемой области (убирает только один виджет, заключить остальные в пустые виджеты и переименовать в условии)
    QList<QWidget*> children = widget->findChildren<QWidget*>();
    foreach (QWidget* child, children) {
        if (child->isVisible() && child->objectName() != "Visibility" && child->objectName() != "wid_color" && child->objectName() != "Save" && child->objectName() != "wid_start") {
            captureRect = captureRect.united(child->geometry());
            child->setVisible(false); // делаем виджет невидимым
        }
    }


    // Получаем изображение виджета
    QPixmap widgetImage = widget->grab(captureRect);

    // Выбираем место для сохранения файла
    QString fileName = QFileDialog::getSaveFileName(nullptr, "Зберегти зображення", "", "Images (*.png *.xpm *.jpg)");

    // Если пользователь выбрал место для сохранения
    if (!fileName.isEmpty()) {
        // Сохраняем изображение
        widgetImage.save(fileName);
    }

    this->setWidgetVisibility(widget, true);
}

void MainWindow::setWidgetVisibility(QWidget* widget, bool visible)
{
    widget->setVisible(visible);
    QList<QWidget*> children = widget->findChildren<QWidget*>();
    foreach (QWidget* child, children) {
        if (child->objectName() != "Visibility" && child->objectName() != "wid_color" && child->objectName() != "Save" && child->objectName() != "wid_start") {
            setWidgetVisibility(child, visible);
        }
    }
}

