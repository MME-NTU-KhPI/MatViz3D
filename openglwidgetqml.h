
#ifndef OPENGLWIDGETQML
#define OPENGLWIDGETQML

#include <QQuickFramebufferObject>
#include <QOpenGLFunctions>
#include <QFutureWatcher>
#include <QPointF>
#include <memory>
#include "ansyswrapper.h"
#include "cornergizmo.hpp"
#include "colormap.hpp"
#include "renderopengl.h"
#include "stressresult.h"
#include "tensorfieldsnapshot.h"
#include "tensorglyphbuilder.h"

class QTimer;


class RenderOpenGL;

class OpenGLWidgetQML : public QQuickFramebufferObject {
    Q_OBJECT

    QML_ELEMENT
protected:
    static RenderOpenGL* m_render;
    static OpenGLWidgetQML* instance;
public:
    explicit OpenGLWidgetQML(QQuickItem *parent = nullptr);
    Renderer *createRenderer() const override;
    virtual ~OpenGLWidgetQML();

    static OpenGLWidgetQML* getInstance();
    /**
     * Set the voxel data.
     * @param voxels 3D array of voxel data.
     * @param numCubes Number of cubes per dimension.
     */
    void setVoxels(int32_t*** voxels, short int numCubes);

    /**
     * Retrieve the voxel data.
     * @return Pointer to the 3D voxel array.
     */
    int32_t*** getVoxels();

    QImage captureScreenshot();
    QImage captureScreenshotWithWhiteBackground();
    void captureScreenshotToClipboard();
    // header, public:
    Q_INVOKABLE void requestSvgExport(const QString& path);


    std::vector<std::array<GLubyte, 4>> generateDistinctColors();

    Q_INVOKABLE void setPlotWireFrame(bool status);
    /** Colors of the currently selected palette (see colorMapPalette), low value -> high value. */
    Q_INVOKABLE QVector<QColor> getColorMap(int numLevels);

    const int cubeSize = 1;

    // ── Stress/strain/displacement field visualization ─────────────────────
    enum class FieldMode { None, Ansys, FFT };

    // Palette used to color the field. Rainbow is sequential (matches the
    // original hardcoded 9-band map); CoolWarm/RdBu are diverging with white
    // at the middle of the current min/max range; Viridis is a perceptually
    // uniform sequential map; Grayscale is black (low) -> white (high).
    // Defined in colormap.hpp so the elastic-surface view shares one palette
    // source; the ordinals are part of the QML API and must not be reordered.
    using ColorMapPalette = matviz_cmap::Palette;

    Q_PROPERTY(int colorMapPalette READ colorMapPaletteIndex WRITE setColorMapPalette NOTIFY colorMapPaletteChanged)
    int colorMapPaletteIndex() const { return int(colorMapPalette); }
    Q_INVOKABLE void setColorMapPalette(int palette);

    // Show a field from an ANSYS single-shot solve. wr is kept alive (its
    // per-node result table is what getValByCoord()/scaleValue01() read) for
    // as long as the field stays displayed.
    void showAnsysField(std::shared_ptr<ansysWrapper> wr, int component, bool deformed = false, float scale = 1.0f);
    // Show a field from an FFT single-shot solve (dense per-voxel arrays).
    void showFFTField(std::shared_ptr<FieldVisualizationData> data, int component, bool deformed = false, float scale = 1.0f);

    /** Switch which component of the currently-shown field is plotted, without re-solving. */
    Q_INVOKABLE void setFieldComponent(int component);
    /** Revert the 3D view to plain per-grain coloring. */
    Q_INVOKABLE void clearFieldVisualization();
    Q_INVOKABLE void setShowDeformed(bool show);
    Q_INVOKABLE void setDeformedScale(float scale);

    /**
     * Supply Euler angles (Bunge ZXZ, degrees) for every grain.
     * Index 0 = grain ID 1, etc.  Pass an empty vector to clear.
     */
    void setGrainOrientations(const std::vector<std::array<float,3>>& orientations);
    const std::vector<std::array<float,3>>& getGrainOrientations() const { return grainOrientations; }

    Q_INVOKABLE void setShowOrientations(bool show);

    /** Scale of each orientation triad relative to one voxel unit. */
    void setOrientationGlyphScale(float scale);

    // ── Tensor field overlays ───────────────────────────────────────────
    // All of this is inert until a solve has produced a field AND the user
    // switches an overlay on: the per-voxel tensor snapshot is built lazily on
    // first use, so headless runs and ordinary structure generation pay nothing.
    //
    // Controls that only change a renderer flag (visibility, voxel opacity) go
    // straight through and cost no CPU. Controls that change geometry go
    // through scheduleGlyphRebuild(), which coalesces slider drags onto a short
    // timer instead of rebuilding the mesh on every pixel of travel.

    /** True once a solved field is present, i.e. the panel has something to show. */
    Q_PROPERTY(bool tensorAvailable READ tensorAvailable NOTIFY tensorStateChanged)
    Q_PROPERTY(int  glyphCount      READ glyphCount      NOTIFY tensorStateChanged)
    /** True when the selected tensor source is absent from the current field. */
    Q_PROPERTY(bool tensorSourceMissing READ tensorSourceMissing NOTIFY tensorStateChanged)

    bool tensorAvailable() const { return fieldMode != FieldMode::None; }
    int  glyphCount() const { return m_glyphCount; }
    bool tensorSourceMissing() const { return m_glyphSourceMissing; }

    Q_INVOKABLE void setShowGlyphs(bool show);
    Q_INVOKABLE void setVoxelOpacity(qreal opacity);

    /** 0 = stress, 1 = strain. */
    Q_INVOKABLE void setTensorSource(int source);
    Q_INVOKABLE void setTensorDeviatoric(bool on);
    /** Sampling stride in voxels; <= 0 selects one automatically from the budget. */
    Q_INVOKABLE void setGlyphStride(int stride);
    Q_INVOKABLE void setGlyphScale(qreal scale);
    Q_INVOKABLE void setGlyphSharpness(qreal gamma);
    /** See GlyphColorMode. */
    Q_INVOKABLE void setGlyphColorMode(int mode);
    /** axis: -1 none, 0 = x, 1 = y, 2 = z. */
    Q_INVOKABLE void setGlyphSlice(int axis, int index);

    // ── Hyperstreamlines ────────────────────────────────────────────────
    // Integration is unbounded in the worst case (a smooth field can carry a
    // curve for thousands of steps), so unlike the glyphs this runs off the main
    // thread. Results are stamped with a generation counter and discarded if the
    // parameters moved on while the job was running.
    Q_PROPERTY(bool streamlinesBusy READ streamlinesBusy NOTIFY tensorStateChanged)
    Q_PROPERTY(int  streamlineCount READ streamlineCount NOTIFY tensorStateChanged)

    bool streamlinesBusy() const { return m_streamBusy; }
    int  streamlineCount() const { return m_streamCount; }

    // Screen positions of the corner-triad axis labels, in logical pixels of
    // this item. The QML overlay binds its X/Y/Z Text elements to these, so
    // the labels track the triad as the camera rotates.
    Q_PROPERTY(QPointF axisLabelX READ axisLabelX NOTIFY axisLabelsChanged)
    Q_PROPERTY(QPointF axisLabelY READ axisLabelY NOTIFY axisLabelsChanged)
    Q_PROPERTY(QPointF axisLabelZ READ axisLabelZ NOTIFY axisLabelsChanged)

    QPointF axisLabelX() const { return projectAxisLabel(QVector3D(1, 0, 0)); }
    QPointF axisLabelY() const { return projectAxisLabel(QVector3D(0, 1, 0)); }
    QPointF axisLabelZ() const { return projectAxisLabel(QVector3D(0, 0, 1)); }

    Q_INVOKABLE void setShowStreamlines(bool show);
    Q_INVOKABLE void setStreamlineSeedStride(int stride);
    Q_INVOKABLE void setStreamlineMaxLines(int lines);
    Q_INVOKABLE void setStreamlineStep(qreal voxels);
    Q_INVOKABLE void setStreamlineMinLinearity(qreal cl);
    Q_INVOKABLE void setStreamlineTubeRadius(qreal radius);


protected:
    //struct RenderOpenGL::Voxel;
    void initializeGL();
    void paintGL();
    void drawAxis();
    void initLights();
    void drawCube(short cubeSize, RenderOpenGL::Voxel vox, bool* neighbors,
                  std::vector<std::array<GLubyte, 4>> &node_colors,
                  const std::array<std::array<float, 3>, 8> &node_disp);
    QSize minimumSizeHint() const;
    QSize sizeHint() const;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

    void qNormalizeAngle(int &angle);

    void calculateScene();
    // calculateScene() only rebuilds the CPU-side voxelScene vector -- every
    // caller must also push it to the renderer (RenderOpenGL keeps its own
    // copy) or nothing visibly changes. Mirrors the push done inline by
    // setVoxels()/explodedValueChanged()/setGrainOrientations().
    void pushSceneToRenderer();

    std::vector<std::array<GLubyte, 4>> createColorMap(int numLevels, ColorMapPalette palette);

    std::array<GLubyte, 4> scalarToColor(float value, const std::vector<std::array<GLubyte, 4>>& colorMap);

    ColorMapPalette colorMapPalette = ColorMapPalette::Rainbow;

    // ── Field visualization state ───────────────────────────────────────
    FieldMode fieldMode = FieldMode::None;
    std::shared_ptr<ansysWrapper>           ansysField;
    std::shared_ptr<FieldVisualizationData> fftField;
    int   fieldComponent = 0;
    bool  showDeformed   = false;
    float deformedScale  = 1.0f;

    // Euler angles per grain: [phi1_deg, Phi_deg, phi2_deg], indexed by (grainID - 1)
    std::vector<std::array<float,3>> grainOrientations;

    // Flat geometry for orientation glyphs: positions (x0,y0,z0, x1,y1,z1) × N lines
    std::vector<float> orientationVerts;   // 6 floats per line vertex pair
    std::vector<float> orientationColors;  // 3 floats per vertex

    bool   showOrientations     = false;
    float  orientationGlyphScale = 1.5f;   // in voxel units

    void buildOrientationGlyphs();           // called from calculateScene()

    // ── Tensor overlay state ────────────────────────────────────────────
    std::shared_ptr<const TensorFieldSnapshot> tensorSnapshot;
    GlyphParams glyphParams;
    bool    showGlyphs           = false;
    float   voxelOpacity         = 1.0f;
    int     m_glyphCount         = 0;
    bool    m_glyphSourceMissing = false;
    QTimer* glyphRebuildTimer    = nullptr;

    /// Build the per-voxel tensor snapshot if it is missing. Returns false when
    /// there is no solved field to build one from.
    bool ensureTensorSnapshot();
    /// Drop the snapshot; the next overlay that needs it rebuilds it.
    void invalidateTensorSnapshot();
    /// Rebuild glyph geometry now and push it to the renderer.
    void rebuildGlyphs();
    /// Rebuild soon, coalescing a burst of parameter changes into one build.
    void scheduleGlyphRebuild();
    /// Per-grain exploded-view offsets, same derivation as calculateScene(),
    /// so glyphs travel with the grain they belong to.
    std::vector<std::array<float, 3>> buildGrainOffsets() const;

    // ── Streamline state ────────────────────────────────────────────────
    StreamlineParams streamParams;
    bool showStreamlines = false;
    bool m_streamBusy    = false;
    int  m_streamCount   = 0;

    /// Bumped on every parameter change that invalidates an in-flight job. The
    /// stamp travels with the result so a finished worker can be recognised as
    /// stale and thrown away, rather than a slow job overwriting a newer fast
    /// one. Carried in this wrapper rather than in StreamlineMesh so the builder
    /// stays free of UI bookkeeping.
    struct StreamJob { int generation = 0; StreamlineMesh mesh; };

    int m_streamGeneration = 0;
    QFutureWatcher<StreamJob>* streamWatcher = nullptr;

    void rebuildStreamlines();
    void onStreamlinesFinished();


public slots:
    // slots for xyz-rotation slider
    void setXRotation(int angle);
    void setYRotation(int angle);
    void setZRotation(int angle);

public:
    void setFrontView();
    void setBackView();
    void setTopView();
    void setBottomView();
    void setLeftView();
    void setRightView();
    void setIsometricView();
    void setDimetricView();
    void setIsometricDownView();
    void setDimetricDownView();

    void setNumCubes(int numCubes);
    void setNumColors(int numColors);
    void setDistanceFactor(int factor);
    void setAnsysWrapper(ansysWrapper *wr);
    void DelayFrameUpdate();

    void setSceneParent(QQuickItem *parentItem);
    void setParentWidget(QWidget *parent);

    Q_INVOKABLE void setDelayAnimation(int delayAnimation);
    Q_INVOKABLE void toggleDebugMode();
    Q_INVOKABLE void toggleFaceCulling();
    Q_INVOKABLE void toggleDepthTest();
    Q_INVOKABLE void explodedValueChanged(double value);

    Q_INVOKABLE void zoomIn();
    Q_INVOKABLE void zoomOut();
    Q_INVOKABLE void zoomToFit();
signals:
    // signaling rotation from mouse movement
    void xRotationChanged(int angle);
    void yRotationChanged(int angle);
    void zRotationChanged(int angle);
    void colorMapPaletteChanged();
    void tensorStateChanged();
    void axisLabelsChanged();

private:
    QTimer* timer;

private:
    GLuint vboIds[3];
    /// Projects a unit axis direction onto this item, reproducing the
    /// rotation-only MVP that drawCornerAxes() uses for the triad.
    QPointF projectAxisLabel(const QVector3D& dir) const;
    void pushRotations();

protected:
    int xRot;
    int yRot;
    int zRot;
    int delayAnimation;

    float distance;
    float zoomFactor = 1.0f;
    float panX = 0.0f;
    float panY = 0.0f;
    void zoomStep(int numSteps);
    float calculateFitDistance() const;

    QColor bgColor;

    QPoint lastPos;
    float distanceFactor = 0;

    int32_t*** voxels;
    short int numCubes;
    int numColors;

    std::vector<std::array<GLubyte, 4>> colors;
    std::vector<float> directionFactors;

    bool isVBOupdateRequired = false;

    // struct Voxel
    // {
    //     GLfloat x, y, z; // Coordinates
    //     GLubyte r, g, b, a; // Color attributes
    //     GLbyte nx, ny, nz; // Normal attributes
    // };
    std::vector<RenderOpenGL::Voxel> voxelScene;

    bool plotWireFrame = false;

    void handleResize();


};




#endif
