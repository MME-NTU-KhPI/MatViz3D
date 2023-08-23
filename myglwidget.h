#ifndef MYGLWIDGET_H
#define MYGLWIDGET_H

#include <QtOpenGLWidgets/QOpenGLWidget>
#include <QMatrix4x4>
#include <QOpenGLFunctions>



class MyGLWidget : public QOpenGLWidget
{
    Q_OBJECT
public:
    explicit MyGLWidget(QWidget *parent = 0);
    void setVoxels(int16_t*** voxels, short int numCubes);
    void repaint_function();
    ~MyGLWidget();
signals:
protected:
    struct Voxel;
    void initializeGL();
    void paintGL();
    void resizeGL(int width, int height);
    void paintSphere(float radius, int numStacks, int numSlices);
    std::vector<std::array<GLubyte, 4>> generateDistinctColors();
    void drawCube(float cubeSize, GLenum type = GL_QUADS);
    void drawCube(short cubeSize, Voxel v);
    QSize minimumSizeHint() const;
    QSize sizeHint() const;
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void wheelEvent(QWheelEvent *event);

    void calculateScene();
public slots:
    // slots for xyz-rotation slider
    void setXRotation(int angle);
    void setYRotation(int angle);
    void setZRotation(int angle);
    void setNumCubes(int numCubes);
    void setNumColors(int numColors);
    void setDistanceFactor(int factor);
    void setDelayAnimation(int delayAnimation);
    void update_function();
    //void explodeCubes(int value);
    //void setColorDistanceFactor(float factor);
    void updateGLWidget(int16_t*** voxels, short int numCubes);
signals:
    // signaling rotation from mouse movement
    void xRotationChanged(int angle);
    void yRotationChanged(int angle);
    void zRotationChanged(int angle);
private:
    QTimer* timer;
protected:
    int xRot;
    int yRot;
    int zRot;
    int delayAnimation;

    float distance;

    QPoint lastPos;
    QMatrix4x4 m_projection;
    float distanceFactor = 0;
    //float colordistanceFactor;

    int16_t*** voxels;
    short int numCubes;
    int numColors;

    std::vector<std::array<GLubyte, 4>> colors;
    std::vector<float> directionFactors;

    struct Voxel
    {
        GLshort x, y, z;
        GLubyte r, g, b, a; // Color attributes
        GLbyte nx, ny, nz; // Normal attributes
    };
    std::vector<Voxel> voxelScene;

    bool plotWireFrame = false;

};

#endif // MYGLWIDGET_H
