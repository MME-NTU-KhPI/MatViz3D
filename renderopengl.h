#pragma once

#include <QQuickFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include "ansyswrapper.h"

/**
 * @class RenderOpenGL
 * A custom OpenGL widget for rendering 3D voxel scenes with interaction capabilities.
 */

class RenderOpenGL : public QQuickFramebufferObject::Renderer, protected QOpenGLFunctions, public QObject
{

public:

    RenderOpenGL();
    virtual ~RenderOpenGL();
    void render() override;
    QSize size() const { return QSize(300, 300); }


private:
    /**
     * OpenGL debug callback for logging messages.
     */
    static void debugCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                              GLsizei length, const GLchar* message, const void* userParam);

public:

    void setPlotWireFrame(bool status);
    void setRotations(int xRot, int yRot, int zRot);
    void setNumCubes(int numCubes);
    void setDistZoomFactor(float distance, float zoomFactor);
    void resizeGL(int width, int height);

protected:
    struct Voxel;
    void initializeGL();
    void paintGL();
    void drawAxis();
    void initLights();


    QSize minimumSizeHint() const;
    QSize sizeHint() const;

    void initializeVBO();
    void updateVBO();

protected:

    GLuint vboIds[3];

    int xRot;
    int yRot;
    int zRot;

    int numCubes;

    float distance;
    float zoomFactor = 1.0f;

    QColor bgColor;


    QMatrix4x4 m_projection;
    float distanceFactor = 0;

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
