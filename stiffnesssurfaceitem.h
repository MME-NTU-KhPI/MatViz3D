// ============================================================================
//  stiffnesssurfaceitem.h  -  QML item hosting the elastic directional surface.
//
//  Declarative by design: it takes the 6x6 matrix as a plain QVariantList
//  property, so QML can bind it to either dbManager.elasticMatrix(row) or
//  stressAnalysisController.stiffnessC without any C++ knowing about the other.
//  No singleton, no cross-object pointers, no lifetime coupling.
//
//  matrixBasis is not optional bookkeeping: three different 6x6 conventions
//  coexist in this codebase (Voigt/GPa in the material table, pipeline/Pa in
//  StiffnessMatrixResult, Mandel internally), and this property is what keeps
//  the distinction explicit at the API boundary instead of implicit.
// ============================================================================
#pragma once

#include <QQuickFramebufferObject>
#include <QVariantList>
#include <QVariantMap>
#include <QPoint>
#include <QTimer>

#include "stiffnesssurfacebuilder.h"

class StiffnessSurfaceItem : public QQuickFramebufferObject
{
    Q_OBJECT
    QML_ELEMENT

    /// Row-major 6x6 as a list of 6 lists of 6 numbers. Empty clears the view.
    Q_PROPERTY(QVariantList matrix READ matrix WRITE setMatrix NOTIFY matrixChanged)
    /// 0 = Voigt/GPa (material database), 1 = pipeline/Pa (solver result).
    Q_PROPERTY(int matrixBasis READ matrixBasis WRITE setMatrixBasis NOTIFY matrixChanged)

    Q_PROPERTY(int    quantity   READ quantity   WRITE setQuantity   NOTIFY paramsChanged)
    Q_PROPERTY(int    componentI READ componentI WRITE setComponentI NOTIFY paramsChanged)
    Q_PROPERTY(int    componentJ READ componentJ WRITE setComponentJ NOTIFY paramsChanged)
    Q_PROPERTY(double spinDeg    READ spinDeg    WRITE setSpinDeg    NOTIFY paramsChanged)
    Q_PROPERTY(bool   extremumOverSpin READ extremumOverSpin WRITE setExtremumOverSpin NOTIFY paramsChanged)
    Q_PROPERTY(int    palette    READ palette    WRITE setPalette    NOTIFY paramsChanged)
    Q_PROPERTY(bool   wireframe  READ wireframe  WRITE setWireframe  NOTIFY paramsChanged)
    Q_PROPERTY(bool   showAxes   READ showAxes   WRITE setShowAxes   NOTIFY paramsChanged)

    // --- read-only results ---
    Q_PROPERTY(bool    valid           READ valid           NOTIFY statsChanged)
    Q_PROPERTY(QString errorMessage    READ errorMessage    NOTIFY statsChanged)
    Q_PROPERTY(double  minValue        READ minValue        NOTIFY statsChanged)
    Q_PROPERTY(double  maxValue        READ maxValue        NOTIFY statsChanged)
    Q_PROPERTY(double  anisotropyRatio READ anisotropyRatio NOTIFY statsChanged)
    Q_PROPERTY(double  zener           READ zener           NOTIFY statsChanged)
    Q_PROPERTY(bool    isCubic         READ isCubic         NOTIFY statsChanged)
    Q_PROPERTY(double  bulkModulus     READ bulkModulus     NOTIFY statsChanged)
    Q_PROPERTY(QString unit            READ unit            NOTIFY statsChanged)

public:
    explicit StiffnessSurfaceItem(QQuickItem* parent = nullptr);

    Renderer* createRenderer() const override;

    QVariantList matrix() const { return m_matrix; }
    void setMatrix(const QVariantList& m);
    int  matrixBasis() const { return m_basis; }
    void setMatrixBasis(int b);

    int    quantity() const { return int(m_params.quantity); }
    void   setQuantity(int q);
    int    componentI() const { return m_params.ci; }
    void   setComponentI(int i);
    int    componentJ() const { return m_params.cj; }
    void   setComponentJ(int j);
    double spinDeg() const { return m_params.spinDeg; }
    void   setSpinDeg(double d);
    bool   extremumOverSpin() const { return m_params.extremumOverSpin; }
    void   setExtremumOverSpin(bool e);
    int    palette() const { return int(m_params.palette); }
    void   setPalette(int p);
    bool   wireframe() const { return m_wireframe; }
    void   setWireframe(bool w);
    bool   showAxes() const { return m_showAxes; }
    void   setShowAxes(bool s);

    bool    valid() const { return m_mesh.valid; }
    QString errorMessage() const { return m_error; }
    double  minValue() const { return m_mesh.minValue; }
    double  maxValue() const { return m_mesh.maxValue; }
    double  anisotropyRatio() const { return m_mesh.anisotropyRatio; }
    double  zener() const { return m_state.zener; }
    bool    isCubic() const { return m_state.cubic; }
    double  bulkModulus() const { return m_state.bulkVRH; }
    QString unit() const { return QString::fromLatin1(m_mesh.unit); }

    /// Names for the quantity combo box, in enum order.
    Q_INVOKABLE static QStringList quantityNames();
    /// Everything the readout panel needs, in one call.
    Q_INVOKABLE QVariantMap stats() const;
    /// Reset the camera to the default three-quarter view.
    Q_INVOKABLE void resetView();

    // --- read by the renderer in synchronize() ---
    const mvsurf::Mesh& mesh() const { return m_mesh; }
    bool  takeMeshDirty();
    float xRot() const { return m_xRot; }
    float yRot() const { return m_yRot; }
    float zRot() const { return m_zRot; }
    float distance() const { return m_distance; }

signals:
    void matrixChanged();
    void paramsChanged();
    void statsChanged();

protected:
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;

private:
    void scheduleRebuild();
    void rebuild();

    QVariantList m_matrix;
    int          m_basis = int(mvsurf::Basis::VoigtGPa);

    mvsurf::Params      m_params;
    mvsurf::ElasticState m_state;
    mvsurf::Mesh        m_mesh;
    QString             m_error;
    bool                m_meshDirty = false;

    // Rebuilding is ~1 ms, but a slider drag fires far faster than the frame
    // rate; coalescing keeps the UI thread responsive without threading.
    QTimer m_rebuildTimer;

    float  m_xRot = -60.0f, m_yRot = 0.0f, m_zRot = 30.0f;
    float  m_distance = 3.2f;
    QPoint m_lastPos;
    bool   m_wireframe = false;
    bool   m_showAxes = true;
};
