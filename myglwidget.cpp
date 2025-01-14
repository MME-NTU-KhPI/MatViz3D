// myglwidget.cpp

#include <QtWidgets>
#include "myglwidget.h"

#include <cmath> // include for sin and cos functions
#include <vector>
#include <cstdlib>
#include <ctime>
#include <QMatrix4x4>
#include <QColor>
#include <array>
#include <QFileDialog>
#include <QTextStream>
#include <QDebug>


MyGLWidget::MyGLWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    xRot = 0;
    yRot = 0;
    zRot = 0;
    distance = 2.0f;
    numCubes = 1;
    voxels = nullptr;
    wr = nullptr;
    timer = new QTimer(this);
    plotComponent = 0;
    bgColor.setRgbF(0.21f, 0.21f, 0.21f);
}

MyGLWidget::~MyGLWidget()
{
    delete timer;
    makeCurrent(); // Ensure context is active before cleanup
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    f->glDeleteBuffers(1, vboIds);
    doneCurrent(); // Release context
}

QSize MyGLWidget::minimumSizeHint() const
{
    return QSize(50, 50);
}

QSize MyGLWidget::sizeHint() const
{
    return QSize(400, 400);
}

void qNormalizeAngle(int &angle)
{
    while (angle < -360 * 16)
        angle += 360 * 16;
    while (angle >= 360 * 16)
        angle -= 360 * 16;
}

void MyGLWidget::setXRotation(int angle)
{
    qNormalizeAngle(angle);
    if (angle != xRot) {
        xRot = angle;
        emit xRotationChanged(angle);
        update();
    }
}

void MyGLWidget::setYRotation(int angle)
{
    qNormalizeAngle(angle);
    if (angle != yRot) {
        yRot = angle;
        emit yRotationChanged(angle);
        update();
    }
}

void MyGLWidget::setZRotation(int angle)
{
    qNormalizeAngle(angle);
    if (angle != zRot) {
        zRot = angle;
        emit zRotationChanged(angle);
        update();
    }
}


/**
 * Set the view to standard isometric projection.
 */
void MyGLWidget::setIsometricView()
{
    xRot =  35.26 * 16;
    yRot = -45.00 * 16;
    zRot =  0;
    update();
}

/**
 * Set the view to standard dimetric projection.
 */
void MyGLWidget::setDimetricView()
{
    xRot =  26.57 * 16;
    yRot = -45.00 * 16;
    zRot =  0;
    update();
}



void MyGLWidget::setNumCubes(int numCubes)
{
    distance = 2 * numCubes;
    this->numCubes = numCubes;
    this->wr = nullptr;
    update();
}


void MyGLWidget::setNumColors(int numColors)
{
    this->numColors = numColors;
    this->colors = generateDistinctColors();
    directionFactors.resize(numColors);

    // Initialize direction factors randomly for each color
    for (int i = 0; i < numColors; i++) {
        directionFactors[i] = ((rand() % 2) == 0) ? 1.0f : -1.0f;
    }
    this->wr = nullptr;
}

void MyGLWidget::setDelayAnimation(int delayAnimation)
{
    this->delayAnimation = delayAnimation;
}


void MyGLWidget::setDistanceFactor(int factor)
{
    distanceFactor = log(factor/10.0 + 1) * numCubes; // 5.0 - is a sensetivity factor. log - inroduce soft sizing
    calculateScene();
    update();
}


void MyGLWidget::setPlotWireFrame(bool status)
{
    this->plotWireFrame = status;
}

/**
 * Capture a screenshot of the OpenGL widget and store it in a buffer.
 * @return A QImage containing the screenshot.
 */
QImage MyGLWidget::captureScreenshot()
{
    this->makeCurrent(); // Ensure the OpenGL context is current

    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();

    // Get the dimensions of the widget
    int width = this->width();
    int height = this->height();

    // Create a buffer to store pixel data
    QImage screenshot(width, height, QImage::Format_RGBA8888);

    // Read pixels from OpenGL framebuffer
    f->glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, screenshot.bits());

    // Convert the image from bottom-left origin to top-left origin
    screenshot = screenshot.mirrored();
    this->doneCurrent();

    return screenshot;
}

/**
 * Capture a screenshot and copy it to the system clipboard.
 */
void MyGLWidget::captureScreenshotToClipboard()
{
    QImage screenshot = captureScreenshotWithWhiteBackground();
    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setImage(screenshot);
}

/**
 * Capture a screenshot of the OpenGL widget, replace the background with white, and store it in a buffer.
 * @return A QImage containing the modified screenshot.
 */
QImage MyGLWidget::captureScreenshotWithWhiteBackground()
{
    QImage screenshot = captureScreenshot();

    // Modify the screenshot to replace gray background with white
    QImage modifiedScreenshot(screenshot.size(), QImage::Format_RGB32);
    const float tol = 0.015f;
    for (int y = 0; y < screenshot.height(); ++y) {
        for (int x = 0; x < screenshot.width(); ++x) {
            QColor pixelColor = screenshot.pixelColor(x, y);

            // Replace gray bg color with white
            if (
                fabs(pixelColor.redF() - bgColor.redF()) <= tol  &&
                fabs(pixelColor.greenF() - bgColor.greenF()) <= tol &&
                fabs(pixelColor.blueF() - bgColor.blueF()) <=tol
                )
            {
                modifiedScreenshot.setPixelColor(x, y, Qt::white);
            }
            else
            {
                modifiedScreenshot.setPixelColor(x, y, pixelColor);
            }
        }
    }

    return modifiedScreenshot;
}

void inline MyGLWidget::initLights()
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

void MyGLWidget::debugCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                               GLsizei length, const GLchar* message, const void* userParam) {
    Q_UNUSED(length);

    // Get the OpenGL widget instance
    const MyGLWidget* widget = static_cast<const MyGLWidget*>(userParam);

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

void MyGLWidget::initializeGL()
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
    glClearDepth(numCubes);                         // 0 is near, 1 is far
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

std::vector<std::array<GLubyte, 4>> MyGLWidget::createColorMap(int numLevels)
{
    std::vector<std::array<GLubyte, 4>> colorMap(9);

    colorMap[8] = {255, 0,      0,   255};
    colorMap[7] = {255, 178,    0,   255};
    colorMap[6] = {255, 255,    0,   255};
    colorMap[5] = {178, 255,    0,   255};
    colorMap[4] = {0,   255,    0,   255};
    colorMap[3] = {0,   255,    178, 255};
    colorMap[2] = {0,   255,    255, 255};
    colorMap[1] = {0,   178,    255, 255};
    colorMap[0] = {0,   0,      255, 255};

    return colorMap;
}

QVector<QColor> MyGLWidget::getColorMap(int numLevels)
{
    std::vector<std::array<GLubyte, 4>> vcmap = MyGLWidget::createColorMap(numLevels);
    QVector<QColor> cmap(numLevels);

    for (int i = 0; i < numLevels; i++)
    {
        QColor c;
        c.setRed(vcmap[i][0]);
        c.setGreen(vcmap[i][1]);
        c.setBlue(vcmap[i][2]);
        cmap[i] = c;
    }
    return cmap;
}

std::array<GLubyte, 4> MyGLWidget::scalarToColor(float value, const std::vector<std::array<GLubyte, 4>>& colorMap)
{
    int index = static_cast<int>(value * (colorMap.size() - 1));
    return colorMap[index];
}

void MyGLWidget::setAnsysWrapper(ansysWrapper *wr)
{
    this->wr = wr;
    this->calculateScene();
}

void MyGLWidget::setComponent(int index)
{
    this->plotComponent = index;
    this->calculateScene();
}

std::vector<std::array<GLubyte, 4>> MyGLWidget::generateDistinctColors()
{
    std::vector<std::array<GLubyte, 4>> colors;
    float hueIncrement = 360.0f / numColors;

    for (int i = 0; i < numColors; ++i) {
        float hue = i * hueIncrement;
        float saturation = 1.0f;
        float value = 1.0f;
        float alpha = 0.85f;

        // Convert HSV to RGB
        float chroma = saturation * value;
        float huePrime = hue / 60.0f;
        float x = chroma * (1.0f - std::abs(std::fmod(huePrime, 2.0f) - 1.0f));
        float r, g, b;

        if (huePrime >= 0 && huePrime < 1)
        {
            r = chroma;
            g = x;
            b = 0;
        } else if (huePrime >= 1 && huePrime < 2)
        {
            r = x;
            g = chroma;
            b = 0;
        } else if (huePrime >= 2 && huePrime < 3)
        {
            r = 0;
            g = chroma;
            b = x;
        } else if (huePrime >= 3 && huePrime < 4)
        {
            r = 0;
            g = x;
            b = chroma;
        } else if (huePrime >= 4 && huePrime < 5)
        {
            r = x;
            g = 0;
            b = chroma;
        } else
        {
            r = chroma;
            g = 0;
            b = x;
        }

        float m = value - chroma;
        r += m;
        g += m;
        b += m;

        colors.push_back({GLubyte(r*255), GLubyte(g*255), GLubyte(b*255), GLubyte(alpha*255)});
    }

    return colors;
}

void MyGLWidget::calculateScene()
{

    static const float node_coordinates[8][3] =
        {
            {0, 0, 0}, // 1
            {0, 0, 1}, // 5
            {0, 1, 1}, // 8
            {0, 1, 0}, // 4
            {1, 0, 0}, // 2
            {1, 0, 1}, // 6
            {1, 1, 1}, // 7
            {1, 1, 0}, // 3
        };

    voxelScene.clear();
    float cubeSize = 1.0; // numCubes;
    for (int i = 0; i < numCubes; i++) { // y
        for (int j = 0; j < numCubes; j++) { // z
            for (int k = 0; k < numCubes; k++) { // x

                assert(voxels[k][i][j] >= 0);

                if(voxels[k][i][j] == 0)
                {
                    continue;
                }

                bool neighbors[6] = {false};  // check for the same type voxels
                {
                    auto vx = voxels[k][i][j];
                    neighbors[0] = (k > 0) ? (voxels[k - 1][i][j] == vx) : false; // -x
                    neighbors[2] = (k < numCubes - 1) ? (voxels[k + 1][i][j] == vx) : false; // +x

                    neighbors[1] = (i < numCubes - 1) ? (voxels[k][i + 1][j] == vx): false; // +y
                    neighbors[3] = (i > 0) ? (voxels[k][i - 1][j] == vx): false; // -y


                    neighbors[4] = (j < numCubes - 1) ? (voxels[k][i][j + 1] == vx) : false; // +z
                    neighbors[5] = (j > 0) ? (voxels[k][i][j - 1] == vx): false; // -z

                    bool c1 = neighbors[0] && neighbors[1];
                    bool c2 = neighbors[2] && neighbors[3];
                    bool c3 = neighbors[4] && neighbors[5];
                    if (c1 && c2 && c3)
                        continue;
                }

                int index = voxels[k][i][j] - 1;
                auto color = colors[index].data();


                GLfloat refColor[] = {1.0f, 1.0f, 1.0f};
                GLfloat diff[] = {  GLfloat(color[0]/255.0 - refColor[0]),
                    GLfloat(color[1]/255.0 - refColor[1]),
                    GLfloat(color[2]/255.0 - refColor[2])
                };

                // Define a direction factor, which can be negative or positive
                float directionFactor = directionFactors[index];

                GLfloat offset[] = {directionFactor * diff[0] * distanceFactor,
                                    directionFactor * diff[1] * distanceFactor,
                                    directionFactor * diff[2] * distanceFactor};

                Voxel v;
                v.x = -(numCubes/2 - k) * cubeSize + offset[0];
                v.y = -(numCubes/2 - i) * cubeSize + offset[1];
                v.z = -(numCubes/2 - j) * cubeSize + offset[2];

                v.r = color[0];
                v.g = color[1];
                v.b = color[2];
                v.a = color[3];

                std::vector<std::array<GLubyte, 4>> node_colors(8);
                if (wr)
                {
                    auto cmap = this->createColorMap(0);
                    for (int l = 0; l < 8; l++)
                    {
                        n3d::node3d key;
                        key.data[0] = node_coordinates[l][0] + k;
                        key.data[1] = node_coordinates[l][1] + i;
                        key.data[2] = node_coordinates[l][2] + j;

                        float val = wr->getValByCoord(key, plotComponent);
                        float val01 = wr->scaleValue01(val, plotComponent);
                        node_colors[l] = this->scalarToColor(val01, cmap);
                    }
                }
                else
                {
                    std::fill(node_colors.begin(), node_colors.end(), colors[index]);
                }

                drawCube(cubeSize, v, neighbors, node_colors);
            }
        }
    }
    isVBOupdateRequired = true;
}

void MyGLWidget::setVoxels(int32_t*** voxels, short int numCubes)
{
    this->voxels = voxels;
    this->numCubes = numCubes;
    calculateScene();
}

void MyGLWidget::updateVoxelColor(Voxel &v1)
{
    if (wr)
    {
        float val = wr->getValByCoord(v1.x, v1.y, v1.z, SX);
        float val01 = wr->scaleValue01(val, SX);
        auto cmap = this->createColorMap(8);
        auto color = this->scalarToColor(val01, cmap);
        v1.r = color[0];
        v1.g = color[1];
        v1.b = color[2];
    }
}

void MyGLWidget::drawCube(short cubeSize, Voxel vox, bool* neighbors, std::vector<std::array<GLubyte, 4>> &node_colors)
{

    static const GLfloat n[6][3] =
        {
            {-1.0, 0.0, 0.0}, // -x
            {0.0, 1.0, 0.0},  // y
            {1.0, 0.0, 0.0},  // x
            {0.0, -1.0, 0.0}, // -y
            {0.0, 0.0, 1.0},  // z
            {0.0, 0.0, -1.0}  // -z
        };

    static const GLint faces[6][4] =
        {
            {0, 1, 2, 3},
            {3, 2, 6, 7},
            {7, 6, 5, 4},
            {4, 5, 1, 0},
            {5, 6, 2, 1},
            {7, 4, 0, 3}
        };
    float v[8][3];

    float offset = 1e-6f; // offset to help z-buffer distinc same quads

    v[0][0] = v[1][0] = v[2][0] = v[3][0] = vox.x + offset;
    v[4][0] = v[5][0] = v[6][0] = v[7][0] = vox.x + cubeSize - offset;
    v[0][1] = v[1][1] = v[4][1] = v[5][1] = vox.y + offset;
    v[2][1] = v[3][1] = v[6][1] = v[7][1] = vox.y + cubeSize - offset;
    v[0][2] = v[3][2] = v[4][2] = v[7][2] = vox.z + offset;
    v[1][2] = v[2][2] = v[5][2] = v[6][2] = vox.z + cubeSize - offset;

    Voxel v1;
    for (int i = 0; i < 6; i++) // for each of 6 face of cube
    {
        if (neighbors[i] )
            continue;

        v1.nx = n[i][0];
        v1.ny = n[i][1];
        v1.nz = n[i][2];

        if  (neighbors[i] == true) // plot hidden faces with gray (50.50.50) color for debug
        {
            v1.r = 50;
            v1.g = 50;
            v1.b = 50;
            v1.a = vox.a;
        }

        for (int j = 0; j < 4; j++) // for each node
        {
            auto fij = faces[i][j];
            v1.x = v[fij][0];
            v1.y = v[fij][1];
            v1.z = v[fij][2];

            v1.a = vox.a;

            v1.r = node_colors[fij][0];
            v1.g = node_colors[fij][1];
            v1.b = node_colors[fij][2];

            //qDebug() << i << j << fij << v1.r << v1.g << v1.b << v1.x <<v1.y << v1.z;

            voxelScene.push_back(v1);
        }

    }
}

void MyGLWidget::drawCube(float cubeSize, GLenum type)
{
    float cb2 = cubeSize / 2.0;

    static const GLfloat n[6][3] =
        {
            {-1.0, 0.0, 0.0},
            {0.0, 1.0, 0.0},
            {1.0, 0.0, 0.0},
            {0.0, -1.0, 0.0},
            {0.0, 0.0, 1.0},
            {0.0, 0.0, -1.0}
        };
    static const GLint faces[6][4] =
        {
            {0, 1, 2, 3},
            {3, 2, 6, 7},
            {7, 6, 5, 4},
            {4, 5, 1, 0},
            {5, 6, 2, 1},
            {7, 4, 0, 3}
        };
    GLfloat v[8][3];
    GLint i;

    v[0][0] = v[1][0] = v[2][0] = v[3][0] = -cb2;
    v[4][0] = v[5][0] = v[6][0] = v[7][0] =  cb2 ;
    v[0][1] = v[1][1] = v[4][1] = v[5][1] = -cb2;
    v[2][1] = v[3][1] = v[6][1] = v[7][1] =  cb2;
    v[0][2] = v[3][2] = v[4][2] = v[7][2] = -cb2;
    v[1][2] = v[2][2] = v[5][2] = v[6][2] =  cb2;

    for (i = 5; i >= 0; i--) {
        glBegin(type);

        glNormal3fv(&n[i][0]);
        glVertex3fv(&v[faces[i][0]][0]);
        glVertex3fv(&v[faces[i][1]][0]);
        glVertex3fv(&v[faces[i][2]][0]);
        glVertex3fv(&v[faces[i][3]][0]);
        glEnd();
    }
}

void MyGLWidget::update_function()
{
    update();
}

void MyGLWidget::drawAxis()
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

void MyGLWidget::initializeVBO()
{
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    // Create buffers for vertex, color, and normal data
    f->glGenBuffers(1, vboIds);
    // Bind the vertex buffer
    f->glBindBuffer(GL_ARRAY_BUFFER, vboIds[0]);
    // upload buffer to GPU
    f->glBufferData(GL_ARRAY_BUFFER, voxelScene.size() * sizeof(Voxel), voxelScene.data() + offsetof(Voxel, x), GL_STATIC_DRAW);
}

void MyGLWidget::updateVBO()
{
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();

    f->glBindBuffer(GL_ARRAY_BUFFER, vboIds[0]);
    f->glBufferData(GL_ARRAY_BUFFER, voxelScene.size() * sizeof(Voxel), voxelScene.data() + offsetof(Voxel, x), GL_STATIC_DRAW);
}

void MyGLWidget::paintGL()
{
    if (isVBOupdateRequired)
    {
        this->updateVBO();
        isVBOupdateRequired = false;
    }

    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
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


void MyGLWidget::resizeGL(int width, int height)
{
    //   int side = qMin(width, height);
    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // Set up perspective projection using glFrustum
    GLfloat aspect = (GLfloat)width / (GLfloat)height;
    GLfloat fov = 45.0f;
    GLfloat nearVal = 1.0f;
    GLfloat farVal = 1000.0f;
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

void MyGLWidget::wheelEvent(QWheelEvent *event)
{
    int numDegrees = event->angleDelta().y() / 8;
    int numSteps = numDegrees / 10;

    if (numSteps > 0) {
        // zoom in
        distance -= numSteps * numCubes * 0.1f; // adjust the distance based on the number of steps
    } else if (numSteps < 0) {
        // zoom out
        distance += -numSteps * numCubes * 0.1f; // adjust the distance based on the number of steps (use the absolute value)
    }

    update(); // redraw the cube with the new camera position
}

void MyGLWidget::mousePressEvent(QMouseEvent *event)
{
    lastPos = event->pos();
}

void MyGLWidget::mouseMoveEvent(QMouseEvent *event)
{
    int dx = event->position().x() - lastPos.x();
    int dy = event->position().y() - lastPos.y();

    if (event->buttons() & Qt::LeftButton) {
        setXRotation(xRot + 8 * dy);
        setYRotation(yRot + 8 * dx);
    } else if (event->buttons() & Qt::RightButton) {
        setXRotation(xRot + 8 * dy);
        setZRotation(zRot + 8 * dx);
    }

    lastPos = event->pos();
}

void MyGLWidget::updateGLWidget(int32_t*** voxels, short int numCubes)
{
    setVoxels(voxels, numCubes);
    QThread::msleep(delayAnimation);
    repaint();
}

int32_t*** MyGLWidget::getVoxels()
{
    return voxels;
}

