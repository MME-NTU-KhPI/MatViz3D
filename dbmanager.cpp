#include "dbmanager.h"

DBManager::DBManager(const QString &dbPath)
{
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath);

    if (!db.open())
        qCritical() << "Error: Unable to connect to database";
    else
    {
        qDebug() << "Database connected successfully";
        createTable();
    }
};

DBManager::~DBManager()
{
    db.close();
};

void DBManager::createTable()
{
    QSqlQuery query;
    QString createTableSQL =
        "CREATE TABLE IF NOT EXISTS material_properties ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "Material TEXT NOT NULL, Type VARCHAR(6),"
        "c11 REAL, c12 REAL, c13 REAL, "
        "c21 REAL DEFAULT 0, c22 REAL DEFAULT 0, c23 REAL DEFAULT 0, "
        "c31 REAL DEFAULT 0, c32 REAL DEFAULT 0, c33 REAL DEFAULT 0, "
        "c44 REAL DEFAULT 0, c55 REAL DEFAULT 0, c66 REAL DEFAULT 0, "
        "E REAL, v REAL, mu REAL, A REAL "
        ");";
    if (!query.exec(createTableSQL))
        qCritical() << "Error creating table: " << query.lastError().text();
    else
        qDebug() << "Table created successfully.";
}

void DBManager::addRecord()
{

}

void DBManager::updateRecord()
{

}

void DBManager::deleteRecordById()
{

}
