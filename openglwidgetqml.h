
#ifndef OPENGLWIDGETQML
#define OPENGLWIDGETQML

#include <QQuickFramebufferObject>
#include <QOpenGLFunctions>
#include "ansyswrapper.h"


class RenderOpenGL;

class OpenGLWidgetQML : public QQuickFramebufferObject {
    Q_OBJECT

    QML_ELEMENT
protected:
    static RenderOpenGL* m_render;

public:
    explicit OpenGLWidgetQML(QQuickItem *parent = nullptr);
    Renderer *createRenderer() const override;
    virtual ~OpenGLWidgetQML();


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

    void setPlotWireFrame(bool status);
    QVector<QColor> getColorMap(int numLevels);
    void setComponent(int index);

    int cubeSize = 1;
signals:
protected:
    struct Voxel;
    void initializeGL();
    void paintGL();
    void drawAxis();
    void initLights();
    void resizeGL(int width, int height);
    void paintSphere(float radius, int numStacks, int numSlices);
    void drawCube(float cubeSize, GLenum type = GL_QUADS);
    void drawCube(short cubeSize, Voxel vox, bool* neighbors, std::vector<std::array<GLubyte, 4>> &node_colors);
    QSize minimumSizeHint() const;
    QSize sizeHint() const;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

    void qNormalizeAngle(int &angle);

    void calculateScene();
    void initializeVBO();
    void updateVBO();

    std::vector<std::array<GLubyte, 4>> createColorMap(int numLevels);

    std::array<GLubyte, 4> scalarToColor(float value, const std::vector<std::array<GLubyte, 4>>& colorMap);
    ansysWrapper *wr;
    int plotComponent;

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
    void setDelayAnimation(int delayAnimation);
    void setAnsysWrapper(ansysWrapper *wr);
    void DelayFrameUpdate();

    void setSceneParent(QQuickItem *parentItem);
    void setParentWidget(QWidget *parent);
signals:
    // signaling rotation from mouse movement
    void xRotationChanged(int angle);
    void yRotationChanged(int angle);
    void zRotationChanged(int angle);
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

    QColor bgColor;

    QPoint lastPos;
    float distanceFactor = 0;

    int32_t*** voxels;
    short int numCubes;
    int numColors;

    std::vector<std::array<GLubyte, 4>> colors;
    std::vector<float> directionFactors;

    void updateVoxelColor(Voxel &v1);

    bool isVBOupdateRequired = false;

    struct Voxel
    {
        GLfloat x, y, z; // Coordinates
        GLubyte r, g, b, a; // Color attributes
        GLbyte nx, ny, nz; // Normal attributes
    };
    std::vector<Voxel> voxelScene;

    bool plotWireFrame = false;

    void handleResize();


};




#endif
