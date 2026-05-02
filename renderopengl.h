#pragma once

#include <QQuickFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QMatrix4x4>
#include <QVector3D>
#include <QColor>
#include <QImage>
#include <QSize>
#include <vector>
#include <QOpenGLContext>

class RenderOpenGL : public QQuickFramebufferObject::Renderer, protected QOpenGLFunctions, public QObject
{
public:
    struct Voxel
    {
        GLfloat x, y, z;
        GLubyte r, g, b, a;
        GLbyte nx, ny, nz;
    };

    RenderOpenGL();
    virtual ~RenderOpenGL();
    void render() override;
    QSize size() const { return QSize(300, 300); }


private:
    static void debugCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                               GLsizei length, const GLchar* message, const void* userParam);

public:
    void setPlotWireFrame(bool status);
    void setRotations(int xRot, int yRot, int zRot);
    void setNumCubes(int numCubes);
    void setDistZoomFactor(float distance, float zoomFactor);
    void resizeGL(int width, int height);
    void toggleDebugMode();
    void toggleFaceCulling();
    void toggleDepthTest();
    void createTestScene();

    void updateVoxelData(std::vector<Voxel>& voxelScene);
    QImage captureScreenshot();

    QOpenGLFramebufferObject* createFramebufferObject(const QSize &size) override;
    void updateVBO();
    void drawCornerAxes();

    void setDevicePixelRatio(float dpr);
protected:

    void initializeGL();
    void paintGL();
    void drawAxis();
    void drawAxisWithMVP(const QMatrix4x4& mvp);
    void initLights();


    QSize minimumSizeHint() const;
    QSize sizeHint() const;

    void initializeVBO();

    int m_cornerSize = 80;   // px
    int m_cornerMargin = 10; // px

    float m_dpr = 1.0f;  // device pixel ratio
protected:

    GLuint vboIds[3];
    GLuint vaoId = 0;
    QOpenGLShaderProgram* shaderProgram = nullptr;
    QOpenGLShaderProgram* axisShaderProgram = nullptr;

    int xRot;
    int yRot;
    int zRot;

    int numCubes;

    float distance;
    float zoomFactor = 1.0f;

    QColor bgColor;

    int width;
    int height;

    QMatrix4x4 m_projection;
    float distanceFactor = 0;

    bool isVBOupdateRequired = false;


    std::vector<Voxel> voxelScene;

    bool plotWireFrame = false;
    bool showNormals = false;
    bool enableFaceCulling = true;
    bool enableDepthTest = true;
    int debugMode = 0;
};
