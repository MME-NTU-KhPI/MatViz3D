#include "renderopengl.h"
#include "glshaders.hpp"
#include "qopenglframebufferobject.h"

#include <QDebug>
#include <QTimer>
#include <QImage>
#include <QThread>

#include <QOpenGLExtraFunctions>
#include <QVector2D>

#include <algorithm>
#include <memory>
#include <unordered_map>

#ifndef GL_MAX_SAMPLES
#define GL_MAX_SAMPLES 0x8D57
#endif
#ifndef GL_RGBA8
#define GL_RGBA8 0x8058
#endif

#ifdef __APPLE__
// macOS ships OpenGL 4.1 without GL_KHR_debug; define the enums so the
// debug callback compiles. The callback itself is never installed on macOS.
#ifndef GL_DEBUG_OUTPUT
#define GL_DEBUG_OUTPUT                   0x92E0
#define GL_DEBUG_OUTPUT_SYNCHRONOUS       0x8242
#define GL_DEBUG_SOURCE_API               0x8246
#define GL_DEBUG_SOURCE_WINDOW_SYSTEM     0x8247
#define GL_DEBUG_SOURCE_SHADER_COMPILER   0x8248
#define GL_DEBUG_SOURCE_THIRD_PARTY       0x8249
#define GL_DEBUG_SOURCE_APPLICATION       0x824A
#define GL_DEBUG_SOURCE_OTHER             0x824B
#define GL_DEBUG_TYPE_ERROR               0x824C
#define GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR 0x824D
#define GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR  0x824E
#define GL_DEBUG_TYPE_PORTABILITY         0x824F
#define GL_DEBUG_TYPE_PERFORMANCE         0x8250
#define GL_DEBUG_TYPE_OTHER               0x8251
#define GL_DEBUG_TYPE_MARKER              0x8268
#define GL_DEBUG_TYPE_PUSH_GROUP          0x8269
#define GL_DEBUG_TYPE_POP_GROUP           0x826A
#define GL_DEBUG_SEVERITY_NOTIFICATION    0x826B
#define GL_DEBUG_SEVERITY_HIGH            0x9146
#define GL_DEBUG_SEVERITY_MEDIUM          0x9147
#define GL_DEBUG_SEVERITY_LOW             0x9148
#endif
#endif


RenderOpenGL::RenderOpenGL()
{
    qDebug() << "RenderOpenGL::RenderOpenGL() - START";
    xRot = 0;
    yRot = 0;
    zRot = 0;
    panX = 0.0f;
    panY = 0.0f;
    distance = 2.0f;
    numCubes = 1;
    width = 300;
    height = 300;
    bgColor.setRgbF(0.21f, 0.21f, 0.21f);
    m_clearColor = bgColor;
    shaderProgram = nullptr;
    axisShaderProgram = nullptr;
    vaoId = 0;




    m_projection.setToIdentity();
    m_projection.perspective(45.0f, 1.0f, 0.1f, 100.0f);

    qDebug() << "RenderOpenGL::RenderOpenGL() - calling initializeOpenGLFunctions()";
    initializeOpenGLFunctions();

    qDebug() << "RenderOpenGL::RenderOpenGL() - getting current context";
    QOpenGLContext *glContext = QOpenGLContext::currentContext();
    if (!glContext) {
        qDebug() << "RenderOpenGL: OpenGL context is NULL!";
    } else {
        qDebug() << "RenderOpenGL: OpenGL initialized. Context valid:" << glContext->isValid();
    }
    qDebug() << "RenderOpenGL::RenderOpenGL() - calling initializeGL()";
    this->initializeGL();

    if (debugMode)
        this->createTestScene();

    qDebug() << "RenderOpenGL::RenderOpenGL() - END";
}

void RenderOpenGL::render()
{
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    if (!ctx) {
        qDebug() << "RenderOpenGL::render() - No current context!";
        return;
    }
    serveCaptureRequest();
    this->paintGL();
    update();
}

std::atomic<int> RenderOpenGL::s_maxCaptureDim{0};

void RenderOpenGL::requestCapture(CaptureRequest request)
{
    {
        QMutexLocker lock(&m_captureMutex);
        if (m_captureRequest && m_captureRequest->receiver && m_captureRequest->done) {
            // A newer request replaces one that was never served; tell the
            // older caller so it does not wait forever.
            auto done = m_captureRequest->done;
            QMetaObject::invokeMethod(m_captureRequest->receiver.data(),
                                      [done]() { done(QImage()); }, Qt::QueuedConnection);
        }
        m_captureRequest = std::move(request);
    }
    update();
}

void RenderOpenGL::serveCaptureRequest()
{
    std::optional<CaptureRequest> req;
    {
        QMutexLocker lock(&m_captureMutex);
        req.swap(m_captureRequest);
    }
    if (!req) return;

    const QImage img = renderOffscreen(req->size, req->background);
    if (req->receiver && req->done) {
        auto done = req->done;
        QMetaObject::invokeMethod(req->receiver.data(), [done, img]() { done(img); },
                                  Qt::QueuedConnection);
    }
}

QImage RenderOpenGL::renderOffscreen(QSize size, CaptureBackground background)
{
    QOpenGLContext* ctx = QOpenGLContext::currentContext();
    if (!ctx || size.isEmpty()) return QImage();
    QOpenGLFunctions* f = ctx->functions();

    // -- Size and sample count the driver can actually give --------------
    GLint maxRb = 0, maxTex = 0, maxSamples = 0;
    GLint maxVp[2] = {0, 0};
    f->glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &maxRb);
    f->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTex);
    f->glGetIntegerv(GL_MAX_VIEWPORT_DIMS, maxVp);
    f->glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    int maxDim = 16384;
    for (GLint v : {maxRb, maxTex, maxVp[0], maxVp[1]})
        if (v > 0) maxDim = std::min<int>(maxDim, v);
    s_maxCaptureDim.store(maxDim);
    if (size.width() > maxDim || size.height() > maxDim)
        size.scale(maxDim, maxDim, Qt::KeepAspectRatio);

    // Colour (RGBA8) + depth/stencil (D24S8) = 8 bytes per sample. Keep the
    // multisampled buffer under ~1.5 GB: at 600 DPI the pixels are so small
    // that 2x is plenty, and 8x of a 7500 x 5000 frame would be 2.4 GB.
    int samples = std::min(8, std::max(0, int(maxSamples)));
    const qint64 budget = qint64(1536) << 20;
    while (samples > 1 && qint64(size.width()) * size.height() * samples * 8 > budget)
        samples /= 2;
    if (samples < 2) samples = 0;

    std::unique_ptr<QOpenGLFramebufferObject> ms;
    for (;;) {
        QOpenGLFramebufferObjectFormat fmt;
        fmt.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        fmt.setInternalTextureFormat(GL_RGBA8);
        fmt.setSamples(samples);
        ms = std::make_unique<QOpenGLFramebufferObject>(size, fmt);
        if (ms->isValid() || samples == 0) break;
        samples = (samples > 2) ? samples / 2 : 0;
    }
    QOpenGLFramebufferObjectFormat resolvedFmt;
    resolvedFmt.setInternalTextureFormat(GL_RGBA8);
    QOpenGLFramebufferObject resolved(size, resolvedFmt);
    if (!ms->isValid() || !resolved.isValid()) {
        qWarning() << "[capture] cannot create a" << size << "framebuffer";
        if (QOpenGLFramebufferObject* screen = framebufferObject()) screen->bind();
        return QImage();
    }

    // -- Render the same camera at the new size ---------------------------
    const int        savedW = width, savedH = height;
    const float      savedDpr = m_dpr;
    const QMatrix4x4 savedProj = m_projection;
    const bool       savedAxes = m_showCornerAxes;
    const QColor     savedClear = m_clearColor;

    // Lines and outlines keep their on-screen proportions: a logical pixel
    // of the view becomes size / view-size pixels of the export.
    m_captureScale = savedW > 0 ? float(size.width()) / float(savedW) : 1.0f;
    width = size.width();
    height = size.height();
    m_dpr = 1.0f;
    updateProjection();               // same 45 degree vertical field of view
    m_showCornerAxes = false;         // the exporter draws its own gizmo
    switch (background) {
    case CaptureBackground::Theme:       m_clearColor = bgColor; break;
    case CaptureBackground::White:       m_clearColor = QColor(255, 255, 255, 255); break;
    case CaptureBackground::Transparent: m_clearColor = QColor(0, 0, 0, 0); break;
    }

    ms->bind();
    f->glViewport(0, 0, width, height);
    m_capturing = true;
    paintGL();
    m_capturing = false;
    QOpenGLFramebufferObject::blitFramebuffer(&resolved, ms.get(),
                                              GL_COLOR_BUFFER_BIT, GL_NEAREST);
    // Not flipped: Qt Quick shows this FBO's texture with GL's bottom row at
    // the top (the corner triad's labels follow that, kGizmoYFlip), and the
    // export has to show what the screen shows -- toImage(true) turned the
    // model upside down.
    // The blending writes premultiplied colour (see paintGL), so the read-back
    // is RGBA8888_Premultiplied; downstream works in ARGB32_Premultiplied.
    QImage img = resolved.toImage(false).convertToFormat(QImage::Format_ARGB32_Premultiplied);

    width = savedW;
    height = savedH;
    m_dpr = savedDpr;
    m_projection = savedProj;
    m_showCornerAxes = savedAxes;
    m_clearColor = savedClear;

    if (QOpenGLFramebufferObject* screen = framebufferObject()) screen->bind();
    f->glViewport(0, 0, static_cast<int>(width * m_dpr), static_cast<int>(height * m_dpr));

    qDebug().noquote() << QStringLiteral("[capture] %1 x %2, %3x MSAA, format %4")
                              .arg(img.width()).arg(img.height()).arg(samples)
                              .arg(int(img.format()));
    return img;
}


QOpenGLFramebufferObject* RenderOpenGL::createFramebufferObject(const QSize &size)
{
    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::Depth);
    //format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
//    format.setSamples(4); // Optional: Enable MSAA for better quality
    return new QOpenGLFramebufferObject(size, format);
}

void RenderOpenGL::updateVoxelData(std::vector<Voxel>& voxelScene)
{
    this->voxelScene = voxelScene;
    isVBOupdateRequired = true;
}

RenderOpenGL::~RenderOpenGL()
{
    qDebug() << "RenderOpenGL::~RenderOpenGL() - START";
    QOpenGLContext *context = QOpenGLContext::currentContext();
    if (context && context->isValid()) {
        qDebug() << "RenderOpenGL::~RenderOpenGL() - context is valid";
        QOpenGLExtraFunctions *ef = context->extraFunctions();
        if (shaderProgram) {
            qDebug() << "RenderOpenGL::~RenderOpenGL() - deleting shaderProgram";
            shaderProgram->release();
            delete shaderProgram;
        }
        if (axisShaderProgram) {
            qDebug() << "RenderOpenGL::~RenderOpenGL() - deleting axisShaderProgram";
            axisShaderProgram->release();
            delete axisShaderProgram;
        }
        delete lineShaderProgram;
        lineShaderProgram = nullptr;
        if (ef && vaoId) {
            qDebug() << "RenderOpenGL::~RenderOpenGL() - deleting VAO:" << vaoId;
            ef->glDeleteVertexArrays(1, &vaoId);
        }
        if (ef) {
            qDebug() << "RenderOpenGL::~RenderOpenGL() - deleting VBOs";
            ef->glDeleteBuffers(3, vboIds);
        }
        if (ef && orientationVAO) {
            ef->glDeleteVertexArrays(1, &orientationVAO);
            ef->glDeleteBuffers(2, orientationVBOs);
        }
        if (ef && glyphMesh.vao) {
            ef->glDeleteVertexArrays(1, &glyphMesh.vao);
            ef->glDeleteBuffers(1, &glyphMesh.vbo);
            ef->glDeleteBuffers(1, &glyphMesh.ebo);
        }
        if (ef && streamMesh.vao) {
            ef->glDeleteVertexArrays(1, &streamMesh.vao);
            ef->glDeleteBuffers(1, &streamMesh.vbo);
            ef->glDeleteBuffers(1, &streamMesh.ebo);
        }
    } else {
        qDebug() << "RenderOpenGL::~RenderOpenGL() - context is not valid";
    }
    qDebug() << "RenderOpenGL::~RenderOpenGL() - END";
}

QSize RenderOpenGL::minimumSizeHint() const
{
    return QSize(50, 50);
}

QSize RenderOpenGL::sizeHint() const
{
    return QSize(400, 400);
}

void inline RenderOpenGL::initLights()
{
}

void RenderOpenGL::setDevicePixelRatio(float dpr)
{
    m_dpr = dpr;
}


void RenderOpenGL::debugCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                               GLsizei length, const GLchar* message, const void* userParam) {
    Q_UNUSED(length);
    Q_UNUSED(userParam);

    // Ignore driver performance hints (e.g. Qt Quick internal uniform buffer updates)
    // and low-priority notification messages
    if (type == GL_DEBUG_TYPE_PERFORMANCE || severity == GL_DEBUG_SEVERITY_NOTIFICATION)
        return;

    static int num_deprecated_warnings = 0;
    if (type == GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR) {
        if (num_deprecated_warnings++ > 0)
            return;
    }

    QString sourceStr;
    switch (source) {
    case GL_DEBUG_SOURCE_API:             sourceStr = "API"; break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   sourceStr = "Window System"; break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER: sourceStr = "Shader Compiler"; break;
    case GL_DEBUG_SOURCE_THIRD_PARTY:     sourceStr = "Third Party"; break;
    case GL_DEBUG_SOURCE_APPLICATION:     sourceStr = "Application"; break;
    case GL_DEBUG_SOURCE_OTHER:           sourceStr = "Other"; break;
    default:                              sourceStr = "Unknown"; break;
    }

    QString typeStr;
    switch (type) {
    case GL_DEBUG_TYPE_ERROR:               typeStr = "Error"; break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: typeStr = "Deprecated Behavior"; break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  typeStr = "Undefined Behavior"; break;
    case GL_DEBUG_TYPE_PORTABILITY:         typeStr = "Portability"; break;
    case GL_DEBUG_TYPE_PERFORMANCE:         typeStr = "Performance"; break;
    case GL_DEBUG_TYPE_MARKER:              typeStr = "Marker"; break;
    case GL_DEBUG_TYPE_PUSH_GROUP:          typeStr = "Push Group"; break;
    case GL_DEBUG_TYPE_POP_GROUP:           typeStr = "Pop Group"; break;
    case GL_DEBUG_TYPE_OTHER:               typeStr = "Other"; break;
    default:                                typeStr = "Unknown"; break;
    }

    QString severityStr;
    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:         severityStr = "High"; break;
    case GL_DEBUG_SEVERITY_MEDIUM:       severityStr = "Medium"; break;
    case GL_DEBUG_SEVERITY_LOW:          severityStr = "Low"; break;
    case GL_DEBUG_SEVERITY_NOTIFICATION: severityStr = "Notification"; break;
    default:                             severityStr = "Unknown"; break;
    }

    qDebug().noquote()
         << "OpenGL Debug Message:"
        << "\n    Source: " << sourceStr
        << "\n    Type: " << typeStr
        << "\n    ID: " << id
        << "\n    Severity: " << severityStr
        << "\n    Message: " << message;
}

void RenderOpenGL::initializeGL()
{
    qDebug() << "RenderOpenGL::initializeGL() - START";

    qDebug() << "RenderOpenGL::initializeGL() - getting functions";
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    QOpenGLExtraFunctions* ef = QOpenGLContext::currentContext()->extraFunctions();

    if (!f || !ef) {
        qDebug() << "RenderOpenGL::initializeGL() - OpenGL functions not available";
        return;
    }
    qDebug() << "RenderOpenGL::initializeGL() - functions available, setting up GL state";

#ifndef Q_OS_MACOS
    f->glEnable(GL_DEBUG_OUTPUT);
    f->glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    ef->glDebugMessageCallback(debugCallback, this);
#else
    Q_UNUSED(ef);
#endif

    f->glClearColor(bgColor.redF(), bgColor.greenF(), bgColor.blueF(), bgColor.alphaF());
    ef->glClearDepthf(1.0f);
    f->glDepthFunc(GL_LEQUAL);
    f->glEnable(GL_DEPTH_TEST);

    //f->glEnable(GL_POLYGON_OFFSET_FILL);
    //f->glPolygonOffset(1.0f, 1.0f); // push filled polygons back slightly

    qDebug() << "RenderOpenGL::initializeGL() - creating main shader program";

    // Shader sources live in glshaders.hpp so the elastic-surface renderer in
    // the material database window shades identically instead of forking a copy.
    const char* vertexShaderSource       = matviz_gl::kLitVertexShader;
    const char* fragmentShaderSource     = matviz_gl::kLitFragmentShader;
    const char* axisVertexShaderSource   = matviz_gl::kAxisVertexShader;
    const char* axisFragmentShaderSource = matviz_gl::kAxisFragmentShader;

    qDebug() << "RenderOpenGL::initializeGL() - creating shader program object";
    shaderProgram = new QOpenGLShaderProgram();
    qDebug() << "RenderOpenGL::initializeGL() - adding vertex shader";
    if (!shaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource)) {
        qDebug() << "Vertex shader compilation failed:" << shaderProgram->log();
        delete shaderProgram;
        shaderProgram = nullptr;
        return;
    }
    qDebug() << "RenderOpenGL::initializeGL() - adding fragment shader";
    if (!shaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource)) {
        qDebug() << "Fragment shader compilation failed:" << shaderProgram->log();
        delete shaderProgram;
        shaderProgram = nullptr;
        return;
    }
    qDebug() << "RenderOpenGL::initializeGL() - linking shader program";
    shaderProgram->bindAttributeLocation("aPosition", 0);
    shaderProgram->bindAttributeLocation("aColor", 1);
    shaderProgram->bindAttributeLocation("aNormal", 2);
    shaderProgram->bindAttributeLocation("aAO", 3);
    if (!shaderProgram->link()) {
        qDebug() << "Shader program linking failed:" << shaderProgram->log();
        delete shaderProgram;
        shaderProgram = nullptr;
        return;
    }
    qDebug() << "RenderOpenGL::initializeGL() - shader program created successfully";

    qDebug() << "RenderOpenGL::initializeGL() - creating axis shader program";
    axisShaderProgram = new QOpenGLShaderProgram();
    if (!axisShaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, axisVertexShaderSource)) {
        qDebug() << "Axis vertex shader compilation failed:" << axisShaderProgram->log();
        delete axisShaderProgram;
        axisShaderProgram = nullptr;
    } else if (!axisShaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, axisFragmentShaderSource)) {
        qDebug() << "Axis fragment shader compilation failed:" << axisShaderProgram->log();
        delete axisShaderProgram;
        axisShaderProgram = nullptr;
    } else {
        axisShaderProgram->bindAttributeLocation("aPosition", 0);
        axisShaderProgram->bindAttributeLocation("aColor", 1);
        if (!axisShaderProgram->link()) {
            qDebug() << "Axis shader program linking failed:" << axisShaderProgram->log();
            delete axisShaderProgram;
            axisShaderProgram = nullptr;
        }
    }
    qDebug() << "RenderOpenGL::initializeGL() - axis shader program created";

    lineShaderProgram = new QOpenGLShaderProgram();
    if (!lineShaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, matviz_gl::kLineVertexShader) ||
        !lineShaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, matviz_gl::kLineFragmentShader)) {
        qDebug() << "Line shader compilation failed:" << lineShaderProgram->log();
        delete lineShaderProgram;
        lineShaderProgram = nullptr;
    } else {
        lineShaderProgram->bindAttributeLocation("aStart", 0);
        lineShaderProgram->bindAttributeLocation("aEnd", 1);
        lineShaderProgram->bindAttributeLocation("aT", 2);
        lineShaderProgram->bindAttributeLocation("aSide", 3);
        lineShaderProgram->bindAttributeLocation("aColor", 4);
        if (!lineShaderProgram->link()) {
            qDebug() << "Line shader linking failed:" << lineShaderProgram->log();
            delete lineShaderProgram;
            lineShaderProgram = nullptr;
        }
    }

    qDebug() << "RenderOpenGL::initializeGL() - generating VAO and VBOs";
    ef->glGenVertexArrays(1, &vaoId);
    ef->glGenBuffers(3, vboIds);
    qDebug() << "RenderOpenGL::initializeGL() - VAO ID:" << vaoId;

    qDebug() << "RenderOpenGL::initializeGL() - calling initializeVBO()";
    initializeVBO();
    qDebug() << "RenderOpenGL::initializeGL() - END";

    qDebug() << "RenderOpenGL::initializeGL() - calling initOrientationVBO()";
    initOrientationVBO();
    qDebug() << "RenderOpenGL::initializeGL() - END";

}

void RenderOpenGL::updateProjection()
{
    if (height <= 0 || width <= 0) return;

    float aspect     = static_cast<float>(width) / static_cast<float>(height);
    float sceneRadius = numCubes * 0.866f;

    // Near plane: never clip into the scene, never go below epsilon
    float nearVal = std::max(0.01f, distance - sceneRadius * 2.0f);

    // Far plane: always covers the full scene from any distance
    float farVal  = distance + sceneRadius * 2.0f + 1.0f;

    // Guard against degenerate frustum
    if (farVal <= nearVal)
        farVal = nearVal + 1.0f;

    // Keep depth precision reasonable — avoid >10000 ratio
    if (farVal / nearVal > 10000.0f)
        nearVal = farVal / 10000.0f;

    float fov = 45.0f;

    m_projection.setToIdentity();
    m_projection.perspective(fov, aspect, nearVal, farVal);

    qDebug() << "updateProjection: distance=" << distance
             << "near=" << nearVal << "far=" << farVal
             << "numCubes=" << numCubes;
}



void RenderOpenGL::drawAxis()
{
    qDebug() << "RenderOpenGL::drawAxis() - START (immediate mode test)";
    float halfNum = static_cast<float>(numCubes) / 2.0f;
    float multNum = 1.15f * static_cast<float>(numCubes);

    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    if (!f) {
        qDebug() << "RenderOpenGL::drawAxis() - No GL functions";
        return;
    }

    qDebug() << "RenderOpenGL::drawAxis() - disabling shader and depth test temporarily";
    if (axisShaderProgram) {
        axisShaderProgram->release();
    }

    f->glDisable(GL_DEPTH_TEST);

    qDebug() << "RenderOpenGL::drawAxis() - drawing X axis (red)";
    glColor3f(1.0f, 0.0f, 0.0f);
    glBegin(GL_LINES);
    glVertex3f(-halfNum, -halfNum, -halfNum);
    glVertex3f(multNum, -halfNum, -halfNum);
    glEnd();

    qDebug() << "RenderOpenGL::drawAxis() - drawing Y axis (green)";
    glColor3f(0.0f, 1.0f, 0.0f);
    glBegin(GL_LINES);
    glVertex3f(-halfNum, -halfNum, -halfNum);
    glVertex3f(-halfNum, multNum, -halfNum);
    glEnd();

    qDebug() << "RenderOpenGL::drawAxis() - drawing Z axis (blue)";
    glColor3f(0.0f, 0.0f, 1.0f);
    glBegin(GL_LINES);
    glVertex3f(-halfNum, -halfNum, -halfNum);
    glVertex3f(-halfNum, -halfNum, multNum);
    glEnd();

    f->glEnable(GL_DEPTH_TEST);
    qDebug() << "RenderOpenGL::drawAxis() - END (immediate mode test)";
}

void RenderOpenGL::requestSvgExport(const QString& path)
{
    m_svgExportPath = path;        // remembered until the next paintGL
    m_svgExportRequested = true;   // same idiom as isVBOupdateRequired
    update();                      // force a frame so the request is served
}

void RenderOpenGL::drawCornerAxes()
{
    if (!axisShaderProgram) return;

    QOpenGLFunctions    *f  = QOpenGLContext::currentContext()->functions();
    QOpenGLExtraFunctions *ef = QOpenGLContext::currentContext()->extraFunctions();
    if (!f || !ef) return;

    // Convert logical → physical pixels
    const int physW      = static_cast<int>(width  * m_dpr);
    const int physH      = static_cast<int>(height * m_dpr);
    const int physSize   = static_cast<int>(matviz_gizmo::kSize  * m_dpr);
    const int physMargin = static_cast<int>(matviz_gizmo::kMargin * m_dpr);

    // ── 1. Switch to a small corner viewport ──────────────────────────────

    f->glViewport(physW - physSize - physMargin,  // x: right edge
                  physH - physSize - physMargin,  // y: bottom edge (GL origin)
                  physSize,
                  physSize);

    // Clear only the depth for this region so axes always appear on top
    f->glClear(GL_DEPTH_BUFFER_BIT);
    f->glEnable(GL_DEPTH_TEST);

    // ── 2. Build rotation-only MVP (no scene translation/scale) ───────────
    QMatrix4x4 view;
    view.setToIdentity();
    view.translate(0.0f, 0.0f, -matviz_gizmo::kCamDist);   // pull camera back slightly
    view.rotate( xRot / 16.0f, 1.0f, 0.0f, 0.0f);
    view.rotate( yRot / 16.0f, 0.0f, 1.0f, 0.0f);
    view.rotate( zRot / 16.0f, 0.0f, 0.0f, 1.0f);

    QMatrix4x4 proj;
    proj.setToIdentity();
    proj.perspective(matviz_gizmo::kFov, 1.0f, 0.1f, 10.0f); // square aspect — matches square viewport

    QMatrix4x4 mvp = proj * view;

    // ── 3. Geometry: unit-length axes + small arrow-tip cross-bars ─────────
    //  X = red   Y = green   Z = blue
    //  Each axis: shaft + two short perpendicular lines at tip (cheap arrowhead)
    const float L  = matviz_gizmo::kAxisLen;   // shaft length
    const float TL = 0.12f;  // arrowhead half-width

    // clang-format off
    std::vector<float> verts = {
        // X shaft
        0,  0,  0,    L,  0,  0,
        // X arrowhead tick
        L,  0,  0,    L-TL,  TL, 0,
        L,  0,  0,    L-TL, -TL, 0,

        // Y shaft
        0,  0,  0,    0,  L,  0,
        // Y arrowhead tick
        0,  L,  0,    TL,  L-TL, 0,
        0,  L,  0,   -TL,  L-TL, 0,

        // Z shaft
        0,  0,  0,    0,  0,  L,
        // Z arrowhead tick
        0,  0,  L,    TL, 0,  L-TL,
        0,  0,  L,   -TL, 0,  L-TL,
    };

    std::vector<float> colors = {
        // X — red (9 segments × 2 verts)
        1,0,0, 1,0,0,
        1,0,0, 1,0,0,
        1,0,0, 1,0,0,
        // Y — green
        0,1,0, 0,1,0,
        0,1,0, 0,1,0,
        0,1,0, 0,1,0,
        // Z — blue
        0,0.5f,1, 0,0.5f,1,
        0,0.5f,1, 0,0.5f,1,
        0,0.5f,1, 0,0.5f,1,
    };
    // clang-format on

    const int vertexCount = static_cast<int>(verts.size()) / 3;

    // ── 4. Upload and draw ─────────────────────────────────────────────────
    axisShaderProgram->bind();
    axisShaderProgram->setUniformValue("uMVP", mvp);

    GLuint vao = 0;
    GLuint vbo[2] = {0, 0};
    ef->glGenVertexArrays(1, &vao);
    ef->glBindVertexArray(vao);
    ef->glGenBuffers(2, vbo);

    ef->glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    ef->glBufferData(GL_ARRAY_BUFFER,
                     verts.size() * sizeof(float),
                     verts.data(), GL_STATIC_DRAW);
    ef->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    ef->glEnableVertexAttribArray(0);

    ef->glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    ef->glBufferData(GL_ARRAY_BUFFER,
                     colors.size() * sizeof(float),
                     colors.data(), GL_STATIC_DRAW);
    ef->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    ef->glEnableVertexAttribArray(1);

    // Thick-line quads: a 1 px GL line stays 1 device pixel in a 300 DPI export.
    Q_UNUSED(vertexCount);
    drawThickLines(verts, colors, mvp, 1.25f, nullptr);
    ef->glBindVertexArray(vao);

    // ── 5. Cleanup ─────────────────────────────────────────────────────────
    ef->glBindBuffer(GL_ARRAY_BUFFER, 0);
    ef->glBindVertexArray(0);
    ef->glDeleteBuffers(2, vbo);
    ef->glDeleteVertexArrays(1, &vao);

    axisShaderProgram->release();
}

void RenderOpenGL::drawAxisWithMVP(const QMatrix4x4& mvp)
{
    if (!axisShaderProgram) return;

    QOpenGLFunctions      *f  = QOpenGLContext::currentContext()->functions();
    QOpenGLExtraFunctions *ef = QOpenGLContext::currentContext()->extraFunctions();
    if (!f || !ef) return;

    float halfNum = static_cast<float>(numCubes) / 2.0f;
    float tipX    = 1.15f * static_cast<float>(numCubes);  // arrow tip position

    // ── Cone parameters ──────────────────────────────────────────────────
    const int   CONE_SEGS   = 12;       // segments around cone base circle
    const float CONE_HEIGHT = numCubes * 0.08f;
    const float CONE_RADIUS = numCubes * 0.03f;
    const float SHAFT_END   = tipX - CONE_HEIGHT;  // shaft stops here, cone starts

    // ── Helper: append one cone (tip along +axis) ────────────────────────
    // tip    = apex point
    // base   = center of base circle
    // right/up = two orthogonal vectors spanning the base plane
    auto appendCone = [&](std::vector<float>& verts,
                          std::vector<float>& cols,
                          QVector3D tip,
                          QVector3D base,
                          QVector3D right,
                          QVector3D up,
                          QVector3D color)
    {
        for (int i = 0; i < CONE_SEGS; ++i)
        {
            float a0 = 2.0f * M_PI * i       / CONE_SEGS;
            float a1 = 2.0f * M_PI * (i + 1) / CONE_SEGS;

            QVector3D p0 = base + CONE_RADIUS * (cosf(a0) * right + sinf(a0) * up);
            QVector3D p1 = base + CONE_RADIUS * (cosf(a1) * right + sinf(a1) * up);

            // Side triangle: tip → p0 → p1
            auto push3 = [&](QVector3D v) {
                verts.push_back(v.x());
                verts.push_back(v.y());
                verts.push_back(v.z());
                cols.push_back(color.x());
                cols.push_back(color.y());
                cols.push_back(color.z());
            };

            push3(tip); push3(p0); push3(p1);

            // Base disk triangle: base center → p1 → p0  (flipped winding)
            push3(base); push3(p1); push3(p0);
        }
    };

    // ── Line vertices: shafts (stop before cone base) ────────────────────
    std::vector<float> lineVerts = {
        // X shaft  -halfNum → SHAFT_END
        -halfNum,  -halfNum, -halfNum,
        SHAFT_END, -halfNum, -halfNum,
        // Y shaft
        -halfNum, -halfNum,  -halfNum,
        -halfNum,  SHAFT_END, -halfNum,
        // Z shaft
        -halfNum, -halfNum, -halfNum,
        -halfNum, -halfNum,  SHAFT_END,
    };

    std::vector<float> lineColors = {
        1,0,0,  1,0,0,   // X red
        0,1,0,  0,1,0,   // Y green
        0,0.5f,1, 0,0.5f,1, // Z blue
    };

    // ── Cone vertices ─────────────────────────────────────────────────────
    std::vector<float> coneVerts;
    std::vector<float> coneColors;
    coneVerts.reserve(CONE_SEGS * 6 * 3 * 3);
    coneColors.reserve(CONE_SEGS * 6 * 3 * 3);

    // X cone  (tip along +X, base circle in YZ plane)
    appendCone(coneVerts, coneColors,
               { tipX,    -halfNum, -halfNum },   // tip
               { SHAFT_END, -halfNum, -halfNum },  // base center
               { 0, 1, 0 }, { 0, 0, 1 },           // right=Y, up=Z
               { 1, 0, 0 });

    // Y cone  (tip along +Y, base circle in XZ plane)
    appendCone(coneVerts, coneColors,
               { -halfNum, tipX,    -halfNum },
               { -halfNum, SHAFT_END, -halfNum },
               { 1, 0, 0 }, { 0, 0, 1 },
               { 0, 1, 0 });

    // Z cone  (tip along +Z, base circle in XY plane)
    appendCone(coneVerts, coneColors,
               { -halfNum, -halfNum, tipX    },
               { -halfNum, -halfNum, SHAFT_END },
               { 1, 0, 0 }, { 0, 1, 0 },
               { 0, 0.5f, 1 });

    // ── Upload & draw ─────────────────────────────────────────────────────
    axisShaderProgram->bind();
    axisShaderProgram->setUniformValue("uMVP", mvp);

    GLuint vao = 0;
    GLuint vbo[4] = {0, 0, 0, 0};
    ef->glGenVertexArrays(1, &vao);
    ef->glBindVertexArray(vao);
    ef->glGenBuffers(4, vbo);

    // — Lines —
    ef->glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    ef->glBufferData(GL_ARRAY_BUFFER,
                     lineVerts.size() * sizeof(float),
                     lineVerts.data(), GL_STATIC_DRAW);
    ef->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    ef->glEnableVertexAttribArray(0);

    ef->glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    ef->glBufferData(GL_ARRAY_BUFFER,
                     lineColors.size() * sizeof(float),
                     lineColors.data(), GL_STATIC_DRAW);
    ef->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    ef->glEnableVertexAttribArray(1);

    drawThickLines(lineVerts, lineColors, mvp, 1.5f, nullptr);
    ef->glBindVertexArray(vao);
    axisShaderProgram->bind();          // drawThickLines bound its own program
    axisShaderProgram->setUniformValue("uMVP", mvp);

    // — Cones —
    ef->glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
    ef->glBufferData(GL_ARRAY_BUFFER,
                     coneVerts.size() * sizeof(float),
                     coneVerts.data(), GL_STATIC_DRAW);
    ef->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    ef->glEnableVertexAttribArray(0);

    ef->glBindBuffer(GL_ARRAY_BUFFER, vbo[3]);
    ef->glBufferData(GL_ARRAY_BUFFER,
                     coneColors.size() * sizeof(float),
                     coneColors.data(), GL_STATIC_DRAW);
    ef->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    ef->glEnableVertexAttribArray(1);

    ef->glDrawArrays(GL_TRIANGLES, 0,
                     static_cast<GLsizei>(coneVerts.size() / 3));

    // — Cleanup —
    ef->glBindBuffer(GL_ARRAY_BUFFER, 0);
    ef->glBindVertexArray(0);
    ef->glDeleteBuffers(4, vbo);
    ef->glDeleteVertexArrays(1, &vao);

    axisShaderProgram->release();
}


void RenderOpenGL::initializeVBO()
{
    QOpenGLExtraFunctions *ef = QOpenGLContext::currentContext()->extraFunctions();

    if (!ef || vaoId == 0) {
        return;
    }

    ef->glBindVertexArray(vaoId);

    if (voxelScene.empty()) {
        ef->glBindBuffer(GL_ARRAY_BUFFER, vboIds[0]);
        ef->glBufferData(GL_ARRAY_BUFFER, sizeof(Voxel), nullptr, GL_STATIC_DRAW);
    } else {
        ef->glBindBuffer(GL_ARRAY_BUFFER, vboIds[0]);
        ef->glBufferData(GL_ARRAY_BUFFER, voxelScene.size() * sizeof(Voxel), voxelScene.data(), GL_STATIC_DRAW);
    }

    setVoxelAttribPointers();

    ef->glBindBuffer(GL_ARRAY_BUFFER, 0);
    ef->glBindVertexArray(0);
}

void RenderOpenGL::setVoxelAttribPointers()
{
    QOpenGLExtraFunctions *ef = QOpenGLContext::currentContext()->extraFunctions();
    if (!ef) return;

    // Must be called with the target VAO and its array buffer already bound --
    // glVertexAttribPointer records whatever GL_ARRAY_BUFFER is current.
    ef->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Voxel), (void*)offsetof(Voxel, x));
    ef->glEnableVertexAttribArray(0);

    ef->glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Voxel), (void*)offsetof(Voxel, r));
    ef->glEnableVertexAttribArray(1);

    ef->glVertexAttribPointer(2, 3, GL_BYTE, GL_TRUE, sizeof(Voxel), (void*)offsetof(Voxel, nx));
    ef->glEnableVertexAttribArray(2);

    ef->glVertexAttribPointer(3, 1, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Voxel), (void*)offsetof(Voxel, ao));
    ef->glEnableVertexAttribArray(3);
}

// ---------------------------------------------------------------------------
//  Indexed overlay meshes
// ---------------------------------------------------------------------------
void RenderOpenGL::initIndexedMesh(IndexedMesh& m)
{
    QOpenGLExtraFunctions *ef = QOpenGLContext::currentContext()->extraFunctions();
    if (!ef || m.vao != 0) return;

    ef->glGenVertexArrays(1, &m.vao);
    ef->glGenBuffers(1, &m.vbo);
    ef->glGenBuffers(1, &m.ebo);
}

void RenderOpenGL::uploadIndexedMesh(IndexedMesh& m)
{
    QOpenGLExtraFunctions *ef = QOpenGLContext::currentContext()->extraFunctions();
    if (!ef) return;
    if (m.vao == 0) initIndexedMesh(m);
    if (m.vao == 0) return;

    ef->glBindVertexArray(m.vao);

    ef->glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    ef->glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(m.verts.size() * sizeof(Voxel)),
                     m.verts.empty() ? nullptr : m.verts.data(),
                     GL_STATIC_DRAW);
    setVoxelAttribPointers();

    // The element buffer binding is VAO state, so this must happen while the
    // VAO is bound -- and the VAO must be unbound BEFORE the element buffer is,
    // or the unbind is recorded into the VAO and the mesh silently draws
    // nothing.
    ef->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    ef->glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(m.indices.size() * sizeof(uint32_t)),
                     m.indices.empty() ? nullptr : m.indices.data(),
                     GL_STATIC_DRAW);

    m.indexCount = static_cast<GLsizei>(m.indices.size());

    ef->glBindVertexArray(0);
    ef->glBindBuffer(GL_ARRAY_BUFFER, 0);
    ef->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    m.dirty = false;
}

void RenderOpenGL::drawIndexedMesh(IndexedMesh& m)
{
    QOpenGLExtraFunctions *ef = QOpenGLContext::currentContext()->extraFunctions();
    if (!ef) return;

    if (m.dirty) uploadIndexedMesh(m);
    if (m.vao == 0 || m.indexCount == 0) return;

    ef->glBindVertexArray(m.vao);
    ef->glDrawElements(GL_TRIANGLES, m.indexCount, GL_UNSIGNED_INT, nullptr);
    ef->glBindVertexArray(0);
}

void RenderOpenGL::updateGlyphMesh(std::vector<Voxel> verts, std::vector<uint32_t> indices)
{
    glyphMesh.verts   = std::move(verts);
    glyphMesh.indices = std::move(indices);
    glyphMesh.dirty   = true;
}

void RenderOpenGL::setShowGlyphs(bool show)
{
    showGlyphs = show;
}

void RenderOpenGL::updateStreamlineMesh(std::vector<Voxel> verts, std::vector<uint32_t> indices)
{
    streamMesh.verts   = std::move(verts);
    streamMesh.indices = std::move(indices);
    streamMesh.dirty   = true;
}

void RenderOpenGL::setShowStreamlines(bool show)
{
    showStreamlines = show;
}

void RenderOpenGL::setVoxelOpacity(float opacity)
{
    voxelOpacity = std::clamp(opacity, 0.0f, 1.0f);
}

void RenderOpenGL::setShading(int model, float ao, float cue)
{
    shadingModel = (model == 1) ? 1 : 0;
    aoStrength   = std::clamp(ao, 0.0f, 1.0f);
    depthCue     = std::clamp(cue, 0.0f, 1.0f);
    update();
}

void RenderOpenGL::updateVBO()
{
    QOpenGLExtraFunctions *ef = QOpenGLContext::currentContext()->extraFunctions();
    if (!ef || vaoId == 0) {
        qDebug() << "updateVBO() - failed: ef=" << (ef != nullptr) << " vaoId=" << vaoId;
        return;
    }

    qDebug() << "updateVBO() - updating" << voxelScene.size() << "vertices, VAO:" << vaoId;
    ef->glBindVertexArray(vaoId);
    ef->glBindBuffer(GL_ARRAY_BUFFER, vboIds[0]);
    ef->glBufferData(GL_ARRAY_BUFFER, voxelScene.size() * sizeof(Voxel), voxelScene.data(), GL_STATIC_DRAW);
    ef->glBindBuffer(GL_ARRAY_BUFFER, 0);
    ef->glBindVertexArray(0);
    qDebug() << "updateVBO() - completed";
}

void RenderOpenGL::paintGL()
{
    if (isVBOupdateRequired && vaoId != 0) {
        this->updateVBO();
        isVBOupdateRequired = false;
    }

    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    QOpenGLExtraFunctions *ef = QOpenGLContext::currentContext()->extraFunctions();
    if (!f || !ef) {
        return;
    }

    f->glClearColor(m_clearColor.redF(), m_clearColor.greenF(),
                    m_clearColor.blueF(), m_clearColor.alphaF());
    f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    f->glDepthMask(GL_TRUE);
#ifndef Q_OS_MACOS
    f->glEnable(GL_NORMALIZE);   // removed in Core Profile
#endif

    QMatrix4x4 view;
    view.setToIdentity();
    view.translate(panX, panY, -distance);
    view.rotate(xRot / 16.0f, 1.0f, 0.0f, 0.0f);
    view.rotate(yRot / 16.0f, 0.0f, 1.0f, 0.0f);
    view.rotate(zRot / 16.0f, 0.0f, 0.0f, 1.0f);

    QMatrix4x4 model;
    model.setToIdentity();

    QMatrix4x4 mvp = m_projection * view * model;

    // Not during an off-screen capture: the SVG must use the window's size.
    if (m_svgExportRequested && !m_capturing) {
        // Reuse the exact on-screen MVP -> SVG matches the viewport 1:1.
        svgx::writeVoxelSVG(m_svgExportPath,
                            svgx::facesFromQuadBuffer(voxelScene),
                            mvp, width, height, Qt::white,
                            /*drawAxes=*/ true, numCubes);   // numCubes is a member
        m_svgExportRequested = false;
    }

    drawAxisWithMVP(mvp);

    if (!shaderProgram || vaoId == 0) {
        return;
    }

    shaderProgram->bind();

    shaderProgram->setUniformValue("uMVP", mvp);
    shaderProgram->setUniformValue("uModel", model);
    shaderProgram->setUniformValue("uView", view);
    shaderProgram->setUniformValue("uProjection", m_projection);

    // Key light — upper front-right (strongest)
    // Fill lights — cover all dark sides with decreasing intensity
    QVector3D lightDirs[6] = {
        QVector3D( 1.0f,  1.0f,  1.0f).normalized(),  // front-right-top  (key)
        QVector3D(-1.0f,  1.0f,  0.5f).normalized(),  // front-left-top   (key fill)
        QVector3D( 0.0f,  1.0f, -1.0f).normalized(),  // back-top          (back fill)
        QVector3D( 0.0f, -1.0f,  0.5f).normalized(),  // bottom-front      (bounce)
        QVector3D(-1.0f, -0.5f, -1.0f).normalized(),  // bottom-back-left  (deep fill)
        QVector3D( 1.0f, -0.5f, -1.0f).normalized(),  // bottom-back-right (deep fill)
    };

    // Weights sum to ~1.0 so accumulated light stays in [0,1] range
    // before clamping. Key lights are stronger, deep fills are subtle.
    const GLfloat lightWeights[6] = {
        0.40f,  // key
        0.25f,  // key fill
        0.15f,  // back fill
        0.10f,  // bounce
        0.05f,  // deep fill
        0.05f,  // deep fill
    };

    for (int i = 0; i < 6; i++) {
        shaderProgram->setUniformValueArray(
            QString("uLightDirections[%1]").arg(i).toUtf8().constData(),
            &lightDirs[i], 1);
    }
    shaderProgram->setUniformValueArray("uLightWeights", lightWeights, 6, 1);

    // CORRECT — extract actual camera world position from view matrix:
    QMatrix4x4 viewInv = view.inverted();
    QVector3D viewPos = viewInv.column(3).toVector3D();
    shaderProgram->setUniformValue("uViewPos", viewPos);

   // QVector3D viewPos(0.0f, 0.0f, -distance);
    shaderProgram->setUniformValue("uDebugMode", debugMode);

    // Depth shading (glshaders.hpp). The depth range spans the scene's
    // bounding sphere as seen from the camera, so the cue does not depend on
    // the zoom: the nearest corner is untouched, the farthest at full cue.
    {
        const float sceneRadius = numCubes * 0.866f;
        shaderProgram->setUniformValue("uShadingModel", shadingModel);
        shaderProgram->setUniformValue("uAOStrength", aoStrength);
        shaderProgram->setUniformValue("uDepthCue", depthCue);
        shaderProgram->setUniformValue("uDepthRange",
                                       QVector2D(std::max(0.0f, distance - sceneRadius),
                                                 distance + sceneRadius));
        shaderProgram->setUniformValue("uMinLight", 0.28f);
    }
    // Outlines are drawn by the lit shader on the faces themselves.
    shaderProgram->setUniformValue("uOutlineMode", plotWireFrame ? outlineKind : 0);
    shaderProgram->setUniformValue("uOutlineWidth", outlineWidth * lineScale());

    // Fade the voxel block back so tensor overlays inside it are visible. With
    // the block translucent its depth writes are also suppressed, so the
    // overlays drawn afterwards are not occluded by the shell they sit inside
    // -- which is the entire point of the control.
    const bool fadeVoxels = (voxelOpacity < 0.999f);
    shaderProgram->setUniformValue("uAlphaScale", voxelOpacity);
    if (fadeVoxels) f->glDepthMask(GL_FALSE);

    ef->glBindVertexArray(vaoId);

    f->glEnable(GL_BLEND);
    // Colour: ordinary "over". Alpha: src + dst*(1 - src_a), so that over a
    // transparent clear (0,0,0,0) the frame holds premultiplied colour and a
    // correct coverage alpha -- an opaque face ends at alpha 1, not at a^2.
    ef->glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                            GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    if (enableFaceCulling) {
        f->glEnable(GL_CULL_FACE);
        glFrontFace(GL_CCW);
        glCullFace(GL_BACK);
    } else {
        f->glDisable(GL_CULL_FACE);
    }

    if (enableDepthTest) {
        f->glEnable(GL_DEPTH_TEST);
    } else {
        f->glDisable(GL_DEPTH_TEST);
    }

    if (!voxelScene.empty()) {
        for (size_t i = 0; i < voxelScene.size(); i += 4) {
            // Draw quad as triangle fan (v0, v1, v2, v3)
            ef->glDrawArrays(GL_TRIANGLE_FAN, i, 4);
        }
    }
    // Overlay meshes carry no face coordinates: no outlines on them.
    shaderProgram->setUniformValue("uOutlineMode", 0);

    // ── Tensor overlays ──────────────────────────────────────────────
    // Drawn inside the shaderProgram bind so they inherit every lighting
    // uniform set above, and at full alpha regardless of the voxel fade.
    if (fadeVoxels) f->glDepthMask(GL_TRUE);
    shaderProgram->setUniformValue("uAlphaScale", 1.0f);

    if (showGlyphs || showStreamlines) {   // drawIndexedMesh() no-ops when empty
        // Culling off: superquadrics at high anisotropy get thin enough that
        // a back face can end up in front of its own front face, and a swept
        // tube has no consistent facing where its cross-section is degenerate.
        const GLboolean hadCull = f->glIsEnabled(GL_CULL_FACE);
        f->glDisable(GL_CULL_FACE);

        if (showGlyphs)      drawIndexedMesh(glyphMesh);
        if (showStreamlines) drawIndexedMesh(streamMesh);

        if (hadCull) f->glEnable(GL_CULL_FACE);
        ef->glBindVertexArray(vaoId);   // restore what the rest of paintGL expects
    }

    drawOrientationGlyphs();

    f->glDisable(GL_BLEND);

    ef->glBindVertexArray(0);
    shaderProgram->release();




    // ── Corner orientation gizmo ─────────────────────────────────────
    if (m_showCornerAxes) {
        drawCornerAxes();
    }

    // Restore full viewport for next frame
    f->glViewport(0, 0,
                  static_cast<int>(width  * m_dpr),
                  static_cast<int>(height * m_dpr));
}

void RenderOpenGL::resizeGL(int width, int height)
{
    this->width  = width;
    this->height = height;

    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    if (ctx)
    {
        ctx->functions()->glViewport(0, 0, width, height);
    }

    updateProjection();  // replaces the old inline projection code
}

void RenderOpenGL::setRotations(int xRot, int yRot, int zRot)
{
    this->xRot = xRot;
    this->yRot = yRot;
    this->zRot = zRot;
    update();
}

void RenderOpenGL::setNumCubes(int numCubes)
{
    this->numCubes = numCubes;
    updateProjection();
    update();
}

void RenderOpenGL::setDistZoomFactor(float distance, float zoomFactor)
{
    this->distance = distance;
    this->zoomFactor = zoomFactor;
    updateProjection();
    update();
}

void RenderOpenGL::setPan(float panX, float panY)
{
    this->panX = panX;
    this->panY = panY;
    update();
}

void RenderOpenGL::setPlotWireFrame(bool status)
{
    plotWireFrame = status;
}

void RenderOpenGL::setOutlines(int kind, float widthPx)
{
    outlineKind  = (kind == 1) ? 1 : 2;
    outlineWidth = std::clamp(widthPx, 0.25f, 8.0f);
}

float RenderOpenGL::lineScale() const
{
    return m_capturing ? m_captureScale : m_dpr;
}

void RenderOpenGL::drawThickLines(const std::vector<float>& verts, const std::vector<float>& colors,
                                  const QMatrix4x4& mvp, float widthPx, const float* singleColor)
{
    if (!lineShaderProgram || verts.size() < 6) return;
    QOpenGLExtraFunctions* ef = QOpenGLContext::currentContext()->extraFunctions();

    // Six vertices per segment: start, end, t, side, colour (11 floats).
    const size_t segments = verts.size() / 6;
    std::vector<float> buf;
    buf.reserve(segments * 6 * 11);
    static const float corners[6][2] = { {0, -1}, {1, -1}, {1, 1}, {0, -1}, {1, 1}, {0, 1} };
    for (size_t sgi = 0; sgi < segments; ++sgi) {
        const float* a = &verts[sgi * 6];
        const float* b = a + 3;
        for (const auto& c : corners) {
            const float* col = (colors.size() >= verts.size())
                                   ? &colors[sgi * 6 + (c[0] > 0.5f ? 3 : 0)] : nullptr;
            buf.insert(buf.end(), { a[0], a[1], a[2], b[0], b[1], b[2], c[0], c[1],
                                    col ? col[0] : 1.0f, col ? col[1] : 1.0f, col ? col[2] : 1.0f });
        }
    }

    GLuint vao = 0, vbo = 0;
    ef->glGenVertexArrays(1, &vao);
    ef->glBindVertexArray(vao);
    ef->glGenBuffers(1, &vbo);
    ef->glBindBuffer(GL_ARRAY_BUFFER, vbo);
    ef->glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(buf.size() * sizeof(float)), buf.data(), GL_STREAM_DRAW);
    const GLsizei stride = 11 * sizeof(float);
    auto attr = [&](GLuint loc, int n, int offset) {
        ef->glVertexAttribPointer(loc, n, GL_FLOAT, GL_FALSE, stride,
                                  reinterpret_cast<const void*>(size_t(offset) * sizeof(float)));
        ef->glEnableVertexAttribArray(loc);
    };
    attr(0, 3, 0); attr(1, 3, 3); attr(2, 1, 6); attr(3, 1, 7); attr(4, 3, 8);

    lineShaderProgram->bind();
    lineShaderProgram->setUniformValue("uMVP", mvp);
    // The viewport in force (the corner triad draws into a small one).
    GLint vp[4] = { 0, 0, 1, 1 };
    QOpenGLContext::currentContext()->functions()->glGetIntegerv(GL_VIEWPORT, vp);
    lineShaderProgram->setUniformValue("uViewport", QVector2D(float(std::max(1, vp[2])), float(std::max(1, vp[3]))));
    lineShaderProgram->setUniformValue("uWidth", widthPx * lineScale());
    lineShaderProgram->setUniformValue("uUseColor", singleColor ? 0 : 1);
    if (singleColor)
        lineShaderProgram->setUniformValue("uColor", QVector3D(singleColor[0], singleColor[1], singleColor[2]));
    ef->glDrawArrays(GL_TRIANGLES, 0, GLsizei(segments * 6));
    lineShaderProgram->release();

    ef->glBindBuffer(GL_ARRAY_BUFFER, 0);
    ef->glBindVertexArray(0);
    ef->glDeleteBuffers(1, &vbo);
    ef->glDeleteVertexArrays(1, &vao);
}

void RenderOpenGL::toggleDebugMode()
{
    debugMode = (debugMode + 1) % 4;
    qDebug() << "RenderOpenGL::toggleDebugMode() - New debug mode:" << debugMode << "(0=lighting, 1=colors, 2=normals, 3=testcube)";
    if (debugMode)
        this->createTestScene();
    else
        voxelScene.clear();
    update();
}

void RenderOpenGL::toggleFaceCulling()
{
    enableFaceCulling = !enableFaceCulling;
    qDebug() << "RenderOpenGL::toggleFaceCulling() - Face culling:" << (enableFaceCulling ? "enabled" : "disabled");
    update();
}

void RenderOpenGL::toggleDepthTest()
{
    enableDepthTest = !enableDepthTest;
    qDebug() << "RenderOpenGL::toggleDepthTest() - Depth test:" << (enableDepthTest ? "enabled" : "disabled");
    update();
}

void RenderOpenGL::createTestScene()
{
    voxelScene.clear();

    float offset = 0.5f;

    for (int x = 0; x < 1; x++) {
        for (int y = 0; y < 1; y++) {
            for (int z = 0; z < 1; z++) {
                float px = x + offset - 1;
                float py = y + offset - 1;
                float pz = z + offset - 1;

                float size = 0.5f;

                // Front face (+Z) — already correct, no change needed
                voxelScene.push_back({px,        py,        pz+size, 255,0,0,255, 0,0,127});
                voxelScene.push_back({px+size,   py,        pz+size, 255,0,0,255, 0,0,127});
                voxelScene.push_back({px+size,   py+size,   pz+size, 255,0,0,255, 0,0,127});
                voxelScene.push_back({px,        py+size,   pz+size, 255,0,0,255, 0,0,127});

                // Back face (-Z) — FIXED: reverse X order to flip winding to CCW from -Z
                voxelScene.push_back({px+size,   py,        pz,      0,255,0,255, 0,0,-128});
                voxelScene.push_back({px,        py,        pz,      0,255,0,255, 0,0,-128});
                voxelScene.push_back({px,        py+size,   pz,      0,255,0,255, 0,0,-128});
                voxelScene.push_back({px+size,   py+size,   pz,      0,255,0,255, 0,0,-128});

                // Right face (+X) — FIXED: start at top-back corner, go CCW from +X
                voxelScene.push_back({px+size,   py+size,   pz,      0,0,255,255, 127,0,0});
                voxelScene.push_back({px+size,   py+size,   pz+size, 0,0,255,255, 127,0,0});
                voxelScene.push_back({px+size,   py,        pz+size, 0,0,255,255, 127,0,0});
                voxelScene.push_back({px+size,   py,        pz,      0,0,255,255, 127,0,0});

                // Left face (-X) — FIXED: reverse from current order
                voxelScene.push_back({px,        py,        pz+size, 255,255,0,255, -128,0,0});
                voxelScene.push_back({px,        py+size,   pz+size, 255,255,0,255, -128,0,0});
                voxelScene.push_back({px,        py+size,   pz,      255,255,0,255, -128,0,0});
                voxelScene.push_back({px,        py,        pz,      255,255,0,255, -128,0,0});

                // Top face (+Y) — FIXED: swap Z order to flip winding to CCW from +Y
                voxelScene.push_back({px,        py+size,   pz+size, 255,0,255,255, 0,127,0});
                voxelScene.push_back({px+size,   py+size,   pz+size, 255,0,255,255, 0,127,0});
                voxelScene.push_back({px+size,   py+size,   pz,      255,0,255,255, 0,127,0});
                voxelScene.push_back({px,        py+size,   pz,      255,0,255,255, 0,127,0});

                // Bottom face (-Y) — FIXED: reverse Z order to flip winding to CCW from -Y
                voxelScene.push_back({px+size,   py,        pz,      0,255,255,255, 0,-128,0});
                voxelScene.push_back({px+size,   py,        pz+size, 0,255,255,255, 0,-128,0});
                voxelScene.push_back({px,        py,        pz+size, 0,255,255,255, 0,-128,0});
                voxelScene.push_back({px,        py,        pz,      0,255,255,255, 0,-128,0});
            }
        }
    }

    qDebug() << "createTestScene() - created" << voxelScene.size() << "vertices (" 
             << (voxelScene.size() / 4) << " quads)";
    isVBOupdateRequired = true;
    update();
}


// ── Orientation VBO setup ─────────────────────────────────────────────

void RenderOpenGL::initOrientationVBO()
{
    QOpenGLExtraFunctions *ef =
        QOpenGLContext::currentContext()->extraFunctions();
    if (!ef) return;

    ef->glGenVertexArrays(1, &orientationVAO);
    ef->glGenBuffers(2, orientationVBOs);

    // Bind VAO and set up attribute pointers with empty buffers
    ef->glBindVertexArray(orientationVAO);

    ef->glBindBuffer(GL_ARRAY_BUFFER, orientationVBOs[0]);
    ef->glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    ef->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    ef->glEnableVertexAttribArray(0);

    ef->glBindBuffer(GL_ARRAY_BUFFER, orientationVBOs[1]);
    ef->glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    ef->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    ef->glEnableVertexAttribArray(1);

    ef->glBindBuffer(GL_ARRAY_BUFFER, 0);
    ef->glBindVertexArray(0);
}

void RenderOpenGL::updateOrientationData(const std::vector<float>& verts,
                                         const std::vector<float>& colors,
                                         const std::vector<float>& normals)
{
    orientationVerts   = verts;
    orientationColors  = colors;
    orientationNormals = normals;
    orientationVBOdirty = true;
}

void RenderOpenGL::setShowOrientations(bool show)
{
    showOrientations = show;
    update();
}

// ── Draw orientation glyphs as GL_LINES ───────────────────────────────

/*
void RenderOpenGL::drawOrientationGlyphs()
{
    if (!showOrientations || orientationVerts.empty() || !axisShaderProgram)
        return;

    QOpenGLFunctions      *f  = QOpenGLContext::currentContext()->functions();
    QOpenGLExtraFunctions *ef = QOpenGLContext::currentContext()->extraFunctions();
    if (!f || !ef) return;

    // Lazily upload to GPU
    if (orientationVBOdirty && orientationVAO != 0) {
        ef->glBindVertexArray(orientationVAO);

        ef->glBindBuffer(GL_ARRAY_BUFFER, orientationVBOs[0]);
        ef->glBufferData(GL_ARRAY_BUFFER,
                         orientationVerts.size() * sizeof(float),
                         orientationVerts.data(), GL_DYNAMIC_DRAW);

        ef->glBindBuffer(GL_ARRAY_BUFFER, orientationVBOs[1]);
        ef->glBufferData(GL_ARRAY_BUFFER,
                         orientationColors.size() * sizeof(float),
                         orientationColors.data(), GL_DYNAMIC_DRAW);

        ef->glBindBuffer(GL_ARRAY_BUFFER, 0);
        ef->glBindVertexArray(0);
        orientationVBOdirty = false;
    }

    // Build the same MVP as paintGL so glyphs align with voxels
    QMatrix4x4 view;
    view.setToIdentity();
    view.translate(panX, panY, -distance);
    view.rotate(xRot / 16.0f, 1.0f, 0.0f, 0.0f);
    view.rotate(yRot / 16.0f, 0.0f, 1.0f, 0.0f);
    view.rotate(zRot / 16.0f, 0.0f, 0.0f, 1.0f);

    QMatrix4x4 model;
    model.setToIdentity();
    QMatrix4x4 mvp = m_projection * view * model;

    axisShaderProgram->bind();
    axisShaderProgram->setUniformValue("uMVP", mvp);

    f->glDisable(GL_DEPTH_TEST);
    f->glLineWidth(1.0f);

    ef->glBindVertexArray(orientationVAO);
    const GLsizei vertexCount =
        static_cast<GLsizei>(orientationVerts.size() / 3);
    ef->glDrawArrays(GL_LINES, 0, vertexCount);
    ef->glBindVertexArray(0);
    f->glEnable(GL_DEPTH_TEST);
    f->glDepthFunc(GL_LEQUAL);
    axisShaderProgram->release();
}

*/

void RenderOpenGL::drawOrientationGlyphs()
{
    if (!showOrientations || orientationVerts.empty() || !axisShaderProgram)
        return;

    QOpenGLFunctions      *f  = QOpenGLContext::currentContext()->functions();
    QOpenGLExtraFunctions *ef = QOpenGLContext::currentContext()->extraFunctions();
    if (!f || !ef) return;

    // ── Build MVP (same as paintGL) ───────────────────────────────────────
    QMatrix4x4 view;
    view.setToIdentity();
    view.translate(panX, panY, -distance);
    view.rotate(xRot / 16.0f, 1.0f, 0.0f, 0.0f);
    view.rotate(yRot / 16.0f, 0.0f, 1.0f, 0.0f);
    view.rotate(zRot / 16.0f, 0.0f, 0.0f, 1.0f);
    const QMatrix4x4 mvp = m_projection * view;

    // ── The triads of grains that face the camera ─────────────────────────
    // Every triad sits on its grain's visible surface patch; it is drawn only
    // while that patch is turned toward the camera (view-space normal +z).
    // Drawn for every grain from its centre, with the depth test off, the
    // triads of hidden and interior grains piled up through the whole cube.
    const size_t triads = orientationVerts.size() / 18;   // 3 axes x 2 points x 3 floats
    std::vector<float> verts, cols;
    verts.reserve(orientationVerts.size());
    cols.reserve(orientationColors.size());
    // Per grain the patch with the largest projected area (area x cos of
    // its normal to the view direction) among those turned toward the camera.
    const bool perGrain = orientationNormals.size() >= 5 * triads;
    std::unordered_map<int, std::pair<float, size_t>> best;   // grain -> (score, triad)
    if (perGrain) {
        for (size_t t = 0; t < triads; ++t) {
            const float* m = &orientationNormals[5 * t];
            const QVector3D n(m[0], m[1], m[2]);
            if (n.lengthSquared() < 1e-6f) continue;
            const float facing = view.mapVector(n.normalized()).z();
            if (facing <= 0.25f) continue;
            const float score = m[3] * facing;
            auto it = best.find(int(m[4]));
            if (it == best.end() || score > it->second.first) best[int(m[4])] = { score, t };
        }
    }
    for (size_t t = 0; t < triads; ++t) {
        if (perGrain) {
            auto it = best.find(int(orientationNormals[5 * t + 4]));
            if (it == best.end() || it->second.second != t) continue;
        }
        verts.insert(verts.end(), orientationVerts.begin() + 18 * t, orientationVerts.begin() + 18 * (t + 1));
        if (orientationColors.size() >= 18 * (t + 1))
            cols.insert(cols.end(), orientationColors.begin() + 18 * t, orientationColors.begin() + 18 * (t + 1));
    }
    if (verts.empty()) return;

    // On top of the surface they are anchored to.
    f->glDisable(GL_DEPTH_TEST);
    f->glEnable(GL_BLEND);
    ef->glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                            GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    // ── The axes as thick lines over a thin dark halo ─────────────────────
    {
        static const float kHalo[3] = { 0.08f, 0.08f, 0.08f };
        drawThickLines(verts, cols, mvp, 3.6f, kHalo);
        drawThickLines(verts, cols, mvp, 2.0f, nullptr);
    }

    // ── Arrowheads: a cone at the positive tip, sized by the axis length ──
    static const float axisRGBf[3][3] = {
        {230/255.0f,  50/255.0f,  50/255.0f},  // X red
        { 50/255.0f, 200/255.0f,  50/255.0f},  // Y green
        { 60/255.0f, 130/255.0f, 255/255.0f},  // Z blue
    };
    const int SEGS = 10;
    std::vector<float> coneVerts, coneColors;
    coneVerts.reserve(verts.size() * 20);
    coneColors.reserve(verts.size() * 20);
    const size_t shown = verts.size() / 18;
    for (size_t t = 0; t < shown; ++t) {
        for (int axis = 0; axis < 3; ++axis) {
            const size_t baseIdx = (t * 6 + axis * 2) * 3;       // grain anchor
            const size_t tipIdx  = baseIdx + 3;                   // positive tip
            const QVector3D tip(verts[tipIdx], verts[tipIdx + 1], verts[tipIdx + 2]);
            const QVector3D root(verts[baseIdx], verts[baseIdx + 1], verts[baseIdx + 2]);
            const float len = (tip - root).length();
            if (len < 1e-4f) continue;
            const QVector3D dir = (tip - root) / len;
            const float coneH = 0.24f * len, coneR = 0.09f * len;
            const QVector3D coneBase = tip - coneH * dir;
            QVector3D up = (std::abs(dir.y()) < 0.9f) ? QVector3D(0, 1, 0) : QVector3D(1, 0, 0);
            const QVector3D right = QVector3D::crossProduct(dir, up).normalized();
            up = QVector3D::crossProduct(right, dir).normalized();
            auto push3 = [&](const QVector3D& v) {
                coneVerts.insert(coneVerts.end(), { v.x(), v.y(), v.z() });
                coneColors.insert(coneColors.end(), { axisRGBf[axis][0], axisRGBf[axis][1], axisRGBf[axis][2] });
            };
            for (int sgi = 0; sgi < SEGS; ++sgi) {
                const float a0 = 2.0f * float(M_PI) * sgi / SEGS;
                const float a1 = 2.0f * float(M_PI) * (sgi + 1) / SEGS;
                const QVector3D p0 = coneBase + coneR * (std::cos(a0) * right + std::sin(a0) * up);
                const QVector3D p1 = coneBase + coneR * (std::cos(a1) * right + std::sin(a1) * up);
                push3(tip); push3(p0); push3(p1);
                push3(coneBase); push3(p1); push3(p0);
            }
        }
    }
    if (!coneVerts.empty()) {
        GLuint vao = 0, vbo[2] = {0, 0};
        ef->glGenVertexArrays(1, &vao);
        ef->glBindVertexArray(vao);
        ef->glGenBuffers(2, vbo);
        ef->glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
        ef->glBufferData(GL_ARRAY_BUFFER, coneVerts.size() * sizeof(float), coneVerts.data(), GL_STREAM_DRAW);
        ef->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
        ef->glEnableVertexAttribArray(0);
        ef->glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
        ef->glBufferData(GL_ARRAY_BUFFER, coneColors.size() * sizeof(float), coneColors.data(), GL_STREAM_DRAW);
        ef->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
        ef->glEnableVertexAttribArray(1);
        axisShaderProgram->bind();
        axisShaderProgram->setUniformValue("uMVP", mvp);
        ef->glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(coneVerts.size() / 3));
        axisShaderProgram->release();
        ef->glBindBuffer(GL_ARRAY_BUFFER, 0);
        ef->glBindVertexArray(0);
        ef->glDeleteBuffers(2, vbo);
        ef->glDeleteVertexArrays(1, &vao);
    }

    // Restore state
    f->glEnable(GL_DEPTH_TEST);
    f->glDisable(GL_BLEND);
}
