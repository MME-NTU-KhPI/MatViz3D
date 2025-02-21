#include "materialdatabaseviewwrapper.h"
#include <QDebug>

MaterialDatabaseViewWrapper::MaterialDatabaseViewWrapper(QObject *parent) : QObject(parent) {}

void MaterialDatabaseViewWrapper::showMessage()
{
    qDebug() << "Button pressed";
}
