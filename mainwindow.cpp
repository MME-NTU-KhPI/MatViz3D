#include "loadstepmanager.h"
#include "parameters.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "myglwidget.h"
#include <QtWidgets>
#include <QMessageBox>
#include <QPushButton>
#include "probability_algorithm.h"
#include <QFileDialog>
#include <QDebug>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QSequentialAnimationGroup>
#include <QTimer>
#include <ctime>
#include <qgifimage.h>
#include "statistics.h"
#include "export.h"
#include "hdf5wrapper.h"
#include <hdf5.h>
#include "stressanalysis.h"


int32_t* createVoxelArray(int32_t*** voxels, int numCubes);
std::unordered_map<int32_t, int> countVoxels(int32_t* voxelArray, int numCubes, int numColors);

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setupFileMenu();
    setupWindowMenu();

    ui->backgrAnim_2->hide();

    connect(ui->numOfPointsRadioButton, &QRadioButton::clicked, this, &MainWindow::onInitialConditionSelectionChanged);
    connect(ui->concentrationRadioButton, &QRadioButton::clicked, this, &MainWindow::onInitialConditionSelectionChanged);
    connect(ui->AlgorithmsBox, &QComboBox::currentTextChanged, this, &MainWindow::onAlgorithmChanged);

    ui->myGLWidget->setIsometricView();
    connect(ui->Rectangle8, &QLineEdit::editingFinished, this, [=]() {
        bool ok;
        Parameters::size = ui->Rectangle8->text().toInt(&ok);
        if (ok) {
            ui->myGLWidget->setNumCubes(Parameters::size);
        }
    });

    connect(ui->Rectangle9, &QLineEdit::editingFinished, this, [=]() {
        bool ok;
        if (ui->numOfPointsRadioButton->isChecked() == true)
        {
            Parameters::points = ui->Rectangle9->text().toInt(&ok);
            if (ok) {
                ui->myGLWidget->setNumColors(Parameters::points);
            }
        }
        else if (ui->concentrationRadioButton->isChecked() == true)
        {
            double concentrationPercentage = ui->Rectangle9->text().toDouble(&ok);
            if (ok) {
                Parameters::points = static_cast<int>(concentrationPercentage * Parameters::size * Parameters::size * Parameters::size / 100.0);
                ui->myGLWidget->setNumColors(Parameters::points);
            }
            qDebug() << "Calculated Points:" << Parameters::points;
        }
        onInitialConditionSelectionChanged();
    });

    connect (ui->AlgorithmsBox, &QComboBox::currentIndexChanged, [this]() {
        if (ui->AlgorithmsBox->currentText() == "Composite") {
            ui->numOfPointsRadioButton->setText("Radius");
        }
        else {
            ui->numOfPointsRadioButton->setText("Number of points");
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
    gif = nullptr;

    scene = new LegendView(this);
    ui->LegendView->setScene(scene);
    scene->setMinMax(0,1000);
    ui->LegendView->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
    ui->LegendView->viewport()->update();
    ui->LegendView->show();

    connect(ui->geom_ID, &QComboBox::currentIndexChanged, this, &MainWindow::on_geom_ID_currentIndexChanged);
    connect(ui->geom_sub_ID, &QComboBox::currentIndexChanged, this, &MainWindow::on_geom_sub_ID_currentIndexChanged);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onLogMessageWritten(const QString &message)
{
    ui->textEdit->append(message);
}

void MainWindow::closeProbabilityWindow()
{
    if (probability_algorithm)
    {
        probability_algorithm->close();
    }
}

void MainWindow::onAlgorithmChanged(const QString &text)
{

    if (text == "DLCA")
    {
        ui->checkBoxAnimation->setChecked(true);
        ui->checkBoxAnimation->setEnabled(false);
    }
    else
    {
        ui->checkBoxAnimation->setChecked(false);
        ui->checkBoxAnimation->setEnabled(true);
    }
    if (text == "Probability Algorithm" && Parameters::nogui != true)
    {
        probability_algorithm = new Probability_Algorithm();
        probability_algorithm->show();
    }
    onInitialConditionSelectionChanged();

}

void MainWindow::onInitialConditionSelectionChanged()
{
    bool ok;
    if (ui->numOfPointsRadioButton->isChecked()) {
        ui->concentrationRadioButton->setChecked(false);
        Parameters::points = ui->Rectangle9->text().toInt(&ok);
        if (ok) {
            ui->myGLWidget->setNumColors(Parameters::points);
        }
        qDebug() << "Number of points is checked. Number of points = " << Parameters::points;


    } else if (ui->concentrationRadioButton->isChecked()) {
        double concentrationPercentage = ui->Rectangle9->text().toDouble(&ok);
        if (ok)
        {
            Parameters::points = static_cast<int>(concentrationPercentage * Parameters::size * Parameters::size * Parameters::size / 100.0);
            if (ui->AlgorithmsBox->currentText() == "Composite")
            {
                double radius = round(sqrt(concentrationPercentage * Parameters::size * Parameters::size / M_PI / 100.0)); // sqrt(psi * a ^2 / pi)

                Parameters::points = static_cast<int>(radius);
                qDebug() << "Fiber radius = " << Parameters::points;
            }
            ui->myGLWidget->setNumColors(Parameters::points);
        }
        ui->numOfPointsRadioButton->setChecked(false);
        qDebug() << "Concentration is checked. Number of points = " << Parameters::points;
    }
}

void MainWindow::checkStart()
{
    bool checked = ui->AlgorithmsBox->currentIndex() != -1;

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
    ui->backgrAnim_2->hide();
    clock_t start_time = clock();

    if (!validateParameters()) return;

    initializeUIForStart();
    registerAlgorithms();

    Parameters params;
    omp_set_num_threads(params.num_threads);
    auto algorithm = AlgorithmFactory::instance().createAlgorithm(ui->AlgorithmsBox->currentText(), params);
    if (!algorithm) {
        QMessageBox::warning(this, "Error", "Unknown algorithm selected.");
        return;
    }

    setAlgorithmFlags(*algorithm);

    if (isAnimation) startGifRecording();

    executeAlgorithm(*algorithm, ui->AlgorithmsBox->currentText());

    if (isAnimation) stopGifRecording();

    finalizeUIAfterCompletion();
    logExecutionTime(start_time);
    startButtonPressed = true;
}



void MainWindow::initializeUIForStart()
{
    ui->Start->setText("Loading...");
    ui->Start->setStyleSheet("background: transparent; color: #CFCECE; font-family: Inter; font-size: 20px; font-style: normal; font-weight: 700; line-height: normal; text-transform: uppercase;");
    QApplication::processEvents();
}

bool MainWindow::validateParameters()
{
    if (std::isdigit(Parameters::size) == 0 && Parameters::size <= 0) {
        QMessageBox::warning(nullptr, "Warning!", "Entered cube size is less than or equal to zero!");
        return false;
    } else if (std::isdigit(Parameters::points) == 0 && Parameters::points <= 0) {
        QMessageBox::warning(nullptr, "Warning!", "Invalid initial points value entered!\nThis will result in incorrect program operation!");
        return false;
    } else if (Parameters::points > std::pow(Parameters::size, 3)) {
        QMessageBox::warning(nullptr, "Warning!", "The entered value of the initial points exceeds the number of points in the cube! This may lead to incorrect operation of the programme. Please make sure that the number of starting points does not exceed the volume of the cube!");
        return false;
    }
    return true;
}

void MainWindow::executeAlgorithm(Parent_Algorithm& algorithm, const QString& algorithmName)
{
    Parameters::voxels = algorithm.Allocate_Memory();
    algorithm.Initialization(isWaveGeneration);
    algorithm.setRemainingPoints(algorithm.getNumColors() - static_cast<int>(Parameters::wave_coefficient * algorithm.getNumColors()));

    auto updateScene = [&]() {
        ui->myGLWidget->setVoxels(algorithm.getVoxels(), algorithm.getNumCubes());
        ui->myGLWidget->DelayFrameUpdate();
        ui->myGLWidget->update();
        QApplication::processEvents();
    };

    updateScene();

    auto start = std::chrono::high_resolution_clock::now();

    while (!algorithm.getDone()) {
        algorithm.Next_Iteration(updateScene);
    }

    auto end = std::chrono::high_resolution_clock::now();
    qDebug() << "Algorithm execution time: " << std::chrono::duration<double>(end - start).count() << " seconds";

    updateScene();
    algorithm.CleanUp();
    qDebug() << algorithmName;
}



void MainWindow::finalizeUIAfterCompletion()
{
    ui->Start->setText("RELOAD");
    ui->Start->setStyleSheet("background: #282828; border-radius: 8px; color: #CFCECE; font-family: Inter; font-size: 20px; font-style: normal; font-weight: 700; line-height: normal; text-transform: uppercase;");
}

void MainWindow::logExecutionTime(clock_t start_time)
{
    clock_t end_time = clock();
    double elapsed_time = double(end_time - start_time) / CLOCKS_PER_SEC;
    qDebug() << "Total execution time: " << elapsed_time << " seconds";
}
void MainWindow::on_statistics_clicked()
{
    clock_t start_time = clock();
    if(startButtonPressed == false)
    {
        QMessageBox::information(nullptr, "Warning!", "The structure was not generated.");
    }
    else{
        form.layersProcessing(Parameters::voxels, Parameters::size);
        form.setPropertyBoxText("-----");
        form.selectProperty();

	//form.setVoxelCounts(Parameters::voxels, Parameters::size);
        form.calcVolume3D(Parameters::voxels, Parameters::size);
        form.surfaceArea3D(Parameters::voxels, Parameters::size);
        form.calcESR();
        form.calcMomentInertia();
        form.calcNormVolume3D();

        QString selectedAlgorithm = ui->AlgorithmsBox->currentText();

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

        form.setWindowTitle("Statistics - " + algorithmName);

        form.show();
    }
    clock_t end_time = clock();
    double elapsed_time = double(end_time - start_time) / CLOCKS_PER_SEC;
    qDebug() << "Total execution time: " << elapsed_time << " seconds";
}


void MainWindow::setupFileMenu() {
    QMenu *fileMenu = new QMenu(this);

    QAction *openHDF = new QAction("Open MV3D hdf5" , this);
    QAction *saveAsImageAction = new QAction("Save as image", this);
    QAction *exportWRLAction = new QAction("Export to wrl", this);
    QAction *exportCSVAction = new QAction("Export to csv", this);
    QAction *saveAsHDF = new QAction("Save as hdf5 file" , this);
    QAction *estimateStressWithANSYS = new QAction("Estimate stresses", this);
    QAction *MakeScreenshot = new QAction("Make screenshot", this);

    fileMenu->addAction(saveAsImageAction);
    fileMenu->addAction(exportWRLAction);
    fileMenu->addAction(exportCSVAction);
    fileMenu->addAction(saveAsHDF);
    fileMenu->addAction(estimateStressWithANSYS);
    fileMenu->addAction(MakeScreenshot);
    fileMenu->addAction(openHDF);

    fileMenu->setStyleSheet("QMenu {"
                            "    background-color: #282828;" // фон меню
                            "    color: rgba(217, 217, 217, 0.70);" // колір тексту
                            "    margin: 0px;"
                            "    padding: 0px;"
                            "}"
                            "QMenu::item {"
                            "    width: 170px;"
                            "    height: 54px;"
                            "    background-color: transparent;"
                            "    padding: 8px 16px;"
                            "    border: 1px solid #969696;"
                            "    border-radius: 15px;"
                            "    font-size: 20px;"
                            "    padding-top: 5px;"
                            "    padding-left: 30px;"
                            "    margin-top: 7px;"
                            "    margin-left: 20px;"
                            "    margin-right: 20px;"
                            "    margin-bottom: 7px;"
                            "}"
                            "QMenu::item:selected {"
                            "    background-color: rgba(217, 217, 217, 0.30);"
                            "}"
                            "QMenu::drop-down {"
                            "    width: 0; height: 0;"
                            "}"
                            "QMenu::indicator {"
                            "    width: 0; height: 0;"
                            "}");

    QFont actionFont;
    actionFont.setPointSize(14);
    openHDF->setFont(actionFont);
    saveAsImageAction->setFont(actionFont);
    exportWRLAction->setFont(actionFont);
    exportCSVAction->setFont(actionFont);
    saveAsHDF->setFont(actionFont);
    estimateStressWithANSYS->setFont(actionFont);
    MakeScreenshot->setFont(actionFont);

    ui->FileButton->setMenu(fileMenu);
    connect(openHDF , &QAction::triggered , this , &MainWindow::openHDF);
    connect(saveAsImageAction, &QAction::triggered, this, &MainWindow::saveAsImage);
    connect(exportWRLAction, &QAction::triggered, this, &MainWindow::exportToWRL);
    connect(exportCSVAction, &QAction::triggered, this, &MainWindow::exportToCSV);
    connect(saveAsHDF , &QAction::triggered , this , &MainWindow::saveHDF);
    connect(estimateStressWithANSYS, &QAction::triggered, this, &MainWindow::estimateStressWithANSYS);
    connect(MakeScreenshot,  &QAction::triggered, this, &MainWindow::copyScreenshotToClipboard);

}

void MainWindow::setupWindowMenu() {
    QMenu *windowMenu = new QMenu(this);

    QCheckBox *allCheckBox = new QCheckBox("All", this);
    animationCheckBox = new QCheckBox("Animation", this);
    dataCheckBox = new QCheckBox("Data", this);
    consoleCheckBox = new QCheckBox("Console", this);

    QWidgetAction *allCheckBoxAction = new QWidgetAction(this);
    QWidgetAction *animationCheckBoxAction = new QWidgetAction(this);
    QWidgetAction *consoleCheckBoxAction = new QWidgetAction(this);
    QWidgetAction *dataCheckBoxAction = new QWidgetAction(this);

    allCheckBoxAction->setDefaultWidget(allCheckBox);
    animationCheckBoxAction->setDefaultWidget(animationCheckBox);
    consoleCheckBoxAction->setDefaultWidget(consoleCheckBox);
    dataCheckBoxAction->setDefaultWidget(dataCheckBox);

    windowMenu->addAction(allCheckBoxAction);
    windowMenu->addAction(animationCheckBoxAction);
    windowMenu->addAction(consoleCheckBoxAction);
    windowMenu->addAction(dataCheckBoxAction);

    dataCheckBox->setChecked(true);
    consoleCheckBox->setChecked(true);
    animationCheckBox->setChecked(true);
    allCheckBox->setChecked(true);

    windowMenu->setStyleSheet("QMenu {"
                              "    width: 195px;"
                              "    height: 200px;"
                              "    background-color: #282828;"
                              "    color: rgba(217, 217, 217, 0.70);"
                              "}"
                              "QCheckBox {"
                              "    padding: 8px 16px;"
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
                              "    image: url(:/icon/checkOff.png);"
                              "}"
                              "QCheckBox::indicator:checked {"
                              "    image: url(:/icon/check.png);"
                              "}");

    ui->WindowButton->setMenu(windowMenu);

#if QT_VERSION > QT_VERSION_CHECK(6, 7, 0)
    connect(allCheckBox, &QCheckBox::checkStateChanged, this, &MainWindow::onAllCheckBoxChanged);
    connect(animationCheckBox, &QCheckBox::checkStateChanged, this, &MainWindow::onAnimationCheckBoxChanged);
    connect(consoleCheckBox, &QCheckBox::checkStateChanged, this, &MainWindow::onConsoleCheckBoxChanged);
    connect(dataCheckBox, &QCheckBox::checkStateChanged, this, &MainWindow::onDataCheckBoxChanged);
#else
    connect(allCheckBox, SIGNAL(stateChanged(int)), this, SLOT(onAllCheckBoxChanged(int)));
    connect(animationCheckBox, SIGNAL(stateChanged(int)), this, SLOT(onAnimationCheckBoxChanged(int)));
    connect(consoleCheckBox, SIGNAL(stateChanged(int)), this, SLOT(onConsoleCheckBoxChanged(int)));
    connect(dataCheckBox, SIGNAL(stateChanged(int)), this, SLOT(onDataCheckBoxChanged(int)));
#endif

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
        if (!pixmap.save(fileName)) {
            QMessageBox::critical(this, "Error", "Failed to save image to " + fileName);
        }
    }

    ui->backgrAnim->show();
    ui->ConsoleWidget->show();
    ui->DataWidget->show();
}

void MainWindow::exportToWRL(){
    if (!startButtonPressed || !Parameters::voxels)
    {
        QMessageBox::information(nullptr, "Warning!", "The structure was not generated.");
        return;
    }
    else
    {
        MyGLWidget *voxelCube = ui->myGLWidget;
        std::vector<std::array<GLubyte, 4>> colors = voxelCube->generateDistinctColors();
        Export::ExportToVRML(colors,Parameters::size,Parameters::voxels);
    }
}

void MainWindow::exportToCSV(){
    if (!startButtonPressed || !Parameters::voxels)
    {
        QMessageBox::information(nullptr, "Warning!", "The structure was not generated.");
        return;
    }
    else
    {
        Export::ExportToCSV(Parameters::size, Parameters::voxels);
    }
}

StressAnalysis sa;
void MainWindow::estimateStressWithANSYS()
{

    if(startButtonPressed == false)
    {
        QMessageBox::information(nullptr, "Warning!", "The structure was not generated.");
        return;
    }
    LoadStepManager& lsm = LoadStepManager::getInstance();
    ui->backgrAnim_2->show();

    sa.estimateStressWithANSYS(Parameters::size, Parameters::points, Parameters::voxels);

    auto cmap = ui->myGLWidget->getColorMap(9);
    int comp = ui->geom_ID->currentIndex();

    float maxv = sa.wr->loadstep_results_max[comp];
    float minv = sa.wr->loadstep_results_min[comp];

    if (!this->scene) {
        qDebug() << "LegendView is not initialized!";
        return;
    }

    qDebug() << "Min/Max values:" << minv << maxv;
    qDebug() << "ColorMap size:" << cmap.size();

    if (minv == maxv) {
        minv -= 0.01f;
        maxv += 0.01f;
    }

    this->scene->setMinMax(minv, maxv);
    this->scene->setCmap(cmap);
    this->scene->draw();

    ui->geom_ID->blockSignals(true);
    ui->geom_sub_ID->blockSignals(true);

    ui->geom_ID->clear();
    ui->geom_ID->addItems(lsm.getGeomSetList());

    ui->geom_sub_ID->clear();
    ui->geom_sub_ID->addItems(lsm.getGeomSetSubList());

    ui->geom_ID->blockSignals(false);
    ui->geom_sub_ID->blockSignals(false);

    ui->LegendView->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
    ui->LegendView->viewport()->update();
    ui->LegendView->show();
    ui->myGLWidget->update();
}


void MainWindow::onAllCheckBoxChanged(int state) {
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
        ui->ConsoleWidget->setVisible(true);
    } else {
        ui->ConsoleWidget->setVisible(false);
    }
}


void MainWindow::onAnimationCheckBoxChanged(int state) {
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
    } else {
        isAnimation = 0;
    }
}


void MainWindow::on_AboutButton_clicked()
{
    about = new About;
    about->show();
}

void MainWindow::on_SliderAnimationSpeed_valueChanged(int value)
{
    const double minValueSlider = 1.0;
    const double maxValueSlider = 100.0;
    const double minValueConverted = 1.0;
    const double maxValueConverted = 0.01;

    const double delayAnimation = ((maxValueConverted - minValueConverted) * (value - minValueSlider) / (maxValueSlider - minValueSlider)) + minValueConverted;

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

void MainWindow::setConcentration(int arg)
{
    ui->concentrationRadioButton->setChecked(true);
    QString str = QString::number(arg);
    ui->Rectangle9->setText(str);
    emit ui->Rectangle9->editingFinished();
}

void MainWindow::setAlgorithms(QString arg)
{
    ui->AlgorithmsBox->setCurrentText(arg);
}

void MainWindow::setAlgorithmFlags(Parent_Algorithm& algorithm)
{
    algorithm.setAnimation(isAnimation);
    algorithm.setWaveGeneration(isWaveGeneration);
    algorithm.setPeriodicStructure(false);
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
    HDF5Wrapper hdf5Wrapper("hdf5_save.hdf");

    int last_set = hdf5Wrapper.readInt("/", "last_set");
    if (last_set == -1)
    {
        last_set = 1;
        hdf5Wrapper.write("/", "last_set", last_set);
    }
    else
    {
        last_set += 1;
        hdf5Wrapper.update("/", "last_set", last_set);
    }

    std::string prefix = ("/" + QString::number(last_set)).toStdString();

    if (Parameters::voxels)
    {
        hdf5Wrapper.write(prefix, "voxels", Parameters::voxels, Parameters::size);
        hdf5Wrapper.write(prefix, "cubeSize", Parameters::size);
        hdf5Wrapper.write(prefix, "numPoints", Parameters::points);
    }
}


void MainWindow::openHDF()
{
    QString fileName = QFileDialog::getOpenFileName(this , "Choose MatViz3d HDF5 file" , "" ,"MV3D HDF5 (*.hdf5)");
    if (fileName.length() == 0)
    {
        qDebug() << "OpenHDF: no file selected";
        return;
    }
    LoadStepManager& lsm = LoadStepManager::getInstance();
    if (lsm.LoadFromHDF5(fileName))
    {
        ui->backgrAnim_2->show();

        ui->geom_ID->blockSignals(true);
        ui->geom_sub_ID->blockSignals(true);

        ui->geom_ID->clear();
        ui->geom_ID->addItems(lsm.getGeomSetList());

        ui->geom_sub_ID->clear();
        ui->geom_sub_ID->addItems(lsm.getGeomSetSubList());

        ui->geom_ID->blockSignals(false);
        ui->geom_sub_ID->blockSignals(false);

        auto cmap = ui->myGLWidget->getColorMap(9);
        int comp = ui->geom_ID->currentIndex();

        float maxv = lsm.getMaxVal(comp);
        float minv = lsm.getMinVal(comp);

        if (!this->scene)
        {
            qDebug() << "LegendView is not initialized!";
            return;
        }

        qDebug() << "Min/Max values:" << minv << maxv;
        qDebug() << "ColorMap size:" << cmap.size();

        if (minv == maxv) {
            minv -= 0.01f;
            maxv += 0.01f;
        }

        this->scene->setMinMax(minv, maxv);
        this->scene->setCmap(cmap);
        this->scene->draw();

        ui->LegendView->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
        ui->LegendView->viewport()->update();
        ui->LegendView->show();
        ui->myGLWidget->setNumColors(lsm.getNumPoints());
        ui->myGLWidget->setVoxels(lsm.getVoxelPtr(), lsm.getCubeSize());
        ui->myGLWidget->update();
    }
}

void MainWindow::on_checkBoxWiregrid_stateChanged(int arg1)
{
    ui->myGLWidget->setPlotWireFrame((bool)arg1);
    ui->myGLWidget->repaint();
}


void MainWindow::on_ComponentID_currentIndexChanged(int index)
{
    LoadStepManager& lsm = LoadStepManager::getInstance();
    float maxv = lsm.getMaxVal(index);
    float minv = lsm.getMinVal(index);
    if (minv == maxv)
    {
        minv -= 0.01f;
        maxv += 0.01f;
    }
    this->scene->setMinMax(minv, maxv);
    ui->myGLWidget->setComponent(index);
    ui->myGLWidget->update();

/*   if (sa.wr && sa.wr->loadstep_results_max.size() > (size_t)index)
    {
       float maxv = sa.wr->loadstep_results_max[index];
       float minv = sa.wr->loadstep_results_min[index];
       this->scene->setMinMax(minv, maxv);
       ui->myGLWidget->setComponent(index);
       ui->myGLWidget->update();
    }
*/
}



void MainWindow::on_geom_ID_currentIndexChanged(int index)
{
    if (index < 0) return;

    LoadStepManager& lsm = LoadStepManager::getInstance();
    if (lsm.LoadGeomSet(index + 1))
    {
        ui->geom_sub_ID->blockSignals(true);
        ui->geom_sub_ID->clear();
        ui->geom_sub_ID->addItems(lsm.getGeomSetSubList());
        ui->geom_sub_ID->blockSignals(false);
        ui->myGLWidget->setNumColors(lsm.getNumPoints());
        ui->myGLWidget->setVoxels(lsm.getVoxelPtr(), lsm.getCubeSize());
        ui->myGLWidget->update();
    }
}

void MainWindow::on_geom_sub_ID_currentIndexChanged(int index)
{
    if (index < 0) return;

    LoadStepManager& lsm = LoadStepManager::getInstance();
    if (lsm.LoadGeomSubStep(index + 1)) {
        auto cmap = ui->myGLWidget->getColorMap(9);
        int comp = ui->geom_ID->currentIndex();

        float maxv = lsm.getMaxVal(comp);
        float minv = lsm.getMinVal(comp);

        if (!this->scene) {
            qDebug() << "LegendView is not initialized!";
            return;
        }

        if (minv == maxv) {
            minv -= 0.01f;
            maxv += 0.01f;
        }

        this->scene->setMinMax(minv, maxv);
        this->scene->setCmap(cmap);
        this->scene->draw();
        ui->myGLWidget->setNumColors(lsm.getNumPoints());
        ui->myGLWidget->setVoxels(lsm.getVoxelPtr(), lsm.getCubeSize());

        ui->LegendView->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
        ui->LegendView->viewport()->update();
        ui->LegendView->show();
        ui->myGLWidget->update();
    }

}


void MainWindow::startGifRecording()
{
    if (isRecording) return;

    isRecording = true;

    if (gif)
        delete gif;

    gif = new QGifImage(QSize(ui->myGLWidget->width(), ui->myGLWidget->height()));

    gif->setDefaultDelay(100); // 10 FPS (100 мс)

    connect(ui->myGLWidget, &QOpenGLWidget::frameSwapped, this, &MainWindow::captureFrame);

    qDebug() << "GIF recording started!";
}

void MainWindow::stopGifRecording()
{
    isRecording = false;

    disconnect(ui->myGLWidget, &QOpenGLWidget::frameSwapped, this, &MainWindow::captureFrame);

    if (gif) {
        gif->save("animation.gif");
        qDebug() << "Animation has been saved!";
        delete gif;
        gif = nullptr;
    }
}

QImage makeWhiteBackground(const QImage &src, const QColor &bgColor) {
    QImage img = src.convertToFormat(QImage::Format_ARGB32);

    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            QColor color = QColor::fromRgba(img.pixel(x, y));
            if ((color.rgb() & 0x00FFFFFF) == (bgColor.rgb() & 0x00FFFFFF)) {
                img.setPixel(x, y, qRgb(255, 255, 255));
            }
        }
    }
    QImage indexed = img.convertToFormat(QImage::Format_Indexed8, static_cast<Qt::ImageConversionFlags>(0));
    return indexed;
}

void MainWindow::captureFrame()
{
    if (!isRecording || !gif)
        return;

    QImage frame = ui->myGLWidget->grabFramebuffer();
    if (frame.isNull()) {
        qCritical() << "Error: Captured an empty frame!";
        return;
    }

    frame = frame.convertToFormat(QImage::Format_ARGB32);
    QColor bgColor = frame.pixelColor(0, 0);

    QImage whiteFrame = makeWhiteBackground(frame, bgColor);

    gif->addFrame(whiteFrame);
    qDebug() << "Frame added! Total frames:" << gif->frameCount();
}

/**
 * Capture a screenshot and copy it to the system clipboard.
 */
void MainWindow::copyScreenshotToClipboard()
{
    QImage glImage = ui->myGLWidget->captureScreenshotWithWhiteBackground();

    if (ui->backgrAnim_2->isVisible()) {
        if (scene) {
            QSize legendSize = scene->sceneRect().size().toSize();
            QImage legendImage(legendSize, QImage::Format_RGB32);
            legendImage.fill(Qt::white);

            QList<QGraphicsTextItem*> textItems;
            QList<QColor> originalColors;

            for (QGraphicsItem* item : scene->items()) {
                if (auto* text = qgraphicsitem_cast<QGraphicsTextItem*>(item)) {
                    textItems.append(text);
                    originalColors.append(text->defaultTextColor());
                    text->setDefaultTextColor(Qt::black);
                }
            }

            QPainter painter(&legendImage);
            scene->render(&painter, QRectF(), scene->sceneRect());
            painter.end();

            for (int i = 0; i < textItems.size(); ++i) {
                textItems[i]->setDefaultTextColor(originalColors[i]);
            }

            int totalWidth = std::max(glImage.width(), legendImage.width());
            int totalHeight = glImage.height() + legendImage.height();

            QImage finalImage(QSize(totalWidth, totalHeight), QImage::Format_RGB32);
            finalImage.fill(Qt::white);

            QPainter finalPainter(&finalImage);
            int xOffset = (glImage.width() - legendImage.width()) / 2;
            if (xOffset < 0) xOffset = 0;

            finalPainter.drawImage(0, 0, glImage);
            finalPainter.drawImage(xOffset, glImage.height(), legendImage);
            finalPainter.end();

            glImage = finalImage;
        }
    }

    QGuiApplication::clipboard()->setImage(glImage);
}



