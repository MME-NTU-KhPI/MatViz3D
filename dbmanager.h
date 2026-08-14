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

    /**
     * @brief Bumped on every edit that changes table contents.
     *
     * QML bindings cannot track a plain Q_INVOKABLE call, so a binding that
     * wants to re-read elasticMatrix() after a cell edit has to read this
     * property too (same trick as the colormap legend in MainWindow.qml).
     */
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

private:
    QSqlDatabase db;
    QSqlTableModel *model = nullptr;
    int m_revision = 0;
    void bumpRevision();
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

    int revision() const { return m_revision; }

    /**
     * @brief Full 6x6 elastic matrix of one table row, in the table's own
     *        units (GPa) and VOIGT convention (index 4=yz, 5=xz, 6=xy).
     *
     * The table stores only the 21 independent constants c11..c66 as an upper
     * triangle; this mirrors them into a full symmetric matrix. Reads
     * model->record(row), so it reflects edits already committed by
     * updateMaterial().
     *
     * @return 6 nested QVariantLists of 6 doubles, or an empty list if the row
     *         is out of range. A brand new material is all zeros, which is not
     *         invertible -- callers must handle that rather than render NaNs.
     */
    Q_INVOKABLE QVariantList elasticMatrix(int row) const;

    /// Material name of a table row, for UI labelling. Empty if out of range.
    Q_INVOKABLE QString materialNameAt(int row) const;

    /**
     * @brief Indices of elastic-constant columns that are zero in every row.
     *
     * The table carries all 21 independent constants, but a database of cubic
     * and isotropic materials leaves most of them at zero -- 24 columns of
     * which only c11/c12/c44 ever differ from zero. The view uses this to
     * collapse the dead columns so the interesting ones fit on screen beside
     * the anisotropy panel.
     *
     * Only c11..c66 are considered; id, Material and Type are never reported,
     * so the caller can hide everything in the returned list unconditionally.
     */
    Q_INVOKABLE QVariantList emptyElasticColumns() const;

signals:
    void revisionChanged();
};

#endif // DBMANAGER_H
