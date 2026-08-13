#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QObject>
#include <QVariant>
#include <QStringList>
#include <QSqlDatabase>
#include <QSqlTableModel>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>

class DBManager : public QObject
{
    Q_OBJECT
private:
    QSqlDatabase db;
    QSqlTableModel *model = nullptr;
    static void createTable(QSqlDatabase& db);
    static void insertInitialData(QSqlDatabase& db);
public:
    explicit DBManager(QObject *parent = nullptr);
    ~DBManager();

    /**
     * @brief Opens (once) the shared material_properties.db connection and
     *        makes sure the table exists and is populated.
     *
     * Callable without a DBManager instance -- headless runs never construct
     * one, but --db_material still has to resolve. Returns an invalid database
     * if SQLite is unavailable.
     */
    static QSqlDatabase materialDatabase();

    /// Material names in table order, for the "materials" schema provider.
    static QStringList materialNames();

    /**
     * @brief Cubic single-crystal constants of one material, in GPa (the unit
     *        the table stores).
     * @return false if the material is not in the database, leaving the
     *         out-parameters untouched.
     */
    static bool cubicConstants(const QString& material,
                               double& c11, double& c12, double& c44,
                               QString& type);

    Q_INVOKABLE void addMaterial(const QString &material);
    Q_INVOKABLE void removeMaterial(int row);
    Q_INVOKABLE void updateMaterial(int row, int column, const QVariant &value);
    Q_INVOKABLE QSqlTableModel* getModel() { return model; };
    Q_INVOKABLE QVariantList executeSelectQuery(const QString& queryString);
};

#endif // DBMANAGER_H
