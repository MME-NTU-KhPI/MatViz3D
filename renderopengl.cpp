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

        uniform vec3 uLightPositions[3];
        uniform vec3 uViewPos;
        uniform int uDebugMode;

        void main()
        {
            vec3 norm = normalize(Normal);
            vec3 viewDir = normalize(uViewPos - FragPos);

            vec3 result = vec3(0.4);

            if (uDebugMode == 3) {
                gl_FragColor = vec4(1.0, 1.0, 1.0, Color.a);
                return;
            }

            for (int i = 0; i < 3; i++) {
                vec3 lightDir = normalize(uLightPositions[i] - FragPos);
                vec3 halfDir = normalize(lightDir + viewDir);
                float diff = max(dot(norm, lightDir), 0.0);
                float spec = pow(max(dot(norm, halfDir), 0.0), 32.0);
                vec3 diffuse = diff * vec3(0.8, 0.8, 0.8) * Color.rgb;
                vec3 specular = spec * vec3(0.3, 0.3, 0.3);
                
                result += diffuse + specular;
            }

            if (uDebugMode == 1) {
                // Color mode: show face colors directly
                gl_FragColor = Color;
            } else if (uDebugMode == 2) {
                // Normal mode: RGB based on normal direction
                gl_FragColor = vec4(abs(norm.x), abs(norm.y), abs(norm.z), Color.a);
            } else {
                // Normal lighting mode
                gl_FragColor = vec4(result, Color.a);
            }
        }
    )";
/*
    const char* fragmentShaderSource = R"(
        #version 130
        varying vec3 FragPos;
        varying vec3 Normal;
        varying vec4 Color;

        uniform vec3 uLightPositions[3];
        uniform vec3 uViewPos;
        uniform int uDebugMode;

        void main()
        {
            vec3 norm = normalize(Normal);

            vec3 result = vec3(0.4);

            for (int i = 0; i < 3; i++) {
                vec3 lightDir = normalize(uLightPositions[i] - FragPos);
                float diff = max(dot(norm, lightDir), 0.0);
                vec3 diffuse = diff * vec3(0.8, 0.8, 0.8);
                result += diffuse;
            }

            if (uDebugMode == 1) {
                // Color mode: visual face normals with RGB colors
                gl_FragColor = Color;
            } else if (uDebugMode == 2) {
                // Normal mode: show normals as RGB
                gl_FragColor = vec4((norm + 1.0) * 0.5, Color.a);
            } else if (uDebugMode == 3) {
                // Test cube: make it white
                gl_FragColor = vec4(1.0, 1.0, 1.0, Color.a);
            } else {
                // Normal mode 0: two-sided lighting
                vec3 viewDir = normalize(uViewPos - FragPos);
                float NdotV = abs(dot(norm, viewDir));
                vec3 resultLit = vec3(0.4) * Color.rgb;

                for (int i = 0; i < 3; i++) {
                    vec3 lightDir = normalize(uLightPositions[i] - FragPos);
                    vec3 halfDir = normalize(lightDir + viewDir);
                    float diff = max(dot(norm, lightDir), 0.0);
                    float spec = pow(max(dot(norm, halfDir), 0.0), 32.0);
                    vec3 diffuse = diff * vec3(0.8, 0.8, 0.8) * Color.rgb;
                    vec3 specular = spec * vec3(0.3, 0.3, 0.3);
                    resultLit += diffuse + specular;
                }

                result = resultLit;
            }

            gl_FragColor = vec4(result, Color.a);
        }
    )";
*/
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

void RenderOpenGL::drawAxisWithMVP(const QMatrix4x4& mvp)
{
    if (!axisShaderProgram) {
        return;
    }

    float halfNum = static_cast<float>(numCubes) / 2.0f;
    float multNum = 1.15f * static_cast<float>(numCubes);

    std::vector<float> axisVertices = {
        -halfNum, -halfNum, -halfNum,
        multNum, -halfNum, -halfNum,
        -halfNum, -halfNum, -halfNum,
        -halfNum, multNum, -halfNum,
        -halfNum, -halfNum, -halfNum,
        -halfNum, -halfNum, multNum
    };

    std::vector<float> axisColors = {
        1.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f
    };

    QOpenGLExtraFunctions *ef = QOpenGLContext::currentContext()->extraFunctions();
    if (!ef) {
        return;
    }

    axisShaderProgram->bind();
    axisShaderProgram->setUniformValue("uMVP", mvp);

    GLuint axisVAO;
    ef->glGenVertexArrays(1, &axisVAO);
    ef->glBindVertexArray(axisVAO);

    GLuint axisVBO[2];
    ef->glGenBuffers(2, axisVBO);

    ef->glBindBuffer(GL_ARRAY_BUFFER, axisVBO[0]);
    ef->glBufferData(GL_ARRAY_BUFFER, axisVertices.size() * sizeof(float), axisVertices.data(), GL_STATIC_DRAW);
    ef->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    ef->glEnableVertexAttribArray(0);

    ef->glBindBuffer(GL_ARRAY_BUFFER, axisVBO[1]);
    ef->glBufferData(GL_ARRAY_BUFFER, axisColors.size() * sizeof(float), axisColors.data(), GL_STATIC_DRAW);
    ef->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    ef->glEnableVertexAttribArray(1);

    ef->glDrawArrays(GL_LINES, 0, 6);

    ef->glBindBuffer(GL_ARRAY_BUFFER, 0);
    ef->glBindVertexArray(0);
    ef->glDeleteVertexArrays(1, &axisVAO);
    ef->glDeleteBuffers(2, axisVBO);

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

    QVector3D lightPositions[3] = {
        QVector3D(100.0f, 0.0f, 100.0f),
        QVector3D(100.0f, 100.0f, 0.0f),
        QVector3D(0.0f, 100.0f, 100.0f)
    };

    for (int i = 0; i < 3; i++) {
        shaderProgram->setUniformValueArray(QString("uLightPositions[%1]").arg(i).toUtf8().constData(), &lightPositions[i], 1);
    }

    QVector3D viewPos(0.0f, 0.0f, -distance);
    shaderProgram->setUniformValue("uViewPos", viewPos);
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

    f->glDisable(GL_BLEND);

    ef->glBindVertexArray(0);
    shaderProgram->release();
}

void RenderOpenGL::resizeGL(int width, int height)
{
    qDebug() << "RenderOpenGL::resizeGL() - START - width:" << width << "height:" << height;
    this->width = width;
    this->height = height;

    if (height <= 0 || width <= 0) {
        qDebug() << "RenderOpenGL::resizeGL() - invalid dimensions, returning";
        return;
    }

    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    if (!ctx) {
        qDebug() << "RenderOpenGL::resizeGL() - No OpenGL context, skipping viewport setting";
    } else {
        QOpenGLFunctions *f = ctx->functions();
        f->glViewport(0, 0, width, height);
        qDebug() << "RenderOpenGL::resizeGL() - viewport set";
    }

    float aspect = static_cast<float>(width) / static_cast<float>(height);
    float sceneRadius = numCubes * 0.866f;

    float fov = 45.0f;
    if (numCubes > 30) {
        fov = 50.0f;
    }

    float distanceToSceneCenter = distance;
    float nearVal = std::max(0.1f, distanceToSceneCenter - sceneRadius - 1.0f);
    float farVal = distanceToSceneCenter + sceneRadius + 1.0f;

    if (nearVal < 0.1f) nearVal = 0.1f;
    if (farVal < nearVal + 1.0f) farVal = nearVal + 1.0f;

    float depthRatio = farVal / nearVal;
    if (depthRatio > 1000.0f) {
        nearVal = farVal / 1000.0f;
    }

    m_projection.setToIdentity();
    m_projection.perspective(fov, aspect, nearVal, farVal);

    qDebug() << "ResizeGL: width=" << width << "height=" << height
             << "near=" << nearVal << "far=" << farVal
             << "distance=" << distance << "numCubes=" << numCubes
             << "fov=" << fov;
    qDebug() << "RenderOpenGL::resizeGL() - END";
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
    this->distance = 2 * numCubes;
    this->numCubes = numCubes;
    update();
}

void RenderOpenGL::setDistZoomFactor(float distance, float zoomFactor)
{
    this->distance = distance;
    this->zoomFactor = zoomFactor;
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

                // Front face (+Z) - Red color
                voxelScene.push_back({px, py, pz + size, 255, 0, 0, 255, 0, 0, 127});
                voxelScene.push_back({px + size, py, pz + size, 255, 0, 0, 255, 0, 0, 127});
                voxelScene.push_back({px + size, py + size, pz + size, 255, 0, 0, 255, 0, 0, 127});
                voxelScene.push_back({px, py + size, pz + size, 255, 0, 0, 255, 0, 0, 127});

                // Back face (-Z) - Green color
                voxelScene.push_back({px, py, pz, 0, 255, 0, 255, 0, 0, -128});
                voxelScene.push_back({px + size, py, pz, 0, 255, 0, 255, 0, 0, -128});
                voxelScene.push_back({px + size, py + size, pz, 0, 255, 0, 255, 0, 0, -128});
                voxelScene.push_back({px, py + size, pz, 0, 255, 0, 255, 0, 0, -128});

                // Right face (+X) - Blue color
                voxelScene.push_back({px + size, py, pz, 0, 0, 255, 255, 127, 0, 0});
                voxelScene.push_back({px + size, py, pz + size, 0, 0, 255, 255, 127, 0, 0});
                voxelScene.push_back({px + size, py + size, pz + size, 0, 0, 255, 255, 127, 0, 0});
                voxelScene.push_back({px + size, py + size, pz, 0, 0, 255, 255, 127, 0, 0});

                // Left face (-X) - Yellow color
                voxelScene.push_back({px, py, pz, 255, 255, 0, 255, -128, 0, 0});
                voxelScene.push_back({px, py + size, pz, 255, 255, 0, 255, -128, 0, 0});
                voxelScene.push_back({px, py + size, pz + size, 255, 255, 0, 255, -128, 0, 0});
                voxelScene.push_back({px, py, pz + size, 255, 255, 0, 255, -128, 0, 0});

                // Top face (+Y) - Magenta color
                voxelScene.push_back({px, py + size, pz, 255, 0, 255, 255, 0, 127, 0});
                voxelScene.push_back({px + size, py + size, pz, 255, 0, 255, 255, 0, 127, 0});
                voxelScene.push_back({px + size, py + size, pz + size, 255, 0, 255, 255, 0, 127, 0});
                voxelScene.push_back({px, py + size, pz + size, 255, 0, 255, 255, 0, 127, 0});

                // Bottom face (-Y) - Cyan color
                voxelScene.push_back({px, py, pz, 0, 255, 255, 255, 0, -128, 0});
                voxelScene.push_back({px, py, pz + size, 0, 255, 255, 255, 0, -128, 0});
                voxelScene.push_back({px + size, py, pz + size, 0, 255, 255, 255, 0, -128, 0});
                voxelScene.push_back({px + size, py, pz, 0, 255, 255, 255, 0, -128, 0});
            }
        }
    }

    qDebug() << "createTestScene() - created" << voxelScene.size() << "vertices (" 
             << (voxelScene.size() / 4) << " quads)";
    isVBOupdateRequired = true;
    update();
}
