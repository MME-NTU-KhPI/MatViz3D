#ifndef MAINWINDOWWRAPPER_H
#define MAINWINDOWWRAPPER_H

#include <QObject>

class MainWindowWrapper : public QObject
{
    Q_OBJECT
public:
    explicit MainWindowWrapper(QObject *parent = nullptr);

    Q_INVOKABLE void showMessage();
};

#endif // MAINWINDOWWRAPPER_H
