#ifndef PROPERTYSELECTORCONTROLLER_H
#define PROPERTYSELECTORCONTROLLER_H

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <vector>

#include "propertysearch.h"

class DatasetBrowserController;

/**
 * @brief The "Selector" tab: target properties in, the closest cells of the
 *        open dataset out (propertysearch.h does the ranking).
 *
 * Works on the dataset the Dataset tab has open. The accepted cells are read
 * once into an index (stiffness, law, density, wall) in batches on the GUI
 * thread, so HDF5 is never touched from two threads; the index is cached per
 * dataset folder under the user cache directory and only new or changed cell
 * files are read again. Showing a result selects that cell in the browser,
 * which puts it into the 3D view and its parameters into the cell panel.
 */
class PropertySelectorController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool         hasDataset  READ hasDataset  NOTIFY indexChanged)
    Q_PROPERTY(QString      datasetName READ datasetName NOTIFY indexChanged)
    Q_PROPERTY(bool         ready       READ ready       NOTIFY indexChanged)
    Q_PROPERTY(bool         indexing    READ indexing    NOTIFY indexChanged)
    Q_PROPERTY(int          indexed     READ indexed     NOTIFY progressChanged)
    Q_PROPERTY(int          toIndex     READ toIndex     NOTIFY progressChanged)
    Q_PROPERTY(int          cellCount   READ cellCount   NOTIFY indexChanged)
    Q_PROPERTY(QVariantList families    READ families    NOTIFY indexChanged)
    Q_PROPERTY(QVariantList results     READ results     NOTIFY resultsChanged)
    Q_PROPERTY(int          currentResult READ currentResult NOTIFY currentChanged)
    Q_PROPERTY(QString      message     READ message     NOTIFY resultsChanged)

public:
    explicit PropertySelectorController(DatasetBrowserController* browser, QObject* parent = nullptr);

    bool hasDataset() const;
    QString datasetName() const;
    bool ready() const              { return m_ready; }
    bool indexing() const           { return m_indexing; }
    int indexed() const             { return static_cast<int>(m_next); }
    int toIndex() const             { return static_cast<int>(m_pending.size()); }
    int cellCount() const           { return static_cast<int>(m_cells.size()); }
    QVariantList families() const   { return m_families; }
    QVariantList results() const    { return m_results; }
    int currentResult() const       { return m_current; }
    QString message() const         { return m_message; }

    /// Reads the accepted cells of the open dataset, in batches; cached files are not re-read.
    Q_INVOKABLE void buildIndex();
    /// The same, in one go (tests, scripts).
    void buildIndexNow();
    /**
     * Ranks the indexed cells. `query`: material {e_gpa, nu, aniso}; targets
     * {Ex, Ey, Ez, Gxy, Gyz, Gxz, Emean, Gmean, K (MPa), nuXy, nuXz, nuYz, nuMin,
     * EzEx, EyEx, EzEy, GxzGxy, GyzGxy, GyzGxz, EmaxEmin, GmaxGmin, AU, zener}
     * for the ticked ones only; rho_min, rho_max; family (key, "" = all); count.
     */
    Q_INVOKABLE void search(const QVariantMap& query);
    /// Opens result `i` in the browser (3D view + cell panel).
    Q_INVOKABLE void show(int i);
    /**
     * What the indexed cells span, for the sliders, each as [low, high]: E, G and
     * K in MPa from the 0.5th percentile with a material of E = `eLowGpa` to the
     * 99.5th with E = `eHighGpa`; nu, nu min ("nuMin"), the density, the three E
     * ratios of the axes together ("axisRatio"), the three G ratios together
     * ("shearRatio"), E max/E min and G max/G min from 1 ("allDir", "allDirG"),
     * A^U from 0 ("aU") and the Zener ratio of the near-cubic cells ("zener")
     * between the same percentiles.
     */
    Q_INVOKABLE QVariantMap ranges(double eLowGpa, double eHighGpa) const;
    /**
     * --select <spec>: asks the window to show the Selector tab and run this search
     * (PropertySelectorPanel.applySpec lists the keys). The dataset comes from --browse.
     */
    void requestShow(const QVariantMap& spec) { emit showRequested(spec); }

    const std::vector<propsearch::Cell>& cells() const { return m_cells; }
    const std::vector<propsearch::Match>& matches() const { return m_matches; }

signals:
    void indexChanged();
    void progressChanged();
    void resultsChanged();
    void currentChanged();
    void showRequested(const QVariantMap& spec);

private:
    struct Pending { QString id, group, family, file; double cellMm = 0.0; };
    struct Cached { qint64 mtime = 0, size = 0; propsearch::Cell cell; };

    void reset();
    void prepare();
    bool indexOne(const Pending& p);
    void finish();
    QString cachePath() const;
    void loadCache();
    void saveCache() const;

    DatasetBrowserController* m_browser = nullptr;
    QString m_folder;
    std::vector<propsearch::Cell> m_cells;
    std::vector<propsearch::Values> m_own;   ///< each cell's moduli with its own law
    std::vector<Pending> m_pending;
    size_t m_next = 0;
    int m_failed = 0;
    bool m_ready = false;
    bool m_indexing = false;
    bool m_preparing = false;   ///< reset() inside prepare() must not start another index
    QHash<QString, Cached> m_cache;
    QTimer m_batch;
    QVariantList m_families;
    std::vector<propsearch::Match> m_matches;
    QVariantList m_results;
    int m_current = -1;
    QString m_message;
};

#endif // PROPERTYSELECTORCONTROLLER_H
