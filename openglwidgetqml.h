
#ifndef OPENGLWIDGETQML
#define OPENGLWIDGETQML

#include <QQuickFramebufferObject>
#include <QOpenGLFunctions>
#include <memory>
#include "ansyswrapper.h"
#include "colormap.hpp"
#include "renderopengl.h"
#include "stressresult.h"


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
    void showAnsysField(std::shared_ptr<ansysWrapper> wr, int component);
    // Show a field from an FFT single-shot solve (dense per-voxel arrays).
    void showFFTField(std::shared_ptr<FieldVisualizationData> data, int component);

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

    Q_INVOKABLE void setShowOrientations(bool show);

    /** Scale of each orientation triad relative to one voxel unit. */
    void setOrientationGlyphScale(float scale);


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
signals:
    // signaling rotation from mouse movement
    void xRotationChanged(int angle);
    void yRotationChanged(int angle);
    void zRotationChanged(int angle);
    void colorMapPaletteChanged();

private:
    QTimer* timer;

private:
    GLuint vboIds[3];

protected:
    int xRot;
    int yRot;
    int zRot;
    int delayAnimation;

    float distance;
    float zoomFactor = 1.0f;
    void zoomStep(int numSteps);

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
