#include "mainwindowwrapper.h"
#include "mainwindowalgorithmhandler.h"
#include "openglwidgetqml.h"

#include <QDebug>

MainWindowWrapper::MainWindowWrapper(QObject *parent) : QObject(parent) {}

void MainWindowWrapper::onStartButton()
{
    Parameters& params = *Parameters::instance();
    algoManager.runAlgorithm(params.getAlgorithm(), isAnimation);
}

void MainWindowWrapper::isometricViewButton()
{
    OpenGLWidgetQML *ogl = OpenGLWidgetQML::getInstance();
    if (ogl)
    {
        ogl->setIsometricView();
    }
}

void MainWindowWrapper::dimetricViewButton()
{
    OpenGLWidgetQML *ogl = OpenGLWidgetQML::getInstance();
    if (ogl)
    {
        ogl->setDimetricView();
    }
}

void MainWindowWrapper::frontViewButton()
{
    OpenGLWidgetQML *ogl = OpenGLWidgetQML::getInstance();
    if (ogl)
    {
        ogl->setFrontView();
    }
}

void MainWindowWrapper::backViewButton()
{
    OpenGLWidgetQML *ogl = OpenGLWidgetQML::getInstance();
    if (ogl)
    {
        ogl->setBackView();
    }
}

void MainWindowWrapper::topViewButton()
{
    OpenGLWidgetQML *ogl = OpenGLWidgetQML::getInstance();
    if (ogl)
    {
        ogl->setTopView();
    }
}

void MainWindowWrapper::bottomViewButton()
{
    OpenGLWidgetQML *ogl = OpenGLWidgetQML::getInstance();
    if (ogl)
    {
        ogl->setBottomView();
    }
}

void MainWindowWrapper::leftViewButton()
{
    OpenGLWidgetQML *ogl = OpenGLWidgetQML::getInstance();
    if (ogl)
    {
        ogl->setLeftView();
    }
}

void MainWindowWrapper::rightViewButton()
{
    OpenGLWidgetQML *ogl = OpenGLWidgetQML::getInstance();
    if (ogl)
    {
        ogl->setRightView();
    }
}
