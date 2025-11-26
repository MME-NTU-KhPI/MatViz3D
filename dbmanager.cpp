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
        insertInitialData();
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
        "Material TEXT NOT NULL, "
        "Type VARCHAR(6) DEFAULT '', "
        "c11 REAL DEFAULT 0, c12 REAL DEFAULT 0, c13 REAL DEFAULT 0, "
        "c14 REAL DEFAULT 0, c15 REAL DEFAULT 0, c16 REAL DEFAULT 0, "
        "c22 REAL DEFAULT 0, c23 REAL DEFAULT 0, c24 REAL DEFAULT 0, "
        "c25 REAL DEFAULT 0, c26 REAL DEFAULT 0, c33 REAL DEFAULT 0, "
        "c34 REAL DEFAULT 0, c35 REAL DEFAULT 0, c36 REAL DEFAULT 0, "
        "c44 REAL DEFAULT 0, c45 REAL DEFAULT 0, c46 REAL DEFAULT 0, "
        "c55 REAL DEFAULT 0, c56 REAL DEFAULT 0, c66 REAL DEFAULT 0"
        ");";
    if (!query.exec(createTableSQL))
        qCritical() << "Error creating table: " << query.lastError().text();
    else
        qDebug() << "Table created successfully.";
}

void DBManager::insertInitialData()
{
    QSqlQuery query(db);

    query.prepare("SELECT COUNT(*) FROM material_properties");
    if (!query.exec() || !query.next()) {
        qCritical() << "Error checking existing data:" << query.lastError().text();
        return;
    }

    if (query.value(0).toInt() > 0) {
        qDebug() << "Initial data already exists, skipping insertion.";
        return;
    }

    query.prepare("INSERT INTO material_properties (Material, Type, c11, c12, c13, c14, c15, c16, "
                  "c22, c23, c24, c25, c26, c33, c34, c35, c36, c44, c45, c46, c55, c56, c66) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

    struct Material {
        QString material;
        QString type;
        double c11, c12, c13, c14, c15, c16;
        double c22, c23, c24, c25, c26;
        double c33, c34, c35, c36;
        double c44, c45, c46;
        double c55, c56;
        double c66;
    };

    QVector<Material> materials = {
        {"Ag", "fcc", 124.73, 94.05, 94.05, 0,0,0, 124.73, 94.05, 0,0,0, 124.73, 0,0,0, 46.58, 0,0, 46.58, 0, 46.58},
        {"Al", "fcc", 107.90, 60.40, 60.40, 0,0,0, 107.90, 60.40, 0,0,0, 107.90, 0,0,0, 28.60, 0,0, 28.60, 0, 28.60},
        {"Au", "fcc", 193.22, 163.78, 163.78, 0,0,0, 193.22, 163.78, 0,0,0, 193.22, 0,0,0, 42.26, 0,0, 42.26, 0, 42.26},
        {"Cu", "fcc", 168.40, 121.40, 121.40, 0,0,0, 168.40, 121.40, 0,0,0, 168.40, 0,0,0, 75.40, 0,0, 75.40, 0, 75.40},
        {"Ir", "fcc", 580.00, 242.00, 242.00, 0,0,0, 580.00, 242.00, 0,0,0, 580.00, 0,0,0, 256.00, 0,0, 256.00, 0, 256.00},
        {"Ni", "fcc", 253.00, 152.00, 152.00, 0,0,0, 253.00, 152.00, 0,0,0, 253.00, 0,0,0, 124.00, 0,0, 124.00, 0, 124.00},
        {"Pb", "fcc", 49.53, 42.29, 42.29, 0,0,0, 49.53, 42.29, 0,0,0, 49.53, 0,0,0, 14.90, 0,0, 14.90, 0, 14.90},
        {"Pt", "fcc", 346.70, 250.70, 250.70, 0,0,0, 346.70, 250.70, 0,0,0, 346.70, 0,0,0, 76.50, 0,0, 76.50, 0, 76.50},
        {"Pd", "fcc", 227.10, 176.00, 176.00, 0,0,0, 227.10, 176.00, 0,0,0, 227.10, 0,0,0, 71.70, 0,0, 71.70, 0, 71.70},
        {"Cr", "bcc", 339.80, 58.60, 58.60, 0,0,0, 339.80, 58.60, 0,0,0, 339.80, 0,0,0, 99.00, 0,0, 99.00, 0, 99.00},
        {"Fe", "bcc", 232.20, 134.70, 134.70, 0,0,0, 232.20, 134.70, 0,0,0, 232.20, 0,0,0, 117.00, 0,0, 117.00, 0, 117.00},
        {"Mo", "bcc", 470.70, 167.50, 167.50, 0,0,0, 470.70, 167.50, 0,0,0, 470.70, 0,0,0, 107.00, 0,0, 107.00, 0, 107.00},
        {"Nb", "bcc", 246.00, 134.00, 134.00, 0,0,0, 246.00, 134.00, 0,0,0, 246.00, 0,0,0, 28.70, 0,0, 28.70, 0, 28.70},
        {"Ta", "bcc", 267.00, 161.00, 161.00, 0,0,0, 267.00, 161.00, 0,0,0, 267.00, 0,0,0, 82.50, 0,0, 82.50, 0, 82.50},
        {"V", "bcc", 228.00, 119.00, 119.00, 0,0,0, 228.00, 119.00, 0,0,0, 228.00, 0,0,0, 42.60, 0,0, 42.60, 0, 42.60},
        {"W", "bcc", 522.40, 204.40, 204.40, 0,0,0, 522.40, 204.40, 0,0,0, 522.40, 0,0,0, 160.50, 0,0, 160.50, 0, 160.50},
        {"C", "dc", 1079.0, 124.00, 124.00, 0,0,0, 1079.0, 124.00, 0,0,0, 1079.0, 0,0,0, 578.00, 0,0, 578.00, 0, 578.00},
        {"Ge", "dc", 129.20, 47.90, 47.90, 0,0,0, 129.20, 47.90, 0,0,0, 129.20, 0,0,0, 67.00, 0,0, 67.00, 0, 67.00},
        {"Si", "dc", 167.40, 65.23, 65.23, 0,0,0, 167.40, 65.23, 0,0,0, 167.40, 0,0,0, 79.57, 0,0, 79.57, 0, 79.57},
        {"GaAs", "zb", 118.41, 53.70, 53.70, 0,0,0, 118.41, 53.70, 0,0,0, 118.41, 0,0,0, 59.12, 0,0, 59.12, 0, 59.12},
        {"GaP", "zb", 141.40, 63.98, 63.98, 0,0,0, 141.40, 63.98, 0,0,0, 141.40, 0,0,0, 70.28, 0,0, 70.28, 0, 70.28},
        {"InP", "zb", 102.20, 57.60, 57.60, 0,0,0, 102.20, 57.60, 0,0,0, 102.20, 0,0,0, 46.00, 0,0, 46.00, 0, 46.00},
        {"LiF", "rs", 111.20, 42.40, 42.40, 0,0,0, 111.20, 42.40, 0,0,0, 111.20, 0,0,0, 64.90, 0,0, 64.90, 0, 64.90},
        {"MgO", "rs", 298.20, 95.25, 95.25, 0,0,0, 298.20, 95.25, 0,0,0, 298.20, 0,0,0, 154.40, 0,0, 154.40, 0, 154.40},
        {"TiC", "rs", 389.10, 43.30, 43.30, 0,0,0, 389.10, 43.30, 0,0,0, 389.10, 0,0,0, 203.20, 0,0, 203.20, 0, 203.20}
    };

    for (const auto &mat : materials) {
        std::array<double, 21> coefficients = {
            mat.c11, mat.c12, mat.c13, mat.c14, mat.c15, mat.c16,
            mat.c22, mat.c23, mat.c24, mat.c25, mat.c26,
            mat.c33, mat.c34, mat.c35, mat.c36,
            mat.c44, mat.c45, mat.c46,
            mat.c55, mat.c56,
            mat.c66
        };

        query.addBindValue(mat.material);
        query.addBindValue(mat.type);
        for (const auto &coef : coefficients) {
            query.addBindValue(coef);
        }
        if (!query.exec()) {
            qCritical() << "Failed to insert data:" << query.lastError().text();
        }
    }
    qDebug() << "Initial data inserted successfully.";
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
        model->select();
    }
}

QVariantList DBManager::executeSelectQuery(const QString& queryString)
{
    QVariantList resultList;
    QSqlQuery query(db);

    if (!query.exec(queryString))
    {
        qCritical() << "Error executing select query:" << query.lastError().text();
        return resultList;
    }

    QSqlRecord record = query.record();

    while (query.next())
    {
        QVariantMap rowMap;
        for (int i = 0; i < record.count(); ++i)
        {
            QString columnName = record.fieldName(i);
            rowMap[columnName] = query.value(i);
        }
        resultList.append(rowMap);
    }

    return resultList;
}
