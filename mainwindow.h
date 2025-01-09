#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#define H5_BUILT_AS_DYNAMIC_LIB
#include <QMainWindow>
#include <QButtonGroup>
#include <QPushButton>
#include <QWidget>
#include <QSlider>
#include "animation.h"
#include "statistics.h"
#include "messagehandler.h"
#include "about.h"
#include "probability_algorithm.h"
#include "legendview.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow

{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    static Probability_Algorithm *probability_algorithm;
    Probability_Algorithm* getProbabilityAlgorithm() { return probability_algorithm; }
    void setNumCubes(short int arg);
    void setNumColors(int arg);
    void setConcentration(int arg);
    void setAlgorithms(QString arg);
    void callExportToCSV();
    int isAnimation = 0;
    int isWaveGeneration = 0;
    int isClosedCube;
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
    MessageHandler *messageHandlerInstance;
    QCheckBox *allCheckBox;
    QCheckBox *dataCheckBox;
    QCheckBox *consoleCheckBox;
    QCheckBox *animationCheckBox;
    bool startButtonPressed;
    LegendView* scene;
};

#endif // MAINWINDOW_H
