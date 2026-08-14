#include "openglwidgetqml.h"
#include "qquickwindow.h"
#include "renderopengl.h"
#include "ansyswrapper.h"

#include <QGuiApplication>
#include <QClipboard>
#include <QTimer>
#include <QtConcurrent>
#include <QImage>
#include <QThread>
#include <algorithm>

RenderOpenGL* OpenGLWidgetQML::m_render = nullptr;
OpenGLWidgetQML* OpenGLWidgetQML::instance = nullptr;

OpenGLWidgetQML::OpenGLWidgetQML(QQuickItem *parent) : QQuickFramebufferObject(parent) {
    setAcceptedMouseButtons(Qt::AllButtons);
    setTextureFollowsItemSize(true);

    xRot = 0;
    yRot = 0;
    zRot = 0;
    distance = 2.0f;
    numCubes = 1;
    voxels = nullptr;
    timer = new QTimer(this);
    delayAnimation = 0;
    bgColor.setRgbF(0.21f, 0.21f, 0.21f);
    connect(this, &QQuickItem::widthChanged, this, &OpenGLWidgetQML::handleResize, Qt::QueuedConnection);
    connect(this, &QQuickItem::heightChanged, this, &OpenGLWidgetQML::handleResize, Qt::QueuedConnection);

    // Only set instance if null (or warn if multiple instances)
    if (instance == nullptr)
    {
        instance = this;
    }
    else
    {
        qWarning() << "Multiple OpenGLWidgetQML instances detected";
    }
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
    m_render->resizeGL(this->width(), this->height());
    m_render->setDevicePixelRatio(window() ? window()->devicePixelRatio() : 1.0f);

    // A fresh renderer starts with default overlay state and an empty glyph
    // buffer, so anything the user had switched on has to be restored -- the
    // scene graph can recreate the renderer at any time (window re-show, device
    // loss) and the overlay would otherwise silently vanish until the next
    // control change. Flags are pushed directly; the geometry needs a rebuild,
    // which cannot happen from a const method, so it is deferred to the event
    // loop.
    m_render->setShowGlyphs(showGlyphs);
    m_render->setShowStreamlines(showStreamlines);
    m_render->setVoxelOpacity(voxelOpacity);
    if (showGlyphs || showStreamlines) {
        auto* self = const_cast<OpenGLWidgetQML*>(this);
        QTimer::singleShot(0, self, [self]() {
            if (self->showGlyphs)      self->rebuildGlyphs();
            if (self->showStreamlines) self->rebuildStreamlines();
        });
    }

    return m_render;
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
    instance = nullptr;
}


void OpenGLWidgetQML::qNormalizeAngle(int &angle)
{
    while (angle < -360 * 16)
        angle += 360 * 16;
    while (angle >= 360 * 16)
        angle -= 360 * 16;
}

void OpenGLWidgetQML::setXRotation(int angle)
{
    qNormalizeAngle(angle);
    if (angle != xRot) {
        xRot = angle;
        if (m_render) {
            m_render->setRotations(xRot, yRot, zRot);
        }
        update();
    }
}

void OpenGLWidgetQML::setYRotation(int angle)
{
    qNormalizeAngle(angle);
    if (angle != yRot) {
        yRot = angle;
        if (m_render) {
            m_render->setRotations(xRot, yRot, zRot);
        }
        update();
    }
}

void OpenGLWidgetQML::setZRotation(int angle)
{
    qNormalizeAngle(angle);
    if (angle != zRot) {
        zRot = angle;
        if (m_render) {
            m_render->setRotations(xRot, yRot, zRot);
        }
        update();
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
    if (m_render) {
        m_render->setRotations(xRot, yRot, zRot);
    }
    update();
}

void OpenGLWidgetQML::setTopView() {
    xRot = 90 * 16;
    yRot = 0;
    zRot = 0;
    if (m_render) {
        m_render->setRotations(xRot, yRot, zRot);
    }
    update();
}

void OpenGLWidgetQML::setBottomView() {
    xRot = -90 * 16;
    yRot = 0;
    zRot = 0;
    if (m_render) {
        m_render->setRotations(xRot, yRot, zRot);
    }
    update();
}

void OpenGLWidgetQML::setLeftView() {
    xRot = 0;
    yRot = 90 * 16;
    zRot = 0;
    if (m_render) {
        m_render->setRotations(xRot, yRot, zRot);
    }
    update();
}

void OpenGLWidgetQML::setRightView() {
    xRot = 0;
    yRot = -90 * 16;
    zRot = 0;
    if (m_render) {
        m_render->setRotations(xRot, yRot, zRot);
    }
    update();
}

/**
 * Set the view to standard isometric projection.
 */
void OpenGLWidgetQML::setIsometricView()
{
    xRot =  35.26 * 16;
    yRot = -45.00 * 16;
    zRot =  0;
    if (m_render) {
        m_render->setRotations(xRot, yRot, zRot);
    }
    update();
}

/**
 * Set the view to standard dimetric projection.
 */
void OpenGLWidgetQML::setDimetricView()
{
    xRot =  26.57 * 16;
    yRot = -45.00 * 16;
    zRot =  0;
    if (m_render) {
        m_render->setRotations(xRot, yRot, zRot);
    }
    update();
}

void OpenGLWidgetQML::setNumCubes(int numCubes)
{
    distance = 2 * numCubes;
    this->numCubes = numCubes;
    this->fieldMode = FieldMode::None;
    this->ansysField.reset();
    this->fftField.reset();
    if (m_render) {
        m_render->setNumCubes(numCubes);
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
    this->plotWireFrame = status;
    if (m_render) {
        m_render->setPlotWireFrame(status);
    }
}

/**
 * Capture a screenshot of the OpenGL widget and store it in a buffer.
 * @return A QImage containing the screenshot.
 */
QImage OpenGLWidgetQML::captureScreenshot()
{
    QImage screenshot;
    if (m_render)
    {
        screenshot = m_render->captureScreenshot();
    }

    return screenshot;
}

/**
 * Capture a screenshot and copy it to the system clipboard.
 */
void OpenGLWidgetQML::captureScreenshotToClipboard()
{
    QImage screenshot = captureScreenshotWithWhiteBackground();
    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setImage(screenshot);
}

/**
 * Capture a screenshot of the OpenGL widget, replace the background with white, and store it in a buffer.
 * @return A QImage containing the modified screenshot.
 */
QImage OpenGLWidgetQML::captureScreenshotWithWhiteBackground()
{
    QImage screenshot = captureScreenshot();

    // Modify the screenshot to replace gray background with white
    QImage modifiedScreenshot(screenshot.size(), QImage::Format_RGB32);
    const float tol = 0.015f;
    for (int y = 0; y < screenshot.height(); ++y) {
        for (int x = 0; x < screenshot.width(); ++x) {
            QColor pixelColor = screenshot.pixelColor(x, y);

            // Replace gray bg color with white
            if (
                fabs(pixelColor.redF() - bgColor.redF()) <= tol  &&
                fabs(pixelColor.greenF() - bgColor.greenF()) <= tol &&
                fabs(pixelColor.blueF() - bgColor.blueF()) <=tol
                )
            {
                modifiedScreenshot.setPixelColor(x, y, Qt::white);
            }
            else
            {
                modifiedScreenshot.setPixelColor(x, y, pixelColor);
            }
        }
    }

    return modifiedScreenshot;
}

void OpenGLWidgetQML::wheelEvent(QWheelEvent *event)
{
    int numDegrees = event->angleDelta().y() / 8;
    int numSteps = numDegrees / 10;

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

void OpenGLWidgetQML::zoomStep(int numSteps)
{
    if (numSteps > 0) {
        zoomFactor *= 1.1f;
        distance -= numSteps * numCubes * 0.1f;
    } else if (numSteps < 0) {
        zoomFactor /= 1.1f;
        distance += -numSteps * numCubes * 0.1f;
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
        setXRotation(xRot + 8 * dy);
        setZRotation(zRot + 8 * dx);
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
    palette = std::min(int(ColorMapPalette::Grayscale), std::max(0, palette));
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

void OpenGLWidgetQML::pushSceneToRenderer()
{
    if (m_render) {
        m_render->updateVoxelData(voxelScene);
        m_render->updateOrientationData(orientationVerts, orientationColors);
    }
    update();
}

void OpenGLWidgetQML::showAnsysField(std::shared_ptr<ansysWrapper> wr, int component)
{
    this->ansysField    = wr;
    this->fftField.reset();
    this->fieldMode      = FieldMode::Ansys;
    this->fieldComponent = component;
    invalidateTensorSnapshot();
    this->calculateScene();
    pushSceneToRenderer();
    emit tensorStateChanged();
}

void OpenGLWidgetQML::showFFTField(std::shared_ptr<FieldVisualizationData> data, int component)
{
    this->fftField       = data;
    this->ansysField.reset();
    this->fieldMode       = FieldMode::FFT;
    this->fieldComponent  = component;
    invalidateTensorSnapshot();
    this->calculateScene();
    pushSceneToRenderer();
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
    std::vector<std::array<GLubyte, 4>> colors;
    if (numColors <= 0) {
        return std::vector<std::array<GLubyte, 4>>();
    }
    float hueIncrement = 360.0f / numColors;


    for (int i = 0; i < numColors; ++i) {
        float hue = i * hueIncrement;
        float saturation = 1.0f;
        float value = 1.0f;
        float alpha = 0.85f;

        float chroma = saturation * value;
        float huePrime = hue / 60.0f;
        float x = chroma * (1.0f - std::abs(std::fmod(huePrime, 2.0f) - 1.0f));
        float r, g, b;

        if (huePrime >= 0 && huePrime < 1)
        {
            r = chroma;
            g = x;
            b = 0;
        } else if (huePrime >= 1 && huePrime < 2)
        {
            r = x;
            g = chroma;
            b = 0;
        } else if (huePrime >= 2 && huePrime < 3)
        {
            r = 0;
            g = chroma;
            b = x;
        } else if (huePrime >= 3 && huePrime < 4)
        {
            r = 0;
            g = x;
            b = chroma;
        } else if (huePrime >= 4 && huePrime < 5)
        {
            r = x;
            g = 0;
            b = chroma;
        } else
        {
            r = chroma;
            g = 0;
            b = x;
        }

        float m = value - chroma;
        r += m;
        g += m;
        b += m;

        colors.push_back({GLubyte(r*255), GLubyte(g*255), GLubyte(b*255), GLubyte(alpha*255)});
    }

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

                if (index >= colors.size() || colors.size() == 0)
                {
                    qCritical() << "Invalid voxel color index:" << index << "size:" << colors.size();
                    continue;
                }
                auto color = colors[index].data();


                GLfloat refColor[] = {1.0f, 1.0f, 1.0f};
                GLfloat diff[] = {  GLfloat(color[0]/255.0 - refColor[0]),
                    GLfloat(color[1]/255.0 - refColor[1]),
                    GLfloat(color[2]/255.0 - refColor[2])
                };

                // Define a direction factor, which can be negative or positive
                float directionFactor = directionFactors[index];

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

                drawCube(cubeSize, v, neighbors, node_colors, node_disp);
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
    voxelScene.clear();
    invalidateTensorSnapshot();   // new structure: any cached tensor field is stale
    calculateScene();
    if (m_render)
    {
        this->setNumCubes(numCubes);
        m_render->setDevicePixelRatio(window() ? window()->devicePixelRatio() : 1.0f);
        m_render->setDistZoomFactor(distance, zoomFactor);
        m_render->setNumCubes(numCubes);
        m_render->updateVoxelData(voxelScene);
        m_render->updateOrientationData(orientationVerts, orientationColors);
        m_render->resizeGL(this->width(), this->height());
    }
}

void OpenGLWidgetQML::drawCube(short cubeSize, RenderOpenGL::Voxel vox, bool* neighbors,
                                std::vector<std::array<GLubyte, 4>> &node_colors,
                                const std::array<std::array<float, 3>, 8> &node_disp)
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

    static const GLint faces[6][4] =
        {
            {0, 1, 2, 3},
            {3, 2, 6, 7},
            {7, 6, 5, 4},
            {4, 5, 1, 0},
            {5, 6, 2, 1},
            {7, 4, 0, 3}
        };
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

        for (int j = 0; j < 4; j++) // for each node
        {
            auto fij = faces[i][j];
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
        m_render->updateOrientationData(orientationVerts, orientationColors);
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
            m_render->updateOrientationData(orientationVerts, orientationColors);
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
            m_render->updateOrientationData(orientationVerts, orientationColors);
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

    // Always build the data — visibility is controlled by the renderer flag.
    // This ensures the VBO is populated before the user enables "Show orientations".
    if (grainOrientations.empty() || !voxels)
        return;

    // ── Pass 1: accumulate centroid AND voxel count per grain ────────────
    // acc[grainID] = { sumX, sumY, sumZ, voxelCount }
    std::unordered_map<int32_t, std::array<double,4>> acc;
    acc.reserve(static_cast<size_t>(numColors) + 1);

    for (int x = 0; x < numCubes; ++x)
        for (int y = 0; y < numCubes; ++y)
            for (int z = 0; z < numCubes; ++z) {
                int32_t id = voxels[x][y][z];
                if (id <= 0) continue;
                auto& a = acc[id];
                a[0] += x;  a[1] += y;  a[2] += z;  a[3] += 1.0;
            }

    // Colours for the three local crystal axes
    static const GLubyte axisRGB[3][3] = {
        {230,  50,  50},   // crystal-X  ≈ red
        { 50, 200,  50},   // crystal-Y  ≈ green
        { 60, 130, 255},   // crystal-Z  ≈ blue
    };

    const float half = static_cast<float>(numCubes) / 2.0f;

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
    for (auto& [id, a] : acc) {
        const int idx = id - 1;   // 0-based colour/orientation index
        if (idx < 0 || idx >= static_cast<int>(grainOrientations.size()))
            continue;

        // ── Grain centroid in world space ─────────────────────────────────
        // calculateScene() maps grid index k → world = -(half - k),
        // so the centroid is -(half - mean_grid_coord).
        float cx = -(half - static_cast<float>(a[0] / a[3]));
        float cy = -(half - static_cast<float>(a[1] / a[3]));
        float cz = -(half - static_cast<float>(a[2] / a[3]));

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
        const float grainRadius = std::cbrt(
            static_cast<float>(a[3]) * 3.0f / (4.0f * static_cast<float>(M_PI)));
        const float halfL = orientationGlyphScale * grainRadius;

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
    }
}
