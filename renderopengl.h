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
#include <cstdint>
#include <QOpenGLContext>
#include "cornergizmo.hpp"
#include "glvertex.hpp"
#include "svgexporter.hpp"

class RenderOpenGL : public QQuickFramebufferObject::Renderer, protected QOpenGLFunctions, public QObject
{
public:
    /// The interleaved vertex layout now lives in glvertex.hpp so that geometry
    /// builders (tensor glyphs, streamline tubes, elastic surfaces) can name it
    /// without including this renderer. Kept as a member alias so every
    /// existing RenderOpenGL::Voxel use site and the VBO layout are unchanged.
    using Voxel = GlVertex;

    RenderOpenGL();
    virtual ~RenderOpenGL();
    void render() override;
    QSize size() const { return QSize(300, 300); }


private:
    static void debugCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                               GLsizei length, const GLchar* message, const void* userParam);
    bool    m_svgExportRequested = false;
    QString m_svgExportPath;

public:
    void setPlotWireFrame(bool status);
    void setRotations(int xRot, int yRot, int zRot);
    void setNumCubes(int numCubes);
    void setDistZoomFactor(float distance, float zoomFactor);
    void setPan(float panX, float panY);
    void resizeGL(int width, int height);
    void toggleDebugMode();
    void toggleFaceCulling();
    void toggleDepthTest();
    void createTestScene();

    void updateVoxelData(std::vector<Voxel>& voxelScene);
    QImage captureScreenshot(bool includeGizmo = false);

    QOpenGLFramebufferObject* createFramebufferObject(const QSize &size) override;
    void updateVBO();
    void drawCornerAxes();
    void setShowCornerAxes(bool show) { m_showCornerAxes = show; }
    bool showCornerAxes() const { return m_showCornerAxes; }

    void setDevicePixelRatio(float dpr);

    void updateOrientationData(const std::vector<float>& verts,
                               const std::vector<float>& colors);
    void setShowOrientations(bool show);

    // ── Tensor overlays ──────────────────────────────────────────────────
    // Geometry is built off-thread into GlVertex + uint32 index arrays and
    // moved in here; the actual GL upload is deferred to the next render()
    // because that is the only place a current context is guaranteed.
    void updateGlyphMesh(std::vector<Voxel> verts, std::vector<uint32_t> indices);
    void setShowGlyphs(bool show);

    void updateStreamlineMesh(std::vector<Voxel> verts, std::vector<uint32_t> indices);
    void setShowStreamlines(bool show);

    /// Alpha multiplier applied to the voxel block only, so overlay geometry
    /// inside it stays visible. 1.0 == opaque, the previous behaviour.
    void setVoxelOpacity(float opacity);

    void requestSvgExport(const QString& path);  // GUI thread -> served next frame

protected:

    void initializeGL();
    void paintGL();
    void drawAxis();
    void drawAxisWithMVP(const QMatrix4x4& mvp);
    void initLights();
    void updateProjection();

    QSize minimumSizeHint() const;
    QSize sizeHint() const;

    void initializeVBO();

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
    float panX = 0.0f;
    float panY = 0.0f;

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

    void drawOrientationGlyphs();
    void initOrientationVBO();

    // ── Indexed overlay meshes ───────────────────────────────────────────
    // One VAO/VBO/EBO triple per overlay, allocated once and reuploaded only
    // when `dirty`. Deliberately NOT modelled on drawOrientationGlyphs(),
    // which generates and deletes its buffers every single frame.
    //
    // Indices are GL_UNSIGNED_INT, not GL_UNSIGNED_SHORT: a few thousand
    // glyphs is already several hundred thousand vertices, well past 65535.
    struct IndexedMesh
    {
        GLuint  vao = 0, vbo = 0, ebo = 0;
        std::vector<Voxel>    verts;
        std::vector<uint32_t> indices;
        GLsizei indexCount = 0;
        bool    dirty      = false;
    };

    void initIndexedMesh(IndexedMesh& m);
    void uploadIndexedMesh(IndexedMesh& m);
    void drawIndexedMesh(IndexedMesh& m);

    /// The three glVertexAttribPointer calls describing the GlVertex layout.
    /// Factored out of initializeVBO() so every VAO in this class describes the
    /// vertex format exactly once, in one place.
    void setVoxelAttribPointers();

    IndexedMesh glyphMesh;
    IndexedMesh streamMesh;
    bool  showGlyphs      = false;
    bool  showStreamlines = false;
    float voxelOpacity    = 1.0f;

    GLuint orientationVAO  = 0;
    GLuint orientationVBOs[2] = {0, 0};  // [0]=positions, [1]=colors

    std::vector<float> orientationVerts;
    std::vector<float> orientationColors;
    bool   showOrientations          = false;
    bool   orientationVBOdirty       = false;
    bool   m_showCornerAxes          = true;
};
