#include "renderopengl.h"
#include "qopenglframebufferobject.h"

#include <QDebug>
#include <QTimer>
#include <QImage>
#include <QThread>

#include <QOpenGLExtraFunctions>


RenderOpenGL::RenderOpenGL()
{
    qDebug() << "RenderOpenGL::RenderOpenGL() - START";
    xRot = 0;
    yRot = 0;
    zRot = 0;
    distance = 2.0f;
    numCubes = 1;
    width = 300;
    height = 300;
    bgColor.setRgbF(0.21f, 0.21f, 0.21f);
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
    this->paintGL();
    update();
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

    f->glEnable(GL_DEBUG_OUTPUT);
    f->glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    ef->glDebugMessageCallback(debugCallback, this);

    f->glClearColor(bgColor.redF(), bgColor.greenF(), bgColor.blueF(), bgColor.alphaF());
    ef->glClearDepthf(1.0f);
    f->glDepthFunc(GL_LEQUAL);
    f->glEnable(GL_DEPTH_TEST);

    //f->glEnable(GL_POLYGON_OFFSET_FILL);
    //f->glPolygonOffset(1.0f, 1.0f); // push filled polygons back slightly

    qDebug() << "RenderOpenGL::initializeGL() - creating main shader program";

    const char* vertexShaderSource = R"(
        #version 130
        attribute vec3 aPosition;
        attribute vec4 aColor;
        attribute vec3 aNormal;

        uniform mat4 uMVP;
        uniform mat4 uModel;
        uniform mat4 uView;
        uniform mat4 uProjection;

        varying vec3 FragPos;
        varying vec3 Normal;
        varying vec4 Color;

        void main()
        {
            FragPos = vec3(uModel * vec4(aPosition, 1.0));
            Normal = normalize(mat3(uModel) * aNormal);
            Color = aColor;
            gl_Position = uMVP * vec4(aPosition, 1.0);
        }
    )";


    const char* fragmentShaderSource = R"(
        #version 130
        varying vec3 FragPos;
        varying vec3 Normal;
        varying vec4 Color;

        uniform vec3 uLightDirections[6];
        uniform float uLightWeights[6];
        uniform vec3 uViewPos;
        uniform int uDebugMode;

        void main()
        {
            // --- Geometry ---
            vec3 norm    = normalize(Normal);
            // if (!gl_FrontFacing) norm = -norm;
            vec3 viewDir = normalize(uViewPos - FragPos);

            // --- Ambient ---
            // Tinted by face color so shadowed faces still show their hue,
            // not a flat gray. Low enough to not wash out diffuse.
            vec3 ambient = 0.50 * Color.rgb;
            vec3 result  = ambient;

            // --- Lights ---

            for (int i = 0; i < 6; i++)
            {
                vec3 lightDir = uLightDirections[i];
                vec3 halfDir  = normalize(lightDir + viewDir);

                // max(dot, 0) not abs() — abs() creates dark bands at 90deg
                // because it mirrors the lighting response, causing entire
                // layers of faces to go dark simultaneously at certain angles.
                float diff = max(dot(norm, lightDir), 0.0);

                // Specular: reduced exponent (16 vs 32) for wider softer highlight,
                // reduced intensity (0.15) so 3 lights don't blow out bright faces.
                float spec = pow(max(dot(norm, halfDir), 0.0), 16.0);

                vec3 diffuse  = diff * uLightWeights[i] * Color.rgb;
                vec3 specular = spec * uLightWeights[i] * 0.15 * vec3(1.0);
                result += (diffuse + specular);
            }

            // --- Clamp ---
            // Prevents overbright faces making adjacent dark faces look
            // even darker by contrast. Hard requirement when accumulating
            // multiple lights without HDR tonemapping.
            result = clamp(result, 0.0, 1.0);

            // --- Debug modes ---
            if (uDebugMode == 1) {
                // Raw vertex color — confirms color data is reaching shader correctly.
                // If all faces same color here, node_colors assignment is broken.
                gl_FragColor = Color;

            } else if (uDebugMode == 2) {
                // Absolute normal as RGB. Each axis pair shows as one color:
                //   X faces = red, Y faces = green, Z faces = blue.
                // If any face shows wrong color, normal encoding or winding is wrong.
                // If all faces show same dark color, GLbyte normals are near zero —
                // check that n[] array uses +-127 not +-1.
                gl_FragColor = vec4(abs(norm.x), abs(norm.y), abs(norm.z), 1.0);

            } else if (uDebugMode == 3) {
                // Flat white — confirms geometry is being drawn and depth test works.
                // If scene disappears here, the issue is in geometry not lighting.
                gl_FragColor = vec4(1.0, 1.0, 1.0, Color.a);

            } else {
                // Normal lighting mode (uDebugMode == 0)
                gl_FragColor = vec4(result, Color.a);
            }
        }
 )";

    const char* axisVertexShaderSource = R"(
        #version 130
        attribute vec3 aPosition;
        attribute vec3 aColor;

        uniform mat4 uMVP;

        varying vec3 Color;

        void main()
        {
            Color = aColor;
            gl_Position = uMVP * vec4(aPosition, 1.0);
        }
    )";

    const char* axisFragmentShaderSource = R"(
        #version 130
        varying vec3 Color;
        void main()
        {
            gl_FragColor = vec4(Color, 1.0);
        }
    )";

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


void RenderOpenGL::drawCornerAxes()
{
    if (!axisShaderProgram) return;

    QOpenGLFunctions    *f  = QOpenGLContext::currentContext()->functions();
    QOpenGLExtraFunctions *ef = QOpenGLContext::currentContext()->extraFunctions();
    if (!f || !ef) return;

    // Convert logical → physical pixels
    const int physW      = static_cast<int>(width  * m_dpr);
    const int physH      = static_cast<int>(height * m_dpr);
    const int physSize   = static_cast<int>(m_cornerSize   * m_dpr);
    const int physMargin = static_cast<int>(m_cornerMargin * m_dpr);

    f->glViewport(physW - physSize - physMargin,  // x: right edge
                  physH - physSize - physMargin,                      // y: bottom edge (GL origin)
                  physSize,
                  physSize);

/*
    // ── 1. Switch to a small corner viewport ──────────────────────────────
    f->glViewport(width  - m_cornerSize - m_cornerMargin,
                  height - m_cornerSize - m_cornerMargin,
                  m_cornerSize,
                  m_cornerSize);
*/
    // Clear only the depth for this region so axes always appear on top
    f->glClear(GL_DEPTH_BUFFER_BIT);
    f->glEnable(GL_DEPTH_TEST);

    // ── 2. Build rotation-only MVP (no scene translation/scale) ───────────
    QMatrix4x4 view;
    view.setToIdentity();
    view.translate(0.0f, 0.0f, -2.8f);          // pull camera back slightly
    view.rotate( xRot / 16.0f, 1.0f, 0.0f, 0.0f);
    view.rotate( yRot / 16.0f, 0.0f, 1.0f, 0.0f);
    view.rotate( zRot / 16.0f, 0.0f, 0.0f, 1.0f);

    QMatrix4x4 proj;
    proj.setToIdentity();
    proj.perspective(45.0f, 1.0f, 0.1f, 10.0f); // square aspect — matches square viewport

    QMatrix4x4 mvp = proj * view;

    // ── 3. Geometry: unit-length axes + small arrow-tip cross-bars ─────────
    //  X = red   Y = green   Z = blue
    //  Each axis: shaft + two short perpendicular lines at tip (cheap arrowhead)
    const float L  = 0.8f;   // shaft length
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

    f->glLineWidth(1.0f);
    ef->glDrawArrays(GL_LINES, 0, vertexCount);

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

    f->glLineWidth(1.0f);
    ef->glDrawArrays(GL_LINES, 0,
                     static_cast<GLsizei>(lineVerts.size() / 3));

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

    ef->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Voxel), (void*)offsetof(Voxel, x));
    ef->glEnableVertexAttribArray(0);

    ef->glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Voxel), (void*)offsetof(Voxel, r));
    ef->glEnableVertexAttribArray(1);

    ef->glVertexAttribPointer(2, 3, GL_BYTE, GL_TRUE, sizeof(Voxel), (void*)offsetof(Voxel, nx));
    ef->glEnableVertexAttribArray(2);

    ef->glBindBuffer(GL_ARRAY_BUFFER, 0);
    ef->glBindVertexArray(0);
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

    f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    f->glDepthMask(GL_TRUE);
    f->glEnable(GL_NORMALIZE);

    QMatrix4x4 view;
    view.setToIdentity();
    view.translate(0.0f, 0.0f, -distance);
    view.rotate(xRot / 16.0f, 1.0f, 0.0f, 0.0f);
    view.rotate(yRot / 16.0f, 0.0f, 1.0f, 0.0f);
    view.rotate(zRot / 16.0f, 0.0f, 0.0f, 1.0f);

    QMatrix4x4 model;
    model.setToIdentity();

    QMatrix4x4 mvp = m_projection * view * model;

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

    ef->glBindVertexArray(vaoId);

    f->glEnable(GL_BLEND);
    f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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

    if (plotWireFrame) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        f->glLineWidth(0.5f);

        for (size_t i = 0; i < voxelScene.size(); i += 4) {
            ef->glDrawArrays(GL_POLYGON, i, 4);
        }

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    //qDebug() << "Drawing voxels - count:" << voxelScene.size();
    if (!voxelScene.empty()) {
        for (size_t i = 0; i < voxelScene.size(); i += 4) {
            // Draw quad as triangle fan (v0, v1, v2, v3)
            ef->glDrawArrays(GL_TRIANGLE_FAN, i, 4);
        }
    //    qDebug() << "Voxel drawing completed";
    } else {
    //    qDebug() << "No voxels to draw!";
    }

     drawOrientationGlyphs();

    f->glDisable(GL_BLEND);

    ef->glBindVertexArray(0);
    shaderProgram->release();




    // ── Corner orientation gizmo ─────────────────────────────────────
    drawCornerAxes();

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
    this->distance = 2 * numCubes;
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

void RenderOpenGL::setPlotWireFrame(bool status)
{
    plotWireFrame = status;
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

QImage RenderOpenGL::captureScreenshot()
{
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    if (!ctx) {
        return QImage();
    }

    QOpenGLFunctions *f = ctx->functions();

    int width = this->width;
    int height = this->height;

    QImage screenshot(width, height, QImage::Format_RGBA8888);

    f->glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, screenshot.bits());

    screenshot = screenshot.mirrored();

    return screenshot;
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
                                         const std::vector<float>& colors)
{
    orientationVerts  = verts;
    orientationColors = colors;
    orientationVBOdirty = true;
}

void RenderOpenGL::setShowOrientations(bool show)
{
    showOrientations = show;
    update();
}

// ── Draw orientation glyphs as GL_LINES ───────────────────────────────

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
    view.translate(0.0f, 0.0f, -distance);
    view.rotate(xRot / 16.0f, 1.0f, 0.0f, 0.0f);
    view.rotate(yRot / 16.0f, 0.0f, 1.0f, 0.0f);
    view.rotate(zRot / 16.0f, 0.0f, 0.0f, 1.0f);
    QMatrix4x4 mvp = m_projection * view;

    axisShaderProgram->bind();
    axisShaderProgram->setUniformValue("uMVP", mvp);

    f->glEnable(GL_DEPTH_TEST);
    f->glLineWidth(1.0f);

    ef->glBindVertexArray(orientationVAO);
    const GLsizei vertexCount =
        static_cast<GLsizei>(orientationVerts.size() / 3);
    ef->glDrawArrays(GL_LINES, 0, vertexCount);
    ef->glBindVertexArray(0);

    axisShaderProgram->release();
}

