#include "mainwindowwrapper.h"
#include <QDebug>

MainWindowWrapper::MainWindowWrapper(QObject *parent) : QObject(parent) {}

void MainWindowWrapper::showMessage()
{
    qDebug() << "Button pressed";
}
