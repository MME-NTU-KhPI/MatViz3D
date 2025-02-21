#include "dbmanager.h"

DBManager::DBManager(QObject *parent)
    : QObject(parent)
{
    QString dbPath = QCoreApplication::applicationDirPath() + "/material_properties.db";
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath);

    if (!db.open())
        qCritical() << "Error: Unable to connect to database";
    else
    {
        qDebug() << "Database connected successfully";
        createTable();
    }

    model = new QSqlTableModel(this, db);
    model->setTable("material_properties");
    model->setEditStrategy(QSqlTableModel::OnManualSubmit); // Важливо для оновлення!
    model->select();
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
        "Material TEXT NOT NULL, Type VARCHAR(6) DEFAULT '',"
        "c11 REAL DEFAULT 0, c12 REAL DEFAULT 0, c13 REAL DEFAULT 0, "
        "c21 REAL DEFAULT 0, c22 REAL DEFAULT 0, c23 REAL DEFAULT 0, "
        "c31 REAL DEFAULT 0, c32 REAL DEFAULT 0, c33 REAL DEFAULT 0, "
        "c44 REAL DEFAULT 0, c55 REAL DEFAULT 0, c66 REAL DEFAULT 0, "
        "E REAL DEFAULT 0, v REAL DEFAULT 0, mu REAL DEFAULT 0, A REAL DEFAULT 0"
        ");";
    if (!query.exec(createTableSQL))
        qCritical() << "Error creating table: " << query.lastError().text();
    else
        qDebug() << "Table created successfully.";
}

Q_INVOKABLE void DBManager::addMaterial(const QString &material)
{
    QSqlQuery query;
    query.prepare("INSERT INTO material_properties (Material) VALUES (:Material)");
    query.bindValue(":Material", material);
    if (!query.exec()) {
        qDebug() << "Error inserting material:" << query.lastError().text();
    } else {
        qDebug() << "Material added!";
        model->select();
    }
}

Q_INVOKABLE void DBManager::removeMaterial(int row)
{
    if (row >= 0 && row < model->rowCount()) {
        model->removeRow(row);
        model->submitAll();
        model->select();
    }
}

Q_INVOKABLE void DBManager::updateMaterial(int row, int column, const QVariant &value) {
    if (row >= 0 && row < model->rowCount()) {
        model->setData(model->index(row, column), value);
        model->submitAll();
        model->select();  // Оновлюємо модель
    }
}
