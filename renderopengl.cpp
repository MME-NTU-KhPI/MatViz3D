#include "renderopengl.h"

#include <QDebug>
#include <QTimer>
#include <QImage>
#include <QThread>

#include <QOpenGLExtraFunctions>


RenderOpenGL::RenderOpenGL()
{

    xRot = 0;
    yRot = 0;
    zRot = 0;
    distance = 2.0f;
    numCubes = 1;
    bgColor.setRgbF(0.21f, 0.21f, 0.21f);

    initializeOpenGLFunctions();

    QOpenGLContext *glContext = QOpenGLContext::currentContext();
    if (!glContext) {
        qDebug() << "RenderOpenGL: OpenGL context is NULL!";
    } else {
        qDebug() << "RenderOpenGL: OpenGL initialized. Context valid:" << glContext->isValid();
    }
    this->initializeGL();

}


void RenderOpenGL::render()
{
    this->paintGL();
    update();
}



RenderOpenGL::~RenderOpenGL()
{
//    makeCurrent(); // Ensure context is active before cleanup
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    f->glDeleteBuffers(1, vboIds);
//    doneCurrent(); // Release context
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
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    f->glClearColor(bgColor.redF(), bgColor.greenF(), bgColor.blueF(), bgColor.alphaF());

    // set up light colors (ambient, diffuse, specular)
    GLfloat lightKa[] = {.2f, .2f, .2f, 1.0f};  // ambient light
    GLfloat lightKd[] = {.7f, .7f, .7f, 1.0f};  // diffuse light
    GLfloat lightKs[] = {.75f, .75f, .75f, 1.0f};  // specular light
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightKa);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightKd);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightKs);

    glLightfv(GL_LIGHT1, GL_AMBIENT, lightKa);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, lightKd);
    glLightfv(GL_LIGHT1, GL_SPECULAR, lightKs);

    glLightfv(GL_LIGHT2, GL_AMBIENT, lightKa);
    glLightfv(GL_LIGHT2, GL_DIFFUSE, lightKd);
    glLightfv(GL_LIGHT2, GL_SPECULAR, lightKs);


    // position the light
    float lightPosZ[4] = {1, 0, 1, 0}; // directional light
    float lightPosX[4] = {1, 1, 0, 0}; // directional light
    float lightPosY[4] = {1, 1, 0, 0}; // directional light

    glLightfv(GL_LIGHT0, GL_POSITION, lightPosZ);
    glLightfv(GL_LIGHT1, GL_POSITION, lightPosX);
    glLightfv(GL_LIGHT2, GL_POSITION, lightPosY);

    glEnable(GL_LIGHT0);                        // MUST enable each light source after configuration
    glEnable(GL_LIGHT1);                        // MUST enable each light source after configuration
    glEnable(GL_LIGHT2);                        // MUST enable each light source after configuration

}

void RenderOpenGL::debugCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                               GLsizei length, const GLchar* message, const void* userParam) {
    Q_UNUSED(length);
    Q_UNUSED(userParam);

    // Map enums to human-readable strings
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

    // Log the debug message using QDebug
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

    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();

    // Enable OpenGL debug output
    f->glEnable(GL_DEBUG_OUTPUT);
    f->glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

    QOpenGLExtraFunctions* ef = QOpenGLContext::currentContext()->extraFunctions();

    // Register the debug message callback
    ef->glDebugMessageCallback(debugCallback, this);


    glShadeModel(GL_SMOOTH);
    f->glEnable(GL_COLOR_MATERIAL);
    f->glEnable(GL_LIGHTING);
    f->glEnable(GL_LIGHT0);

    // enable /disable features
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    //glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_CULL_FACE);

    glEnable(GL_NORMALIZE); // normalize normals length to 1

    // track material ambient and diffuse from surface color, call it before glEnable(GL_COLOR_MATERIAL)
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_COLOR_MATERIAL);

    glClearColor(bgColor.redF(), bgColor.greenF(), bgColor.blueF(), bgColor.alphaF());     // background color
    glClearStencil(0);                          // clear stencil buffer
    glClearDepth(numCubes);                     // 0 is near, 1 is far
    glDepthFunc(GL_LESS);

    glEnable(GL_LINE_SMOOTH); //smooth line drawing

    // TODO: fix transperancy
    glEnable(GL_ALPHA_TEST);
    glEnable(GL_BLEND); // enbale blending
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    initLights(); // introduce light sources

    // set the material properties
    GLfloat matAmb[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat matDif[] = { 0.6f, 0.6f, 0.6f, 1.0f };
    GLfloat matSpc[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat matShn[] = { 50.0f };

    glMaterialfv(GL_FRONT, GL_AMBIENT, matAmb);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, matDif);
    glMaterialfv(GL_FRONT, GL_SPECULAR, matSpc);
    glMaterialfv(GL_FRONT, GL_SHININESS, matShn);

    this->initializeVBO();

}


void RenderOpenGL::drawAxis()
{
    // Draw X-axis (Red)
    glColor3f(1.0, 0.0, 0.0);
    glBegin(GL_LINES);
    glVertex3f(-numCubes/2, -numCubes/2, -numCubes/2);
    glVertex3f(1.15*numCubes, -numCubes/2, -numCubes/2);
    glEnd();

    // Draw Y-axis (Green)
    glColor3f(0.0, 1.0, 0.0);
    glBegin(GL_LINES);
    glVertex3f(-numCubes/2, -numCubes/2, -numCubes/2);
    glVertex3f(-numCubes/2, 1.15*numCubes, -numCubes/2);
    glEnd();

    // Draw Z-axis (Blue)
    glColor3f(0.0, 0.0, 1.0);
    glBegin(GL_LINES);
    glVertex3f(-numCubes/2, -numCubes/2, -numCubes/2);
    glVertex3f(-numCubes/2, -numCubes/2, 1.15*numCubes);
    glEnd();
}

void RenderOpenGL::initializeVBO()
{
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    // Create buffers for vertex, color, and normal data
    f->glGenBuffers(1, vboIds);
    // Bind the vertex buffer
    f->glBindBuffer(GL_ARRAY_BUFFER, vboIds[0]);
    // upload buffer to GPU
    f->glBufferData(GL_ARRAY_BUFFER, voxelScene.size() * sizeof(Voxel), voxelScene.data() + offsetof(Voxel, x), GL_STATIC_DRAW);
}

void RenderOpenGL::updateVBO()
{
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();

    f->glBindBuffer(GL_ARRAY_BUFFER, vboIds[0]);
    f->glBufferData(GL_ARRAY_BUFFER, voxelScene.size() * sizeof(Voxel), voxelScene.data() + offsetof(Voxel, x), GL_STATIC_DRAW);
}

void RenderOpenGL::paintGL()
{
    if (isVBOupdateRequired)
    {
        this->updateVBO();
        isVBOupdateRequired = false;
    }

    // clear buffer
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // save the initial ModelView matrix before modifying ModelView matrix
    glPushMatrix();

    glLoadIdentity();

    glTranslatef(0.0, 0.0, -distance);
    glRotatef(xRot / 16.0, 1.0, 0.0, 0.0);
    glRotatef(yRot / 16.0, 0.0, 1.0, 0.0);
    glRotatef(zRot / 16.0, 0.0, 0.0, 1.0);
    //qDebug() << "paintGL rotations angles:" << xRot / 16.0 << yRot / 16.0 << zRot / 16.0;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    glEnable(GL_NORMALIZE);

    // Draw X-Y-Z axis in Red for X, Green for Y, Blue for Z
    this->drawAxis();


    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    // Bind VBO
    f->glBindBuffer(GL_ARRAY_BUFFER, vboIds[0]);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);

    // Load vertix array
    glVertexPointer(3, GL_FLOAT, sizeof(Voxel), (void*)offsetof(Voxel, x));

    if (plotWireFrame == true)
    {
        glEnable(GL_POLYGON_OFFSET_LINE); // Enable polygon offset for lines
        glPolygonOffset(-1.0f, -1.0f); // Set the polygon offset factor and units
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(0.1);

        for (size_t i = 0; i < voxelScene.size() ; i += 4)
        {
            glColor4f(0.5f, 0.5f, 0.5f, 0.5f);
            glDrawArrays(GL_POLYGON, i, 4); // plot 4 lines for each face
        }
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    //load color array
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Voxel), (void*)offsetof(Voxel, r));
    //load normal array
    glNormalPointer(GL_BYTE, sizeof(Voxel), (void*)offsetof(Voxel, nx));

    //Draw all quads by vertixes
    glDrawArrays(GL_QUADS, 0, voxelScene.size()); // 24 vertices per voxel (6 faces * 4 vertices)

    // Unbind the buffers and disable client-side capabilities
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);

    f->glBindBuffer(GL_ARRAY_BUFFER, 0);

    glPopMatrix();
}


void RenderOpenGL::resizeGL(int width, int height)
{
    //   int side = qMin(width, height);
    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // Set up perspective projection using glFrustum
    GLfloat aspect = (GLfloat)width / (GLfloat)height;
    GLfloat fov = 45.0f;
    GLfloat baseNear = 1.0f;     // Базовое значение ближней границы
    GLfloat baseFar = 1000.0f;   // Базовое значение дальней границы
    GLfloat minNear = 0.1f;      // Минимальное значение ближней границы
    GLfloat maxFar = 5000.0f;    // Максимальное значение дальней границы
    GLfloat nearVal = std::max(baseNear / zoomFactor, minNear);
    GLfloat farVal = std::min(baseFar * zoomFactor, maxFar);
    GLfloat top = nearVal * std::tan(fov * 0.5f * 3.14159f / 180.0f);
    GLfloat bottom = -top;
    GLfloat left = bottom * aspect;
    GLfloat right = top * aspect;

#ifdef QT_OPENGL_ES
    glFrustum(left, right, bottom, top, nearVal, farVal);
#else
    glFrustum(left, right, bottom, top, nearVal, farVal);
#endif
    glMatrixMode(GL_MODELVIEW);
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
}


void RenderOpenGL::setPlotWireFrame(bool status)
{
    plotWireFrame = status;
}
