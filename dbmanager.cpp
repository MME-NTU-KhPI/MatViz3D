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

    int rowCount = query.value(0).toInt();
    if (rowCount > 0) {
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
        double c22, c23, c24, c25, c26, c33;
        double c34, c35, c36, c44, c45, c46;
        double c55, c56, c66;
    };


    QVector<Material> materials = {
        {"Ag", "fcc", 124.00, 93.40, 93.40, 0, 0, 0, 93.40, 124.00, 93.40, 0, 0, 0, 93.40, 93.40, 124.00, 0, 0, 0, 46.10, 46.10, 46.10},
        {"Al", "fcc", 107.30, 60.90, 60.90, 0, 0, 0, 60.90, 107.30, 60.90, 0, 0, 0, 60.90, 60.90, 107.30, 0, 0, 0, 28.30, 28.30, 28.30},
        {"Au", "fcc", 192.90, 163.80, 163.80, 0, 0, 0, 163.80, 192.90, 163.80, 0, 0, 0, 163.80, 163.80, 192.90, 0, 0, 0, 41.50, 41.50, 41.50},
        {"Cu", "fcc", 168.40, 121.40, 121.40, 0, 0, 0, 121.40, 168.40, 121.40, 0, 0, 0, 121.40, 121.40, 168.40, 0, 0, 0, 75.40, 75.40, 75.40},
        {"Ir", "fcc", 580.00, 242.00, 242.00, 0, 0, 0, 242.00, 580.00, 242.00, 0, 0, 0, 242.00, 242.00, 580.00, 0, 0, 0, 256.00, 256.00, 256.00},
        {"Ni", "fcc", 246.50, 147.30, 147.30, 0, 0, 0, 147.30, 246.50, 147.30, 0, 0, 0, 147.30, 147.30, 246.50, 0, 0, 0, 127.40, 127.40, 127.40},
        {"Pb", "fcc", 49.50, 42.30, 42.30, 0, 0, 0, 42.30, 49.50, 42.30, 0, 0, 0, 42.30, 42.30, 49.50, 0, 0, 0, 14.90, 14.90, 14.90},
        {"Pd", "fcc", 227.10, 176.00, 176.00, 0, 0, 0, 176.00, 227.10, 176.00, 0, 0, 0, 176.00, 176.00, 227.10, 0, 0, 0, 71.70, 71.70, 71.70},
        {"Pt", "fcc", 346.70, 250.70, 250.70, 0, 0, 0, 250.70, 346.70, 250.70, 0, 0, 0, 250.70, 250.70, 346.70, 0, 0, 0, 76.50, 76.50, 76.50},
        {"Cr", "bcc", 339.80, 58.60, 58.60, 0, 0, 0, 58.60, 339.80, 58.60, 0, 0, 0, 58.60, 58.60, 339.80, 0, 0, 0, 99.00, 99.00, 99.00},
        {"Fe", "bcc", 231.40, 134.70, 134.70, 0, 0, 0, 134.70, 231.40, 134.70, 0, 0, 0, 134.70, 134.70, 231.40, 0, 0, 0, 116.40, 116.40, 116.40},
        {"K", "bcc", 4.14, 2.21, 2.21, 0, 0, 0, 2.21, 4.14, 2.21, 0, 0, 0, 2.21, 2.21, 4.14, 0, 0, 0, 2.63, 2.63, 2.63},
        {"Li", "bcc", 13.50, 11.44, 11.44, 0, 0, 0, 11.44, 13.50, 11.44, 0, 0, 0, 11.44, 11.44, 13.50, 0, 0, 0, 8.78, 8.78, 8.78},
        {"Mo", "bcc", 440.80, 172.40, 172.40, 0, 0, 0, 172.40, 440.80, 172.40, 0, 0, 0, 172.40, 172.40, 440.80, 0, 0, 0, 121.70, 121.70, 121.70},
        {"Na", "bcc", 6.15, 4.96, 4.96, 0, 0, 0, 4.96, 6.15, 4.96, 0, 0, 0, 4.96, 4.96, 6.15, 0, 0, 0, 5.92, 5.92, 5.92},
        {"Nb", "bcc", 240.20, 125.60, 125.60, 0, 0, 0, 125.60, 240.20, 125.60, 0, 0, 0, 125.60, 125.60, 240.20, 0, 0, 0, 28.20, 28.20, 28.20},
        {"Ta", "bcc", 260.20, 154.50, 154.50, 0, 0, 0, 154.50, 260.20, 154.50, 0, 0, 0, 154.50, 154.50, 260.20, 0, 0, 0, 82.60, 82.60, 82.60},
        {"V", "bcc", 228.00, 118.70, 118.70, 0, 0, 0, 118.70, 228.00, 118.70, 0, 0, 0, 118.70, 118.70, 228.00, 0, 0, 0, 42.60, 42.60, 42.60},
        {"W", "bcc", 522.40, 204.40, 204.40, 0, 0, 0, 204.40, 522.40, 204.40, 0, 0, 0, 204.40, 204.40, 522.40, 0, 0, 0, 160.80, 160.80, 160.80},
        {"C", "dc", 949.00, 151.00, 151.00, 0, 0, 0, 151.00, 949.00, 151.00, 0, 0, 0, 151.00, 151.00, 949.00, 0, 0, 0, 521.00, 521.00, 521.00},
        {"Ge", "dc", 128.40, 48.20, 48.20, 0, 0, 0, 48.20, 128.40, 48.20, 0, 0, 0, 48.20, 48.20, 128.40, 0, 0, 0, 66.70, 66.70, 66.70},
        {"Si", "dc", 166.20, 64.40, 64.40, 0, 0, 0, 64.40, 166.20, 64.40, 0, 0, 0, 64.40, 64.40, 166.20, 0, 0, 0, 79.80, 79.80, 79.80}
    };

    for (const auto &mat : materials) {
        std::array<double, 21> coefficients = {
            mat.c11, mat.c12, mat.c13, mat.c14, mat.c15, mat.c16,
            mat.c22, mat.c23, mat.c24, mat.c25, mat.c26, mat.c33,
            mat.c34, mat.c35, mat.c36, mat.c44, mat.c45, mat.c46,
            mat.c55, mat.c56, mat.c66
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
        model->select();  // Оновлюємо модель
    }
}
