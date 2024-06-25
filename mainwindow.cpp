#include <iostream>
#include <chrono>
#include <Windows.h>
#include "parameters.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "myglwidget.h"
#include <QtWidgets>
#include <QMessageBox>
#include <QPushButton>
#include "radial.h"
#include "neumann.h"
#include "moore.h"
#include "dlca.h"
#include "probability_ellipse.h"
#include "probability_circle.h"
#include "parent_algorithm.h"
#include <QFileDialog>
#include <QDebug>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QSequentialAnimationGroup>
#include <ctime>
#include <qgifimage.h>
#include "statistics.h"
#include "export.h"
#include <hdf5.h>



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
    connect(this, &MainWindow::on_Start_clicked, this, &MainWindow::on_Start_clicked);


    connect(ui->Rectangle8, &QLineEdit::editingFinished, this, [=]() {
        bool ok;
        Parameters::size = ui->Rectangle8->text().toInt(&ok);
        if (ok) {
            ui->myGLWidget->setNumCubes(Parameters::size);
        }
    });

    connect(ui->Rectangle9, &QLineEdit::editingFinished, this, [=]() {
        bool ok;
        Parameters::points = ui->Rectangle9->text().toInt(&ok);
        if (ok) {
            ui->myGLWidget->setNumColors(Parameters::points);
        }
    });

    ui->Rectangle10->setMinimum(0);
    ui->Rectangle10->setMaximum(20);
    ui->Rectangle10->setSingleStep(1);
    ui->Rectangle10->setTickInterval(5);
    connect(ui->Rectangle10, &QSlider::valueChanged, ui->myGLWidget, &MyGLWidget::setDistanceFactor);

    messageHandlerInstance = new MessageHandler(ui->textEdit);
    connect(messageHandlerInstance, &MessageHandler::messageWrittenSignal, this, &MainWindow::onLogMessageWritten);

    startButtonPressed = false;
}

MainWindow::~MainWindow()
{
   // saveHDF();
    delete ui;
}

void MainWindow::onLogMessageWritten(const QString &message)
{
    ui->textEdit->append(message); // Вивід повідомлень в textEdit
}

// Функція перевірки для старт
void MainWindow::checkStart()
{
    bool checked = ui->AlgorithmsBox->currentIndex() != -1; // Перевірте, чи обраний елемент

    if (checked) {
        ui->Start->setEnabled(true);
        ui->Start->setStyleSheet("background: #282828; border-radius: 8px; width: 280px; height: 93px; color: #CFCECE; font-family: Inter; font-size: 20px; font-style: normal; font-weight: 700; line-height: normal; text-transform: uppercase;");
    } else if(!checked) {
        ui->Start->setEnabled(false);
        ui->Start->setStyleSheet("background: #282828; border-radius: 8px; width: 280px; height: 93px; color: rgba(150, 150, 150, 0.5); font-family: 'Inter'; font-style: normal; font-weight: 700; font-size: 48px; line-height: 58px;");
    }
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
    clock_t start_time = clock(); // Фіксація часу початку виконання
    QString selectedAlgorithm = ui->AlgorithmsBox->currentText();
    this->on_SliderAnimationSpeed_valueChanged(ui->SliderAnimationSpeed->value());
    if(std::isdigit(Parameters::size) == 0 && Parameters::size <= 0)
    {
        QMessageBox::warning(nullptr,"Warning!","Entered cube size is less than or equal to zero!");
    }
    else if(std::isdigit(Parameters::points) == 0 && Parameters::points <= 0)
    {
        QMessageBox::warning(nullptr, "Warning!", "Invalid initial points value entered!\nThis will result in incorrect program operation!");
    }
    ui->Start->setText("Loading...");
    ui->Start->setStyleSheet("background: transparent; color: #CFCECE; font-family: Inter; font-size: 20px; font-style: normal; font-weight: 700; line-height: normal; text-transform: uppercase;");
    QApplication::processEvents();
    if (selectedAlgorithm == "Neumann")
    {
        Neumann start(Parameters::size, Parameters::points);
        Parameters::voxels = start.Generate_Initial_Cube();
        start.Generate_Random_Starting_Points(isWaveGeneration);
        start.remainingPoints = start.numColors - static_cast<int>(0.1 * start.numColors);
        if (isAnimation == 0)
        {
            start.Generate_Filling(isAnimation, isWaveGeneration);
            ui->myGLWidget->setVoxels(start.voxels,start.numCubes);
            ui->myGLWidget->update();
        }
        else
        {
            while (!start.grains.empty())
            {
                start.Generate_Filling(isAnimation, isWaveGeneration);
                QApplication::processEvents();
                ui->myGLWidget->updateGLWidget(start.voxels,start.numCubes);
            }
        }
        qDebug() << "NEUMANN";
    }
    else if (selectedAlgorithm == "Probability Circle")
    {
            Probability_Circle start(Parameters::size, Parameters::points);
            Parameters::voxels = start.Generate_Initial_Cube();
            start.Generate_Random_Starting_Points(isWaveGeneration);
            start.remainingPoints = start.numColors - static_cast<int>(0.1 * start.numColors);
            if (isAnimation == 0)
            {
                start.Generate_Filling(isAnimation, isWaveGeneration);
                ui->myGLWidget->setVoxels(start.voxels,start.numCubes);
                ui->myGLWidget->update();
            }
            else
            {
                while (!start.grains.empty())
                {
                    start.Generate_Filling(isAnimation, isWaveGeneration);
                    QApplication::processEvents();
                    ui->myGLWidget->updateGLWidget(start.voxels,start.numCubes);
                }
            }
            qDebug() << "PROBABILITY CIRCLE";
    }
    else if (selectedAlgorithm == "Probability Ellipse")
    {
            Probability_Ellipse start(Parameters::size, Parameters::points);
            Parameters::voxels = start.Generate_Initial_Cube();
            start.Generate_Random_Starting_Points(isWaveGeneration);
            start.remainingPoints = start.numColors - static_cast<int>(0.1 * start.numColors);
            if (isAnimation == 0)
            {
                start.Generate_Filling(isAnimation, isWaveGeneration);
                ui->myGLWidget->setVoxels(start.voxels,start.numCubes);
                ui->myGLWidget->update();
            }
            else
            {
                while (!start.grains.empty())
                {
                    start.Generate_Filling(isAnimation, isWaveGeneration);
                    QApplication::processEvents();
                    ui->myGLWidget->updateGLWidget(start.voxels,start.numCubes);
                }
            }
            qDebug() << "PROBABILITY ELLIPSE";
    }
    else if (selectedAlgorithm == "Moore")
    {
            Moore start(Parameters::size, Parameters::points);
            Parameters::voxels = start.Generate_Initial_Cube();
            start.Generate_Random_Starting_Points(isWaveGeneration);
            start.remainingPoints = start.numColors - static_cast<int>(0.1 * start.numColors);
            if (isAnimation == 0)
            {
                start.Generate_Filling(isAnimation, isWaveGeneration);
                ui->myGLWidget->setVoxels(start.voxels,start.numCubes);
                ui->myGLWidget->update();
            }
            else
            {
                while (!start.grains.empty())
                {
                    start.Generate_Filling(isAnimation, isWaveGeneration);
                    QApplication::processEvents();
                    ui->myGLWidget->updateGLWidget(start.voxels,start.numCubes);
                }
            }
            qDebug() << "MOORE";
    }
    else if(selectedAlgorithm == "Radial")
    {
            Radial start(Parameters::size, Parameters::points);
            Parameters::voxels = start.Generate_Initial_Cube();
            start.Generate_Random_Starting_Points(isWaveGeneration);
            start.remainingPoints = start.numColors - static_cast<int>(0.1 * start.numColors);
            if (isAnimation == 0)
            {
                start.Generate_Filling(isAnimation, isWaveGeneration);
                ui->myGLWidget->setVoxels(start.voxels,start.numCubes);
                ui->myGLWidget->update();
            }
            else
            {
                while (!start.grains.empty())
                {
                    start.Generate_Filling(isAnimation, isWaveGeneration);
                    QApplication::processEvents();
                    ui->myGLWidget->updateGLWidget(start.voxels,start.numCubes);
                }
            }
            qDebug() << "RADIAL";
    }
    if(selectedAlgorithm == "DLCA")
    {
            DLCA start(Parameters::size, Parameters::points);
            Parameters::voxels = start.Generate_Initial_Cube();
            start.Generate_Random_Starting_Points();
            if (isAnimation == 0)
            {
                start.Generate_Filling(isAnimation, isWaveGeneration);
                ui->myGLWidget->setVoxels(start.voxels,start.numCubes);
                ui->myGLWidget->update();
            }
            else
            {
                while (!start.grains.empty())
                {
                    start.Generate_Filling(isAnimation, isWaveGeneration);
                    QApplication::processEvents();
                    ui->myGLWidget->updateGLWidget(start.voxels,start.numCubes);
                }
            }
            qDebug() << "DLCA";
    }
    ui->Start->setText("RELOAD");
    ui->Start->setStyleSheet("background: #282828; border-radius: 8px; color: #CFCECE; font-family: Inter; font-size: 20px; font-style: normal; font-weight: 700; line-height: normal; text-transform: uppercase;");
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

    // Додайте інші дії до FileMenu
    QAction *openHDF = new QAction("Open h5 file" , this);
    QAction *saveAsImageAction = new QAction("Save as image", this);
    QAction *exportWRLAction = new QAction("Export to wrl", this);
    QAction *exportCSVAction = new QAction("Export to csv", this);
    QAction *saveAsHDF = new QAction("Save as hdf5 file" , this);

    fileMenu->addAction(openHDF);
    fileMenu->addAction(saveAsImageAction);
    fileMenu->addAction(exportWRLAction);
    fileMenu->addAction(exportCSVAction);
    fileMenu->addAction(saveAsHDF);


    // Кастомізація FileMenu за допомогою CSS
    fileMenu->setStyleSheet("QMenu {"
                            "    width: 255px;"
                            "    height: 430px;"
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
                            "}"

                    );

    // Кастомізація QAction
    QFont actionFont;
    actionFont.setPointSize(14);  // розмір тексту
    openHDF->setFont(actionFont);
    saveAsImageAction->setFont(actionFont);
    exportWRLAction->setFont(actionFont);
    exportCSVAction->setFont(actionFont);
    saveAsHDF->setFont(actionFont);

    // Призначте це меню кнопці
    ui->FileButton->setMenu(fileMenu);
    connect(openHDF , &QAction::triggered , this , &MainWindow::openHDF);
    connect(saveAsImageAction, &QAction::triggered, this, &MainWindow::saveAsImage);
    connect(exportWRLAction, &QAction::triggered, this, &MainWindow::exportToWRL);
    connect(exportCSVAction, &QAction::triggered, this, &MainWindow::exportToCSV);
    connect(saveAsHDF , &QAction::triggered , this , &MainWindow::saveHDF);
}

void MainWindow::setupWindowMenu() {
    // Створіть QMenu для WindowMenu
    QMenu *windowMenu = new QMenu(this);

    // Створіть чекбокси для WindowMenu
    QCheckBox *allCheckBox = new QCheckBox("All", this);
    animationCheckBox = new QCheckBox("Animation", this);
    dataCheckBox = new QCheckBox("Data", this);
    consoleCheckBox = new QCheckBox("Console", this);

    // Створіть QWidgetAction для кожного чекбоксу
    QWidgetAction *allCheckBoxAction = new QWidgetAction(this);
    QWidgetAction *animationCheckBoxAction = new QWidgetAction(this);
    QWidgetAction *consoleCheckBoxAction = new QWidgetAction(this);
    QWidgetAction *dataCheckBoxAction = new QWidgetAction(this);

    // Встановіть віджети для QWidgetAction
    allCheckBoxAction->setDefaultWidget(allCheckBox);
    animationCheckBoxAction->setDefaultWidget(animationCheckBox);
    consoleCheckBoxAction->setDefaultWidget(consoleCheckBox);
    dataCheckBoxAction->setDefaultWidget(dataCheckBox);

    // Додайте QWidgetAction до WindowMenu
    windowMenu->addAction(allCheckBoxAction);
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
        MyGLWidget *voxelCube = ui->myGLWidget;
        std::vector<std::array<GLubyte, 4>> colors = voxelCube->generateDistinctColors();
        Export::ExportToVRML(colors,Parameters::size,Parameters::voxels);
    }
}

// Cube csv export button
void MainWindow::exportToCSV(){
    if(startButtonPressed == false)
    {
        QMessageBox::information(nullptr, "Warning!", "The structure was not generated.");
    }
    else
    {
        Export::ExportToCSV(Parameters::size, Parameters::voxels);
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

    ui->myGLWidget->setDelayAnimation(delayAnimation*1000);
}

void MainWindow::setNumCubes(short int arg)
{
    QString str = QString::number(arg);
    ui->Rectangle8->setText(str);
    emit ui->Rectangle8->editingFinished();
}

void MainWindow::setNumColors(int arg)
{
    QString str = QString::number(arg);
    ui->Rectangle9->setText(str);
    emit ui->Rectangle9->editingFinished();
}

void MainWindow::setAlgorithms(QString arg)
{
    ui->AlgorithmsBox->setCurrentText(arg);
}

void MainWindow::onStartClicked()
{
    emit on_Start_clicked();
}

void MainWindow::callExportToCSV()
{
    emit exportToCSV();
}

void MainWindow::saveHDF()
{
    if(Parameters::voxels)
    {
        hid_t file_id, dataset_id, dataspace_id; /* identifiers */

        hsize_t size = static_cast<hsize_t>(Parameters::size);

        hsize_t dims[3] = {size, size, size};

        herr_t status;

        const short& vox_size = Parameters::size;

        /* Create a new file using default properties. */
        file_id = H5Fcreate("testfile.h5", H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
        if (file_id < 0) {
            std::cerr << "Error creating file" << std::endl;
            return;
        }

        /* Create the data space for the dataset. */
        dataspace_id = H5Screate_simple(3, dims, NULL);
        if (dataspace_id < 0) {
            std::cerr << "Error creating dataspace" << std::endl;
            H5Fclose(file_id);
            return;
        }

        /* Create the dataset. */
        dataset_id = H5Dcreate2(file_id, "/voxels", H5T_STD_I16LE, dataspace_id,
                                H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        if (dataset_id < 0) {
            std::cerr << "Error creating dataset" << std::endl;
            H5Sclose(dataspace_id);
            H5Fclose(file_id);
            return;
        }

       // int16_t* data = new int16_t[vox_size * vox_size * vox_size];
        std::unique_ptr<int16_t[], std::default_delete<int16_t[]>> data(new int16_t[vox_size * vox_size * vox_size]);
        for (int i = 0; i < vox_size; i++) {
            for (int j = 0; j < vox_size; j++) {
                for (int k = 0; k < vox_size; k++) {
                    data[i * vox_size * vox_size + j * vox_size + k] = Parameters::voxels[i][j][k];
                }
            }
        }

        status = H5Dwrite(dataset_id, H5T_NATIVE_INT16, H5S_ALL, H5S_ALL, H5P_DEFAULT, data.get());
        if (status < 0) {
            std::cerr << "Error writing dataset" << std::endl;
            H5Dclose(dataset_id);
            H5Sclose(dataspace_id);
            H5Fclose(file_id);
            return;
        }

        /* End access to the dataset and release resources used by it. */
        status = H5Dclose(dataset_id);
        if (status < 0) {
            std::cerr << "Error closing dataset" << std::endl;
            H5Sclose(dataspace_id);
            H5Fclose(file_id);
            return;
        }

        /* Terminate access to the data space. */
        status = H5Sclose(dataspace_id);
        if (status < 0) {
            std::cerr << "Error closing dataspace" << std::endl;
            H5Fclose(file_id);
            return;
        }



        hsize_t Size_dims[1] = { 1 };
        hid_t Size_dataset_id;
        hid_t  Size_dataspace_id = H5Screate_simple(1, Size_dims, NULL);
        if (Size_dataspace_id < 0) {
            std::cerr << "Error creating new dataspace" << std::endl;
            H5Fclose(file_id);
            return;
        }

        Size_dataset_id = H5Dcreate2(file_id, "/Size", H5T_NATIVE_SHORT, Size_dataspace_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);


        if (Size_dataset_id < 0) {
            std::cerr << "Error creating new dataset" << std::endl;
            H5Sclose(Size_dataspace_id);
            H5Fclose(file_id);
            return;
        }

         status = H5Dwrite(Size_dataset_id, H5T_NATIVE_SHORT, H5S_ALL, H5S_ALL, H5P_DEFAULT, &Parameters::size);
        if (status < 0) {
            std::cerr << "Error writing new dataset" << std::endl;
            H5Dclose(Size_dataset_id);
            H5Sclose(Size_dataspace_id);
            H5Fclose(file_id);
            return;
        }

        H5Dclose(Size_dataset_id);
        H5Sclose(Size_dataspace_id);


        hsize_t Points_dims[1] = { 1 };
        hid_t Points_dataset_id;
        hid_t  Points_dataspace_id = H5Screate_simple(1, Points_dims, NULL);
        if (Points_dataspace_id < 0) {
            std::cerr << "Error creating new dataspace" << std::endl;
            H5Fclose(file_id);
            return;
        }

        Points_dataset_id = H5Dcreate2(file_id, "/Points", H5T_NATIVE_INT, Points_dataspace_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        if (Points_dataset_id < 0) {
            std::cerr << "Error creating new dataset" << std::endl;
            H5Sclose(Points_dataspace_id);
            H5Fclose(file_id);
            return;
        }

        status = H5Dwrite(Points_dataset_id, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, &Parameters::points);
        if (status < 0) {
            std::cerr << "Error writing new dataset" << std::endl;
            H5Dclose(Points_dataset_id);
            H5Sclose(Points_dataspace_id);
            H5Fclose(file_id);
            return;
        }

        H5Dclose(Points_dataset_id);
        H5Sclose(Points_dataspace_id);




        std::string stdStr =  ui->AlgorithmsBox->currentText().toStdString();


        hsize_t str_dims[1] = { 1 };
        hid_t str_dataspace_id = H5Screate_simple(1, str_dims, NULL);
        if (str_dataspace_id < 0) {
            std::cerr << "Error creating new dataspace" << std::endl;
            H5Fclose(file_id);
            return;
        }

        hid_t strtype = H5Tcopy(H5T_C_S1);
        H5Tset_size(strtype, stdStr.size() + 1); // +1 для null-терминатора
        hid_t str_dataset_id = H5Dcreate2(file_id, "/Algo", strtype, str_dataspace_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        if (str_dataset_id < 0) {
            std::cerr << "Error creating new dataset" << std::endl;
            H5Sclose(str_dataspace_id);
            H5Fclose(file_id);
            return;
        }

       status = H5Dwrite(str_dataset_id, strtype, H5S_ALL, H5S_ALL, H5P_DEFAULT, stdStr.c_str());
        if (status < 0) {
            std::cerr << "Error writing new dataset" << std::endl;
            H5Dclose(str_dataset_id);
            H5Sclose(str_dataspace_id);
            H5Fclose(file_id);
            return;
        }

        H5Dclose(str_dataset_id);
        H5Sclose(str_dataspace_id);
        H5Tclose(strtype);





        /* Close the file. */
        status = H5Fclose(file_id);
        if (status < 0) {
            std::cerr << "Error closing file" << std::endl;
            return;
        }

    }

}


void MainWindow::openHDF()
{
    QString path;
    path = QFileDialog::getOpenFileName(this , "Choose file" , "C:/" ,"H5file (*.h5)");

    m_h5handler.setPath(path);
    m_h5handler.getAll(Parameters::voxels , Parameters::size , Parameters::points , Parameters::algorithm);



    QString selectedAlgorithm = Parameters::algorithm;
    clock_t start_time = clock(); // Фіксація часу початку виконання
    this->on_SliderAnimationSpeed_valueChanged(ui->SliderAnimationSpeed->value());
    if(std::isdigit(Parameters::size) == 0 && Parameters::size <= 0)
    {
        QMessageBox::warning(nullptr,"Warning!","Entered cube size is less than or equal to zero!");
    }
    else if(std::isdigit(Parameters::points) == 0 && Parameters::points <= 0)
    {
        QMessageBox::warning(nullptr, "Warning!", "Invalid initial points value entered!\nThis will result in incorrect program operation!");
    }
    ui->Start->setText("Loading...");
    ui->Start->setStyleSheet("background: transparent; color: #CFCECE; font-family: Inter; font-size: 20px; font-style: normal; font-weight: 700; line-height: normal; text-transform: uppercase;");
    QApplication::processEvents();
    if (selectedAlgorithm == "Neumann")
    {
        Neumann start(Parameters::size, Parameters::points);
        start.voxels = Parameters::voxels;
        start.Generate_Random_Starting_Points(isWaveGeneration);
        start.remainingPoints = start.numColors - static_cast<int>(0.1 * start.numColors);
        if (isAnimation == 0)
        {
            start.Generate_Filling(isAnimation, isWaveGeneration);
            ui->myGLWidget->setNumColors(Parameters::points);
            ui->myGLWidget->setVoxels(start.voxels,start.numCubes);
            ui->myGLWidget->update();
        }
        else
        {
            while (!start.grains.empty())
            {
                start.Generate_Filling(isAnimation, isWaveGeneration);
                QApplication::processEvents();
                ui->myGLWidget->updateGLWidget(start.voxels,start.numCubes);
            }
        }
        qDebug() << "NEUMANN";
    }
    else if (selectedAlgorithm == "Probability Circle")
    {
        Probability_Circle start(Parameters::size, Parameters::points);
        start.voxels = Parameters::voxels;
        start.Generate_Random_Starting_Points(isWaveGeneration);
        start.remainingPoints = start.numColors - static_cast<int>(0.1 * start.numColors);
        if (isAnimation == 0)
        {
            start.Generate_Filling(isAnimation, isWaveGeneration);
            ui->myGLWidget->setNumColors(Parameters::points);
            ui->myGLWidget->setVoxels(start.voxels,start.numCubes);
            ui->myGLWidget->update();
        }
        else
        {
            while (!start.grains.empty())
            {
                start.Generate_Filling(isAnimation, isWaveGeneration);
                QApplication::processEvents();
                ui->myGLWidget->updateGLWidget(start.voxels,start.numCubes);
            }
        }
        qDebug() << "PROBABILITY CIRCLE";
    }
    else if (selectedAlgorithm == "Probability Ellipse")
    {
        Probability_Ellipse start(Parameters::size, Parameters::points);
        start.voxels = Parameters::voxels;
        start.Generate_Random_Starting_Points(isWaveGeneration);
        start.remainingPoints = start.numColors - static_cast<int>(0.1 * start.numColors);
        if (isAnimation == 0)
        {
            start.Generate_Filling(isAnimation, isWaveGeneration);
            ui->myGLWidget->setNumColors(Parameters::points);
            ui->myGLWidget->setVoxels(start.voxels,start.numCubes);
            ui->myGLWidget->update();
        }
        else
        {
            while (!start.grains.empty())
            {
                start.Generate_Filling(isAnimation, isWaveGeneration);
                QApplication::processEvents();
                ui->myGLWidget->updateGLWidget(start.voxels,start.numCubes);
            }
        }
        qDebug() << "PROBABILITY ELLIPSE";
    }
    else if (selectedAlgorithm == "Moore")
    {
        Moore start(Parameters::size, Parameters::points);
        start.voxels = Parameters::voxels;
        start.Generate_Random_Starting_Points(isWaveGeneration);
        start.remainingPoints = start.numColors - static_cast<int>(0.1 * start.numColors);
        if (isAnimation == 0)
        {
            start.Generate_Filling(isAnimation, isWaveGeneration);
            ui->myGLWidget->setNumColors(Parameters::points);
            ui->myGLWidget->setVoxels(start.voxels,start.numCubes);
            ui->myGLWidget->update();
        }
        else
        {
            while (!start.grains.empty())
            {
                start.Generate_Filling(isAnimation, isWaveGeneration);
                QApplication::processEvents();
                ui->myGLWidget->updateGLWidget(start.voxels,start.numCubes);
            }
        }
        qDebug() << "MOORE";
    }
    else if(selectedAlgorithm == "Radial")
    {
        Radial start(Parameters::size, Parameters::points);
        start.voxels = Parameters::voxels;
        start.Generate_Random_Starting_Points(isWaveGeneration);
        start.remainingPoints = start.numColors - static_cast<int>(0.1 * start.numColors);
        if (isAnimation == 0)
        {
            start.Generate_Filling(isAnimation, isWaveGeneration);
            ui->myGLWidget->setNumColors(Parameters::points);
            ui->myGLWidget->setVoxels(start.voxels,start.numCubes);
            ui->myGLWidget->update();
        }
        else
        {
            while (!start.grains.empty())
            {
                start.Generate_Filling(isAnimation, isWaveGeneration);
                QApplication::processEvents();
                ui->myGLWidget->updateGLWidget(start.voxels,start.numCubes);
            }
        }
        qDebug() << "RADIAL";
    }
    if(selectedAlgorithm == "DLCA")
    {
        DLCA start(Parameters::size, Parameters::points);
        start.voxels = Parameters::voxels;
        start.Generate_Random_Starting_Points();
        if (isAnimation == 0)
        {
            start.Generate_Filling(isAnimation, isWaveGeneration);
            ui->myGLWidget->setNumColors(Parameters::points);
            ui->myGLWidget->setVoxels(start.voxels,start.numCubes);
            ui->myGLWidget->update();
        }
        else
        {
            while (!start.grains.empty())
            {
                start.Generate_Filling(isAnimation, isWaveGeneration);
                QApplication::processEvents();
                ui->myGLWidget->updateGLWidget(start.voxels,start.numCubes);
            }
        }
        qDebug() << "DLCA";
    }

    ui->Start->setText("RELOAD");
    ui->Start->setStyleSheet("background: #282828; border-radius: 8px; color: #CFCECE; font-family: Inter; font-size: 20px; font-style: normal; font-weight: 700; line-height: normal; text-transform: uppercase;");
    clock_t end_time = clock(); // Фіксація часу завершення виконання
    double elapsed_time = double(end_time - start_time) / CLOCKS_PER_SEC; // Обчислення часу виконання в секундах
    qDebug() << "Total execution time: " << elapsed_time << " seconds";
    ui->myGLWidget->calculateSurfaceArea();
    startButtonPressed = true;
}

void MainWindow::on_checkBoxWiregrid_stateChanged(int arg1)
{
    ui->myGLWidget->setPlotWireFrame((bool)arg1);
    ui->myGLWidget->repaint();
}

