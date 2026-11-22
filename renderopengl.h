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
#include <QMutex>
#include <QPointer>
#include <atomic>
#include <functional>
#include <optional>
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

    // ── Off-screen capture ───────────────────────────────────────────────
    // A real render of the scene at an arbitrary size into a multisampled
    // FBO, not a rescale of the on-screen texture. The request is made on the
    // GUI thread and served at the start of the next render() on the render
    // thread (the only place the GL context is current); the image is handed
    // back on the receiver's thread through a queued call.
    enum class CaptureBackground { Theme, White, Transparent };
    struct CaptureRequest
    {
        QSize             size;                      // physical pixels
        CaptureBackground background = CaptureBackground::Theme;
        QPointer<QObject> receiver;                  // done() runs on its thread
        std::function<void(QImage)> done;            // null image on failure
    };
    void requestCapture(CaptureRequest request);     // GUI thread

    /// Largest square capture the current context can render (renderbuffer,
    /// texture and viewport limits), known after the first frame; 0 before.
    static int maxCaptureDimension() { return s_maxCaptureDim.load(); }

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

    /// 0 = classic (six object-fixed lights), 1 = depth shading (camera-fixed
    /// key and fill light, hemispherical ambient, AO, depth cueing).
    void setShading(int model, float aoStrength, float depthCue);

    /// Outlines drawn by the face shader while the outline toggle
    /// (setPlotWireFrame) is on: kind 1 = every voxel edge, 2 = grain and pore
    /// boundaries only; width in logical pixels (times the DPR on screen,
    /// times DPI / 96 in an export).
    void setOutlines(int kind, float widthPx);

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

    /// Serve a pending CaptureRequest, if any. Called from render().
    void serveCaptureRequest();
    QImage renderOffscreen(QSize size, CaptureBackground background);

    bool                          m_capturing = false;   // inside renderOffscreen()'s paintGL()
    float                         m_captureScale = 1.0f; // export pixels per logical view pixel
    /// Device pixels per logical pixel of the current frame: the DPR on
    /// screen, the export scale during a capture.
    float lineScale() const;
    /// Segments (pairs of points in `verts`, colours per point in `colors`)
    /// as screen-space quads `widthPx` logical pixels wide; one colour for all
    /// when `singleColor` is given.
    void drawThickLines(const std::vector<float>& verts, const std::vector<float>& colors,
                        const QMatrix4x4& mvp, float widthPx, const float* singleColor);
    QMutex                        m_captureMutex;
    std::optional<CaptureRequest> m_captureRequest;
    static std::atomic<int>       s_maxCaptureDim;

    /// Clear colour of the next paintGL(); the theme colour on screen, white or
    /// fully transparent during a capture.
    QColor m_clearColor;

    float m_dpr = 1.0f;  // device pixel ratio
protected:

    GLuint vboIds[3];
    GLuint vaoId = 0;
    QOpenGLShaderProgram* shaderProgram = nullptr;
    QOpenGLShaderProgram* axisShaderProgram = nullptr;
    QOpenGLShaderProgram* lineShaderProgram = nullptr;

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

    bool  plotWireFrame = false;     // outline toggle (the toolbar's former wireframe button)
    int   outlineKind  = 2;          // 1 voxel edges, 2 grain / pore boundaries
    float outlineWidth = 1.5f;       // logical pixels
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
    int   shadingModel    = 1;
    float aoStrength      = 0.85f;
    float depthCue        = 0.35f;

    GLuint orientationVAO  = 0;
    GLuint orientationVBOs[2] = {0, 0};  // [0]=positions, [1]=colors

    std::vector<float> orientationVerts;
    std::vector<float> orientationColors;
    bool   showOrientations          = false;
    bool   orientationVBOdirty       = false;
    bool   m_showCornerAxes          = true;
};
