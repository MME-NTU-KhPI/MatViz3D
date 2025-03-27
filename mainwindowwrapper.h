#ifndef MAINWINDOWWRAPPER_H
#define MAINWINDOWWRAPPER_H

#include <QObject>

class MainWindowWrapper : public QObject
{
    Q_OBJECT
public:
    explicit MainWindowWrapper(QObject *parent = nullptr);

    Q_INVOKABLE void startButton();
    Q_INVOKABLE void isometricViewButton();
    Q_INVOKABLE void dimetricViewButton();
    Q_INVOKABLE void frontViewButton();
    Q_INVOKABLE void backViewButton();
    Q_INVOKABLE void topViewButton();
    Q_INVOKABLE void bottomViewButton();
    Q_INVOKABLE void leftViewButton();
    Q_INVOKABLE void rightViewButton();
};

#endif // MAINWINDOWWRAPPER_H
