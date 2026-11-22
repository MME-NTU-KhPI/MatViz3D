#include "openglwidgetqml.h"
#include "qquickwindow.h"
#include "renderopengl.h"
#include "ansyswrapper.h"
#include "exportcontroller.h"
#include "stressanalysiscontroller.h"

#include <QGuiApplication>
#include <QClipboard>
#include <QTimer>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <memory>
#include <QtConcurrent>
#include <QImage>
#include <QThread>
#include <QtMath>
#include <algorithm>

// QQuickFramebufferObject hands its FBO texture to the scene graph flipped
// vertically, so NDC +Y ends up pointing DOWN on screen. Flip to false if the
// labels come out mirrored about the triad's horizontal axis.
static constexpr bool kGizmoYFlip = true;

RenderOpenGL* OpenGLWidgetQML::m_render = nullptr;

// Corner nodes of each cube face, in the order the face is drawn (CCW seen
// from outside). Node l sits at (l >= 4, l in {2,3,6,7}, l in {1,2,5,6}).
// Faces: -X, +Y, +X, -Y, +Z, -Z (the order of drawCube's normals).
static const int kCubeFaces[6][4] =
    {
        {0, 1, 2, 3},
        {3, 2, 6, 7},
        {7, 6, 5, 4},
        {4, 5, 1, 0},
        {5, 6, 2, 1},
        {7, 4, 0, 3}
    };
OpenGLWidgetQML* OpenGLWidgetQML::instance = nullptr;

OpenGLWidgetQML::OpenGLWidgetQML(QQuickItem *parent) : QQuickFramebufferObject(parent) {
    setAcceptedMouseButtons(Qt::AllButtons);
    setTextureFollowsItemSize(true);

    numCubes = 1;
    panX = 0.0f;
    panY = 0.0f;
    distance = calculateFitDistance();
    setDimetricDownView();
    voxels = nullptr;
    timer = new QTimer(this);
    delayAnimation = 0;
    bgColor.setRgbF(0.21f, 0.21f, 0.21f);
    connect(this, &QQuickItem::widthChanged, this, &OpenGLWidgetQML::handleResize, Qt::QueuedConnection);
    connect(this, &QQuickItem::heightChanged, this, &OpenGLWidgetQML::handleResize, Qt::QueuedConnection);

    // Label positions depend on the item size, not just on rotation.
    connect(this, &QQuickItem::widthChanged,  this, &OpenGLWidgetQML::axisLabelsChanged);
    connect(this, &QQuickItem::heightChanged, this, &OpenGLWidgetQML::axisLabelsChanged);

    // Always keep instance pointing to active OpenGLWidgetQML
    instance = this;
}

OpenGLWidgetQML* OpenGLWidgetQML::getInstance()
{
    return instance;
}

QQuickFramebufferObject::Renderer *OpenGLWidgetQML::createRenderer() const
{
    if (m_render)
    {
        delete m_render;
    }
    m_render = new RenderOpenGL();
    configureRenderer(m_render);

    // A fresh renderer starts with default overlay state and an empty glyph
    // buffer, so anything the user had switched on has to be restored -- the
    // scene graph can recreate the renderer at any time (window re-show, device
    // loss) and the overlay would otherwise silently vanish until the next
    // control change. Flags are pushed directly; the geometry needs a rebuild,
    // which cannot happen from a const method, so it is deferred to the event
    // loop.
    if (showGlyphs || showStreamlines) {
        auto* self = const_cast<OpenGLWidgetQML*>(this);
        QTimer::singleShot(0, self, [self]() {
            if (self->showGlyphs)      self->rebuildGlyphs();
            if (self->showStreamlines) self->rebuildStreamlines();
        });
    }

    return m_render;
}

void OpenGLWidgetQML::configureRenderer(RenderOpenGL* r) const
{
    r->resizeGL(this->width(), this->height());
    r->setDevicePixelRatio(window() ? window()->devicePixelRatio() : 1.0f);
    r->setRotations(xRot, yRot, zRot);
    r->setPan(panX, panY);
    r->setDistZoomFactor(distance, zoomFactor);
    r->setNumCubes(numCubes);
    r->setShowCornerAxes(m_showCornerAxes);
    r->setShowGlyphs(showGlyphs);
    r->setShowStreamlines(showStreamlines);
    r->setVoxelOpacity(voxelOpacity);
    r->setShading(m_shadingModel, float(m_aoStrength), float(m_depthCue));
    r->setPlotWireFrame(plotWireFrame);
    r->setOutlines(m_outlineKind, float(m_outlineWidth));
    r->updateOrientationData(orientationVerts, orientationColors, orientationNormals);
    r->setShowOrientations(showOrientations);
}

QImage OpenGLWidgetQML::renderHeadless(QSize pixelSize, RenderOpenGL::CaptureBackground background)
{
    if (pixelSize.isEmpty()) return QImage();
    // One context for the life of the program: creating a GL context costs
    // tens of milliseconds and some drivers leak a little each time.
    static QOpenGLContext*    ctx = nullptr;
    static QOffscreenSurface* surface = nullptr;
    if (!ctx) {
        auto* c = new QOpenGLContext;
        c->setFormat(QSurfaceFormat::defaultFormat());
        if (!c->create()) { qWarning() << "[capture] headless: cannot create a GL context"; delete c; return QImage(); }
        surface = new QOffscreenSurface;
        surface->setFormat(c->format());
        surface->create();
        ctx = c;
    }
    if (!ctx->makeCurrent(surface)) { qWarning() << "[capture] headless: makeCurrent failed"; return QImage(); }
    QImage img;
    {
        // A renderer of its own, set up like the window's. Tensor glyph and
        // streamline meshes are not copied (they live in the window's renderer).
        std::unique_ptr<RenderOpenGL> r(new RenderOpenGL());
        configureRenderer(r.get());
        r->updateVoxelData(voxelScene);
        img = r->renderImage(pixelSize, background);
    }
    ctx->doneCurrent();
    qDebug().noquote() << QStringLiteral("[capture] headless render %1 x %2").arg(img.width()).arg(img.height());
    return img;
}

std::vector<svgx::ShadedFace> OpenGLWidgetQML::vectorFaces(QSize px) const
{
    std::vector<svgx::ShadedFace> out;
    if (px.isEmpty() || voxelScene.size() < 4) return out;

    // The camera of RenderOpenGL::paintGL() / updateProjection().
    const float W = float(px.width()), H = float(px.height());
    const float R = numCubes * 0.866f;
    float nearV = std::max(0.01f, distance - R * 2.0f);
    float farV  = distance + R * 2.0f + 1.0f;
    if (farV <= nearV) farV = nearV + 1.0f;
    if (farV / nearV > 10000.0f) nearV = farV / 10000.0f;
    QMatrix4x4 proj;
    proj.perspective(45.0f, W / H, nearV, farV);
    QMatrix4x4 view;
    view.translate(panX, panY, -distance);
    view.rotate(xRot / 16.0f, 1.0f, 0.0f, 0.0f);
    view.rotate(yRot / 16.0f, 0.0f, 1.0f, 0.0f);
    view.rotate(zRot / 16.0f, 0.0f, 0.0f, 1.0f);
    const QMatrix4x4 mvp = proj * view;
    const QMatrix3x3 nrm = view.normalMatrix();
    const float depthNear = std::max(0.0f, distance - R), depthFar = distance + R;

    const bool outlines = plotWireFrame;
    const QVector3D keyDir  = QVector3D(-0.65f, -0.55f, 0.55f).normalized();
    const QVector3D fillDir = QVector3D( 0.80f,  0.10f, 0.60f).normalized();

    out.reserve(voxelScene.size() / 8);
    for (size_t i = 0; i + 3 < voxelScene.size(); i += 4) {
        svgx::ShadedFace f;
        bool ok = true;
        float zsum = 0.0f;
        QVector3D centre;
        for (int k = 0; k < 4; ++k) {
            const GlVertex& g = voxelScene[i + k];
            const QVector4D clip = mvp * QVector4D(g.x, g.y, g.z, 1.0f);
            if (clip.w() <= 0.0f) { ok = false; break; }
            f.pts[k] = QPointF((clip.x() / clip.w() * 0.5f + 0.5f) * W,
                               (clip.y() / clip.w() * 0.5f + 0.5f) * H);
            const QVector3D v = (view * QVector4D(g.x, g.y, g.z, 1.0f)).toVector3D();
            centre += v;
            zsum += v.z();
        }
        if (!ok) continue;
        // Front faces are counter-clockwise in these coordinates (y grows with
        // NDC y, as in the read-back capture); a back face has negative area.
        qreal area = 0.0;
        for (int k = 0; k < 4; ++k) {
            const QPointF& a = f.pts[k];
            const QPointF& b = f.pts[(k + 1) % 4];
            area += a.x() * b.y() - b.x() * a.y();
        }
        if (area <= 0.0) continue;
        f.depth = zsum / 4.0f;           // view space: more negative = farther
        centre /= 4.0f;

        // Base colour and AO, averaged over the face.
        float r = 0, gg = 0, b = 0, ao = 0;
        for (int k = 0; k < 4; ++k) {
            const GlVertex& g = voxelScene[i + k];
            r += g.r; gg += g.g; b += g.b;
            ao += float(g.ao & 3) / 3.0f;
        }
        QVector3D base(r / (4.0f * 255.0f), gg / (4.0f * 255.0f), b / (4.0f * 255.0f));
        ao /= 4.0f;
        const GlVertex& g0 = voxelScene[i];

        QVector3D c;
        if (m_shadingModel == 1) {
            // glshaders.hpp depthShading(), per face.
            const float* m = nrm.constData();   // column-major
            const QVector3D n0(g0.nx / 127.0f, g0.ny / 127.0f, g0.nz / 127.0f);
            QVector3D n(m[0] * n0.x() + m[3] * n0.y() + m[6] * n0.z(),
                        m[1] * n0.x() + m[4] * n0.y() + m[7] * n0.z(),
                        m[2] * n0.x() + m[5] * n0.y() + m[8] * n0.z());
            n.normalize();
            const QVector3D v = (-centre).normalized();
            const float aoS = float(m_aoStrength);
            const float aoF = 1.0f + (0.45f + 0.55f * ao - 1.0f) * aoS;
            const float up = 0.5f - 0.5f * n.y();
            const float amb = 0.28f + (0.44f - 0.28f) * up;
            const float dk = std::max(QVector3D::dotProduct(n, keyDir), 0.0f);
            const float df = std::max(QVector3D::dotProduct(n, fillDir), 0.0f);
            const float direct = 0.66f * dk + 0.28f * df;
            const float spec = std::pow(std::max(QVector3D::dotProduct(n, (keyDir + v).normalized()), 0.0f), 32.0f) * 0.12f;
            const float mixAo = 1.0f + (aoF - 1.0f) * 0.6f;
            c = base * (amb * aoF + direct * mixAo) + QVector3D(spec, spec, spec) * aoF;
            c = QVector3D(std::max(c.x(), base.x() * 0.28f), std::max(c.y(), base.y() * 0.28f),
                          std::max(c.z(), base.z() * 0.28f));
            const float d = -centre.z();
            const float t = std::clamp((d - depthNear) / std::max(depthFar - depthNear, 1e-3f), 0.0f, 1.0f);
            const float lum = 0.2126f * c.x() + 0.7152f * c.y() + 0.0722f * c.z();
            const QVector3D farC = (QVector3D(lum, lum, lum) * 0.4f + c * 0.6f) * 0.62f;
            c = c + (farC - c) * (float(m_depthCue) * t);
        } else {
            const QRgb q = svgx::shadeFace(g0);
            c = QVector3D(qRed(q), qGreen(q), qBlue(q)) / 255.0f;
            c = QVector3D(c.x() * base.x() * 255.0f / std::max(1.0f, float(g0.r)),
                          c.y() * base.y() * 255.0f / std::max(1.0f, float(g0.g)),
                          c.z() * base.z() * 255.0f / std::max(1.0f, float(g0.b)));
        }
        auto to8 = [](float v) { return int(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f)); };
        f.color = qRgb(to8(c.x()), to8(c.y()), to8(c.z()));
        const float dark = (m_outlineKind == 1) ? 0.55f : 0.30f;
        f.edgeColor = qRgb(to8(c.x() * dark), to8(c.y() * dark), to8(c.z() * dark));
        f.edgeMask = !outlines ? 0 : (m_outlineKind == 1 ? 15 : ((g0.ao >> 2) & 15));
        out.push_back(f);
    }

    // The scene axes of RenderOpenGL::drawAxisWithMVP(): shafts from the
    // corner (-n/2, -n/2, -n/2) to 1.15 n minus the cone, arrowheads as
    // triangles. Cut into short pieces, each sorted by its own depth among
    // the faces, so the cube hides an axis where the GL depth test would.
    {
        auto toPx = [&](const QVector3D& w, QPointF& pt, float& z) {
            const QVector4D clip = mvp * QVector4D(w, 1.0f);
            if (clip.w() <= 0.0f) return false;
            pt = QPointF((clip.x() / clip.w() * 0.5f + 0.5f) * W, (clip.y() / clip.w() * 0.5f + 0.5f) * H);
            z = (view * QVector4D(w, 1.0f)).z();
            return true;
        };
        const float half = numCubes / 2.0f;
        const float tip = 1.15f * numCubes;
        const float coneH = numCubes * 0.08f, coneR = numCubes * 0.03f;
        const QVector3D origin(-half, -half, -half);
        const QRgb cols[3] = { qRgb(255, 0, 0), qRgb(0, 255, 0), qRgb(0, 128, 255) };
        // Pieces lie on cube edges for part of their length. There the faces
        // that share the edge win, as they do in the GL depth test (a pull
        // toward the camera drew the axes over the cube, the red one dashed):
        // a push of 0.6 voxel puts each piece behind the face centres next to it.
        const float bias = -0.6f;
        for (int a = 0; a < 3; ++a) {
            QVector3D dir(0, 0, 0);
            dir[a] = 1.0f;
            const QVector3D shaftEnd = origin + dir * (tip - coneH + half);
            const int pieces = 48;
            for (int k = 0; k < pieces; ++k) {
                const QVector3D p0 = origin + (shaftEnd - origin) * (float(k) / pieces);
                const QVector3D p1 = origin + (shaftEnd - origin) * (float(k + 1) / pieces);
                svgx::ShadedFace l;
                float z0, z1;
                if (!toPx(p0, l.pts[0], z0) || !toPx(p1, l.pts[1], z1)) continue;
                l.kind = 1;
                l.width = 1.5f;
                l.color = cols[a];
                l.edgeColor = cols[a];
                l.edgeMask = 0;
                l.depth = 0.5f * (z0 + z1) + bias;
                out.push_back(l);
            }
            // Arrowhead: the cone's outline as seen, a triangle from the base
            // circle's widest projected chord to the tip.
            const QVector3D tipW = origin + dir * (tip + half);
            const QVector3D baseW = shaftEnd;
            svgx::ShadedFace t;
            float zt, zb;
            QPointF tipP, baseP;
            if (!toPx(tipW, tipP, zt) || !toPx(baseW, baseP, zb)) continue;
            QPointF axis2 = tipP - baseP;
            const qreal len = std::hypot(axis2.x(), axis2.y());
            if (len < 1e-6) continue;
            const QPointF perp(-axis2.y() / len, axis2.x() / len);
            // Projected radius: the larger of the two circle directions.
            qreal rPx = 0.0;
            for (int b = 0; b < 3; ++b) {
                if (b == a) continue;
                QVector3D e(0, 0, 0);
                e[b] = coneR;
                QPointF q; float zq;
                if (toPx(baseW + e, q, zq)) rPx = std::max(rPx, std::hypot(q.x() - baseP.x(), q.y() - baseP.y()));
            }
            t.kind = 2;
            t.pts[0] = tipP;
            t.pts[1] = baseP + perp * rPx;
            t.pts[2] = baseP - perp * rPx;
            t.pts[3] = tipP;
            t.color = cols[a];
            t.edgeColor = cols[a];
            t.edgeMask = 0;
            t.depth = 0.5f * (zt + zb) + bias;
            out.push_back(t);
        }
    }

    std::sort(out.begin(), out.end(),
              [](const svgx::ShadedFace& a, const svgx::ShadedFace& b) { return a.depth < b.depth; });
    return out;
}

OpenGLWidgetQML::~OpenGLWidgetQML()
{
    if (timer)
    {
        delete timer;
    }
    if (m_render)
    {
        //delete m_render;
    }
    if (instance == this)
    {
        instance = nullptr;
    }
}


void OpenGLWidgetQML::qNormalizeAngle(int &angle)
{
    while (angle < -360 * 16)
        angle += 360 * 16;
    while (angle >= 360 * 16)
        angle -= 360 * 16;
}

// cpp — mirror exactly how your other setters reach m_render:
void OpenGLWidgetQML::requestSvgExport(const QString& path)
{
    if (m_render) m_render->requestSvgExport(path);  // guard like the other setters
    update();                                        // force a frame to serve it
}

QPointF OpenGLWidgetQML::projectAxisLabel(const QVector3D& dir) const
{
    // Same view matrix as RenderOpenGL::drawCornerAxes(): rotation only, no
    // scene translation, zoom or numCubes -- the triad never scales with the
    // scene, it only turns.
    QMatrix4x4 view;
    view.translate(0.0f, 0.0f, -matviz_gizmo::kCamDist);
    view.rotate(xRot / 16.0f, 1.0f, 0.0f, 0.0f);
    view.rotate(yRot / 16.0f, 0.0f, 1.0f, 0.0f);
    view.rotate(zRot / 16.0f, 0.0f, 0.0f, 1.0f);

    // Aspect 1.0: the triad viewport is square.
    QMatrix4x4 proj;
    proj.perspective(matviz_gizmo::kFov, 1.0f, 0.1f, 10.0f);

    // Explicit w-divide rather than QMatrix4x4::map(), which divides
    // implicitly and leaves the w == 0 case undefined.
    const QVector4D clip =
        (proj * view) * QVector4D(dir * matviz_gizmo::kAxisLen, 1.0f);

    if (qFuzzyIsNull(clip.w()))
        return QPointF(-1000.0, -1000.0);   // park the label off-screen

    const float ndcX = clip.x() / clip.w();  // NDC, range [-1, +1]
    const float ndcY = clip.y() / clip.w();

    // Top-left corner of the triad viewport, in logical pixels. No device
    // pixel ratio here: QQuickItem::width() is already logical, and so is
    // everything QML positions.
    const float size    = matviz_gizmo::kSize;
    const float margin  = matviz_gizmo::kMargin;
    const float originX = float(width())  - size - margin;
    const float originY = float(height()) - size - margin;

    const float localX = (ndcX * 0.5f + 0.5f) * size;
    const float localY = kGizmoYFlip ? (ndcY * 0.5f + 0.5f) * size
                                     : (0.5f - ndcY * 0.5f) * size;

    return QPointF(originX + localX, originY + localY);
}

// Single choke point for "the camera turned": pushes the angles to the
// renderer, repaints, and tells QML to move the corner-triad labels. Every
// rotation setter and view preset goes through here so none can forget the
// notification.
void OpenGLWidgetQML::pushRotations()
{
    if (m_render) {
        m_render->setRotations(xRot, yRot, zRot);
    }
    update();
    emit axisLabelsChanged();
}

void OpenGLWidgetQML::setXRotation(int angle)
{
    qNormalizeAngle(angle);
    if (angle != xRot) {
        xRot = angle;
        pushRotations();
    }
}

void OpenGLWidgetQML::setYRotation(int angle)
{
    qNormalizeAngle(angle);
    if (angle != yRot) {
        yRot = angle;
        pushRotations();
    }
}

void OpenGLWidgetQML::setZRotation(int angle)
{
    qNormalizeAngle(angle);
    if (angle != zRot) {
        zRot = angle;
        pushRotations();
    }
}

void OpenGLWidgetQML::setFrontView() {
    xRot = 0;
    yRot = 0;
    zRot = 0;
    if (m_render) {
        m_render->setRotations(xRot, yRot, zRot);
    }
    update();
}

void OpenGLWidgetQML::setBackView() {
    xRot = 0;
    yRot = 180 * 16;
    zRot = 0;
    pushRotations();
}

void OpenGLWidgetQML::setTopView() {
    xRot = 90 * 16;
    yRot = 0;
    zRot = 0;
    pushRotations();
}

void OpenGLWidgetQML::setBottomView() {
    xRot = -90 * 16;
    yRot = 0;
    zRot = 0;
    pushRotations();
}

void OpenGLWidgetQML::setLeftView() {
    xRot = 0;
    yRot = 90 * 16;
    zRot = 0;
    pushRotations();
}

void OpenGLWidgetQML::setRightView() {
    xRot = 0;
    yRot = -90 * 16;
    zRot = 0;
    pushRotations();
}

/**
 * Set the view to standard isometric projection.
 */
void OpenGLWidgetQML::setIsometricView()
{
    xRot =  35.26 * 16;
    yRot = -45.00 * 16;
    zRot =  0;
    pushRotations();
}

/**
 * Set the view to standard dimetric projection.
 */
void OpenGLWidgetQML::setDimetricView()
{
    xRot =  26.57 * 16;
    yRot = -45.00 * 16;
    zRot =  0;
    pushRotations();
}

/**
 * Set the view to isometric projection looking from below (X and Z axis tilted down).
 */
void OpenGLWidgetQML::setIsometricDownView()
{
    xRot = -35.26 * 16;
    yRot = -45.00 * 16;
    zRot =  0;
    pushRotations();
}

/**
 * Set the view to dimetric projection looking from below (X and Z axis tilted down).
 */
void OpenGLWidgetQML::setDimetricDownView()
{
    xRot = -26.57 * 16;
    yRot = -45.00 * 16;
    zRot =  0;
    pushRotations();
}

void OpenGLWidgetQML::setNumCubes(int numCubes)
{
    this->numCubes = numCubes;
    distance = calculateFitDistance();
    panX = 0.0f;
    panY = 0.0f;
    this->fieldMode = FieldMode::None;
    this->ansysField.reset();
    this->fftField.reset();
    if (m_render) {
        m_render->setPan(panX, panY);
        m_render->setNumCubes(numCubes);
        m_render->setDistZoomFactor(distance, zoomFactor);
    }
    update();
}


void OpenGLWidgetQML::setNumColors(int numColors)
{
    this->numColors = numColors;
    this->colors = generateDistinctColors();
    directionFactors.resize(numColors);

    // Initialize direction factors randomly for each color
    for (int i = 0; i < numColors; i++) {
        directionFactors[i] = ((rand() % 2) == 0) ? 1.0f : -1.0f;
    }
    this->fieldMode = FieldMode::None;
    this->ansysField.reset();
    this->fftField.reset();
}

void OpenGLWidgetQML::setDelayAnimation(int delayAnimation)
{
    this->delayAnimation = delayAnimation;
}


void OpenGLWidgetQML::setDistanceFactor(int factor)
{
    if (numCubes <= 0)
    {
        return;
    }
    factor = qMax(0, factor);
    distanceFactor = log(factor/10.0 + 1) * numCubes; // 5.0 - is a sensetivity factor. log - inroduce soft sizing
    qDebug() << "distanceFactor = " << distanceFactor;
    calculateScene();
    update();
}


void OpenGLWidgetQML::setPlotWireFrame(bool status)
{
    if (plotWireFrame == status) return;
    this->plotWireFrame = status;
    if (m_render) {
        m_render->setPlotWireFrame(status);
    }
    update();
    emit outlinesChanged();
}

void OpenGLWidgetQML::setOutlineKind(int kind)
{
    kind = (kind == 1) ? 1 : 2;
    if (kind == m_outlineKind) return;
    m_outlineKind = kind;
    if (m_render) m_render->setOutlines(m_outlineKind, float(m_outlineWidth));
    update();
    emit outlinesChanged();
}

void OpenGLWidgetQML::setOutlineWidth(qreal w)
{
    w = std::clamp(w, 0.25, 8.0);
    if (qFuzzyCompare(w + 1.0, m_outlineWidth + 1.0)) return;
    m_outlineWidth = w;
    if (m_render) m_render->setOutlines(m_outlineKind, float(m_outlineWidth));
    update();
    emit outlinesChanged();
}

void OpenGLWidgetQML::setShowCornerAxes(bool show)
{
    if (m_showCornerAxes != show) {
        m_showCornerAxes = show;
        if (m_render) {
            m_render->setShowCornerAxes(show);
        }
        update();
        emit showCornerAxesChanged();
    }
}

bool OpenGLWidgetQML::showCornerAxes() const
{
    return m_showCornerAxes;
}

void OpenGLWidgetQML::pushShading()
{
    if (m_render) m_render->setShading(m_shadingModel, float(m_aoStrength), float(m_depthCue));
    update();
    emit shadingChanged();
}

void OpenGLWidgetQML::setShadingModel(int model)
{
    model = (model == 1) ? 1 : 0;
    if (model == m_shadingModel) return;
    m_shadingModel = model;
    pushShading();
}

void OpenGLWidgetQML::setAoStrength(qreal v)
{
    v = std::clamp(v, 0.0, 1.0);
    if (qFuzzyCompare(v + 1.0, m_aoStrength + 1.0)) return;
    m_aoStrength = v;
    pushShading();
}

void OpenGLWidgetQML::setDepthCue(qreal v)
{
    v = std::clamp(v, 0.0, 1.0);
    if (qFuzzyCompare(v + 1.0, m_depthCue + 1.0)) return;
    m_depthCue = v;
    pushShading();
}

bool OpenGLWidgetQML::requestCapture(QSize pixelSize, RenderOpenGL::CaptureBackground background,
                                     QObject* receiver, std::function<void(QImage)> done)
{
    if (!m_render || pixelSize.isEmpty()) return false;
    RenderOpenGL::CaptureRequest req;
    req.size       = pixelSize;
    req.background = background;
    req.receiver   = receiver;
    req.done       = std::move(done);
    m_render->requestCapture(std::move(req));
    update();   // make sure a frame comes to serve it
    return true;
}

void OpenGLWidgetQML::wheelEvent(QWheelEvent *event)
{
    const int deltaY = event->angleDelta().y();
    if (deltaY == 0) return;

    int numSteps = deltaY / 120;
    if (numSteps == 0) {
        numSteps = (deltaY > 0) ? 1 : -1;
    }

    zoomStep(numSteps);
}

void OpenGLWidgetQML::zoomIn()
{
    zoomStep(1);
}

void OpenGLWidgetQML::zoomOut()
{
    zoomStep(-1);
}

float OpenGLWidgetQML::calculateFitDistance() const
{
    const float n = (numCubes > 0) ? static_cast<float>(numCubes) : 1.0f;
    const float sceneRadius = n * 0.8660254f; // sqrt(3)/2 * n
    const float fovY = 45.0f;
    const float fovYRad = qDegreesToRadians(fovY / 2.0f);
    const float tanHalfFovY = std::tan(fovYRad); // ~0.41421356

    float aspect = 1.0f;
    if (this->height() > 0 && this->width() > 0) {
        aspect = static_cast<float>(this->width()) / static_cast<float>(this->height());
    }

    const float tanHalfFov = (aspect < 1.0f && aspect > 0.01f) ? (tanHalfFovY * aspect) : tanHalfFovY;
    // (d - R) * tanHalfFov >= R => d >= R * (1 + 1/tanHalfFov)
    // Ensures all corners are within the frustum under any 3D rotation, plus 15% margin.
    return sceneRadius * (1.0f + 1.0f / tanHalfFov) * 1.15f;
}

void OpenGLWidgetQML::zoomToFit()
{
    panX = 0.0f;
    panY = 0.0f;
    zoomFactor = 1.0f;
    distance = calculateFitDistance();
    if (m_render) {
        m_render->setPan(panX, panY);
        m_render->setDistZoomFactor(distance, zoomFactor);
        m_render->resizeGL(this->width(), this->height());
    }
    update();
}

void OpenGLWidgetQML::zoomStep(int numSteps)
{
    if (numSteps == 0) return;

    const float n = (numCubes > 0) ? static_cast<float>(numCubes) : 1.0f;
    const float minDistance = n * 0.3f;
    const float maxDistance = n * 30.0f;

    // Multiplicative zoom: step by ~8% per notch, smooth across all zoom levels
    const float factor = std::pow(0.92f, static_cast<float>(numSteps));
    distance *= factor;
    distance = std::clamp(distance, minDistance, maxDistance);

    if (numSteps > 0) {
        zoomFactor *= 1.1f;
    } else {
        zoomFactor /= 1.1f;
    }

    if (m_render) {
        m_render->setDistZoomFactor(distance, zoomFactor);
    }
    update();
}

void OpenGLWidgetQML::mousePressEvent(QMouseEvent *event)
{
    lastPos = event->pos();
}

void OpenGLWidgetQML::mouseMoveEvent(QMouseEvent *event)
{
    int dx = event->position().x() - lastPos.x();
    int dy = event->position().y() - lastPos.y();

    if (event->buttons() & Qt::LeftButton) {
        setXRotation(xRot + 8 * dy);
        setYRotation(yRot + 8 * dx);
    } else if (event->buttons() & Qt::RightButton) {
        // Pan the cube center: convert screen pixel delta to world units at current distance.
        const float h = std::max(1.0f, static_cast<float>(this->height()));
        const float worldPerPixel = (0.8284f * distance) / h;
        panX += dx * worldPerPixel;
        panY += dy * worldPerPixel;
        if (m_render) {
            m_render->setPan(panX, panY);
        }
        update();
    }

    lastPos = event->pos();
}

void OpenGLWidgetQML::handleResize()
{
    if (m_render) 
    {
        m_render->setDevicePixelRatio(window() ? window()->devicePixelRatio() : 1.0f);
        m_render->resizeGL(this->width(), this->height());
    }
    update();
}



// The palette tables themselves live in colormap.hpp so the elastic-surface
// view in the material database window shares them; these remain as the
// existing call sites' entry points.
std::vector<std::array<GLubyte, 4>> OpenGLWidgetQML::createColorMap(int numLevels, ColorMapPalette palette)
{
    return matviz_cmap::createColorMap(numLevels, palette);
}

QVector<QColor> OpenGLWidgetQML::getColorMap(int numLevels)
{
    std::vector<std::array<GLubyte, 4>> vcmap = createColorMap(numLevels, colorMapPalette);
    QVector<QColor> cmap(vcmap.size());

    for (size_t i = 0; i < vcmap.size(); i++)
    {
        QColor c;
        c.setRed(vcmap[i][0]);
        c.setGreen(vcmap[i][1]);
        c.setBlue(vcmap[i][2]);
        cmap[i] = c;
    }
    return cmap;
}

void OpenGLWidgetQML::setColorMapPalette(int palette)
{
    palette = std::min(matviz_cmap::kPaletteCount - 1, std::max(0, palette));
    if (int(colorMapPalette) == palette) return;
    colorMapPalette = static_cast<ColorMapPalette>(palette);
    emit colorMapPaletteChanged();
    if (fieldMode != FieldMode::None) {
        calculateScene();
        pushSceneToRenderer();
        scheduleGlyphRebuild();   // glyph colours come from the same palette
        if (showStreamlines) rebuildStreamlines();
    }
}

std::array<GLubyte, 4> OpenGLWidgetQML::scalarToColor(float value, const std::vector<std::array<GLubyte, 4>>& colorMap)
{
    return matviz_cmap::scalarToColor(value, colorMap);
}

double OpenGLWidgetQML::getFieldMin() const
{
    if (fieldMode == FieldMode::Ansys && ansysField) {
        if (fieldComponent >= 0 && fieldComponent < (int)ansysField->loadstep_results_min.size())
            return ansysField->loadstep_results_min[fieldComponent];
    } else if (fieldMode == FieldMode::FFT && fftField) {
        if (fieldComponent >= 0 && fieldComponent < (int)fftField->componentMin.size())
            return fftField->componentMin[fieldComponent];
    }
    if (StressAnalysisController* sa = StressAnalysisController::getInstance())
        return sa->fieldMin();
    return 0.0;
}

double OpenGLWidgetQML::getFieldMax() const
{
    if (fieldMode == FieldMode::Ansys && ansysField) {
        if (fieldComponent >= 0 && fieldComponent < (int)ansysField->loadstep_results_max.size())
            return ansysField->loadstep_results_max[fieldComponent];
    } else if (fieldMode == FieldMode::FFT && fftField) {
        if (fieldComponent >= 0 && fieldComponent < (int)fftField->componentMax.size())
            return fftField->componentMax[fieldComponent];
    }
    if (StressAnalysisController* sa = StressAnalysisController::getInstance())
        return sa->fieldMax();
    return 0.0;
}

QString OpenGLWidgetQML::getFieldComponentName() const
{
    if (StressAnalysisController* sa = StressAnalysisController::getInstance()) {
        const QStringList list = sa->fieldComponents();
        const int idx = sa->fieldComponentIndex();
        if (idx >= 0 && idx < list.size()) return list[idx];
    }
    switch (fieldComponent) {
        case SX: return QStringLiteral("SX");
        case SY: return QStringLiteral("SY");
        case SZ: return QStringLiteral("SZ");
        case SXY: return QStringLiteral("SXY");
        case SYZ: return QStringLiteral("SYZ");
        case SXZ: return QStringLiteral("SXZ");
        case SEQV: return QStringLiteral("von Mises (SEQV)");
        case EpsX: return QStringLiteral("EpsX");
        case EpsY: return QStringLiteral("EpsY");
        case EpsZ: return QStringLiteral("EpsZ");
        case EpsXY: return QStringLiteral("EpsXY");
        case EpsYZ: return QStringLiteral("EpsYZ");
        case EpsXZ: return QStringLiteral("EpsXZ");
        case EpsEQV: return QStringLiteral("eqv. strain");
        case UX: return QStringLiteral("UX");
        case UY: return QStringLiteral("UY");
        case UZ: return QStringLiteral("UZ");
        case USUM: return QStringLiteral("USUM");
        case SCHMID_FACTOR: return QStringLiteral("Schmid Factor");
        case YIELD_STATUS: return QStringLiteral("Yield Status");
        default: return QStringLiteral("Field");
    }
}

void OpenGLWidgetQML::pushSceneToRenderer()
{
    if (m_render) {
        m_render->updateVoxelData(voxelScene);
        m_render->updateOrientationData(orientationVerts, orientationColors, orientationNormals);
    }
    update();
}

void OpenGLWidgetQML::showAnsysField(std::shared_ptr<ansysWrapper> wr, int component, bool deformed, float scale)
{
    this->ansysField    = wr;
    this->fftField.reset();
    this->fieldMode      = FieldMode::Ansys;
    this->fieldComponent = component;
    this->showDeformed   = deformed;
    this->deformedScale  = scale;
    invalidateTensorSnapshot();
    this->calculateScene();
    pushSceneToRenderer();
    if (showGlyphs) scheduleGlyphRebuild();
    emit tensorStateChanged();
}

void OpenGLWidgetQML::showFFTField(std::shared_ptr<FieldVisualizationData> data, int component, bool deformed, float scale)
{
    this->fftField       = data;
    this->ansysField.reset();
    this->fieldMode       = FieldMode::FFT;
    this->fieldComponent  = component;
    this->showDeformed    = deformed;
    this->deformedScale   = scale;
    invalidateTensorSnapshot();
    this->calculateScene();
    pushSceneToRenderer();
    if (showGlyphs) scheduleGlyphRebuild();
    emit tensorStateChanged();
}

void OpenGLWidgetQML::setFieldComponent(int component)
{
    if (fieldMode == FieldMode::None) return;
    this->fieldComponent = component;
    // The snapshot caches the selected component as its normalized colour
    // scalar, so it has to be rebuilt -- but only if a glyph overlay is
    // actually using it.
    invalidateTensorSnapshot();
    this->calculateScene();
    pushSceneToRenderer();
    scheduleGlyphRebuild();
}

void OpenGLWidgetQML::clearFieldVisualization()
{
    this->fieldMode = FieldMode::None;
    this->ansysField.reset();
    this->fftField.reset();
    this->showDeformed = false;
    invalidateTensorSnapshot();
    this->calculateScene();
    pushSceneToRenderer();
    emit tensorStateChanged();
}

void OpenGLWidgetQML::setShowDeformed(bool show)
{
    this->showDeformed = show;
    this->calculateScene();
    pushSceneToRenderer();
    if (showGlyphs) scheduleGlyphRebuild();
}

void OpenGLWidgetQML::setDeformedScale(float scale)
{
    this->deformedScale = scale;
    if (showDeformed) {
        this->calculateScene();
        pushSceneToRenderer();
        if (showGlyphs) scheduleGlyphRebuild();
    }
}

// ---------------------------------------------------------------------------
//  Tensor field overlays
// ---------------------------------------------------------------------------
void OpenGLWidgetQML::invalidateTensorSnapshot()
{
    tensorSnapshot.reset();
}

bool OpenGLWidgetQML::ensureTensorSnapshot()
{
    if (tensorSnapshot) return true;
    if (!voxels || numCubes <= 0) return false;

    // Built on demand rather than eagerly on every solve: a full snapshot is
    // tens of MB and, for ANSYS, several seconds of nodal lookups. Users who
    // never open the tensor panel should never pay for either.
    if (fieldMode == FieldMode::FFT && fftField) {
        tensorSnapshot = buildSnapshotFromFFT(*fftField, voxels, fieldComponent);
    } else if (fieldMode == FieldMode::Ansys && ansysField) {
        tensorSnapshot = buildSnapshotFromAnsys(*ansysField, numCubes, voxels, fieldComponent);
    }
    return static_cast<bool>(tensorSnapshot);
}

std::vector<std::array<float, 3>> OpenGLWidgetQML::buildGrainOffsets() const
{
    std::vector<std::array<float, 3>> offsets;
    if (distanceFactor <= 0.0f) return offsets;   // not exploded

    // Same derivation as calculateScene(): each grain is pushed away along the
    // difference between its colour and white, scaled by its direction factor.
    const size_t n = std::min(colors.size(), directionFactors.size());
    offsets.resize(n);
    for (size_t idx = 0; idx < n; ++idx) {
        const auto& c = colors[idx];
        const float diff[3] = { c[0] / 255.0f - 1.0f,
                                c[1] / 255.0f - 1.0f,
                                c[2] / 255.0f - 1.0f };
        for (int t = 0; t < 3; ++t)
            offsets[idx][t] = directionFactors[idx] * diff[t] * distanceFactor;
    }
    return offsets;
}

void OpenGLWidgetQML::rebuildGlyphs()
{
    if (!m_render) return;

    if (!showGlyphs) {
        m_render->updateGlyphMesh({}, {});
        m_glyphCount = 0;
        update();
        emit tensorStateChanged();
        return;
    }

    if (!ensureTensorSnapshot()) {
        m_glyphCount = 0;
        m_glyphSourceMissing = true;
        emit tensorStateChanged();
        return;
    }

    glyphParams.numCubes      = numCubes;
    glyphParams.cubeSize      = static_cast<float>(cubeSize);
    glyphParams.grainOffset   = buildGrainOffsets();
    glyphParams.showDeformed  = showDeformed;
    glyphParams.deformedScale = deformedScale;
    glyphParams.palette       = colorMapPalette;

    GlyphMesh mesh = buildGlyphMesh(*tensorSnapshot, glyphParams);

    m_glyphCount         = mesh.glyphCount;
    m_glyphSourceMissing = mesh.sourceMissing;

    m_render->updateGlyphMesh(std::move(mesh.verts), std::move(mesh.indices));
    update();
    emit tensorStateChanged();
}

void OpenGLWidgetQML::scheduleGlyphRebuild()
{
    if (!showGlyphs) return;
    if (!glyphRebuildTimer) {
        glyphRebuildTimer = new QTimer(this);
        glyphRebuildTimer->setSingleShot(true);
        glyphRebuildTimer->setInterval(120);
        connect(glyphRebuildTimer, &QTimer::timeout, this, &OpenGLWidgetQML::rebuildGlyphs);
    }
    glyphRebuildTimer->start();   // restarting collapses a slider drag into one build
}

void OpenGLWidgetQML::setShowGlyphs(bool show)
{
    if (showGlyphs == show) return;
    showGlyphs = show;
    if (m_render) m_render->setShowGlyphs(show);
    rebuildGlyphs();              // immediate: this is a click, not a drag
}

void OpenGLWidgetQML::setVoxelOpacity(qreal opacity)
{
    voxelOpacity = static_cast<float>(std::clamp(opacity, 0.0, 1.0));
    if (m_render) {
        m_render->setVoxelOpacity(voxelOpacity);   // renderer flag only, no re-mesh
        update();
    }
}

void OpenGLWidgetQML::setTensorSource(int source)
{
    const TensorSource s = (source == 1) ? TensorSource::Strain : TensorSource::Stress;
    if (glyphParams.source == s) return;
    glyphParams.source = s;
    scheduleGlyphRebuild();
    if (showStreamlines) rebuildStreamlines();
}

void OpenGLWidgetQML::setTensorDeviatoric(bool on)
{
    if (glyphParams.deviatoric == on) return;
    glyphParams.deviatoric = on;
    scheduleGlyphRebuild();
    if (showStreamlines) rebuildStreamlines();
}

void OpenGLWidgetQML::setGlyphStride(int stride)
{
    if (glyphParams.stride == stride) return;
    glyphParams.stride = stride;
    scheduleGlyphRebuild();
}

void OpenGLWidgetQML::setGlyphScale(qreal scale)
{
    glyphParams.maxHalfAxis = static_cast<float>(std::clamp(scale, 0.05, 2.0));
    scheduleGlyphRebuild();
}

void OpenGLWidgetQML::setGlyphSharpness(qreal gamma)
{
    glyphParams.gamma = static_cast<float>(std::clamp(gamma, 0.0, 6.0));
    scheduleGlyphRebuild();
}

void OpenGLWidgetQML::setGlyphColorMode(int mode)
{
    const GlyphColorMode m = static_cast<GlyphColorMode>(std::clamp(mode, 0, 3));
    if (glyphParams.colorMode == m) return;
    glyphParams.colorMode = m;
    scheduleGlyphRebuild();
    if (showStreamlines) rebuildStreamlines();
}

void OpenGLWidgetQML::setGlyphSlice(int axis, int index)
{
    glyphParams.sliceAxis  = std::clamp(axis, -1, 2);
    glyphParams.sliceIndex = index;
    scheduleGlyphRebuild();
}

// ---------------------------------------------------------------------------
//  Hyperstreamlines (off-thread)
// ---------------------------------------------------------------------------
void OpenGLWidgetQML::rebuildStreamlines()
{
    if (!m_render) return;

    // Every entry point bumps the generation, so a job already running becomes
    // stale the moment its inputs change.
    ++m_streamGeneration;

    if (!showStreamlines) {
        m_render->updateStreamlineMesh({}, {});
        m_streamCount = 0;
        m_streamBusy  = false;
        update();
        emit tensorStateChanged();
        return;
    }

    if (!ensureTensorSnapshot()) {
        m_streamCount = 0;
        m_streamBusy  = false;
        emit tensorStateChanged();
        return;
    }

    if (!streamWatcher) {
        streamWatcher = new QFutureWatcher<StreamJob>(this);
        connect(streamWatcher, &QFutureWatcher<StreamJob>::finished,
                this, &OpenGLWidgetQML::onStreamlinesFinished);
    }

    streamParams.numCubes  = numCubes;
    streamParams.cubeSize  = static_cast<float>(cubeSize);
    streamParams.source    = glyphParams.source;       // one Source control for both
    streamParams.deviatoric = glyphParams.deviatoric;
    streamParams.colorMode = glyphParams.colorMode;
    streamParams.palette   = colorMapPalette;

    // The worker captures only an immutable snapshot and a POD parameter struct:
    // no GL singleton, no Parameters::voxels, no LoadStepManager. Per the
    // threading convention in CLAUDE.md those are main-thread only.
    auto snap = tensorSnapshot;
    const StreamlineParams params = streamParams;
    const int generation = m_streamGeneration;

    m_streamBusy = true;
    emit tensorStateChanged();

    streamWatcher->setFuture(QtConcurrent::run([snap, params, generation]() {
        StreamJob job;
        job.generation = generation;
        job.mesh = buildStreamlineMesh(*snap, params);
        return job;
    }));
}

void OpenGLWidgetQML::onStreamlinesFinished()
{
    m_streamBusy = false;

    if (!streamWatcher || !streamWatcher->isFinished() || streamWatcher->isCanceled()) {
        emit tensorStateChanged();
        return;
    }

    StreamJob job = streamWatcher->result();

    // Superseded while it ran: the parameters have moved on, so this geometry
    // would be visibly wrong. Another job is already queued for the current
    // ones, so simply drop this.
    if (job.generation != m_streamGeneration) {
        emit tensorStateChanged();
        return;
    }

    if (!showStreamlines || !m_render) {
        emit tensorStateChanged();
        return;
    }

    // Back on the main thread, which is the only place the renderer may be
    // touched.
    m_streamCount = job.mesh.lineCount;
    qDebug() << "streamlines:" << job.mesh.lineCount << "lines,"
             << job.mesh.stationCount << "stations, stride" << job.mesh.strideUsed
             << "verts" << job.mesh.verts.size();

    m_render->updateStreamlineMesh(std::move(job.mesh.verts), std::move(job.mesh.indices));
    update();
    emit tensorStateChanged();
}

void OpenGLWidgetQML::setShowStreamlines(bool show)
{
    if (showStreamlines == show) return;
    showStreamlines = show;
    if (m_render) m_render->setShowStreamlines(show);
    rebuildStreamlines();
}

void OpenGLWidgetQML::setStreamlineSeedStride(int stride)
{
    if (streamParams.seedStride == stride) return;
    streamParams.seedStride = stride;
    if (showStreamlines) rebuildStreamlines();
}

void OpenGLWidgetQML::setStreamlineMaxLines(int lines)
{
    lines = std::clamp(lines, 1, 5000);
    if (streamParams.maxLines == lines) return;
    streamParams.maxLines = lines;
    if (showStreamlines) rebuildStreamlines();
}

void OpenGLWidgetQML::setStreamlineStep(qreal voxels)
{
    streamParams.stepVoxels = std::clamp(double(voxels), 0.05, 1.0);
    if (showStreamlines) rebuildStreamlines();
}

void OpenGLWidgetQML::setStreamlineMinLinearity(qreal cl)
{
    streamParams.minLinearity = std::clamp(double(cl), 0.0, 0.95);
    if (showStreamlines) rebuildStreamlines();
}

void OpenGLWidgetQML::setStreamlineTubeRadius(qreal radius)
{
    streamParams.tubeRadius = static_cast<float>(std::clamp(double(radius), 0.02, 1.5));
    if (showStreamlines) rebuildStreamlines();
}

std::vector<std::array<GLubyte, 4>> OpenGLWidgetQML::generateDistinctColors()
{
    // Opaque OKLCH colours, golden-ratio hues (matviz_cmap::grainColor): a
    // grain keeps its colour whatever the grain count.
    std::vector<std::array<GLubyte, 4>> colors;
    if (numColors <= 0) return colors;
    colors.reserve(size_t(numColors));
    for (int i = 0; i < numColors; ++i)
        colors.push_back(matviz_cmap::grainColor(i));
    return colors;
}

void OpenGLWidgetQML::calculateScene()
{

    static const float node_coordinates[8][3] =
        {
            {0, 0, 0}, // 1
            {0, 0, 1}, // 5
            {0, 1, 1}, // 8
            {0, 1, 0}, // 4
            {1, 0, 0}, // 2
            {1, 0, 1}, // 6
            {1, 1, 1}, // 7
            {1, 1, 0}, // 3
        };

    if (!voxels || numCubes <= 0) return;
    voxelScene.clear();
    float cubeSize = 1.0; // numCubes;
    const auto fieldCmap = (fieldMode != FieldMode::None) ? createColorMap(9, colorMapPalette)
                                                            : std::vector<std::array<GLubyte, 4>>{};
    for (int i = 0; i < numCubes; i++) { // y
        for (int j = 0; j < numCubes; j++) { // z
            for (int k = 0; k < numCubes; k++) { // x

                assert(voxels[k][i][j] >= 0);

                if(voxels[k][i][j] == 0)
                {
                    continue;
                }

                const bool isExploded = (distanceFactor > 0.0f);

                bool neighbors[6] = {false, false, false, false, false, false};

                auto vx = voxels[k][i][j];
                // In exploded view, emit all non-enclosed faces
                if (isExploded)
                {
                    // Exploded view: cull face if neighbor is same color (==vx)
                    neighbors[0] = (k > 0)            && (voxels[k-1][i][j] == vx);  // -x
                    neighbors[2] = (k < numCubes - 1) && (voxels[k+1][i][j] == vx);  // +x
                    neighbors[1] = (i < numCubes - 1) && (voxels[k][i+1][j] == vx);  // +y
                    neighbors[3] = (i > 0)            && (voxels[k][i-1][j] == vx);  // -y
                    neighbors[4] = (j < numCubes - 1) && (voxels[k][i][j+1] == vx);  // +z
                    neighbors[5] = (j > 0)            && (voxels[k][i][j-1] == vx);  // -z
                }
                else
                {
                    // Solid view: cull face if neighbor is any occupied voxel (!=0)
                    neighbors[0] = (k > 0)            && (voxels[k-1][i][j] != 0);  // -x
                    neighbors[2] = (k < numCubes - 1) && (voxels[k+1][i][j] != 0);  // +x
                    neighbors[1] = (i < numCubes - 1) && (voxels[k][i+1][j] != 0);  // +y
                    neighbors[3] = (i > 0)            && (voxels[k][i-1][j] != 0);  // -y
                    neighbors[4] = (j < numCubes - 1) && (voxels[k][i][j+1] != 0);  // +z
                    neighbors[5] = (j > 0)            && (voxels[k][i][j-1] != 0);  // -z

                }
                // Skip fully enclosed voxels in both modes
                if (neighbors[0] && neighbors[1] && neighbors[2] &&
                    neighbors[3] && neighbors[4] && neighbors[5])
                    continue;


 /*               bool neighbors[6] = {false, false, false, false, false, false};


                auto vx = distanceFactor > 0 ? voxels[k][i][j] : 0; // if exploded view enabled draw all faces, otherwise => outer faces
                // Cull shared faces between same-type voxels only
                neighbors[0] = (k > 0)            && (voxels[k-1][i][j] == vx);
                neighbors[2] = (k < numCubes - 1) && (voxels[k+1][i][j] == vx);
                neighbors[1] = (i < numCubes - 1) && (voxels[k][i+1][j] == vx);
                neighbors[3] = (i > 0)            && (voxels[k][i-1][j] == vx);
                neighbors[4] = (j < numCubes - 1) && (voxels[k][i][j+1] == vx);
                neighbors[5] = (j > 0)            && (voxels[k][i][j-1] == vx);

                // Skip fully enclosed voxels entirely
                if (neighbors[0] && neighbors[1] && neighbors[2] &&
                    neighbors[3] && neighbors[4] && neighbors[5])
                    continue;
*/

 /*               bool neighbors[6] = {false};  // check for the same type voxels
                {
                    auto vx = voxels[k][i][j];
                    neighbors[0] = (k > 0) ? (voxels[k - 1][i][j] == vx) : false; // -x
                    neighbors[2] = (k < numCubes - 1) ? (voxels[k + 1][i][j] == vx) : false; // +x

                    neighbors[1] = (i < numCubes - 1) ? (voxels[k][i + 1][j] == vx): false; // +y
                    neighbors[3] = (i > 0) ? (voxels[k][i - 1][j] == vx): false; // -y


                    neighbors[4] = (j < numCubes - 1) ? (voxels[k][i][j + 1] == vx) : false; // +z
                    neighbors[5] = (j > 0) ? (voxels[k][i][j - 1] == vx): false; // -z

                    bool c1 = neighbors[0] && neighbors[1];
                    bool c2 = neighbors[2] && neighbors[3];
                    bool c3 = neighbors[4] && neighbors[5];
                    if (c1 && c2 && c3)
                        continue;
                }
*/
                size_t index = voxels[k][i][j] - 1;

                if (colors.empty())
                {
                    continue;
                }
                if (index >= colors.size())
                {
                    index = index % colors.size();
                }
                auto color = colors[index].data();


                GLfloat refColor[] = {1.0f, 1.0f, 1.0f};
                GLfloat diff[] = {  GLfloat(color[0]/255.0 - refColor[0]),
                    GLfloat(color[1]/255.0 - refColor[1]),
                    GLfloat(color[2]/255.0 - refColor[2])
                };

                // Define a direction factor, which can be negative or positive
                float directionFactor = (index < directionFactors.size()) ? directionFactors[index] : 1.0f;

                GLfloat offset[] = {directionFactor * diff[0] * distanceFactor,
                                    directionFactor * diff[1] * distanceFactor,
                                    directionFactor * diff[2] * distanceFactor};

                RenderOpenGL::Voxel v;
                v.x = -(numCubes/2 - k) * cubeSize + offset[0];
                v.y = -(numCubes/2 - i) * cubeSize + offset[1];
                v.z = -(numCubes/2 - j) * cubeSize + offset[2];

                v.r = color[0];
                v.g = color[1];
                v.b = color[2];
                v.a = color[3];

                std::vector<std::array<GLubyte, 4>> node_colors(8);
                std::array<std::array<float, 3>, 8> node_disp{};   // zero == undeformed

                if (fieldMode == FieldMode::Ansys && ansysField)
                {
                    for (int l = 0; l < 8; l++)
                    {
                        n3d::node3d key;
                        key.data[0] = node_coordinates[l][0] + k;
                        key.data[1] = node_coordinates[l][1] + i;
                        key.data[2] = node_coordinates[l][2] + j;

                        float val = ansysField->getValByCoord(key, fieldComponent);
                        float val01 = std::min(1.0f, std::max(0.0f, ansysField->scaleValue01(val, fieldComponent)));
                        node_colors[l] = this->scalarToColor(val01, fieldCmap);

                        if (showDeformed)
                        {
                            const float ux = ansysField->getValByCoord(key, UX);
                            const float uy = ansysField->getValByCoord(key, UY);
                            const float uz = ansysField->getValByCoord(key, UZ);
                            node_disp[l] = {ux * deformedScale, uy * deformedScale, uz * deformedScale};
                        }
                    }
                }
                else if (fieldMode == FieldMode::FFT && fftField && fftField->componentValid[fieldComponent])
                {
                    // Flat-shaded: FFT only has one (element-averaged) value per voxel.
                    const int denseIdx = fftField->denseIndex(k, i, j);
                    const float val  = fftField->perVoxel[fieldComponent][denseIdx];
                    const float minv = fftField->componentMin[fieldComponent];
                    const float maxv = fftField->componentMax[fieldComponent];
                    float val01 = (maxv > minv) ? (val - minv) / (maxv - minv) : 1.0f;
                    val01 = std::min(1.0f, std::max(0.0f, val01));

                    auto color = this->scalarToColor(val01, fieldCmap);
                    std::fill(node_colors.begin(), node_colors.end(), color);

                    if (showDeformed)
                    {
                        // FFT has no nodal displacement field: approximate the deformed
                        // shape by applying the RVE macro (tensor) strain as a uniform
                        // affine map u = eps.x to every corner's undeformed world position.
                        const double* eps = fftField->macroStrain;
                        for (int l = 0; l < 8; l++)
                        {
                            const float wx = v.x + node_coordinates[l][0] * cubeSize;
                            const float wy = v.y + node_coordinates[l][1] * cubeSize;
                            const float wz = v.z + node_coordinates[l][2] * cubeSize;
                            const float dx = float(eps[0] * wx + eps[3] * wy + eps[5] * wz);
                            const float dy = float(eps[3] * wx + eps[1] * wy + eps[4] * wz);
                            const float dz = float(eps[5] * wx + eps[4] * wy + eps[2] * wz);
                            node_disp[l] = {dx * deformedScale, dy * deformedScale, dz * deformedScale};
                        }
                    }
                }
                else
                {
                    std::fill(node_colors.begin(), node_colors.end(), colors[index]);
                }

                // ── Per-vertex ambient occlusion ─────────────────────────
                // For each corner of a visible face, look at the three voxels
                // around that corner in the layer just outside the face: the
                // two along the face's edges (side1, side2) and the diagonal
                // one (corner). Level 3 = open, 0 = a concave corner closed on
                // both sides. "Occupied" matches the face culling: any solid
                // voxel, or in the exploded view only this grain's own.
                //
                // ── Boundary edges ───────────────────────────────────────
                // Edge q of a face runs from its corner q to corner q + 1.
                // It is a boundary (outline mode 2) where the surface meets
                // another grain, or turns a real corner: convex (no voxel
                // across the edge, and none below that either) or concave
                // (a wall of this grain's neighbour at least two voxels
                // high). The one-voxel steps of a voxelized curved surface
                // are not boundaries: a line at every step of a gyroid's
                // staircase drowned the shape in noise.
                GLubyte faceAO[6][4];
                GLubyte faceEdges[6] = { 0, 0, 0, 0, 0, 0 };
                {
                    static const int faceAxis[6] = { 0, 1, 0, 1, 2, 2 };   // -X +Y +X -Y +Z -Z
                    static const int faceSign[6] = { -1, 1, 1, -1, 1, -1 };
                    const int n = numCubes;
                    auto occ = [&](int x, int y, int z) -> int {
                        if (x < 0 || y < 0 || z < 0 || x >= n || y >= n || z >= n) return 0;
                        const int32_t w = voxels[x][y][z];
                        return isExploded ? (w == vx) : (w != 0);
                    };
                    const int p0[3] = { k, i, j };   // voxels[x][y][z]
                    for (int f = 0; f < 6; ++f) {
                        if (neighbors[f]) continue;
                        const int a  = faceAxis[f];
                        const int b1 = (a + 1) % 3, b2 = (a + 2) % 3;
                        for (int q = 0; q < 4; ++q) {
                            const int node = kCubeFaces[f][q];
                            const int c[3] = { node >= 4 ? 1 : 0,
                                               (node == 2 || node == 3 || node == 6 || node == 7) ? 1 : 0,
                                               (node == 1 || node == 2 || node == 5 || node == 6) ? 1 : 0 };
                            int base[3] = { p0[0], p0[1], p0[2] };
                            base[a] += faceSign[f];
                            int s1p[3] = { base[0], base[1], base[2] };
                            int s2p[3] = { base[0], base[1], base[2] };
                            s1p[b1] += c[b1] ? 1 : -1;
                            s2p[b2] += c[b2] ? 1 : -1;
                            int cp[3] = { s1p[0], s1p[1], s1p[2] };
                            cp[b2] += c[b2] ? 1 : -1;
                            const int s1 = occ(s1p[0], s1p[1], s1p[2]);
                            const int s2 = occ(s2p[0], s2p[1], s2p[2]);
                            const int cc = occ(cp[0], cp[1], cp[2]);
                            const int level = (s1 && s2) ? 0 : 3 - (s1 + s2 + cc);
                            faceAO[f][q] = GLubyte(level);
                        }
                        for (int q = 0; q < 4; ++q) {
                            const int na = kCubeFaces[f][q], nb = kCubeFaces[f][(q + 1) % 4];
                            const int ca[3] = { na >= 4, na == 2 || na == 3 || na == 6 || na == 7,
                                                na == 1 || na == 2 || na == 5 || na == 6 };
                            const int cb[3] = { nb >= 4, nb == 2 || nb == 3 || nb == 6 || nb == 7,
                                                nb == 1 || nb == 2 || nb == 5 || nb == 6 };
                            // The in-plane axis both corners share is the one
                            // the edge sits on; step across it.
                            const int bo = (ca[b1] == cb[b1]) ? b1 : b2;
                            int np[3] = { p0[0], p0[1], p0[2] };
                            np[bo] += ca[bo] ? 1 : -1;
                            const bool inside = np[0] >= 0 && np[1] >= 0 && np[2] >= 0 &&
                                                np[0] < n && np[1] < n && np[2] < n;
                            const int32_t w = inside ? voxels[np[0]][np[1]][np[2]] : 0;
                            const int sa = faceSign[f];
                            auto occAt = [&](int k) {   // occupied, k layers out along the normal
                                int t[3] = { np[0], np[1], np[2] };
                                t[a] += k * sa;
                                return occ(t[0], t[1], t[2]) != 0;
                            };
                            bool boundary;
                            if (w == vx)            // the face continues, or a concave corner:
                                boundary = occAt(1) && occAt(2);    // a wall at least 2 voxels high
                            else if (w != 0 && !isExploded)
                                boundary = true;    // another grain: a grain boundary
                            else                    // a convex corner, unless a 1-voxel step
                                boundary = !occAt(-1);
                            if (boundary) faceEdges[f] |= GLubyte(1u << q);
                        }
                    }
                }

                drawCube(cubeSize, v, neighbors, node_colors, node_disp, faceAO, faceEdges);
            }
        }
    }
    buildOrientationGlyphs();
    isVBOupdateRequired = true;
}

void OpenGLWidgetQML::setVoxels(int32_t*** voxels, short int numCubes)
{
    this->voxels = voxels;
    this->numCubes = numCubes;

    if (voxels && numCubes > 0) {
        int maxId = 0;
        for (int k = 0; k < numCubes; ++k)
            for (int i = 0; i < numCubes; ++i)
                for (int j = 0; j < numCubes; ++j)
                    if (voxels[k][i][j] > maxId) maxId = voxels[k][i][j];

        if (maxId > (int)colors.size()) {
            setNumColors(maxId);
        }
    }

    distance = calculateFitDistance();
    panX = 0.0f;
    panY = 0.0f;
    voxelScene.clear();
    invalidateTensorSnapshot();   // new structure: any cached tensor field is stale
    calculateScene();
    if (m_render)
    {
        m_render->setPan(panX, panY);
        m_render->setDevicePixelRatio(window() ? window()->devicePixelRatio() : 1.0f);
        m_render->setDistZoomFactor(distance, zoomFactor);
        m_render->setNumCubes(numCubes);
        m_render->updateVoxelData(voxelScene);
        m_render->updateOrientationData(orientationVerts, orientationColors, orientationNormals);
        m_render->resizeGL(this->width(), this->height());
    }
    update();
}

void OpenGLWidgetQML::drawCube(short cubeSize, RenderOpenGL::Voxel vox, bool* neighbors,
                                std::vector<std::array<GLubyte, 4>> &node_colors,
                                const std::array<std::array<float, 3>, 8> &node_disp,
                                const GLubyte (&faceAO)[6][4],
                                const GLubyte (&faceEdges)[6])
{

/*    static const GLfloat n[6][3] =
        {
            {-1.0, 0.0, 0.0}, // -x
            {0.0, 1.0, 0.0},  // y
            {1.0, 0.0, 0.0},  // x
            {0.0, -1.0, 0.0}, // -y
            {0.0, 0.0, 1.0},  // z
            {0.0, 0.0, -1.0}  // -z
        };
  */
    static const GLbyte n[6][3] = {
        {-127,    0,    0},  // -X
        {   0,  127,    0},  // +Y
        { 127,    0,    0},  // +X
        {   0, -127,    0},  // -Y
        {   0,    0,  127},  // +Z
        {   0,    0, -127}   // -Z
    };

    const auto& faces = kCubeFaces;
    float v[8][3];

    float offset = 0.0; // offset to help z-buffer distinc same quads

    v[0][0] = v[1][0] = v[2][0] = v[3][0] = vox.x + offset;
    v[4][0] = v[5][0] = v[6][0] = v[7][0] = vox.x + cubeSize - offset;
    v[0][1] = v[1][1] = v[4][1] = v[5][1] = vox.y + offset;
    v[2][1] = v[3][1] = v[6][1] = v[7][1] = vox.y + cubeSize - offset;
    v[0][2] = v[3][2] = v[4][2] = v[7][2] = vox.z + offset;
    v[1][2] = v[2][2] = v[5][2] = v[6][2] = vox.z + cubeSize - offset;

    RenderOpenGL::Voxel v1;
    for (int i = 0; i < 6; i++) // for each of 6 face of cube
    {
        if (neighbors[i])
            continue;

        v1.nx = n[i][0];
        v1.ny = n[i][1];
        v1.nz = n[i][2];

        if  (neighbors[i] == true) // plot hidden faces with gray (50.50.50) color for debug
        {
            v1.r = 50;
            v1.g = 50;
            v1.b = 50;
            v1.a = vox.a;
        }

        // The quad is drawn as a fan from its first vertex, i.e. split along
        // the 0-2 diagonal. With AO that split shows: an occluded corner
        // smears along whichever diagonal it lies on. Split along the brighter
        // pair instead (rotate the start to vertex 1), so a dark corner stays
        // in its own triangle. The winding is unchanged.
        const int start = (int(faceAO[i][1]) + faceAO[i][3] > int(faceAO[i][0]) + faceAO[i][2]) ? 1 : 0;

        for (int jj = 0; jj < 4; jj++) // for each node
        {
            const int j = (jj + start) % 4;
            auto fij = faces[i][j];
            // Edge jj of the drawn quad is edge j of the face.
            int mask = 0;
            for (int e = 0; e < 4; ++e)
                if (faceEdges[i] & (1u << ((e + start) % 4))) mask |= 1 << e;
            v1.ao = glVoxelPack(faceAO[i][j], mask, jj);
            v1.x = v[fij][0] + node_disp[fij][0];
            v1.y = v[fij][1] + node_disp[fij][1];
            v1.z = v[fij][2] + node_disp[fij][2];

            v1.a = vox.a;

            v1.r = node_colors[fij][0];
            v1.g = node_colors[fij][1];
            v1.b = node_colors[fij][2];

            //qDebug() << i << j << fij << v1.r << v1.g << v1.b << v1.x <<v1.y << v1.z;

            voxelScene.push_back(v1);
        }

    }
}

void OpenGLWidgetQML::DelayFrameUpdate()
{
    QThread::msleep(delayAnimation);
}

int32_t*** OpenGLWidgetQML::getVoxels()
{
    return voxels;
}

void OpenGLWidgetQML::toggleDebugMode()
{
    qDebug() << "OpenGLWidgetQML::toggleDebugMode() - Called from QML";
    if (m_render) {
        qDebug() << "OpenGLWidgetQML::toggleDebugMode() - Calling renderer method";
        m_render->toggleDebugMode();
        update();
    } else {
        qWarning() << "OpenGLWidgetQML::toggleDebugMode() - ERROR: m_render is null!";
    }
}

void OpenGLWidgetQML::toggleFaceCulling()
{
    qDebug() << "OpenGLWidgetQML::toggleFaceCulling() - Called from QML";
    if (m_render) {
        m_render->toggleFaceCulling();
        update();
    } else {
        qWarning() << "OpenGLWidgetQML::toggleFaceCulling() - ERROR: m_render is null!";
    }
}

void OpenGLWidgetQML::toggleDepthTest()
{
    qDebug() << "OpenGLWidgetQML::toggleDepthTest() - Called from QML";
    if (m_render) {
        m_render->toggleDepthTest();
        update();
    } else {
        qWarning() << "OpenGLWidgetQML::toggleDepthTest() - ERROR: m_render is null!";
    }
}


void OpenGLWidgetQML::explodedValueChanged(double value)
{
    if (qFuzzyCompare(distanceFactor, (float)value))
        return;
    value = value < 1 ? 0 : value;

    // setDistanceFactor() calls calculateScene() which rebuilds both
    // voxelScene and orientationVerts with the new explosion offset.
    setDistanceFactor(value);

    if (m_render) {
        m_render->updateVoxelData(voxelScene);
        // Push updated glyph positions so they track their grains.
        m_render->updateOrientationData(orientationVerts, orientationColors, orientationNormals);
        scheduleGlyphRebuild();   // tensor glyphs travel with their grain too
        qDebug() << "Exploded View Value Changed";
    } else {
        qWarning() << "OpenGLWidgetQML::explodedValueChanged() - ERROR: m_render is null!";
    }
}


// ── Orientation helpers ──────────────────────────────────────────────

void OpenGLWidgetQML::setGrainOrientations(
    const std::vector<std::array<float,3>>& orientations)
{
    grainOrientations = orientations;
    if (voxels) {
        calculateScene();   // rebuild voxel scene + orientation glyphs
        if (m_render) {
            m_render->updateVoxelData(voxelScene);
            m_render->updateOrientationData(orientationVerts, orientationColors, orientationNormals);
            update();
        }
    }
}

void OpenGLWidgetQML::setShowOrientations(bool show)
{
    qDebug() << "setShowOrientations(); show = " << show;
    showOrientations = show;
    if (m_render) {
        // Re-upload orientation data whenever the switch is turned on.
        // This covers the case where the data was built during the algorithm
        // run but not yet pushed to the renderer (e.g. showOrientations was
        // false at the time and the VBO was never dirtied).
        if (show)
            m_render->updateOrientationData(orientationVerts, orientationColors, orientationNormals);
        m_render->setShowOrientations(show);
        update();
    }
}

void OpenGLWidgetQML::setOrientationGlyphScale(float scale)
{
    orientationGlyphScale = scale;
    if (voxels)
        calculateScene();
}

// ── Bunge ZXZ rotation matrix ─────────────────────────────────────────
// Returns column-major 3×3 stored as R[col][row] so that
//   R[0] = crystal-X axis in sample frame
//   R[1] = crystal-Y axis in sample frame
//   R[2] = crystal-Z axis in sample frame
static void bungeZXZ(float phi1, float Phi, float phi2,
                     float R[3][3])
{
    const float c1 = cosf(phi1), s1 = sinf(phi1);
    const float c  = cosf(Phi),  s  = sinf(Phi);
    const float c2 = cosf(phi2), s2 = sinf(phi2);

    // Standard Bunge ZXZ: R = Rz(phi1)·Rx(Phi)·Rz(phi2)
    // Row-major storage; R[i][j] is row i, col j
    R[0][0] =  c1*c2 - s1*s2*c;
    R[0][1] = -c1*s2 - s1*c2*c;
    R[0][2] =  s1*s;

    R[1][0] =  s1*c2 + c1*s2*c;
    R[1][1] = -s1*s2 + c1*c2*c;
    R[1][2] = -c1*s;

    R[2][0] =  s2*s;
    R[2][1] =  c2*s;
    R[2][2] =  c;
}

void OpenGLWidgetQML::buildOrientationGlyphs()
{
    orientationVerts.clear();
    orientationColors.clear();
    orientationNormals.clear();

    // Always build the data — visibility is controlled by the renderer flag.
    // This ensures the VBO is populated before the user enables "Show orientations".
    if (grainOrientations.empty() || !voxels)
        return;

    // ── Pass 1: every grain's visible surface patches ────────────────────
    // The faces calculateScene() draws (solid view: against void or the
    // cube's boundary; exploded view: against any other grain), summed per
    // grain and per face direction: face-centre sum, normal sum, face count.
    // A triad goes on a patch, not at the grain's centre, so it marks the
    // grain where it is seen; grains with no visible face (inside the cube)
    // get none. One patch per direction: a grain on a cube edge shows on two
    // sides, and the mean over both pointed between them, often away from
    // the camera. The renderer shows, per grain, the largest patch turned
    // toward the camera.
    // acc[grainID * 6 + direction] = { sumX, sumY, sumZ, nX, nY, nZ, faces }
    std::unordered_map<int64_t, std::array<double,7>> acc;
    acc.reserve(static_cast<size_t>(numColors) + 1);
    const bool exploded = (distanceFactor > 0.0f);
    const int  halfI = numCubes / 2;            // calculateScene(): world = -(n/2 - k)
    static const int nb[6][3] = { {-1,0,0}, {1,0,0}, {0,-1,0}, {0,1,0}, {0,0,-1}, {0,0,1} };
    int dirIndex = 0;
    for (int x = 0; x < numCubes; ++x)
        for (int y = 0; y < numCubes; ++y)
            for (int z = 0; z < numCubes; ++z) {
                const int32_t id = voxels[x][y][z];
                if (id <= 0) continue;
                dirIndex = -1;
                for (const auto& d : nb) {
                    ++dirIndex;
                    const int X = x + d[0], Y = y + d[1], Z = z + d[2];
                    const bool inside = X >= 0 && Y >= 0 && Z >= 0 && X < numCubes && Y < numCubes && Z < numCubes;
                    const int32_t w = inside ? voxels[X][Y][Z] : 0;
                    if (exploded ? (w == id) : (w != 0)) continue;   // face not drawn
                    auto& a = acc[int64_t(id) * 6 + dirIndex];
                    a[0] += -(halfI - x) + 0.5 + 0.5 * d[0];
                    a[1] += -(halfI - y) + 0.5 + 0.5 * d[1];
                    a[2] += -(halfI - z) + 0.5 + 0.5 * d[2];
                    a[3] += d[0]; a[4] += d[1]; a[5] += d[2];
                    a[6] += 1.0;
                }
            }

    // Colours for the three local crystal axes
    static const GLubyte axisRGB[3][3] = {
        {230,  50,  50},   // crystal-X  ≈ red
        { 50, 200,  50},   // crystal-Y  ≈ green
        { 60, 130, 255},   // crystal-Z  ≈ blue
    };


    // Append one vertex (position + colour) to the flat VBO arrays.
    auto pushVertex = [&](float x, float y, float z,
                          GLubyte r, GLubyte g, GLubyte b)
    {
        orientationVerts.push_back(x);
        orientationVerts.push_back(y);
        orientationVerts.push_back(z);
        orientationColors.push_back(r / 255.0f);
        orientationColors.push_back(g / 255.0f);
        orientationColors.push_back(b / 255.0f);
    };

    // ── Pass 2: emit one orientation triad per grain ─────────────────────
    for (auto& [key, a] : acc) {
        const int32_t id = static_cast<int32_t>(key / 6);
        const int idx = id - 1;   // 0-based colour/orientation index
        // Specks of a few faces would carry triads larger than themselves.
        if (a[6] < 4.0) continue;
        if (idx < 0 || idx >= static_cast<int>(grainOrientations.size()))
            continue;

        // ── Grain centroid in world space ─────────────────────────────────
        // calculateScene() maps grid index k → world = -(half - k),
        // so the centroid is -(half - mean_grid_coord).
        float cx = static_cast<float>(a[0] / a[6]);
        float cy = static_cast<float>(a[1] / a[6]);
        float cz = static_cast<float>(a[2] / a[6]);

        // ── Exploded-view offset ──────────────────────────────────────────
        // calculateScene() shifts every voxel of a grain by a vector
        //   directionFactor * (colour/255 - 1) * distanceFactor
        // The centroid must receive the same rigid-body shift so the
        // glyph stays centred on its grain when exploded view is active.
        if (distanceFactor > 0.0f) {
            const size_t ci = static_cast<size_t>(idx);
            if (ci < colors.size() && ci < directionFactors.size()) {
                const auto& col = colors[ci];
                const float df  = directionFactors[ci];
                cx += df * (col[0] / 255.0f - 1.0f) * distanceFactor;
                cy += df * (col[1] / 255.0f - 1.0f) * distanceFactor;
                cz += df * (col[2] / 255.0f - 1.0f) * distanceFactor;
            }
        }

        // ── Per-grain line half-length ────────────────────────────────────
        // Treat the grain as a sphere of the same volume.
        //   voxelCount = (4/3)π r³  →  r = cbrt(3N / 4π)
        // L = orientationGlyphScale * r, so every grain gets a triad
        // proportional to its own physical size.
        // Each line is CENTRED on the centroid (tip-to-tip = 2 * halfL),
        // so both tips are at distance halfL from the centroid.  As long as
        // halfL > grainRadius both tips protrude past the surface and remain
        // visible even without special depth-test handling.
        // Length from the patch: its area A (faces) as a disc of radius
        // sqrt(A / pi), half of that at the default scale, at least 1.5
        // voxels and at most 12 % of the cube edge, so big grains do not
        // reach across their neighbours.
        const float patchRadius = std::sqrt(static_cast<float>(a[6]) / static_cast<float>(M_PI));
        const float halfL = std::clamp(0.5f * patchRadius, 1.5f, std::max(1.5f, 0.12f * numCubes))
                            * (orientationGlyphScale / 1.5f);

        // ── Bunge ZXZ rotation matrix ─────────────────────────────────────
        const auto& eu = grainOrientations[static_cast<size_t>(idx)];
        const float phi1 = eu[0] * static_cast<float>(M_PI) / 180.0f;
        const float Phi  = eu[1] * static_cast<float>(M_PI) / 180.0f;
        const float phi2 = eu[2] * static_cast<float>(M_PI) / 180.0f;

        float R[3][3];
        bungeZXZ(phi1, Phi, phi2, R);

        // ── Emit three centred line segments ──────────────────────────────
        // Column `axis` of R is the unit vector of crystal axis `axis`
        // expressed in the sample (world) frame.
        // The line runs from  centroid - halfL*dir
        //                 to  centroid + halfL*dir
        for (int axis = 0; axis < 3; ++axis) {
            const float hx = halfL * R[0][axis];
            const float hy = halfL * R[1][axis];
            const float hz = halfL * R[2][axis];

            const GLubyte r = axisRGB[axis][0];
            const GLubyte g = axisRGB[axis][1];
            const GLubyte b = axisRGB[axis][2];

            pushVertex(cx, cy, cz, r, g, b);
            //pushVertex(cx - hx, cy - hy, cz - hz, r, g, b);  // negative tip
            pushVertex(cx + hx, cy + hy, cz + hz, r, g, b);  // positive tip
        }
        // The patch's outward normal, its area and its grain: the renderer
        // shows, per grain, the largest patch that faces the camera.
        orientationNormals.push_back(static_cast<float>(a[3] / a[6]));
        orientationNormals.push_back(static_cast<float>(a[4] / a[6]));
        orientationNormals.push_back(static_cast<float>(a[5] / a[6]));
        orientationNormals.push_back(static_cast<float>(a[6]));
        orientationNormals.push_back(static_cast<float>(id));
    }
}
