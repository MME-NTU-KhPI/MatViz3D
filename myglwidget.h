#ifndef MYGLWIDGET_H
#define MYGLWIDGET_H

#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>

#include <QtOpenGLWidgets/QOpenGLWidget>
#include <QMatrix4x4>
#include <QOpenGLFunctions>
#include "ansysWrapper.h"

/**
 * @class MyGLWidget
 * A custom OpenGL widget for rendering 3D voxel scenes with interaction capabilities.
 */
class MyGLWidget : public QOpenGLWidget
{
    Q_OBJECT
    /**
     * OpenGL debug callback for logging messages.
     */
    static void debugCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                              GLsizei length, const GLchar* message, const void* userParam);

public:
    /**
     * Constructor to initialize the OpenGL widget.
     * @param parent The parent widget.
     */
    explicit MyGLWidget(QWidget *parent = 0);

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
    void repaint_function();

    QImage captureScreenshot();
    QImage captureScreenshotWithWhiteBackground();
    void captureScreenshotToClipboard();


    std::vector<std::array<GLubyte, 4>> generateDistinctColors();

    void setPlotWireFrame(bool status);
    QVector<QColor> getColorMap(int numLevels);
    void setComponent(int index);
    ~MyGLWidget();
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
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void wheelEvent(QWheelEvent *event);

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
    void setIsometricView();
    void setDimetricView();

    void setNumCubes(int numCubes);
    void setNumColors(int numColors);
    void setDistanceFactor(int factor);
    void setDelayAnimation(int delayAnimation);
    void setAnsysWrapper(ansysWrapper *wr);
    void update_function();
    void updateGLWidget(int32_t*** voxels, short int numCubes);
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

    QColor bgColor;

    QPoint lastPos;
    QMatrix4x4 m_projection;
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

};

#endif // MYGLWIDGET_H
