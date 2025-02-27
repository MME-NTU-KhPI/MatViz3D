#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QtSQL>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>

class DBManager : public QObject
{
    Q_OBJECT
private:
    QSqlDatabase db;
    QSqlTableModel *model;
    void createTable();
    void insertInitialData();
public:
    explicit DBManager(QObject *parent = nullptr);
    ~DBManager();
    Q_INVOKABLE void addMaterial(const QString &material);
    Q_INVOKABLE void removeMaterial(int row);
    Q_INVOKABLE void updateMaterial(int row, int column, const QVariant &value);
    Q_INVOKABLE QSqlTableModel* getModel() { return model; };
};

#endif // DBMANAGER_H
