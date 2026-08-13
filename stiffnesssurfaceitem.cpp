#include "stiffnesssurfaceitem.h"
#include "stiffnesssurfacerenderer.h"

#include <QMouseEvent>
#include <QWheelEvent>
#include <QStringList>
#include <QDebug>

#include <algorithm>
#include <cmath>

StiffnessSurfaceItem::StiffnessSurfaceItem(QQuickItem* parent)
    : QQuickFramebufferObject(parent)
{
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    setMirrorVertically(true);   // FBO origin is bottom-left, Quick's is top-left

    m_rebuildTimer.setSingleShot(true);
    m_rebuildTimer.setInterval(60);
    connect(&m_rebuildTimer, &QTimer::timeout, this, &StiffnessSurfaceItem::rebuild);
}

QQuickFramebufferObject::Renderer* StiffnessSurfaceItem::createRenderer() const
{
    // A fresh renderer every time, owned by Qt. Deliberately not the static
    // single-instance pattern used by OpenGLWidgetQML -- that is exactly what
    // prevents a second view from existing.
    return new StiffnessSurfaceRenderer;
}

// ---------------------------------------------------------------------------
//  Inputs
// ---------------------------------------------------------------------------
void StiffnessSurfaceItem::setMatrix(const QVariantList& m)
{
    m_matrix = m;
    emit matrixChanged();
    scheduleRebuild();
}

void StiffnessSurfaceItem::setMatrixBasis(int b)
{
    b = std::clamp(b, 0, 1);
    if (m_basis == b) return;
    m_basis = b;
    emit matrixChanged();
    scheduleRebuild();
}

void StiffnessSurfaceItem::setQuantity(int q)
{
    q = std::clamp(q, 0, int(mvsurf::Quantity::S_component));
    if (int(m_params.quantity) == q) return;
    m_params.quantity = static_cast<mvsurf::Quantity>(q);
    emit paramsChanged();
    scheduleRebuild();
}

void StiffnessSurfaceItem::setComponentI(int i)
{
    i = std::clamp(i, 0, 5);
    if (m_params.ci == i) return;
    m_params.ci = i;
    emit paramsChanged();
    scheduleRebuild();
}

void StiffnessSurfaceItem::setComponentJ(int j)
{
    j = std::clamp(j, 0, 5);
    if (m_params.cj == j) return;
    m_params.cj = j;
    emit paramsChanged();
    scheduleRebuild();
}

void StiffnessSurfaceItem::setSpinDeg(double d)
{
    if (qFuzzyCompare(m_params.spinDeg, d)) return;
    m_params.spinDeg = d;
    emit paramsChanged();
    scheduleRebuild();
}

void StiffnessSurfaceItem::setExtremumOverSpin(bool e)
{
    if (m_params.extremumOverSpin == e) return;
    m_params.extremumOverSpin = e;
    emit paramsChanged();
    scheduleRebuild();
}

void StiffnessSurfaceItem::setPalette(int p)
{
    p = std::clamp(p, 0, matviz_cmap::kPaletteCount - 1);
    if (int(m_params.palette) == p) return;
    m_params.palette = static_cast<matviz_cmap::Palette>(p);
    emit paramsChanged();
    scheduleRebuild();
}

void StiffnessSurfaceItem::setWireframe(bool w)
{
    if (m_wireframe == w) return;
    m_wireframe = w;
    emit paramsChanged();
    update();          // render-state only: no rebuild needed
}

void StiffnessSurfaceItem::setShowAxes(bool s)
{
    if (m_showAxes == s) return;
    m_showAxes = s;
    emit paramsChanged();
    update();
}

// ---------------------------------------------------------------------------
//  Rebuild
// ---------------------------------------------------------------------------
void StiffnessSurfaceItem::scheduleRebuild()
{
    m_rebuildTimer.start();
}

void StiffnessSurfaceItem::rebuild()
{
    m_error.clear();

    if (m_matrix.size() != 6) {
        m_state = mvsurf::ElasticState{};
        m_mesh  = mvsurf::Mesh{};
        m_error = m_matrix.isEmpty() ? tr("No material selected")
                                     : tr("Expected a 6x6 matrix");
        m_meshDirty = true;
        emit statsChanged();
        update();
        return;
    }

    double M[6][6] = {{0}};
    for (int i = 0; i < 6; ++i) {
        const QVariantList row = m_matrix.at(i).toList();
        if (row.size() != 6) {
            m_state = mvsurf::ElasticState{};
            m_mesh  = mvsurf::Mesh{};
            m_error = tr("Expected a 6x6 matrix");
            m_meshDirty = true;
            emit statsChanged();
            update();
            return;
        }
        for (int j = 0; j < 6; ++j) M[i][j] = row.at(j).toDouble();
    }

    if (!mvsurf::buildElasticState(M, static_cast<mvsurf::Basis>(m_basis), m_state)) {
        m_mesh = mvsurf::Mesh{};
        // A brand new material row is all zeros, so this is a normal state to
        // be in, not an exceptional one -- say so plainly instead of drawing
        // NaN geometry.
        m_error = tr("Elastic matrix is singular (all-zero or degenerate row)");
        m_meshDirty = true;
        emit statsChanged();
        update();
        return;
    }

    m_mesh = mvsurf::buildSurface(m_state, m_params);
    if (!m_mesh.valid)
        m_error = tr("Selected quantity is identically zero");

    qDebug().noquote()
        << QString("[StiffnessSurfaceItem] %1: min=%2 max=%3 %4  anisotropy=%5"
                   "  cubic=%6 zener=%7  verts=%8 tris=%9")
               .arg(QString::fromLatin1(mvsurf::quantityName(m_params.quantity)))
               .arg(m_mesh.minValue, 0, 'g', 6).arg(m_mesh.maxValue, 0, 'g', 6)
               .arg(QString::fromLatin1(m_mesh.unit))
               .arg(m_mesh.anisotropyRatio, 0, 'g', 5)
               .arg(m_state.cubic ? "yes" : "no")
               .arg(m_state.zener, 0, 'g', 5)
               .arg(m_mesh.verts.size()).arg(m_mesh.indices.size() / 3);

    m_meshDirty = true;
    emit statsChanged();
    update();
}

bool StiffnessSurfaceItem::takeMeshDirty()
{
    const bool d = m_meshDirty;
    m_meshDirty = false;
    return d;
}

// ---------------------------------------------------------------------------
//  UI helpers
// ---------------------------------------------------------------------------
QStringList StiffnessSurfaceItem::quantityNames()
{
    QStringList names;
    for (int q = 0; q <= int(mvsurf::Quantity::S_component); ++q)
        names << QString::fromLatin1(mvsurf::quantityName(static_cast<mvsurf::Quantity>(q)));
    return names;
}

QVariantMap StiffnessSurfaceItem::stats() const
{
    QVariantMap m;
    m["valid"]           = m_mesh.valid;
    m["error"]           = m_error;
    m["min"]             = m_mesh.minValue;
    m["max"]             = m_mesh.maxValue;
    m["anisotropyRatio"] = m_mesh.anisotropyRatio;
    m["unit"]            = QString::fromLatin1(m_mesh.unit);
    m["cubic"]           = m_state.cubic;
    m["zener"]           = m_state.zener;
    m["bulkVRH"]         = m_state.bulkVRH;
    return m;
}

void StiffnessSurfaceItem::resetView()
{
    m_xRot = -60.0f; m_yRot = 0.0f; m_zRot = 30.0f;
    m_distance = 3.2f;
    update();
}

// ---------------------------------------------------------------------------
//  Camera interaction -- same feel as the main 3D view
// ---------------------------------------------------------------------------
void StiffnessSurfaceItem::mousePressEvent(QMouseEvent* e)
{
    m_lastPos = e->pos();
}

void StiffnessSurfaceItem::mouseMoveEvent(QMouseEvent* e)
{
    const int dx = e->pos().x() - m_lastPos.x();
    const int dy = e->pos().y() - m_lastPos.y();

    if (e->buttons() & Qt::LeftButton) {
        m_xRot += 0.5f * dy;
        m_zRot += 0.5f * dx;
    } else if (e->buttons() & Qt::RightButton) {
        m_xRot += 0.5f * dy;
        m_yRot += 0.5f * dx;
    }
    m_lastPos = e->pos();
    update();
}

void StiffnessSurfaceItem::wheelEvent(QWheelEvent* e)
{
    const float steps = float(e->angleDelta().y()) / 120.0f;
    m_distance = std::clamp(m_distance - steps * 0.2f, 1.4f, 12.0f);
    update();
}
