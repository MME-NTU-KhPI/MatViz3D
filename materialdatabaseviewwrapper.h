#ifndef MATERIALDATABASEVIEWWRAPPER_H
#define MATERIALDATABASEVIEWWRAPPER_H

#include <QObject>

class MaterialDatabaseViewWrapper : public QObject
{
    Q_OBJECT
public:
    explicit MaterialDatabaseViewWrapper(QObject *parent = nullptr);

    Q_INVOKABLE void showMessage();
};

#endif // MATERIALDATABASEVIEWWRAPPER_H
