#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QtSQL>
#include <QDebug>

class DBManager
{
private:
    QSqlDatabase db;
    void createTable();
public:
    DBManager(const QString &dbPath);
    ~DBManager();
    void updateRecord();
    void deleteRecordById();
    void addRecord();
};

#endif // DBMANAGER_H
