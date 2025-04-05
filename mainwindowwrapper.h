#ifndef MAINWINDOWWRAPPER_H
#define MAINWINDOWWRAPPER_H

#include <QObject>
#include "mainwindowalgorithmhandler.h"

class MainWindowWrapper : public QObject
{
    Q_OBJECT
public:
    explicit MainWindowWrapper(QObject *parent = nullptr);

    Q_INVOKABLE void onStartButton();
    Q_INVOKABLE void isometricViewButton();
    Q_INVOKABLE void dimetricViewButton();
    Q_INVOKABLE void frontViewButton();
    Q_INVOKABLE void backViewButton();
    Q_INVOKABLE void topViewButton();
    Q_INVOKABLE void bottomViewButton();
    Q_INVOKABLE void leftViewButton();
    Q_INVOKABLE void rightViewButton();   

private:
    MainWindowAlgorithmHandler algoManager;
};

#endif // MAINWINDOWWRAPPER_H
