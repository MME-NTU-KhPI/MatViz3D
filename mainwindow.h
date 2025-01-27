
#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#define H5_BUILT_AS_DYNAMIC_LIB
#include <QMainWindow>
#include <QButtonGroup>
#include <QPushButton>
#include <QWidget>
#include <QSlider>
#include "statistics.h"
#include "messagehandler.h"
#include "about.h"
#include "probability_algorithm.h"
#include "algorithmfactory.h"
#include "legendview.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow

{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    void setNumCubes(short int arg);
    void setNumColors(int arg);
    void setConcentration(int arg);
    void setAlgorithms(QString arg);
    void callExportToCSV();
    bool isAnimation = false;
    bool isWaveGeneration = false;
    bool isPeriodicStructure = false;
    int delayAnimation;
    ~MainWindow();

public slots:
    void onStartClicked();
    void estimateStressWithANSYS();

private slots:
    void checkStart();
    void onLogMessageWritten(const QString &message);
    void setupFileMenu();
    void saveAsImage();
    void exportToWRL();
    void initializeUIForStart();
    bool validateParameters();
    void clearVoxels();
    void executeAlgorithm(Parent_Algorithm& algorithm, const QString& algorithmName);

    void executeProbabilityAlgorithm();
    void finalizeUIAfterCompletion();
    void logExecutionTime(clock_t start_time);

    void setupWindowMenu();
    void onConsoleCheckBoxChanged(int state);
    void onAnimationCheckBoxChanged(int state);
    void onDataCheckBoxChanged(int state);
    void onAllCheckBoxChanged(int state);
    void exportToCSV();
    void onInitialConditionSelectionChanged();
    void onProbabilityAlgorithmChanged(const QString &text);
    void saveHDF();
    void openHDF();

    void on_Start_clicked();

    void on_statistics_clicked();

    void on_checkBoxAnimation_stateChanged(int arg1);

    void on_AboutButton_clicked();

    void on_SliderAnimationSpeed_valueChanged(int value);

    void on_checkBoxWiregrid_stateChanged(int arg1);

    void on_ComponentID_currentIndexChanged(int index);

protected:
    void keyPressEvent(QKeyEvent *event);

private:
    Ui::MainWindow *ui;
    Statistics form;
    About *about;
    Probability_Algorithm *probability_algorithm;
    MessageHandler *messageHandlerInstance;
    QCheckBox *allCheckBox;
    QCheckBox *dataCheckBox;
    QCheckBox *consoleCheckBox;
    QCheckBox *animationCheckBox;
    bool startButtonPressed;
    LegendView* scene;
};

#endif // MAINWINDOW_H
