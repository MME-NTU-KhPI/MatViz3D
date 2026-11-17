#ifndef NEURALGENERATORCONTROLLER_H
#define NEURALGENERATORCONTROLLER_H

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <atomic>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include "celldecoder.h"
#include "celldiffusion.h"
#include "cellpredictor.h"
#include "cellstiffness.h"
#include "printability.h"
#include "requestcheck.h"

/**
 * @brief The "Neural" tab: target properties in, a voxel cell from the network out.
 *
 * The generative model is two networks (latent diffusion). The diffusion network
 * (CellDiffusion) writes a code of 16 x 16 x 16 x 4 numbers for the target
 * properties and the material in 50 steps, and the decoder of the cell
 * autoencoder (CellDecoder) turns the code into a 64^3 cell.
 *
 * The decoder weights are built in (models/cell_decoder.bin, 2 MB). The diffusion
 * weights are 94 MB and stay a file, models/cell_diffusion.bin, looked for in
 * MATVIZ_CELL_DIFFUSION, next to the executable, in the application bundle and in
 * the source tree. Without that file the tab falls back to a random code with
 * the statistics of real codes: the targets are then collected but not used, and
 * the cell shows the decoder at work, not a design.
 *
 * A generated cell is cleared of loose pieces (optional), goes through the
 * printability check and into the 3D view as a one-material cell (grain 1 =
 * solid, 0 = void) with the material of the form.
 *
 * What the cell has against what was asked: compare() solves the last cell the
 * way the cells of the dataset were solved (cellstiff::solve: six loads, the
 * material of the form, the dataset's solver settings) in a second worker
 * thread and fills `comparison` with a row per ticked target: asked, obtained,
 * deviation. The density row is known as soon as the cell is made; a solve
 * takes minutes and can be stopped.
 *
 * Before any solve the property predictor (CellPredictor, the third network:
 * models/cell_predictor.bin, 12 MB, looked for like the diffusion weights and in
 * MATVIZ_CELL_PREDICTOR) reads the finished cell and gives its estimate of the
 * same properties in a fraction of a second: the `predicted` column of
 * `comparison`. It is an estimate with an error of its own (the note of the
 * weights file says how large), shown next to the solved values, never instead.
 *
 * checkRequest() tells, while the form is being filled in, which targets
 * cannot hold together (reqcheck::check: bounds no shape of the material can
 * pass at the asked density, and numbers that contradict each other). It warns;
 * Generate still sends the request as it is.
 */
class NeuralGeneratorController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool        available        READ available        CONSTANT)
    Q_PROPERTY(QString     modelInfo        READ modelInfo        CONSTANT)
    Q_PROPERTY(bool        diffusion        READ diffusion        CONSTANT)
    Q_PROPERTY(QString     diffusionInfo    READ diffusionInfo    CONSTANT)
    Q_PROPERTY(QString     diffusionNote    READ diffusionNote    CONSTANT)
    Q_PROPERTY(QString     diffusionMissing READ diffusionMissing CONSTANT)
    Q_PROPERTY(bool        predictor        READ predictor        CONSTANT)
    Q_PROPERTY(QString     predictorInfo    READ predictorInfo    CONSTANT)
    Q_PROPERTY(QString     predictorNote    READ predictorNote    CONSTANT)
    Q_PROPERTY(bool        busy             READ busy             NOTIFY busyChanged)
    Q_PROPERTY(int         step             READ step             NOTIFY progressChanged)
    Q_PROPERTY(int         steps            READ steps            NOTIFY progressChanged)
    Q_PROPERTY(QVariantMap report           READ report           NOTIFY reportChanged)
    Q_PROPERTY(QString     message          READ message          NOTIFY reportChanged)
    Q_PROPERTY(bool        canCompare       READ canCompare       NOTIFY comparisonChanged)
    Q_PROPERTY(QVariantList comparison      READ comparison       NOTIFY comparisonChanged)
    Q_PROPERTY(QVariantMap solveInfo        READ solveInfo        NOTIFY comparisonChanged)
    Q_PROPERTY(bool        solving          READ solving          NOTIFY solvingChanged)
    Q_PROPERTY(QString     solveProgress    READ solveProgress    NOTIFY solveProgressChanged)

public:
    explicit NeuralGeneratorController(QObject* parent = nullptr);
    ~NeuralGeneratorController() override;

    bool available() const   { return m_decoder.loaded(); }
    /// "ae1 step 16965 · 567 912 weights · code 16³ × 4 → cell 64³"
    QString modelInfo() const;
    /// The diffusion weights were found: the code is written from the targets, not drawn at random.
    bool diffusion() const   { return m_useDiffusion; }
    /// "dif2 step 30000 · 23 484 868 weights · 50 steps"
    QString diffusionInfo() const;
    /// What was measured on the cells of this model (written into the weights file at export).
    QString diffusionNote() const { return m_useDiffusion ? QString::fromStdString(m_diffusion.note()) : QString(); }
    /// Why there is no diffusion model (empty when there is one).
    QString diffusionMissing() const { return m_diffusionMissing; }
    /// The predictor weights were found: every cell gets the network's estimate of its properties.
    bool predictor() const   { return m_usePredictor; }
    /// "pred1 step 14000 · 3 076 608 weights"
    QString predictorInfo() const;
    /// What was measured on this predictor (written into the weights file at export).
    QString predictorNote() const { return m_usePredictor ? QString::fromStdString(m_predictor.note()) : QString(); }
    bool busy() const        { return m_busy; }
    /// Step of the sampling being done, from 1 (0 before the first one), and how many there are.
    int step() const         { return m_step; }
    int steps() const        { return m_steps; }
    /**
     * The last cell: seed, decode_ms, check_ms, density, bodies, islands, directions,
     * thin_pct, wall_mm, unsupported_pct, build_direction, ok, verdict, cell_mm,
     * voxel_mm, targets (how many were set), target_names, issues (the texts of
     * checkRequest() for the request of this cell); diffusion (was the code
     * written by the diffusion network). With the diffusion network: model, steps,
     * sample_ms, asked_density (when the density was a target), used / clipped /
     * unknown (the targets passed to the network; passed at the edge of the range
     * the network was trained on, with that range; not passed), cleaned,
     * removed_pieces, removed_pct (share of the solid in the pieces cleared).
     * Without it: smooth. With the predictor: predictor (its name), predict_ms,
     * predicted (its estimate of every property by its key, the density too,
     * moduli in MPa). Empty before the first cell.
     */
    QVariantMap report() const { return m_report; }
    QString message() const  { return m_message; }

    /// There is a generated cell to solve.
    bool canCompare() const  { return !m_cell.empty(); }
    /**
     * The ticked targets of the last cell next to what the cell has, one map per
     * target in the order of propsearch::Target, the density last: key ("Ex" ...
     * "nuMin", "density"), modulus (shown in MPa), relative (deviation in percent
     * of the asked value; false: obtained minus asked, for Poisson's ratios and
     * A^U), asked, obtained, deviation, and the predictor's estimate with its own
     * deviation from the asked value: predicted, predicted_deviation (NaN without
     * the predictor). Before the cell is solved only the density row has its
     * obtained value, the others hold NaN.
     */
    QVariantList comparison() const { return m_comparison; }
    /**
     * The solve behind `comparison`, empty until there is one. state: "done";
     * "first" while a second solve with a softer void is running (the values are
     * those of the first one); "stopped" when that second solve was stopped (the
     * same values stay). seconds, iterations (all loads of all solves), threads,
     * grid, void_eta and share of the solve the values come from (share: the
     * smallest share of the strain energy the solid holds over the six loads),
     * share_first, passes, converged, load_path (share at least 0.9: there is a
     * solid path for every load), values (every property of propsearch::Target by
     * its key, moduli in MPa). error: why a solve gave nothing or broke off.
     */
    QVariantMap solveInfo() const { return m_solveInfo; }
    bool solving() const     { return m_solving; }
    /// "load 3 of 6 · iteration 142 · error 3.2e-04 · 42 s" while solving.
    QString solveProgress() const { return m_solveProgress; }

    /**
     * Makes one cell in a worker thread and shows it. `condition`: material
     * {e_gpa, nu, aniso}; targets {Ex, ..., zener} for the ticked ones (the keys
     * of PropertySelectorController::search, moduli in MPa); density (optional);
     * cell_mm (edge of the cell, sets the voxel size for the printability check);
     * seed (optional, 0 = a new one): of the starting noise; clean (default true:
     * keep the largest body only). Without the diffusion network: smooth (blur of
     * the random code, in code positions).
     */
    Q_INVOKABLE void generate(const QVariantMap& condition);
    /**
     * The conflicts between the targets of `condition` (the map generate() takes):
     * one map per conflict, {keys: the targets involved (the request's keys, the
     * density as "density"), text: what is wrong, with the numbers}. Empty: none.
     * No data is read; it takes microseconds and is meant to run on every change
     * of the form.
     */
    Q_INVOKABLE QVariantList checkRequest(const QVariantMap& condition) const;
    /**
     * Solves the last cell in a worker thread with the settings of the dataset
     * and Parameters::num_threads threads, and fills `comparison` and `solveInfo`.
     * Nothing happens while a cell is being made or solved; no cell is made
     * while one is being solved.
     */
    Q_INVOKABLE void compare();
    /// Stops the solve within one solver iteration.
    Q_INVOKABLE void cancelCompare() { m_solveCancel = true; }
    /// --neural_compare: solve the next cell as soon as it is made.
    void setCompareNext(bool on) { m_compareNext = on; }
    /// Asks the main window to show the tab and generate the cell of `seed` (0 = a new one).
    void requestShow(quint32 seed) { emit showRequested(seed); }
    /// Called after a cell went into the 3D view (the HDF5 view must forget which file's geometry was there).
    void setAfterShow(std::function<void()> f) { m_afterShow = std::move(f); }

    const CellDecoder& decoder() const { return m_decoder; }

signals:
    void busyChanged();
    void progressChanged();
    void reportChanged();
    void showRequested(quint32 seed);
    void comparisonChanged();
    void solvingChanged();
    void solveProgressChanged();
    /// A solve started by compare() is over, whatever it gave.
    void compareFinished();

private:
    struct Result {
        int32_t*** voxels = nullptr;   ///< owned until it becomes Parameters::voxels; null when no cell was made
        int n = 0;
        PrintabilityReport print;
        double sampleMs = 0.0, decodeMs = 0.0, checkMs = 0.0;
        bool usedDiffusion = false;
        int steps = 0;
        size_t solidVoxels = 0, removedVoxels = 0;   ///< solid before the cleaning, and what it cleared
        int removedPieces = 0;
        std::map<std::string, double> predicted;     ///< the predictor's estimate of the cell as shown (moduli in Pa); empty without it
        double predictMs = 0.0;
        QString error;                 ///< why no cell was made (empty when the sampling was stopped by the program closing)
    };

    void finish();
    void finishCompare();
    /// The targets of a condition in the units of the form (moduli in MPa), the density as "density", and the material.
    static std::map<std::string, double> formTargets(const QVariantMap& condition, reqcheck::Material* material, reqcheck::Form* form = nullptr);
    void setRows(const propsearch::Values* obtained);
    void showSolution(const cellstiff::Solution& solution, const QString& state);

    CellDecoder m_decoder;
    CellDiffusion m_diffusion;
    bool m_useDiffusion = false;
    QString m_diffusionMissing;
    CellPredictor m_predictor;
    bool m_usePredictor = false;
    QFutureWatcher<Result> m_watcher;
    std::atomic<bool> m_cancel { false };   ///< set by the destructor: the sampling stops at its next step
    std::function<void()> m_afterShow;
    QVariantMap m_condition;   ///< of the cell being made
    QStringList m_used, m_clipped, m_unknown;   ///< what the request did with the targets (see report())
    QStringList m_issues;                       ///< the conflicts between the targets of the cell being made (checkRequest)
    quint32 m_seed = 0;
    double m_smooth = 0.0, m_cellMm = 12.0;
    bool m_clean = true;
    bool m_busy = false;
    bool m_showWaiting = false;   ///< the cell is made and waits for the grid to be free (Parameters::whenVoxelsFree)
    int m_step = 0, m_steps = 0;
    QVariantMap m_report;
    QString m_message;

    // The last cell as it was made, for compare(): a copy, the 3D view may hold another structure by then.
    std::vector<uint8_t> m_cell;               ///< cellstiff::occupancy of the cell; empty before the first one
    int m_cellN = 0;
    double m_cellDensity = 0.0;
    propsearch::Law m_cellLaw;                 ///< the material it was asked for
    std::map<std::string, double> m_asked;     ///< the ticked targets, in the units of the form
    std::map<std::string, double> m_predicted; ///< the predictor's estimate of the cell (moduli in Pa); empty without the predictor
    double m_askedDensity = std::numeric_limits<double>::quiet_NaN();   ///< NaN: the density was not a target
    QFutureWatcher<cellstiff::Solution> m_solveWatcher;
    std::atomic<bool> m_solveCancel { false };
    QElapsedTimer m_solveClock;
    bool m_solving = false;
    bool m_compareNext = false;
    int m_solveThreads = 0;
    QString m_solveProgress;
    QVariantList m_comparison;
    QVariantMap m_solveInfo;
};

#endif // NEURALGENERATORCONTROLLER_H
