#include <QCoreApplication>
#include <QSet>
#include "dbmanager.h"
#include "phasematerial.h"   // cubicVoigt()

// Opened lazily and shared by every caller: the GUI builds a DBManager at
// startup, but headless runs resolve --db_material without one, and both must
// end up talking to the same file and the same seeded table.
QSqlDatabase DBManager::materialDatabase()
{
    if (QSqlDatabase::contains(QSqlDatabase::defaultConnection)) {
        QSqlDatabase existing = QSqlDatabase::database(QSqlDatabase::defaultConnection);
        if (existing.isOpen())
            return existing;
    }

    if (!QSqlDatabase::isDriverAvailable("QSQLITE")) {
        qCritical() << "Error: the QSQLITE driver is not available";
        return QSqlDatabase();
    }

    QString dbPath = QCoreApplication::applicationDirPath() + "/material_properties.db";
    QSqlDatabase db = QSqlDatabase::contains(QSqlDatabase::defaultConnection)
                          ? QSqlDatabase::database(QSqlDatabase::defaultConnection, /*open=*/false)
                          : QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath);

    if (!db.open()) {
        qCritical() << "Error: Unable to connect to database" << db.lastError().text();
        return QSqlDatabase();
    }

    qDebug() << "Database connected successfully";
    createTable(db);
    insertInitialData(db);
    return db;
}

QStringList DBManager::materialNames()
{
    QStringList out;
    QSqlDatabase db = materialDatabase();
    if (!db.isOpen())
        return out;

    QSqlQuery query(db);
    if (!query.exec("SELECT Material FROM material_properties ORDER BY id")) {
        qCritical() << "Error listing materials:" << query.lastError().text();
        return out;
    }
    while (query.next()) {
        const QString name = query.value(0).toString();
        if (!name.isEmpty())
            out << name;
    }
    return out;
}

bool DBManager::cubicConstants(const QString& material,
                               double& c11, double& c12, double& c44,
                               QString& type)
{
    QSqlDatabase db = materialDatabase();
    if (!db.isOpen())
        return false;

    QSqlQuery query(db);
    query.prepare("SELECT c11, c12, c44, Type FROM material_properties "
                  "WHERE Material = :m LIMIT 1");
    query.bindValue(":m", material);
    if (!query.exec()) {
        qCritical() << "Error querying material" << material << ":" << query.lastError().text();
        return false;
    }
    if (!query.next())
        return false;

    c11  = query.value(0).toDouble();
    c12  = query.value(1).toDouble();
    c44  = query.value(2).toDouble();
    type = query.value(3).toString();
    return true;
}

bool DBManager::stiffnessMatrix(const QString& material,
                                double C[6][6],
                                QString& type)
{
    QSqlDatabase db = materialDatabase();
    if (!db.isOpen())
        return false;

    // The table keeps the upper triangle only; build the column list in the
    // same c<i><j> naming elasticMatrix() uses so the two stay in step.
    QStringList cols;
    for (int i = 1; i <= 6; ++i)
        for (int j = i; j <= 6; ++j)
            cols << QStringLiteral("c%1%2").arg(i).arg(j);

    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT %1, Type FROM material_properties "
                                 "WHERE Material = :m LIMIT 1").arg(cols.join(", ")));
    query.bindValue(":m", material);
    if (!query.exec()) {
        qCritical() << "Error querying material" << material << ":" << query.lastError().text();
        return false;
    }
    if (!query.next())
        return false;

    double M[6][6] = {{0}};
    int col = 0;
    for (int i = 0; i < 6; ++i)
        for (int j = i; j < 6; ++j, ++col)
            M[i][j] = M[j][i] = query.value(col).toDouble();

    type = query.value(col).toString();

    if (M[0][0] <= 0.0)
        return false;                       // no usable constants in this row

    // A hand-added cubic material typically has only c11/c12/c44 filled in,
    // which as a raw matrix is singular (c22 = c33 = 0). Reconstruct what those
    // three constants actually mean rather than handing a solver a broken
    // tensor -- cubicConstants() reads the same row and would disagree.
    if (M[1][1] <= 0.0 || M[2][2] <= 0.0) {
        qInfo() << "material" << material
                << "has only the cubic constants filled in; expanding c11/c12/c44"
                << "into the full cubic matrix";
        cubicVoigt(M[0][0], M[0][1], M[3][3], M);
    }

    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j) C[i][j] = M[i][j];
    return true;
}

DBManager::DBManager(QObject *parent)
    : QObject(parent)
{
    db = materialDatabase();
    if (!db.isOpen())
        return;

    model = new QSqlTableModel(this, db);
    model->setTable("material_properties");
    model->setEditStrategy(QSqlTableModel::OnManualSubmit); // Важливо для оновлення!
    model->select();
};

DBManager::~DBManager()
{
    db.close();
};

void DBManager::createTable(QSqlDatabase& db)
{
    QSqlQuery query(db);
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

void DBManager::insertInitialData(QSqlDatabase& db)
{
    QSqlQuery query(db);

    // Which materials the file already has. Seeding is per-material rather than
    // all-or-nothing: the table is only ever created empty once, so an
    // existing installation would never see a material added later. Rows that
    // are already there are left exactly as they are, edits included.
    QSet<QString> present;
    if (query.exec("SELECT Material FROM material_properties")) {
        while (query.next())
            present.insert(query.value(0).toString());
    } else {
        qCritical() << "Error checking existing data:" << query.lastError().text();
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

    int inserted = 0;

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
        {"TiC", "rs", 389.10, 43.30, 43.30, 0,0,0, 389.10, 43.30, 0,0,0, 389.10, 0,0,0, 203.20, 0,0, 203.20, 0, 203.20},

        // ── Composite constituents ────────────────────────────────────────
        // Everything above is a cubic single crystal. A fiber-reinforced RVE
        // needs the other kind of constituent: engineering materials that are
        // already homogeneous at the voxel scale.
        //
        // Type "iso": isotropic, so c11 = lambda + 2mu, c12 = lambda,
        // c44 = mu = (c11 - c12)/2, and the orientation a solver assigns is
        // irrelevant -- rotating them is a no-op.
        {"Epoxy",   "iso",   5.62,   3.02,   3.02, 0,0,0,   5.62,   3.02, 0,0,0,   5.62, 0,0,0,   1.30, 0,0,   1.30, 0,   1.30},
        {"E-glass", "iso",  82.20,  23.19,  23.19, 0,0,0,  82.20,  23.19, 0,0,0,  82.20, 0,0,0,  29.51, 0,0,  29.51, 0,  29.51},
        {"Al2O3",   "iso", 433.85, 122.37, 122.37, 0,0,0, 433.85, 122.37, 0,0,0, 433.85, 0,0,0, 155.74, 0,0, 155.74, 0, 155.74},
        {"SiC",     "iso", 429.58,  69.93,  69.93, 0,0,0, 429.58,  69.93, 0,0,0, 429.58, 0,0,0, 179.82, 0,0, 179.82, 0, 179.82},

        // Type "ti": transversely isotropic about axis 3, which is the axis
        // Composite aligns a fiber to -- so c33 is the stiff along-fiber
        // direction. T300-class PAN carbon fiber, from the usual engineering
        // constants (Ea = 230, Et = 15, Ga = 15, nu_a = 0.2, nu_t = 0.07 GPa)
        // inverted to stiffnesses. Note c66 = (c11 - c12)/2 = 7.01, which is
        // what makes the 1-2 plane isotropic.
        {"C-fiber", "ti",   15.12,   1.10,   3.24, 0,0,0,  15.12,   3.24, 0,0,0, 231.30, 0,0,0,  15.00, 0,0,  15.00, 0,   7.01}
    };

    for (const auto &mat : materials) {
        if (present.contains(mat.material))
            continue;

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
        } else {
            ++inserted;
        }
    }

    if (inserted > 0)
        qDebug() << "Seeded" << inserted << "material(s) into material_properties.";
    else
        qDebug() << "material_properties is already up to date.";
}

Q_INVOKABLE void DBManager::addMaterial(const QString &material)
{
    if (!model) return;
    QSqlQuery query(db);
    query.prepare("INSERT INTO material_properties (Material) VALUES (:Material)");
    query.bindValue(":Material", material);
    if (!query.exec()) {
        qDebug() << "Error inserting material:" << query.lastError().text();
    } else {
        qDebug() << "Material added!";
        model->select();
        bumpRevision();
    }
}

Q_INVOKABLE void DBManager::removeMaterial(int row)
{
    if (!model) return;
    if (row >= 0 && row < model->rowCount()) {
        model->removeRow(row);
        model->submitAll();
        model->select();
        bumpRevision();
    }
}

Q_INVOKABLE void DBManager::updateMaterial(int row, int column, const QVariant &value) {
    if (!model) return;
    if (row >= 0 && row < model->rowCount()) {
        model->setData(model->index(row, column), value);
        model->submitAll();
        model->select();
        bumpRevision();
    }
}

void DBManager::bumpRevision()
{
    ++m_revision;
    emit revisionChanged();
}

QVariantList DBManager::elasticMatrix(int row) const
{
    QVariantList out;
    if (!model || row < 0 || row >= model->rowCount())
        return out;

    const QSqlRecord rec = model->record(row);

    // The table stores the upper triangle only (c11..c66); mirror it.
    double c[6][6] = {{0}};
    for (int i = 0; i < 6; ++i) {
        for (int j = i; j < 6; ++j) {
            const QString col = QStringLiteral("c%1%2").arg(i + 1).arg(j + 1);
            const int idx = rec.indexOf(col);
            if (idx < 0) continue;              // column missing: leave at 0
            const double v = rec.value(idx).toDouble();
            c[i][j] = v;
            c[j][i] = v;
        }
    }

    for (int i = 0; i < 6; ++i) {
        QVariantList r;
        for (int j = 0; j < 6; ++j) r.append(c[i][j]);
        out.append(QVariant(r));
    }
    return out;
}

QString DBManager::materialNameAt(int row) const
{
    if (!model || row < 0 || row >= model->rowCount())
        return QString();
    const QSqlRecord rec = model->record(row);
    const int idx = rec.indexOf(QStringLiteral("Material"));
    return (idx < 0) ? QString() : rec.value(idx).toString();
}

QVariantList DBManager::emptyElasticColumns() const
{
    QVariantList empty;
    if (!model) return empty;

    // Make sure every row is loaded: QSqlTableModel fetches lazily, so a plain
    // rowCount() on a large table only sees the first batch and would call
    // columns "empty" on the strength of the first 256 rows.
    while (model->canFetchMore()) model->fetchMore();

    const int rows = model->rowCount();
    const int cols = model->columnCount();
    if (rows <= 0) return empty;

    const QSqlRecord header = model->record();
    for (int c = 0; c < cols; ++c) {
        const QString name = header.fieldName(c);
        // Identity columns always stay: hiding them would leave rows unlabelled.
        if (name.compare(QStringLiteral("id"),       Qt::CaseInsensitive) == 0) continue;
        if (name.compare(QStringLiteral("Material"), Qt::CaseInsensitive) == 0) continue;
        if (name.compare(QStringLiteral("Type"),     Qt::CaseInsensitive) == 0) continue;

        bool allZero = true;
        for (int r = 0; r < rows && allZero; ++r) {
            bool ok = false;
            const double v = model->data(model->index(r, c)).toDouble(&ok);
            if (!ok || v != 0.0) allZero = false;
        }
        if (allZero) empty.append(c);
    }
    return empty;
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
