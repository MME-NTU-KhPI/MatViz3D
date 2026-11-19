#ifndef DATASETBROWSERCONTROLLER_H
#define DATASETBROWSERCONTROLLER_H

#include <QAbstractListModel>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <functional>
#include <vector>

/**
 * @brief Rows of a dataset manifest (tools/dataset/make_dataset.py) as a list
 *        model: one row per planned cell, filtered by the browser.
 *
 * Only what the list shows is parsed up front; the full CSV line is kept and
 * parsed when a row is selected, so a 100 000-row manifest stays small.
 */
class DatasetRowsModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        IdRole = Qt::UserRole + 1, GroupRole, FamilyRole, StatusRole,
        DensityRole, CellMmRole, GridRole, HasFileRole
    };
    struct Row {
        QString id, group, family, status;
        double  density = 0.0;
        double  cellMm  = 0.0;
        int     grid    = 0;
        bool    hasFile = false;
        QString file;           ///< the cell's HDF5, when known (single-file mode)
        QString line;           ///< the raw CSV line, parsed on selection
    };

    explicit DatasetRowsModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setView(std::vector<const Row*> view);
    const Row* at(int i) const { return (i >= 0 && i < static_cast<int>(m_view.size())) ? m_view[static_cast<size_t>(i)] : nullptr; }
    int size() const { return static_cast<int>(m_view.size()); }

private:
    std::vector<const Row*> m_view;
};

/**
 * @brief Browser for the cells of a dataset folder (manifest.csv + cells/<id>.hdf5).
 *
 * Opens a dataset folder (or a cell file / the manifest inside one), lists its
 * rows with group / status / id filters, and on selection loads the cell's
 * HDF5 through Hdf5ProjectController, which puts the voxels into the 3D view,
 * and lays out its parameters by section: the manifest row, the geometry
 * parameters that matter for the cell's family, the material law, the
 * printability report and the effective stiffness.
 *
 * Opening the cell (into the 3D view) is delegated to a callback set by main.cpp
 * (Hdf5ProjectController::openFile), so the controller has no GUI dependency.
 */
class DatasetBrowserController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString     folder        READ folder        NOTIFY datasetChanged)
    Q_PROPERTY(bool        isOpen        READ isOpen        NOTIFY datasetChanged)
    Q_PROPERTY(QObject*    rows          READ rows          CONSTANT)
    Q_PROPERTY(int         totalRows     READ totalRows     NOTIFY datasetChanged)
    Q_PROPERTY(int         shownRows     READ shownRows     NOTIFY filterChanged)
    Q_PROPERTY(QStringList groups        READ groups        NOTIFY datasetChanged)
    Q_PROPERTY(QStringList statuses      READ statuses      CONSTANT)
    Q_PROPERTY(QString     groupFilter   READ groupFilter   WRITE setGroupFilter  NOTIFY filterChanged)
    Q_PROPERTY(QString     statusFilter  READ statusFilter  WRITE setStatusFilter NOTIFY filterChanged)
    Q_PROPERTY(QString     search        READ search        WRITE setSearch       NOTIFY filterChanged)
    Q_PROPERTY(int         currentIndex  READ currentIndex  NOTIFY currentChanged)
    Q_PROPERTY(QString     currentId     READ currentId     NOTIFY currentChanged)
    Q_PROPERTY(QString     currentTitle  READ currentTitle  NOTIFY currentChanged)
    Q_PROPERTY(QString     currentFile   READ currentFile   NOTIFY currentChanged)
    Q_PROPERTY(QVariantList sections     READ sections      NOTIFY currentChanged)
    Q_PROPERTY(QString     message       READ message       NOTIFY currentChanged)

public:
    explicit DatasetBrowserController(QObject* parent = nullptr);

    QString folder() const          { return m_folder; }
    bool isOpen() const             { return !m_all.empty(); }
    QObject* rows()                 { return &m_model; }
    int totalRows() const           { return static_cast<int>(m_all.size()); }
    int shownRows() const           { return m_model.size(); }
    QStringList groups() const      { return m_groups; }
    QStringList statuses() const;
    QString groupFilter() const     { return m_groupFilter; }
    QString statusFilter() const    { return m_statusFilter; }
    QString search() const          { return m_search; }
    int currentIndex() const        { return m_current; }
    QString currentId() const       { return m_currentId; }
    QString currentTitle() const    { return m_currentTitle; }
    QString currentFile() const     { return m_currentFile; }
    QVariantList sections() const   { return m_sections; }
    QString message() const         { return m_message; }

    void setGroupFilter(const QString& g);
    void setStatusFilter(const QString& s);
    void setSearch(const QString& s);

    /// A dataset folder, its manifest.csv, or a cell file inside its cells/ folder.
    Q_INVOKABLE bool openPath(const QString& path);
    Q_INVOKABLE bool openUrl(const QUrl& url) { return openPath(url.toLocalFile()); }
    Q_INVOKABLE void select(int index);
    /// Selects the row with this id, relaxing the filters if they hide it.
    Q_INVOKABLE bool selectId(const QString& id);
    Q_INVOKABLE void next();
    Q_INVOKABLE void prev();
    Q_INVOKABLE void reload();
    /// Ask the main window to show the browser (used by `--browse`).
    Q_INVOKABLE void requestShow() { emit showRequested(); }

    /// Every row of the open dataset, unfiltered (the property selector indexes them).
    const std::vector<DatasetRowsModel::Row>& allRows() const { return m_all; }
    /// The HDF5 file of a row, empty when it has none.
    QString cellFile(const DatasetRowsModel::Row& row) const;

    /// Opens a cell file in the 3D view; returns false when it could not.
    void setCellOpener(std::function<bool(const QString&)> opener) { m_opener = std::move(opener); }

    /// The "Stiffness matrix" section of the cell panel from C (Pa) and S (1/Pa), both in the
    /// pipeline order xx yy zz xy yz xz with engineering shear strains (C44 = G xy):
    ///   grid / gridFaint / gridLabels  C as 36 strings for a 6 x 6 grid, in one unit (MPa while the
    ///                                  largest entry is below 10 GPa, else GPa) with the decimals
    ///                                  that give that entry 4 significant digits; an entry that
    ///                                  rounds to zero is "0" and faint;
    ///   unit                           "MPa" or "GPa";
    ///   voigtC / voigtS                C in that unit and S in 1 / that unit, 6 rows of 6 numbers,
    ///                                  reordered to the Voigt order of the material matrix window
    ///                                  (xx yy zz yz xz xy), for its "Open as matrix" view.
    static QVariantMap matrixSection(const double C[6][6], const double S[6][6]);

signals:
    void showRequested();
    void datasetChanged();
    void filterChanged();
    void currentChanged();

private:
    void applyFilter(const QString& keepId = QString());
    void buildSections(const DatasetRowsModel::Row& row);

    DatasetRowsModel m_model;
    std::vector<DatasetRowsModel::Row> m_all;
    QStringList m_header;
    QString m_folder;
    QString m_lawRanges;    ///< the ranges the dataset draws its material laws from (meta.json), for the Material section
    QStringList m_groups;
    QString m_groupFilter  = QStringLiteral("All groups");
    QString m_statusFilter = QStringLiteral("With cell file");
    QString m_search;
    int m_current = -1;
    QString m_currentId, m_currentTitle, m_currentFile, m_message;
    QVariantList m_sections;
    std::function<bool(const QString&)> m_opener;
};

#endif // DATASETBROWSERCONTROLLER_H
